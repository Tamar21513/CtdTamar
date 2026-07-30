#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "ThirdParty/doctest.h"

#include "Auth/AuthService.hpp"
#include "Auth/PasswordHasher.hpp"
#include "Rooms/RoomManager.hpp"
#include "WebSocket/WebSocketHub.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <future>
#include <mutex>
#include <string>
#include <vector>

using namespace ctd::gateway;

TEST_CASE("username and password validation preserves client contract") {
    CHECK(AuthService::normalizeUsername(" Alice.Name ") == "alice.name");
    CHECK_THROWS_AS(
        AuthService::validateUsername("x"), std::invalid_argument);
    CHECK(AuthService::validPassword("StrongPassword!"));
    CHECK_FALSE(AuthService::validPassword("short"));
}

TEST_CASE("Argon2id password hashes verify without exposing the password") {
    PasswordHasher passwords;
    const std::string password = "Correct horse battery staple";
    const auto hash = passwords.hash(password);
    CHECK(hash.find(password) == std::string::npos);
    CHECK(passwords.verify(hash, password));
    CHECK_FALSE(passwords.verify(hash, "Wrong password"));
}

TEST_CASE("public and hidden room snapshots do not leak hidden waiting room") {
    RoomManager rooms;
    const auto publicRoom =
        rooms.create({"u1", "alice", 1}, "public");
    const auto hiddenRoom =
        rooms.create({"u2", "bob", 2}, "hidden");
    REQUIRE(hiddenRoom.code.has_value());
    CHECK(hiddenRoom.code->size() == 6);
    const auto snapshot = rooms.lobbySnapshot();
    CHECK(snapshot.at("waiting_rooms").as_array().size() == 1);
    CHECK(snapshot.at("waiting_rooms").as_array()[0]
              .as_object().at("room_id").as_string() == publicRoom.id);
}

TEST_CASE("join assigns deterministic colors and spectator is read only data") {
    RoomManager rooms;
    const auto waiting =
        rooms.create({"u1", "alice", 1}, "public");
    const auto active =
        rooms.joinPublic(waiting.id, {"u2", "bob", 2});
    REQUIRE(active.white.has_value());
    REQUIRE(active.black.has_value());
    CHECK(active.white->id == "u1");
    CHECK(active.black->id == "u2");
    const auto watched =
        rooms.watch(active.id, {"u3", "carol", 3});
    CHECK(watched.spectators.size() == 1);
    CHECK_THROWS_AS(
        rooms.watch(active.id, {"u1", "alice", 4}), RoomError);
}

TEST_CASE("hidden join normalizes code and active lobby does not leak it") {
    RoomManager rooms;
    const auto waiting =
        rooms.create({"u1", "alice", 1}, "hidden");
    REQUIRE(waiting.code.has_value());
    std::string lowercase = *waiting.code;
    std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(),
        [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
    const auto active =
        rooms.joinHidden("  " + lowercase + "  ", {"u2", "bob", 2});
    CHECK(active.status == "active");
    const auto games =
        rooms.lobbySnapshot().at("active_games").as_array();
    REQUIRE(games.size() == 1);
    CHECK_FALSE(games[0].as_object().contains("room_code"));
    CHECK_FALSE(games[0].as_object().contains("visibility"));
}

TEST_CASE("duplicate player participation is rejected") {
    RoomManager rooms;
    rooms.create({"u1", "alice", 1}, "public");
    CHECK_THROWS_AS(
        rooms.create({"u1", "alice", 9}, "hidden"), RoomError);
    const auto other = rooms.create({"u2", "bob", 2}, "public");
    CHECK_THROWS_AS(
        rooms.joinPublic(other.id, {"u1", "alice", 9}), RoomError);
}

TEST_CASE("only one racing join succeeds") {
    RoomManager rooms;
    const auto waiting =
        rooms.create({"host", "host", 1}, "public");
    auto first = std::async(std::launch::async, [&] {
        try {
            rooms.joinPublic(waiting.id, {"a", "a_user", 2});
            return true;
        } catch (const RoomError&) {
            return false;
        }
    });
    auto second = std::async(std::launch::async, [&] {
        try {
            rooms.joinPublic(waiting.id, {"b", "b_user", 3});
            return true;
        } catch (const RoomError&) {
            return false;
        }
    });
    CHECK(static_cast<int>(first.get()) +
              static_cast<int>(second.get()) == 1);
}

TEST_CASE("player disconnect removes room but spectator disconnect does not") {
    RoomManager rooms;
    auto waiting = rooms.create({"u1", "alice", 1}, "public");
    auto active = rooms.joinPublic(waiting.id, {"u2", "bob", 2});
    rooms.watch(active.id, {"u3", "carol", 3});
    const auto spectatorCleanup = rooms.removeConnection("u3", 3);
    CHECK(rooms.find(active.id).has_value());
    const auto playerCleanup = rooms.removeConnection("u1", 1);
    CHECK(playerCleanup.lobbyChanged);
    CHECK_FALSE(rooms.find(active.id).has_value());
}

TEST_CASE("waiting host disconnect removes room") {
    RoomManager rooms;
    const auto waiting = rooms.create({"u1", "alice", 1}, "public");
    const auto cleanup = rooms.removeConnection("u1", 1);
    CHECK(cleanup.lobbyChanged);
    CHECK(cleanup.notifications.empty());
    CHECK_FALSE(rooms.find(waiting.id).has_value());
}

TEST_CASE("active player disconnect notifies opponent and spectators") {
    RoomManager rooms;
    const auto waiting = rooms.create({"u1", "alice", 1}, "public");
    const auto active =
        rooms.joinPublic(waiting.id, {"u2", "bob", 2});
    rooms.watch(active.id, {"u3", "carol", 3});
    const auto cleanup = rooms.removeConnection("u1", 1);
    REQUIRE(cleanup.notifications.size() == 2);
    bool opponentNotified = false;
    bool spectatorNotified = false;
    for (const auto& [connection, message] : cleanup.notifications) {
        const auto type = std::string(message.at("type").as_string());
        opponentNotified |=
            connection == 2 && type == "opponent_disconnected";
        spectatorNotified |=
            connection == 3 && type == "room_status";
    }
    CHECK(opponentNotified);
    CHECK(spectatorNotified);
}

TEST_CASE("WebSocket hub supports simultaneous lobby subscribers") {
    WebSocketHub hub;
    std::atomic<int> deliveries{0};
    std::vector<std::future<void>> subscriptions;
    for (int index = 0; index < 16; ++index) {
        const auto id = hub.add([&](std::string message) {
            if (message == "snapshot") {
                ++deliveries;
            }
        });
        subscriptions.push_back(std::async(std::launch::async, [&, id] {
            hub.subscribeLobby(id);
        }));
    }
    for (auto& subscription : subscriptions) {
        subscription.get();
    }
    hub.broadcastLobby("snapshot");
    CHECK(deliveries == 16);
}
