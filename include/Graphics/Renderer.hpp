#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include "VisualState.hpp"
#include "../Core/MoveHistory.hpp"

struct VisualPiece {
    int pieceId = -1;
    std::string pieceCode;
    VisualState state = VisualState::Idle;
    int row = 0;
    int col = 0;
    double pixelX = 0.0;
    double pixelY = 0.0;
    double cooldownRatio = 0.0;
    std::string imagePath;
    double opacity = 1.0;
    bool alignVisibleCenter = false;
};

class Renderer {
public:
    static const int SIDE_PANEL_WIDTH = 250;
    static const int SCORE_PANEL_HEIGHT = 70;

    Renderer(const std::string& boardImagePath, int boardDisplaySize);
    Renderer(const std::string& boardImagePath, int boardDisplaySize, const std::string& windowName);

    void render(
        const std::vector<VisualPiece>& pieces,
        bool gameOver = false,
        int blackScore = 0,
        int whiteScore = 0,
        const std::vector<MoveHistoryEntry>& blackMoves = {},
        const std::vector<MoveHistoryEntry>& whiteMoves = {}
    );

    cv::Mat cropTransparentMargins(const cv::Mat& sprite) const;

private:
    std::string boardImagePath;
    int boardDisplaySize;
    std::string windowName;

    void drawPieces(cv::Mat& canvas, const std::vector<VisualPiece>& pieces);
    void drawSinglePiece(cv::Mat& board, const VisualPiece& piece, int boardStartX, int boardStartY,
        int cellSizeX, int cellSizeY, int defaultSpriteSize);
    void drawRestEffects(cv::Mat& board, const VisualPiece& piece, int pieceX, int pieceY, int spriteSize);
    void drawCooldownHourglass(cv::Mat& board, int x, int y, double cooldownRatio);
    void drawHourglassFrame(cv::Mat& board, int x, int y, int width, int height) const;
    void drawHourglassSand(
        cv::Mat& board,
        int x,
        int y,
        int width,
        int height,
        double cooldownRatio
    ) const;
    void drawSleepZ(cv::Mat& board, int x, int y, VisualState state);
    void drawGameOver(cv::Mat& board);
    void drawScore(cv::Mat& canvas, const std::string& label, int score, int y);
    void drawMoveHistory(cv::Mat& canvas, const std::vector<MoveHistoryEntry>& moves,
        const std::string& title, int x, int y, int width, int height);
    std::string formatTime(long long timeMs) const;
    cv::Point2d findVisibleCenter(const cv::Mat& sprite) const;
    cv::Mat loadBoardImage() const;
    cv::Mat loadPieceSprite(const VisualPiece& piece) const;
    cv::Mat resizePieceSprite(const cv::Mat& sprite, int spriteSize, bool cropMargins) const;
    std::string buildPieceImagePath(const VisualPiece& piece) const;
    std::string buildFallbackIdlePath(const VisualPiece& piece) const;
    int getSpriteSizeForPiece(const VisualPiece& piece, int defaultSize) const;
    bool isRestState(VisualState state) const;
    bool fileExists(const std::string& path) const;
};
