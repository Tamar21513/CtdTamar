// Windows socket definitions must be loaded
// before headers containing "using namespace std".
#include "Network/TcpConnection.hpp"

#include "App/VisualApp.hpp"

#include "Client/ClientGameState.hpp"
#include "Client/GameClient.hpp"
#include "Client/GameStartState.hpp"
#include "Client/Username.hpp"
#include "Client/NetworkController.hpp"
#include "Core/MoveHistory.hpp"
#include "Core/Piece.hpp"
#include "Graphics/AnimationLibrary.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/VisualSnapshotBuilder.hpp"
#include "Network/NetworkDefaults.hpp"

#include <opencv2/opencv.hpp>

#include <chrono>
#include <deque>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

const std::string WINDOW_NAME =
    "Kung Fu Chess";

struct MouseClick {
    int x;
    int y;
};

struct MouseState {
    int x = 0;
    int y = 0;

    std::deque<MouseClick>
        pendingClicks;
};

struct VisualLayout {
    int boardSize;

    int boardStartX;
    int boardStartY;

    int cellSizeX;
    int cellSizeY;

    int spriteSize;
};

class SocketEnvironment {
public:
    SocketEnvironment() {
        WSADATA data{};

        const int result =
            WSAStartup(
                MAKEWORD(2, 2),
                &data
            );

        if (result != 0) {
            throw std::runtime_error(
                "WSAStartup failed"
            );
        }
    }

    ~SocketEnvironment() {
        WSACleanup();
    }

    SocketEnvironment(
        const SocketEnvironment&
    ) = delete;

    SocketEnvironment& operator=(
        const SocketEnvironment&
    ) = delete;
};

MouseState mouseState;

const cv::Rect USERNAME_FIELD(
    250,
    265,
    600,
    64
);

const cv::Rect CONNECT_BUTTON(
    425,
    370,
    250,
    64
);

void onMouse(
    int event,
    int x,
    int y,
    int,
    void*
) {
    mouseState.x = x;
    mouseState.y = y;

    if (event == cv::EVENT_LBUTTONDOWN) {
        mouseState.pendingClicks.push_back({
            x,
            y
        });
    }
}

VisualLayout createVisualLayout() {
    const int boardSize = 600;

    return {
        boardSize,

        Renderer::SIDE_PANEL_WIDTH +
            static_cast<int>(
                boardSize * 0.11
            ),

        Renderer::SCORE_PANEL_HEIGHT +
            static_cast<int>(
                boardSize * 0.11
            ),

        static_cast<int>(
            boardSize * 0.098
        ),

        static_cast<int>(
            boardSize * 0.097
        ),

        static_cast<int>(
            boardSize * 0.105
        )
    };
}

long long currentEpochMs() {
    return std::chrono::duration_cast<
        std::chrono::milliseconds
    >(
        std::chrono::system_clock::now()
            .time_since_epoch()
    ).count();
}

long long currentSteadyMs() {
    return std::chrono::duration_cast<
        std::chrono::milliseconds
    >(
        std::chrono::steady_clock::now()
            .time_since_epoch()
    ).count();
}

