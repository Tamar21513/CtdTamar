#include "../../include/Client/ClientGameState.hpp"

ClientGameState::ClientGameState()
    : currentSnapshot(),
      initialized(false) {
}

bool ClientGameState::applyMessage(
    const Message& message
) {
    if (!message.hasSnapshot) {
        return false;
    }

    // Do not allow an older network update
    // to overwrite a newer state.
    if (
        initialized &&
        message.snapshot.serverTimeMs <
            currentSnapshot.serverTimeMs
    ) {
        return false;
    }

    currentSnapshot = message.snapshot;
    initialized = true;

    return true;
}

bool ClientGameState::hasSnapshot() const {
    return initialized;
}

const GameStateSnapshot&
ClientGameState::getSnapshot() const {
    return currentSnapshot;
}

const PieceSnapshot*
ClientGameState::findPieceById(
    int pieceId
) const {
    if (!initialized) {
        return nullptr;
    }

    for (
        const PieceSnapshot& piece :
        currentSnapshot.pieces
    ) {
        if (piece.id == pieceId) {
            return &piece;
        }
    }

    return nullptr;
}

const PieceSnapshot*
ClientGameState::findPieceAt(
    const Position& position
) const {
    if (!initialized) {
        return nullptr;
    }

    for (
        const PieceSnapshot& piece :
        currentSnapshot.pieces
    ) {
        if (piece.position == position) {
            return &piece;
        }
    }

    return nullptr;
}

bool ClientGameState::isGameOver() const {
    return initialized &&
           currentSnapshot.gameOver;
}

int ClientGameState::getWhiteScore() const {
    return currentSnapshot.whiteScore;
}

int ClientGameState::getBlackScore() const {
    return currentSnapshot.blackScore;
}

long long
ClientGameState::getServerTimeMs() const {
    return currentSnapshot.serverTimeMs;
}