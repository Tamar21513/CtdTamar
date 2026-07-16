#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

#include "VisualState.hpp"

struct VisualPiece {
    std::string pieceCode;
    VisualState state;
    int row;
    int col;
    double pixelX;
    double pixelY;
    double cooldownRatio;
    std::string imagePath;
    double opacity = 1.0;
};

class Renderer {
public:
    Renderer(const std::string& boardImagePath, int boardDisplaySize);
    Renderer(const std::string& boardImagePath, int boardDisplaySize, const std::string& windowName);

    void render(const std::vector<VisualPiece>& pieces);

private:
    std::string boardImagePath;
    int boardDisplaySize;
    std::string windowName;

    void drawPieces(cv::Mat& board, const std::vector<VisualPiece>& pieces);
    void drawSinglePiece(cv::Mat& board, const VisualPiece& piece, int boardStartX, int boardStartY, int cellSizeX, int cellSizeY, int defaultSpriteSize);
    void drawRestEffects(cv::Mat& board, const VisualPiece& piece, int pieceX, int pieceY, int spriteSize);
    void drawCooldownHourglass(cv::Mat& board, int x, int y, double cooldownRatio);
    void drawSleepZ(cv::Mat& board, int x, int y, VisualState state);

    std::string buildPieceImagePath(const VisualPiece& piece) const;
    std::string buildFallbackIdlePath(const VisualPiece& piece) const;

    int getSpriteSizeForPiece(const VisualPiece& piece, int defaultSize) const;
    bool isRestState(VisualState state) const;
    bool fileExists(const std::string& path) const;
};