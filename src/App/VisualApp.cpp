#include "../../include/App/VisualApp.hpp"

#include "../../include/Control/Controller.hpp"

#include "../../include/Core/Board.hpp"
#include "../../include/Core/Piece.hpp"
#include "../../include/Core/Position.hpp"
#include "../../include/Core/Results.hpp"

#include "../../include/Engine/GameEngine.hpp"

#include "../../include/Graphics/AnimationLibrary.hpp"
#include "../../include/Graphics/Renderer.hpp"
#include "../../include/Graphics/VisualSnapshotBuilder.hpp"

#include <opencv2/opencv.hpp>

#include <chrono>
#include <deque>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

static const std::string WINDOW_NAME =
    "Kung Fu Chess";

struct MouseClick {
    int x;
    int y;
};

struct MouseState {
    int x = 0;
    int y = 0;

    std::deque<MouseClick> pendingClicks;

    /*
     * המרחק בין מרכז הכלי לבין המקום
     * המדויק שבו העכבר לחץ עליו.
     */
    double grabOffsetX = 0.0;
    double grabOffsetY = 0.0;

    bool hasGrabOffset = false;
};

static MouseState mouseState;

static void onMouse(
    int event,
    int x,
    int y,
    int flags,
    void* userdata
) {
    mouseState.x = x;
    mouseState.y = y;

    if (event == cv::EVENT_LBUTTONDOWN) {
        mouseState.pendingClicks.push_back({
            x,
            y
        });
    }
}

