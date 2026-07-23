// Windows socket definitions must be loaded
// before headers containing "using namespace std".
#include "../../include/Network/TcpConnection.hpp"

#include "../../include/App/VisualApp.hpp"

#include "../../include/Client/ClientGameState.hpp"
#include "../../include/Client/GameClient.hpp"
#include "../../include/Client/NetworkController.hpp"
#include "../../include/Core/MoveHistory.hpp"
#include "../../include/Core/Piece.hpp"
#include "../../include/Graphics/AnimationLibrary.hpp"
#include "../../include/Graphics/Renderer.hpp"
#include "../../include/Graphics/VisualSnapshotBuilder.hpp"

#include <opencv2/opencv.hpp>

#include <chrono>
#include <deque>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
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
    ClientGameState& gameState
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

} // namespace

void VisualApp::run() {
    std::cout
        << "VisualApp started\n";

    SocketEnvironment sockets;

    GameClient client;

    client.connectTo(
        "127.0.0.1",
        5050
    );

    std::cout
        << "Connected to game server\n";

    ClientGameState gameState;

    waitForInitialSnapshot(
        client,
        gameState
    );

    std::cout
        << "Initial snapshot received: "
        << gameState.getSnapshot().pieces.size()
        << " pieces\n";

    const VisualLayout layout =
        createVisualLayout();

    cv::namedWindow(
        WINDOW_NAME,
        cv::WINDOW_AUTOSIZE
    );

    cv::setMouseCallback(
        WINDOW_NAME,
        onMouse
    );

    Renderer renderer(
        R"(assets\Board\board.png)",
        layout.boardSize,
        WINDOW_NAME
    );

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

        if (!client.isConnected()) {
            std::cerr
                << "Connection lost: "
                << client.getConnectionError()
                << '\n';

            break;
        }

        processPendingClicks(
            controller,
            gameState.isGameOver()
        );

        // Apply updates that arrived while a move
        // request was being processed.
        controller.applyPendingUpdates();

        const GameStateSnapshot& snapshot =
            gameState.getSnapshot();

        const std::vector<VisualPiece>
            visualPieces =
                snapshotBuilder.build(
                    snapshot,
                    deltaMs
                );

        std::vector<VisualPiece>
            piecesToRender =
                visualPieces;

        appendSelectionGhost(
            piecesToRender,
            visualPieces,
            controller,
            snapshot.gameOver
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

        renderer.render(
            piecesToRender,
            snapshot.gameOver,
            snapshot.blackScore,
            snapshot.whiteScore,
            blackMoves,
            whiteMoves
        );

        if (cv::waitKey(16) == 27) {
            break;
        }
    }

    client.disconnect();
    cv::destroyAllWindows();
}