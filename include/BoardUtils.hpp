#ifndef BOARD_UTILS_HPP
#define BOARD_UTILS_HPP

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include "Piece.hpp"
using namespace std;

using Board = vector<vector<unique_ptr<Piece>>>;

string trim(const string& str);
bool isAllowedToken(const string& token);
vector<string> splitLineToTokens(const string& line);
bool ifBoardProperly(const vector<vector<string>>& textBoard);
unique_ptr<Piece> createPieceFromToken(const string& token);
Board boardConstruction(const vector<vector<string>>& textBoard);
bool isInsideBoard(int row, int col, const Board& board);
bool isFriendlyPiece(const unique_ptr<Piece>& first, const unique_ptr<Piece>& second);
void printBoard(const Board& board);
vector<vector<string>> readTextBoard();

#endif
