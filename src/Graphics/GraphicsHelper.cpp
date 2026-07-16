#include "../../include/Graphics/GraphicsHelper.hpp"

#include <stdexcept>

cv::Mat GraphicsHelper::readImage(const std::string& path) {
    cv::Mat image = cv::imread(path, cv::IMREAD_UNCHANGED);

    if (image.empty()) {
        throw std::runtime_error("Cannot load image: " + path);
    }

    return image;
}

void GraphicsHelper::drawTransparent(cv::Mat& target, const cv::Mat& source, int x, int y) {
    if (target.empty() || source.empty()) {
        throw std::runtime_error("Images must be loaded before drawing.");
    }

    cv::Mat sourceImage = source;

    if (sourceImage.channels() == 3) {
        cv::cvtColor(sourceImage, sourceImage, cv::COLOR_BGR2BGRA);
    }

    if (target.channels() == 3) {
        cv::cvtColor(target, target, cv::COLOR_BGR2BGRA);
    }

    int drawWidth = sourceImage.cols;
    int drawHeight = sourceImage.rows;

    if (x >= target.cols || y >= target.rows) {
        return;
    }

    if (x + drawWidth > target.cols) {
        drawWidth = target.cols - x;
    }

    if (y + drawHeight > target.rows) {
        drawHeight = target.rows - y;
    }

    if (drawWidth <= 0 || drawHeight <= 0) {
        return;
    }

    for (int row = 0; row < drawHeight; row++) {
        for (int col = 0; col < drawWidth; col++) {
            cv::Vec4b sourcePixel = sourceImage.at<cv::Vec4b>(row, col);
            cv::Vec4b& targetPixel = target.at<cv::Vec4b>(y + row, x + col);

            double alpha = sourcePixel[3] / 255.0;

            targetPixel[0] = static_cast<uchar>((1.0 - alpha) * targetPixel[0] + alpha * sourcePixel[0]);
            targetPixel[1] = static_cast<uchar>((1.0 - alpha) * targetPixel[1] + alpha * sourcePixel[1]);
            targetPixel[2] = static_cast<uchar>((1.0 - alpha) * targetPixel[2] + alpha * sourcePixel[2]);
            targetPixel[3] = 255;
        }
    }
}

void GraphicsHelper::show(const std::string& windowName, const cv::Mat& image) {
    cv::imshow(windowName, image);
    cv::waitKey(0);
    cv::destroyAllWindows();
}

void GraphicsHelper::drawTransparentWithOpacity(cv::Mat& background, const cv::Mat& foreground, int x, int y, double opacity) {
    if (background.empty() || foreground.empty()) {
        return;
    }

    if (opacity < 0.0) {
        opacity = 0.0;
    }

    if (opacity > 1.0) {
        opacity = 1.0;
    }

    for (int row = 0; row < foreground.rows; row++) {
        for (int col = 0; col < foreground.cols; col++) {
            int bgX = x + col;
            int bgY = y + row;

            if (bgX < 0 || bgY < 0 || bgX >= background.cols || bgY >= background.rows) {
                continue;
            }

            double alpha = opacity;

            cv::Vec3b foregroundColor;

            if (foreground.channels() == 4) {
                cv::Vec4b pixel = foreground.at<cv::Vec4b>(row, col);

                double pngAlpha = pixel[3] / 255.0;
                alpha *= pngAlpha;

                foregroundColor = cv::Vec3b(pixel[0], pixel[1], pixel[2]);
            } else {
                foregroundColor = foreground.at<cv::Vec3b>(row, col);
            }

            cv::Vec3b& backgroundColor = background.at<cv::Vec3b>(bgY, bgX);

            for (int channel = 0; channel < 3; channel++) {
                backgroundColor[channel] = static_cast<unsigned char>(
                    backgroundColor[channel] * (1.0 - alpha) +
                    foregroundColor[channel] * alpha
                );
            }
        }
    }
}