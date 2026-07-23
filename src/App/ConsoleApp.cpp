#include "../../include/App/ConsoleApp.hpp"

#include <iostream>
#include <string>
#include <vector>

#include "../../include/IO/BoardParser.hpp"
#include "../../include/IO/BoardPrinter.hpp"
#include "../../include/Core/Config.hpp"
#include "../../include/Engine/GameEngine.hpp"
#include "../../include/Control/Controller.hpp"

using namespace std;

// Removes surrounding whitespace from one input line.
static string trim(const string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");

    if (start == string::npos) {
        return "";
    }

    size_t end = str.find_last_not_of(" \t\r\n");

    return str.substr(start, end - start + 1);
}

// Runs the command-line game loop.
void ConsoleApp::run() {
    vector<string> boardLines;
    string line;
    bool readingBoard = false;

    while (getline(cin, line)) {
        line = trim(line);

        if (line == Config::BOARD_HEADER || line == "Board") {
            readingBoard = true;
            continue;
        }

        if (line == Config::COMMANDS_HEADER || line == "Commands") {
            break;
        }

        if (readingBoard && !line.empty()) {
            boardLines.push_back(line);
        }
    }

    Board board = BoardParser::parse(boardLines);

    GameEngine engine(board);
    Controller controller(engine);

    string command;

    while (cin >> command) {
        if (command == "click") {
            int x;
            int y;
            cin >> x >> y;

            controller.click(x, y);
        }
        else if (command == "jump") {
            int x;
            int y;
            cin >> x >> y;

            controller.jump(x, y);
        }
        else if (command == "print") {
            string what;
            cin >> what;

            if (what == "board") {
                cout << BoardPrinter::print(engine.getBoard());
            }
        }
        else if (command == "wait") {
            long long ms;
            cin >> ms;

            engine.wait(ms);
        }
    }
}
