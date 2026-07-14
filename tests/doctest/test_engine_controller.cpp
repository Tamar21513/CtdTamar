#include "../../include/ThirdParty/doctest.h"
#include "../../include/Control/Controller.hpp"
#include "../../include/Engine/GameEngine.hpp"
#include "../../include/Core/Results.hpp"
#include "TestHelpers.hpp"

TEST_SUITE("GameEngine basic movement") {
    TEST_CASE("illegal requests return exact reasons") {
        GameEngine engine(parseBoard({"wK .", ". bK"}));
        CHECK(engine.requestMove(Position(-1,0), Position(0,0)).reason == Reasons::OUTSIDE_BOARD);
        CHECK(engine.requestMove(Position(0,1), Position(1,1)).reason == Reasons::EMPTY_SOURCE);
        CHECK(engine.requestMove(Position(0,0), Position(0,0)).reason == Reasons::FRIENDLY_DESTINATION);
        CHECK(engine.requestMove(Position(0,0), Position(1,0)).reason == Reasons::OK);
    }

    TEST_CASE("board is not updated before first virtual step") {
        GameEngine engine(parseBoard({"wR . ."}));
        CHECK(engine.requestMove(Position(0,0), Position(0,2)).isAccepted);
        engine.wait(999);
        CHECK(boardText(engine.getBoard()) == "wR . .\n");
        auto piece = engine.getBoard().getPieceAt(Position(0,0));
        REQUIRE(piece != nullptr);
        CHECK(piece->getState() == PieceState::Moving);
    }

    TEST_CASE("board remains logical origin after intermediate virtual step") {
        GameEngine engine(parseBoard({"wR . ."}));
        engine.requestMove(Position(0,0), Position(0,2));
        engine.wait(1000);
        CHECK(boardText(engine.getBoard()) == "wR . .\n");
        engine.wait(999);
        CHECK(boardText(engine.getBoard()) == "wR . .\n");
    }

    TEST_CASE("arrival happens exactly on required boundary") {
        GameEngine engine(parseBoard({"wR . ."}));
        auto piece = engine.getBoard().getPieceAt(Position(0,0));
        engine.requestMove(Position(0,0), Position(0,2));
        engine.wait(1999);
        CHECK(engine.getBoard().getPieceAt(Position(0,0)) == piece);
        engine.wait(1);
        CHECK(engine.getBoard().getPieceAt(Position(0,2)) == piece);
        CHECK(piece->getState() == PieceState::Idle);
    }

    TEST_CASE("large wait processes all due steps") {
        GameEngine engine(parseBoard({"wR . . . ."}));
        engine.requestMove(Position(0,0), Position(0,4));
        engine.wait(100000);
        requireToken(engine.getBoard(), 0, 4, "wR");
        requireEmpty(engine.getBoard(), 0, 0);
    }

    TEST_CASE("zero and negative wait do not advance a move") {
        GameEngine engine(parseBoard({"wR ."}));
        engine.requestMove(Position(0,0), Position(0,1));
        engine.wait(0);
        engine.wait(-500);
        requireToken(engine.getBoard(), 0, 0, "wR");
        engine.wait(1500);
        requireToken(engine.getBoard(), 0, 1, "wR");
    }

    TEST_CASE("moving piece cannot receive another move") {
        GameEngine engine(parseBoard({"wR . ."}));
        auto first = engine.requestMove(Position(0,0), Position(0,2));
        auto second = engine.requestMove(Position(0,0), Position(0,1));
        CHECK(first.isAccepted);
        CHECK_FALSE(second.isAccepted);
        CHECK(second.reason == Reasons::MOTION_IN_PROGRESS);
    }

    TEST_CASE("piece can move again immediately after arrival") {
        GameEngine engine(parseBoard({"wR . ."}));
        engine.requestMove(Position(0,0), Position(0,2));
        engine.wait(2000);
        CHECK(engine.requestMove(Position(0,2), Position(0,0)).isAccepted);
        engine.wait(2000);
        requireToken(engine.getBoard(), 0, 0, "wR");
    }
}

