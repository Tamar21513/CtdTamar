#ifndef GAME_HPP
#define GAME_HPP
//תנועה שעדין לא הסתיימה
struct ActiveMove {int fromRow; int fromCol; int toRow; int toCol; long long startTimeMs; long long finishTimeMs;};

#include "BoardUtils.hpp"

bool isPieceMoving(const vector<ActiveMove>& activeMoves, int row, int col);
void clearSelection(bool& hasSelection, int& selectedRow, int& selectedCol);
void moveSelectedPiece(Board& board, int fromRow, int fromCol, int toRow, int toCol, long long currentTimeMs);
void startMove(vector<ActiveMove>& activeMoves, int fromRow, int fromCol, int toRow, int toCol, long long currentTimeMs);
void settleCompletedMoves(Board& board, vector<ActiveMove>& activeMoves, long long currentTimeMs);
void handleClick(Board& board, vector<ActiveMove>& activeMoves, int x, int y, bool& hasSelection, int& selectedRow, int& selectedCol, long long currentTimeMs);
void handleWait(Board& board, vector<ActiveMove>& activeMoves, long long& currentTimeMs, long long ms);
bool isCellOnRoute(int routeFromRow, int routeFromCol, int routeToRow, int routeToCol, int checkRow, int checkCol);
bool hasCommonRouteConflict(const vector<ActiveMove>& activeMoves, int fromRow, int fromCol, int toRow, int toCol);
void runCommands(Board& board);

#endif