static int findVisualPieceByCell(
    const std::vector<VisualPiece>& pieces,
    const Position& cell
) {
    for (size_t i = 0; i < pieces.size(); i++) {
        const VisualPiece& piece =
            pieces[i];

        if (
            piece.row == cell.getRow() &&
            piece.col == cell.getCol()
        ) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

static VisualPiece createSelectionGhost(
    const VisualPiece& selectedPiece,
    int mouseX,
    int mouseY
) {
    VisualPiece ghost =
        selectedPiece;

    ghost.pixelX =
        static_cast<double>(mouseX);

    ghost.pixelY =
        static_cast<double>(mouseY);

    ghost.opacity = 0.55;

    /*
     * מפעיל חיתוך של השוליים השקופים.
     */
    ghost.alignVisibleCenter = true;

    return ghost;
}

static void printControllerResult(
    const ControllerResult& result
) {
    std::cout
        << "Controller: "
        << result.reason
        << std::endl;
}

static void placeBackRank(
    Board& board,
    int row,
    PieceColor color,
    int& nextId
) {
    const PieceKind backRank[8] = {
        PieceKind::Rook,
        PieceKind::Knight,
        PieceKind::Bishop,
        PieceKind::Queen,
        PieceKind::King,
        PieceKind::Bishop,
        PieceKind::Knight,
        PieceKind::Rook
    };

    for (int col = 0; col < 8; col++) {
        board.placePiece(
            Position(row, col),
            std::make_shared<Piece>(
                nextId++,
                color,
                backRank[col]
            )
        );
    }
}

static void placePawnRank(
    Board& board,
    int row,
    PieceColor color,
    int& nextId
) {
    for (int col = 0; col < 8; col++) {
        board.placePiece(
            Position(row, col),
            std::make_shared<Piece>(
                nextId++,
                color,
                PieceKind::Pawn
            )
        );
    }
}

static Board createInitialVisualBoard() {
    Board board(8, 8);

    int nextId = 1;

    /*
     * הכלים השחורים למעלה.
     */
    placeBackRank(
        board,
        0,
        PieceColor::Black,
        nextId
    );

    placePawnRank(
        board,
        1,
        PieceColor::Black,
        nextId
    );

    /*
     * הכלים הלבנים למטה.
     */
    placePawnRank(
        board,
        6,
        PieceColor::White,
        nextId
    );

    placeBackRank(
        board,
        7,
        PieceColor::White,
        nextId
    );

    return board;
}

void VisualApp::run() {
    std::cout
        << "VisualApp started"
        << std::endl;

    const int boardSize = 800;

    const int boardStartX =
        static_cast<int>(
            boardSize * 0.11
        );

    const int boardStartY =
        static_cast<int>(
            boardSize * 0.11
        );

    const int cellSizeX =
        static_cast<int>(
            boardSize * 0.098
        );

    const int cellSizeY =
        static_cast<int>(
            boardSize * 0.097
        );

    const int spriteSize =
        static_cast<int>(
            boardSize * 0.105
        );

    cv::namedWindow(
        WINDOW_NAME,
        cv::WINDOW_AUTOSIZE
    );

    cv::setMouseCallback(
        WINDOW_NAME,
        onMouse
    );

    Renderer renderer(
        R"(assets\Board\board.png)",
        boardSize,
        WINDOW_NAME
    );

    AnimationLibrary animationLibrary;

    GameEngine engine(
        createInitialVisualBoard()
    );

    Controller controller(
        engine,
        boardStartX,
        boardStartY,
        cellSizeX,
        cellSizeY
    );

    VisualSnapshotBuilder snapshotBuilder(
        boardStartX,
        boardStartY,
        cellSizeX,
        cellSizeY,
        spriteSize,
        animationLibrary
    );

    std::vector<VisualPiece> visualPieces =
        snapshotBuilder.build(
            engine,
            0
        );

    auto previousTime =
        std::chrono::steady_clock::now();

    while (true) {
        auto currentTime =
            std::chrono::steady_clock::now();

        long long deltaMs =
            std::chrono::duration_cast<
                std::chrono::milliseconds
            >(
                currentTime - previousTime
            ).count();

        previousTime =
            currentTime;

        /*
         * לאחר סיום המשחק לא שולחים
         * לחיצות נוספות ל־Controller.
         */
        if (!engine.isGameOver()) {
            while (
                !mouseState.pendingClicks.empty()
            ) {
                MouseClick click =
                    mouseState.pendingClicks.front();

                mouseState.pendingClicks.pop_front();

                ControllerResult result =
                    controller.click(
                        click.x,
                        click.y
                    );

                printControllerResult(
                    result
                );
            }
        } else {
            /*
             * מנקים לחיצות שנשארו בתור
             * לאחר סיום המשחק.
             */
            mouseState.pendingClicks.clear();
        }

        /*
         * מקדמים את זמן המשחק.
         */
        engine.wait(deltaMs);

        /*
         * בונים את המצב הגרפי מחדש
         * מתוך GameEngine.
         */
        visualPieces =
            snapshotBuilder.build(
                engine,
                deltaMs
            );

        std::vector<VisualPiece> piecesToRender =
            visualPieces;

        /*
         * מציגים כלי רפאים רק כאשר
         * המשחק עדיין פעיל.
         */
        if (!engine.isGameOver()) {
            std::optional<Position> selectedCell =
                controller.getSelectedCell();

            if (selectedCell.has_value()) {
                int selectedVisualIndex =
                    findVisualPieceByCell(
                        visualPieces,
                        selectedCell.value()
                    );

                if (selectedVisualIndex != -1) {
                    /*
                     * מחלישים מעט את הכלי המקורי.
                     */
                    piecesToRender[
                        selectedVisualIndex
                    ].opacity = 0.25;

                    /*
                     * עותק שקוף שמרכז התמונה שלו
                     * נמצא בדיוק מתחת לעכבר.
                     */
                    piecesToRender.push_back(
                        createSelectionGhost(
                            visualPieces[
                                selectedVisualIndex
                            ],
                            mouseState.x,
                            mouseState.y
                        )
                    );
                }
            }
        }

        /*
         * חשוב:
         * מעבירים ל־Renderer את מצב סיום המשחק.
         */
        renderer.render(
            piecesToRender,
            engine.isGameOver()
        );

        int key =
            cv::waitKey(16);

        if (key == 27) {
            break;
        }
    }

    cv::destroyAllWindows();
}