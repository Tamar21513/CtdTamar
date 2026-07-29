#pragma once

#include "Lobby/LobbyAction.hpp"
#include "Lobby/LobbyApplicationState.hpp"
#include "Lobby/LobbyLayout.hpp"

namespace ctd::lobby {

class LobbyInputMapper {
public:
    LobbyAction mapClick(
        const cv::Point& point,
        const LobbyViewModel& view,
        const LobbyLayoutResult& layout) const;
};

}  // namespace ctd::lobby
