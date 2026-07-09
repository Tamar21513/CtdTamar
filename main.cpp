#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <memory>

#include "Piece.cpp"

using namespace std;

using Board = vector<vector<unique_ptr<Piece>>>;

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
    if (token == ".")
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
            cout << "ERROR ROW_WIDTH_MISMATCH" << endl;
            return false;
        }

        for (size_t j = 0; j < numCols; j++) {
            if (!isAllowedToken(textBoard[i][j])) {
                cout << "ERROR UNKNOWN_TOKEN" << endl;
                return false;
            }
        }
    }

    return true;
}

// יצירת כלי מתוך טוקן
unique_ptr<Piece> createPieceFromToken(const string& token) {
    if (token == ".") {
        return nullptr;
    }

    string color = token.substr(0, 1);
    string type = token.substr(1, 1);

    return make_unique<Piece>(color, type, 6000);
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
    if (row < 0 || col < 0) {
        return false;
    }

    if (row >= (int)board.size()) {
        return false;
    }

    if (board.empty()) {
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

    return first->color == second->color;
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
                cout << ".";
            } else {
                cout << board[i][j]->token();
            }
        }

        cout << endl;
    }
}

// טיפול בלחיצה
void handleClick(Board& board,
                 int x,
                 int y,
                 bool& hasSelection,
                 int& selectedRow,
                 int& selectedCol,
                 long long currentTimeMs) {
    
    int row = y / 100;
    int col = x / 100;

    // לחיצה מחוץ ללוח
    if (!isInsideBoard(row, col, board)) {
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
    if (!isInsideBoard(selectedRow, selectedCol, board) ||
        board[selectedRow][selectedCol] == nullptr) {
        
        hasSelection = false;
        selectedRow = -1;
        selectedCol = -1;
        return;
    }

    // אם לחצו על כלי ידידותי — מחליפים בחירה
    if (isFriendlyPiece(board[selectedRow][selectedCol], board[row][col])) {
        selectedRow = row;
        selectedCol = col;
        return;
    }

    // אחרת: מעבר או אכילה
    board[row][col] = move(board[selectedRow][selectedCol]);
    board[row][col]->markMoved(currentTimeMs);

    board[selectedRow][selectedCol] = nullptr;

    hasSelection = false;
    selectedRow = -1;
    selectedCol = -1;
}

int main() {
    vector<vector<string>> textBoard;
    string line;
    bool readingBoard = false;

    // קריאת הלוח
    while (getline(cin, line)) {
        line = trim(line);

        if (line == "Board:") {
            readingBoard = true;
            continue;
        }

        if (line == "Commands:") {
            break;
        }

        if (readingBoard) {
            vector<string> row = splitLineToTokens(line);

            if (!row.empty()) {
                textBoard.push_back(row);
            }
        }
    }

    if (!ifBoardProperly(textBoard)) {
        return 0;
    }

    Board board = boardConstruction(textBoard);

    bool hasSelection = false;
    int selectedRow = -1;
    int selectedCol = -1;
    long long currentTimeMs = 0;

    string command;

    // קריאת הפקודות
    while (cin >> command) {
        if (command == "click") {
            int x, y;
            cin >> x >> y;

            handleClick(board, x, y, hasSelection, selectedRow, selectedCol, currentTimeMs);
        }

        else if (command == "wait") {
            long long ms;
            cin >> ms;

            currentTimeMs += ms;
        }

        else if (command == "print") {
            string what;
            cin >> what;

            if (what == "board") {
                printBoard(board);
            }
        }
    }

    return 0;
}