#ifndef SERVER_APP_HPP
#define SERVER_APP_HPP

class ServerApp {
public:
    void run();

    // Runs the server on the requested TCP port.
    void run(unsigned short port);
};

#endif
