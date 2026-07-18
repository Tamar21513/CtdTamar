#pragma once

#include <vector>

#include "AnimatedPiece.hpp"
#include "AnimationLibrary.hpp"
#include "Renderer.hpp"
#include "VisualStateMachine.hpp"

#include "../Engine/GameEngine.hpp"

class VisualSnapshotBuilder {
private:
    struct CachedPiece {
        int pieceId;

        AnimatedPiece animatedPiece;
    };

    int boardStartX;
    int boardStartY;

    int cellSizeX;
    int cellSizeY;

    int spriteSize;

    const AnimationLibrary&
        animationLibrary;

    VisualStateMachine
        visualStateMachine;

    std::vector<CachedPiece>
        cachedPieces;

    double cellCenterX(
        int col
    ) const;

    double cellCenterY(
        int row
    ) const;

    CachedPiece* findCachedPiece(
        int pieceId
    );

    const MovingPieceInfo*
    findMovement(
        const GameEngine& engine,
        int pieceId
    ) const;

    const JumpInfo*
    findJump(
        const GameEngine& engine,
        int pieceId
    ) const;

    void removeMissingPieces(
        const std::vector<int>&
            visiblePieceIds
    );

public:
    VisualSnapshotBuilder(
        int boardStartX,
        int boardStartY,
        int cellSizeX,
        int cellSizeY,
        int spriteSize,
        const AnimationLibrary&
            animationLibrary
    );

    std::vector<VisualPiece> build(
        const GameEngine& engine,
        long long deltaMs
    );
};