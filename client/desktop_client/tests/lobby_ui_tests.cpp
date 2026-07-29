#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "ThirdParty/doctest.h"

#include "Lobby/LobbyApplicationState.hpp"
#include "Lobby/LobbyController.hpp"
#include "Lobby/LobbyInputMapper.hpp"
#include "Lobby/LobbyLayout.hpp"
#include "Lobby/LobbyRenderer.hpp"

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
    room.visibility = "public";
    room.status = "waiting";
    room.host = LobbyUser{"user-" + id, host};
    room.createdAt = "2026-07-29T00:00:00Z";
    return room;
}

LobbyRoom activeRoom(const std::string& id) {
    LobbyRoom room;
    room.roomId = id;
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
    std::vector<std::string> joinedRooms;
    std::vector<std::string> hiddenCodes;
    std::vector<std::string> watchedRooms;

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
    bool createPublicRoom() override {
        ++publicCreateCalls;
        return true;
    }
    bool createHiddenRoom() override {
        ++hiddenCreateCalls;
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

    controller.handle({LobbyActionType::CreatePublicRoom, {}});
    controller.handle({LobbyActionType::CreatePublicRoom, {}});
    CHECK(transport.publicCreateCalls == 1);
    state.clearPending();
    controller.handle({LobbyActionType::CreateHiddenRoom, {}});
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
        "room-1", "white", {"2", "bob"}}});
    CHECK(state.view().screen == LobbyScreen::RoomReady);
    REQUIRE(state.view().roomReady.has_value());
    CHECK(state.view().roomReady->assignedColor == "white");
    CHECK(state.view().roomReady->opponentUsername == "bob");

    state.applyEvent(LobbyEvent{WatchingGameEvent{
        "room-1",
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
        "game", "alice", "bob", 2};
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

    first.controller.handle({
        LobbyActionType::CreatePublicRoom, {}});
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
                    LobbyScreen::RoomReady &&
                second.state.view().screen ==
                    LobbyScreen::RoomReady &&
                !spectator.state.view().activeRooms.empty();
        }));
    REQUIRE(first.state.view().roomReady.has_value());
    REQUIRE(second.state.view().roomReady.has_value());
    CHECK(first.state.view().roomReady->assignedColor == "white");
    CHECK(second.state.view().roomReady->assignedColor == "black");

    spectator.controller.handle({
        LobbyActionType::WatchRoom, publicRoomId});
    REQUIRE(tickUntil(
        {&first, &second, &spectator},
        [&spectator]() {
            return spectator.state.view().screen ==
                LobbyScreen::SpectatorPlaceholder;
        }));
    REQUIRE(spectator.state.view().spectator.has_value());
    CHECK(spectator.state.view().spectator->whiteUsername == userA);
    CHECK(spectator.state.view().spectator->blackUsername == userB);
    CHECK(spectator.state.view().spectator->spectatorCount == 1);

    first.controller.handle({LobbyActionType::Logout, {}});
    second.controller.tick();
    spectator.controller.tick();
    second.controller.handle({LobbyActionType::Logout, {}});
    spectator.controller.handle({LobbyActionType::Logout, {}});

    loginLive(first, userA, password);
    loginLive(second, userB, password);
    first.controller.handle({
        LobbyActionType::CreateHiddenRoom, {}});
    REQUIRE(tickUntil(
        {&first, &second},
        [&first]() {
            return first.state.view().screen ==
                LobbyScreen::HiddenRoomWaiting;
        }));
    const auto hiddenCode = first.state.view().hiddenRoomCode;
    REQUIRE(hiddenCode.size() == 6);
    CHECK(second.state.view().waitingRooms.empty());

    second.controller.handle({
        LobbyActionType::OpenJoinHidden, {}});
    for (const char character : hiddenCode) {
        second.controller.inputCharacter(character);
    }
    second.controller.handle({
        LobbyActionType::SubmitHiddenCode, {}});
    REQUIRE(tickUntil(
        {&first, &second},
        [&first, &second]() {
            return first.state.view().screen ==
                    LobbyScreen::RoomReady &&
                second.state.view().screen ==
                    LobbyScreen::RoomReady;
        }));
    CHECK(first.state.view().roomReady->assignedColor == "white");
    CHECK(second.state.view().roomReady->assignedColor == "black");

    first.controller.handle({LobbyActionType::Logout, {}});
    second.controller.handle({LobbyActionType::Logout, {}});
}
