#include "../../include/Graphics/AnimatedPiece.hpp"

#include <iostream>
#include <vector>

// Implements AnimatedPiece.
AnimatedPiece::AnimatedPiece(
    const std::string& pieceCode,
    VisualState state,
    int row,
    int col,
    double pixelX,
    double pixelY,
    double cooldownRatio,
    const AnimationLibrary& animationLibrary
) {
    this->pieceCode = pieceCode;
    this->state = state;

    this->row = row;
    this->col = col;

    this->pixelX = pixelX;
    this->pixelY = pixelY;

    this->cooldownRatio = cooldownRatio;

    this->animationLibrary = &animationLibrary;

    loadAnimation();
}

// Implements loadAnimation.
void AnimatedPiece::loadAnimation() {
    if (animationLibrary == nullptr) {
        return;
    }

    std::vector<std::string> frames =
        animationLibrary->getFramePaths(
            pieceCode,
            state
        );

    if (
        frames.empty() &&
        state != VisualState::Idle
    ) {
        std::cout
            << "No frames for state. "
            << "Falling back to idle. Piece: "
            << pieceCode
            << std::endl;

        frames =
            animationLibrary->getFramePaths(
                pieceCode,
                VisualState::Idle
            );
    }

    if (frames.empty()) {
        std::cout
            << "No frames found for piece: "
            << pieceCode
            << std::endl;

        return;
    }

    int fps =
        animationLibrary->getFramesPerSecond(pieceCode, state);

    bool loop =
        animationLibrary->isLoop(pieceCode, state);

    animationPlayer.setFrames(
        frames,
        fps,
        loop
    );
}

// Implements update.
void AnimatedPiece::update(long long deltaMs) {
    animationPlayer.update(deltaMs);
}

// Implements setState.
void AnimatedPiece::setState(
    VisualState newState
) {
    if (state == newState) {
        return;
    }

    state = newState;

    loadAnimation();
}

// Implements setPixelPosition.
void AnimatedPiece::setPixelPosition(
    double newPixelX,
    double newPixelY
) {
    pixelX = newPixelX;
    pixelY = newPixelY;
}

// Implements setCooldownRatio.
void AnimatedPiece::setCooldownRatio(
    double newCooldownRatio
) {
    if (newCooldownRatio < 0.0) {
        newCooldownRatio = 0.0;
    }

    if (newCooldownRatio > 1.0) {
        newCooldownRatio = 1.0;
    }

    cooldownRatio = newCooldownRatio;
}

// Implements setBoardCell.
void AnimatedPiece::setBoardCell(
    int newRow,
    int newCol
) {
    row = newRow;
    col = newCol;
}

// Implements getRow.
int AnimatedPiece::getRow() const {
    return row;
}

// Implements getCol.
int AnimatedPiece::getCol() const {
    return col;
}

// Implements getPieceCode.
std::string AnimatedPiece::getPieceCode() const {
    return pieceCode;
}

// Implements getState.
VisualState AnimatedPiece::getState() const {
    return state;
}

// Implements setPieceCode.
void AnimatedPiece::setPieceCode(
    const std::string& newPieceCode
) {
    if (pieceCode == newPieceCode) {
        return;
    }

    pieceCode = newPieceCode;

    // Reload animations after a pawn changes into a queen.
    loadAnimation();
}

// Implements toVisualPiece.
VisualPiece AnimatedPiece::toVisualPiece() const {
    VisualPiece piece;

    piece.pieceCode = pieceCode;
    piece.state = state;

    piece.row = row;
    piece.col = col;

    piece.pixelX = pixelX;
    piece.pixelY = pixelY;

    piece.cooldownRatio = cooldownRatio;

    piece.imagePath =
        animationPlayer.getCurrentFramePath();

    piece.opacity = 1.0;

    return piece;
}
