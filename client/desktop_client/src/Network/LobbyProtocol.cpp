#include "Network/LobbyProtocol.hpp"

#include <boost/json.hpp>

#include <stdexcept>

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

LobbyUser parseUser(const json::object& object) {
    return {requiredString(object, "id"),
            requiredString(object, "username")};
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

std::string LobbyProtocol::createRoom(bool hidden) {
    return serialize({
        {"type", "create_room"},
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
                *white,
                *black,
                optionalInteger(object, "spectator_count")}}, {}};
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
        return {std::nullopt, "Unsupported message type"};
    } catch (const std::exception&) {
        return {std::nullopt, "Malformed lobby message"};
    }
}

}  // namespace ctd::network
