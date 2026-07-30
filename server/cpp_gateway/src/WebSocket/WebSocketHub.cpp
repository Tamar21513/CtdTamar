#include "WebSocket/WebSocketHub.hpp"
#include <vector>
namespace ctd::gateway {
std::size_t WebSocketHub::add(Sender sender) {
    std::lock_guard lock(mutex_);
    const auto id = nextId_++;
    senders_[id] = std::move(sender);
    return id;
}
void WebSocketHub::remove(std::size_t id) {
    std::lock_guard lock(mutex_);
    senders_.erase(id); lobby_.erase(id);
}
void WebSocketHub::subscribeLobby(std::size_t id) {
    std::lock_guard lock(mutex_);
    if (senders_.count(id)) lobby_.insert(id);
}
void WebSocketHub::send(std::size_t id, const std::string& message) {
    Sender sender;
    { std::lock_guard lock(mutex_);
      const auto found = senders_.find(id);
      if (found == senders_.end()) return;
      sender = found->second; }
    sender(message);
}
void WebSocketHub::broadcastLobby(const std::string& message) {
    std::vector<Sender> senders;
    { std::lock_guard lock(mutex_);
      for (auto id : lobby_) if (senders_.count(id)) senders.push_back(senders_[id]); }
    for (auto& sender : senders) sender(message);
}
}
