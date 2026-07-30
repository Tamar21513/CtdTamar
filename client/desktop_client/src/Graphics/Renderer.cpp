#include "Graphics/Renderer.hpp"
#include "Graphics/GraphicsHelper.hpp"
#include "Client/PlayerDisplay.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// Creates a renderer with the default window name.
Renderer::Renderer(const std::string& boardImagePath, int boardDisplaySize)
    : Renderer(boardImagePath, boardDisplaySize, "Kung Fu Chess") {}

// Creates a renderer for one board image and window.
Renderer::Renderer(
    const std::string& boardImagePath,
    int boardDisplaySize,
    const std::string& windowName
) : boardImagePath(boardImagePath),
    boardDisplaySize(boardDisplaySize),
    windowName(windowName) {}

// Loads and normalizes the board image to BGR format.
cv::Mat Renderer::loadBoardImage() const {
    cv::Mat board = GraphicsHelper::readImage(boardImagePath);
    if (board.empty()) {
        return board;
    }

    cv::resize(board, board, cv::Size(boardDisplaySize, boardDisplaySize));
    if (board.channels() == 4) {
        cv::cvtColor(board, board, cv::COLOR_BGRA2BGR);
    } else if (board.channels() == 1) {
        cv::cvtColor(board, board, cv::COLOR_GRAY2BGR);
    } else if (board.channels() != 3) {
        return cv::Mat();
    }
    return board;
}

// Renders a complete game frame.
void Renderer::render(
    const std::vector<VisualPiece>& pieces,
    bool gameOver,
    int blackScore,
    int whiteScore,
    const std::vector<MoveHistoryEntry>& blackMoves,
    const std::vector<MoveHistoryEntry>& whiteMoves,
    const std::string& localPlayerColor,
    const std::string& whiteUsername,
    const std::string& blackUsername,
    const std::string& statusLine1,
    const std::string& statusLine2,
    RenderOverlayMode overlayMode,
    bool showSpectatorBackButton
) {
    const cv::Mat board = loadBoardImage();
    if (board.empty()) {
        std::cout << "Could not load a supported board image: " << boardImagePath << std::endl;
        return;
    }

    const int canvasWidth = boardDisplaySize + 2 * SIDE_PANEL_WIDTH;
    const int canvasHeight = boardDisplaySize + 2 * SCORE_PANEL_HEIGHT;
    cv::Mat canvas(canvasHeight, canvasWidth, CV_8UC3, cv::Scalar(225, 225, 225));
    board.copyTo(canvas(cv::Rect(SIDE_PANEL_WIDTH, SCORE_PANEL_HEIGHT, boardDisplaySize, boardDisplaySize)));

    const bool localIsBlack =
        localPlayerColor == "black";
    const bool localIsWhite =
        localPlayerColor == "white";
    const PlayerScoreDisplay scoreDisplay =
        buildPlayerScoreDisplay(
            whiteUsername,
            blackUsername,
            whiteScore,
            blackScore
        );

    drawScore(
        canvas,
        scoreDisplay.topName,
        scoreDisplay.topScore,
        0,
        localIsBlack
    );
    drawScore(
        canvas,
        scoreDisplay.bottomName,
        scoreDisplay.bottomScore,
        SCORE_PANEL_HEIGHT +
            boardDisplaySize,
        localIsWhite
    );
    drawMoveHistory(canvas, blackMoves, "BLACK MOVES", 0, SCORE_PANEL_HEIGHT, SIDE_PANEL_WIDTH, boardDisplaySize);
    drawMoveHistory(canvas, whiteMoves, "WHITE MOVES", SIDE_PANEL_WIDTH + boardDisplaySize,
                    SCORE_PANEL_HEIGHT, SIDE_PANEL_WIDTH, boardDisplaySize);
    drawPieces(canvas, pieces);
    if (gameOver) {
        drawGameOver(canvas);
    }
    drawOverlay(
        canvas,
        statusLine1,
        statusLine2,
        overlayMode
    );
    if (showSpectatorBackButton) {
        const cv::Rect button = spectatorBackButtonRect();
        cv::rectangle(
            canvas, button, cv::Scalar(45, 45, 45), cv::FILLED);
        cv::rectangle(
            canvas, button, cv::Scalar(215, 145, 55), 2);
        cv::putText(
            canvas,
            "BACK TO LOBBY",
            cv::Point(button.x + 18, button.y + 28),
            cv::FONT_HERSHEY_SIMPLEX,
            0.58,
            cv::Scalar(255, 255, 255),
            2,
            cv::LINE_AA);
    }
    cv::imshow(windowName, canvas);
}

