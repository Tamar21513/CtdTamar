#pragma once

#include "Messaging/GameStateSnapshot.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ctd::network {

struct LobbyUser {
    std::string id;
    std::string username;
    int rating = 1200;
};

struct LobbyRoom {
    std::string roomId;
    std::string name;
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
    std::string roomName;
    std::string color;
    LobbyUser opponent;
};
struct WatchingGameEvent {
    std::string roomId;
    std::string roomName;
    LobbyUser white;
    LobbyUser black;
    int spectatorCount = 0;
};
struct MatchReadyEvent {
    std::string roomId;
    std::string color;
    std::string opponent;
    unsigned long long revision = 0;
    GameStateSnapshot state;
    std::string gameStartsAt;
};
struct MatchCountdownEvent {
    std::string roomId;
    std::string value;
    std::string gameStartsAt;
};
struct MatchStartedEvent {
    std::string roomId;
    unsigned long long revision = 0;
    GameStateSnapshot state;
    std::string gameStartsAt;
};
struct MatchCancelledEvent {
    std::string roomId;
    std::string reason;
};
struct MatchSnapshotEvent {
    std::string roomId;
    unsigned long long revision = 0;
    GameStateSnapshot state;
};
struct MatchStateEvent {
    std::string roomId;
    unsigned long long revision = 0;
    GameStateSnapshot state;
};
struct MoveResultEvent {
    std::string roomId;
    unsigned long long sequence = 0;
    bool accepted = false;
    std::string reason;
};
struct SpectatorLeftEvent { std::string roomId; };
struct OpponentDisconnectedEvent { std::string roomId; };
struct RoomStatusEvent {
    std::string roomId;
    std::string status;
};
struct ProtocolErrorEvent {
    std::string code;
    std::string message;
};
struct SearchingEvent {};
struct MatchNotFoundEvent {};
struct FindMatchCancelledEvent {};

using LobbyEvent = std::variant<
    ConnectedEvent,
    PongEvent,
    LobbySnapshotEvent,
    RoomCreatedEvent,
    GameStartedEvent,
    WatchingGameEvent,
    MatchReadyEvent,
    MatchCountdownEvent,
    MatchStartedEvent,
    MatchCancelledEvent,
    MatchSnapshotEvent,
    MatchStateEvent,
    MoveResultEvent,
    SpectatorLeftEvent,
    OpponentDisconnectedEvent,
    RoomStatusEvent,
    ProtocolErrorEvent,
    SearchingEvent,
    MatchNotFoundEvent,
    FindMatchCancelledEvent>;

struct ProtocolParseResult {
    std::optional<LobbyEvent> event;
    std::string error;
};

class LobbyProtocol {
public:
    static std::string ping();
    static std::string subscribeLobby();
    static std::string getLobby();
    static std::string createRoom(
        const std::string& name,
        bool hidden);
    static std::string joinRoom(const std::string& roomId);
    static std::string joinHiddenRoom(const std::string& roomCode);
    static std::string watchGame(const std::string& roomId);
    static std::string leaveSpectator(const std::string& roomId);
    static std::string findMatch();
    static std::string cancelFindMatch();
    static std::string jumpRequest(
        const std::string& roomId,
        unsigned long long sequence,
        int row,
        int col);
    static std::string moveRequest(
        const std::string& roomId,
        unsigned long long sequence,
        int sourceRow,
        int sourceCol,
        int destinationRow,
        int destinationCol);
    static ProtocolParseResult parse(const std::string& message);
};

}  // namespace ctd::network
