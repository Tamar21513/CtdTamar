#include "Network/WebSocketClient.hpp"
#include "Network/ThreadSafeQueue.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>

#include <atomic>
#include <deque>
#include <thread>
#include <utility>

namespace ctd::network {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

struct ParsedWebSocketUrl {
    std::string scheme;
    std::string host;
    std::string port;
    std::string target;
};

ParsedWebSocketUrl parseWebSocketUrl(const std::string& url) {
    const auto schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) {
        throw std::invalid_argument("WebSocket URL is missing a scheme");
    }
    ParsedWebSocketUrl parsed;
    parsed.scheme = url.substr(0, schemeEnd);
    const auto authorityStart = schemeEnd + 3;
    const auto pathStart = url.find('/', authorityStart);
    const auto authority = url.substr(
        authorityStart, pathStart - authorityStart);
    parsed.target = pathStart == std::string::npos
        ? "/" : url.substr(pathStart);
    const auto colon = authority.rfind(':');
    if (colon == std::string::npos) {
        parsed.host = authority;
        parsed.port = parsed.scheme == "wss" ? "443" : "80";
    } else {
        parsed.host = authority.substr(0, colon);
        parsed.port = authority.substr(colon + 1);
    }
    if (parsed.host.empty() || parsed.port.empty()) {
        throw std::invalid_argument("WebSocket URL authority is invalid");
    }
    return parsed;
}

}  // namespace

class WebSocketClient::Impl {
public:
    explicit Impl(SessionCookieJar& cookieJar)
        : cookieJar(cookieJar),
          resolver(context),
          socket(context) {}

    void push(WebSocketEvent event) {
        events.push(std::move(event));
    }

    void readNext() {
        socket.async_read(
            buffer,
            [this](beast::error_code error, std::size_t) {
                if (error) {
                    const auto code = static_cast<unsigned short>(
                        socket.reason().code);
                    if (WebSocketClient::eventKindForCloseCode(code) ==
                        WebSocketEventKind::AuthenticationFailed) {
                        state.store(
                            WebSocketConnectionState::Failed);
                        push({WebSocketEventKind::AuthenticationFailed,
                              {}, code});
                    } else if (error == websocket::error::closed) {
                        state.store(
                            WebSocketConnectionState::Disconnected);
                        push({WebSocketEventKind::Closed, {}, code});
                    } else if (
                        state.load() !=
                        WebSocketConnectionState::Closing) {
                        state.store(
                            WebSocketConnectionState::Failed);
                        push({WebSocketEventKind::TransportError,
                              error.message(), code});
                    }
                    return;
                }
                push({WebSocketEventKind::Text,
                      beast::buffers_to_string(buffer.data()), 0});
                buffer.consume(buffer.size());
                readNext();
            });
    }

    void writeNext() {
        if (outgoing.empty() || writeActive) {
            return;
        }
        writeActive = true;
        socket.async_write(
            asio::buffer(outgoing.front()),
            [this](beast::error_code error, std::size_t) {
                if (error) {
                    state.store(WebSocketConnectionState::Failed);
                    push({WebSocketEventKind::TransportError,
                          error.message(), 0});
                    return;
                }
                outgoing.pop_front();
                writeActive = false;
                writeNext();
            });
    }

    void run(
        ParsedWebSocketUrl parsed,
        std::optional<std::string> cookie) {
        try {
            if (parsed.scheme != "ws") {
                throw std::invalid_argument(
                    "Only local ws:// is supported by this transport");
            }
            const auto endpoints =
                resolver.resolve(parsed.host, parsed.port);
            asio::connect(socket.next_layer(), endpoints);
            socket.set_option(
                websocket::stream_base::timeout::suggested(
                    beast::role_type::client));
            socket.set_option(websocket::stream_base::decorator(
                [cookie = std::move(cookie)](
                    websocket::request_type& request) {
                    request.set(
                        beast::http::field::user_agent,
                        "CTD-Native-Client/1");
                    if (cookie) {
                        request.set(
                            beast::http::field::cookie, *cookie);
                    }
                }));
            socket.handshake(parsed.host, parsed.target);
            state.store(WebSocketConnectionState::Connected);
            readNext();
            context.run();
        } catch (const std::exception& error) {
            state.store(WebSocketConnectionState::Failed);
            push({WebSocketEventKind::TransportError, error.what(), 0});
        }
    }

