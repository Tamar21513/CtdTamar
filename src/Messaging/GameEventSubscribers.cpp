#include "../../include/Messaging/GameEventSubscribers.hpp"

MoveHistorySubscriber::MoveHistorySubscriber(EventBus& eventBus)
    : eventBus(eventBus),
      subscriptionId(eventBus.subscribe(
          GameEventType::MoveCompleted,
          [this](const GameEvent& event) {
              std::lock_guard<std::mutex> lock(this->mutex);
              ++completedMoveCount;
              lastCompletedMove = event;
          }
      )) {
}

MoveHistorySubscriber::~MoveHistorySubscriber() {
    eventBus.unsubscribe(subscriptionId);
}

std::size_t MoveHistorySubscriber::getCompletedMoveCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return completedMoveCount;
}

GameEvent MoveHistorySubscriber::getLastCompletedMove() const {
    std::lock_guard<std::mutex> lock(mutex);
    return lastCompletedMove;
}

ScoreSubscriber::ScoreSubscriber(EventBus& eventBus)
    : eventBus(eventBus),
      subscriptionId(eventBus.subscribe(
          GameEventType::ScoreUpdated,
          [this](const GameEvent& event) {
              std::lock_guard<std::mutex> lock(this->mutex);
              ++updateCount;
              whiteScore = event.whiteScore;
              blackScore = event.blackScore;
          }
      )) {
}

ScoreSubscriber::~ScoreSubscriber() {
    eventBus.unsubscribe(subscriptionId);
}

std::size_t ScoreSubscriber::getUpdateCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return updateCount;
}

int ScoreSubscriber::getWhiteScore() const {
    std::lock_guard<std::mutex> lock(mutex);
    return whiteScore;
}

int ScoreSubscriber::getBlackScore() const {
    std::lock_guard<std::mutex> lock(mutex);
    return blackScore;
}

LifecycleSubscriber::LifecycleSubscriber(EventBus& eventBus)
    : eventBus(eventBus),
      startedSubscriptionId(eventBus.subscribe(
          GameEventType::GameStarted,
          [this](const GameEvent&) {
              std::lock_guard<std::mutex> lock(this->mutex);
              ++startedCount;
          }
      )),
      endedSubscriptionId(eventBus.subscribe(
          GameEventType::GameEnded,
          [this](const GameEvent& event) {
              std::lock_guard<std::mutex> lock(this->mutex);
              ++endedCount;
              lastEndedEvent = event;
          }
      )) {
}

LifecycleSubscriber::~LifecycleSubscriber() {
    eventBus.unsubscribe(startedSubscriptionId);
    eventBus.unsubscribe(endedSubscriptionId);
}

std::size_t LifecycleSubscriber::getStartedCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return startedCount;
}

std::size_t LifecycleSubscriber::getEndedCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return endedCount;
}

GameEvent LifecycleSubscriber::getLastEndedEvent() const {
    std::lock_guard<std::mutex> lock(mutex);
    return lastEndedEvent;
}

GameLifecyclePublisher::GameLifecyclePublisher(EventBus& eventBus)
    : eventBus(eventBus),
      endedSubscriptionId(eventBus.subscribe(
          GameEventType::GameEnded,
          [this](const GameEvent&) {
              std::lock_guard<std::mutex> lock(this->mutex);
              ended = true;
          }
      )) {
}

GameLifecyclePublisher::~GameLifecyclePublisher() {
    eventBus.unsubscribe(endedSubscriptionId);
}

bool GameLifecyclePublisher::publishStarted() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (started) return false;
        started = true;
    }

    GameEvent event;
    event.type = GameEventType::GameStarted;
    eventBus.publish(event);
    return true;
}

bool GameLifecyclePublisher::publishEnded(
    const std::string& reason,
    bool hasWinner,
    PieceColor winner,
    int whiteScore,
    int blackScore
) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (ended) return false;
        ended = true;
    }

    GameEvent event;
    event.type = GameEventType::GameEnded;
    event.gameEndReason = reason;
    event.hasWinner = hasWinner;
    event.winner = winner;
    event.whiteScore = whiteScore;
    event.blackScore = blackScore;
    eventBus.publish(event);
    return true;
}

void GameLifecyclePublisher::reset() {
    std::lock_guard<std::mutex> lock(mutex);
    started = false;
    ended = false;
}
