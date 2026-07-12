#ifndef RULES_H
#define RULES_H

#include <string>
#include <functional>

using namespace std;

using CellOccupied = function<bool(int, int)>;

bool isSameCell(int fromRow, int fromCol, int toRow, int toCol);

bool kingRule(int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied);
bool rookRule(int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied);
bool bishopRule(int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied);
bool queenRule(int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied);
bool knightRule(int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied);

bool isLegalMoveByType(const string& type_piece, int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied);
bool isPathClear(int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied);

#endif