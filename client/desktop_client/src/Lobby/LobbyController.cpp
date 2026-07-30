#include "Lobby/LobbyController.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <type_traits>

namespace ctd::lobby {

LobbyController::LobbyController(
    LobbyApplicationState& state,
    ILobbyTransport& transport)
    : state_(state),
      transport_(transport) {}

LobbyController::~LobbyController() {
    if (authenticationTaskActive_) {
        authenticationTask_.wait();
    }
    std::fill(password_.begin(), password_.end(), '\0');
    password_.clear();
}

void LobbyController::beginAuthentication(bool registration) {
    auto& view = state_.editableForInput();
    if (authenticationTaskActive_ ||
        view.pendingAction != PendingLobbyAction::None) {
        return;
    }
    if (view.usernameInput.size() < 3 || password_.size() < 10) {
        state_.authenticationFailed(
            "Enter a valid username and a password of at least 10 characters.");
        return;
    }
    const auto username = view.usernameInput;
    auto password = password_;
    state_.beginAuthentication(
        registration
            ? PendingLobbyAction::Register
            : PendingLobbyAction::Login);
    authenticationTaskActive_ = true;
    authenticationTask_ = std::async(
        std::launch::async,
        [this, username, password, registration]() mutable {
            ctd::network::AuthResult result;
            if (registration) {
                result = transport_.registerUser(username, password);
                if (result.succeeded()) {
                    result = transport_.login(username, password);
                }
            } else {
                result = transport_.login(username, password);
            }
            std::fill(password.begin(), password.end(), '\0');
            return AuthenticationOutcome{
                std::move(result), username};
        });
}

void LobbyController::finishAuthentication(
    AuthenticationOutcome outcome) {
    const bool registration =
        state_.view().pendingAction ==
            PendingLobbyAction::Register;
    std::fill(password_.begin(), password_.end(), '\0');
    password_.clear();
    state_.editableForInput().passwordLength = 0;
    if (!outcome.result.succeeded() || !outcome.result.user) {
        state_.authenticationFailed(
            authErrorMessage(outcome.result, registration));
        return;
    }
    state_.authenticationSucceeded(outcome.result.user->username);
    if (!transport_.connectRealtime()) {
        state_.authenticationFailed(
            "Could not connect to the lobby service.");
    }
}

bool LobbyController::beginRoomAction(
    PendingLobbyAction pending,
    const std::string& roomId) {
    auto& view = state_.editableForInput();
    if (!view.networkActionsEnabled()) {
        return false;
    }
    view.pendingAction = pending;
    view.pendingRoomId = roomId;
    view.visibleError.clear();
    return true;
}

void LobbyController::handle(const LobbyAction& action) {
    auto& view = state_.editableForInput();
    switch (action.type) {
        case LobbyActionType::FocusUsername:
            view.focusedField = AuthenticationField::Username;
            break;
        case LobbyActionType::FocusPassword:
            view.focusedField = AuthenticationField::Password;
            break;
        case LobbyActionType::Login:
            beginAuthentication(false);
            break;
        case LobbyActionType::Register:
            beginAuthentication(true);
            break;
        case LobbyActionType::OpenCreateRoom:
            if (view.networkActionsEnabled()) {
                view.modal = LobbyModal::CreateRoom;
                view.selectedRoomVisibility =
                    RoomVisibilitySelection::Public;
                view.visibleError.clear();
            }
            break;
        case LobbyActionType::OpenJoinHidden:
            if (view.networkActionsEnabled()) {
                view.modal = LobbyModal::JoinHiddenRoom;
                view.hiddenRoomCodeInput.clear();
            }
            break;
        case LobbyActionType::SelectPublicVisibility:
            view.selectedRoomVisibility =
                RoomVisibilitySelection::Public;
            break;
        case LobbyActionType::SelectHiddenVisibility:
            view.selectedRoomVisibility =
                RoomVisibilitySelection::Hidden;
            break;
        case LobbyActionType::SubmitCreateRoom: {
            const auto name = trimRoomName(view.roomNameInput);
            if (name.empty()) {
                state_.showError("Room name is required.");
                break;
            }
            if (name.size() > 40) {
                state_.showError(
                    "Room name must be at most 40 UTF-8 bytes.");
                break;
            }
            const bool hidden =
                view.selectedRoomVisibility ==
                RoomVisibilitySelection::Hidden;
            if (beginRoomAction(
                    hidden
                        ? PendingLobbyAction::CreateHiddenRoom
                        : PendingLobbyAction::CreatePublicRoom) &&
                !transport_.createRoom(name, hidden)) {
                state_.showError("Could not create the room.");
            }
            break;
        }
        case LobbyActionType::SubmitHiddenCode: {
            const auto code =
                normalizeHiddenCode(view.hiddenRoomCodeInput);
            if (code.size() != 6) {
                state_.showError(
                    "Room codes contain exactly six characters.");
                break;
            }
            if (beginRoomAction(
                    PendingLobbyAction::JoinHiddenRoom) &&
                !transport_.joinHiddenRoom(code)) {
                state_.showError("Could not join the hidden room.");
            }
            break;
        }
        case LobbyActionType::CancelModal:
            if (view.pendingAction == PendingLobbyAction::None) {
                view.modal = LobbyModal::None;
            }
            break;
        case LobbyActionType::JoinPublicRoom:
            if (beginRoomAction(
                    PendingLobbyAction::JoinPublicRoom,
                    action.value) &&
                !transport_.joinPublicRoom(action.value)) {
                state_.showError("Could not join the room.");
            }
            break;
        case LobbyActionType::WatchRoom:
            if (beginRoomAction(
                    PendingLobbyAction::WatchRoom,
                    action.value) &&
                !transport_.watchRoom(action.value)) {
                state_.showError("Could not watch the room.");
            }
            break;
        case LobbyActionType::BackToLobby:
            if (view.match && view.match->spectator) {
                transport_.leaveSpectator(view.match->roomId);
            }
            view.screen = LobbyScreen::Lobby;
            view.spectator.reset();
            view.roomReady.reset();
            view.match.reset();
            view.hiddenRoomCode.clear();
            view.currentRoomName.clear();
            view.pendingRoomId.clear();
            view.visibleError.clear();
            transport_.requestLobby();
            break;
        case LobbyActionType::Logout:
            transport_.logout();
            std::fill(password_.begin(), password_.end(), '\0');
            password_.clear();
            state_.resetForLogout();
            break;
        case LobbyActionType::NextWaitingPage:
            if (!action.value.empty()) {
                view.waitingPage = std::min(
                    view.waitingPage + 1,
                    std::max(0, std::stoi(action.value)));
            }
            break;
        case LobbyActionType::PreviousWaitingPage:
            view.waitingPage = std::max(0, view.waitingPage - 1);
            break;
        case LobbyActionType::NextActivePage:
            if (!action.value.empty()) {
                view.activePage = std::min(
                    view.activePage + 1,
                    std::max(0, std::stoi(action.value)));
            }
            break;
        case LobbyActionType::PreviousActivePage:
            view.activePage = std::max(0, view.activePage - 1);
            break;
        case LobbyActionType::None:
        default:
            break;
    }
}

void LobbyController::inputCharacter(char value) {
    auto& view = state_.editableForInput();
    if (view.screen == LobbyScreen::Authentication) {
        if (view.focusedField == AuthenticationField::Username) {
            if (std::isprint(static_cast<unsigned char>(value)) &&
                view.usernameInput.size() < 32) {
                view.usernameInput.push_back(value);
            }
        } else if (
            std::isprint(static_cast<unsigned char>(value)) &&
            password_.size() < 128) {
            password_.push_back(value);
            view.passwordLength = password_.size();
        }
    } else if (view.modal == LobbyModal::CreateRoom) {
        if (std::isprint(static_cast<unsigned char>(value))) {
            auto candidate = view.roomNameInput;
            candidate.push_back(value);
            if (candidate.size() <= 40) {
                view.roomNameInput = std::move(candidate);
                view.visibleError.clear();
            } else {
                state_.showError(
                    "Room name must be at most 40 UTF-8 bytes.");
            }
        }
    } else if (view.modal == LobbyModal::JoinHiddenRoom) {
        const auto upper = static_cast<char>(
            std::toupper(static_cast<unsigned char>(value)));
        if (std::isalnum(static_cast<unsigned char>(upper)) &&
            view.hiddenRoomCodeInput.size() < 6) {
            view.hiddenRoomCodeInput.push_back(upper);
        }
    }
}

void LobbyController::backspace() {
    auto& view = state_.editableForInput();
    if (view.screen == LobbyScreen::Authentication) {
        if (view.focusedField == AuthenticationField::Username) {
            if (!view.usernameInput.empty()) {
                view.usernameInput.pop_back();
            }
        } else if (!password_.empty()) {
            password_.back() = '\0';
            password_.pop_back();
            view.passwordLength = password_.size();
        }
    } else if (
        view.modal == LobbyModal::CreateRoom &&
        !view.roomNameInput.empty()) {
        view.roomNameInput.pop_back();
        view.visibleError.clear();
    } else if (
        view.modal == LobbyModal::JoinHiddenRoom &&
        !view.hiddenRoomCodeInput.empty()) {
        view.hiddenRoomCodeInput.pop_back();
    }
}

void LobbyController::applyTransportEvent(
    const ctd::network::WebSocketEvent& event) {
    if (event.kind ==
        ctd::network::WebSocketEventKind::AuthenticationFailed) {
        state_.resetForLogout();
        state_.authenticationFailed(
            "Your session expired. Please sign in again.");
    } else if (
        event.kind == ctd::network::WebSocketEventKind::Closed ||
        event.kind == ctd::network::WebSocketEventKind::TransportError) {
        state_.setConnectionState(
            ctd::network::WebSocketConnectionState::Disconnected);
    }
}

void LobbyController::consumeNetworkEvents() {
    while (auto envelope = transport_.pollEvent()) {
        if (envelope->transportEvent) {
            applyTransportEvent(*envelope->transportEvent);
        } else if (envelope->protocolEvent) {
            const bool connected = std::holds_alternative<
                ctd::network::ConnectedEvent>(
                *envelope->protocolEvent);
            state_.applyEvent(*envelope->protocolEvent);
            if (connected && !transport_.subscribeLobby()) {
                state_.showError(
                    "Could not subscribe to lobby updates.");
            }
        } else if (!envelope->parseError.empty()) {
            state_.showError("The lobby sent an invalid message.");
        }
    }
}

void LobbyController::tick() {
    if (authenticationTaskActive_ &&
        authenticationTask_.wait_for(std::chrono::milliseconds(0)) ==
            std::future_status::ready) {
        auto outcome = authenticationTask_.get();
        authenticationTaskActive_ = false;
        finishAuthentication(std::move(outcome));
    }
    state_.setConnectionState(transport_.connectionState());
    consumeNetworkEvents();
}

bool LobbyController::hasAuthenticationTask() const {
    return authenticationTaskActive_;
}

std::string LobbyController::normalizeHiddenCode(
    const std::string& value) {
    const std::string allowed =
        "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::string normalized;
    for (const unsigned char character : value) {
        if (!std::isspace(character)) {
            const auto upper = static_cast<char>(
                std::toupper(character));
            if (allowed.find(upper) == std::string::npos) {
                return {};
            }
            normalized.push_back(upper);
        }
    }
    return normalized;
}

void LobbyController::gameBoardClick(int row, int col) {
    auto& view = state_.editableForInput();
    if (!view.match || view.match->spectator ||
        view.screen != LobbyScreen::Game ||
        row < 0 || row > 7 || col < 0 || col > 7) {
        return;
    }
    auto& match = *view.match;
    if (!match.selectedSource) {
        const auto piece = std::find_if(
            match.snapshot.pieces.begin(),
            match.snapshot.pieces.end(),
            [row, col](const PieceSnapshot& value) {
                return value.position.getRow() == row &&
                    value.position.getCol() == col &&
                    value.state != "captured";
            });
        const char expected =
            match.assignedColor == "black" ? 'b' : 'w';
        if (piece == match.snapshot.pieces.end() ||
            piece->token.empty() ||
            piece->token.front() != expected) {
            match.moveStatus = "Select one of your pieces.";
            return;
        }
        match.selectedSource = Position(row, col);
        match.moveStatus.clear();
        return;
    }

    const Position source = *match.selectedSource;
    const auto sequence = match.nextSequence++;
    match.selectedSource.reset();
    if (source == Position(row, col)) {
        if (!transport_.sendJump(
                match.roomId,
                sequence,
                row,
                col)) {
            match.moveStatus = "Could not send the jump.";
        }
        return;
    }
    if (!transport_.sendMove(
            match.roomId,
            sequence,
            source.getRow(),
            source.getCol(),
            row,
            col)) {
        match.moveStatus = "Could not send the move.";
    } else {
        match.moveStatus = "Waiting for authoritative result...";
    }
}

void LobbyController::gameBoardJump(int row, int col) {
    auto& view = state_.editableForInput();
    if (!view.match || view.match->spectator ||
        view.screen != LobbyScreen::Game ||
        row < 0 || row > 7 || col < 0 || col > 7) {
        return;
    }
    auto& match = *view.match;
    const auto piece = std::find_if(
        match.snapshot.pieces.begin(),
        match.snapshot.pieces.end(),
        [row, col](const PieceSnapshot& value) {
            return value.position.getRow() == row &&
                value.position.getCol() == col &&
                value.state != "captured";
        });
    const char expected =
        match.assignedColor == "black" ? 'b' : 'w';
    if (piece == match.snapshot.pieces.end() ||
        piece->token.empty() ||
        piece->token.front() != expected) {
        match.moveStatus = "Select one of your pieces.";
        return;
    }
    match.selectedSource.reset();
    const auto sequence = match.nextSequence++;
    if (!transport_.sendJump(
            match.roomId, sequence, row, col)) {
        match.moveStatus = "Could not send the jump.";
    }
}

std::string LobbyController::trimRoomName(
    const std::string& value) {
    const auto first = std::find_if_not(
        value.begin(), value.end(), [](unsigned char character) {
            return std::isspace(character);
        });
    if (first == value.end()) {
        return {};
    }
    const auto last = std::find_if_not(
        value.rbegin(), value.rend(), [](unsigned char character) {
            return std::isspace(character);
        }).base();
    return std::string(first, last);
}

std::string LobbyController::authErrorMessage(
    const ctd::network::AuthResult& result,
    bool registration) {
    using ctd::network::AuthResultKind;
    switch (result.kind) {
        case AuthResultKind::InvalidCredentials:
            return "Invalid username or password.";
        case AuthResultKind::AlreadyRegistered:
            return "That username is already registered.";
        case AuthResultKind::ValidationError:
            return registration
                ? "The username or password does not meet the requirements."
                : "Check the username and password format.";
        case AuthResultKind::TransportError:
            return "The authentication service is unreachable.";
        case AuthResultKind::ServerError:
            return "The authentication service is unavailable.";
        default:
            return "Authentication could not be completed.";
    }
}

}  // namespace ctd::lobby
