#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ctd::network {

struct LobbyUser {
    std::string id;
    std::string username;
};

struct LobbyRoom {
    std::string roomId;
    std::string visibility;
    std::string status;
    std::optional<LobbyUser> host;
    std::optional<LobbyUser> white;
    std::optional<LobbyUser> black;
    std::optional<std::string> roomCode;
    std::string createdAt;
    int spectatorCount = 0;
};

struct ConnectedEvent { LobbyUser user; };
struct PongEvent {};
struct LobbySnapshotEvent {
    std::vector<LobbyRoom> waitingRooms;
    std::vector<LobbyRoom> activeGames;
};
struct RoomCreatedEvent { LobbyRoom room; };
struct GameStartedEvent {
    std::string roomId;
    std::string color;
    LobbyUser opponent;
};
struct WatchingGameEvent {
    std::string roomId;
    LobbyUser white;
    LobbyUser black;
    int spectatorCount = 0;
};
struct OpponentDisconnectedEvent { std::string roomId; };
struct RoomStatusEvent {
    std::string roomId;
    std::string status;
};
struct ProtocolErrorEvent {
    std::string code;
    std::string message;
};

using LobbyEvent = std::variant<
    ConnectedEvent,
    PongEvent,
    LobbySnapshotEvent,
    RoomCreatedEvent,
    GameStartedEvent,
    WatchingGameEvent,
    OpponentDisconnectedEvent,
    RoomStatusEvent,
    ProtocolErrorEvent>;

struct ProtocolParseResult {
    std::optional<LobbyEvent> event;
    std::string error;
};

class LobbyProtocol {
public:
    static std::string ping();
    static std::string subscribeLobby();
    static std::string getLobby();
    static std::string createRoom(bool hidden);
    static std::string joinRoom(const std::string& roomId);
    static std::string joinHiddenRoom(const std::string& roomCode);
    static std::string watchGame(const std::string& roomId);
    static ProtocolParseResult parse(const std::string& message);
};

}  // namespace ctd::network
