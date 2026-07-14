#include "../../include/ThirdParty/doctest.h"
#include "../../include/Core/Board.hpp"
#include "../../include/IO/BoardMapper.hpp"
#include "../../include/IO/BoardParser.hpp"
#include "../../include/IO/BoardPrinter.hpp"
#include "../../include/Core/Config.hpp"
#include "TestHelpers.hpp"

TEST_SUITE("Board") {
    TEST_CASE("default board has zero dimensions") {
        Board board;
        CHECK(board.getHeight() == 0);
        CHECK(board.getWidth() == 0);
        CHECK_FALSE(board.isInside(Position(0, 0)));
        CHECK(board.getPieceAt(Position(0, 0)) == nullptr);
    }

    TEST_CASE("rectangular dimensions and boundaries") {
        Board board(2, 4);
        CHECK(board.getHeight() == 2);
        CHECK(board.getWidth() == 4);
        CHECK(board.isInside(Position(0, 0)));
        CHECK(board.isInside(Position(1, 3)));
        CHECK_FALSE(board.isInside(Position(-1, 0)));
        CHECK_FALSE(board.isInside(Position(0, -1)));
        CHECK_FALSE(board.isInside(Position(2, 0)));
        CHECK_FALSE(board.isInside(Position(0, 4)));
    }

    TEST_CASE("place rejects outside and occupied cells") {
        Board board(2, 2);
        auto king = makePiece(1, PieceColor::White, PieceKind::King);
        auto rook = makePiece(2, PieceColor::White, PieceKind::Rook);
        CHECK_FALSE(board.placePiece(Position(-1, 0), king));
        CHECK_FALSE(board.placePiece(Position(2, 0), king));
        CHECK(board.placePiece(Position(0, 0), king));
        CHECK_FALSE(board.placePiece(Position(0, 0), rook));
        CHECK(board.getPieceAt(Position(0, 0)) == king);
    }

    TEST_CASE("remove outside is harmless and remove inside empties cell") {
        Board board(1, 1);
        auto piece = makePiece(1, PieceColor::Black, PieceKind::Queen);
        board.placePiece(Position(0, 0), piece);
        board.removePiece(Position(-1, 0));
        CHECK(board.getPieceAt(Position(0, 0)) == piece);
        board.removePiece(Position(0, 0));
        CHECK(board.isEmpty(Position(0, 0)));
    }

    TEST_CASE("set can replace and clear a cell") {
        Board board(1, 1);
        auto first = makePiece(1, PieceColor::White, PieceKind::King);
        auto second = makePiece(2, PieceColor::Black, PieceKind::Queen);
        board.setPieceAt(Position(0, 0), first);
        CHECK(board.getPieceAt(Position(0, 0)) == first);
        board.setPieceAt(Position(0, 0), second);
        CHECK(board.getPieceAt(Position(0, 0)) == second);
        board.setPieceAt(Position(0, 0), nullptr);
        CHECK(board.isEmpty(Position(0, 0)));
        board.setPieceAt(Position(3, 3), first);
        CHECK(board.isEmpty(Position(0, 0)));
    }

    TEST_CASE("move transfers pointer and can capture destination") {
        Board board(1, 2);
        auto rook = makePiece(1, PieceColor::White, PieceKind::Rook);
        auto enemy = makePiece(2, PieceColor::Black, PieceKind::Knight);
        board.placePiece(Position(0, 0), rook);
        board.placePiece(Position(0, 1), enemy);
        board.movePiece(Position(0, 0), Position(0, 1));
        CHECK(board.isEmpty(Position(0, 0)));
        CHECK(board.getPieceAt(Position(0, 1)) == rook);
    }

    TEST_CASE("move with outside endpoint changes nothing") {
        Board board(1, 1);
        auto piece = makePiece(1, PieceColor::White, PieceKind::King);
        board.placePiece(Position(0, 0), piece);
        board.movePiece(Position(0, 0), Position(0, 1));
        CHECK(board.getPieceAt(Position(0, 0)) == piece);
        board.movePiece(Position(-1, 0), Position(0, 0));
        CHECK(board.getPieceAt(Position(0, 0)) == piece);
    }
}

