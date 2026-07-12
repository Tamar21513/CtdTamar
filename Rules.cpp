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
bool kingRule(int fromRow, int fromCol, int toRow, int toCol) {
    if (isSameCell(fromRow, fromCol, toRow, toCol))
        return false;

    if (abs(toRow - fromRow) <= 1 && abs(toCol - fromCol) <= 1)
        return true;

    return false;
}

// חוק תזוזה של צריח
bool rookRule(int fromRow, int fromCol, int toRow, int toCol) {
    if (isSameCell(fromRow, fromCol, toRow, toCol))
        return false;

    if (fromRow == toRow || fromCol == toCol)
        return true;

    return false;
}

// חוק תזוזה של רץ
bool bishopRule(int fromRow, int fromCol, int toRow, int toCol) {
    if (isSameCell(fromRow, fromCol, toRow, toCol))
        return false;

    if (abs(toRow - fromRow) == abs(toCol - fromCol))
        return true;

    return false;
}

// חוק תזוזה של מלכה
bool queenRule(int fromRow, int fromCol, int toRow, int toCol) {
    if (isSameCell(fromRow, fromCol, toRow, toCol))
        return false;

    if (rookRule(fromRow, fromCol, toRow, toCol) || bishopRule(fromRow, fromCol, toRow, toCol))
        return true;

    return false;
}

// חוק תזוזה של פרש
bool knightRule(int fromRow, int fromCol, int toRow, int toCol) {
    if (isSameCell(fromRow, fromCol, toRow, toCol))
        return false;

    int absRow = abs(toRow - fromRow);
    int absCol = abs(toCol - fromCol);

    if ((absRow == 2 && absCol == 1) || (absRow == 1 && absCol == 2))
        return true;

    return false;
}

// טיפוס של פונקציית חוק תזוזה
using MoveRule = function<bool(int, int, int, int)>;

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
bool isLegalMoveByType(const string& type_piece, int fromRow, int fromCol, int toRow, int toCol) {
    static unordered_map<string, MoveRule> rules = createMoveRules();

    if (rules.find(type_piece) == rules.end())
        return false;

    return rules[type_piece](fromRow, fromCol, toRow, toCol);
}