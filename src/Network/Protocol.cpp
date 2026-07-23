#include "../../include/Network/Protocol.hpp"

#include <cctype>
#include <climits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

void skipWhitespace(
    const std::string& text,
    std::size_t& position
) {
    while (
        position < text.size() &&
        std::isspace(
            static_cast<unsigned char>(text[position])
        )
    ) {
        ++position;
    }
}

std::string escapeJson(const std::string& value) {
    std::string result;

    for (unsigned char character : value) {
        switch (character) {
            case '\\':
                result += "\\\\";
                break;

            case '"':
                result += "\\\"";
                break;

            case '\n':
                result += "\\n";
                break;

            case '\r':
                result += "\\r";
                break;

            case '\t':
                result += "\\t";
                break;

            default:
                if (character < 0x20) {
                    throw std::invalid_argument(
                        "Protocol: unsupported control character"
                    );
                }

                result += static_cast<char>(character);
        }
    }

    return result;
}

std::size_t findStringEnd(
    const std::string& text,
    std::size_t start
) {
    bool escaped = false;

    for (
        std::size_t position = start + 1;
        position < text.size();
        ++position
    ) {
        const char character = text[position];

        if (escaped) {
            escaped = false;
        }
        else if (character == '\\') {
            escaped = true;
        }
        else if (character == '"') {
            return position;
        }
    }

    throw std::invalid_argument(
        "Protocol: unterminated string"
    );
}

std::size_t findValue(
    const std::string& json,
    const std::string& key
) {
    if (
        json.size() < 2 ||
        json.front() != '{' ||
        json.back() != '}'
    ) {
        throw std::invalid_argument(
            "Protocol: expected a JSON object"
        );
    }

    int objectDepth = 1;
    int arrayDepth = 0;

    for (
        std::size_t position = 1;
        position + 1 < json.size();
        ++position
    ) {
        const char character = json[position];

        if (character == '"') {
            const std::size_t end =
                findStringEnd(json, position);

            if (objectDepth == 1 && arrayDepth == 0) {
                std::size_t after = end + 1;
                skipWhitespace(json, after);

                if (
                    after < json.size() &&
                    json[after] == ':' &&
                    json.substr(
                        position + 1,
                        end - position - 1
                    ) == key
                ) {
                    ++after;
                    skipWhitespace(json, after);
                    return after;
                }
            }

            position = end;
        }
        else if (character == '{') {
            ++objectDepth;
        }
        else if (character == '}') {
            --objectDepth;
        }
        else if (character == '[') {
            ++arrayDepth;
        }
        else if (character == ']') {
            --arrayDepth;
        }
    }

    throw std::invalid_argument(
        "Protocol: missing field '" + key + "'"
    );
}

std::size_t findMatchingDelimiter(
    const std::string& text,
    std::size_t start,
    char opening,
    char closing
) {
    if (
        start >= text.size() ||
        text[start] != opening
    ) {
        throw std::invalid_argument(
            "Protocol: invalid JSON delimiter"
        );
    }

    int depth = 0;

    for (
        std::size_t position = start;
        position < text.size();
        ++position
    ) {
        const char character = text[position];

        if (character == '"') {
            position = findStringEnd(text, position);
        }
        else if (character == opening) {
            ++depth;
        }
        else if (character == closing) {
            --depth;

            if (depth == 0) {
                return position;
            }
        }
    }

    throw std::invalid_argument(
        "Protocol: unmatched JSON delimiter"
    );
}

std::string readObject(
    const std::string& json,
    const std::string& key
) {
    const std::size_t start = findValue(json, key);

    if (
        start >= json.size() ||
        json[start] != '{'
    ) {
        throw std::invalid_argument(
            "Protocol: field '" + key +
            "' must be an object"
        );
    }

    const std::size_t end = findMatchingDelimiter(
        json,
        start,
        '{',
        '}'
    );

    return json.substr(start, end - start + 1);
}

std::string readArray(
    const std::string& json,
    const std::string& key
) {
    const std::size_t start = findValue(json, key);

    if (
        start >= json.size() ||
        json[start] != '['
    ) {
        throw std::invalid_argument(
            "Protocol: field '" + key +
            "' must be an array"
        );
    }

    const std::size_t end = findMatchingDelimiter(
        json,
        start,
        '[',
        ']'
    );

    return json.substr(start, end - start + 1);
}

