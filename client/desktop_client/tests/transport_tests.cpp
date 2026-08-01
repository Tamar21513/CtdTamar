#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "ThirdParty/doctest.h"

#include "Network/ApiClient.hpp"
#include "Network/ClientTransportConfig.hpp"
#include "Network/LobbyProtocol.hpp"
#include "Network/LobbyClient.hpp"
#include "Network/SessionCookieJar.hpp"
#include "Network/ThreadSafeQueue.hpp"
#include "Network/WebSocketClient.hpp"

#include <boost/json.hpp>

#include <deque>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

using namespace ctd::network;

namespace {

class FakeHttpTransport final : public IHttpTransport {
public:
    std::deque<HttpResponse> responses;
    std::vector<HttpRequest> requests;

    HttpResponse send(const HttpRequest& request) override {
        requests.push_back(request);
        REQUIRE_FALSE(responses.empty());
        auto response = responses.front();
        responses.pop_front();
        return response;
    }
};

HttpResponse userResponse(int status = 200) {
    return {
        status,
        R"({"id":"user-id","username":"alice","created_at":"2026-07-29T00:00:00Z"})"};
}

std::string valueOf(
    const std::string& jsonText,
    const char* key) {
    return std::string(
        boost::json::parse(jsonText).as_object().at(key).as_string());
}

std::optional<LobbyClientEvent> waitForLobbyEvent(
    LobbyClient& client,
    const std::chrono::seconds timeout = std::chrono::seconds(5)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (auto event = client.pollEvent()) {
            return event;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return std::nullopt;
}

bool waitForState(
    LobbyClient& client,
    WebSocketConnectionState expected,
    const std::chrono::seconds timeout = std::chrono::seconds(5)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (client.connectionState() == expected) {
            return true;
        }
        if (client.connectionState() == WebSocketConnectionState::Failed) {
            return expected == WebSocketConnectionState::Failed;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

}  // namespace

TEST_CASE("SessionCookieJar retains only ctd_session safely") {
    SessionCookieJar jar;
    jar.acceptSetCookie("theme=dark; Path=/");
    CHECK_FALSE(jar.hasSession());
    jar.acceptSetCookie(
        "ctd_session=opaque-one; HttpOnly; SameSite=Lax; Path=/");
    CHECK(jar.hasSession());
    CHECK(jar.debugDescription() == "SessionCookieJar(authenticated)");
    CHECK(jar.debugDescription().find("opaque-one") == std::string::npos);
    jar.acceptSetCookie("ctd_session=opaque-two; HttpOnly");
    CHECK(jar.hasSession());
    jar.clear();
    CHECK_FALSE(jar.hasSession());
    CHECK(jar.debugDescription() == "SessionCookieJar(empty)");
}

TEST_CASE("SessionCookieJar ignores malformed Set-Cookie") {
    SessionCookieJar jar;
    jar.acceptSetCookie("not-a-cookie");
    jar.acceptSetCookie("ctd_session");
    jar.acceptSetCookie("=missing-name");
    CHECK_FALSE(jar.hasSession());
}

TEST_CASE("rating_updated message parses into RatingUpdatedEvent") {
    auto update = LobbyProtocol::parse(
        R"({"type":"rating_updated","rating":1234})");
    INFO(update.error);
    REQUIRE(update.event.has_value());
    REQUIRE(std::holds_alternative<RatingUpdatedEvent>(*update.event));
    CHECK(std::get<RatingUpdatedEvent>(*update.event).rating == 1234);
}

TEST_CASE("makeTransportConfig builds http and ws URLs from host and port") {
    const auto config =
        ctd::network::makeTransportConfig("192.168.1.50", 8000);
    CHECK(config.apiBaseUrl == "http://192.168.1.50:8000");
    CHECK(config.websocketUrl == "ws://192.168.1.50:8000/ws");
    const auto defaulted = ctd::network::ClientTransportConfig{};
    CHECK(defaulted.apiBaseUrl == "http://127.0.0.1:8000");
}

TEST_CASE("ApiClient parses registration login me and logout") {
    auto transport = std::make_shared<FakeHttpTransport>();
    transport->responses.push_back(userResponse(201));
    auto login = userResponse();
    login.headers.emplace_back(
        "Set-Cookie", "ctd_session=opaque; HttpOnly; Path=/");
    transport->responses.push_back(login);
    transport->responses.push_back(userResponse());
    transport->responses.push_back({204, ""});

    SessionCookieJar jar;
    ApiClient api({}, jar, transport);
    CHECK(api.registerUser("alice", "SecretPassword!").succeeded());
    CHECK(api.login("alice", "SecretPassword!").succeeded());
    CHECK(jar.hasSession());
    const auto me = api.me();
    REQUIRE(me.user.has_value());
    CHECK(me.user->username == "alice");
    CHECK(api.logout().succeeded());
    CHECK_FALSE(jar.hasSession());
    for (const auto& request : transport->requests) {
        CHECK(request.url.find("SecretPassword!") == std::string::npos);
    }
}

TEST_CASE("ApiClient registration request contains only username and password") {
    auto transport = std::make_shared<FakeHttpTransport>();
    transport->responses.push_back(userResponse(201));
    SessionCookieJar jar;
    ApiClient api({}, jar, transport);
    CHECK(api.registerUser("alice", "SecretPassword!").succeeded());

    REQUIRE(transport->requests.size() == 1);
    const auto& request = transport->requests.front();
    CHECK(request.url.find("/auth/register") != std::string::npos);
    const auto body = boost::json::parse(request.body).as_object();
    CHECK(body.size() == 2);
    CHECK(body.contains("username"));
    CHECK(body.contains("password"));
    CHECK_FALSE(body.contains("confirm_password"));
    CHECK_FALSE(body.contains("confirmPassword"));
    CHECK(std::string(body.at("username").as_string()) == "alice");
    CHECK(
        std::string(body.at("password").as_string()) ==
        "SecretPassword!");
}

TEST_CASE("ApiClient exposes generic failed login without password") {
    auto transport = std::make_shared<FakeHttpTransport>();
    transport->responses.push_back({
        401, R"({"detail":"Invalid username or password"})"});
    SessionCookieJar jar;
    ApiClient api({}, jar, transport);
    const auto result = api.login("alice", "NeverLogThisPassword!");
    CHECK(result.kind == AuthResultKind::InvalidCredentials);
    CHECK(result.errorCode == "invalid_credentials");
    CHECK(result.message.find("NeverLogThisPassword!") ==
          std::string::npos);
    CHECK_FALSE(jar.hasSession());
}

TEST_CASE("ApiClient clears cookie after unauthorized me") {
    auto transport = std::make_shared<FakeHttpTransport>();
    auto login = userResponse();
    login.headers.emplace_back(
        "Set-Cookie", "ctd_session=opaque; HttpOnly");
    transport->responses.push_back(login);
    transport->responses.push_back({
        401, R"({"detail":"Not authenticated"})"});
    SessionCookieJar jar;
    ApiClient api({}, jar, transport);
    REQUIRE(api.login("alice", "SecretPassword!").succeeded());
    CHECK(api.me().kind == AuthResultKind::Unauthorized);
    CHECK_FALSE(jar.hasSession());
}

TEST_CASE("LobbyProtocol serializes every outgoing operation") {
    CHECK(valueOf(LobbyProtocol::subscribeLobby(), "type") ==
          "subscribe_lobby");
    CHECK(valueOf(LobbyProtocol::getLobby(), "type") == "get_lobby");
    CHECK(valueOf(
        LobbyProtocol::createRoom("My Room", false), "visibility") ==
          "public");
    CHECK(valueOf(
        LobbyProtocol::createRoom("Private Match", true), "visibility") ==
          "hidden");
    CHECK(valueOf(
        LobbyProtocol::createRoom("My Room", false), "name") ==
          "My Room");
    CHECK(valueOf(LobbyProtocol::joinRoom("room-1"), "room_id") ==
          "room-1");
    CHECK(valueOf(
        LobbyProtocol::joinHiddenRoom("ABC234"), "room_code") ==
        "ABC234");
    CHECK(valueOf(LobbyProtocol::watchGame("room-2"), "room_id") ==
          "room-2");
}

TEST_CASE("LobbyProtocol parses connected and lobby snapshot") {
    auto connected = LobbyProtocol::parse(
        R"({"type":"connected","user":{"id":"1","username":"alice"}})");
    REQUIRE(connected.event.has_value());
    CHECK(std::holds_alternative<ConnectedEvent>(*connected.event));

    auto snapshot = LobbyProtocol::parse(
        R"({"type":"lobby_snapshot","waiting_rooms":[{"room_id":"r1","name":"Morning Match","visibility":"public","status":"waiting","host":{"id":"1","username":"alice"},"created_at":"now"}],"active_games":[{"room_id":"r2","name":"Final Table","status":"active","white":{"id":"1","username":"alice"},"black":{"id":"2","username":"bob"},"spectator_count":3,"created_at":"now"}]})");
    REQUIRE(snapshot.event.has_value());
    const auto& event =
        std::get<LobbySnapshotEvent>(*snapshot.event);
    CHECK(event.waitingRooms.size() == 1);
    CHECK(event.activeGames.size() == 1);
    CHECK(event.waitingRooms[0].name == "Morning Match");
    CHECK(event.activeGames[0].name == "Final Table");
    CHECK(event.activeGames[0].spectatorCount == 3);
}

TEST_CASE("LobbyProtocol parses room lifecycle messages") {
    const auto roomCreated = LobbyProtocol::parse(
        R"({"type":"room_created","room":{"room_id":"r","name":"Private Match","visibility":"hidden","status":"waiting","host":{"id":"1","username":"alice"},"room_code":"ABC234"}})");
    CHECK(roomCreated.event.has_value());
    CHECK(std::holds_alternative<RoomCreatedEvent>(*roomCreated.event));

    const auto started = LobbyProtocol::parse(
        R"({"type":"game_started","room_id":"r","name":"Private Match","color":"white","opponent":{"id":"2","username":"bob"}})");
    CHECK(started.event.has_value());
    CHECK(std::holds_alternative<GameStartedEvent>(*started.event));

    const auto watching = LobbyProtocol::parse(
        R"({"type":"watching_game","room_id":"r","name":"Private Match","white":{"id":"1","username":"alice"},"black":{"id":"2","username":"bob"},"spectator_count":1})");
    CHECK(watching.event.has_value());
    CHECK(std::holds_alternative<WatchingGameEvent>(*watching.event));

    const auto status = LobbyProtocol::parse(
        R"({"type":"room_status","room_id":"r","status":"removed"})");
    CHECK(status.event.has_value());
    CHECK(std::holds_alternative<RoomStatusEvent>(*status.event));
}

TEST_CASE("LobbyProtocol preserves errors and rejects malformed input") {
    const auto structured = LobbyProtocol::parse(
        R"({"type":"error","code":"room_not_found","message":"missing"})");
    REQUIRE(structured.event.has_value());
    const auto& error =
        std::get<ProtocolErrorEvent>(*structured.event);
    CHECK(error.code == "room_not_found");
    CHECK(error.message == "missing");

    CHECK_FALSE(LobbyProtocol::parse("{").event.has_value());
    CHECK_FALSE(
        LobbyProtocol::parse(R"({"type":"connected"})")
            .event.has_value());
    CHECK_FALSE(
        LobbyProtocol::parse(R"({"type":"future_event"})")
            .event.has_value());
}

TEST_CASE("WebSocket handshake metadata protects the token") {
    SessionCookieJar empty;
    WebSocketClient unauthenticated(empty);
    auto metadata = unauthenticated.handshakeMetadata(
        "ws://127.0.0.1:8000/ws");
    CHECK_FALSE(metadata.includesCookie);
    CHECK_FALSE(metadata.tokenAppearsInUrl);

    SessionCookieJar authenticated;
    authenticated.acceptSetCookie(
        "ctd_session=top-secret; HttpOnly; Path=/");
    WebSocketClient websocket(authenticated);
    metadata = websocket.handshakeMetadata(
        "ws://127.0.0.1:8000/ws");
    CHECK(metadata.includesCookie);
    CHECK_FALSE(metadata.tokenAppearsInUrl);
    CHECK(metadata.target == "/ws");
    CHECK(WebSocketClient::eventKindForCloseCode(4401) ==
          WebSocketEventKind::AuthenticationFailed);
}

TEST_CASE("WebSocket shutdown without a connection does not hang") {
    SessionCookieJar jar;
    WebSocketClient websocket(jar);
    websocket.disconnect();
    CHECK(websocket.state() ==
          WebSocketConnectionState::Disconnected);
}

TEST_CASE("Network events cross the thread boundary safely") {
    ThreadSafeQueue<std::string> queue;
    std::thread producer([&queue]() {
        queue.push("lobby_snapshot");
    });
    producer.join();
    const auto value = queue.tryPop();
    REQUIRE(value.has_value());
    CHECK(*value == "lobby_snapshot");
    CHECK_FALSE(queue.tryPop().has_value());
}

TEST_CASE("authoritative match protocol serializes moves") {
    const auto value = boost::json::parse(
        LobbyProtocol::moveRequest(
            "room-1", 7, 6, 4, 5, 4)).as_object();
    CHECK(value.at("type").as_string() == "move_request");
    CHECK(value.at("room_id").as_string() == "room-1");
    CHECK(value.at("sequence").as_int64() == 7);
    CHECK(value.at("from").as_object().at("row").as_int64() == 6);
    CHECK(value.at("to").as_object().at("row").as_int64() == 5);

    const auto jump = boost::json::parse(
        LobbyProtocol::jumpRequest(
            "room-1", 8, 6, 4)).as_object();
    CHECK(jump.at("type").as_string() == "jump_request");
    CHECK(jump.at("sequence").as_int64() == 8);
    CHECK(jump.at("cell").as_object().at("row").as_int64() == 6);
    CHECK(jump.at("cell").as_object().at("col").as_int64() == 4);

    const auto leave = boost::json::parse(
        LobbyProtocol::leaveSpectator("room-1")).as_object();
    CHECK(leave.at("type").as_string() == "leave_spectator");
}

TEST_CASE("authoritative snapshots and results parse") {
    const std::string state =
        R"({"serverTimeMs":1,"gameOver":false,"whiteScore":0,)"
        R"("blackScore":0,"whiteUsername":"alice",)"
        R"("blackUsername":"bob","playersReady":true,)"
        R"("gameplayStartsAtEpochMs":10,)"
        R"("pieces":[{"id":1,"token":"wP",)"
        R"("position":{"row":6,"col":4},"state":"idle",)"
        R"("hasMoved":false,"remainingCooldownMs":0,)"
        R"("totalCooldownMs":0}],"motions":[],"jumps":[],)"
        R"("whiteMoveHistory":[],"blackMoveHistory":[]})";
    auto ready = LobbyProtocol::parse(
        R"({"type":"match_ready","room_id":"room-1",)"
        R"("color":"white","opponent":"bob","revision":1,)"
        R"("game_starts_at":"2026-07-30T12:00:00Z",)"
        R"("white_rating":1350,"black_rating":1180,)"
        "\"state\":" + state + "}");
    INFO(ready.error);
    REQUIRE(ready.event.has_value());
    REQUIRE(std::holds_alternative<MatchReadyEvent>(*ready.event));
    const auto& event = std::get<MatchReadyEvent>(*ready.event);
    CHECK(event.color == "white");
    CHECK(event.revision == 1);
    CHECK(event.state.pieces.size() == 1);
    CHECK(event.gameStartsAt == "2026-07-30T12:00:00Z");
    CHECK(event.whiteRating == 1350);
    CHECK(event.blackRating == 1180);

    auto countdown = LobbyProtocol::parse(
        R"({"type":"match_countdown","room_id":"room-1",)"
        R"("value":2,"game_starts_at":"2026-07-30T12:00:00Z"})");
    REQUIRE(countdown.event.has_value());
    CHECK(std::get<MatchCountdownEvent>(*countdown.event).value == "2");
    auto go = LobbyProtocol::parse(
        R"({"type":"match_countdown","room_id":"room-1",)"
        R"("value":"GO","game_starts_at":"2026-07-30T12:00:00Z"})");
    REQUIRE(go.event.has_value());
    CHECK(std::get<MatchCountdownEvent>(*go.event).value == "GO");

    auto result = LobbyProtocol::parse(
        R"({"type":"move_result","room_id":"room-1",)"
        R"("sequence":7,"accepted":false,)"
        R"("reason":"not_your_piece"})");
    REQUIRE(result.event.has_value());
    CHECK(std::get<MoveResultEvent>(*result.event).reason ==
          "not_your_piece");
}

TEST_CASE("Live native authentication and lobby transport") {
    if (std::getenv("CTD_RUN_LIVE_TRANSPORT_TEST") == nullptr) {
        return;
    }
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto username = "native_stage3e_" + suffix;
    const std::string password = "Stage3E-Test-Password!";

    LobbyClient client;
    REQUIRE(client.registerUser(username, password).succeeded());
    const auto login = client.login(username, password);
    REQUIRE(login.succeeded());
    REQUIRE(login.user.has_value());
    CHECK(login.user->username == username);
    REQUIRE(client.connectRealtime());
    REQUIRE(waitForState(
        client, WebSocketConnectionState::Connected));

    auto connected = waitForLobbyEvent(client);
    REQUIRE(connected.has_value());
    REQUIRE(connected->protocolEvent.has_value());
    CHECK(std::holds_alternative<ConnectedEvent>(
        *connected->protocolEvent));

    REQUIRE(client.ping());
    auto pong = waitForLobbyEvent(client);
    REQUIRE(pong.has_value());
    REQUIRE(pong->protocolEvent.has_value());
    CHECK(std::holds_alternative<PongEvent>(*pong->protocolEvent));

    REQUIRE(client.subscribeLobby());
    auto snapshot = waitForLobbyEvent(client);
    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->protocolEvent.has_value());
    CHECK(std::holds_alternative<LobbySnapshotEvent>(
        *snapshot->protocolEvent));

    REQUIRE(client.createRoom("Native Live Room", false));
    auto created = waitForLobbyEvent(client);
    REQUIRE(created.has_value());
    REQUIRE(created->protocolEvent.has_value());
    CHECK(std::holds_alternative<RoomCreatedEvent>(
        *created->protocolEvent));
    auto update = waitForLobbyEvent(client);
    REQUIRE(update.has_value());
    REQUIRE(update->protocolEvent.has_value());
    CHECK(std::holds_alternative<LobbySnapshotEvent>(
        *update->protocolEvent));

    REQUIRE(client.logout().succeeded());
    CHECK_FALSE(client.connectRealtime());

    SessionCookieJar emptyJar;
    WebSocketClient unauthenticated(emptyJar);
    REQUIRE(unauthenticated.connect("ws://127.0.0.1:8000/ws"));
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (unauthenticated.state() ==
               WebSocketConnectionState::Connecting &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(unauthenticated.state() !=
          WebSocketConnectionState::Connected);
    unauthenticated.disconnect();
}
