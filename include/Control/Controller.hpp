#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <optional>

#include "../Engine/GameEngine.hpp"
#include "../IO/BoardMapper.hpp"
#include "../Core/Position.hpp"
#include "../Core/Results.hpp"

using namespace std;

class Controller {
private:
    GameEngine& engine;
    optional<Position> selectedCell;

public:
    Controller(GameEngine& engine);
    ControllerResult click(int x, int y);
    ControllerResult jump(int x, int y);
    bool hasSelection() const;
    optional<Position> getSelectedCell() const;
};

#endif