TEST_SUITE("Captures game over and promotion") {
    TEST_CASE("capture occurs only when attacker reaches target") {
        GameEngine engine(parseBoard({"wR . bN"}));
        auto enemy = engine.getBoard().getPieceAt(Position(0,2));
        engine.requestMove(Position(0,0), Position(0,2));
        engine.wait(1999);
        CHECK(engine.getBoard().getPieceAt(Position(0,2)) == enemy);
        CHECK(enemy->getState() == PieceState::Idle);
        engine.wait(1);
        requireToken(engine.getBoard(), 0, 2, "wR");
        CHECK(enemy->getState() == PieceState::Captured);
    }

    TEST_CASE("capturing king ends game and blocks moves and jumps") {
        GameEngine engine(parseBoard({"wR bK", "wN ."}));
        engine.requestMove(Position(0,0), Position(0,1));
        engine.wait(1000);
        CHECK(engine.isGameOver());
        CHECK(engine.requestMove(Position(1,0), Position(0,2)).reason == Reasons::GAME_OVER);
        CHECK(engine.requestJump(Position(1,0)).reason == Reasons::GAME_OVER);
    }

    TEST_CASE("game is not over before king capture arrival") {
        GameEngine engine(parseBoard({"wR . bK"}));
        engine.requestMove(Position(0,0), Position(0,2));
        engine.wait(1999);
        CHECK_FALSE(engine.isGameOver());
        engine.wait(1);
        CHECK(engine.isGameOver());
    }

    TEST_CASE("white pawn promotes after straight arrival") {
        GameEngine engine(parseBoard({". .", "wP .", ". ."}));
        auto pawn = engine.getBoard().getPieceAt(Position(1,0));
        engine.requestMove(Position(1,0), Position(0,0));
        engine.wait(1000);
        CHECK(pawn->getKind() == PieceKind::Queen);
        CHECK(pawn->token() == "wQ");
    }

    TEST_CASE("black pawn promotes after capture") {
        GameEngine engine(parseBoard({". .", ". bP", "wR ."}));
        auto pawn = engine.getBoard().getPieceAt(Position(1,1));
        engine.requestMove(Position(1,1), Position(2,0));
        engine.wait(1000);
        CHECK(pawn->getKind() == PieceKind::Queen);
        requireToken(engine.getBoard(), 2, 0, "bQ");
    }

    TEST_CASE("pawn not on last row remains pawn") {
        GameEngine engine(parseBoard({". .", ". .", "wP .", ". ."}));
        auto pawn = engine.getBoard().getPieceAt(Position(2,0));
        engine.requestMove(Position(2,0), Position(1,0));
        engine.wait(1000);
        CHECK(pawn->getKind() == PieceKind::Pawn);
    }
}

TEST_SUITE("Concurrent movement and collisions") {
    TEST_CASE("independent pieces can move concurrently") {
        GameEngine engine(parseBoard({"wR . .", "bR . ."}));
        CHECK(engine.requestMove(Position(0,0), Position(0,2)).isAccepted);
        CHECK(engine.requestMove(Position(1,0), Position(1,2)).isAccepted);
        engine.wait(2000);
        requireToken(engine.getBoard(), 0, 2, "wR");
        requireToken(engine.getBoard(), 1, 2, "bR");
    }

    TEST_CASE("same color pieces stop in adjacent cells instead of overlapping") {
        GameEngine engine(parseBoard({"wR . . wR"}));
        auto left = engine.getBoard().getPieceAt(Position(0,0));
        auto right = engine.getBoard().getPieceAt(Position(0,3));
        CHECK(engine.requestMove(Position(0,0), Position(0,2)).isAccepted);
        CHECK(engine.requestMove(Position(0,3), Position(0,1)).isAccepted);
        engine.wait(1000);
        CHECK(left->getState() == PieceState::Moving);
        CHECK(right->getState() == PieceState::Moving);
        CHECK(boardText(engine.getBoard()) == "wR . . wR\n");
        engine.wait(1000);
        CHECK(left->getState() == PieceState::Idle);
        CHECK(right->getState() == PieceState::Idle);
        CHECK(engine.getBoard().getPieceAt(Position(0,1)) == left);
        CHECK(engine.getBoard().getPieceAt(Position(0,2)) == right);
    }

    TEST_CASE("enemy pieces entering same virtual cell are resolved deterministically") {
        GameEngine engine(parseBoard({"wR . bR"}));
        auto white = engine.getBoard().getPieceAt(Position(0,0));
        auto black = engine.getBoard().getPieceAt(Position(0,2));
        CHECK(engine.requestMove(Position(0,0), Position(0,1)).isAccepted);
        CHECK(engine.requestMove(Position(0,2), Position(0,1)).isAccepted);
        engine.wait(1000);
        CHECK(white->getState() == PieceState::Captured);
        CHECK(black->getState() == PieceState::Idle);
        CHECK(engine.getBoard().getPieceAt(Position(0,1)) == black);
    }

    TEST_CASE("path validation still sees a moving piece at its logical source") {
        GameEngine engine(parseBoard({"wR bR . .", ". . . ."}));
        CHECK(engine.requestMove(Position(0,1), Position(1,1)).isAccepted);
        MoveResult blocked = engine.requestMove(Position(0,0), Position(0,3));
        CHECK_FALSE(blocked.isAccepted);
        CHECK(blocked.reason == Reasons::ILLEGAL_PIECE_MOVE);
        engine.wait(1000);
        requireToken(engine.getBoard(), 1, 1, "bR");
        requireToken(engine.getBoard(), 0, 0, "wR");
    }
}

