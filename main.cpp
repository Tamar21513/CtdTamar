// Git repository: https://github.com/Tamar21513/CtdTamar

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <memory>

#include "classes/Piece.cpp"
#include "Rules.h"

using namespace std;

using Board = vector<vector<unique_ptr<Piece>>>;

const int CELL_SIZE = 100;
const long long DEFAULT_COOLDOWN_MS = 6000;

const string BOARD_HEADER = "Board:";
const string COMMANDS_HEADER = "Commands:";
const string CLICK_COMMAND = "click";
const string WAIT_COMMAND = "wait";
const string PRINT_COMMAND = "print";
const string BOARD_WORD = "board";
const string EMPTY_CELL = ".";

const string ERROR_UNKNOWN_TOKEN = "ERROR UNKNOWN_TOKEN";
const string ERROR_ROW_WIDTH_MISMATCH = "ERROR ROW_WIDTH_MISMATCH";

// מוחק רווחים מיותרים בתחילת ובסוף שורה
string trim(const string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");

    if (start == string::npos) {
        return "";
    }

    size_t end = str.find_last_not_of(" \t\r\n");

    return str.substr(start, end - start + 1);
}

// האם הטוקן הוא כלי משחק או נקודה
bool isAllowedToken(const string& token) {
    if (token == EMPTY_CELL)
        return true;

    if (token.size() != 2)
        return false;

    char color = token[0];
    char piece = token[1];

    bool validColor = color == 'w' || color == 'b';
    bool validPiece = piece == 'K' || piece == 'Q' || piece == 'R' ||
                      piece == 'B' || piece == 'N' || piece == 'P';

    return validColor && validPiece;
}

