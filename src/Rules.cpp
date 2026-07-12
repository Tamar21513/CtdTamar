#include <string>
#include <unordered_map>
#include <functional>
#include <cmath>
#include "../include/Rules.hpp"

using namespace std;

// בדיקה האם המקור והיעד הם אותו תא
bool isSameCell(int fromRow, int fromCol, int toRow, int toCol) {
    return fromRow == toRow && fromCol == toCol;
}

// חוק תזוזה של מלך
bool kingRule(const string& color, int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied, CellColor getCellColor) {
    if (isSameCell(fromRow, fromCol, toRow, toCol))
        return false;

    if (abs(toRow - fromRow) <= 1 && abs(toCol - fromCol) <= 1)
        return true;

    return false;
}

// חוק תזוזה של צריח
bool rookRule(const string& color, int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied, CellColor getCellColor) {
    if (isSameCell(fromRow, fromCol, toRow, toCol))
        return false;

    if (fromRow == toRow || fromCol == toCol)
        return isPathClear(fromRow, fromCol, toRow, toCol, isOccupied);

    return false;
}

// חוק תזוזה של רץ
bool bishopRule(const string& color, int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied, CellColor getCellColor) {
    if (isSameCell(fromRow, fromCol, toRow, toCol))
        return false;

    if (abs(toRow - fromRow) == abs(toCol - fromCol))
        return isPathClear(fromRow, fromCol, toRow, toCol, isOccupied);

    return false;
}

// חוק תזוזה של מלכה
bool queenRule(const string& color, int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied, CellColor getCellColor) {
    if (isSameCell(fromRow, fromCol, toRow, toCol))
        return false;

    if (rookRule(color ,fromRow, fromCol, toRow, toCol, isOccupied, getCellColor) || bishopRule(color ,fromRow, fromCol, toRow, toCol, isOccupied , getCellColor))
        return true;

    return false;
}

// חוק תזוזה של פרש
bool knightRule(const string& color, int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied, CellColor getCellColor) {
    if (isSameCell(fromRow, fromCol, toRow, toCol))
        return false;

    int absRow = abs(toRow - fromRow);
    int absCol = abs(toCol - fromCol);

    if ((absRow == 2 && absCol == 1) || (absRow == 1 && absCol == 2))
        return true;

    return false;
}


//חוק תזוזת החייל
bool pawnRule(const string& color, int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied, CellColor getCellColor) {
    if (isSameCell(fromRow, fromCol, toRow, toCol))
        return false;

    int direction = 0;

    if (color == "w")
        direction = -1;
    else if (color == "b")
        direction = 1;
    else
        return false;

    int rowDiff = toRow - fromRow;
    int colDiff = toCol - fromCol;

    // תנועה קדימה: רק תא אחד, רק אם היעד ריק
    if (rowDiff == direction && colDiff == 0) {
        return !isOccupied(toRow, toCol);
    }

    // אכילה באלכסון: רק תא אחד קדימה באלכסון, רק אם יש שם אויב
    if (rowDiff == direction && abs(colDiff) == 1) {
        if (!isOccupied(toRow, toCol))
            return false;

        return getCellColor(toRow, toCol) != color;
    }

    return false;
}


//האם אין כלי המפריע לתזוזה
bool isPathClear(int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied) {
    int rowStep = 0;
    int colStep = 0;

    if (toRow > fromRow)
        rowStep = 1;
    else if (toRow < fromRow)
        rowStep = -1;

    if (toCol > fromCol)
        colStep = 1;
    else if (toCol < fromCol)
        colStep = -1;

    int currentRow = fromRow + rowStep;
    int currentCol = fromCol + colStep;

    while (currentRow != toRow || currentCol != toCol) {
        if (isOccupied(currentRow, currentCol))
            return false;

        currentRow += rowStep;
        currentCol += colStep;
    }

    return true;
}

// טיפוס של פונקציית חוק תזוזה
using MoveRule = function<bool(const string&, int, int, int, int, CellOccupied, CellColor)>;

// יצירת מילון של סוג כלי -> פונקציית חוק
unordered_map<string, MoveRule> createMoveRules() {
    unordered_map<string, MoveRule> rules;

    rules["K"] = kingRule;
    rules["R"] = rookRule;
    rules["B"] = bishopRule;
    rules["Q"] = queenRule;
    rules["N"] = knightRule;
    rules["P"] = pawnRule;

    return rules;
}

// בדיקה האם לכלי מסוג מסוים מותר לבצע את התזוזה
bool isLegalMoveByType(const string& type_piece,const string& color, int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied, CellColor getCellColor) {
    static unordered_map<string, MoveRule> rules = createMoveRules();

    if (rules.find(type_piece) == rules.end())
        return false;

    return rules[type_piece](color, fromRow, fromCol, toRow, toCol, isOccupied, getCellColor);
}