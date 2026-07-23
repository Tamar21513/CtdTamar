#include "../../include/IO/BoardPrinter.hpp"

// Implements print.
string BoardPrinter::print(const Board& board) {
    string result = "";

    for (int row = 0; row < board.getHeight(); row++) {
        for (int col = 0; col < board.getWidth(); col++) {
            if (col > 0) {
                result += " ";
            }

            shared_ptr<Piece> piece = board.getPieceAt(Position(row, col));

            if (piece == nullptr) {
                result += ".";
            } else {
                result += piece->token();
            }
        }

        result += "\n";
    }

    return result;
}
