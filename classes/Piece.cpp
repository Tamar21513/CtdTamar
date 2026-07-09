#include <string>
using namespace std;

class Piece {
public:
    string color;           
    string type;            
    long long cooldownMs;   
    long long lastMovedAt; 
    
    Piece(string color, string type, long long cooldownMs) {
        this->color = color;
        this->type = type;
        this->cooldownMs = cooldownMs;
        this->lastMovedAt = 0;
    }

    string token() const {
        return color + type;
    }

    bool canMove(long long currentTimeMs) const {
        return currentTimeMs - lastMovedAt >= cooldownMs;
    }

    // מעדכן שהכלי זז עכשיו
    void markMoved(long long currentTimeMs) {
        lastMovedAt = currentTimeMs;
    }
};