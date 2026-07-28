#ifndef TEST_HELPERS_HPP
#define TEST_HELPERS_HPP

#include <memory>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Core/Board.hpp"
#include "IO/BoardParser.hpp"
#include "IO/BoardPrinter.hpp"
#include "Engine/GameEngine.hpp"
#include "Core/Piece.hpp"
#include "Core/Position.hpp"

inline std::shared_ptr<Piece> makePiece(int id, PieceColor color, PieceKind kind) {
    return std::make_shared<Piece>(id, color, kind);
}

inline Board parseBoard(std::initializer_list<std::string> rows) {
    return BoardParser::parse(std::vector<std::string>(rows));
}

inline std::string boardText(const Board& board) {
    return BoardPrinter::print(board);
}

inline std::string captureParserOutput(const std::vector<std::string>& rows, Board& parsed) {
    std::ostringstream captured;
    std::streambuf* oldBuffer = std::cout.rdbuf(captured.rdbuf());
    parsed = BoardParser::parse(rows);
    std::cout.rdbuf(oldBuffer);
    return captured.str();
}

inline void requireToken(const Board& board, int row, int col, const std::string& token) {
    std::shared_ptr<Piece> piece = board.getPieceAt(Position(row, col));
    REQUIRE(piece != nullptr);
    CHECK(piece->token() == token);
}

inline void requireEmpty(const Board& board, int row, int col) {
    CHECK(board.getPieceAt(Position(row, col)) == nullptr);
}

#endif