// חלוקת שורה לטוקנים
vector<string> splitLineToTokens(const string& line) {
    vector<string> tokens;
    string token;
    stringstream ss(line);

    while (ss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

// בדיקה האם הלוח תקין
bool ifBoardProperly(const vector<vector<string>>& textBoard) {
    if (textBoard.empty()) {
        return false;
    }

    size_t numCols = textBoard[0].size();

    for (size_t i = 0; i < textBoard.size(); i++) {
        if (textBoard[i].size() != numCols) {
            cout << ERROR_ROW_WIDTH_MISMATCH << endl;
            return false;
        }

        for (size_t j = 0; j < numCols; j++) {
            if (!isAllowedToken(textBoard[i][j])) {
                cout << ERROR_UNKNOWN_TOKEN << endl;
                return false;
            }
        }
    }

    return true;
}

// יצירת כלי מתוך טוקן
unique_ptr<Piece> createPieceFromToken(const string& token) {
    if (token == EMPTY_CELL) {
        return nullptr;
    }

    string color = token.substr(0, 1);
    string type = token.substr(1, 1);

    return make_unique<Piece>(color, type, DEFAULT_COOLDOWN_MS);
}

// בניית לוח אובייקטים
Board boardConstruction(const vector<vector<string>>& textBoard) {
    Board board;

    for (size_t i = 0; i < textBoard.size(); i++) {
        vector<unique_ptr<Piece>> row;

        for (size_t j = 0; j < textBoard[i].size(); j++) {
            row.push_back(createPieceFromToken(textBoard[i][j]));
        }

        board.push_back(move(row));
    }

    return board;
}

// בדיקה אם תא נמצא בתוך הלוח
bool isInsideBoard(int row, int col, const Board& board) {
    if (board.empty()) {
        return false;
    }

    if (row < 0 || col < 0) {
        return false;
    }

    if (row >= (int)board.size()) {
        return false;
    }

    if (col >= (int)board[0].size()) {
        return false;
    }

    return true;
}

// האם שני כלים הם מאותו צבע
bool isFriendlyPiece(const unique_ptr<Piece>& first, const unique_ptr<Piece>& second) {
    if (first == nullptr || second == nullptr) {
        return false;
    }

    return first->isSameColor(*second);
}

// הדפסת לוח
void printBoard(const Board& board) {
    size_t numCols = board[0].size();

    for (size_t i = 0; i < board.size(); i++) {
        for (size_t j = 0; j < numCols; j++) {
            if (j > 0) {
                cout << " ";
            }

            if (board[i][j] == nullptr) {
                cout << EMPTY_CELL;
            } else {
                cout << board[i][j]->token();
            }
        }

        cout << endl;
    }
}

// ביצוע מעבר של כלי
void moveSelectedPiece(Board& board, int fromRow, int fromCol, int toRow, int toCol, long long currentTimeMs) {
    board[toRow][toCol] = move(board[fromRow][fromCol]);
    board[toRow][toCol]->markMoved(currentTimeMs);

    board[fromRow][fromCol] = nullptr;
}

// איפוס בחירה
void clearSelection(bool& hasSelection, int& selectedRow, int& selectedCol) {
    hasSelection = false;
    selectedRow = -1;
    selectedCol = -1;
}

// טיפול בלחיצה
void handleClick(Board& board, int x, int y, bool& hasSelection, int& selectedRow, int& selectedCol, long long currentTimeMs) {
    int row = y / CELL_SIZE;
    int col = x / CELL_SIZE;

    // לחיצה מחוץ ללוח
    if (!isInsideBoard(row, col, board)) {
        clearSelection(hasSelection, selectedRow, selectedCol);
        return;
    }

    // אם אין כלי נבחר כרגע
    if (!hasSelection) {
        if (board[row][col] != nullptr) {
            hasSelection = true;
            selectedRow = row;
            selectedCol = col;
        }

        return;
    }

    // אם הבחירה הקודמת כבר לא תקינה
    if (!isInsideBoard(selectedRow, selectedCol, board) || board[selectedRow][selectedCol] == nullptr) {
        clearSelection(hasSelection, selectedRow, selectedCol);
        return;
    }

    // אם לחצו על כלי ידידותי — מחליפים בחירה
    if (isFriendlyPiece(board[selectedRow][selectedCol], board[row][col])) {
        selectedRow = row;
        selectedCol = col;
        return;
    }
    //מציאת סוג הכלי
    string type_piece = board[selectedRow][selectedCol]->getType();
    
    CellOccupied isOccupied = [&board](int checkRow, int checkCol) {
        return board[checkRow][checkCol] != nullptr;
    };
    
    //בדיקה האם נותר לכלי זה לבצע הזזה זו
    if (!isLegalMoveByType(type_piece, selectedRow, selectedCol, row, col, isOccupied)) {
        clearSelection(hasSelection, selectedRow, selectedCol);
        return;
    }

    // אחרת: מעבר או אכילה
    moveSelectedPiece(board, selectedRow, selectedCol, row, col, currentTimeMs);

    clearSelection(hasSelection, selectedRow, selectedCol);
}

// טיפול בהמתנה
void handleWait(long long& currentTimeMs, long long ms) {
    currentTimeMs += ms;
}

// קריאת הלוח הטקסטואלי
vector<vector<string>> readTextBoard() {
    vector<vector<string>> textBoard;
    string line;
    bool readingBoard = false;

    while (getline(cin, line)) {
        line = trim(line);

        if (line == BOARD_HEADER) {
            readingBoard = true;
            continue;
        }

        if (line == COMMANDS_HEADER) {
            break;
        }

        if (readingBoard) {
            vector<string> row = splitLineToTokens(line);

            if (!row.empty()) {
                textBoard.push_back(row);
            }
        }
    }

    return textBoard;
}

// הרצת הפקודות
void runCommands(Board& board) {
    bool hasSelection = false;
    int selectedRow = -1;
    int selectedCol = -1;
    long long currentTimeMs = 0;

    string command;

    while (cin >> command) {
        if (command == CLICK_COMMAND) {
            int x, y;
            cin >> x >> y;

            handleClick(board, x, y, hasSelection, selectedRow, selectedCol, currentTimeMs);
        }

        else if (command == WAIT_COMMAND) {
            long long ms;
            cin >> ms;

            handleWait(currentTimeMs, ms);
        }

        else if (command == PRINT_COMMAND) {
            string what;
            cin >> what;

            if (what == BOARD_WORD) {
                printBoard(board);
            }
        }
    }
}

int main() {
    vector<vector<string>> textBoard = readTextBoard();

    if (!ifBoardProperly(textBoard)) {
        return 0;
    }

    Board board = boardConstruction(textBoard);

    runCommands(board);

    return 0;
}