std::vector<std::string> readObjectArray(
    const std::string& json,
    const std::string& key
) {
    const std::string array = readArray(json, key);
    std::vector<std::string> objects;

    std::size_t position = 1;
    skipWhitespace(array, position);

    if (position == array.size() - 1) {
        return objects;
    }

    while (position < array.size() - 1) {
        if (array[position] != '{') {
            throw std::invalid_argument(
                "Protocol: array '" + key +
                "' must contain objects"
            );
        }

        const std::size_t end = findMatchingDelimiter(
            array,
            position,
            '{',
            '}'
        );

        objects.push_back(
            array.substr(position, end - position + 1)
        );

        position = end + 1;
        skipWhitespace(array, position);

        if (position == array.size() - 1) {
            break;
        }

        if (array[position] != ',') {
            throw std::invalid_argument(
                "Protocol: invalid array '" + key + "'"
            );
        }

        ++position;
        skipWhitespace(array, position);

        if (position == array.size() - 1) {
            throw std::invalid_argument(
                "Protocol: trailing comma in array '" +
                key + "'"
            );
        }
    }

    return objects;
}

std::string readString(
    const std::string& json,
    const std::string& key
) {
    std::size_t position = findValue(json, key);

    if (
        position >= json.size() ||
        json[position] != '"'
    ) {
        throw std::invalid_argument(
            "Protocol: field '" + key +
            "' must be a string"
        );
    }

    std::string result;
    bool escaped = false;

    for (++position; position < json.size(); ++position) {
        const char character = json[position];

        if (escaped) {
            switch (character) {
                case '\\':
                    result += '\\';
                    break;

                case '"':
                    result += '"';
                    break;

                case 'n':
                    result += '\n';
                    break;

                case 'r':
                    result += '\r';
                    break;

                case 't':
                    result += '\t';
                    break;

                default:
                    throw std::invalid_argument(
                        "Protocol: invalid string escape"
                    );
            }

            escaped = false;
        }
        else if (character == '\\') {
            escaped = true;
        }
        else if (character == '"') {
            return result;
        }
        else {
            result += character;
        }
    }

    throw std::invalid_argument(
        "Protocol: unterminated string"
    );
}

long long readSigned(
    const std::string& json,
    const std::string& key
) {
    const std::size_t position = findValue(json, key);
    std::size_t charactersUsed = 0;
    long long value = 0;

    try {
        value = std::stoll(
            json.substr(position),
            &charactersUsed
        );
    }
    catch (...) {
        throw std::invalid_argument(
            "Protocol: field '" + key +
            "' must be an integer"
        );
    }

    std::size_t after = position + charactersUsed;
    skipWhitespace(json, after);

    if (
        charactersUsed == 0 ||
        after >= json.size() ||
        (
            json[after] != ',' &&
            json[after] != '}' &&
            json[after] != ']'
        )
    ) {
        throw std::invalid_argument(
            "Protocol: field '" + key +
            "' must be an integer"
        );
    }

    return value;
}

int readInt(
    const std::string& json,
    const std::string& key
) {
    const long long value = readSigned(json, key);

    if (value < INT_MIN || value > INT_MAX) {
        throw std::invalid_argument(
            "Protocol: field '" + key +
            "' is outside the integer range"
        );
    }

    return static_cast<int>(value);
}

unsigned long long readUnsigned(
    const std::string& json,
    const std::string& key
) {
    const long long value = readSigned(json, key);

    if (value < 0) {
        throw std::invalid_argument(
            "Protocol: field '" + key +
            "' cannot be negative"
        );
    }

    return static_cast<unsigned long long>(value);
}

bool readBool(
    const std::string& json,
    const std::string& key
) {
    const std::size_t position = findValue(json, key);

    if (json.compare(position, 4, "true") == 0) {
        return true;
    }

    if (json.compare(position, 5, "false") == 0) {
        return false;
    }

    throw std::invalid_argument(
        "Protocol: field '" + key +
        "' must be boolean"
    );
}

void requireNonNegative(
    long long value,
    const std::string& field
) {
    if (value < 0) {
        throw std::invalid_argument(
            "Protocol: field '" + field +
            "' cannot be negative"
        );
    }
}

bool isBoardPosition(const Position& position) {
    return
        position.getRow() >= 0 &&
        position.getRow() <= 7 &&
        position.getCol() >= 0 &&
        position.getCol() <= 7;
}

Position readPosition(
    const std::string& json,
    const std::string& key,
    bool allowUnused = false
) {
    const std::string object = readObject(json, key);
    const int row = readInt(object, "row");
    const int col = readInt(object, "col");

    const Position position(row, col);

    if (allowUnused && row == -1 && col == -1) {
        return position;
    }

    if (!isBoardPosition(position)) {
        throw std::invalid_argument(
            "Protocol: position '" + key +
            "' is outside the board"
        );
    }

    return position;
}

