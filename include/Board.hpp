#ifndef BOARD_HPP
#define BOARD_HPP

#include <vector>
#include <memory>

#include "Piece.hpp"
#include "Position.hpp"

using namespace std;

class Board {
private:
    int height;
    int width;
    vector<vector<shared_ptr<Piece>>> cells;

public:
    Board();
    Board(int height, int width);

    int getHeight() const;
    int getWidth() const;
    bool isInside(const Position& position) const;

    shared_ptr<Piece> getPieceAt(const Position& position) const;

    bool isEmpty(const Position& position) const;
    bool placePiece(const Position& position, shared_ptr<Piece> piece);
    void removePiece(const Position& position);
    void movePiece(const Position& source, const Position& destination);
};

#endif