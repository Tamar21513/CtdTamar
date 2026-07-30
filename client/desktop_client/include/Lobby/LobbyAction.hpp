#pragma once

#include <string>

namespace ctd::lobby {

enum class LobbyActionType {
    None,
    FocusUsername,
    FocusPassword,
    Login,
    Register,
    OpenCreateRoom,
    OpenJoinHidden,
    SelectPublicVisibility,
    SelectHiddenVisibility,
    SubmitCreateRoom,
    SubmitHiddenCode,
    CancelModal,
    JoinPublicRoom,
    WatchRoom,
    BackToLobby,
    Logout,
    NextWaitingPage,
    PreviousWaitingPage,
    NextActivePage,
    PreviousActivePage
};

struct LobbyAction {
    LobbyActionType type = LobbyActionType::None;
    std::string value;
};

}  // namespace ctd::lobby
