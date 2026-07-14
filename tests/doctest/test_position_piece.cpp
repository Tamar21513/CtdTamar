#include "doctest.h"
#include "Piece.hpp"
#include "Position.hpp"

TEST_SUITE("Position") {
    TEST_CASE("default position is invalid sentinel") {
        Position position;
        CHECK(position.getRow() == -1);
        CHECK(position.getCol() == -1);
        CHECK(position.toString() == "(-1,-1)");
    }

    TEST_CASE("position stores positive zero and negative coordinates") {
        Position zero(0, 0);
        Position positive(7, 11);
        Position negative(-3, -9);
        CHECK(zero.toString() == "(0,0)");
        CHECK(positive.toString() == "(7,11)");
        CHECK(negative.toString() == "(-3,-9)");
    }

    TEST_CASE("position equality compares both coordinates") {
        CHECK(Position(2, 3) == Position(2, 3));
        CHECK(Position(2, 3) != Position(3, 2));
        CHECK(Position(2, 3) != Position(2, 4));
        CHECK(Position(2, 3) != Position(1, 3));
    }
}

TEST_SUITE("Piece") {
    TEST_CASE("constructor stores all fields and starts idle") {
        Piece piece(42, PieceColor::White, PieceKind::Knight);
        CHECK(piece.getId() == 42);
        CHECK(piece.getColor() == PieceColor::White);
        CHECK(piece.getKind() == PieceKind::Knight);
        CHECK(piece.getState() == PieceState::Idle);
        CHECK(piece.token() == "wN");
    }

    TEST_CASE("piece state can move through every state") {
        Piece piece(1, PieceColor::Black, PieceKind::King);
        piece.setState(PieceState::Moving);
        CHECK(piece.getState() == PieceState::Moving);
        piece.setState(PieceState::Airborne);
        CHECK(piece.getState() == PieceState::Airborne);
        piece.setState(PieceState::Captured);
        CHECK(piece.getState() == PieceState::Captured);
        piece.setState(PieceState::Idle);
        CHECK(piece.getState() == PieceState::Idle);
    }

    TEST_CASE("piece kind can be changed for promotion") {
        Piece piece(8, PieceColor::White, PieceKind::Pawn);
        piece.setKind(PieceKind::Queen);
        CHECK(piece.getKind() == PieceKind::Queen);
        CHECK(piece.token() == "wQ");
    }

    TEST_CASE("every valid token round trips") {
        const char colors[] = {'w', 'b'};
        const char kinds[] = {'K', 'Q', 'R', 'B', 'N', 'P'};
        int id = 1;
        for (char color : colors) {
            for (char kind : kinds) {
                Piece piece(id++, Piece::colorFromChar(color), Piece::kindFromChar(kind));
                std::string expected;
                expected += color;
                expected += kind;
                CHECK(piece.token() == expected);
            }
        }
    }

    TEST_CASE("conversion fallbacks are deterministic") {
        CHECK(Piece::colorFromChar('w') == PieceColor::White);
        CHECK(Piece::colorFromChar('x') == PieceColor::Black);
        CHECK(Piece::kindFromChar('K') == PieceKind::King);
        CHECK(Piece::kindFromChar('Q') == PieceKind::Queen);
        CHECK(Piece::kindFromChar('R') == PieceKind::Rook);
        CHECK(Piece::kindFromChar('B') == PieceKind::Bishop);
        CHECK(Piece::kindFromChar('N') == PieceKind::Knight);
        CHECK(Piece::kindFromChar('x') == PieceKind::Pawn);
    }
}
