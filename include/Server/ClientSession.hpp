#ifndef CLIENT_SESSION_HPP
#define CLIENT_SESSION_HPP

#include "../Network/TcpConnection.hpp"
#include "../Core/Piece.hpp"

#include <atomic>
#include <mutex>
#include <string>

class ClientSession {
private:
    int clientId;
    PieceColor assignedColor;

    TcpConnection connection;

    std::atomic<bool> connected;

    /*
     * מונע משני threads לשלוח באותו זמן
     * דרך אותו socket.
     */
    std::mutex sendMutex;

public:
    ClientSession(
        int clientId,
        PieceColor assignedColor,
        TcpConnection connection
    );

    ClientSession(
        const ClientSession&
    ) = delete;

    ClientSession& operator=(
        const ClientSession&
    ) = delete;

    int getClientId() const;

    PieceColor getAssignedColor() const;

    bool isConnected() const;

    TcpConnection& getConnection();

    void sendMessage(
        const std::string& message
    );

    void disconnect();
};

#endif