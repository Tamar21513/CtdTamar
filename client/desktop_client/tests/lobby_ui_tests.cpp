#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "ThirdParty/doctest.h"

#include "Lobby/LobbyApplicationState.hpp"
#include "Lobby/LobbyController.hpp"
#include "Lobby/LobbyInputMapper.hpp"
#include "Lobby/LobbyLayout.hpp"
#include "Lobby/LobbyRenderer.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/AnimationLibrary.hpp"
#include "Graphics/VisualStateMachine.hpp"

#include <chrono>
#include <cstdlib>
#include <deque>
#include <thread>

using namespace ctd::lobby;
using namespace ctd::network;

namespace {

AuthenticatedUser testUser() {
    return {"user-1", "alice", "2026-07-29T00:00:00Z"};
}

LobbyRoom waitingRoom(
    const std::string& id,
    const std::string& host = "alice") {
    LobbyRoom room;
    room.roomId = id;
    room.name = "Room " + id;
    room.visibility = "public";
    room.status = "waiting";
    room.host = LobbyUser{"user-" + id, host};
    room.createdAt = "2026-07-29T00:00:00Z";
    return room;
}

LobbyRoom activeRoom(const std::string& id) {
    LobbyRoom room;
    room.roomId = id;
    room.name = "Live " + id;
    room.status = "active";
    room.white = LobbyUser{"white-id", "WhitePlayer"};
    room.black = LobbyUser{"black-id", "BlackPlayer"};
    room.spectatorCount = 4;
    room.createdAt = "2026-07-29T00:00:00Z";
    return room;
}

class FakeLobbyTransport final : public ILobbyTransport {
public:
    AuthResult registerResult{
        AuthResultKind::Success, 201, testUser()};
    AuthResult loginResult{
        AuthResultKind::Success, 200, testUser()};
    AuthResult logoutResult{
        AuthResultKind::Success, 204};
    WebSocketConnectionState state =
        WebSocketConnectionState::Connected;
    std::deque<LobbyClientEvent> events;
    int registerCalls = 0;
    int loginCalls = 0;
    int logoutCalls = 0;
    int subscribeCalls = 0;
    int publicCreateCalls = 0;
    int hiddenCreateCalls = 0;
    std::vector<std::string> createdRoomNames;
    std::vector<std::string> joinedRooms;
    std::vector<std::string> hiddenCodes;
    std::vector<std::string> watchedRooms;
    std::vector<std::string> leftSpectatorRooms;
    struct SentMove {
        std::string roomId;
        unsigned long long sequence;
        int sourceRow;
        int sourceCol;
        int destinationRow;
        int destinationCol;
    };
    std::vector<SentMove> sentMoves;
    std::vector<SentMove> sentJumps;