void writePosition(
    std::ostringstream& output,
    const Position& position
) {
    output
        << "{\"row\":"
        << position.getRow()
        << ",\"col\":"
        << position.getCol()
        << "}";
}

void writePiece(
    std::ostringstream& output,
    const PieceSnapshot& piece
) {
    output
        << "{\"id\":" << piece.id
        << ",\"token\":\"" << escapeJson(piece.token) << "\""
        << ",\"position\":";

    writePosition(output, piece.position);

    output
        << ",\"state\":\"" << escapeJson(piece.state) << "\""
        << ",\"hasMoved\":"
        << (piece.hasMoved ? "true" : "false")
        << ",\"remainingCooldownMs\":"
        << piece.remainingCooldownMs
        << ",\"totalCooldownMs\":"
        << piece.totalCooldownMs
        << "}";
}

void writeMotion(
    std::ostringstream& output,
    const MotionSnapshot& motion
) {
    output
        << "{\"pieceId\":" << motion.pieceId
        << ",\"source\":";

    writePosition(output, motion.source);

    output << ",\"destination\":";
    writePosition(output, motion.destination);

    output << ",\"currentCell\":";
    writePosition(output, motion.currentCell);

    output
        << ",\"rowStep\":" << motion.rowStep
        << ",\"colStep\":" << motion.colStep
        << ",\"directMove\":"
        << (motion.directMove ? "true" : "false")
        << ",\"stepDurationMs\":"
        << motion.stepDurationMs
        << ",\"nextStepTimeMs\":"
        << motion.nextStepTimeMs
        << ",\"order\":"
        << motion.order
        << "}";
}

void writeJump(
    std::ostringstream& output,
    const JumpSnapshot& jump
) {
    output
        << "{\"pieceId\":" << jump.pieceId
        << ",\"cell\":";

    writePosition(output, jump.cell);

    output
        << ",\"finishTimeMs\":"
        << jump.finishTimeMs
        << "}";
}

void writeHistory(
    std::ostringstream& output,
    const MoveHistorySnapshot& history
) {
    output
        << "{\"completedAtMs\":"
        << history.completedAtMs
        << ",\"color\":\""
        << escapeJson(history.color)
        << "\""
        << ",\"pieceKind\":\""
        << escapeJson(history.pieceKind)
        << "\""
        << ",\"source\":";

    writePosition(output, history.source);

    output << ",\"destination\":";
    writePosition(output, history.destination);

    output
        << ",\"wasCapture\":"
        << (history.wasCapture ? "true" : "false")
        << ",\"wasPromotion\":"
        << (history.wasPromotion ? "true" : "false")
        << ",\"wasJump\":"
        << (history.wasJump ? "true" : "false")
        << ",\"notation\":\""
        << escapeJson(history.notation)
        << "\"}";
}

template <typename T, typename Writer>
void writeArray(
    std::ostringstream& output,
    const std::vector<T>& items,
    Writer writer
) {
    output << '[';

    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index > 0) {
            output << ',';
        }

        writer(output, items[index]);
    }

    output << ']';
}

void writeSnapshot(
    std::ostringstream& output,
    const GameStateSnapshot& snapshot
) {
    output
        << "{\"serverTimeMs\":"
        << snapshot.serverTimeMs
        << ",\"gameOver\":"
        << (snapshot.gameOver ? "true" : "false")
        << ",\"whiteScore\":"
        << snapshot.whiteScore
        << ",\"blackScore\":"
        << snapshot.blackScore
        << ",\"pieces\":";

    writeArray(output, snapshot.pieces, writePiece);

    output << ",\"motions\":";
    writeArray(output, snapshot.motions, writeMotion);

    output << ",\"jumps\":";
    writeArray(output, snapshot.jumps, writeJump);

    output << ",\"whiteMoveHistory\":";
    writeArray(
        output,
        snapshot.whiteMoveHistory,
        writeHistory
    );

    output << ",\"blackMoveHistory\":";
    writeArray(
        output,
        snapshot.blackMoveHistory,
        writeHistory
    );

    output << '}';
}

bool isValidToken(const std::string& token) {
    if (
        token.size() != 2 ||
        (token[0] != 'w' && token[0] != 'b')
    ) {
        return false;
    }

    const std::string kinds = "KQRBNP";
    return kinds.find(token[1]) != std::string::npos;
}

bool isValidState(const std::string& state) {
    return
        state == "idle" ||
        state == "moving" ||
        state == "airborne" ||
        state == "captured";
}

bool isValidColor(const std::string& color) {
    return color == "w" || color == "b";
}

