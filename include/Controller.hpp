#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <optional>

#include "GameEngine.hpp"
#include "BoardMapper.hpp"
#include "Position.hpp"
#include "Results.hpp"

using namespace std;

class Controller {
private:
    GameEngine& engine;
    optional<Position> selectedCell;

public:
    Controller(GameEngine& engine);
    ControllerResult click(int x, int y);
    bool hasSelection() const;
    optional<Position> getSelectedCell() const;
};

#endif