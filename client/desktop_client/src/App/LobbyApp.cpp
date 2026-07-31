#include "App/LobbyApp.hpp"

#include "Lobby/LobbyApplicationState.hpp"
#include "Lobby/LobbyController.hpp"
#include "Lobby/LobbyInputMapper.hpp"
#include "Lobby/LobbyLayout.hpp"
#include "Lobby/LobbyRenderer.hpp"
#include "Lobby/LobbyTransport.hpp"
#include "Graphics/AnimationLibrary.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/VisualSnapshotBuilder.hpp"
#include "Logging/FileLogger.hpp"

#include <opencv2/highgui.hpp>

#include <algorithm>
#include <deque>
#include <chrono>

namespace ctd::lobby {
namespace {

const char* WindowName = "Kung Fu Chess";

enum class PointerEventType { Click, Jump, Wheel };

struct PointerEvent {
    PointerEventType type;
    cv::Point position;
    int wheelDelta = 0;
};

struct PointerState {
    cv::Point position{-1, -1};
    std::deque<PointerEvent> events;
};

void mouseCallback(
    int event,
    int x,
    int y,
    int flags,
    void* context) {
    auto& pointer = *static_cast<PointerState*>(context);
    pointer.position = {x, y};
    if (event == cv::EVENT_LBUTTONDOWN) {
        pointer.events.push_back({
            PointerEventType::Click, {x, y}, 0});
    } else if (event == cv::EVENT_RBUTTONDOWN) {
        pointer.events.push_back({
            PointerEventType::Jump, {x, y}, 0});
    } else if (event == cv::EVENT_MOUSEWHEEL) {
        pointer.events.push_back({
            PointerEventType::Wheel,
            {x, y},
            cv::getMouseWheelDelta(flags)});
    }
}

void handleKey(
    int key,
    LobbyController& controller,
    const LobbyViewModel& view) {
    if (key == 8) {
        controller.backspace();
        return;
    }
    if (key == 9 &&
        view.screen == LobbyScreen::Authentication) {
        controller.handleTab();
        return;
    }
    if (key == 13) {
        controller.handleEnter();
        return;
    }
    if (key >= 32 && key <= 126) {
        controller.inputCharacter(static_cast<char>(key));
    }
}

std::vector<MoveHistoryEntry> convertHistory(
    const std::vector<MoveHistorySnapshot>& values) {
    std::vector<MoveHistoryEntry> result;
    result.reserve(values.size());
    for (const auto& value : values) {
        MoveHistoryEntry entry;
        entry.completedAtMs = value.completedAtMs;
        entry.color = value.color == "b"
            ? PieceColor::Black
            : PieceColor::White;
        entry.pieceKind = value.pieceKind.empty()
            ? PieceKind::Pawn
            : Piece::kindFromChar(value.pieceKind[0]);
        entry.source = value.source;
        entry.destination = value.destination;
        entry.wasCapture = value.wasCapture;
        entry.wasPromotion = value.wasPromotion;
        entry.wasJump = value.wasJump;
        entry.notation = value.notation;
        result.push_back(entry);
    }
    return result;
}

}  // namespace

void LobbyApp::run() {
    auto& logger = ctd::logging::defaultLogger();
    logger.info("LobbyApp starting");
    cv::namedWindow(WindowName, cv::WINDOW_NORMAL);
    cv::resizeWindow(WindowName, 1366, 768);
    logger.info("Window created");

    PointerState pointer;
    cv::setMouseCallback(WindowName, mouseCallback, &pointer);

    LobbyApplicationState state;
    LobbyTransport transport;
    LobbyController controller(state, transport);
    LobbyLayout layoutCalculator;
    LobbyInputMapper inputMapper;
    LobbyRenderer renderer;
    constexpr int boardSize = 600;
    constexpr int boardInset = 66;
    constexpr int boardStartX =
        Renderer::SIDE_PANEL_WIDTH + boardInset;
    constexpr int boardStartY =
        Renderer::SCORE_PANEL_HEIGHT + boardInset;
    constexpr int cellSize = 58;
    AnimationLibrary animationLibrary;
    VisualSnapshotBuilder snapshotBuilder(
        boardStartX,
        boardStartY,
        cellSize,
        cellSize,
        63,
        animationLibrary);
    Renderer gameRenderer(
        R"(assets\Board\board.png)",
        boardSize,
        WindowName);
    auto previousFrame = std::chrono::steady_clock::now();
    LobbyScreen previousScreen = state.view().screen;

    logger.info("Entering main loop");
    bool running = true;
    while (running) {
        controller.tick();
        const auto view = state.snapshot();
        if (view.screen != previousScreen) {
            const bool gameScreen =
                view.screen == LobbyScreen::Game ||
                view.screen == LobbyScreen::SpectatorGame;
            cv::resizeWindow(
                WindowName,
                gameScreen ? 1100 : 1366,
                gameScreen ? 740 : 768);
            previousScreen = view.screen;
        }
        const auto imageRect = cv::getWindowImageRect(WindowName);
        const int width = std::max(
            LobbyLayout::MinimumWidth,
            imageRect.width > 0 ? imageRect.width : 1366);
        const int height = std::max(
            LobbyLayout::MinimumHeight,
            imageRect.height > 0 ? imageRect.height : 768);
        const auto layout = layoutCalculator.calculate(
            width,
            height,
            static_cast<int>(view.waitingRooms.size()),
            static_cast<int>(view.activeRooms.size()),
            view.waitingPage,
            view.activePage,
            view.authMode == AuthenticationMode::Register);

        while (!pointer.events.empty()) {
            const auto event = pointer.events.front();
            pointer.events.pop_front();
            if (event.type == PointerEventType::Click) {
                if (state.view().screen ==
                        LobbyScreen::SpectatorGame &&
                    Renderer::spectatorBackButtonRect().contains(
                        event.position)) {
                    controller.handle({
                        LobbyActionType::BackToLobby, {}});
                } else if (
                    state.view().screen == LobbyScreen::Game) {
                    const int boardX =
                        event.position.x - boardStartX;
                    const int boardY =
                        event.position.y - boardStartY;
                    if (boardX >= 0 && boardY >= 0 &&
                        boardX < cellSize * 8 &&
                        boardY < cellSize * 8) {
                        controller.gameBoardClick(
                            boardY / cellSize,
                            boardX / cellSize);
                    }
                } else if (
                    state.view().screen !=
                    LobbyScreen::SpectatorGame) {
                    controller.handle(inputMapper.mapClick(
                        event.position, state.view(), layout));
                }
            } else if (event.type == PointerEventType::Jump) {
                if (state.view().screen == LobbyScreen::Game) {
                    const int boardX =
                        event.position.x - boardStartX;
                    const int boardY =
                        event.position.y - boardStartY;
                    if (boardX >= 0 && boardY >= 0 &&
                        boardX < cellSize * 8 &&
                        boardY < cellSize * 8) {
                        controller.gameBoardJump(
                            boardY / cellSize,
                            boardX / cellSize);
                    }
                }
            } else if (
                state.view().screen == LobbyScreen::Lobby) {
                const bool waiting =
                    layout.waitingSection.contains(event.position);
                const int itemCount = waiting
                    ? static_cast<int>(
                        state.view().waitingRooms.size())
                    : static_cast<int>(
                        state.view().activeRooms.size());
                const int maximumPage = std::max(
                    0,
                    (itemCount - 1) / layout.cardsPerPage);
                controller.handle({
                    event.wheelDelta < 0
                        ? (waiting
                            ? LobbyActionType::NextWaitingPage
                            : LobbyActionType::NextActivePage)
                        : (waiting
                            ? LobbyActionType::PreviousWaitingPage
                            : LobbyActionType::PreviousActivePage),
                    std::to_string(maximumPage)});
            }
        }

        const auto frameView = state.snapshot();
        if ((frameView.screen == LobbyScreen::Game ||
             frameView.screen == LobbyScreen::SpectatorGame) &&
            frameView.match) {
            const auto now = std::chrono::steady_clock::now();
            const auto delta = std::chrono::duration_cast<
                std::chrono::milliseconds>(
                now - previousFrame).count();
            previousFrame = now;
            const auto& match = *frameView.match;
            const auto presentation =
                gameplayPresentation(frameView);
            gameRenderer.render(
                snapshotBuilder.build(match.snapshot, delta),
                match.snapshot.gameOver,
                match.snapshot.blackScore,
                match.snapshot.whiteScore,
                convertHistory(
                    match.snapshot.blackMoveHistory),
                convertHistory(
                    match.snapshot.whiteMoveHistory),
                match.spectator ? "" : match.assignedColor,
                match.snapshot.whiteUsername,
                match.snapshot.blackUsername,
                presentation.statusLine1,
                presentation.statusLine2,
                RenderOverlayMode::None,
                presentation.showSpectatorBackButton,
                match.phase == MatchPhase::Countdown
                    ? match.countdownValue : "");
        } else {
            previousFrame = std::chrono::steady_clock::now();
            const auto frame = renderer.render(
                frameView, layout, pointer.position);
            cv::imshow(WindowName, frame);
        }
        const int key = cv::waitKeyEx(16);
        if (key == 27) {
            if (state.view().screen ==
                LobbyScreen::SpectatorGame) {
                controller.handle({
                    LobbyActionType::BackToLobby, {}});
            } else if (state.view().modal != LobbyModal::None) {
                controller.handle({
                    LobbyActionType::CancelModal, {}});
            } else {
                running = false;
            }
        } else {
            handleKey(key, controller, state.view());
        }
        if (cv::getWindowProperty(
                WindowName, cv::WND_PROP_VISIBLE) < 1) {
            running = false;
        }
    }
    transport.disconnectRealtime();
    logger.info("Destroying window");
    try {
        if (cv::getWindowProperty(
                WindowName,
                cv::WND_PROP_VISIBLE) >= 0) {
            cv::destroyWindow(WindowName);
        }
    } catch (...) {
    }
}

}  // namespace ctd::lobby
