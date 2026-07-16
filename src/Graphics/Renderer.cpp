#include "../../include/Graphics/Renderer.hpp"
#include "../../include/Graphics/GraphicsHelper.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

Renderer::Renderer(const std::string& boardImagePath, int boardDisplaySize) {
    this->boardImagePath = boardImagePath;
    this->boardDisplaySize = boardDisplaySize;
    this->windowName = "Kung Fu Chess Visual Motion Test";
}

Renderer::Renderer(const std::string& boardImagePath, int boardDisplaySize, const std::string& windowName) {
    this->boardImagePath = boardImagePath;
    this->boardDisplaySize = boardDisplaySize;
    this->windowName = windowName;
}

void Renderer::render(const std::vector<VisualPiece>& pieces) {
    cv::Mat board = GraphicsHelper::readImage(boardImagePath);

    if (board.empty()) {
        std::cout << "Could not load board image: " << boardImagePath << std::endl;
        return;
    }

    cv::resize(board, board, cv::Size(boardDisplaySize, boardDisplaySize));

    drawPieces(board, pieces);

    cv::imshow(windowName, board);
}

void Renderer::drawPieces(cv::Mat& board, const std::vector<VisualPiece>& pieces) {
    int boardWidth = board.cols;
    int boardHeight = board.rows;

    int cellSizeX = static_cast<int>(boardWidth * 0.098);
    int cellSizeY = static_cast<int>(boardHeight * 0.097);

    int defaultSpriteSize = static_cast<int>(boardWidth * 0.105);

    int boardStartX = static_cast<int>(boardWidth * 0.11);
    int boardStartY = static_cast<int>(boardHeight * 0.11);

    for (const VisualPiece& piece : pieces) {
        drawSinglePiece(board, piece, boardStartX, boardStartY, cellSizeX, cellSizeY, defaultSpriteSize);
    }
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
    std::string imagePath = piece.imagePath;

    if (imagePath.empty()) {
        imagePath = buildPieceImagePath(piece);
    }

    if (!fileExists(imagePath)) {
        imagePath = buildFallbackIdlePath(piece);
    }

    if (!fileExists(imagePath)) {
        std::cout << "Missing sprite for piece: " << piece.pieceCode << std::endl;
        return;
    }

    cv::Mat sprite = GraphicsHelper::readImage(imagePath);

    if (sprite.empty()) {
        std::cout << "Could not load sprite: " << imagePath << std::endl;
        return;
    }

    int spriteSize = getSpriteSizeForPiece(piece, defaultSpriteSize);
    cv::resize(sprite, sprite, cv::Size(spriteSize, spriteSize));

    int x = static_cast<int>(piece.pixelX - spriteSize / 2.0);
    int y = static_cast<int>(piece.pixelY - spriteSize / 2.0);

    double opacity = piece.opacity;

    if (opacity <= 0.0 || opacity > 1.0) {
        opacity = 1.0;
    }

    if (opacity >= 0.99) {
        GraphicsHelper::drawTransparent(board, sprite, x, y);
    } else {
        GraphicsHelper::drawTransparentWithOpacity(board, sprite, x, y, opacity);
    }

    if (isRestState(piece.state)) {
        drawRestEffects(board, piece, x, y, spriteSize);
    }
}

void Renderer::drawRestEffects(cv::Mat& board, const VisualPiece& piece, int pieceX, int pieceY, int spriteSize) {
    int zX = pieceX + static_cast<int>(spriteSize * 0.70);
    int zY = pieceY + static_cast<int>(spriteSize * 0.22);

    drawSleepZ(board, zX, zY, piece.state);

    int hourglassX = pieceX + static_cast<int>(spriteSize * 0.68);
    int hourglassY = pieceY + static_cast<int>(spriteSize * 0.62);

    drawCooldownHourglass(board, hourglassX, hourglassY, piece.cooldownRatio);
}

void Renderer::drawCooldownHourglass(cv::Mat& board, int x, int y, double cooldownRatio) {
    if (cooldownRatio < 0.0) {
        cooldownRatio = 0.0;
    }

    if (cooldownRatio > 1.0) {
        cooldownRatio = 1.0;
    }

    int width = 18;
    int height = 28;

    cv::Scalar outlineColor(60, 40, 20, 255);
    cv::Scalar sandColor(40, 180, 240, 255);
    cv::Scalar emptyColor(230, 230, 230, 255);

    cv::rectangle(board, cv::Rect(x, y, width, height), outlineColor, 2);

    cv::line(board, cv::Point(x + 2, y + 2), cv::Point(x + width - 3, y + height - 3), outlineColor, 1);
    cv::line(board, cv::Point(x + width - 3, y + 2), cv::Point(x + 2, y + height - 3), outlineColor, 1);

    int sandHeight = static_cast<int>((height - 6) * cooldownRatio);

    cv::rectangle(
        board,
        cv::Rect(x + 4, y + height - 4 - sandHeight, width - 8, sandHeight),
        sandColor,
        cv::FILLED
    );

    if (sandHeight <= 1) {
        cv::circle(board, cv::Point(x + width / 2, y + height - 5), 2, emptyColor, cv::FILLED);
    }
}

void Renderer::drawSleepZ(cv::Mat& board, int x, int y, VisualState state) {
    double fontScale = 0.55;
    int thickness = 2;

    if (state == VisualState::LongRest) {
        fontScale = 0.75;
        thickness = 2;
    }

    cv::Scalar zColor(255, 255, 255, 255);
    cv::Scalar shadowColor(70, 70, 70, 255);

    cv::putText(board, "Z", cv::Point(x + 1, y + 1), cv::FONT_HERSHEY_SIMPLEX, fontScale, shadowColor, thickness + 1);
    cv::putText(board, "Z", cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX, fontScale, zColor, thickness);

    if (state == VisualState::LongRest) {
        cv::putText(board, "z", cv::Point(x + 13, y - 14), cv::FONT_HERSHEY_SIMPLEX, 0.45, shadowColor, 2);
        cv::putText(board, "z", cv::Point(x + 12, y - 15), cv::FONT_HERSHEY_SIMPLEX, 0.45, zColor, 1);
    }
}

std::string Renderer::buildPieceImagePath(const VisualPiece& piece) const {
    return "assets\\pieces\\" +
           piece.pieceCode +
           "\\states\\" +
           visualStateToFolderName(piece.state) +
           "\\sprites\\1.png";
}

std::string Renderer::buildFallbackIdlePath(const VisualPiece& piece) const {
    return "assets\\pieces\\" +
           piece.pieceCode +
           "\\states\\idle\\sprites\\1.png";
}

int Renderer::getSpriteSizeForPiece(const VisualPiece& piece, int defaultSize) const {
    bool isKing = piece.pieceCode == "wK" || piece.pieceCode == "bK";
    bool isQueen = piece.pieceCode == "wQ" || piece.pieceCode == "bQ";

    if (isKing || isQueen) {
        return static_cast<int>(defaultSize * 1.35);
    }

    return defaultSize;
}

bool Renderer::isRestState(VisualState state) const {
    return state == VisualState::ShortRest || state == VisualState::LongRest;
}

bool Renderer::fileExists(const std::string& path) const {
    return fs::exists(path);
}

