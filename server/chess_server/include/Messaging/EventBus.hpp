#ifndef EVENT_BUS_HPP
#define EVENT_BUS_HPP

#include "GameEvent.hpp"
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>

class EventBus {
public:
    using SubscriptionId = std::uint64_t;
    using EventHandler = std::function<void(const GameEvent&)>;

    SubscriptionId subscribe(GameEventType type, EventHandler handler);
    bool unsubscribe(SubscriptionId id);
    void publish(const GameEvent& event);

private:
    struct Subscription {
        GameEventType type;
        EventHandler handler;
    };

    std::mutex mutex;
    SubscriptionId nextId = 1;
    std::unordered_map<SubscriptionId, Subscription> subscriptions;
};

#endif
