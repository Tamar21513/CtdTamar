#include "../../include/IO/BoardParser.hpp"
#include "../../include/Core/Config.hpp"

#include <sstream>
#include <iostream>

// Implements isAllowedToken.
bool BoardParser::isAllowedToken(const string& token) {
    if (token == Config::EMPTY_CELL) {
        return true;
    }

    if (token.size() != 2) {
        return false;
    }

    char color = token[0];
    char kind = token[1];
    bool validColor = color == 'w' || color == 'b';
    bool validKind = kind == 'K' || kind == 'Q' || kind == 'R' || kind == 'B' || kind == 'N' || kind == 'P';

    return validColor && validKind;
}

// Implements splitLineToTokens.
vector<string> BoardParser::splitLineToTokens(const string& line) {
    vector<string> tokens;
    string token;
    stringstream ss(line);

    while (ss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

// Implements parse.
Board BoardParser::parse(const vector<string>& boardLines) {
    vector<vector<string>> tokensByRow;

    for (size_t i = 0; i < boardLines.size(); i++) {
        vector<string> rowTokens = splitLineToTokens(boardLines[i]);

        if (!rowTokens.empty()) {
            tokensByRow.push_back(rowTokens);
        }
    }

    if (tokensByRow.empty()) {
        return Board(0, 0);
    }

    size_t expectedWidth = tokensByRow[0].size();

    for (size_t row = 0; row < tokensByRow.size(); row++) {
        if (tokensByRow[row].size() != expectedWidth) {
            cout << Config::ERROR_ROW_WIDTH_MISMATCH << endl;
            return Board(0, 0);
        }

        for (size_t col = 0; col < tokensByRow[row].size(); col++) {
            if (!isAllowedToken(tokensByRow[row][col])) {
                cout << Config::ERROR_UNKNOWN_TOKEN << endl;
                return Board(0, 0);
            }
        }
    }

    int height = (int)tokensByRow.size();
    int width = (int)expectedWidth;

    Board board(height, width);

    int nextPieceId = 1;

    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            string token = tokensByRow[row][col];

            if (token == Config::EMPTY_CELL) {
                continue;
            }

            PieceColor color = Piece::colorFromChar(token[0]);
            PieceKind kind = Piece::kindFromChar(token[1]);

            shared_ptr<Piece> piece = make_shared<Piece>(nextPieceId, color, kind);
            nextPieceId++;

            board.placePiece(Position(row, col), piece);
        }
    }

    return board;
}