void drawUsernameScreen(
    const std::string& username,
    const std::string& message
) {
    cv::Mat screen(
        740,
        1100,
        CV_8UC3,
        cv::Scalar(238, 241, 245)
    );

    cv::putText(
        screen,
        "Kung Fu Chess",
        cv::Point(365, 145),
        cv::FONT_HERSHEY_DUPLEX,
        1.5,
        cv::Scalar(35, 35, 35),
        3,
        cv::LINE_AA
    );
    cv::putText(
        screen,
        "Display name",
        cv::Point(
            USERNAME_FIELD.x,
            USERNAME_FIELD.y - 18
        ),
        cv::FONT_HERSHEY_SIMPLEX,
        0.75,
        cv::Scalar(60, 60, 60),
        2,
        cv::LINE_AA
    );
    cv::rectangle(
        screen,
        USERNAME_FIELD,
        cv::Scalar(255, 255, 255),
        cv::FILLED
    );
    cv::rectangle(
        screen,
        USERNAME_FIELD,
        cv::Scalar(75, 125, 210),
        2
    );
    cv::putText(
        screen,
        username,
        cv::Point(
            USERNAME_FIELD.x + 16,
            USERNAME_FIELD.y + 42
        ),
        cv::FONT_HERSHEY_SIMPLEX,
        0.8,
        cv::Scalar(35, 35, 35),
        2,
        cv::LINE_AA
    );
    cv::rectangle(
        screen,
        CONNECT_BUTTON,
        cv::Scalar(75, 125, 210),
        cv::FILLED
    );
    cv::putText(
        screen,
        "Connect",
        cv::Point(
            CONNECT_BUTTON.x + 70,
            CONNECT_BUTTON.y + 42
        ),
        cv::FONT_HERSHEY_SIMPLEX,
        0.9,
        cv::Scalar(255, 255, 255),
        2,
        cv::LINE_AA
    );

    if (!message.empty()) {
        cv::putText(
            screen,
            message,
            cv::Point(250, 485),
            cv::FONT_HERSHEY_SIMPLEX,
            0.65,
            cv::Scalar(40, 40, 190),
            2,
            cv::LINE_AA
        );
    }

    cv::imshow(WINDOW_NAME, screen);
}

bool consumeConnectClick() {
    bool clicked = false;
    while (!mouseState.pendingClicks.empty()) {
        const MouseClick click =
            mouseState.pendingClicks.front();
        mouseState.pendingClicks.pop_front();
        if (
            CONNECT_BUTTON.contains(
                cv::Point(click.x, click.y)
            )
        ) {
            clicked = true;
        }
    }
    return clicked;
}

std::string requestUsernameInWindow() {
    std::string username;
    std::string validationMessage;

    while (true) {
        drawUsernameScreen(
            username,
            validationMessage
        );

        const int key = cv::waitKeyEx(16);
        const bool submit =
            key == 13 ||
            consumeConnectClick();

        if (
            key == 27 ||
            cv::getWindowProperty(
                WINDOW_NAME,
                cv::WND_PROP_VISIBLE
            ) < 1
        ) {
            return "";
        }

        if (key == 8 && !username.empty()) {
            username.pop_back();
            validationMessage.clear();
        }
        else if (
            key >= 32 &&
            key <= 255 &&
            username.size() <
                Username::MAX_LENGTH
        ) {
            username.push_back(
                static_cast<char>(key)
            );
            validationMessage.clear();
        }

        if (submit) {
            username = Username::trim(username);
            if (Username::isValid(username)) {
                mouseState.pendingClicks.clear();
                return username;
            }
            validationMessage =
                "Please enter a display name.";
        }
    }
}

int findVisualPieceByCell(
    const std::vector<VisualPiece>& pieces,
    const Position& cell
) {
    for (
        std::size_t index = 0;
        index < pieces.size();
        ++index
    ) {
        if (
            pieces[index].row ==
                cell.getRow() &&
            pieces[index].col ==
                cell.getCol()
        ) {
            return static_cast<int>(
                index
            );
        }
    }

    return -1;
}

VisualPiece createSelectionGhost(
    const VisualPiece& selectedPiece,
    int mouseX,
    int mouseY
) {
    VisualPiece ghost = selectedPiece;

    ghost.pixelX =
        static_cast<double>(mouseX);

    ghost.pixelY =
        static_cast<double>(mouseY);

    ghost.opacity = 0.55;
    ghost.alignVisibleCenter = true;

    return ghost;
}

void printControllerResult(
    const ControllerResult& result
) {
    std::cout
        << "NetworkController: "
        << result.reason
        << '\n';
}