TEST_SUITE("Jump behavior") {
    TEST_CASE("jump validates outside empty and state") {
        GameEngine engine(parseBoard({"wK ."}));
        CHECK(engine.requestJump(Position(-1,0)).reason == Reasons::OUTSIDE_BOARD);
        CHECK(engine.requestJump(Position(0,1)).reason == Reasons::EMPTY_SOURCE);
        CHECK(engine.requestJump(Position(0,0)).reason == Reasons::JUMP_STARTED);
        CHECK(engine.requestJump(Position(0,0)).reason == Reasons::CANNOT_JUMP);
    }

    TEST_CASE("jump keeps piece on board and lands exactly at duration") {
        GameEngine engine(parseBoard({"wK"}));
        auto piece = engine.getBoard().getPieceAt(Position(0,0));
        engine.requestJump(Position(0,0));
        CHECK(piece->getState() == PieceState::Airborne);
        CHECK(engine.getBoard().getPieceAt(Position(0,0)) == piece);
        engine.wait(999);
        CHECK(piece->getState() == PieceState::Airborne);
        engine.wait(1);
        CHECK(piece->getState() == PieceState::Idle);
    }

    TEST_CASE("airborne piece cannot move") {
        GameEngine engine(parseBoard({"wR ."}));
        engine.requestJump(Position(0,0));
        MoveResult result = engine.requestMove(Position(0,0), Position(0,1));
        CHECK_FALSE(result.isAccepted);
        CHECK(result.reason == Reasons::MOTION_IN_PROGRESS);
    }

    TEST_CASE("moving piece cannot jump") {
        GameEngine engine(parseBoard({"wR ."}));
        engine.requestMove(Position(0,0), Position(0,1));
        MoveResult result = engine.requestJump(Position(0,0));
        CHECK_FALSE(result.isAccepted);
        CHECK(result.reason == Reasons::CANNOT_JUMP);
    }

    TEST_CASE("enemy arriving while target is airborne loses the attacker") {
        GameEngine engine(parseBoard({"wR bR"}));
        auto attacker = engine.getBoard().getPieceAt(Position(0,0));
        auto airborne = engine.getBoard().getPieceAt(Position(0,1));
        engine.requestJump(Position(0,1));
        engine.requestMove(Position(0,0), Position(0,1));
        engine.wait(1000);
        CHECK(attacker->getState() == PieceState::Captured);
        CHECK(engine.getBoard().getPieceAt(Position(0,1)) == airborne);
        CHECK(airborne->getState() == PieceState::Idle);
    }

    TEST_CASE("if moving king reaches airborne enemy the moving king is captured and game ends") {
        GameEngine engine(parseBoard({"wK bR"}));
        auto king = engine.getBoard().getPieceAt(Position(0,0));
        engine.requestJump(Position(0,1));
        engine.requestMove(Position(0,0), Position(0,1));
        engine.wait(1000);
        CHECK(king->getState() == PieceState::Captured);
        CHECK(engine.isGameOver());
    }
}

