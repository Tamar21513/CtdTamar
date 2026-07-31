#include "Network/LobbyProtocol.hpp"

#include "Network/Protocol.hpp"

#include <boost/json.hpp>

#include <stdexcept>
#include <regex>

namespace ctd::network {
namespace {
namespace json = boost::json;

const json::object& requiredObject(
    const json::object& object,
    const char* key) {
    const auto* value = object.if_contains(key);
    if (!value || !value->is_object()) {
        throw std::invalid_argument(std::string("Missing object: ") + key);
    }
    return value->as_object();
}

const json::array& requiredArray(
    const json::object& object,
    const char* key) {
    const auto* value = object.if_contains(key);
    if (!value || !value->is_array()) {
        throw std::invalid_argument(std::string("Missing array: ") + key);
    }
    return value->as_array();
}

std::string requiredString(
    const json::object& object,
    const char* key) {
    const auto* value = object.if_contains(key);
    if (!value || !value->is_string()) {
        throw std::invalid_argument(std::string("Missing string: ") + key);
    }
    return std::string(value->as_string());
}

std::string requiredUtcTimestamp(
    const json::object& object, const char* key) {
    const auto value = requiredString(object, key);
    static const std::regex pattern(
        R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?Z$)");
    if (!std::regex_match(value, pattern)) {
        throw std::invalid_argument("Invalid UTC timestamp");
    }
    return value;
}

int optionalInteger(
    const json::object& object,
    const char* key) {
    const auto* value = object.if_contains(key);
    if (!value) {
        return 0;
    }
    if (!value->is_int64() || value->as_int64() < 0) {
        throw std::invalid_argument(std::string("Invalid integer: ") + key);
    }
    return static_cast<int>(value->as_int64());
}

unsigned long long requiredUnsigned(
    const json::object& object,
    const char* key) {
    const auto* value = object.if_contains(key);
    if (!value || !value->is_int64() || value->as_int64() < 0) {
        throw std::invalid_argument(std::string("Invalid integer: ") + key);
    }
    return static_cast<unsigned long long>(value->as_int64());
}

GameStateSnapshot parseSnapshot(const json::object& object) {
    json::object envelope{
        {"version", Protocol::VERSION},
        {"type", "game_state_updated"},
        {"sequence", 0},
        {"source", {{"row", -1}, {"col", -1}}},
        {"destination", {{"row", -1}, {"col", -1}}},
        {"accepted", true},
        {"reason", "gateway_snapshot"},
        {"playerColor", ""},
        {"reconnectToken", ""},
        {"username", ""},
        {"whiteUsername", ""},
        {"blackUsername", ""},
        {"secondsRemaining", 0},
        {"createdAtMs", 0},
        {"hasSnapshot", true},
        {"snapshot", object}};
    return Protocol::deserialize(json::serialize(envelope)).snapshot;
}

int optionalRating(const json::object& object) {
    const auto* value = object.if_contains("rating");
    if (!value) {
        return 1200;
    }
    if (!value->is_int64()) {
        throw std::invalid_argument("Invalid integer: rating");
    }
    return static_cast<int>(value->as_int64());
}

LobbyUser parseUser(const json::object& object) {
    return {requiredString(object, "id"),
            requiredString(object, "username"),
            optionalRating(object)};
}

std::optional<LobbyUser> optionalUser(
    const json::object& object,
    const char* key) {
    const auto* value = object.if_contains(key);
    if (!value || value->is_null()) {
        return std::nullopt;
    }
    if (!value->is_object()) {
        throw std::invalid_argument(std::string("Invalid user: ") + key);
    }
    return parseUser(value->as_object());
}

LobbyRoom parseRoom(const json::object& object) {
    LobbyRoom room;
    room.roomId = requiredString(object, "room_id");
    room.name = requiredString(object, "name");
    room.status = requiredString(object, "status");
    if (const auto* value = object.if_contains("visibility");
        value && value->is_string()) {
        room.visibility = std::string(value->as_string());
    }
    room.host = optionalUser(object, "host");
    room.white = optionalUser(object, "white");
    room.black = optionalUser(object, "black");
    if (const auto* value = object.if_contains("room_code");
        value && value->is_string()) {
        room.roomCode = std::string(value->as_string());
    }
    if (const auto* value = object.if_contains("created_at");
        value && value->is_string()) {
        room.createdAt = std::string(value->as_string());
    }
    room.spectatorCount = optionalInteger(object, "spectator_count");
    return room;
}

std::vector<LobbyRoom> parseRooms(const json::array& values) {
    std::vector<LobbyRoom> rooms;
    rooms.reserve(values.size());
    for (const auto& value : values) {
        if (!value.is_object()) {
            throw std::invalid_argument("Room must be an object");
        }
        rooms.push_back(parseRoom(value.as_object()));
    }
    return rooms;
}

std::string serialize(json::object object) {
    return json::serialize(object);
}

}  // namespace

std::string LobbyProtocol::ping() {
    return serialize({{"type", "ping"}});
}

std::string LobbyProtocol::subscribeLobby() {
    return serialize({{"type", "subscribe_lobby"}});
}

std::string LobbyProtocol::getLobby() {
    return serialize({{"type", "get_lobby"}});
}

std::string LobbyProtocol::createRoom(
    const std::string& name,
    bool hidden) {
    return serialize({
        {"type", "create_room"},
        {"name", name},
        {"visibility", hidden ? "hidden" : "public"}});
}

std::string LobbyProtocol::joinRoom(const std::string& roomId) {
    return serialize({{"type", "join_room"}, {"room_id", roomId}});
}

std::string LobbyProtocol::joinHiddenRoom(
    const std::string& roomCode) {
    return serialize({
        {"type", "join_hidden_room"},
        {"room_code", roomCode}});
}

std::string LobbyProtocol::watchGame(const std::string& roomId) {
    return serialize({{"type", "watch_game"}, {"room_id", roomId}});
}

std::string LobbyProtocol::leaveSpectator(
    const std::string& roomId) {
    return serialize({
        {"type", "leave_spectator"},
        {"room_id", roomId}});
}

std::string LobbyProtocol::findMatch() {
    return serialize({{"type", "find_match"}});
}

std::string LobbyProtocol::cancelFindMatch() {
    return serialize({{"type", "cancel_find_match"}});
}

std::string LobbyProtocol::jumpRequest(
    const std::string& roomId,
    unsigned long long sequence,
    int row,
    int col) {
    return serialize({
        {"type", "jump_request"},
        {"room_id", roomId},
        {"sequence", sequence},
        {"cell", {{"row", row}, {"col", col}}}});
}

std::string LobbyProtocol::moveRequest(
    const std::string& roomId,
    unsigned long long sequence,
    int sourceRow,
    int sourceCol,
    int destinationRow,
    int destinationCol) {
    return serialize({
        {"type", "move_request"},
        {"room_id", roomId},
        {"sequence", sequence},
        {"from", {{"row", sourceRow}, {"col", sourceCol}}},
        {"to", {{"row", destinationRow}, {"col", destinationCol}}}});
}

ProtocolParseResult LobbyProtocol::parse(
    const std::string& message) {
    try {
        const auto value = json::parse(message);
        if (!value.is_object()) {
            return {std::nullopt, "Message must be a JSON object"};
        }
        const auto& object = value.as_object();
        const auto type = requiredString(object, "type");
        if (type == "connected") {
            return {LobbyEvent{ConnectedEvent{
                parseUser(requiredObject(object, "user"))}}, {}};
        }
        if (type == "pong") {
            return {LobbyEvent{PongEvent{}}, {}};
        }
        if (type == "lobby_snapshot") {
            return {LobbyEvent{LobbySnapshotEvent{
                parseRooms(requiredArray(object, "waiting_rooms")),
                parseRooms(requiredArray(object, "active_games"))}}, {}};
        }
        if (type == "room_created") {
            return {LobbyEvent{RoomCreatedEvent{
                parseRoom(requiredObject(object, "room"))}}, {}};
        }
        if (type == "game_started") {
            return {LobbyEvent{GameStartedEvent{
                requiredString(object, "room_id"),
                requiredString(object, "name"),
                requiredString(object, "color"),
                parseUser(requiredObject(object, "opponent"))}}, {}};
        }
        if (type == "watching_game") {
            const auto white = optionalUser(object, "white");
            const auto black = optionalUser(object, "black");
            if (!white || !black) {
                throw std::invalid_argument(
                    "Watching game requires both players");
            }
            return {LobbyEvent{WatchingGameEvent{
                requiredString(object, "room_id"),
                requiredString(object, "name"),
                *white,
                *black,
                optionalInteger(object, "spectator_count")}}, {}};
        }
        if (type == "match_ready") {
            return {LobbyEvent{MatchReadyEvent{
                requiredString(object, "room_id"),
                requiredString(object, "color"),
                requiredString(object, "opponent"),
                requiredUnsigned(object, "revision"),
                parseSnapshot(requiredObject(object, "state")),
                requiredUtcTimestamp(object, "game_starts_at")}}, {}};
        }
        if (type == "match_countdown") {
            const auto* raw = object.if_contains("value");
            std::string value;
            if (raw && raw->is_int64() &&
                raw->as_int64() >= 1 && raw->as_int64() <= 3) {
                value = std::to_string(raw->as_int64());
            } else if (raw && raw->is_string() &&
                       raw->as_string() == "GO") {
                value = "GO";
            } else {
                throw std::invalid_argument("Invalid countdown value");
            }
            return {LobbyEvent{MatchCountdownEvent{
                requiredString(object, "room_id"), value,
                requiredUtcTimestamp(object, "game_starts_at")}}, {}};
        }
        if (type == "match_started") {
            return {LobbyEvent{MatchStartedEvent{
                requiredString(object, "room_id"),
                requiredUnsigned(object, "revision"),
                parseSnapshot(requiredObject(object, "state")),
                requiredUtcTimestamp(object, "game_starts_at")}}, {}};
        }
        if (type == "match_cancelled") {
            return {LobbyEvent{MatchCancelledEvent{
                requiredString(object, "room_id"),
                requiredString(object, "reason")}}, {}};
        }
        if (type == "match_snapshot") {
            return {LobbyEvent{MatchSnapshotEvent{
                requiredString(object, "room_id"),
                requiredUnsigned(object, "revision"),
                parseSnapshot(requiredObject(object, "state"))}}, {}};
        }
        if (type == "match_state") {
            return {LobbyEvent{MatchStateEvent{
                requiredString(object, "room_id"),
                requiredUnsigned(object, "revision"),
                parseSnapshot(requiredObject(object, "state"))}}, {}};
        }
        if (type == "move_result") {
            const auto* accepted = object.if_contains("accepted");
            if (!accepted || !accepted->is_bool()) {
                throw std::invalid_argument("Missing bool: accepted");
            }
            return {LobbyEvent{MoveResultEvent{
                requiredString(object, "room_id"),
                requiredUnsigned(object, "sequence"),
                accepted->as_bool(),
                requiredString(object, "reason")}}, {}};
        }
        if (type == "spectator_left") {
            return {LobbyEvent{SpectatorLeftEvent{
                requiredString(object, "room_id")}}, {}};
        }
        if (type == "opponent_disconnected") {
            return {LobbyEvent{OpponentDisconnectedEvent{
                requiredString(object, "room_id")}}, {}};
        }
        if (type == "room_status") {
            return {LobbyEvent{RoomStatusEvent{
                requiredString(object, "room_id"),
                requiredString(object, "status")}}, {}};
        }
        if (type == "error") {
            return {LobbyEvent{ProtocolErrorEvent{
                requiredString(object, "code"),
                requiredString(object, "message")}}, {}};
        }
        if (type == "searching") {
            return {LobbyEvent{SearchingEvent{}}, {}};
        }
        if (type == "match_not_found") {
            return {LobbyEvent{MatchNotFoundEvent{}}, {}};
        }
        if (type == "find_match_cancelled") {
            return {LobbyEvent{FindMatchCancelledEvent{}}, {}};
        }
        return {std::nullopt, "Unsupported message type"};
    } catch (const std::exception& error) {
        return {
            std::nullopt,
            std::string("Malformed lobby message: ") + error.what()};
    }
}

}  // namespace ctd::network
