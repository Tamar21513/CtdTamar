#pragma once

#include <string>

#include "VisualState.hpp"
#include "AnimationPlayer.hpp"
#include "AnimationLibrary.hpp"
#include "Renderer.hpp"

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
    void setPixelPosition(double newPixelX, double newPixelY);
    void setCooldownRatio(double newCooldownRatio);
    void setBoardCell(int newRow, int newCol);

    int getRow() const;
    int getCol() const;
    
    VisualPiece toVisualPiece() const;

    std::string getPieceCode() const;
    VisualState getState() const;
};