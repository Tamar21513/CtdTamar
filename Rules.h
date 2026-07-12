#ifndef RULES_H
#define RULES_H

#include <string>
using namespace std;

bool isSameCell(int fromRow, int fromCol, int toRow, int toCol);

bool kingRule(int fromRow, int fromCol, int toRow, int toCol);
bool rookRule(int fromRow, int fromCol, int toRow, int toCol);
bool bishopRule(int fromRow, int fromCol, int toRow, int toCol);
bool queenRule(int fromRow, int fromCol, int toRow, int toCol);
bool knightRule(int fromRow, int fromCol, int toRow, int toCol);

bool isLegalMoveByType(const string& type_piece, int fromRow, int fromCol, int toRow, int toCol);

#endif