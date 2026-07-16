#include "../../include/App/VisualApp.hpp"

#include "../../include/Graphics/Renderer.hpp"
#include "../../include/Graphics/VisualState.hpp"
#include "../../include/Graphics/VisualMotion.hpp"
#include "../../include/Graphics/AnimationLibrary.hpp"
#include "../../include/Graphics/AnimatedPiece.hpp"

#include <opencv2/opencv.hpp>

#include <iostream>
#include <vector>
#include <chrono>
#include <string>

static const std::string WINDOW_NAME = "Kung Fu Chess";

struct MouseState {
    int x = 0;
    int y = 0;
    bool leftClicked = false;
};

static MouseState mouseState;

static void onMouse(int event, int x, int y, int flags, void* userdata) {
    mouseState.x = x;
    mouseState.y = y;

    if (event == cv::EVENT_LBUTTONDOWN) {
        std::cout << "Mouse click at x=" << x << " y=" << y << std::endl;
        mouseState.leftClicked = true;
    }
}

static bool canSelectForMove(VisualState state) {
    return state == VisualState::Idle;
}

static double lerp(double start, double end, double progress) {
    return start + (end - start) * progress;
}

static int cellToPieceX(int col, int boardStartX, int cellSizeX, int spriteSize) {
    return boardStartX + col * cellSizeX + cellSizeX / 2;
}

static int cellToPieceY(int row, int boardStartY, int cellSizeY, int spriteSize) {
    return boardStartY + row * cellSizeY + cellSizeY / 2;
}

static int mouseToCol(int mouseX, int boardStartX, int cellSizeX) {
    return (mouseX - boardStartX) / cellSizeX;
}

static int mouseToRow(int mouseY, int boardStartY, int cellSizeY) {
    return (mouseY - boardStartY) / cellSizeY;
}

static bool isInsideBoardCell(int row, int col) {
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}

static double cooldownRatioByTime(long long elapsedMs, long long cycleMs) {
    if (cycleMs <= 0) {
        return 0.0;
    }

    long long value = elapsedMs % cycleMs;

    double ratio = 1.0 - static_cast<double>(value) / static_cast<double>(cycleMs);

    if (ratio < 0.0) {
        return 0.0;
    }

    if (ratio > 1.0) {
        return 1.0;
    }

    return ratio;
}

static AnimatedPiece createAnimatedPieceAtCell(
    const std::string& code,
    VisualState state,
    int row,
    int col,
    double cooldownRatio,
    int boardStartX,
    int boardStartY,
    int cellSizeX,
    int cellSizeY,
    int spriteSize,
    const AnimationLibrary& library
) {
    double x = cellToPieceX(col, boardStartX, cellSizeX, spriteSize);
    double y = cellToPieceY(row, boardStartY, cellSizeY, spriteSize);

    return AnimatedPiece(
        code,
        state,
        row,
        col,
        x,
        y,
        cooldownRatio,
        library
    );
}

static int getSpriteSizeForPieceCode(const std::string& pieceCode, int defaultSpriteSize) {
    bool isKing = pieceCode == "wK" || pieceCode == "bK";
    bool isQueen = pieceCode == "wQ" || pieceCode == "bQ";

    if (isKing || isQueen) {
        return static_cast<int>(defaultSpriteSize * 1.35);
    }

    return defaultSpriteSize;
}

static int findPieceIndexAtMouse(
    const std::vector<AnimatedPiece>& pieces,
    int mouseX,
    int mouseY,
    int defaultSpriteSize
) {
    for (int i = static_cast<int>(pieces.size()) - 1; i >= 0; i--) {
        VisualPiece visualPiece = pieces[i].toVisualPiece();

        int spriteSize = getSpriteSizeForPieceCode(
            visualPiece.pieceCode,
            defaultSpriteSize
        );

        int pieceLeft = static_cast<int>(visualPiece.pixelX - spriteSize / 2.0);
        int pieceTop = static_cast<int>(visualPiece.pixelY - spriteSize / 2.0);

        int pieceRight = pieceLeft + spriteSize;
        int pieceBottom = pieceTop + spriteSize;

        bool insideX = mouseX >= pieceLeft && mouseX <= pieceRight;
        bool insideY = mouseY >= pieceTop && mouseY <= pieceBottom;

        if (insideX && insideY) {
            return i;
        }
    }

    return -1;
}

static VisualPiece createGhostPieceUnderMouse(
    const AnimatedPiece& selectedPiece,
    int mouseX,
    int mouseY,
    double dragOffsetX,
    double dragOffsetY
) {
    VisualPiece ghost = selectedPiece.toVisualPiece();

    ghost.pixelX = mouseX + dragOffsetX;
    ghost.pixelY = mouseY + dragOffsetY;

    ghost.opacity = 0.45;

    return ghost;
}

