#pragma once

#include "Network/SessionCookieJar.hpp"

#include <memory>
#include <optional>
#include <string>

namespace ctd::network {

enum class WebSocketConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Closing,
    Failed
};

enum class WebSocketEventKind {
    Text,
    Closed,
    AuthenticationFailed,
    TransportError
};

struct WebSocketEvent {
    WebSocketEventKind kind;
    std::string text;
    unsigned short closeCode = 0;
};

struct WebSocketHandshakeMetadata {
    std::string target;
    bool includesCookie = false;
    bool tokenAppearsInUrl = false;
};

class WebSocketClient {
public:
    explicit WebSocketClient(SessionCookieJar& cookieJar);
    ~WebSocketClient();

    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    bool connect(const std::string& url);
    bool sendText(const std::string& message);
    void disconnect();
    WebSocketConnectionState state() const;
    std::optional<WebSocketEvent> pollEvent();
    WebSocketHandshakeMetadata handshakeMetadata(
        const std::string& url) const;
    static WebSocketEventKind eventKindForCloseCode(
        unsigned short closeCode);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    SessionCookieJar& cookieJar_;
};

}  // namespace ctd::network
