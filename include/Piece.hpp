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
    Captured
};

class Piece {
private:
    int id;
    PieceColor color;
    PieceKind kind;
    PieceState state;

public:
    Piece(int id, PieceColor color, PieceKind kind);

    int getId() const;
    PieceColor getColor() const;
    PieceKind getKind() const;
    PieceState getState() const;

    void setState(PieceState newState);

    string token() const;

    static PieceColor colorFromChar(char colorChar);
    static PieceKind kindFromChar(char kindChar);

    static char colorToChar(PieceColor color);
    static char kindToChar(PieceKind kind);
};

#endif