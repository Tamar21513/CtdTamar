#ifndef PIECE_RULES_HPP
#define PIECE_RULES_HPP

#include "Board.hpp"
#include "Position.hpp"
#include "Piece.hpp"

class PieceRules {
private:
    static bool isPathClear(const Board& board, const Position& source, const Position& destination);

public:
    static bool canMoveLikeRook(const Board& board, const Position& source, const Position& destination);
    static bool canMoveLikeBishop(const Board& board, const Position& source, const Position& destination);
    static bool canMoveLikeQueen(const Board& board, const Position& source, const Position& destination);
    static bool canMoveLikeKnight(const Position& source, const Position& destination);
    static bool canMoveLikeKing(const Position& source, const Position& destination);
    static bool canMoveLikePawn(const Board& board, const Position& source, const Position& destination, PieceColor color);
};

#endif