TEST_SUITE("BoardParser and printer") {
    TEST_CASE("empty and blank input returns empty board") {
        CHECK(BoardParser::parse({}).getHeight() == 0);
        CHECK(BoardParser::parse({"", "   ", "\t"}).getWidth() == 0);
    }

    TEST_CASE("single cell boards parse") {
        Board empty = BoardParser::parse({"."});
        CHECK(empty.getHeight() == 1);
        CHECK(empty.getWidth() == 1);
        CHECK(boardText(empty) == ".\n");

        Board occupied = BoardParser::parse({"bK"});
        CHECK(boardText(occupied) == "bK\n");
    }

    TEST_CASE("rectangular board ignores extra whitespace") {
        Board board = BoardParser::parse({"  wK   . bQ  ", " .\t wP   ."});
        CHECK(board.getHeight() == 2);
        CHECK(board.getWidth() == 3);
        CHECK(boardText(board) == "wK . bQ\n. wP .\n");
    }

    TEST_CASE("all piece tokens parse with increasing ids") {
        Board board = BoardParser::parse({"wK wQ wR wB wN wP", "bK bQ bR bB bN bP"});
        int expectedId = 1;
        for (int row = 0; row < 2; ++row) {
            for (int col = 0; col < 6; ++col) {
                auto piece = board.getPieceAt(Position(row, col));
                REQUIRE(piece != nullptr);
                CHECK(piece->getId() == expectedId++);
            }
        }
    }

    TEST_CASE("unknown token lengths and characters are rejected") {
        const std::vector<std::string> badTokens = {"x", "w", "wKK", "xK", "wX", "WK", "bk"};
        for (const std::string& token : badTokens) {
            Board parsed;
            std::string output = captureParserOutput({token}, parsed);
            CHECK(output == Config::ERROR_UNKNOWN_TOKEN + "\n");
            CHECK(parsed.getHeight() == 0);
            CHECK(parsed.getWidth() == 0);
        }
    }

    TEST_CASE("row width mismatch is rejected before token validation in later row") {
        Board parsed;
        std::string output = captureParserOutput({"wK .", "bad"}, parsed);
        CHECK(output == Config::ERROR_ROW_WIDTH_MISMATCH + "\n");
        CHECK(parsed.getHeight() == 0);
    }

    TEST_CASE("same width invalid token reports unknown token") {
        Board parsed;
        std::string output = captureParserOutput({"wK .", "xK ."}, parsed);
        CHECK(output == Config::ERROR_UNKNOWN_TOKEN + "\n");
        CHECK(parsed.getWidth() == 0);
    }

    TEST_CASE("printer handles empty board") {
        CHECK(BoardPrinter::print(Board()) == "");
        CHECK(BoardPrinter::print(Board(0, 5)) == "");
    }
}

TEST_SUITE("BoardMapper") {
    TEST_CASE("maps all four corners of cells") {
        Board board(2, 3);
        auto p00 = BoardMapper::pixelToCell(0, 0, board);
        auto p00Last = BoardMapper::pixelToCell(99, 99, board);
        auto p12 = BoardMapper::pixelToCell(299, 199, board);
        REQUIRE(p00.has_value());
        REQUIRE(p00Last.has_value());
        REQUIRE(p12.has_value());
        CHECK(p00.value() == Position(0, 0));
        CHECK(p00Last.value() == Position(0, 0));
        CHECK(p12.value() == Position(1, 2));
    }

    TEST_CASE("exact cell boundary belongs to next cell") {
        Board board(3, 3);
        CHECK(BoardMapper::pixelToCell(100, 0, board).value() == Position(0, 1));
        CHECK(BoardMapper::pixelToCell(0, 100, board).value() == Position(1, 0));
        CHECK(BoardMapper::pixelToCell(100, 100, board).value() == Position(1, 1));
    }

    TEST_CASE("negative and exact outer borders are outside") {
        Board board(2, 3);
        CHECK_FALSE(BoardMapper::pixelToCell(-1, 0, board).has_value());
        CHECK_FALSE(BoardMapper::pixelToCell(0, -1, board).has_value());
        CHECK_FALSE(BoardMapper::pixelToCell(300, 0, board).has_value());
        CHECK_FALSE(BoardMapper::pixelToCell(0, 200, board).has_value());
    }

    TEST_CASE("zero sized board maps nothing") {
        CHECK_FALSE(BoardMapper::pixelToCell(0, 0, Board()).has_value());
    }
}
