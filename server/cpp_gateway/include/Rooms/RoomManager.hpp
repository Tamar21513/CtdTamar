#pragma once

#include <boost/json.hpp>

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ctd::gateway {

struct RoomUser {
    std::string id;
    std::string username;
    std::size_t connectionId = 0;
};

struct Room {
    std::string id;
    std::string visibility;
    std::string status = "waiting";
    RoomUser host;
    std::optional<RoomUser> guest;
    std::optional<RoomUser> white;
    std::optional<RoomUser> black;
    std::optional<std::string> code;
    std::unordered_map<std::size_t, RoomUser> spectators;
    std::string createdAt;
};

class RoomError final : public std::runtime_error {
public:
    RoomError(std::string code, std::string message);
    const std::string code;
};

struct RoomCleanup {
    bool lobbyChanged = false;
    std::vector<std::pair<std::size_t, boost::json::object>> notifications;
};

class RoomManager {
public:
    Room create(
        const RoomUser& host,
        const std::string& visibility);
    Room joinPublic(
        const std::string& roomId,
        const RoomUser& guest);
    Room joinHidden(
        const std::string& code,
        const RoomUser& guest);
    Room watch(
        const std::string& roomId,
        const RoomUser& spectator);
    boost::json::object lobbySnapshot() const;
    RoomCleanup removeConnection(
        const std::string& userId,
        std::size_t connectionId);
    std::optional<Room> find(const std::string& roomId) const;

private:
    Room joinLocked(Room& room, const RoomUser& guest);
    std::string uniqueCodeLocked();
    static std::string randomId();
    static std::string now();

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Room> rooms_;
    std::unordered_map<std::string, std::string> codeToRoom_;
    std::unordered_map<std::string, std::string> playerRoom_;
    std::unordered_map<std::size_t, std::string> spectatorRoom_;
};

boost::json::object publicUser(const RoomUser& user);

}  // namespace ctd::gateway
