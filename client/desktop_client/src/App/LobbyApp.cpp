#include "App/LobbyApp.hpp"

#include "Lobby/LobbyApplicationState.hpp"
#include "Lobby/LobbyController.hpp"
#include "Lobby/LobbyInputMapper.hpp"
#include "Lobby/LobbyLayout.hpp"
#include "Lobby/LobbyRenderer.hpp"
#include "Lobby/LobbyTransport.hpp"

#include <opencv2/highgui.hpp>

#include <algorithm>
#include <deque>

namespace ctd::lobby {
namespace {

const char* WindowName = "Kung Fu Chess";

enum class PointerEventType { Click, Wheel };

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
        controller.handle({
            view.focusedField == AuthenticationField::Username
                ? LobbyActionType::FocusPassword
                : LobbyActionType::FocusUsername,
            {}});
        return;
    }
    if (key == 13) {
        if (view.modal == LobbyModal::JoinHiddenRoom) {
            controller.handle({
                LobbyActionType::SubmitHiddenCode, {}});
        } else if (view.screen == LobbyScreen::Authentication) {
            controller.handle({LobbyActionType::Login, {}});
        }
        return;
    }
    if (key >= 32 && key <= 126) {
        controller.inputCharacter(static_cast<char>(key));
    }
}

}  // namespace

void LobbyApp::run() {
    cv::namedWindow(WindowName, cv::WINDOW_NORMAL);
    cv::resizeWindow(WindowName, 1366, 768);

    PointerState pointer;
    cv::setMouseCallback(WindowName, mouseCallback, &pointer);

    LobbyApplicationState state;
    LobbyTransport transport;
    LobbyController controller(state, transport);
    LobbyLayout layoutCalculator;
    LobbyInputMapper inputMapper;
    LobbyRenderer renderer;

    bool running = true;
    while (running) {
        controller.tick();
        const auto view = state.snapshot();
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
            view.activePage);

        while (!pointer.events.empty()) {
            const auto event = pointer.events.front();
            pointer.events.pop_front();
            if (event.type == PointerEventType::Click) {
                controller.handle(inputMapper.mapClick(
                    event.position, state.view(), layout));
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

        const auto frame = renderer.render(
            state.snapshot(), layout, pointer.position);
        cv::imshow(WindowName, frame);
        const int key = cv::waitKeyEx(16);
        if (key == 27) {
            if (state.view().modal != LobbyModal::None) {
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
    cv::destroyWindow(WindowName);
}

}  // namespace ctd::lobby
