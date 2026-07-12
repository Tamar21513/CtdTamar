#include "Game.hpp"
#include "Config.hpp"
#include "Rules.hpp"

using namespace std;

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

//יצירת תנועה פעילה - עדין לא מזיזים את הכלי שבלוח
void startMove(vector<ActiveMove>& activeMoves, int fromRow, int fromCol, int toRow, int toCol, long long currentTimeMs){
    ActiveMove activeMove;

    activeMove.fromRow = fromRow;
    activeMove.fromCol = fromCol;
    activeMove.toRow = toRow;
    activeMove.toCol = toCol;
    activeMove.startTimeMs = currentTimeMs;
    activeMove.finishTimeMs = currentTimeMs+ Config::MOVE_TIME_PER_CELL_MS;

    activeMoves.push_back(activeMove);
}

//בדיקה אילו מהלכים הסתיימו
void settleCompletedMoves(Board& board, vector<ActiveMove>& activeMoves, long long currentTimeMs){
    vector<ActiveMove> stillMoving;

    for(size_t i = 0; i<activeMoves.size(); i++){
        ActiveMove moveData = activeMoves[i];

        if(currentTimeMs >= moveData.finishTimeMs){
            int fromRow = moveData.fromRow;
            int fromCol = moveData.fromCol;
            int toRaw = moveData.toRow;
            int toCol = moveData.toCol;

            if(isInsideBoard(fromRow, fromCol, board) && isInsideBoard(toRaw, toCol, board) && board[fromRow][fromCol] != nullptr)
                moveSelectedPiece(board, fromRow, fromCol, toRaw, toCol, currentTimeMs);
        }
        else{
            stillMoving.push_back(moveData);
        }
    }
    activeMoves = stillMoving;
}

// טיפול בלחיצה
void handleClick(Board& board,vector<ActiveMove>& activeMoves, int x, int y, bool& hasSelection, int& selectedRow, int& selectedCol, long long currentTimeMs) {
    int row = y / Config::CELL_SIZE;
    int col = x / Config::CELL_SIZE;

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

    // מציאת סוג הכלי
    string type_piece = board[selectedRow][selectedCol]->getType();
    // מציאת צבע הכלי
    string color_piece = board[selectedRow][selectedCol]->getColor();

    CellOccupied isOccupied = [&board](int checkRow, int checkCol) {
        return board[checkRow][checkCol] != nullptr;
    };

    CellColor getCellColor = [&board](int checkRow, int checkCol) {
        if (board[checkRow][checkCol] == nullptr)
            return string("");

        return board[checkRow][checkCol]->getColor();
    };

    // בדיקה האם מותר לכלי זה לבצע הזזה זו
    if (!isLegalMoveByType(type_piece, color_piece, selectedRow, selectedCol, row, col, isOccupied, getCellColor)) {
        clearSelection(hasSelection, selectedRow, selectedCol);
        return;
    }

    //  אחרת: לא מזיזים מיד. רק יוצרים תנועה פעילה 
    startMove(activeMoves, selectedRow, selectedCol, row, col, currentTimeMs);

    clearSelection(hasSelection, selectedRow, selectedCol);
}

// טיפול בהמתנה
void handleWait(Board& board, vector<ActiveMove>& activeMoves, long long& currentTimeMs, long long ms) {
    currentTimeMs += ms;

    settleCompletedMoves(board, activeMoves, currentTimeMs);
}

// הרצת הפקודות
void runCommands(Board& board) {
    bool hasSelection = false;
    int selectedRow = -1;
    int selectedCol = -1;
    long long currentTimeMs = 0;
    vector<ActiveMove> activeMoves;

    string command;

    while (cin >> command) {
        if (command == Config::CLICK_COMMAND) {
            int x, y;
            cin >> x >> y;

            handleClick(board, activeMoves, x, y, hasSelection, selectedRow, selectedCol, currentTimeMs);
        }

        else if (command == Config::WAIT_COMMAND) {
            long long ms;
            cin >> ms;

            handleWait(board, activeMoves, currentTimeMs, ms);
        }

        else if (command == Config::PRINT_COMMAND) {
            string what;
            cin >> what;

            if (what == Config::BOARD_WORD) {
                printBoard(board);
            }
        }
    }
}
