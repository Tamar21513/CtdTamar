#include "Rooms/RoomManager.hpp"

#include <sodium.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace ctd::gateway {
namespace {

boost::json::object waitingRoomSummary(const Room& room) {
    return {
        {"room_id", room.id},
        {"host", publicUser(room.host)},
        {"visibility", room.visibility},
        {"status", room.status},
        {"created_at", room.createdAt}};
}

boost::json::object activeRoomSummary(const Room& room) {
    return {
        {"room_id", room.id},
        {"white", publicUser(*room.white)},
        {"black", publicUser(*room.black)},
        {"status", room.status},
        {"spectator_count", room.spectators.size()},
        {"created_at", room.createdAt}};
}

}  // namespace

RoomError::RoomError(std::string errorCode, std::string message)
    : std::runtime_error(std::move(message)), code(std::move(errorCode)) {}

boost::json::object publicUser(const RoomUser& user) {
    return {{"id", user.id}, {"username", user.username}};
}

std::string RoomManager::randomId() {
    std::array<unsigned char, 16> bytes{};
    randombytes_buf(bytes.data(), bytes.size());
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if (index == 4 || index == 6 || index == 8 || index == 10) {
            output << '-';
        }
        output << std::setw(2) << static_cast<int>(bytes[index]);
    }
    return output.str();
}

std::string RoomManager::now() {
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return std::to_string(seconds);
}

std::string RoomManager::uniqueCodeLocked() {
    static constexpr char Alphabet[] =
        "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    while (true) {
        std::string code(6, 'A');
        for (char& value : code) {
            value = Alphabet[randombytes_uniform(sizeof(Alphabet) - 1)];
        }
        if (codeToRoom_.find(code) == codeToRoom_.end()) {
            return code;
        }
    }
}

Room RoomManager::create(
    const RoomUser& host,
    const std::string& visibility) {
    if (visibility != "public" && visibility != "hidden") {
        throw RoomError(
            "invalid_visibility", "Visibility must be public or hidden.");
    }
    std::lock_guard lock(mutex_);
    if (playerRoom_.count(host.id)) {
        throw RoomError(
            "already_in_room", "User is already participating in a room.");
    }
    Room room;
    room.id = randomId();
    room.visibility = visibility;
    room.host = host;
    room.createdAt = now();
    if (visibility == "hidden") {
        room.code = uniqueCodeLocked();
        codeToRoom_[*room.code] = room.id;
    }
    playerRoom_[host.id] = room.id;
    rooms_[room.id] = room;
    return room;
}

Room RoomManager::joinLocked(Room& room, const RoomUser& guest) {
    if (room.host.id == guest.id) {
        throw RoomError(
            "cannot_join_own_room", "The host cannot join their own room.");
    }
    if (playerRoom_.count(guest.id)) {
        throw RoomError(
            "already_in_room", "User is already participating in a room.");
    }
    if (room.status != "waiting" || room.guest) {
        throw RoomError(
            "room_unavailable", "Room is no longer available.");
    }
    room.guest = guest;
    room.white = room.host;
    room.black = guest;
    room.status = "active";
    playerRoom_[guest.id] = room.id;
    if (room.code) {
        codeToRoom_.erase(*room.code);
    }
    return room;
}

Room RoomManager::joinPublic(
    const std::string& roomId,
    const RoomUser& guest) {
    std::lock_guard lock(mutex_);
    auto found = rooms_.find(roomId);
    if (found == rooms_.end()) {
        throw RoomError("room_not_found", "Room was not found.");
    }
    if (found->second.visibility != "public") {
        throw RoomError("room_not_public", "Room is not public.");
    }
    return joinLocked(found->second, guest);
}

Room RoomManager::joinHidden(
    const std::string& code,
    const RoomUser& guest) {
    const auto first = code.find_first_not_of(" \t\r\n");
    const auto last = code.find_last_not_of(" \t\r\n");
    std::string normalized = first == std::string::npos
        ? "" : code.substr(first, last - first + 1);
    std::transform(
        normalized.begin(), normalized.end(), normalized.begin(),
        [](unsigned char value) {
            return static_cast<char>(std::toupper(value));
        });
    std::lock_guard lock(mutex_);
    const auto mapped = codeToRoom_.find(normalized);
    if (mapped == codeToRoom_.end()) {
        throw RoomError(
            "invalid_room_code",
            "Room code is invalid or no longer available.");
    }
    return joinLocked(rooms_.at(mapped->second), guest);
}

