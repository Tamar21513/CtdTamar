#include "../../include/ThirdParty/doctest.h"
#include "../../include/Rules/PieceRules.hpp"
#include "../../include/Rules/RuleEngine.hpp"
#include "../../include/Core/Results.hpp"
#include "TestHelpers.hpp"

TEST_SUITE("Rook bishop queen rules") {
    TEST_CASE("rook supports every straight direction") {
        Board board(8, 8);
        CHECK(PieceRules::canMoveLikeRook(board, Position(3, 3), Position(3, 7)));
        CHECK(PieceRules::canMoveLikeRook(board, Position(3, 3), Position(3, 0)));
        CHECK(PieceRules::canMoveLikeRook(board, Position(3, 3), Position(7, 3)));
        CHECK(PieceRules::canMoveLikeRook(board, Position(3, 3), Position(0, 3)));
        CHECK_FALSE(PieceRules::canMoveLikeRook(board, Position(3, 3), Position(3, 3)));
        CHECK_FALSE(PieceRules::canMoveLikeRook(board, Position(3, 3), Position(4, 4)));
    }

    TEST_CASE("rook destination may be occupied but intermediate square may not") {
        Board board(1, 4);
        board.placePiece(Position(0, 3), makePiece(1, PieceColor::Black, PieceKind::King));
        CHECK(PieceRules::canMoveLikeRook(board, Position(0, 0), Position(0, 3)));
        board.placePiece(Position(0, 1), makePiece(2, PieceColor::White, PieceKind::Pawn));
        CHECK_FALSE(PieceRules::canMoveLikeRook(board, Position(0, 0), Position(0, 3)));
    }

    TEST_CASE("bishop supports all diagonals and rejects non diagonal") {
        Board board(8, 8);
        CHECK(PieceRules::canMoveLikeBishop(board, Position(3, 3), Position(7, 7)));
        CHECK(PieceRules::canMoveLikeBishop(board, Position(3, 3), Position(0, 0)));
        CHECK(PieceRules::canMoveLikeBishop(board, Position(3, 3), Position(0, 6)));
        CHECK(PieceRules::canMoveLikeBishop(board, Position(3, 3), Position(6, 0)));
        CHECK_FALSE(PieceRules::canMoveLikeBishop(board, Position(3, 3), Position(3, 3)));
        CHECK_FALSE(PieceRules::canMoveLikeBishop(board, Position(3, 3), Position(5, 6)));
    }

    TEST_CASE("bishop path blocking checks every intermediate cell") {
        Board board(5, 5);
        CHECK(PieceRules::canMoveLikeBishop(board, Position(0, 0), Position(4, 4)));
        board.placePiece(Position(1, 1), makePiece(1, PieceColor::White, PieceKind::Pawn));
        CHECK_FALSE(PieceRules::canMoveLikeBishop(board, Position(0, 0), Position(4, 4)));
        board.removePiece(Position(1, 1));
        board.placePiece(Position(3, 3), makePiece(2, PieceColor::Black, PieceKind::Pawn));
        CHECK_FALSE(PieceRules::canMoveLikeBishop(board, Position(0, 0), Position(4, 4)));
    }

    TEST_CASE("queen combines rook and bishop") {
        Board board(8, 8);
        CHECK(PieceRules::canMoveLikeQueen(board, Position(4, 4), Position(4, 0)));
        CHECK(PieceRules::canMoveLikeQueen(board, Position(4, 4), Position(0, 4)));
        CHECK(PieceRules::canMoveLikeQueen(board, Position(4, 4), Position(0, 0)));
        CHECK_FALSE(PieceRules::canMoveLikeQueen(board, Position(4, 4), Position(6, 5)));
        CHECK_FALSE(PieceRules::canMoveLikeQueen(board, Position(4, 4), Position(4, 4)));
    }
}

