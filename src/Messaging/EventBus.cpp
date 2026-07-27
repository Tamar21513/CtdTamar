#include "../../include/Messaging/EventBus.hpp"
#include <utility>
#include <vector>

EventBus::SubscriptionId EventBus::subscribe(
    GameEventType type,
    EventHandler handler
) {
    std::lock_guard<std::mutex> lock(mutex);
    const SubscriptionId id = nextId++;
    subscriptions.emplace(id, Subscription{type, std::move(handler)});
    return id;
}

bool EventBus::unsubscribe(SubscriptionId id) {
    std::lock_guard<std::mutex> lock(mutex);
    return subscriptions.erase(id) != 0;
}

void EventBus::publish(const GameEvent& event) {
    std::vector<EventHandler> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& item : subscriptions) {
            if (item.second.type == event.type) {
                handlers.push_back(item.second.handler);
            }
        }
    }

    for (const EventHandler& handler : handlers) {
        if (handler) handler(event);
    }
}