Room RoomManager::watch(
    const std::string& roomId,
    const RoomUser& spectator) {
    std::lock_guard lock(mutex_);
    auto found = rooms_.find(roomId);
    if (found == rooms_.end()) {
        throw RoomError("room_not_found", "Room was not found.");
    }
    Room& room = found->second;
    if (room.status != "active") {
        throw RoomError(
            "room_not_active", "Only active games can be watched.");
    }
    if ((room.white && room.white->id == spectator.id) ||
        (room.black && room.black->id == spectator.id)) {
        throw RoomError(
            "player_cannot_spectate",
            "Players cannot spectate their own game.");
    }
    if (spectatorRoom_.count(spectator.connectionId)) {
        throw RoomError(
            "already_watching",
            "Connection is already watching a game.");
    }
    room.spectators[spectator.connectionId] = spectator;
    spectatorRoom_[spectator.connectionId] = room.id;
    return room;
}

boost::json::object RoomManager::lobbySnapshot() const {
    std::lock_guard lock(mutex_);
    boost::json::array waiting;
    boost::json::array active;
    for (const auto& [id, room] : rooms_) {
        (void)id;
        if (room.status == "waiting" && room.visibility == "public") {
            waiting.push_back(waitingRoomSummary(room));
        } else if (room.status == "active") {
            active.push_back(activeRoomSummary(room));
        }
    }
    return {
        {"type", "lobby_snapshot"},
        {"waiting_rooms", std::move(waiting)},
        {"active_games", std::move(active)}};
}

RoomCleanup RoomManager::removeConnection(
    const std::string& userId,
    std::size_t connectionId) {
    std::lock_guard lock(mutex_);
    RoomCleanup cleanup;
    const auto watched = spectatorRoom_.find(connectionId);
    if (watched != spectatorRoom_.end()) {
        auto room = rooms_.find(watched->second);
        if (room != rooms_.end()) {
            room->second.spectators.erase(connectionId);
            cleanup.lobbyChanged = true;
        }
        spectatorRoom_.erase(watched);
    }
    const auto membership = playerRoom_.find(userId);
    if (membership == playerRoom_.end()) {
        return cleanup;
    }
    auto found = rooms_.find(membership->second);
    if (found == rooms_.end()) {
        playerRoom_.erase(membership);
        return cleanup;
    }
    Room room = found->second;
    const bool ownsConnection =
        (room.host.id == userId &&
         room.host.connectionId == connectionId) ||
        (room.guest && room.guest->id == userId &&
         room.guest->connectionId == connectionId);
    if (!ownsConnection) {
        return cleanup;
    }
    rooms_.erase(found);
    if (room.code) codeToRoom_.erase(*room.code);
    playerRoom_.erase(room.host.id);
    if (room.guest) playerRoom_.erase(room.guest->id);
    for (const auto& [id, spectator] : room.spectators) {
        (void)spectator;
        spectatorRoom_.erase(id);
        cleanup.notifications.push_back({
            id,
            {{"type", "room_status"},
             {"room_id", room.id},
             {"status", "removed"}}});
    }
    if (room.status == "active") {
        const auto opponent =
            room.white && room.white->id == userId ? room.black : room.white;
        if (opponent) {
            cleanup.notifications.push_back({
                opponent->connectionId,
                {{"type", "opponent_disconnected"},
                 {"room_id", room.id}}});
        }
    }
    cleanup.lobbyChanged = true;
    return cleanup;
}

std::optional<Room> RoomManager::find(
    const std::string& roomId) const {
    std::lock_guard lock(mutex_);
    const auto found = rooms_.find(roomId);
    return found == rooms_.end()
        ? std::nullopt
        : std::optional<Room>(found->second);
}

}  // namespace ctd::gateway