    SessionCookieJar& cookieJar;
    asio::io_context context;
    tcp::resolver resolver;
    websocket::stream<tcp::socket> socket;
    beast::flat_buffer buffer;
    std::atomic<WebSocketConnectionState> state{
        WebSocketConnectionState::Disconnected};
    std::thread worker;
    ThreadSafeQueue<WebSocketEvent> events;
    std::deque<std::string> outgoing;
    bool writeActive = false;
};

WebSocketClient::WebSocketClient(SessionCookieJar& cookieJar)
    : impl_(std::make_unique<Impl>(cookieJar)),
      cookieJar_(cookieJar) {}

WebSocketClient::~WebSocketClient() {
    disconnect();
}

bool WebSocketClient::connect(const std::string& url) {
    if (state() != WebSocketConnectionState::Disconnected &&
        state() != WebSocketConnectionState::Failed) {
        return false;
    }
    if (impl_->worker.joinable()) {
        impl_->worker.join();
    }
    try {
        auto parsed = parseWebSocketUrl(url);
        impl_ = std::make_unique<Impl>(cookieJar_);
        impl_->state.store(WebSocketConnectionState::Connecting);
        const auto cookie = cookieJar_.cookieHeader();
        impl_->worker = std::thread(
            [this, parsed = std::move(parsed), cookie]() mutable {
                impl_->run(std::move(parsed), cookie);
            });
        return true;
    } catch (const std::exception& error) {
        impl_->state.store(WebSocketConnectionState::Failed);
        impl_->push({
            WebSocketEventKind::TransportError, error.what(), 0});
        return false;
    }
}

bool WebSocketClient::sendText(const std::string& message) {
    if (state() != WebSocketConnectionState::Connected) {
        return false;
    }
    asio::post(impl_->context, [this, message]() {
        impl_->outgoing.push_back(message);
        impl_->writeNext();
    });
    return true;
}

void WebSocketClient::disconnect() {
    const auto current = state();
    if (current == WebSocketConnectionState::Connected) {
        impl_->state.store(WebSocketConnectionState::Closing);
        asio::post(impl_->context, [this]() {
            impl_->socket.async_close(
                websocket::close_code::normal,
                [this](beast::error_code error) {
                    impl_->state.store(
                        WebSocketConnectionState::Disconnected);
                    if (error && error != websocket::error::closed) {
                        impl_->push({
                            WebSocketEventKind::TransportError,
                            error.message(), 0});
                    } else {
                        impl_->push({
                            WebSocketEventKind::Closed, {}, 1000});
                    }
                });
        });
    } else if (current == WebSocketConnectionState::Connecting) {
        impl_->state.store(WebSocketConnectionState::Closing);
        impl_->context.stop();
    } else {
        impl_->context.stop();
    }
    if (impl_->worker.joinable()) {
        impl_->worker.join();
    }
    impl_->state.store(WebSocketConnectionState::Disconnected);
}

WebSocketConnectionState WebSocketClient::state() const {
    return impl_->state.load();
}

std::optional<WebSocketEvent> WebSocketClient::pollEvent() {
    return impl_->events.tryPop();
}

WebSocketHandshakeMetadata WebSocketClient::handshakeMetadata(
    const std::string& url) const {
    const auto parsed = parseWebSocketUrl(url);
    const auto cookie = cookieJar_.cookieHeader();
    return {
        parsed.target,
        cookie.has_value(),
        cookie && url.find(*cookie) != std::string::npos};
}

WebSocketEventKind WebSocketClient::eventKindForCloseCode(
    unsigned short closeCode) {
    return closeCode == 4401
        ? WebSocketEventKind::AuthenticationFailed
        : WebSocketEventKind::Closed;
}

}  // namespace ctd::network
