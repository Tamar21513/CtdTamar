#include "../include/Network/Protocol.hpp"

#include <cassert>
#include <iostream>

int main() {
    Message original;

    original.type = MessageType::MoveRequest;
    original.sequence = 15;
    original.source = Position(6, 4);
    original.destination = Position(5, 4);
    original.accepted = false;
    original.reason = "test";
    original.createdAtMs = 8400;

    std::string json = Protocol::serialize(original);

    Message restored = Protocol::deserialize(json);

    assert(restored.type == original.type);
    assert(restored.sequence == original.sequence);
    assert(restored.source == original.source);
    assert(restored.destination == original.destination);
    assert(restored.accepted == original.accepted);
    assert(restored.reason == original.reason);
    assert(restored.createdAtMs == original.createdAtMs);

    std::cout << "Protocol test passed\n";
    std::cout << json << '\n';

    return 0;
}