cv::Rect Renderer::spectatorBackButtonRect() {
    return {24, 14, 190, 42};
}

// Draws all visible pieces.
void Renderer::drawPieces(cv::Mat& canvas, const std::vector<VisualPiece>& pieces) {
    const int defaultSpriteSize = static_cast<int>(boardDisplaySize * 0.105);
    for (const VisualPiece& piece : pieces) {
        drawSinglePiece(canvas, piece, 0, 0, 0, 0, defaultSpriteSize);
    }
}

// Draws one score panel.
void Renderer::drawScore(
    cv::Mat& canvas,
    const std::string& label,
    int score,
    int y,
    bool localPlayer
) {
    const cv::Rect area(SIDE_PANEL_WIDTH, y, boardDisplaySize, SCORE_PANEL_HEIGHT);
    cv::rectangle(canvas, area, cv::Scalar(245, 245, 245), cv::FILLED);
    cv::rectangle(
        canvas,
        area,
        localPlayer
            ? cv::Scalar(0, 165, 255)
            : cv::Scalar(120, 120, 120),
        localPlayer ? 5 : 1
    );

    const std::string text = label + ": " + std::to_string(score);
    int baseline = 0;
    const cv::Size size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.8, 2, &baseline);
    const cv::Point origin(
        area.x + (area.width - size.width) / 2,
        area.y + (area.height + size.height) / 2
    );
    cv::putText(canvas, text, origin, cv::FONT_HERSHEY_SIMPLEX, 0.8,
                cv::Scalar(35, 35, 35), 2, cv::LINE_AA);
}

