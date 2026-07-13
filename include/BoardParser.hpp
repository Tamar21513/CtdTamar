#ifndef BOARD_PARSER_HPP
#define BOARD_PARSER_HPP

#include <string>
#include <vector>

#include "Board.hpp"

using namespace std;

class BoardParser {
private:
    static bool isAllowedToken(const string& token);
    static vector<string> splitLineToTokens(const string& line);

public:
    static Board parse(const vector<string>& boardLines);
};

#endif