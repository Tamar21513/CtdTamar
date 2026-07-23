#ifndef PIECE_HPP
#define PIECE_HPP

#include <string>

using namespace std;

enum class PieceColor {
    White,
    Black
};

enum class PieceKind {
    King,
    Queen,
    Rook,
    Bishop,
    Knight,
    Pawn
};

enum class PieceState {
    Idle,
    Moving,
    Airborne,
    Captured
};

class Piece {
private:
    int id;

    PieceColor color;
    PieceKind kind;
    PieceState state;

    long long cooldownStartedAtMs;
    long long cooldownUntilMs;
    long long totalCooldownMs;
    bool hasMoved;

public:
    Piece(
        int id,
        PieceColor color,
        PieceKind kind
    );

    int getId() const;

    PieceColor getColor() const;
    PieceKind getKind() const;
    PieceState getState() const;

    void setState(PieceState newState);
    void setKind(PieceKind newKind);
    bool getHasMoved() const;
    void markAsMoved();

    void startCooldown(
        long long currentTimeMs,
        long long durationMs
    );

    bool isOnCooldown(
        long long currentTimeMs
    ) const;

    long long getRemainingCooldownMs(
        long long currentTimeMs
    ) const;

    long long getTotalCooldownMs() const;

    double getCooldownRatio(
        long long currentTimeMs
    ) const;

    string token() const;

    static PieceColor colorFromChar(
        char colorChar
    );

    static PieceKind kindFromChar(
        char kindChar
    );

    static char colorToChar(
        PieceColor color
    );

    static char kindToChar(
        PieceKind kind
    );
};

#endif