    AuthResult registerUser(
        const std::string&,
        const std::string&) override {
        ++registerCalls;
        return registerResult;
    }
    AuthResult login(
        const std::string&,
        const std::string&) override {
        ++loginCalls;
        return loginResult;
    }
    AuthResult logout() override {
        ++logoutCalls;
        return logoutResult;
    }
    bool connectRealtime() override { return true; }
    void disconnectRealtime() override {}
    WebSocketConnectionState connectionState() const override {
        return state;
    }
    bool subscribeLobby() override {
        ++subscribeCalls;
        return true;
    }
    bool requestLobby() override { return true; }
    bool createRoom(
        const std::string& name,
        bool hidden) override {
        createdRoomNames.push_back(name);
        if (hidden) {
            ++hiddenCreateCalls;
        } else {
            ++publicCreateCalls;
        }
        return true;
    }
    bool joinPublicRoom(const std::string& roomId) override {
        joinedRooms.push_back(roomId);
        return true;
    }
    bool joinHiddenRoom(const std::string& code) override {
        hiddenCodes.push_back(code);
        return true;
    }
    bool watchRoom(const std::string& roomId) override {
        watchedRooms.push_back(roomId);
        return true;
    }
    bool leaveSpectator(const std::string& roomId) override {
        leftSpectatorRooms.push_back(roomId);
        return true;
    }
    bool sendMove(
        const std::string& roomId,
        unsigned long long sequence,
        int sourceRow,
        int sourceCol,
        int destinationRow,
        int destinationCol) override {
        sentMoves.push_back({
            roomId,
            sequence,
            sourceRow,
            sourceCol,
            destinationRow,
            destinationCol});
        return true;
    }
    bool sendJump(
        const std::string& roomId,
        unsigned long long sequence,
        int row,
        int col) override {
        sentJumps.push_back({
            roomId, sequence, row, col, row, col});
        return true;
    }
    std::optional<LobbyClientEvent> pollEvent() override {
        if (events.empty()) {
            return std::nullopt;
        }
        auto event = std::move(events.front());
        events.pop_front();
        return event;
    }
};

void typeCredentials(LobbyController& controller) {
    for (const char value : std::string("alice")) {
        controller.inputCharacter(value);
    }
    controller.handle({LobbyActionType::FocusPassword, {}});
    for (const char value : std::string("StrongPassword!")) {
        controller.inputCharacter(value);
    }
}

void waitForAuthentication(LobbyController& controller) {
    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(2);
    while (controller.hasAuthenticationTask() &&
           std::chrono::steady_clock::now() < deadline) {
        controller.tick();
        std::this_thread::sleep_for(
            std::chrono::milliseconds(1));
    }
    controller.tick();
}

void makeLobbyReady(
    LobbyApplicationState& state,
    FakeLobbyTransport& transport,
    LobbyController& controller) {
    state.authenticationSucceeded("alice");
    state.setConnectionState(WebSocketConnectionState::Connected);
    state.applyEvent(LobbyEvent{LobbySnapshotEvent{
        {waitingRoom("room-1")},
        {activeRoom("game-1")}}});
    controller.tick();
    transport.state = WebSocketConnectionState::Connected;
}

struct LiveUiClient {
    LobbyApplicationState state;
    LobbyTransport transport;
    LobbyController controller;

