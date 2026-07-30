#include "Http/HttpServer.hpp"
#include "Http/HttpRouter.hpp"
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <deque>

namespace ctd::gateway {
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

class SocketSession : public std::enable_shared_from_this<SocketSession> {
public:
    SocketSession(tcp::socket socket, GatewayState& state)
        : stream_(std::move(socket)), state_(state), router_(state) {}
    void run() { readHttp(); }
private:
    void readHttp() {
        auto self = shared_from_this();
        http::async_read(stream_, buffer_, request_,
            [self](beast::error_code error, std::size_t) {
                if (!error) self->handleHttp();
            });
    }
    void handleHttp() {
        if (websocket::is_upgrade(request_) && request_.target() == "/ws") {
            user_ = router_.authenticate(request_);
            websocket_.emplace(std::move(stream_));
            auto self = shared_from_this();
            websocket_->async_accept(request_, [self](beast::error_code error) {
                if (error) return;
                if (!self->user_) {
                    websocket::close_reason reason;
                    reason.code = static_cast<websocket::close_code>(4401);
                    reason.reason = "unauthorized";
                    self->websocket_->async_close(reason, [](auto){});
                    return;
                }
                self->connectionId_ = self->state_.hub.add(
                    [weak = std::weak_ptr<SocketSession>(self)](std::string text) {
                        if (auto locked = weak.lock()) locked->queue(std::move(text));
                    });
                self->queue(boost::json::serialize(boost::json::object{
                    {"type", "connected"},
                    {"user", boost::json::object{
                        {"id", self->user_->id},
                        {"username", self->user_->username}}}}));
                self->readWebSocket();
            });
            return;
        }
        response_ = router_.route(request_);
        auto self = shared_from_this();
        http::async_write(stream_, response_,
            [self](beast::error_code, std::size_t) {
                beast::error_code ignored;
                self->stream_.socket().shutdown(
                    tcp::socket::shutdown_send, ignored);
            });
    }
    void queue(std::string message) {
        asio::post(websocket_->get_executor(),
            [self = shared_from_this(), message = std::move(message)] {
                const bool idle = self->outgoing_.empty();
                self->outgoing_.push_back(message);
                if (idle) self->writeWebSocket();
            });
    }
    void writeWebSocket() {
        auto self = shared_from_this();
        websocket_->async_write(asio::buffer(outgoing_.front()),
            [self](beast::error_code error, std::size_t) {
                if (error) return;
                self->outgoing_.pop_front();
                if (!self->outgoing_.empty()) self->writeWebSocket();
            });
    }
    void readWebSocket() {
        auto self = shared_from_this();
        websocket_->async_read(buffer_,
            [self](beast::error_code error, std::size_t) {
                if (error) { self->cleanup(); return; }
                const auto text = beast::buffers_to_string(self->buffer_.data());
                self->buffer_.consume(self->buffer_.size());
                self->handleMessage(text);
                self->readWebSocket();
            });
    }
    void handleMessage(const std::string& text) {
        try {
            const auto object = boost::json::parse(text).as_object();
            const auto type = std::string(object.at("type").as_string());
            if (type == "ping") { queue(R"({"type":"pong"})"); return; }
            if (type == "subscribe_lobby") {
                state_.hub.subscribeLobby(connectionId_);
                queue(boost::json::serialize(state_.rooms.lobbySnapshot()));
                return;
            }
            if (type == "get_lobby") {
                queue(boost::json::serialize(state_.rooms.lobbySnapshot()));
                return;
            }
            RoomUser current{user_->id, user_->username, connectionId_};
            if (type == "create_room") {
                auto room = state_.rooms.create(
                    current, std::string(object.at("visibility").as_string()));
                boost::json::object body{
                    {"room_id", room.id}, {"visibility", room.visibility},
                    {"status", room.status}, {"host", publicUser(room.host)}};
                if (room.code) body["room_code"] = *room.code;
                queue(boost::json::serialize(boost::json::object{
                    {"type", "room_created"}, {"room", body}}));
                broadcastLobby(); return;
            }
            if (type == "join_room" || type == "join_hidden_room") {
                auto room = type == "join_room"
                    ? state_.rooms.joinPublic(
                        std::string(object.at("room_id").as_string()), current)
                    : state_.rooms.joinHidden(
                        std::string(object.at("room_code").as_string()), current);
                startRoom(room); broadcastLobby(); return;
            }
            if (type == "watch_game") {
                auto room = state_.rooms.watch(
                    std::string(object.at("room_id").as_string()), current);
                queue(boost::json::serialize(boost::json::object{
                    {"type", "watching_game"}, {"room_id", room.id},
                    {"white", publicUser(*room.white)},
                    {"black", publicUser(*room.black)},
                    {"spectator_count", room.spectators.size()}}));
                broadcastLobby(); return;
            }
            queueError("unsupported_message_type", "Message type is not supported");
        } catch (const RoomError& error) {
            queueError(error.code, error.what());
        } catch (const boost::system::system_error&) {
            queueError("invalid_json", "Message must be valid JSON");
        } catch (const std::exception&) {
            queueError("invalid_message", "Message must be a JSON object");
        }
    }
    void queueError(const std::string& code, const std::string& message) {
        queue(boost::json::serialize(boost::json::object{
            {"type", "error"}, {"code", code}, {"message", message}}));
    }
    void startRoom(const Room& room) {
        for (const auto& player : {*room.white, *room.black}) {
            const auto opponent = player.id == room.white->id
                ? *room.black : *room.white;
            state_.hub.send(player.connectionId, boost::json::serialize(
                boost::json::object{
                {"type", "game_started"}, {"room_id", room.id},
                {"color", player.id == room.white->id ? "white" : "black"},
                {"opponent", publicUser(opponent)}}));
        }
    }
    void broadcastLobby() {
        state_.hub.broadcastLobby(
            boost::json::serialize(state_.rooms.lobbySnapshot()));
    }
    void cleanup() {
        if (!user_ || connectionId_ == 0) return;
        const auto result =
            state_.rooms.removeConnection(user_->id, connectionId_);
        for (const auto& [id, message] : result.notifications)
            state_.hub.send(id, boost::json::serialize(message));
        state_.hub.remove(connectionId_);
        if (result.lobbyChanged) broadcastLobby();
        connectionId_ = 0;
    }
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
    http::response<http::string_body> response_;
    GatewayState& state_;
    HttpRouter router_;
    std::optional<websocket::stream<beast::tcp_stream>> websocket_;
    std::optional<User> user_;
    std::size_t connectionId_ = 0;
    std::deque<std::string> outgoing_;
};

class HttpServer::Listener : public std::enable_shared_from_this<Listener> {
public:
    Listener(asio::io_context& context, tcp::endpoint endpoint, GatewayState& state)
        : acceptor_(context), state_(state) {
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(asio::socket_base::reuse_address(true));
        acceptor_.bind(endpoint); acceptor_.listen();
    }
    void run() { accept(); }
private:
    void accept() {
        acceptor_.async_accept(
            asio::make_strand(acceptor_.get_executor()),
            [self = shared_from_this()](
            beast::error_code error, tcp::socket socket) {
            if (!error) std::make_shared<SocketSession>(
                std::move(socket), self->state_)->run();
            self->accept();
        });
    }
    tcp::acceptor acceptor_;
    GatewayState& state_;
};

HttpServer::HttpServer(
    asio::io_context& context, const std::string& address,
    unsigned short port, GatewayState& state)
    : listener_(std::make_shared<Listener>(
          context, tcp::endpoint(asio::ip::make_address(address), port), state)) {
    listener_->run();
}
}