bool isValidKind(const std::string& kind) {
    return
        kind.size() == 1 &&
        std::string("KQRBNP").find(kind[0]) !=
            std::string::npos;
}

PieceSnapshot readPiece(const std::string& object) {
    PieceSnapshot piece;

    piece.id = readInt(object, "id");
    piece.token = readString(object, "token");
    piece.position = readPosition(object, "position");
    piece.state = readString(object, "state");
    piece.hasMoved = readBool(object, "hasMoved");
    piece.remainingCooldownMs =
        readSigned(object, "remainingCooldownMs");
    piece.totalCooldownMs =
        readSigned(object, "totalCooldownMs");

    if (piece.id < 0) {
        throw std::invalid_argument(
            "Protocol: piece id cannot be negative"
        );
    }

    if (!isValidToken(piece.token)) {
        throw std::invalid_argument(
            "Protocol: invalid piece token"
        );
    }

    if (!isValidState(piece.state)) {
        throw std::invalid_argument(
            "Protocol: invalid piece state"
        );
    }

    requireNonNegative(
        piece.remainingCooldownMs,
        "remainingCooldownMs"
    );
    requireNonNegative(
        piece.totalCooldownMs,
        "totalCooldownMs"
    );

    if (
        piece.remainingCooldownMs >
        piece.totalCooldownMs
    ) {
        throw std::invalid_argument(
            "Protocol: remaining cooldown exceeds total cooldown"
        );
    }

    return piece;
}

MotionSnapshot readMotion(const std::string& object) {
    MotionSnapshot motion;

    motion.pieceId = readInt(object, "pieceId");
    motion.source = readPosition(object, "source");
    motion.destination =
        readPosition(object, "destination");
    motion.currentCell =
        readPosition(object, "currentCell");
    motion.rowStep = readInt(object, "rowStep");
    motion.colStep = readInt(object, "colStep");
    motion.directMove = readBool(object, "directMove");
    motion.stepDurationMs =
        readSigned(object, "stepDurationMs");
    motion.nextStepTimeMs =
        readSigned(object, "nextStepTimeMs");
    motion.order = readInt(object, "order");

    if (motion.pieceId < 0) {
        throw std::invalid_argument(
            "Protocol: motion pieceId cannot be negative"
        );
    }

    if (
        motion.rowStep < -1 || motion.rowStep > 1 ||
        motion.colStep < -1 || motion.colStep > 1
    ) {
        throw std::invalid_argument(
            "Protocol: motion step must be between -1 and 1"
        );
    }

    requireNonNegative(
        motion.stepDurationMs,
        "stepDurationMs"
    );
    requireNonNegative(
        motion.nextStepTimeMs,
        "nextStepTimeMs"
    );

    if (motion.order < 0) {
        throw std::invalid_argument(
            "Protocol: motion order cannot be negative"
        );
    }

    return motion;
}

JumpSnapshot readJump(const std::string& object) {
    JumpSnapshot jump;

    jump.pieceId = readInt(object, "pieceId");
    jump.cell = readPosition(object, "cell");
    jump.finishTimeMs =
        readSigned(object, "finishTimeMs");

    if (jump.pieceId < 0) {
        throw std::invalid_argument(
            "Protocol: jump pieceId cannot be negative"
        );
    }

    requireNonNegative(
        jump.finishTimeMs,
        "finishTimeMs"
    );

    return jump;
}

MoveHistorySnapshot readHistory(
    const std::string& object
) {
    MoveHistorySnapshot history;

    history.completedAtMs =
        readSigned(object, "completedAtMs");
    history.color = readString(object, "color");
    history.pieceKind = readString(object, "pieceKind");
    history.source = readPosition(object, "source");
    history.destination =
        readPosition(object, "destination");
    history.wasCapture = readBool(object, "wasCapture");
    history.wasPromotion =
        readBool(object, "wasPromotion");
    history.wasJump = readBool(object, "wasJump");
    history.notation = readString(object, "notation");

    requireNonNegative(
        history.completedAtMs,
        "completedAtMs"
    );

    if (!isValidColor(history.color)) {
        throw std::invalid_argument(
            "Protocol: invalid history color"
        );
    }

    if (!isValidKind(history.pieceKind)) {
        throw std::invalid_argument(
            "Protocol: invalid history piece kind"
        );
    }

    return history;
}

