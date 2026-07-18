#include "../../include/Graphics/Renderer.hpp"
#include "../../include/Graphics/GraphicsHelper.hpp"

#include <filesystem>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

Renderer::Renderer(
    const std::string& boardImagePath,
    int boardDisplaySize
) {
    this->boardImagePath =
        boardImagePath;

    this->boardDisplaySize =
        boardDisplaySize;

    this->windowName =
        "Kung Fu Chess";
}

Renderer::Renderer(
    const std::string& boardImagePath,
    int boardDisplaySize,
    const std::string& windowName
) {
    this->boardImagePath =
        boardImagePath;

    this->boardDisplaySize =
        boardDisplaySize;

    this->windowName =
        windowName;
}

void Renderer::render(
    const std::vector<VisualPiece>& pieces,
    bool gameOver
) {
    cv::Mat board =
        GraphicsHelper::readImage(
            boardImagePath
        );

    if (board.empty()) {
        std::cout
            << "Could not load board image: "
            << boardImagePath
            << std::endl;

        return;
    }

    cv::resize(
        board,
        board,
        cv::Size(
            boardDisplaySize,
            boardDisplaySize
        )
    );

    drawPieces(
        board,
        pieces
    );

    /*
     * GAME OVER מצויר אחרון,
     * כדי שיופיע מעל הלוח ומעל הכלים.
     */
    if (gameOver) {
        drawGameOver(board);
    }

    cv::imshow(
        windowName,
        board
    );
}

void Renderer::drawPieces(
    cv::Mat& board,
    const std::vector<VisualPiece>& pieces
) {
    int boardWidth =
        board.cols;

    int boardHeight =
        board.rows;

    int cellSizeX =
        static_cast<int>(
            boardWidth * 0.098
        );

    int cellSizeY =
        static_cast<int>(
            boardHeight * 0.097
        );

    int defaultSpriteSize =
        static_cast<int>(
            boardWidth * 0.105
        );

    int boardStartX =
        static_cast<int>(
            boardWidth * 0.11
        );

    int boardStartY =
        static_cast<int>(
            boardHeight * 0.11
        );

    for (
        const VisualPiece& piece :
        pieces
    ) {
        drawSinglePiece(
            board,
            piece,
            boardStartX,
            boardStartY,
            cellSizeX,
            cellSizeY,
            defaultSpriteSize
        );
    }
}