void processPendingClicks(
    NetworkController& controller,
    bool gameOver
) {
    if (gameOver) {
        mouseState.pendingClicks.clear();
        return;
    }

    while (
        !mouseState.pendingClicks.empty()
    ) {
        const MouseClick click =
            mouseState.pendingClicks.front();

        mouseState.pendingClicks.pop_front();

        printControllerResult(
            controller.click(
                click.x,
                click.y
            )
        );
    }
}

void appendSelectionGhost(
    std::vector<VisualPiece>& piecesToRender,
    const std::vector<VisualPiece>& visualPieces,
    const NetworkController& controller,
    bool gameOver
) {
    if (gameOver) {
        return;
    }

    const std::optional<Position>
        selectedCell =
            controller.getSelectedCell();

    if (!selectedCell.has_value()) {
        return;
    }

    const int selectedIndex =
        findVisualPieceByCell(
            visualPieces,
            selectedCell.value()
        );

    if (selectedIndex < 0) {
        return;
    }

    piecesToRender[selectedIndex].opacity =
        0.25;

    piecesToRender.push_back(
        createSelectionGhost(
            visualPieces[selectedIndex],
            mouseState.x,
            mouseState.y
        )
    );
}

std::vector<MoveHistoryEntry>
convertMoveHistory(
    const std::vector<MoveHistorySnapshot>&
        snapshots
) {
    std::vector<MoveHistoryEntry> result;

    result.reserve(snapshots.size());

    for (
        const MoveHistorySnapshot& snapshot :
        snapshots
    ) {
        MoveHistoryEntry entry;

        entry.completedAtMs =
            snapshot.completedAtMs;

        entry.color =
            snapshot.color == "b"
                ? PieceColor::Black
                : PieceColor::White;

        entry.pieceKind =
            snapshot.pieceKind.empty()
                ? PieceKind::Pawn
                : Piece::kindFromChar(
                    snapshot.pieceKind[0]
                );

        entry.source =
            snapshot.source;

        entry.destination =
            snapshot.destination;

        entry.wasCapture =
            snapshot.wasCapture;

        entry.wasPromotion =
            snapshot.wasPromotion;

        entry.wasJump =
            snapshot.wasJump;

        entry.notation =
            snapshot.notation;

        result.push_back(entry);
    }

    return result;
}

void waitForInitialSnapshot(
    GameClient& client,
    ClientGameState& gameState,
    Renderer& renderer
) {
    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(5);

    while (
        std::chrono::steady_clock::now() <
        deadline
    ) {
        Message update;

        while (
            client.tryReceiveUpdate(update)
        ) {
            gameState.applyMessage(update);
        }

        if (client.isGameFull()) {
            return;
        }

        if (client.hasPlayerAssignment()) {
            renderer.render(
                {},
                false,
                0,
                0,
                {},
                {},
                client.getAssignedColor() ==
                        PieceColor::White
                    ? "white"
                    : "black",
                client.getWhiteUsername(),
                client.getBlackUsername(),
                "Waiting for another player...",
                "",
                RenderOverlayMode::Waiting
            );
            cv::waitKey(16);
        }

        if (gameState.hasSnapshot()) {
            return;
        }

        if (!client.isConnected()) {
            const std::string error =
                client.getConnectionError();

            throw std::runtime_error(
                error.empty()
                    ? "Server disconnected"
                    : error
            );
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10)
        );
    }

    throw std::runtime_error(
        "Timed out while waiting for "
        "the initial game snapshot"
    );
}

// Returns the color token used by the renderer.
std::string buildLocalPlayerColor(
    const GameClient& client
) {
    if (!client.hasPlayerAssignment()) {
        return "";
    }

    return
        client.getAssignedColor() ==
                PieceColor::White
            ? "white"
            : "black";
}

