#pragma once

#include <string>

class VisualApp {
public:
    void run();

    // Runs the visual client against one server endpoint.
    void run(
        const std::string& serverHost,
        unsigned short serverPort
    );
};
