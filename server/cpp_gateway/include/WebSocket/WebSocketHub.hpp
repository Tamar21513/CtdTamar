#pragma once
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace ctd::gateway {
class WebSocketHub {
public:
    using Sender = std::function<void(std::string)>;
    std::size_t add(Sender sender);
    void remove(std::size_t id);
    void subscribeLobby(std::size_t id);
    void send(std::size_t id, const std::string& message);
    void broadcastLobby(const std::string& message);
private:
    std::mutex mutex_;
    std::size_t nextId_ = 1;
    std::unordered_map<std::size_t, Sender> senders_;
    std::unordered_set<std::size_t> lobby_;
};
}