TEST_SUITE("Knight and king rules") {
    TEST_CASE("all eight knight moves are legal") {
        Position source(4, 4);
        const Position destinations[] = {
            Position(2, 3), Position(2, 5), Position(3, 2), Position(3, 6),
            Position(5, 2), Position(5, 6), Position(6, 3), Position(6, 5)
        };
        for (const Position& destination : destinations) {
            CHECK(PieceRules::canMoveLikeKnight(source, destination));
        }
    }

    TEST_CASE("knight rejects common wrong shapes") {
        Position source(4, 4);
        CHECK_FALSE(PieceRules::canMoveLikeKnight(source, source));
        CHECK_FALSE(PieceRules::canMoveLikeKnight(source, Position(5, 5)));
        CHECK_FALSE(PieceRules::canMoveLikeKnight(source, Position(4, 6)));
        CHECK_FALSE(PieceRules::canMoveLikeKnight(source, Position(7, 4)));
    }

    TEST_CASE("king supports all adjacent cells only") {
        Position source(4, 4);
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                Position destination(4 + dr, 4 + dc);
                if (dr == 0 && dc == 0) {
                    CHECK_FALSE(PieceRules::canMoveLikeKing(source, destination));
                } else {
                    CHECK(PieceRules::canMoveLikeKing(source, destination));
                }
            }
        }
        CHECK_FALSE(PieceRules::canMoveLikeKing(source, Position(6, 4)));
        CHECK_FALSE(PieceRules::canMoveLikeKing(source, Position(4, 2)));
    }
}

TEST_SUITE("Pawn rules") {
    TEST_CASE("white pawn one and two steps from start") {
        Board board(8, 8);
        CHECK(PieceRules::canMoveLikePawn(board, Position(6, 3), Position(5, 3), PieceColor::White));
        CHECK(PieceRules::canMoveLikePawn(board, Position(6, 3), Position(4, 3), PieceColor::White));
        CHECK_FALSE(PieceRules::canMoveLikePawn(board, Position(5, 3), Position(3, 3), PieceColor::White));
        CHECK_FALSE(PieceRules::canMoveLikePawn(board, Position(6, 3), Position(7, 3), PieceColor::White));
    }

    TEST_CASE("black pawn one and two steps from start") {
        Board board(8, 8);
        CHECK(PieceRules::canMoveLikePawn(board, Position(1, 3), Position(2, 3), PieceColor::Black));
        CHECK(PieceRules::canMoveLikePawn(board, Position(1, 3), Position(3, 3), PieceColor::Black));
        CHECK_FALSE(PieceRules::canMoveLikePawn(board, Position(2, 3), Position(4, 3), PieceColor::Black));
        CHECK_FALSE(PieceRules::canMoveLikePawn(board, Position(1, 3), Position(0, 3), PieceColor::Black));
    }

    TEST_CASE("pawn cannot move through or into occupied forward cells") {
        Board board(8, 8);
        board.placePiece(Position(5, 3), makePiece(1, PieceColor::Black, PieceKind::Knight));
        CHECK_FALSE(PieceRules::canMoveLikePawn(board, Position(6, 3), Position(5, 3), PieceColor::White));
        CHECK_FALSE(PieceRules::canMoveLikePawn(board, Position(6, 3), Position(4, 3), PieceColor::White));
        board.removePiece(Position(5, 3));
        board.placePiece(Position(4, 3), makePiece(2, PieceColor::Black, PieceKind::Knight));
        CHECK_FALSE(PieceRules::canMoveLikePawn(board, Position(6, 3), Position(4, 3), PieceColor::White));
    }

    TEST_CASE("pawn diagonal requires enemy and supports both sides") {
        Board board(8, 8);
        board.placePiece(Position(5, 2), makePiece(1, PieceColor::Black, PieceKind::Rook));
        board.placePiece(Position(5, 4), makePiece(2, PieceColor::Black, PieceKind::Rook));
        CHECK(PieceRules::canMoveLikePawn(board, Position(6, 3), Position(5, 2), PieceColor::White));
        CHECK(PieceRules::canMoveLikePawn(board, Position(6, 3), Position(5, 4), PieceColor::White));
        CHECK(PieceRules::canMoveLikePawn(board, Position(6, 3), Position(5, 3), PieceColor::White));
        CHECK_FALSE(PieceRules::canMoveLikePawn(board, Position(6, 3), Position(5, 5), PieceColor::White));
    }

    TEST_CASE("pawn cannot capture friendly piece diagonally") {
        Board board(8, 8);
        board.placePiece(Position(5, 2), makePiece(1, PieceColor::White, PieceKind::Rook));
        CHECK_FALSE(PieceRules::canMoveLikePawn(board, Position(6, 3), Position(5, 2), PieceColor::White));
    }
}

