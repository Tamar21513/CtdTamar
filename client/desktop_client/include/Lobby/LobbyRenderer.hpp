#pragma once

#include "Lobby/LobbyApplicationState.hpp"
#include "Lobby/LobbyLayout.hpp"

#include <opencv2/core/mat.hpp>

namespace ctd::lobby {

class LobbyRenderer {
public:
    cv::Mat render(
        const LobbyViewModel& view,
        const LobbyLayoutResult& layout,
        const cv::Point& mousePosition = {-1, -1}) const;

private:
    void drawAuthentication(
        cv::Mat& frame,
        const LobbyViewModel& view,
        const LobbyLayoutResult& layout,
        const cv::Point& mouse) const;
    void drawLobby(
        cv::Mat& frame,
        const LobbyViewModel& view,
        const LobbyLayoutResult& layout,
        const cv::Point& mouse) const;
    void drawHeader(
        cv::Mat& frame,
        const LobbyViewModel& view,
        const LobbyLayoutResult& layout,
        const cv::Point& mouse) const;
    void drawRoomCard(
        cv::Mat& frame,
        const ctd::network::LobbyRoom& room,
        const cv::Rect& rectangle,
        bool active,
        bool enabled,
        bool pending,
        const cv::Point& mouse) const;
    void drawModal(
        cv::Mat& frame,
        const LobbyViewModel& view,
        const LobbyLayoutResult& layout,
        const cv::Point& mouse) const;
    void drawPlaceholder(
        cv::Mat& frame,
        const LobbyViewModel& view,
        const LobbyLayoutResult& layout,
        const cv::Point& mouse) const;
};

}  // namespace ctd::lobby
