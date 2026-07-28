#ifndef GAME_EVENT_SUBSCRIBERS_HPP
#define GAME_EVENT_SUBSCRIBERS_HPP

#include "EventBus.hpp"
#include <cstddef>
#include <mutex>

class MoveHistorySubscriber {
public:
    explicit MoveHistorySubscriber(EventBus& eventBus);
    ~MoveHistorySubscriber();

    std::size_t getCompletedMoveCount() const;
    GameEvent getLastCompletedMove() const;

private:
    EventBus& eventBus;
    EventBus::SubscriptionId subscriptionId;
    mutable std::mutex mutex;
    std::size_t completedMoveCount = 0;
    GameEvent lastCompletedMove;
};

class ScoreSubscriber {
public:
    explicit ScoreSubscriber(EventBus& eventBus);
    ~ScoreSubscriber();

    std::size_t getUpdateCount() const;
    int getWhiteScore() const;
    int getBlackScore() const;

private:
    EventBus& eventBus;
    EventBus::SubscriptionId subscriptionId;
    mutable std::mutex mutex;
    std::size_t updateCount = 0;
    int whiteScore = 0;
    int blackScore = 0;
};

class LifecycleSubscriber {
public:
    explicit LifecycleSubscriber(EventBus& eventBus);
    ~LifecycleSubscriber();

    std::size_t getStartedCount() const;
    std::size_t getEndedCount() const;
    GameEvent getLastEndedEvent() const;

private:
    EventBus& eventBus;
    EventBus::SubscriptionId startedSubscriptionId;
    EventBus::SubscriptionId endedSubscriptionId;
    mutable std::mutex mutex;
    std::size_t startedCount = 0;
    std::size_t endedCount = 0;
    GameEvent lastEndedEvent;
};

class GameLifecyclePublisher {
public:
    explicit GameLifecyclePublisher(EventBus& eventBus);
    ~GameLifecyclePublisher();

    bool publishStarted();
    bool publishEnded(
        const std::string& reason,
        bool hasWinner = false,
        PieceColor winner = PieceColor::White,
        int whiteScore = 0,
        int blackScore = 0
    );
    void reset();

private:
    EventBus& eventBus;
    EventBus::SubscriptionId endedSubscriptionId;
    std::mutex mutex;
    bool started = false;
    bool ended = false;
};

#endif
