#pragma once

#include <fstream>
#include <regex>
#include <sstream>
#include <string>

struct PieceStateConfig {
    long long moveTimePerCellMs = 1000;
    long long jumpDurationMs = 1000;
    long long shortCooldownMs = 3000;
};

class PieceConfigReader {
private:
    static long long readLong(const std::string& text, const std::string& key, long long fallback) {
        std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*([0-9]+)");
        std::smatch match;
        return std::regex_search(text, match, pattern) ? std::stoll(match[1].str()) : fallback;
    }

public:
    static PieceStateConfig load(const std::string& pieceCode, const std::string& state) {
        PieceStateConfig config;
        std::ifstream input("assets\\pieces\\" + pieceCode + "\\states\\" + state + "\\config.json");
        if (!input) return config;
        std::ostringstream buffer;
        buffer << input.rdbuf();
        const std::string text = buffer.str();
        config.moveTimePerCellMs = readLong(text, "move_time_per_cell_ms", config.moveTimePerCellMs);
        config.jumpDurationMs = readLong(text, "jump_duration_ms", config.jumpDurationMs);
        config.shortCooldownMs = readLong(text, "short_cooldown_ms", config.shortCooldownMs);
        return config;
    }
};
