#include <string>
using namespace std;

class Piece {
private:
    string color;           
    string type;            
    long long cooldownMs;   
    long long lastMovedAt; 
    
public:
    Piece(string color, string type, long long cooldownMs) {
        this->color = color;
        this->type = type;
        this->cooldownMs = cooldownMs;
        this->lastMovedAt = 0;
    }

    string getColor() const {
        return color;
    }

    string getType() const {
        return type;
    }

    long long getCooldownMs() const {
        return cooldownMs;
    }

    long long getLastMovedAt() const {
        return lastMovedAt;
    }

    string token() const {
        return color + type;
    }

    bool isSameColor(const Piece& other) const {
        return color == other.color;
    }

    bool canMove(long long currentTimeMs) const {
        return currentTimeMs - lastMovedAt >= cooldownMs;
    }

    // מעדכן שהכלי זז עכשיו
    void markMoved(long long currentTimeMs) {
        lastMovedAt = currentTimeMs;
    }
};