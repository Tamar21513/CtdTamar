#include "../../include/ThirdParty/doctest.h"
#include "../../include/Messaging/EventBus.hpp"
#include "../../include/Messaging/GameEventSubscribers.hpp"
#include "TestHelpers.hpp"

#include <atomic>

TEST_SUITE("EventBus") {
    TEST_CASE("two move subscribers receive one event") {
        EventBus bus;
        int first = 0;
        int second = 0;
        bus.subscribe(GameEventType::MoveCompleted, [&](const GameEvent&) { ++first; });
        bus.subscribe(GameEventType::MoveCompleted, [&](const GameEvent&) { ++second; });

        GameEvent event;
        event.type = GameEventType::MoveCompleted;
        bus.publish(event);

        CHECK(first == 1);
        CHECK(second == 1);
    }

    TEST_CASE("subscribers receive only their event type") {
        EventBus bus;
        int scoreEvents = 0;
        bus.subscribe(GameEventType::ScoreUpdated, [&](const GameEvent&) { ++scoreEvents; });

        GameEvent event;
        event.type = GameEventType::MoveCompleted;
        bus.publish(event);

        CHECK(scoreEvents == 0);
    }

    TEST_CASE("unsubscribe stops future delivery") {
        EventBus bus;
        int calls = 0;
        const auto id = bus.subscribe(
            GameEventType::MoveCompleted,
            [&](const GameEvent&) { ++calls; }
        );
        CHECK(bus.unsubscribe(id));

        GameEvent event;
        event.type = GameEventType::MoveCompleted;
        bus.publish(event);
        CHECK(calls == 0);
    }

    TEST_CASE("handler can unsubscribe itself") {
        EventBus bus;
        int calls = 0;
        EventBus::SubscriptionId id = 0;
        id = bus.subscribe(
            GameEventType::MoveCompleted,
            [&](const GameEvent&) {
                ++calls;
                bus.unsubscribe(id);
            }
        );

        GameEvent event;
        event.type = GameEventType::MoveCompleted;
        bus.publish(event);
        bus.publish(event);
        CHECK(calls == 1);
    }

    TEST_CASE("sound requests are filtered by event type") {
        EventBus bus;
        int soundRequests = 0;
        bus.subscribe(
            GameEventType::SoundRequested,
            [&](const GameEvent& event) {
                ++soundRequests;
                CHECK(event.sound == "capture");
            }
        );

        GameEvent moveEvent;
        moveEvent.type = GameEventType::MoveCompleted;
        bus.publish(moveEvent);

        GameEvent soundEvent;
        soundEvent.type = GameEventType::SoundRequested;
        soundEvent.sound = "capture";
        bus.publish(soundEvent);

        CHECK(soundRequests == 1);
    }
}

TEST_SUITE("Event subscribers") {
    TEST_CASE("focused subscribers expose observed event state") {
        EventBus bus;
        MoveHistorySubscriber moves(bus);
        ScoreSubscriber scores(bus);
        LifecycleSubscriber lifecycle(bus);

        GameEvent move;
        move.type = GameEventType::MoveCompleted;
        move.destination = Position(2, 3);
        bus.publish(move);

        GameEvent score;
        score.type = GameEventType::ScoreUpdated;
        score.whiteScore = 5;
        score.blackScore = 3;
        bus.publish(score);

        GameEvent started;
        started.type = GameEventType::GameStarted;
        bus.publish(started);

        CHECK(moves.getCompletedMoveCount() == 1);
        CHECK(moves.getLastCompletedMove().destination == Position(2, 3));
        CHECK(scores.getUpdateCount() == 1);
        CHECK(scores.getWhiteScore() == 5);
        CHECK(scores.getBlackScore() == 3);
        CHECK(lifecycle.getStartedCount() == 1);
    }

    TEST_CASE("game start is published once until lifecycle reset") {
        EventBus bus;
        LifecycleSubscriber lifecycle(bus);
        GameLifecyclePublisher publisher(bus);

        CHECK(publisher.publishStarted());
        CHECK_FALSE(publisher.publishStarted());
        CHECK(lifecycle.getStartedCount() == 1);

        publisher.reset();
        CHECK(publisher.publishStarted());
        CHECK(lifecycle.getStartedCount() == 2);
    }

    TEST_CASE("game end is published once across engine and server closure") {
        EventBus bus;
        LifecycleSubscriber lifecycle(bus);
        GameLifecyclePublisher publisher(bus);
        GameEngine engine(parseBoard({"wR . bK"}), &bus);

        REQUIRE(engine.requestMove(Position(0, 0), Position(0, 2)).isAccepted);
        engine.wait(2000);

        CHECK_FALSE(publisher.publishEnded("opponent_did_not_reconnect"));
        CHECK(lifecycle.getEndedCount() == 1);
        CHECK(lifecycle.getLastEndedEvent().gameEndReason == "king_captured");
    }
}

TEST_SUITE("GameEngine events") {
    TEST_CASE("one completed move publishes once without score event") {
        EventBus bus;
        int moves = 0;
        int scores = 0;
        bus.subscribe(GameEventType::MoveCompleted, [&](const GameEvent&) { ++moves; });
        bus.subscribe(GameEventType::ScoreUpdated, [&](const GameEvent&) { ++scores; });
        GameEngine engine(parseBoard({"wR . ."}), &bus);

        REQUIRE(engine.requestMove(Position(0, 0), Position(0, 2)).isAccepted);
        engine.wait(2000);

        CHECK(moves == 1);
        CHECK(scores == 0);
    }

    TEST_CASE("one scored capture publishes one score event") {
        EventBus bus;
        int moves = 0;
        int scores = 0;
        int resultingWhiteScore = 0;
        bus.subscribe(GameEventType::MoveCompleted, [&](const GameEvent& event) {
            ++moves;
            CHECK(event.wasCapture);
            CHECK(event.hasCapturedPiece);
            CHECK(event.capturedPieceKind == PieceKind::Knight);
        });
        bus.subscribe(GameEventType::ScoreUpdated, [&](const GameEvent& event) {
            ++scores;
            resultingWhiteScore = event.whiteScore;
        });
        GameEngine engine(parseBoard({"wR . bN"}), &bus);

        REQUIRE(engine.requestMove(Position(0, 0), Position(0, 2)).isAccepted);
        engine.wait(2000);

        CHECK(moves == 1);
        CHECK(scores == 1);
        CHECK(resultingWhiteScore == 3);
    }

    TEST_CASE("king capture ends the game exactly once") {
        EventBus bus;
        int ended = 0;
        bus.subscribe(GameEventType::GameEnded, [&](const GameEvent& event) {
            ++ended;
            CHECK(event.hasWinner);
            CHECK(event.winner == PieceColor::White);
        });
        GameEngine engine(parseBoard({"wR . bK"}), &bus);

        REQUIRE(engine.requestMove(Position(0, 0), Position(0, 2)).isAccepted);
        engine.wait(2000);
        engine.wait(2000);

        CHECK(ended == 1);
    }
}
