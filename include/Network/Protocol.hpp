#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <string>

#include "../Messaging/Message.hpp"

namespace Protocol {
    constexpr int VERSION = 2;

    std::string messageTypeToString(MessageType type);
    MessageType messageTypeFromString(const std::string& value);

    std::string serialize(const Message& message);
    Message deserialize(const std::string& json);
}

#endif