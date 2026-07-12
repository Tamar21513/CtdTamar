#ifndef PIECE_HPP
#define PIECE_HPP

#include <string>
using namespace std;

class Piece {
private:
    string color;
    string type;
    long long cooldownMs;
    long long lastMovedAt;

public:
    Piece(string color, string type, long long cooldownMs);

    string getColor() const;
    string getType() const;
    long long getCooldownMs() const;
    long long getLastMovedAt() const;

    string token() const;
    bool isSameColor(const Piece& other) const;
    bool canMove(long long currentTimeMs) const;
    void markMoved(long long currentTimeMs);
};

#endif
