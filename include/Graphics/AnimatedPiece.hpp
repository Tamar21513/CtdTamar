#pragma once

#include <string>

#include "AnimationLibrary.hpp"
#include "AnimationPlayer.hpp"
#include "Renderer.hpp"
#include "VisualState.hpp"

class AnimatedPiece {
private:
    std::string pieceCode;

    VisualState state;

    int row;
    int col;

    double pixelX;
    double pixelY;

    double cooldownRatio;

    AnimationPlayer animationPlayer;

    const AnimationLibrary* animationLibrary;

    void loadAnimation();

public:
    AnimatedPiece(
        const std::string& pieceCode,
        VisualState state,
        int row,
        int col,
        double pixelX,
        double pixelY,
        double cooldownRatio,
        const AnimationLibrary& animationLibrary
    );

    void update(long long deltaMs);

    void setState(VisualState newState);

    void setPixelPosition(
        double newPixelX,
        double newPixelY
    );

    void setCooldownRatio(
        double newCooldownRatio
    );

    void setBoardCell(
        int newRow,
        int newCol
    );

    void setPieceCode(
        const std::string& newPieceCode
    );

    int getRow() const;
    int getCol() const;

    std::string getPieceCode() const;

    VisualState getState() const;

    VisualPiece toVisualPiece() const;
};