TEST_SUITE("Controller") {
    TEST_CASE("outside click without selection is unhandled") {
        GameEngine engine(parseBoard({"wK"}));
        Controller controller(engine);
        ControllerResult result = controller.click(-1, 0);
        CHECK_FALSE(result.handled);
        CHECK(result.reason == Reasons::CLICK_OUTSIDE);
        CHECK_FALSE(controller.hasSelection());
    }

    TEST_CASE("outside click clears existing selection") {
        GameEngine engine(parseBoard({"wK"}));
        Controller controller(engine);
        CHECK(controller.click(50,50).handled);
        CHECK(controller.hasSelection());
        ControllerResult result = controller.click(100,0);
        CHECK(result.handled);
        CHECK(result.reason == Reasons::CLICK_OUTSIDE);
        CHECK_FALSE(controller.hasSelection());
    }

    TEST_CASE("empty first click does not select") {
        GameEngine engine(parseBoard({"wK ."}));
        Controller controller(engine);
        ControllerResult result = controller.click(150,50);
        CHECK_FALSE(result.handled);
        CHECK(result.reason == Reasons::EMPTY_CLICK);
        CHECK_FALSE(controller.hasSelection());
    }

    TEST_CASE("friendly second click replaces selection") {
        GameEngine engine(parseBoard({"wK wR"}));
        Controller controller(engine);
        controller.click(50,50);
        ControllerResult result = controller.click(150,50);
        CHECK(result.handled);
        CHECK(result.reason == Reasons::SELECTED);
        REQUIRE(controller.getSelectedCell().has_value());
        CHECK(controller.getSelectedCell().value() == Position(0,1));
    }

    TEST_CASE("legal second click starts move and clears selection") {
        GameEngine engine(parseBoard({"wR ."}));
        Controller controller(engine);
        controller.click(50,50);
        ControllerResult result = controller.click(150,50);
        CHECK(result.handled);
        CHECK(result.reason == Reasons::OK);
        CHECK_FALSE(controller.hasSelection());
        engine.wait(1000);
        requireToken(engine.getBoard(), 0, 1, "wR");
    }

    TEST_CASE("illegal second click clears selection") {
        GameEngine engine(parseBoard({"wR .", ". ."}));
        Controller controller(engine);
        controller.click(50,50);
        ControllerResult result = controller.click(150,150);
        CHECK_FALSE(result.handled);
        CHECK(result.reason == Reasons::ILLEGAL_PIECE_MOVE);
        CHECK_FALSE(controller.hasSelection());
    }

    TEST_CASE("jump always clears selection") {
        GameEngine engine(parseBoard({"wK ."}));
        Controller controller(engine);
        controller.click(50,50);
        CHECK(controller.hasSelection());
        ControllerResult result = controller.jump(50,50);
        CHECK(result.handled);
        CHECK(result.reason == Reasons::JUMP_STARTED);
        CHECK_FALSE(controller.hasSelection());
    }

    TEST_CASE("jump outside reports click outside") {
        GameEngine engine(parseBoard({"wK"}));
        Controller controller(engine);
        ControllerResult result = controller.jump(100,0);
        CHECK_FALSE(result.handled);
        CHECK(result.reason == Reasons::CLICK_OUTSIDE);
    }
}
TEST_SUITE("Extra coverage time ordering") {

    TEST_CASE("jump lands before the next moving-piece step") {
        GameEngine engine(
            parseBoard({
                "wR . . .",
                "bN . . ."
            })
        );

        auto rook =
            engine.getBoard().getPieceAt(Position(0, 0));

        auto knight =
            engine.getBoard().getPieceAt(Position(1, 0));

        REQUIRE(rook != nullptr);
        REQUIRE(knight != nullptr);

        CHECK(
            engine.requestMove(
                Position(0, 0),
                Position(0, 3)
            ).isAccepted
        );

        engine.wait(500);

        CHECK(
            engine.requestJump(
                Position(1, 0)
            ).isAccepted
        );

        /*
            זמן נוכחי בסיום ההמתנה: 1500 ms

            תנועת הצריח הראשונה:
            1000 ms

            נחיתת הפרש:
            1500 ms

            תנועת הצריח הבאה:
            2000 ms

            לכן סדר האירועים צריך להיות:
            1. צעד צריח
            2. נחיתת פרש
            3. עדיין לא הצעד הבא של הצריח
        */
        engine.wait(1000);

        CHECK(rook->getState() == PieceState::Moving);
        CHECK(knight->getState() == PieceState::Idle);

        CHECK(
            engine.getBoard().getPieceAt(Position(0, 0))
            == rook
        );

        CHECK(
            engine.getBoard().getPieceAt(Position(1, 0))
            == knight
        );
    }

    TEST_CASE("old logical cell of a moving piece is treated as empty") {
        GameEngine engine(
            parseBoard({
                "bR . .",
                "wR . ."
            })
        );

        auto firstRook =
            engine.getBoard().getPieceAt(Position(0, 0));

        auto secondRook =
            engine.getBoard().getPieceAt(Position(1, 0));

        REQUIRE(firstRook != nullptr);
        REQUIRE(secondRook != nullptr);

        /*
            הצריח השחור מתחיל לנוע מ-(0,0) אל (0,2).

            לאחר הצעד הראשון המיקום הווירטואלי שלו הוא (0,1),
            אבל בלוח הפיזי הוא עדיין שמור ב-(0,0).
        */
        CHECK(
            engine.requestMove(
                Position(0, 0),
                Position(0, 2)
            ).isAccepted
        );

        /*
            הצריח הלבן נע אל התא הישן של הצריח השחור.
        */
        CHECK(
            engine.requestMove(
                Position(1, 0),
                Position(0, 0)
            ).isAccepted
        );

        engine.wait(1000);

        /*
            הצריח השחור כבר נמצא וירטואלית ב-(0,1),
            ולכן התא הישן (0,0) צריך להיחשב פנוי.
        */
        CHECK(firstRook->getState() == PieceState::Moving);
        CHECK(secondRook->getState() == PieceState::Idle);

        CHECK(
            engine.getBoard().getPieceAt(Position(0, 0))
            == secondRook
        );

        CHECK(
            firstRook->getState() != PieceState::Captured
        );
    }
}