// Draws a phase-specific overlay inside the playable board.
void Renderer::drawOverlay(
    cv::Mat& canvas,
    const std::string& statusLine1,
    const std::string& statusLine2,
    RenderOverlayMode overlayMode
) {
    if (
        statusLine1.empty() ||
        overlayMode == RenderOverlayMode::None
    ) {
        return;
    }

    const cv::Rect boardArea(
        SIDE_PANEL_WIDTH,
        SCORE_PANEL_HEIGHT,
        boardDisplaySize,
        boardDisplaySize
    );

    if (overlayMode == RenderOverlayMode::Countdown) {
        cv::Mat dimmed =
            canvas(boardArea).clone();
        dimmed.setTo(cv::Scalar(20, 20, 20));
        cv::addWeighted(
            dimmed,
            0.34,
            canvas(boardArea),
            0.66,
            0.0,
            canvas(boardArea)
        );
    }

    double scale = 1.0;
    int thickness = 3;
    if (overlayMode == RenderOverlayMode::Countdown) {
        int unitBaseline = 0;
        const cv::Size unitSize =
            cv::getTextSize(
                statusLine1,
                cv::FONT_HERSHEY_DUPLEX,
                1.0,
                thickness,
                &unitBaseline
            );
        const double heightRatio =
            statusLine1 == "GO!" ? 0.25 : 0.31;
        scale =
            boardDisplaySize * heightRatio /
            std::max(1, unitSize.height);
        thickness = std::max(
            5,
            static_cast<int>(scale * 2.0)
        );
    }
    else if (overlayMode == RenderOverlayMode::Waiting) {
        thickness = 3;
        int unitBaseline = 0;
        const cv::Size unitSize =
            cv::getTextSize(
                statusLine1,
                cv::FONT_HERSHEY_DUPLEX,
                1.0,
                thickness,
                &unitBaseline
            );
        scale = std::min(
            1.45,
            boardDisplaySize * 0.78 /
                std::max(1, unitSize.width)
        );
    }
    else {
        scale = 0.75;
        thickness = 2;
    }

    int baseline = 0;
    const cv::Size primarySize =
        cv::getTextSize(
            statusLine1,
            cv::FONT_HERSHEY_DUPLEX,
            scale,
            thickness,
            &baseline
        );

    if (overlayMode == RenderOverlayMode::Waiting) {
        const int paddingX =
            std::max(28, boardDisplaySize / 24);
        const int paddingY =
            std::max(24, boardDisplaySize / 28);
        const int textHeight =
            primarySize.height + baseline;
        const cv::Rect panel(
            boardArea.x +
                (boardArea.width -
                    primarySize.width -
                    2 * paddingX) / 2,
            boardArea.y +
                (boardArea.height -
                    textHeight -
                    2 * paddingY) / 2,
            primarySize.width + 2 * paddingX,
            textHeight + 2 * paddingY
        );
        cv::Mat darkPanel =
            canvas(panel).clone();
        darkPanel.setTo(cv::Scalar(20, 20, 20));
        cv::addWeighted(
            darkPanel,
            0.72,
            canvas(panel),
            0.28,
            0.0,
            canvas(panel)
        );
    }

    const auto drawCenteredText =
        [this, &canvas, &boardArea](
            const std::string& text,
            int centerY,
            double textScale,
            int textThickness,
            const cv::Scalar& color
        ) {
            int baseline = 0;
            const cv::Size size =
                cv::getTextSize(
                    text,
                    cv::FONT_HERSHEY_DUPLEX,
                    textScale,
                    textThickness,
                    &baseline
                );

            const cv::Point origin(
                boardArea.x +
                    (boardArea.width - size.width) / 2,
                centerY +
                    (size.height - baseline) / 2
            );
            cv::putText(
                canvas, text, origin,
                cv::FONT_HERSHEY_DUPLEX, textScale,
                cv::Scalar(15, 15, 15),
                textThickness + 6, cv::LINE_AA
            );
            cv::putText(
                canvas, text, origin,
                cv::FONT_HERSHEY_DUPLEX, textScale,
                color,
                textThickness, cv::LINE_AA
            );
        };

    drawCenteredText(
        statusLine1,
        boardArea.y + boardArea.height / 2,
        scale,
        thickness,
        overlayMode == RenderOverlayMode::Countdown
            ? cv::Scalar(170, 225, 255)
            : cv::Scalar(255, 255, 255)
    );

    if (!statusLine2.empty()) {
        drawCenteredText(
            statusLine2,
            boardArea.y +
                boardArea.height / 2 + 58,
            0.72,
            2,
            cv::Scalar(255, 255, 255)
        );
    }
}

// Formats milliseconds for the move history panel.
std::string Renderer::formatTime(long long timeMs) const {
    const long long minutes = timeMs / 60000;
    const long long seconds = (timeMs / 1000) % 60;
    const long long millis = timeMs % 1000;
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02lld:%02lld.%03lld", minutes, seconds, millis);
    return buffer;
}