    LiveUiClient()
        : controller(state, transport) {}
};

void typeCredentials(
    LobbyController& controller,
    const std::string& username,
    const std::string& password) {
    controller.handle({LobbyActionType::FocusUsername, {}});
    for (const char value : username) {
        controller.inputCharacter(value);
    }
    controller.handle({LobbyActionType::FocusPassword, {}});
    for (const char value : password) {
        controller.inputCharacter(value);
    }
}

template <typename Predicate>
bool tickUntil(
    std::vector<LiveUiClient*> clients,
    Predicate predicate,
    std::chrono::seconds timeout = std::chrono::seconds(8)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        for (auto* client : clients) {
            client->controller.tick();
        }
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

void registerLive(
    LiveUiClient& client,
    const std::string& username,
    const std::string& password) {
    typeCredentials(client.controller, username, password);
    client.controller.handle({LobbyActionType::Register, {}});
    REQUIRE(tickUntil(
        {&client},
        [&client]() {
            return client.state.view().screen == LobbyScreen::Lobby;
        }));
}

void loginLive(
    LiveUiClient& client,
    const std::string& username,
    const std::string& password) {
    typeCredentials(client.controller, username, password);
    client.controller.handle({LobbyActionType::Login, {}});
    REQUIRE(tickUntil(
        {&client},
        [&client]() {
            return client.state.view().screen == LobbyScreen::Lobby;
        }));
}

}  // namespace

TEST_CASE("LobbyLayout produces a bounded desktop card grid") {
    LobbyLayout calculator;
    CHECK(calculator.columnsForWidth(1200) == 3);
    CHECK(calculator.columnsForWidth(1366) == 3);
    CHECK(calculator.columnsForWidth(1800) == 4);
    for (const auto size :
         {cv::Size{1200, 700}, cv::Size{1366, 768},
          cv::Size{1800, 1000}}) {
        const auto layout =
            calculator.calculate(size.width, size.height, 8, 8);
        CHECK((layout.header & layout.waitingSection).area() == 0);
        CHECK((layout.waitingSection & layout.activeSection).area() == 0);
        for (const auto& card : layout.waitingCards) {
            CHECK((card & layout.waitingSection) == card);
        }
        for (const auto& card : layout.activeCards) {
            CHECK((card & layout.activeSection) == card);
        }
        CHECK(layout.cardsPerPage > 0);
    }
}

TEST_CASE("Login and registration are explicit separate actions") {
    LobbyApplicationState state;
    FakeLobbyTransport transport;
    LobbyController controller(state, transport);
    typeCredentials(controller);
    controller.handle({LobbyActionType::Login, {}});
    CHECK(state.view().pendingAction == PendingLobbyAction::Login);
    waitForAuthentication(controller);
    CHECK(transport.loginCalls == 1);
    CHECK(transport.registerCalls == 0);
    CHECK(state.view().screen == LobbyScreen::Connecting);

    state.resetForLogout();
    typeCredentials(controller);
    controller.handle({LobbyActionType::Register, {}});
    CHECK(state.view().pendingAction == PendingLobbyAction::Register);
    waitForAuthentication(controller);
    CHECK(transport.registerCalls == 1);
    CHECK(transport.loginCalls == 2);
}

TEST_CASE("Failed login stays on authentication and shows safe error") {
    LobbyApplicationState state;
    FakeLobbyTransport transport;
    transport.loginResult = {
        AuthResultKind::InvalidCredentials,
        401,
        std::nullopt,
        "invalid_credentials",
        "Invalid username or password"};
    LobbyController controller(state, transport);
    typeCredentials(controller);
    controller.handle({LobbyActionType::Login, {}});
    waitForAuthentication(controller);
    CHECK(state.view().screen == LobbyScreen::Authentication);
    CHECK(state.view().visibleError ==
          "Invalid username or password.");
    CHECK(state.view().passwordLength == 0);
}

TEST_CASE("Connected subscribes and snapshot replaces stale rooms") {
    LobbyApplicationState state;
    FakeLobbyTransport transport;
    LobbyController controller(state, transport);
    state.authenticationSucceeded("alice");
    transport.events.push_back({
        LobbyEvent{ConnectedEvent{{"1", "alice"}}},
        std::nullopt,
        {}});
    transport.events.push_back({
        LobbyEvent{LobbySnapshotEvent{
            {waitingRoom("old")}, {}}},
        std::nullopt,
        {}});
    controller.tick();
    CHECK(transport.subscribeCalls == 1);
    CHECK(state.view().waitingRooms.size() == 1);
    CHECK(state.view().waitingRooms.front().roomId == "old");
    state.applyEvent(LobbyEvent{LobbySnapshotEvent{
        {waitingRoom("new-a"), waitingRoom("new-b")},
        {activeRoom("live")}}});
    CHECK(state.view().waitingRooms.size() == 2);
    CHECK(state.view().waitingRooms.front().roomId == "new-a");
    CHECK(state.view().activeRooms.size() == 1);
}

TEST_CASE("Room commands use typed IDs and block duplicates") {
    LobbyApplicationState state;
    FakeLobbyTransport transport;
    LobbyController controller(state, transport);
    makeLobbyReady(state, transport, controller);

    state.editableForInput().roomNameInput = "  Friendly Room  ";
    controller.handle({LobbyActionType::SubmitCreateRoom, {}});
    controller.handle({LobbyActionType::SubmitCreateRoom, {}});
    CHECK(transport.publicCreateCalls == 1);
    CHECK(transport.createdRoomNames ==
          std::vector<std::string>{"Friendly Room"});
    state.clearPending();
    controller.handle({
        LobbyActionType::SelectHiddenVisibility, {}});
    controller.handle({LobbyActionType::SubmitCreateRoom, {}});
    CHECK(transport.hiddenCreateCalls == 1);

    state.clearPending();
    controller.handle({
        LobbyActionType::JoinPublicRoom, "room-42"});
    CHECK(transport.joinedRooms ==
          std::vector<std::string>{"room-42"});

    state.clearPending();
    auto& view = state.editableForInput();
    view.modal = LobbyModal::JoinHiddenRoom;
    view.hiddenRoomCodeInput = "abc234";
    controller.handle({LobbyActionType::SubmitHiddenCode, {}});
    CHECK(transport.hiddenCodes ==
          std::vector<std::string>{"ABC234"});

    state.clearPending();
    view.screen = LobbyScreen::Lobby;
    view.modal = LobbyModal::JoinHiddenRoom;
    view.hiddenRoomCodeInput = "O0I111";
    controller.handle({LobbyActionType::SubmitHiddenCode, {}});
    CHECK(transport.hiddenCodes.size() == 1);
    CHECK_FALSE(view.visibleError.empty());

    state.clearPending();
    view.screen = LobbyScreen::Lobby;
    view.modal = LobbyModal::None;
    controller.handle({LobbyActionType::WatchRoom, "game-9"});
    CHECK(transport.watchedRooms ==
          std::vector<std::string>{"game-9"});
}

GameStateSnapshot matchSnapshot() {
    GameStateSnapshot snapshot;
    snapshot.playersReady = true;
    snapshot.whiteUsername = "alice";
    snapshot.blackUsername = "bob";
    PieceSnapshot piece;
    piece.id = 1;
    piece.token = "wP";
    piece.position = Position(6, 4);
    snapshot.pieces.push_back(piece);
    return snapshot;
}

TEST_CASE("Room name input supports typing backspace and validation") {
    LobbyApplicationState state;
    FakeLobbyTransport transport;
    LobbyController controller(state, transport);
    makeLobbyReady(state, transport, controller);

    controller.handle({LobbyActionType::OpenCreateRoom, {}});
    controller.inputCharacter('A');
    controller.inputCharacter('B');
    controller.backspace();
    CHECK(state.view().roomNameInput == "A");

    state.editableForInput().roomNameInput = "   ";
    controller.handle({LobbyActionType::SubmitCreateRoom, {}});
    CHECK(transport.createdRoomNames.empty());
    CHECK(state.view().visibleError == "Room name is required.");

    state.editableForInput().roomNameInput = std::string(41, 'x');
    controller.handle({LobbyActionType::SubmitCreateRoom, {}});
    CHECK(transport.createdRoomNames.empty());
    CHECK(state.view().visibleError ==
          "Room name must be at most 40 UTF-8 bytes.");
}

TEST_CASE("Room creation keeps errors and clears name only on success") {
    LobbyApplicationState state;
    FakeLobbyTransport transport;
    LobbyController controller(state, transport);
    makeLobbyReady(state, transport, controller);
    auto& view = state.editableForInput();
    view.modal = LobbyModal::CreateRoom;
    view.roomNameInput = "My Room";
    controller.handle({LobbyActionType::SubmitCreateRoom, {}});
    CHECK(view.roomNameInput == "My Room");

    state.applyEvent(LobbyEvent{ProtocolErrorEvent{
        "room_name_taken",
        "A room with this name already exists."}});
    CHECK(view.visibleError ==
          "A room with this name already exists.");
    CHECK(view.roomNameInput == "My Room");

    auto room = waitingRoom("created");
    room.name = "My Room";
    state.applyEvent(LobbyEvent{RoomCreatedEvent{room}});
    CHECK(view.roomNameInput.empty());
    CHECK(view.currentRoomName == "My Room");
}

TEST_CASE("Create modal maps visibility and submit controls") {
    LobbyApplicationState state;
    auto& view = state.editableForInput();
    view.screen = LobbyScreen::Lobby;
    view.modal = LobbyModal::CreateRoom;
    LobbyLayout calculator;
    const auto layout = calculator.calculate(1366, 768, 0, 0);
    LobbyInputMapper mapper;

    auto action = mapper.mapClick(
        layout.hiddenVisibilityButton.tl() + cv::Point{4, 4},
        view, layout);
    CHECK(action.type == LobbyActionType::SelectHiddenVisibility);
    action = mapper.mapClick(
        layout.modalPrimaryButton.tl() + cv::Point{4, 4},
        view, layout);
    CHECK(action.type == LobbyActionType::SubmitCreateRoom);
}

TEST_CASE("Paging actions remain within supplied bounds") {
    LobbyApplicationState state;
    FakeLobbyTransport transport;
    LobbyController controller(state, transport);
    auto& view = state.editableForInput();
    view.waitingPage = 0;
    controller.handle({
        LobbyActionType::NextWaitingPage, "2"});
    controller.handle({
        LobbyActionType::NextWaitingPage, "2"});
    controller.handle({
        LobbyActionType::NextWaitingPage, "2"});
    CHECK(view.waitingPage == 2);
    controller.handle({
        LobbyActionType::PreviousWaitingPage, {}});
    controller.handle({
        LobbyActionType::PreviousWaitingPage, {}});
    controller.handle({
        LobbyActionType::PreviousWaitingPage, {}});
    CHECK(view.waitingPage == 0);
}

TEST_CASE("Structured errors disconnect and logout clear state") {
    LobbyApplicationState state;
    FakeLobbyTransport transport;
    LobbyController controller(state, transport);
    makeLobbyReady(state, transport, controller);
    state.editableForInput().pendingAction =
        PendingLobbyAction::JoinPublicRoom;
    state.applyEvent(LobbyEvent{ProtocolErrorEvent{
        "room_unavailable", "Room is no longer available."}});
    CHECK(state.view().pendingAction == PendingLobbyAction::None);
    CHECK(state.view().visibleError ==
          "Room is no longer available.");

    transport.state = WebSocketConnectionState::Disconnected;
    controller.tick();
    CHECK_FALSE(state.view().networkActionsEnabled());
    controller.handle({LobbyActionType::Logout, {}});
    CHECK(transport.logoutCalls == 1);
    CHECK(state.view().screen == LobbyScreen::Authentication);
    CHECK(state.view().authenticatedUsername.empty());
    CHECK(state.view().waitingRooms.empty());
}

TEST_CASE("Game and spectator events open honest placeholders") {
    LobbyApplicationState state;
    state.applyEvent(LobbyEvent{GameStartedEvent{
        "room-1", "Named Match", "white", {"2", "bob"}}});
    CHECK(state.view().screen == LobbyScreen::RoomReady);
    REQUIRE(state.view().roomReady.has_value());
    CHECK(state.view().roomReady->assignedColor == "white");
    CHECK(state.view().roomReady->roomName == "Named Match");
    CHECK(state.view().roomReady->opponentUsername == "bob");

    state.applyEvent(LobbyEvent{WatchingGameEvent{
        "room-1",
        "Named Match",
        {"1", "alice"},
        {"2", "bob"},
        3}});
    CHECK(state.view().screen ==
          LobbyScreen::SpectatorPlaceholder);
    REQUIRE(state.view().spectator.has_value());
    CHECK(state.view().spectator->spectatorCount == 3);
}

TEST_CASE("Input mapping selects only the correct card action") {
    LobbyApplicationState state;
    auto& view = state.editableForInput();
    view.screen = LobbyScreen::Lobby;
    view.connectionState = WebSocketConnectionState::Connected;
    view.waitingRooms = {
        waitingRoom("first"), waitingRoom("second")};
    LobbyLayout calculator;
    const auto layout = calculator.calculate(1366, 768, 2, 0);
    LobbyInputMapper mapper;
    const auto secondButton =
        LobbyLayout::cardActionButton(layout.waitingCards[1]);
    const auto action = mapper.mapClick(
        {secondButton.x + 5, secondButton.y + 5},
        view,
        layout);
    CHECK(action.type == LobbyActionType::JoinPublicRoom);
    CHECK(action.value == "second");
    CHECK(mapper.mapClick({10, 300}, view, layout).type ==
          LobbyActionType::None);
    CHECK(mapper.mapClick(
        {layout.createRoomButton.x + 5,
         layout.createRoomButton.y + 5},
        view,
        layout).type == LobbyActionType::OpenCreateRoom);
}

TEST_CASE("LobbyRenderer smoke screens do not mutate view models") {
    LobbyRenderer renderer;
    LobbyLayout calculator;
    LobbyApplicationState state;
    auto layout = calculator.calculate(1366, 768, 0, 0);
    const auto before = state.snapshot();
    CHECK_FALSE(renderer.render(before, layout).empty());
    CHECK(state.view().screen == before.screen);

    auto& view = state.editableForInput();
    view.screen = LobbyScreen::Lobby;
    view.connectionState = WebSocketConnectionState::Connected;
    CHECK_FALSE(renderer.render(state.snapshot(), layout).empty());
    view.waitingRooms = {waitingRoom("room")};
    view.activeRooms = {activeRoom("game")};
    layout = calculator.calculate(1366, 768, 1, 1);
    CHECK_FALSE(renderer.render(state.snapshot(), layout).empty());

    view.screen = LobbyScreen::HiddenRoomWaiting;
    view.hiddenRoomCode = "ABC234";
    CHECK_FALSE(renderer.render(state.snapshot(), layout).empty());
    view.screen = LobbyScreen::SpectatorPlaceholder;
    view.spectator = SpectatorView{
        "game", "Named Match", "alice", "bob", 2};
    CHECK_FALSE(renderer.render(state.snapshot(), layout).empty());
}

TEST_CASE("Live multi-client native lobby flow") {
    if (std::getenv("CTD_RUN_LIVE_LOBBY_UI_TEST") == nullptr) {
        return;
    }
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string password = "Stage3F-Test-Password!";
    const auto userA = "stage3f_a_" + suffix;
    const auto userB = "stage3f_b_" + suffix;
    const auto userC = "stage3f_c_" + suffix;
    LiveUiClient first;
    LiveUiClient second;
    LiveUiClient spectator;

    registerLive(first, userA, password);
    registerLive(second, userB, password);
    registerLive(spectator, userC, password);

    first.state.editableForInput().roomNameInput = "Public Test Room";
    first.controller.handle({
        LobbyActionType::SubmitCreateRoom, {}});
    REQUIRE(tickUntil(
        {&first, &second, &spectator},
        [&first, &second]() {
            return first.state.view().screen ==
                    LobbyScreen::WaitingRoom &&
                second.state.view().waitingRooms.size() == 1;
        }));
    const auto publicRoomId = first.state.view().pendingRoomId;
    REQUIRE_FALSE(publicRoomId.empty());
    second.controller.handle({
        LobbyActionType::JoinPublicRoom, publicRoomId});
    REQUIRE(tickUntil(
        {&first, &second, &spectator},
        [&first, &second, &spectator]() {
            return first.state.view().screen ==
                    LobbyScreen::Game &&
                second.state.view().screen ==
                    LobbyScreen::Game &&
                !spectator.state.view().activeRooms.empty();
        }));
    REQUIRE(first.state.view().match.has_value());
    REQUIRE(second.state.view().match.has_value());
    CHECK(first.state.view().match->assignedColor == "white");
    CHECK(second.state.view().match->assignedColor == "black");

    spectator.controller.handle({
        LobbyActionType::WatchRoom, publicRoomId});
    REQUIRE(tickUntil(
        {&first, &second, &spectator},
        [&spectator]() {
            return spectator.state.view().screen ==
                LobbyScreen::SpectatorGame;
        }));
    REQUIRE(spectator.state.view().spectator.has_value());
    CHECK(spectator.state.view().spectator->whiteUsername == userA);
    CHECK(spectator.state.view().spectator->blackUsername == userB);
    CHECK(spectator.state.view().spectator->spectatorCount == 1);

    spectator.controller.handle({
        LobbyActionType::BackToLobby, {}});
    REQUIRE(tickUntil(
        {&first, &second, &spectator},
        [&spectator]() {
            return spectator.state.view().screen ==
                    LobbyScreen::Lobby &&
                !spectator.state.view().activeRooms.empty() &&
                spectator.state.view()
                        .activeRooms[0].spectatorCount == 0;
        }));

    first.controller.handle({LobbyActionType::Logout, {}});
    second.controller.tick();
    spectator.controller.tick();
    second.controller.handle({LobbyActionType::Logout, {}});
    spectator.controller.handle({LobbyActionType::Logout, {}});
}

TEST_CASE("match ready opens authoritative board and stores color") {
    LobbyApplicationState state;
    state.applyEvent(MatchReadyEvent{
        "room-1", "white", "bob", 1, matchSnapshot()});
    REQUIRE(state.view().match.has_value());
    CHECK(state.view().screen == LobbyScreen::Game);
    CHECK(state.view().match->assignedColor == "white");
    CHECK(state.view().match->snapshot.pieces.size() == 1);
}

TEST_CASE("moves are sent without optimistic board mutation") {
    LobbyApplicationState state;
    FakeLobbyTransport transport;
    LobbyController controller(state, transport);
    state.applyEvent(MatchReadyEvent{
        "room-1", "white", "bob", 1, matchSnapshot()});
    const auto before = state.view().match->snapshot;
    controller.gameBoardClick(6, 4);
    controller.gameBoardClick(5, 4);
    REQUIRE(transport.sentMoves.size() == 1);
    CHECK(transport.sentMoves[0].roomId == "room-1");
    CHECK(transport.sentMoves[0].sequence == 1);
    CHECK(transport.sentMoves[0].sourceRow == 6);
    CHECK(transport.sentMoves[0].destinationRow == 5);
    CHECK(state.view().match->snapshot.pieces[0].position ==
          before.pieces[0].position);
}

TEST_CASE("rejection is displayed and stale revisions are ignored") {
    LobbyApplicationState state;
    state.applyEvent(MatchReadyEvent{
        "room-1", "black", "alice", 3, matchSnapshot()});
    state.applyEvent(MoveResultEvent{
        "room-1", 1, false, "not_your_piece"});
    CHECK(state.view().match->moveStatus ==
          "Move rejected: not_your_piece");

    auto stale = matchSnapshot();
    stale.pieces.clear();
    state.applyEvent(MatchStateEvent{"room-1", 2, stale});
    CHECK(state.view().match->snapshot.pieces.size() == 1);
    state.applyEvent(MatchStateEvent{"room-1", 4, stale});
    CHECK(state.view().match->snapshot.pieces.empty());
}

TEST_CASE("spectator board is read only") {
    LobbyApplicationState state;
    FakeLobbyTransport transport;
    LobbyController controller(state, transport);
    state.editableForInput().authenticatedUsername = "viewer";
    state.applyEvent(MatchSnapshotEvent{
        "room-1", 1, matchSnapshot()});
    CHECK(state.view().screen == LobbyScreen::SpectatorGame);
    REQUIRE(state.view().match.has_value());
    CHECK(state.view().match->spectator);
    controller.gameBoardClick(6, 4);
    controller.gameBoardClick(5, 4);
    CHECK(transport.sentMoves.empty());
    CHECK(transport.sentJumps.empty());

    controller.handle({LobbyActionType::BackToLobby, {}});
    CHECK(state.view().screen == LobbyScreen::Lobby);
    REQUIRE(transport.leftSpectatorRooms.size() == 1);
    CHECK(transport.leftSpectatorRooms[0] == "room-1");
    CHECK_FALSE(state.view().match.has_value());
    CHECK(state.view().authenticatedUsername == "viewer");
}

TEST_CASE("same-cell and explicit inputs use authoritative jump") {
    LobbyApplicationState state;
    FakeLobbyTransport transport;
    LobbyController controller(state, transport);
    state.applyEvent(MatchReadyEvent{
        "room-1", "white", "bob", 1, matchSnapshot()});

    controller.gameBoardClick(6, 4);
    controller.gameBoardClick(6, 4);
    REQUIRE(transport.sentJumps.size() == 1);
    CHECK(transport.sentMoves.empty());
    CHECK(transport.sentJumps[0].sourceRow == 6);
    CHECK(transport.sentJumps[0].sourceCol == 4);
    CHECK(transport.sentJumps[0].destinationRow == 6);
    CHECK(transport.sentJumps[0].destinationCol == 4);
    CHECK(state.view().match->snapshot.pieces[0].position ==
          Position(6, 4));

    controller.gameBoardJump(6, 4);
    CHECK(transport.sentJumps.size() == 2);
}

TEST_CASE("gameplay presentation has no captions") {
    LobbyViewModel player;
    player.screen = LobbyScreen::Game;
    player.visibleError = "debug";
    player.statusMessage = "waiting";
    player.match = AuthoritativeMatchView{};
    player.match->moveStatus = "Move rejected: test";
    const auto playerPresentation = gameplayPresentation(player);
    CHECK(playerPresentation.statusLine1.empty());
    CHECK(playerPresentation.statusLine2.empty());
    CHECK_FALSE(playerPresentation.showSpectatorBackButton);

    player.screen = LobbyScreen::SpectatorGame;
    const auto spectatorPresentation = gameplayPresentation(player);
    CHECK(spectatorPresentation.statusLine1.empty());
    CHECK(spectatorPresentation.statusLine2.empty());
    CHECK(spectatorPresentation.showSpectatorBackButton);
    const auto button = Renderer::spectatorBackButtonRect();
    const cv::Rect playableBoard(
        Renderer::SIDE_PANEL_WIDTH + 66,
        Renderer::SCORE_PANEL_HEIGHT + 66,
        58 * 8,
        58 * 8);
    CHECK((button & playableBoard).area() == 0);
}

TEST_CASE("jump animation transitions through short rest to idle") {
    CHECK(visualStateToFolderName(VisualState::Jump) == "jump");
    VisualStateMachine machine;
    CHECK(machine.chooseState(
              PieceState::Airborne, 0, 0) ==
          VisualState::Jump);
    CHECK(machine.chooseState(
              PieceState::Idle, 900, 2000) ==
          VisualState::ShortRest);
    CHECK(machine.chooseState(
              PieceState::Idle, 0, 2000) ==
          VisualState::Idle);
}
