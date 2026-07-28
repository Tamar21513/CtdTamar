#ifndef CLIENT_APP_HPP
#define CLIENT_APP_HPP

#include <string>

class ClientApp {
public:
    void run();

    // Runs the client against one server endpoint.
    void run(
        const std::string& serverHost,
        unsigned short serverPort
    );
};

#endif
