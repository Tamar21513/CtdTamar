// Git repository: https://github.com/Tamar21513/CtdTamar

#include "include/BoardUtils.hpp"
#include "include/Game.hpp"

int main() {
    vector<vector<string>> textBoard = readTextBoard();

    if (!ifBoardProperly(textBoard)) {
        return 0;
    }

    Board board = boardConstruction(textBoard);

    runCommands(board);

    return 0;
}
