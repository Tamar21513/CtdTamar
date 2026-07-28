#include "Graphics/AnimationLibrary.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    const std::filesystem::path executablePath =
        std::filesystem::absolute(argv[0]);
    std::filesystem::current_path(
        executablePath.parent_path()
    );

    const std::vector<std::string> pieces = {
        "wK", "wQ", "wR", "wB", "wN", "wP",
        "bK", "bQ", "bR", "bB", "bN", "bP"
    };
    const std::vector<VisualState> states = {
        VisualState::Idle,
        VisualState::Move,
        VisualState::Jump,
        VisualState::ShortRest,
        VisualState::LongRest
    };

    AnimationLibrary library;
    for (const std::string& piece : pieces) {
        for (VisualState state : states) {
            if (
                library.getFramePaths(
                    piece,
                    state
                ).empty()
            ) {
                throw std::runtime_error(
                    "No frames found for piece " +
                    piece
                );
            }
        }
    }

    if (
        !std::filesystem::exists(
            "assets/Board/board.png"
        )
    ) {
        throw std::runtime_error(
            "Board asset was not found"
        );
    }

    std::cout
        << "All client animation and board assets found\n";
    return 0;
}
