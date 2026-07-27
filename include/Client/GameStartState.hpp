#ifndef GAME_START_STATE_HPP
#define GAME_START_STATE_HPP

#include "../Messaging/GameStateSnapshot.hpp"

#include <string>

enum class ClientUiPhase {
    UsernameEntry,
    Connecting,
    WaitingForOpponent,
    Countdown,
    Playing,
    Reconnecting,
    GameOver
};

struct GameStartDisplay {
    ClientUiPhase phase =
        ClientUiPhase::WaitingForOpponent;
    std::string text = "Waiting for another player...";
};

inline GameStartDisplay buildGameStartDisplay(
    bool playersReady,
    long long gameplayStartsAtEpochMs,
    long long nowEpochMs
) {
    if (!playersReady) {
        return {
            ClientUiPhase::WaitingForOpponent,
            "Waiting for another player..."
        };
    }

    const long long remaining =
        gameplayStartsAtEpochMs - nowEpochMs;
    if (remaining <= 0) {
        return {ClientUiPhase::Playing, ""};
    }
    if (remaining <= 1000) {
        return {ClientUiPhase::Countdown, "GO!"};
    }
    if (remaining <= 2000) {
        return {ClientUiPhase::Countdown, "1"};
    }
    if (remaining <= 3000) {
        return {ClientUiPhase::Countdown, "2"};
    }
    return {ClientUiPhase::Countdown, "3"};
}

class ClientUiPhaseMachine {
private:
    ClientUiPhase currentPhase;
    bool startObserved;
    bool playingReached;
    long long steadyStartMs;

public:
    ClientUiPhaseMachine()
        : currentPhase(ClientUiPhase::UsernameEntry),
          startObserved(false),
          playingReached(false),
          steadyStartMs(0) {
    }

    void markConnecting() {
        if (!playingReached) {
            currentPhase = ClientUiPhase::Connecting;
        }
    }

    void markReconnecting() {
        if (startObserved) {
            currentPhase = ClientUiPhase::Reconnecting;
        }
    }

    GameStartDisplay observe(
        const GameStateSnapshot& snapshot,
        long long nowEpochMs,
        long long nowSteadyMs
    ) {
        if (snapshot.gameOver) {
            currentPhase = ClientUiPhase::GameOver;
            return {currentPhase, ""};
        }

        if (playingReached) {
            currentPhase = ClientUiPhase::Playing;
            return {currentPhase, ""};
        }

        if (!startObserved && snapshot.playersReady) {
            startObserved = true;
            const long long remaining =
                snapshot.gameplayStartsAtEpochMs -
                nowEpochMs;
            steadyStartMs =
                nowSteadyMs +
                (remaining > 0 ? remaining : 0);
        }

        if (!startObserved) {
            currentPhase =
                ClientUiPhase::WaitingForOpponent;
            return {
                currentPhase,
                "Waiting for another player..."
            };
        }

        const GameStartDisplay display =
            buildGameStartDisplay(
                true,
                steadyStartMs,
                nowSteadyMs
            );
        currentPhase = display.phase;
        if (currentPhase == ClientUiPhase::Playing) {
            playingReached = true;
        }
        return display;
    }

    ClientUiPhase phase() const {
        return currentPhase;
    }

    bool hasStartedCountdown() const {
        return startObserved;
    }
};

#endif
