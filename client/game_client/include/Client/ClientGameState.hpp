#ifndef CLIENT_GAME_STATE_HPP
#define CLIENT_GAME_STATE_HPP

#include "Messaging/Message.hpp"

#include <mutex>

// Stores the latest authoritative snapshot received
// from the game server.
class ClientGameState {
private:
    mutable std::mutex snapshotMutex;
    GameStateSnapshot currentSnapshot;
    bool initialized;

public:
    ClientGameState();

    // Applies the snapshot contained in a server message.
    // Returns false if the message has no snapshot.
    bool applyMessage(const Message& message);

    bool hasSnapshot() const;

    GameStateSnapshot copySnapshot() const;

    const PieceSnapshot*
    findPieceById(int pieceId) const;

    const PieceSnapshot*
    findPieceAt(
        const Position& position
    ) const;

    bool isGameOver() const;

    int getWhiteScore() const;
    int getBlackScore() const;
    std::string getWhiteUsername() const;
    std::string getBlackUsername() const;

    long long getServerTimeMs() const;
};

#endif