cv::Mat Renderer::cropTransparentMargins(
    const cv::Mat& sprite
) const {
    if (sprite.empty()) {
        return sprite;
    }

    if (sprite.channels() < 4) {
        return sprite.clone();
    }

    int minX = sprite.cols;
    int minY = sprite.rows;
    int maxX = -1;
    int maxY = -1;

    for (int y = 0; y < sprite.rows; y++) {
        for (int x = 0; x < sprite.cols; x++) {
            const cv::Vec4b& pixel =
                sprite.at<cv::Vec4b>(y, x);

            if (pixel[3] <= 10) {
                continue;
            }

            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }

    if (
        maxX < minX ||
        maxY < minY
    ) {
        return sprite.clone();
    }

    cv::Rect visibleArea(
        minX,
        minY,
        maxX - minX + 1,
        maxY - minY + 1
    );

    return sprite(visibleArea).clone();
}

void Renderer::drawSinglePiece(
    cv::Mat& board,
    const VisualPiece& piece,
    int boardStartX,
    int boardStartY,
    int cellSizeX,
    int cellSizeY,
    int defaultSpriteSize
) {
    std::string imagePath =
        piece.imagePath;

    if (imagePath.empty()) {
        imagePath =
            buildPieceImagePath(
                piece
            );
    }

    if (!fileExists(imagePath)) {
        imagePath =
            buildFallbackIdlePath(
                piece
            );
    }

    if (!fileExists(imagePath)) {
        std::cout
            << "Missing sprite for piece: "
            << piece.pieceCode
            << std::endl;

        return;
    }

    cv::Mat sprite =
        GraphicsHelper::readImage(
            imagePath
        );

    if (sprite.empty()) {
        std::cout
            << "Could not load sprite: "
            << imagePath
            << std::endl;

        return;
    }

    int spriteSize =
        getSpriteSizeForPiece(
            piece,
            defaultSpriteSize
        );

int x = 0;
int y = 0;

if (piece.alignVisibleCenter) {
    /*
     * כלי הרפאים:
     * חותכים את כל השטח השקוף.
     */
    cv::Mat croppedSprite =
        cropTransparentMargins(sprite);

    if (croppedSprite.empty()) {
        croppedSprite = sprite.clone();
    }

    /*
     * שומרים על יחס הרוחב והגובה
     * של הדמות הגלויה.
     */
    int longestSide =
        std::max(
            croppedSprite.cols,
            croppedSprite.rows
        );

    double scale = 1.0;

    if (longestSide > 0) {
        scale =
            static_cast<double>(spriteSize) /
            static_cast<double>(longestSide);
    }

    int resizedWidth =
        std::max(
            1,
            static_cast<int>(
                croppedSprite.cols * scale
            )
        );

    int resizedHeight =
        std::max(
            1,
            static_cast<int>(
                croppedSprite.rows * scale
            )
        );

        cv::resize(
            croppedSprite,
            sprite,
            cv::Size(
                resizedWidth,
                resizedHeight
            )
        );
    
        /*
         * מרכז הדמות הגלויה נמצא
         * בדיוק על מיקום העכבר.
         */
        x = static_cast<int>(
            piece.pixelX -
            sprite.cols / 2.0
        );
    
        y = static_cast<int>(
            piece.pixelY -
            sprite.rows / 2.0
        );
    } else {
        /*
         * כלי רגיל על הלוח.
         */
        cv::resize(
            sprite,
            sprite,
            cv::Size(
                spriteSize,
                spriteSize
            )
        );
    
        x = static_cast<int>(
            piece.pixelX -
            sprite.cols / 2.0
        );
    
        y = static_cast<int>(
            piece.pixelY -
            sprite.rows / 2.0
        );
    }

    double opacity =
        piece.opacity;

    if (opacity < 0.0) {
        opacity = 0.0;
    }

    if (opacity > 1.0) {
        opacity = 1.0;
    }

    if (opacity <= 0.0) {
        return;
    }

    if (opacity >= 0.99) {
        GraphicsHelper::drawTransparent(
            board,
            sprite,
            x,
            y
        );
    } else {
        GraphicsHelper::
            drawTransparentWithOpacity(
                board,
                sprite,
                x,
                y,
                opacity
            );
    }

    /*
     * אפקטי מנוחה מוצגים רק על הכלי האמיתי,
     * ולא על כלי הרפאים השקוף.
     */
    if (
        isRestState(piece.state) &&
        piece.opacity >= 0.99
    ) {
        drawRestEffects(
            board,
            piece,
            x,
            y,
            spriteSize
        );
    }
}

void Renderer::drawRestEffects(
    cv::Mat& board,
    const VisualPiece& piece,
    int pieceX,
    int pieceY,
    int spriteSize
) {
    int zX =
        pieceX +
        static_cast<int>(
            spriteSize * 0.70
        );

    int zY =
        pieceY +
        static_cast<int>(
            spriteSize * 0.22
        );

    drawSleepZ(
        board,
        zX,
        zY,
        piece.state
    );

    int hourglassX =
        pieceX +
        static_cast<int>(
            spriteSize * 0.68
        );

    int hourglassY =
        pieceY +
        static_cast<int>(
            spriteSize * 0.62
        );

    drawCooldownHourglass(
        board,
        hourglassX,
        hourglassY,
        piece.cooldownRatio
    );
}

void Renderer::drawCooldownHourglass(
    cv::Mat& board,
    int x,
    int y,
    double cooldownRatio
) {
    if (cooldownRatio < 0.0) {
        cooldownRatio = 0.0;
    }

    if (cooldownRatio > 1.0) {
        cooldownRatio = 1.0;
    }

    const int width = 18;
    const int height = 28;

    cv::Scalar outlineColor(
        60,
        40,
        20
    );

    cv::Scalar sandColor(
        40,
        180,
        240
    );

    /*
     * מסגרת עליונה ותחתונה.
     */
    cv::line(
        board,
        cv::Point(
            x,
            y
        ),
        cv::Point(
            x + width,
            y
        ),
        outlineColor,
        2,
        cv::LINE_AA
    );

    cv::line(
        board,
        cv::Point(
            x,
            y + height
        ),
        cv::Point(
            x + width,
            y + height
        ),
        outlineColor,
        2,
        cv::LINE_AA
    );

    /*
     * גוף שעון החול.
     */
    cv::line(
        board,
        cv::Point(
            x + 2,
            y + 2
        ),
        cv::Point(
            x + width - 2,
            y + height - 2
        ),
        outlineColor,
        2,
        cv::LINE_AA
    );

    cv::line(
        board,
        cv::Point(
            x + width - 2,
            y + 2
        ),
        cv::Point(
            x + 2,
            y + height - 2
        ),
        outlineColor,
        2,
        cv::LINE_AA
    );

    /*
     * כמות החול בחלק העליון קטנה
     * ככל שהמנוחה מתקדמת.
     */
    int upperSandHeight =
        static_cast<int>(
            9.0 * cooldownRatio
        );

    if (upperSandHeight > 0) {
        std::vector<cv::Point>
            upperTriangle;

        upperTriangle.push_back(
            cv::Point(
                x + 4,
                y + 4
            )
        );

        upperTriangle.push_back(
            cv::Point(
                x + width - 4,
                y + 4
            )
        );

        upperTriangle.push_back(
            cv::Point(
                x + width / 2,
                y + 4 +
                    upperSandHeight
            )
        );

        cv::fillConvexPoly(
            board,
            upperTriangle,
            sandColor,
            cv::LINE_AA
        );
    }

    /*
     * ככל שה־cooldown קטן,
     * יותר חול מצטבר בחלק התחתון.
     */
    double finishedRatio =
        1.0 - cooldownRatio;

    int lowerSandHeight =
        static_cast<int>(
            9.0 * finishedRatio
        );

    if (lowerSandHeight > 0) {
        std::vector<cv::Point>
            lowerTriangle;

        lowerTriangle.push_back(
            cv::Point(
                x + width / 2,
                y + height - 4 -
                    lowerSandHeight
            )
        );

        lowerTriangle.push_back(
            cv::Point(
                x + 4,
                y + height - 4
            )
        );

        lowerTriangle.push_back(
            cv::Point(
                x + width - 4,
                y + height - 4
            )
        );

        cv::fillConvexPoly(
            board,
            lowerTriangle,
            sandColor,
            cv::LINE_AA
        );
    }

    /*
     * זרם חול במרכז.
     */
    if (
        cooldownRatio > 0.03 &&
        cooldownRatio < 0.97
    ) {
        cv::line(
            board,
            cv::Point(
                x + width / 2,
                y + 10
            ),
            cv::Point(
                x + width / 2,
                y + height - 10
            ),
            sandColor,
            1,
            cv::LINE_AA
        );
    }
}

void Renderer::drawSleepZ(
    cv::Mat& board,
    int x,
    int y,
    VisualState state
) {
    double fontScale = 0.55;
    int thickness = 2;

    if (
        state ==
        VisualState::LongRest
    ) {
        fontScale = 0.75;
    }

    cv::Scalar textColor(
        255,
        255,
        255
    );

    cv::Scalar shadowColor(
        50,
        50,
        50
    );

    cv::putText(
        board,
        "Z",
        cv::Point(
            x + 2,
            y + 2
        ),
        cv::FONT_HERSHEY_SIMPLEX,
        fontScale,
        shadowColor,
        thickness + 2,
        cv::LINE_AA
    );

    cv::putText(
        board,
        "Z",
        cv::Point(
            x,
            y
        ),
        cv::FONT_HERSHEY_SIMPLEX,
        fontScale,
        textColor,
        thickness,
        cv::LINE_AA
    );

    if (
        state ==
        VisualState::LongRest
    ) {
        cv::putText(
            board,
            "z",
            cv::Point(
                x + 15,
                y - 13
            ),
            cv::FONT_HERSHEY_SIMPLEX,
            0.45,
            shadowColor,
            3,
            cv::LINE_AA
        );

        cv::putText(
            board,
            "z",
            cv::Point(
                x + 13,
                y - 15
            ),
            cv::FONT_HERSHEY_SIMPLEX,
            0.45,
            textColor,
            1,
            cv::LINE_AA
        );
    }
}

void Renderer::drawGameOver(
    cv::Mat& board
) {
    const std::string text =
        "GAME OVER";

    /*
     * גודל יחסי לגודל הלוח.
     */
    double fontScale =
        board.cols / 400.0;

    int thickness =
        std::max(
            4,
            board.cols / 150
        );

    int baseline = 0;

    cv::Size textSize =
        cv::getTextSize(
            text,
            cv::FONT_HERSHEY_DUPLEX,
            fontScale,
            thickness,
            &baseline
        );

    int textX =
        (board.cols -
         textSize.width) / 2;

    int textY =
        (board.rows +
         textSize.height) / 2;

    int paddingX =
        static_cast<int>(
            board.cols * 0.045
        );

    int paddingY =
        static_cast<int>(
            board.rows * 0.035
        );

    cv::Rect backgroundRect(
        textX - paddingX,
        textY -
            textSize.height -
            paddingY,
        textSize.width +
            paddingX * 2,
        textSize.height +
            paddingY * 2 +
            baseline
    );

    /*
     * מוודאים שהמלבן נשאר בתוך התמונה.
     */
    backgroundRect &=
        cv::Rect(
            0,
            0,
            board.cols,
            board.rows
        );

    cv::Mat overlay =
        board.clone();

    cv::rectangle(
        overlay,
        backgroundRect,
        cv::Scalar(
            15,
            15,
            15
        ),
        cv::FILLED
    );

    /*
     * רקע כהה חצי שקוף.
     */
    cv::addWeighted(
        overlay,
        0.78,
        board,
        0.22,
        0.0,
        board
    );

    /*
     * מסגרת בהירה מסביב להודעה.
     */
    cv::rectangle(
        board,
        backgroundRect,
        cv::Scalar(
            240,
            240,
            240
        ),
        3,
        cv::LINE_AA
    );

    /*
     * צל לכיתוב.
     */
    cv::putText(
        board,
        text,
        cv::Point(
            textX + 5,
            textY + 5
        ),
        cv::FONT_HERSHEY_DUPLEX,
        fontScale,
        cv::Scalar(
            0,
            0,
            0
        ),
        thickness + 4,
        cv::LINE_AA
    );

    /*
     * הכיתוב הראשי.
     */
    cv::putText(
        board,
        text,
        cv::Point(
            textX,
            textY
        ),
        cv::FONT_HERSHEY_DUPLEX,
        fontScale,
        cv::Scalar(
            255,
            255,
            255
        ),
        thickness,
        cv::LINE_AA
    );
}

std::string Renderer::buildPieceImagePath(
    const VisualPiece& piece
) const {
    return
        "assets\\pieces\\" +
        piece.pieceCode +
        "\\states\\" +
        visualStateToFolderName(
            piece.state
        ) +
        "\\sprites\\1.png";
}

std::string Renderer::buildFallbackIdlePath(
    const VisualPiece& piece
) const {
    return
        "assets\\pieces\\" +
        piece.pieceCode +
        "\\states\\idle\\sprites\\1.png";
}

int Renderer::getSpriteSizeForPiece(
    const VisualPiece& piece,
    int defaultSize
) const {
    bool isKing =
        piece.pieceCode == "wK" ||
        piece.pieceCode == "bK";

    bool isQueen =
        piece.pieceCode == "wQ" ||
        piece.pieceCode == "bQ";

    if (isKing || isQueen) {
        return static_cast<int>(
            defaultSize * 1.35
        );
    }

    return defaultSize;
}

bool Renderer::isRestState(
    VisualState state
) const {
    return
        state ==
            VisualState::ShortRest ||
        state ==
            VisualState::LongRest;
}

bool Renderer::fileExists(
    const std::string& path
) const {
    return fs::exists(path);
}