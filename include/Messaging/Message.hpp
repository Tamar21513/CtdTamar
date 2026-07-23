#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <string>

#include "../Core/Position.hpp"
#include "GameStateSnapshot.hpp"

enum class MessageType {
    MoveRequest,
    JumpRequest,
    MoveAccepted,
    MoveRejected,
    GameStateUpdated,
    GameOver
};

struct Message {
    MessageType type = MessageType::MoveRejected;

    unsigned long long sequence = 0;

    Position source;
    Position destination;

    bool accepted = false;
    std::string reason;

    long long createdAtMs = 0;

    // Requests do not normally contain a snapshot.
    // Server responses and updates can contain one.
    bool hasSnapshot = false;

    GameStateSnapshot snapshot;
};

#endif