GameStateSnapshot readSnapshot(
    const std::string& object
) {
    GameStateSnapshot snapshot;

    snapshot.serverTimeMs =
        readSigned(object, "serverTimeMs");
    snapshot.gameOver = readBool(object, "gameOver");
    snapshot.whiteScore = readInt(object, "whiteScore");
    snapshot.blackScore = readInt(object, "blackScore");

    requireNonNegative(
        snapshot.serverTimeMs,
        "serverTimeMs"
    );

    if (
        snapshot.whiteScore < 0 ||
        snapshot.blackScore < 0
    ) {
        throw std::invalid_argument(
            "Protocol: scores cannot be negative"
        );
    }

    for (
        const std::string& pieceObject :
        readObjectArray(object, "pieces")
    ) {
        snapshot.pieces.push_back(
            readPiece(pieceObject)
        );
    }

    for (
        const std::string& motionObject :
        readObjectArray(object, "motions")
    ) {
        snapshot.motions.push_back(
            readMotion(motionObject)
        );
    }

    for (
        const std::string& jumpObject :
        readObjectArray(object, "jumps")
    ) {
        snapshot.jumps.push_back(
            readJump(jumpObject)
        );
    }

    for (
        const std::string& historyObject :
        readObjectArray(object, "whiteMoveHistory")
    ) {
        snapshot.whiteMoveHistory.push_back(
            readHistory(historyObject)
        );
    }

    for (
        const std::string& historyObject :
        readObjectArray(object, "blackMoveHistory")
    ) {
        snapshot.blackMoveHistory.push_back(
            readHistory(historyObject)
        );
    }

    return snapshot;
}

} // namespace

std::string Protocol::messageTypeToString(
    MessageType type
) {
    switch (type) {
        case MessageType::MoveRequest:
            return "move_request";

        case MessageType::JumpRequest:
            return "jump_request";

        case MessageType::MoveAccepted:
            return "move_accepted";

        case MessageType::MoveRejected:
            return "move_rejected";

        case MessageType::GameStateUpdated:
            return "game_state_updated";

        case MessageType::GameOver:
            return "game_over";
    }

    throw std::invalid_argument(
        "Protocol: unknown message type"
    );
}

MessageType Protocol::messageTypeFromString(
    const std::string& value
) {
    if (value == "move_request") {
        return MessageType::MoveRequest;
    }

    if (value == "jump_request") {
        return MessageType::JumpRequest;
    }

    if (value == "move_accepted") {
        return MessageType::MoveAccepted;
    }

    if (value == "move_rejected") {
        return MessageType::MoveRejected;
    }

    if (value == "game_state_updated") {
        return MessageType::GameStateUpdated;
    }

    if (value == "game_over") {
        return MessageType::GameOver;
    }

    throw std::invalid_argument(
        "Protocol: unsupported message type '" +
        value + "'"
    );
}

std::string Protocol::serialize(
    const Message& message
) {
    std::ostringstream output;

    output
        << "{\"version\":" << VERSION
        << ",\"type\":\""
        << messageTypeToString(message.type)
        << "\""
        << ",\"sequence\":" << message.sequence
        << ",\"source\":";

    writePosition(output, message.source);

    output << ",\"destination\":";
    writePosition(output, message.destination);

    output
        << ",\"accepted\":"
        << (message.accepted ? "true" : "false")
        << ",\"reason\":\""
        << escapeJson(message.reason)
        << "\""
        << ",\"createdAtMs\":"
        << message.createdAtMs
        << ",\"hasSnapshot\":"
        << (message.hasSnapshot ? "true" : "false");

    if (message.hasSnapshot) {
        output << ",\"snapshot\":";
        writeSnapshot(output, message.snapshot);
    }

    output << '}';

    return output.str();
}

Message Protocol::deserialize(
    const std::string& json
) {
    if (
        json.empty() ||
        json.front() != '{' ||
        json.back() != '}'
    ) {
        throw std::invalid_argument(
            "Protocol: message must be a JSON object"
        );
    }

    const long long version = readSigned(json, "version");

    if (version != VERSION) {
        throw std::invalid_argument(
            "Protocol: unsupported protocol version"
        );
    }

    Message message;

    message.type = messageTypeFromString(
        readString(json, "type")
    );
    message.sequence = readUnsigned(json, "sequence");
    message.source = readPosition(json, "source", true);
    message.destination =
        readPosition(json, "destination", true);
    message.accepted = readBool(json, "accepted");
    message.reason = readString(json, "reason");
    message.createdAtMs = readSigned(json, "createdAtMs");
    message.hasSnapshot = readBool(json, "hasSnapshot");

    requireNonNegative(
        message.createdAtMs,
        "createdAtMs"
    );

    if (message.hasSnapshot) {
        message.snapshot = readSnapshot(
            readObject(json, "snapshot")
        );
    }

    return message;
}