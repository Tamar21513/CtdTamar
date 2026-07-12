#include <string>
#include <unordered_map>
#include <functional>
#include <cmath>
#include "Rules.h"

using namespace std;

// בדיקה האם המקור והיעד הם אותו תא
bool isSameCell(int fromRow, int fromCol, int toRow, int toCol) {
    return fromRow == toRow && fromCol == toCol;
}

// חוק תזוזה של מלך
bool kingRule(int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied) {
    if (isSameCell(fromRow, fromCol, toRow, toCol))
        return false;

    if (abs(toRow - fromRow) <= 1 && abs(toCol - fromCol) <= 1)
        return true;

    return false;
}

// חוק תזוזה של צריח
bool rookRule(int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied) {
    if (isSameCell(fromRow, fromCol, toRow, toCol))
        return false;

    if (fromRow == toRow || fromCol == toCol)
        return isPathClear(fromRow, fromCol, toRow, toCol, isOccupied);

    return false;
}

// חוק תזוזה של רץ
bool bishopRule(int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied) {
    if (isSameCell(fromRow, fromCol, toRow, toCol))
        return false;

    if (abs(toRow - fromRow) == abs(toCol - fromCol))
        return isPathClear(fromRow, fromCol, toRow, toCol, isOccupied);

    return false;
}

// חוק תזוזה של מלכה
bool queenRule(int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied) {
    if (isSameCell(fromRow, fromCol, toRow, toCol))
        return false;

    if (rookRule(fromRow, fromCol, toRow, toCol, isOccupied) || bishopRule(fromRow, fromCol, toRow, toCol, isOccupied))
        return true;

    return false;
}

// חוק תזוזה של פרש
bool knightRule(int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied) {
    if (isSameCell(fromRow, fromCol, toRow, toCol))
        return false;

    int absRow = abs(toRow - fromRow);
    int absCol = abs(toCol - fromCol);

    if ((absRow == 2 && absCol == 1) || (absRow == 1 && absCol == 2))
        return true;

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
using MoveRule = function<bool(int, int, int, int,CellOccupied)>;

// יצירת מילון של סוג כלי -> פונקציית חוק
unordered_map<string, MoveRule> createMoveRules() {
    unordered_map<string, MoveRule> rules;

    rules["K"] = kingRule;
    rules["R"] = rookRule;
    rules["B"] = bishopRule;
    rules["Q"] = queenRule;
    rules["N"] = knightRule;

    return rules;
}

// בדיקה האם לכלי מסוג מסוים מותר לבצע את התזוזה
bool isLegalMoveByType(const string& type_piece, int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied) {
    static unordered_map<string, MoveRule> rules = createMoveRules();

    if (rules.find(type_piece) == rules.end())
        return false;

    return rules[type_piece](fromRow, fromCol, toRow, toCol, isOccupied);
}