// Draws one player's move history.
void Renderer::drawMoveHistory(
    cv::Mat& canvas,
    const std::vector<MoveHistoryEntry>& moves,
    const std::string& title,
    int x,
    int y,
    int width,
    int height
) {
    const cv::Rect area(x, y, width, height);
    cv::rectangle(canvas, area, cv::Scalar(248, 248, 248), cv::FILLED);
    cv::rectangle(canvas, area, cv::Scalar(120, 120, 120), 1);
    cv::putText(canvas, title, cv::Point(x + 18, y + 30), cv::FONT_HERSHEY_SIMPLEX,
                0.6, cv::Scalar(30, 30, 30), 2, cv::LINE_AA);
    cv::putText(canvas, "Time", cv::Point(x + 8, y + 58), cv::FONT_HERSHEY_SIMPLEX,
                0.45, cv::Scalar(40, 40, 40), 1, cv::LINE_AA);
    cv::putText(canvas, "Move", cv::Point(x + width - 78, y + 58), cv::FONT_HERSHEY_SIMPLEX,
                0.45, cv::Scalar(40, 40, 40), 1, cv::LINE_AA);

    const int rowHeight = 27;
    const int maxRows = std::max(0, (height - 90) / rowHeight);
    const int start = std::max(0, static_cast<int>(moves.size()) - maxRows);
    for (int index = start; index < static_cast<int>(moves.size()); ++index) {
        const int textY = y + 84 + (index - start) * rowHeight;
        cv::line(canvas, cv::Point(x, textY + 7), cv::Point(x + width, textY + 7),
                 cv::Scalar(220, 220, 220), 1);
        cv::putText(canvas, formatTime(moves[index].completedAtMs), cv::Point(x + 7, textY),
                    cv::FONT_HERSHEY_SIMPLEX, 0.42, cv::Scalar(55, 55, 55), 1, cv::LINE_AA);
        cv::putText(canvas, moves[index].notation, cv::Point(x + width - 82, textY),
                    cv::FONT_HERSHEY_SIMPLEX, 0.48, cv::Scalar(55, 55, 55), 1, cv::LINE_AA);
    }
}

// Crops fully transparent margins from a sprite.
cv::Mat Renderer::cropTransparentMargins(const cv::Mat& sprite) const {
    if (sprite.empty() || sprite.channels() < 4) {
        return sprite.clone();
    }

    int minX = sprite.cols;
    int minY = sprite.rows;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < sprite.rows; ++y) {
        for (int x = 0; x < sprite.cols; ++x) {
            if (sprite.at<cv::Vec4b>(y, x)[3] > 10) {
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }
    }
    if (maxX < minX || maxY < minY) {
        return sprite.clone();
    }
    return sprite(cv::Rect(minX, minY, maxX - minX + 1, maxY - minY + 1)).clone();
}

// Loads the configured sprite or its idle fallback.
cv::Mat Renderer::loadPieceSprite(const VisualPiece& piece) const {
    std::string imagePath = piece.imagePath.empty() ? buildPieceImagePath(piece) : piece.imagePath;
    if (!fileExists(imagePath)) {
        imagePath = buildFallbackIdlePath(piece);
    }
    if (!fileExists(imagePath)) {
        std::cout << "Missing sprite for piece: " << piece.pieceCode << std::endl;
        return cv::Mat();
    }
    return GraphicsHelper::readImage(imagePath);
}

// Resizes a sprite while preserving visible proportions when requested.
cv::Mat Renderer::resizePieceSprite(
    const cv::Mat& source,
    int spriteSize,
    bool cropMargins
) const {
    cv::Mat sprite = cropMargins ? cropTransparentMargins(source) : source.clone();
    if (sprite.empty()) {
        return sprite;
    }

    cv::Size size(spriteSize, spriteSize);
    if (cropMargins) {
        const int longestSide = std::max(sprite.cols, sprite.rows);
        const double scale = longestSide > 0 ? static_cast<double>(spriteSize) / longestSide : 1.0;
        size = cv::Size(
            std::max(1, static_cast<int>(sprite.cols * scale)),
            std::max(1, static_cast<int>(sprite.rows * scale))
        );
    }
    cv::resize(sprite, sprite, size);
    return sprite;
}

