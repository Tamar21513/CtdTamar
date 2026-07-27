#include "../../include/ThirdParty/doctest.h"
#include "../../include/Client/PlayerDisplay.hpp"
#include "../../include/Client/GameStartState.hpp"
#include "../../include/Client/ClientGameState.hpp"
#include "../../include/Client/Username.hpp"
#include "../../include/Messaging/Message.hpp"
#include "../../include/Network/Protocol.hpp"

#include <sstream>
#include <atomic>
#include <thread>

TEST_SUITE("Username protocol") {
    TEST_CASE("valid username with spaces survives serialization") {
        Message request;
        request.type = MessageType::ReconnectRequest;
        request.username = "Ada Lovelace";

        const Message decoded =
            Protocol::deserialize(
                Protocol::serialize(request)
            );

        CHECK(decoded.username == "Ada Lovelace");
    }

    TEST_CASE("authoritative player names survive snapshot serialization") {
        Message update;
        update.type = MessageType::GameStateUpdated;
        update.hasSnapshot = true;
        update.snapshot.whiteUsername = "White Player";
        update.snapshot.blackUsername = "Black Player";

        const Message decoded =
            Protocol::deserialize(
                Protocol::serialize(update)
            );

        CHECK(
            decoded.snapshot.whiteUsername ==
            "White Player"
        );
        CHECK(
            decoded.snapshot.blackUsername ==
            "Black Player"
        );
    }

    TEST_CASE("empty and whitespace-only usernames are invalid") {
        CHECK_FALSE(Username::isValid(""));
        CHECK_FALSE(Username::isValid(" \t\r\n "));
        CHECK(Username::isValid("  Grace Hopper  "));
    }

    TEST_CASE("prompt preserves internal spaces and retries invalid input") {
        std::istringstream input(
            "   \n  Katherine Johnson  \n"
        );
        std::ostringstream output;

        CHECK(
            Username::prompt(input, output) ==
            "Katherine Johnson"
        );
    }

    TEST_CASE("player labels preserve color and waiting state") {
        const PlayerScoreDisplay waiting =
            buildPlayerScoreDisplay(
                "Alice",
                "",
                5,
                0
            );
        CHECK(waiting.bottomName == "Alice");
        CHECK(waiting.bottomScore == 5);
        CHECK(
            waiting.topName ==
            "Waiting for opponent"
        );

        const PlayerScoreDisplay ready =
            buildPlayerScoreDisplay(
                "Alice",
                "Bob",
                5,
                3
            );
        CHECK(ready.topName == "Bob");
        CHECK(ready.topScore == 3);
        CHECK(ready.bottomName == "Alice");
    }

    TEST_CASE("waiting countdown and playing occur in order") {
        const long long start = 10000;

        CHECK(
            buildGameStartDisplay(
                false,
                start,
                5000
            ).phase ==
                ClientUiPhase::WaitingForOpponent
        );
        CHECK(
            buildGameStartDisplay(
                true,
                start,
                6000
            ).text == "3"
        );
        CHECK(
            buildGameStartDisplay(
                true,
                start,
                7001
            ).text == "2"
        );
        CHECK(
            buildGameStartDisplay(
                true,
                start,
                8001
            ).text == "1"
        );
        CHECK(
            buildGameStartDisplay(
                true,
                start,
                9001
            ).text == "GO!"
        );
        CHECK(
            buildGameStartDisplay(
                true,
                start,
                10000
            ).phase == ClientUiPhase::Playing
        );
    }

    TEST_CASE("duplicate start does not restart countdown") {
        ClientUiPhaseMachine phases;
        GameStateSnapshot snapshot;
        snapshot.playersReady = false;

        CHECK(
            phases.observe(snapshot, 1000, 5000).phase ==
            ClientUiPhase::WaitingForOpponent
        );

        snapshot.playersReady = true;
        snapshot.gameplayStartsAtEpochMs = 5000;
        CHECK(phases.observe(snapshot, 1000, 5000).text == "3");
        CHECK(phases.hasStartedCountdown());

        snapshot.gameplayStartsAtEpochMs = 9000;
        CHECK(phases.observe(snapshot, 3000, 7000).text == "1");
        CHECK(
            phases.observe(snapshot, 5000, 9000).phase ==
            ClientUiPhase::Playing
        );
        CHECK(
            phases.observe(snapshot, 5000, 9001).phase ==
            ClientUiPhase::Playing
        );
    }

    TEST_CASE("partial player update cannot regress playing state") {
        ClientUiPhaseMachine phases;
        GameStateSnapshot snapshot;
        snapshot.playersReady = true;
        snapshot.gameplayStartsAtEpochMs = 1000;

        CHECK(
            phases.observe(snapshot, 1000, 5000).phase ==
            ClientUiPhase::Playing
        );

        snapshot.playersReady = false;
        snapshot.whiteUsername.clear();
        snapshot.blackUsername.clear();
        CHECK(
            phases.observe(snapshot, 1001, 5001).phase ==
            ClientUiPhase::Playing
        );
    }

    TEST_CASE("disconnect preserves pieces and reconnect skips countdown") {
        ClientGameState state;
        Message started;
        started.hasSnapshot = true;
        started.snapshot.playersReady = true;
        started.snapshot.gameplayStartsAtEpochMs = 1000;
        started.snapshot.serverTimeMs = 10;
        PieceSnapshot piece;
        piece.id = 7;
        piece.token = "wK";
        started.snapshot.pieces.push_back(piece);
        REQUIRE(state.applyMessage(started));

        ClientUiPhaseMachine phases;
        CHECK(
            phases.observe(
                state.copySnapshot(),
                1000,
                5000
            ).phase == ClientUiPhase::Playing
        );

        Message disconnect;
        disconnect.type = MessageType::Reconnecting;
        CHECK_FALSE(state.applyMessage(disconnect));
        CHECK(state.copySnapshot().pieces.size() == 1);

        phases.markReconnecting();
        CHECK(phases.phase() == ClientUiPhase::Reconnecting);
        CHECK(
            phases.observe(
                state.copySnapshot(),
                1001,
                5001
            ).phase == ClientUiPhase::Playing
        );
    }

    TEST_CASE("one complete snapshot is copied per frame") {
        ClientGameState state;
        Message initial;
        initial.hasSnapshot = true;
        REQUIRE(state.applyMessage(initial));

        std::atomic<int> published(0);
        std::atomic<int> acknowledged(0);
        std::thread writer([&]() {
            for (int value = 1; value <= 500; ++value) {
                Message update;
                update.hasSnapshot = true;
                update.snapshot.serverTimeMs = value;
                update.snapshot.whiteScore = value;
                update.snapshot.blackScore = value;
                state.applyMessage(update);
                published.store(value);
                while (
                    acknowledged.load() < value
                ) {
                    std::this_thread::yield();
                }
            }
        });

        for (int value = 1; value <= 500; ++value) {
            while (published.load() < value) {
                std::this_thread::yield();
            }
            const GameStateSnapshot frame =
                state.copySnapshot();
            CHECK(
                frame.whiteScore ==
                frame.blackScore
            );
            CHECK(frame.whiteScore == value);
            acknowledged.store(value);
        }

        writer.join();
        const GameStateSnapshot finalFrame =
            state.copySnapshot();
        CHECK(finalFrame.whiteScore == 500);
        CHECK(finalFrame.blackScore == 500);
    }

    TEST_CASE("regressive snapshot cannot clear an active board") {
        ClientGameState state;
        Message active;
        active.hasSnapshot = true;
        active.snapshot.playersReady = true;
        active.snapshot.serverTimeMs = 20;
        PieceSnapshot piece;
        piece.id = 1;
        piece.token = "wK";
        active.snapshot.pieces.push_back(piece);
        REQUIRE(state.applyMessage(active));

        Message staleWaiting;
        staleWaiting.hasSnapshot = true;
        staleWaiting.snapshot.serverTimeMs = 20;
        staleWaiting.snapshot.playersReady = false;
        CHECK_FALSE(state.applyMessage(staleWaiting));
        CHECK(state.copySnapshot().playersReady);
        CHECK(state.copySnapshot().pieces.size() == 1);
    }
}
