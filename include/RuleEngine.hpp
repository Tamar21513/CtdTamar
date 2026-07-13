#ifndef RULE_ENGINE_HPP
#define RULE_ENGINE_HPP

#include "Board.hpp"
#include "Position.hpp"
#include "Results.hpp"

class RuleEngine {
private:
    bool isLegalByPieceKind(const Board& board, const Position& source, const Position& destination) const;

public:
    MoveValidation validateMove(const Board& board, const Position& source, const Position& destination) const;
};

#endif