void VisualApp::run() {
    std::cout << "VisualApp mouse move test started" << std::endl;

    cv::namedWindow(WINDOW_NAME, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(WINDOW_NAME, onMouse);

    Renderer renderer(R"(assets\Board\board.png)", 800, WINDOW_NAME);
    AnimationLibrary animationLibrary;

    int boardSize = 800;

    int boardStartX = static_cast<int>(boardSize * 0.11);
    int boardStartY = static_cast<int>(boardSize * 0.11);

    int cellSizeX = static_cast<int>(boardSize * 0.098);
    int cellSizeY = static_cast<int>(boardSize * 0.097);

    int spriteSize = static_cast<int>(boardSize * 0.105);

    std::vector<AnimatedPiece> animatedPieces;

    animatedPieces.push_back(
        createAnimatedPieceAtCell(
            "wN",
            VisualState::Idle,
            0,
            1,
            0.0,
            boardStartX,
            boardStartY,
            cellSizeX,
            cellSizeY,
            spriteSize,
            animationLibrary
        )
    );

    animatedPieces.push_back(
        createAnimatedPieceAtCell(
            "wQ",
            VisualState::LongRest,
            0,
            3,
            1.0,
            boardStartX,
            boardStartY,
            cellSizeX,
            cellSizeY,
            spriteSize,
            animationLibrary
        )
    );

    animatedPieces.push_back(
        createAnimatedPieceAtCell(
            "wR",
            VisualState::ShortRest,
            0,
            5,
            1.0,
            boardStartX,
            boardStartY,
            cellSizeX,
            cellSizeY,
            spriteSize,
            animationLibrary
        )
    );

    animatedPieces.push_back(
        createAnimatedPieceAtCell(
            "bK",
            VisualState::Idle,
            7,
            4,
            0.0,
            boardStartX,
            boardStartY,
            cellSizeX,
            cellSizeY,
            spriteSize,
            animationLibrary
        )
    );

    animatedPieces.push_back(
        createAnimatedPieceAtCell(
            "bP",
            VisualState::Jump,
            6,
            2,
            0.0,
            boardStartX,
            boardStartY,
            cellSizeX,
            cellSizeY,
            spriteSize,
            animationLibrary
        )
    );

    int selectedPieceIndex = -1;
    double dragOffsetX = 0.0;
    double dragOffsetY = 0.0;

    int movingPieceIndex = -1;
    VisualMotion activeMotion;
    bool hasActiveMotion = false;

    long long queenRestDurationMs = 6000;
    long long rookRestDurationMs = 3000;

    bool queenRestFinished = false;
    bool rookRestFinished = false;

    auto startTime = std::chrono::steady_clock::now();
    auto previousTime = std::chrono::steady_clock::now();

    while (true) {
        auto currentTime = std::chrono::steady_clock::now();

        long long deltaMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - previousTime
        ).count();

        long long totalElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - startTime
        ).count();

        previousTime = currentTime;

        if (!queenRestFinished) {
            if (totalElapsedMs >= queenRestDurationMs) {
                queenRestFinished = true;

                animatedPieces[1].setCooldownRatio(0.0);
                animatedPieces[1].setState(VisualState::Idle);

                std::cout << "wQ rest finished. Back to idle." << std::endl;
            } else {
                double ratio = 1.0 - static_cast<double>(totalElapsedMs) /
                                      static_cast<double>(queenRestDurationMs);

                animatedPieces[1].setCooldownRatio(ratio);
            }
        }

        if (!rookRestFinished) {
            if (totalElapsedMs >= rookRestDurationMs) {
                rookRestFinished = true;

                animatedPieces[2].setCooldownRatio(0.0);
                animatedPieces[2].setState(VisualState::Idle);

                std::cout << "wR rest finished. Back to idle." << std::endl;
            } else {
                double ratio = 1.0 - static_cast<double>(totalElapsedMs) /
                                      static_cast<double>(rookRestDurationMs);

                animatedPieces[2].setCooldownRatio(ratio);
            }
        }

        if (hasActiveMotion && movingPieceIndex != -1) {
            activeMotion.elapsedMs += deltaMs;

            if (activeMotion.elapsedMs >= activeMotion.durationMs) {
                activeMotion.elapsedMs = activeMotion.durationMs;
                hasActiveMotion = false;

                double finalX = cellToPieceX(
                    activeMotion.toCol,
                    boardStartX,
                    cellSizeX,
                    spriteSize
                );

                double finalY = cellToPieceY(
                    activeMotion.toRow,
                    boardStartY,
                    cellSizeY,
                    spriteSize
                );

                animatedPieces[movingPieceIndex].setPixelPosition(finalX, finalY);

                animatedPieces[movingPieceIndex].setBoardCell(
                    activeMotion.toRow,
                    activeMotion.toCol
                );

                animatedPieces[movingPieceIndex].setState(VisualState::Idle);

                std::cout << "Visual move finished at row "
                          << activeMotion.toRow
                          << " col "
                          << activeMotion.toCol
                          << std::endl;

                movingPieceIndex = -1;
            } else {
                double progress = getMotionProgress(activeMotion);

                int startX = cellToPieceX(
                    activeMotion.fromCol,
                    boardStartX,
                    cellSizeX,
                    spriteSize
                );

                int startY = cellToPieceY(
                    activeMotion.fromRow,
                    boardStartY,
                    cellSizeY,
                    spriteSize
                );

                int endX = cellToPieceX(
                    activeMotion.toCol,
                    boardStartX,
                    cellSizeX,
                    spriteSize
                );

                int endY = cellToPieceY(
                    activeMotion.toRow,
                    boardStartY,
                    cellSizeY,
                    spriteSize
                );

                double currentX = lerp(startX, endX, progress);
                double currentY = lerp(startY, endY, progress);

                animatedPieces[movingPieceIndex].setPixelPosition(currentX, currentY);
            }
        }

        for (AnimatedPiece& piece : animatedPieces) {
            piece.update(deltaMs);
        }

        if (mouseState.leftClicked) {
            mouseState.leftClicked = false;

            int clickedRow = mouseToRow(mouseState.y, boardStartY, cellSizeY);
            int clickedCol = mouseToCol(mouseState.x, boardStartX, cellSizeX);

            std::cout << "Clicked cell row "
                      << clickedRow
                      << " col "
                      << clickedCol
                      << std::endl;

            if (!isInsideBoardCell(clickedRow, clickedCol)) {
                selectedPieceIndex = -1;
                std::cout << "Clicked outside board. Selection cleared." << std::endl;
            } else {
                int clickedPieceIndex = findPieceIndexAtMouse(
                    animatedPieces,
                    mouseState.x,
                    mouseState.y,
                    spriteSize
                );

                if (clickedPieceIndex != -1) {
                    VisualPiece clickedPiece = animatedPieces[clickedPieceIndex].toVisualPiece();

                    if (!canSelectForMove(clickedPiece.state)) {
                        std::cout << "Cannot select piece for move because it is not idle: "
                                  << clickedPiece.pieceCode
                                  << std::endl;

                        selectedPieceIndex = -1;
                    } else if (hasActiveMotion && clickedPieceIndex == movingPieceIndex) {
                        std::cout << "Cannot select piece while it is moving" << std::endl;
                    } else {
                        selectedPieceIndex = clickedPieceIndex;

                        VisualPiece selected = animatedPieces[selectedPieceIndex].toVisualPiece();

                        dragOffsetX = selected.pixelX - mouseState.x;
                        dragOffsetY = selected.pixelY - mouseState.y;

                        std::cout << "Selected piece: "
                                  << selected.pieceCode
                                  << " at row "
                                  << selected.row
                                  << " col "
                                  << selected.col
                                  << std::endl;

                        std::cout << "Drag offset x="
                                  << dragOffsetX
                                  << " y="
                                  << dragOffsetY
                                  << std::endl;
                    }
                } else {
                    if (selectedPieceIndex != -1) {
                        if (hasActiveMotion) {
                            std::cout << "Another piece is already moving" << std::endl;
                            selectedPieceIndex = -1;
                        } else {
                            VisualPiece selected = animatedPieces[selectedPieceIndex].toVisualPiece();

                            if (!canSelectForMove(selected.state)) {
                                std::cout << "Move blocked. Piece is not idle: "
                                          << selected.pieceCode
                                          << std::endl;

                                selectedPieceIndex = -1;
                            } else {
                                std::cout << "Move request: "
                                          << selected.pieceCode
                                          << " from row "
                                          << selected.row
                                          << " col "
                                          << selected.col
                                          << " to row "
                                          << clickedRow
                                          << " col "
                                          << clickedCol
                                          << std::endl;

                                movingPieceIndex = selectedPieceIndex;

                                activeMotion = createVisualMotion(
                                    selected.row,
                                    selected.col,
                                    clickedRow,
                                    clickedCol
                                );

                                hasActiveMotion = true;

                                animatedPieces[movingPieceIndex].setState(VisualState::Move);

                                std::cout << "Started visual move. Duration: "
                                          << activeMotion.durationMs
                                          << " ms"
                                          << std::endl;

                                selectedPieceIndex = -1;
                            }
                        }
                    }
                }
            }
        }

        std::vector<VisualPiece> visualPieces;

        for (const AnimatedPiece& piece : animatedPieces) {
            visualPieces.push_back(piece.toVisualPiece());
        }

        if (selectedPieceIndex != -1) {
            VisualPiece ghostPiece = createGhostPieceUnderMouse(
                animatedPieces[selectedPieceIndex],
                mouseState.x,
                mouseState.y,
                dragOffsetX,
                dragOffsetY
            );

            visualPieces.push_back(ghostPiece);
        }

        renderer.render(visualPieces);

        int key = cv::waitKey(16);

        if (key == 27) {
            break;
        }
    }

    cv::destroyAllWindows();
}