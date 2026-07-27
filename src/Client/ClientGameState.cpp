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

    std::lock_guard<std::mutex> lock(
        snapshotMutex
    );

    if (
        initialized &&
        message.snapshot.serverTimeMs <
            currentSnapshot.serverTimeMs
    ) {
        return false;
    }

    if (
        initialized &&
        message.snapshot.serverTimeMs ==
            currentSnapshot.serverTimeMs &&
        !(
            !currentSnapshot.playersReady &&
            message.snapshot.playersReady
        ) &&
        !(
            !currentSnapshot.gameOver &&
            message.snapshot.gameOver
        )
    ) {
        return false;
    }

    if (
        initialized &&
        currentSnapshot.playersReady &&
        !message.snapshot.playersReady
    ) {
        return false;
    }

    if (
        initialized &&
        currentSnapshot.playersReady &&
        !currentSnapshot.pieces.empty() &&
        message.snapshot.pieces.empty() &&
        !message.snapshot.gameOver
    ) {
        return false;
    }

    currentSnapshot = message.snapshot;
    initialized = true;

    return true;
}

bool ClientGameState::hasSnapshot() const {
    std::lock_guard<std::mutex> lock(
        snapshotMutex
    );
    return initialized;
}

GameStateSnapshot
ClientGameState::copySnapshot() const {
    std::lock_guard<std::mutex> lock(
        snapshotMutex
    );
    return currentSnapshot;
}

const PieceSnapshot*
ClientGameState::findPieceById(
    int pieceId
) const {
    std::lock_guard<std::mutex> lock(
        snapshotMutex
    );
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
    std::lock_guard<std::mutex> lock(
        snapshotMutex
    );
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
    std::lock_guard<std::mutex> lock(
        snapshotMutex
    );
    return initialized &&
           currentSnapshot.gameOver;
}

int ClientGameState::getWhiteScore() const {
    std::lock_guard<std::mutex> lock(
        snapshotMutex
    );
    return currentSnapshot.whiteScore;
}

int ClientGameState::getBlackScore() const {
    std::lock_guard<std::mutex> lock(
        snapshotMutex
    );
    return currentSnapshot.blackScore;
}

std::string
ClientGameState::getWhiteUsername() const {
    std::lock_guard<std::mutex> lock(
        snapshotMutex
    );
    return currentSnapshot.whiteUsername;
}

std::string
ClientGameState::getBlackUsername() const {
    std::lock_guard<std::mutex> lock(
        snapshotMutex
    );
    return currentSnapshot.blackUsername;
}

long long
ClientGameState::getServerTimeMs() const {
    std::lock_guard<std::mutex> lock(
        snapshotMutex
    );
    return currentSnapshot.serverTimeMs;
}
