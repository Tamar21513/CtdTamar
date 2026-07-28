#pragma once

#include <opencv2/opencv.hpp>
#include <string>

class GraphicsHelper {
public:
    static cv::Mat readImage(const std::string& path);
    static void drawTransparent(cv::Mat& target, const cv::Mat& source, int x, int y);
    static void show(const std::string& windowName, const cv::Mat& image);
    static void drawTransparentWithOpacity(cv::Mat& background, const cv::Mat& foreground, int x, int y, double opacity);
};