// Draws one piece and its optional rest effects.
void Renderer::drawSinglePiece(
    cv::Mat& board,
    const VisualPiece& piece,
    int,
    int,
    int,
    int,
    int defaultSpriteSize
) {
    const int spriteSize = getSpriteSizeForPiece(piece, defaultSpriteSize);
    const cv::Mat source = loadPieceSprite(piece);
    if (source.empty()) {
        return;
    }
    const cv::Mat sprite = resizePieceSprite(source, spriteSize, piece.alignVisibleCenter);
    const int x = static_cast<int>(piece.pixelX - sprite.cols / 2.0);
    const int y = static_cast<int>(piece.pixelY - sprite.rows / 2.0);
    const double opacity = std::max(0.0, std::min(1.0, piece.opacity));
    if (opacity <= 0.0) {
        return;
    }

    if (opacity >= 0.99) {
        GraphicsHelper::drawTransparent(board, sprite, x, y);
    } else {
        GraphicsHelper::drawTransparentWithOpacity(board, sprite, x, y, opacity);
    }
    if (isRestState(piece.state) && opacity >= 0.99) {
        drawRestEffects(board, piece, x, y, spriteSize);
    }
}

// Draws sleep and cooldown indicators around a resting piece.
void Renderer::drawRestEffects(
    cv::Mat& board,
    const VisualPiece& piece,
    int pieceX,
    int pieceY,
    int spriteSize
) {
    drawSleepZ(
        board,
        pieceX + static_cast<int>(spriteSize * 0.70),
        pieceY + static_cast<int>(spriteSize * 0.22),
        piece.state
    );
    drawCooldownHourglass(
        board,
        pieceX + static_cast<int>(spriteSize * 0.68),
        pieceY + static_cast<int>(spriteSize * 0.62),
        piece.cooldownRatio
    );
}

// Draws the fixed outline of an hourglass.
void Renderer::drawHourglassFrame(
    cv::Mat& board,
    int x,
    int y,
    int width,
    int height
) const {
    const cv::Scalar color(60, 40, 20);
    cv::line(board, cv::Point(x, y), cv::Point(x + width, y), color, 2, cv::LINE_AA);
    cv::line(board, cv::Point(x, y + height), cv::Point(x + width, y + height), color, 2, cv::LINE_AA);
    cv::line(board, cv::Point(x + 2, y + 2), cv::Point(x + width - 2, y + height - 2), color, 2, cv::LINE_AA);
    cv::line(board, cv::Point(x + width - 2, y + 2), cv::Point(x + 2, y + height - 2), color, 2, cv::LINE_AA);
}

// Draws the changing sand inside an hourglass.
void Renderer::drawHourglassSand(
    cv::Mat& board,
    int x,
    int y,
    int width,
    int height,
    double cooldownRatio
) const {
    const cv::Scalar sandColor(40, 180, 240);
    const int upperHeight = static_cast<int>(9.0 * cooldownRatio);
    const int lowerHeight = static_cast<int>(9.0 * (1.0 - cooldownRatio));

    if (upperHeight > 0) {
        const std::vector<cv::Point> upper{
            cv::Point(x + 4, y + 4),
            cv::Point(x + width - 4, y + 4),
            cv::Point(x + width / 2, y + 4 + upperHeight)
        };
        cv::fillConvexPoly(board, upper, sandColor, cv::LINE_AA);
    }
    if (lowerHeight > 0) {
        const std::vector<cv::Point> lower{
            cv::Point(x + width / 2, y + height - 4 - lowerHeight),
            cv::Point(x + 4, y + height - 4),
            cv::Point(x + width - 4, y + height - 4)
        };
        cv::fillConvexPoly(board, lower, sandColor, cv::LINE_AA);
    }
    if (cooldownRatio > 0.03 && cooldownRatio < 0.97) {
        cv::line(board, cv::Point(x + width / 2, y + 10),
                 cv::Point(x + width / 2, y + height - 10), sandColor, 1, cv::LINE_AA);
    }
}

// Draws one cooldown hourglass.
void Renderer::drawCooldownHourglass(cv::Mat& board, int x, int y, double cooldownRatio) {
    const double ratio = std::max(0.0, std::min(1.0, cooldownRatio));
    const int width = 18;
    const int height = 28;
    drawHourglassFrame(board, x, y, width, height);
    drawHourglassSand(board, x, y, width, height, ratio);
}