// Builds the server-authoritative status lines.
std::pair<std::string, std::string>
buildConnectionStatus(
    const GameClient& client
) {
    if (client.isGameClosed()) {
        return {
            "Game closed",
            "Reconnection time expired"
        };
    }

    if (client.isTryingToReconnect()) {
        std::string secondLine =
            "Trying to reconnect...";

        if (
            client.getReconnectSecondsRemaining() >
            0
        ) {
            secondLine +=
                "  Reconnection time left: " +
                std::to_string(
                    client
                        .getReconnectSecondsRemaining()
                );
        }

        return {
            "Connection lost",
            secondLine
        };
    }

    if (client.isReconnecting()) {
        return {
            "Opponent disconnected",
            "Waiting for reconnection: " +
                std::to_string(
                    client
                        .getReconnectSecondsRemaining()
                )
        };
    }

    if (!client.hasGameStarted()) {
        return {
            "Waiting for opponent...",
            ""
        };
    }

    return {"", ""};
}

void showGameFullMessage() {
    cv::Mat screen(
        240,
        640,
        CV_8UC3,
        cv::Scalar(245, 245, 245)
    );

    const std::string text =
        "Game is full";
    int baseline = 0;
    const cv::Size textSize =
        cv::getTextSize(
            text,
            cv::FONT_HERSHEY_DUPLEX,
            1.3,
            2,
            &baseline
        );

    cv::putText(
        screen,
        text,
        cv::Point(
            (screen.cols - textSize.width) / 2,
            (screen.rows + textSize.height) / 2
        ),
        cv::FONT_HERSHEY_DUPLEX,
        1.3,
        cv::Scalar(35, 35, 35),
        2,
        cv::LINE_AA
    );

    cv::imshow(WINDOW_NAME, screen);
    cv::waitKey(0);
}

} // namespace

// Runs the visual client with the default endpoint.
void VisualApp::run() {
    run(
        NetworkDefaults::HOST,
        NetworkDefaults::PORT
    );
}