TEST_SUITE("RuleEngine") {
    TEST_CASE("validation error precedence") {
        RuleEngine rules;
        Board board = parseBoard({"wK .", ". bK"});
        CHECK(rules.validateMove(board, Position(-1, 0), Position(0, 0)).reason == Reasons::OUTSIDE_BOARD);
        CHECK(rules.validateMove(board, Position(0, 1), Position(1, 1)).reason == Reasons::EMPTY_SOURCE);
        CHECK(rules.validateMove(board, Position(0, 0), Position(0, 0)).reason == Reasons::FRIENDLY_DESTINATION);
    }

    TEST_CASE("friendly destination and enemy destination") {
        RuleEngine rules;
        Board board = parseBoard({"wR wN bN"});
        MoveValidation friendly = rules.validateMove(board, Position(0, 0), Position(0, 1));
        CHECK_FALSE(friendly.isValid);
        CHECK(friendly.reason == Reasons::FRIENDLY_DESTINATION);

        board.removePiece(Position(0, 1));
        MoveValidation enemy = rules.validateMove(board, Position(0, 0), Position(0, 2));
        CHECK(enemy.isValid);
        CHECK(enemy.reason == Reasons::OK);
    }

    TEST_CASE("every piece kind is dispatched") {
        RuleEngine rules;
        struct Case { std::string row; Position source; Position destination; };
        const Case cases[] = {
            {"wK .", Position(0,0), Position(0,1)},
            {"wQ .", Position(0,0), Position(0,1)},
            {"wR .", Position(0,0), Position(0,1)},
            {"wB .|. .", Position(0,0), Position(1,1)},
            {"wN . .|. . .|. . .", Position(0,0), Position(2,1)},
            {". .|wP .|. .", Position(1,0), Position(0,0)}
        };
        for (const Case& item : cases) {
            std::vector<std::string> rows;
            std::stringstream ss(item.row);
            std::string row;
            while (std::getline(ss, row, '|')) rows.push_back(row);
            Board board = BoardParser::parse(rows);
            CHECK(rules.validateMove(board, item.source, item.destination).isValid);
        }
    }
}

TEST_SUITE("Extra coverage rules") {

    TEST_CASE("same cell is rejected directly by every PieceRules function") {
        Board board(8, 8);
        Position sameCell(4, 4);

        CHECK_FALSE(
            PieceRules::canMoveLikeRook(
                board,
                sameCell,
                sameCell
            )
        );

        CHECK_FALSE(
            PieceRules::canMoveLikeBishop(
                board,
                sameCell,
                sameCell
            )
        );

        CHECK_FALSE(
            PieceRules::canMoveLikeQueen(
                board,
                sameCell,
                sameCell
            )
        );

        CHECK_FALSE(
            PieceRules::canMoveLikeKnight(
                sameCell,
                sameCell
            )
        );

        CHECK_FALSE(
            PieceRules::canMoveLikeKing(
                sameCell,
                sameCell
            )
        );

        CHECK_FALSE(
            PieceRules::canMoveLikePawn(
                board,
                sameCell,
                sameCell,
                PieceColor::White
            )
        );
    }

    TEST_CASE("pawn diagonal move to empty cell is rejected directly") {
        Board board(8, 8);

        Position source(6, 3);
        Position emptyDiagonalDestination(5, 2);

        CHECK(board.isEmpty(emptyDiagonalDestination));

        CHECK_FALSE(
            PieceRules::canMoveLikePawn(
                board,
                source,
                emptyDiagonalDestination,
                PieceColor::White
            )
        );
    }

    TEST_CASE("RuleEngine rejects an unknown PieceKind") {
        Board board(1, 2);

        auto unknownPiece = makePiece(
            999,
            PieceColor::White,
            static_cast<PieceKind>(999)
        );

        REQUIRE(
            board.placePiece(
                Position(0, 0),
                unknownPiece
            )
        );

        RuleEngine rules;

        MoveValidation result = rules.validateMove(
            board,
            Position(0, 0),
            Position(0, 1)
        );

        CHECK_FALSE(result.isValid);
        CHECK(result.reason == Reasons::ILLEGAL_PIECE_MOVE);
    }
}