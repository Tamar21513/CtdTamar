#include "../../include/ThirdParty/doctest.h"
#include "../../include/Core/Config.hpp"
#include "../../include/Realtime/RealTimeArbiter.hpp"
#include "TestHelpers.hpp"

TEST_SUITE("RealTimeArbiter") {
    TEST_CASE("new arbiter has no active motion") {
        RealTimeArbiter arbiter;
        CHECK_FALSE(arbiter.hasActiveMotion());
        TimeEvents events = arbiter.advanceTime(1000);
        CHECK(events.steps.empty());
        CHECK(events.jumpLandings.empty());
    }

    TEST_CASE("start motion marks piece moving") {
        RealTimeArbiter arbiter;
        auto piece = makePiece(1, PieceColor::White, PieceKind::Rook);
        arbiter.startMotion(piece, Position(0,0), Position(0,2));
        CHECK(arbiter.hasActiveMotion());
        CHECK(piece->getState() == PieceState::Moving);
    }

    TEST_CASE("motion emits no event before boundary") {
        RealTimeArbiter arbiter;
        auto piece = makePiece(1, PieceColor::White, PieceKind::Rook);
        arbiter.startMotion(piece, Position(0,0), Position(0,2));
        CHECK(arbiter.advanceTime(999).steps.empty());
        TimeEvents events = arbiter.advanceTime(1);
        REQUIRE(events.steps.size() == 1);
        CHECK(events.steps[0].from == Position(0,0));
        CHECK(events.steps[0].to == Position(0,1));
        CHECK_FALSE(events.steps[0].reachedDestination);
        CHECK(events.steps[0].eventTimeMs == 1000);
    }

    TEST_CASE("large advance emits all steps in chronological order") {
        RealTimeArbiter arbiter;
        auto piece = makePiece(1, PieceColor::White, PieceKind::Bishop);
        arbiter.startMotion(piece, Position(0,0), Position(3,3));
        TimeEvents events = arbiter.advanceTime(3000);
        REQUIRE(events.steps.size() == 3);
        CHECK(events.steps[0].to == Position(1,1));
        CHECK(events.steps[1].to == Position(2,2));
        CHECK(events.steps[2].to == Position(3,3));
        CHECK(events.steps[2].reachedDestination);
        CHECK_FALSE(arbiter.hasActiveMotion());
    }

    TEST_CASE("all sign combinations generate correct next cell") {
        struct Case { Position source; Position destination; Position first; };
        const Case cases[] = {
            {Position(1,1), Position(2,1), Position(2,1)},
            {Position(1,1), Position(0,1), Position(0,1)},
            {Position(1,1), Position(1,2), Position(1,2)},
            {Position(1,1), Position(1,0), Position(1,0)},
            {Position(1,1), Position(2,2), Position(2,2)},
            {Position(1,1), Position(0,0), Position(0,0)}
        };
        for (const Case& item : cases) {
            RealTimeArbiter arbiter;
            auto piece = makePiece(1, PieceColor::White, PieceKind::Queen);
            arbiter.startMotion(piece, item.source, item.destination);
            TimeEvents events = arbiter.advanceTime(Config::MOVE_TIME_PER_CELL_MS);
            REQUIRE(events.steps.size() == 1);
            CHECK(events.steps[0].to == item.first);
        }
    }

    TEST_CASE("events with same time preserve start order") {
        RealTimeArbiter arbiter;
        auto first = makePiece(1, PieceColor::White, PieceKind::Rook);
        auto second = makePiece(2, PieceColor::Black, PieceKind::Rook);
        arbiter.startMotion(first, Position(0,0), Position(0,1));
        arbiter.startMotion(second, Position(1,0), Position(1,1));
        TimeEvents events = arbiter.advanceTime(1000);
        REQUIRE(events.steps.size() == 2);
        CHECK(events.steps[0].piece == first);
        CHECK(events.steps[1].piece == second);
        CHECK(events.steps[0].order == 0);
        CHECK(events.steps[1].order == 0);
    }

    TEST_CASE("non moving or null motion is discarded") {
        RealTimeArbiter arbiter;
        auto piece = makePiece(1, PieceColor::White, PieceKind::Rook);
        arbiter.startMotion(nullptr, Position(0,0), Position(0,1));
        arbiter.startMotion(piece, Position(1,0), Position(1,1));
        piece->setState(PieceState::Captured);
        TimeEvents events = arbiter.advanceTime(1000);
        CHECK(events.steps.empty());
        CHECK_FALSE(arbiter.hasActiveMotion());
    }

    TEST_CASE("jump lands exactly at duration") {
        RealTimeArbiter arbiter;
        auto piece = makePiece(1, PieceColor::White, PieceKind::Knight);
        arbiter.startJump(piece, Position(2,2));
        CHECK(piece->getState() == PieceState::Airborne);
        CHECK(arbiter.advanceTime(999).jumpLandings.empty());
        TimeEvents events = arbiter.advanceTime(1);
        REQUIRE(events.jumpLandings.size() == 1);
        CHECK(events.jumpLandings[0].piece == piece);
        CHECK(events.jumpLandings[0].cell == Position(2,2));
    }

    TEST_CASE("multiple jumps land when due") {
        RealTimeArbiter arbiter;
        auto first = makePiece(1, PieceColor::White, PieceKind::Knight);
        auto second = makePiece(2, PieceColor::Black, PieceKind::Knight);
        arbiter.startJump(first, Position(0,0));
        arbiter.advanceTime(500);
        arbiter.startJump(second, Position(1,1));
        TimeEvents firstLanding = arbiter.advanceTime(500);
        REQUIRE(firstLanding.jumpLandings.size() == 1);
        CHECK(firstLanding.jumpLandings[0].piece == first);
        TimeEvents secondLanding = arbiter.advanceTime(500);
        REQUIRE(secondLanding.jumpLandings.size() == 1);
        CHECK(secondLanding.jumpLandings[0].piece == second);
    }
}