// Draws the sleep marker for a resting piece.
void Renderer::drawSleepZ(cv::Mat& board, int x, int y, VisualState state) {
    const bool longRest = state == VisualState::LongRest;
    const double fontScale = longRest ? 0.75 : 0.55;
    const cv::Scalar textColor(255, 255, 255);
    const cv::Scalar shadowColor(50, 50, 50);

    cv::putText(board, "Z", cv::Point(x + 2, y + 2), cv::FONT_HERSHEY_SIMPLEX,
                fontScale, shadowColor, 4, cv::LINE_AA);
    cv::putText(board, "Z", cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX,
                fontScale, textColor, 2, cv::LINE_AA);
    if (longRest) {
        cv::putText(board, "z", cv::Point(x + 15, y - 13), cv::FONT_HERSHEY_SIMPLEX,
                    0.45, shadowColor, 3, cv::LINE_AA);
        cv::putText(board, "z", cv::Point(x + 13, y - 15), cv::FONT_HERSHEY_SIMPLEX,
                    0.45, textColor, 1, cv::LINE_AA);
    }
}

// Draws the centered game-over overlay.
void Renderer::drawGameOver(cv::Mat& board) {
    const std::string text = "GAME OVER";
    const double fontScale = board.cols / 400.0;
    const int thickness = std::max(4, board.cols / 150);
    int baseline = 0;
    const cv::Size textSize = cv::getTextSize(
        text,
        cv::FONT_HERSHEY_DUPLEX,
        fontScale,
        thickness,
        &baseline
    );
    const int textX = (board.cols - textSize.width) / 2;
    const int textY = (board.rows + textSize.height) / 2;
    cv::Rect backgroundRect(
        textX - static_cast<int>(board.cols * 0.045),
        textY - textSize.height - static_cast<int>(board.rows * 0.035),
        textSize.width + static_cast<int>(board.cols * 0.09),
        textSize.height + static_cast<int>(board.rows * 0.07) + baseline
    );
    backgroundRect &= cv::Rect(0, 0, board.cols, board.rows);

    cv::Mat overlay = board.clone();
    cv::rectangle(overlay, backgroundRect, cv::Scalar(15, 15, 15), cv::FILLED);
    cv::addWeighted(overlay, 0.78, board, 0.22, 0.0, board);
    cv::rectangle(board, backgroundRect, cv::Scalar(240, 240, 240), 3, cv::LINE_AA);
    cv::putText(board, text, cv::Point(textX + 5, textY + 5), cv::FONT_HERSHEY_DUPLEX,
                fontScale, cv::Scalar(0, 0, 0), thickness + 4, cv::LINE_AA);
    cv::putText(board, text, cv::Point(textX, textY), cv::FONT_HERSHEY_DUPLEX,
                fontScale, cv::Scalar(255, 255, 255), thickness, cv::LINE_AA);
}

// Builds the primary sprite path for a piece state.
std::string Renderer::buildPieceImagePath(const VisualPiece& piece) const {
    return "assets\\pieces\\" + piece.pieceCode + "\\states\\" +
        visualStateToFolderName(piece.state) + "\\sprites\\1.png";
}

// Builds the idle fallback sprite path.
std::string Renderer::buildFallbackIdlePath(const VisualPiece& piece) const {
    return "assets\\pieces\\" + piece.pieceCode + "\\states\\idle\\sprites\\1.png";
}

// Returns the display size for one piece type.
int Renderer::getSpriteSizeForPiece(const VisualPiece& piece, int defaultSize) const {
    const bool isKing = piece.pieceCode == "wK" || piece.pieceCode == "bK";
    const bool isQueen = piece.pieceCode == "wQ" || piece.pieceCode == "bQ";
    return isKing || isQueen ? static_cast<int>(defaultSize * 1.35) : defaultSize;
}

// Reports whether a visual state represents cooldown rest.
bool Renderer::isRestState(VisualState state) const {
    return state == VisualState::ShortRest || state == VisualState::LongRest;
}

// Reports whether an asset path exists.
bool Renderer::fileExists(const std::string& path) const {
    return fs::exists(path);
}
