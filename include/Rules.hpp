#ifndef RULES_H
#define RULES_H

#include <string>
#include <functional>

using namespace std;

using CellOccupied = function<bool(int, int)>;
using CellColor = function<string(int, int)>;

bool isSameCell(int fromRow, int fromCol, int toRow, int toCol);

bool kingRule(const string& color, int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied, CellColor getCellColor);
bool rookRule(const string& color, int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied, CellColor getCellColor);
bool bishopRule(const string& color, int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied, CellColor getCellColor);
bool queenRule(const string& color, int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied, CellColor getCellColor);
bool knightRule(const string& color, int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied, CellColor getCellColor);
bool pawnRule(const string& color, int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied, CellColor getCellColor);

bool isLegalMoveByType(const string& type_piece,const string& color_piece , int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied, CellColor getCellColor);
bool isPathClear(int fromRow, int fromCol, int toRow, int toCol, CellOccupied isOccupied);

#endif