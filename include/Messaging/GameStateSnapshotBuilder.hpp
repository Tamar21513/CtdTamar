#ifndef GAME_STATE_SNAPSHOT_BUILDER_HPP
#define GAME_STATE_SNAPSHOT_BUILDER_HPP

#include "GameStateSnapshot.hpp"

class GameEngine;

// Converts the internal GameEngine state
// into data that can be sent through the network.
class GameStateSnapshotBuilder {
public:
    static GameStateSnapshot build(
        const GameEngine& engine
    );
};

#endif