// Runs the visual client against one server endpoint.
void VisualApp::run(
    const std::string& serverHost,
    unsigned short serverPort
) {
    std::cout
        << "VisualApp started\n";

    cv::namedWindow(
        WINDOW_NAME,
        cv::WINDOW_AUTOSIZE
    );
    cv::setMouseCallback(
        WINDOW_NAME,
        onMouse
    );

    const std::string username =
        requestUsernameInWindow();
    if (username.empty()) {
        cv::destroyAllWindows();
        return;
    }

    SocketEnvironment sockets;

    GameClient client;
    ClientUiPhaseMachine phaseMachine;
    phaseMachine.markConnecting();
    drawUsernameScreen(
        username,
        "Connecting..."
    );
    cv::waitKey(1);

    try {
        client.connectTo(
            serverHost,
            serverPort,
            username
        );
    }
    catch (...) {
        throw std::runtime_error(
            "Unable to connect to " +
            serverHost +
            ":" +
            std::to_string(serverPort)
        );
    }

    std::cout
        << "Connected to game server\n";

    ClientGameState gameState;

    const VisualLayout layout =
        createVisualLayout();

    Renderer renderer(
        R"(assets\Board\board.png)",
        layout.boardSize,
        WINDOW_NAME
    );

    waitForInitialSnapshot(
        client,
        gameState,
        renderer
    );

    if (client.isGameFull()) {
        showGameFullMessage();
        client.disconnect();
        cv::destroyAllWindows();
        return;
    }

    std::cout
        << "Initial snapshot received: "
        << gameState.copySnapshot().pieces.size()
        << " pieces\n";

    std::cout
        << "Reconnect token: "
        << client.getReconnectToken()
        << '\n';

    AnimationLibrary animationLibrary;

    NetworkController controller(
        client,
        gameState,
        layout.boardStartX,
        layout.boardStartY,
        layout.cellSizeX,
        layout.cellSizeY
    );

    VisualSnapshotBuilder snapshotBuilder(
        layout.boardStartX,
        layout.boardStartY,
        layout.cellSizeX,
        layout.cellSizeY,
        layout.spriteSize,
        animationLibrary
    );

    auto previousTime =
        std::chrono::steady_clock::now();

    while (true) {
        const auto currentTime =
            std::chrono::steady_clock::now();

        const long long deltaMs =
            std::chrono::duration_cast<
                std::chrono::milliseconds
            >(
                currentTime - previousTime
            ).count();

        previousTime = currentTime;

        controller.applyPendingUpdates();

        if (
            !client.isConnected() &&
            !client.isTryingToReconnect() &&
            !client.isGameClosed()
        ) {
            std::cerr
                << "Connection lost: "
                << client.getConnectionError()
                << '\n';

            break;
        }

        // Apply updates that arrived while a move
        // request was being processed.
        controller.applyPendingUpdates();

        const GameStateSnapshot snapshot =
            gameState.copySnapshot();

        const GameStartDisplay startDisplay =
            phaseMachine.observe(
                snapshot,
                currentEpochMs(),
                currentSteadyMs()
            );
        ClientUiPhase framePhase =
            startDisplay.phase;
        if (
            client.isReconnecting() ||
            client.isTryingToReconnect()
        ) {
            phaseMachine.markReconnecting();
            framePhase = ClientUiPhase::Reconnecting;
        }

        const bool gameplayEnabled =
            framePhase ==
                ClientUiPhase::Playing ||
            framePhase ==
                ClientUiPhase::Reconnecting;

        processPendingClicks(
            controller,
            snapshot.gameOver ||
                client.isGameClosed() ||
                !gameplayEnabled
        );

        const std::vector<VisualPiece>
            visualPieces =
                snapshotBuilder.build(
                    snapshot,
                    gameplayEnabled
                        ? deltaMs
                        : 0
                );

        std::vector<VisualPiece>
            piecesToRender =
                framePhase !=
                    ClientUiPhase::WaitingForOpponent
                    ? visualPieces
                    : std::vector<VisualPiece>{};

        appendSelectionGhost(
            piecesToRender,
            visualPieces,
            controller,
            snapshot.gameOver ||
                !gameplayEnabled
        );

        const std::vector<MoveHistoryEntry>
            blackMoves =
                convertMoveHistory(
                    snapshot.blackMoveHistory
                );

        const std::vector<MoveHistoryEntry>
            whiteMoves =
                convertMoveHistory(
                    snapshot.whiteMoveHistory
                );

        auto connectionStatus =
            buildConnectionStatus(client);
        RenderOverlayMode overlayMode =
            connectionStatus.first.empty()
                ? RenderOverlayMode::None
                : RenderOverlayMode::Status;

        if (
            !client.isReconnecting() &&
            !client.isTryingToReconnect() &&
            !client.isGameClosed() &&
            framePhase !=
                ClientUiPhase::Playing &&
            framePhase !=
                ClientUiPhase::Reconnecting &&
            framePhase !=
                ClientUiPhase::GameOver
        ) {
            connectionStatus = {
                startDisplay.text,
                ""
            };
            overlayMode =
                framePhase ==
                        ClientUiPhase::Countdown
                    ? RenderOverlayMode::Countdown
                    : RenderOverlayMode::Waiting;
        }

        renderer.render(
            piecesToRender,
            snapshot.gameOver,
            snapshot.blackScore,
            snapshot.whiteScore,
            blackMoves,
            whiteMoves,
            buildLocalPlayerColor(client),
            snapshot.whiteUsername,
            snapshot.blackUsername,
            connectionStatus.first,
            connectionStatus.second,
            overlayMode
        );

        cv::setWindowTitle(
            WINDOW_NAME,
            WINDOW_NAME
        );

        const int key = cv::waitKey(16);

        if (
            key == 27 ||
            cv::getWindowProperty(
                WINDOW_NAME,
                cv::WND_PROP_VISIBLE
            ) < 1
        ) {
            break;
        }
    }

    client.disconnect();
    cv::destroyAllWindows();
}
