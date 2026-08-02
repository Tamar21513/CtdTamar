#pragma once

#include <string>

namespace ctd::lobby {

enum class LobbyActionType {
    None,
    FocusUsername,
    FocusPassword,
    FocusConfirmPassword,
    ToggleAuthMode,
    Login,
    Register,
    OpenCreateRoom,
    OpenJoinHidden,
    OpenMatchHistory,
    SelectPublicVisibility,
    SelectHiddenVisibility,
    SubmitCreateRoom,
    SubmitHiddenCode,
    CancelModal,
    JoinPublicRoom,
    WatchRoom,
    FindMatch,
    CancelFindMatch,
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
