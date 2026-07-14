#include "../../include/Rules/RuleEngine.hpp"
#include "../../include/Rules/PieceRules.hpp"

bool RuleEngine::isLegalByPieceKind(const Board& board, const Position& source, const Position& destination) const {
    shared_ptr<Piece> piece = board.getPieceAt(source);

    if (piece == nullptr) {
        return false;
    }

    PieceKind kind = piece->getKind();

    if (kind == PieceKind::Rook) {
        return PieceRules::canMoveLikeRook(board, source, destination);
    }

    if (kind == PieceKind::Bishop) {
        return PieceRules::canMoveLikeBishop(board, source, destination);
    }

    if (kind == PieceKind::Queen) {
        return PieceRules::canMoveLikeQueen(board, source, destination);
    }

    if (kind == PieceKind::Knight) {
        return PieceRules::canMoveLikeKnight(source, destination);
    }

    if (kind == PieceKind::King) {
        return PieceRules::canMoveLikeKing(source, destination);
    }

    if (kind == PieceKind::Pawn) {
        return PieceRules::canMoveLikePawn(board, source, destination, piece->getColor());
    }

    return false;
}

MoveValidation RuleEngine::validateMove(const Board& board, const Position& source, const Position& destination) const {
    if (!board.isInside(source) || !board.isInside(destination)) {
        return {false, Reasons::OUTSIDE_BOARD};
    }

    shared_ptr<Piece> sourcePiece = board.getPieceAt(source);

    if (sourcePiece == nullptr) {
        return {false, Reasons::EMPTY_SOURCE};
    }

    shared_ptr<Piece> destinationPiece = board.getPieceAt(destination);

    if (destinationPiece != nullptr && destinationPiece->getColor() == sourcePiece->getColor()) {
        return {false, Reasons::FRIENDLY_DESTINATION};
    }

    if (!isLegalByPieceKind(board, source, destination)) {
        return {false, Reasons::ILLEGAL_PIECE_MOVE};
    }

    return {true, Reasons::OK};
}