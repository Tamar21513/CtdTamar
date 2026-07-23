#include "include/App/ConsoleApp.hpp"
#include "include/App/VisualApp.hpp"
#include "include/Client/ClientApp.hpp"
#include "include/Server/ServerApp.hpp"

#include <exception>
#include <iostream>
#include <string>

// Selects and starts the requested application mode.
int main(int argc, char* argv[]) {
    try {
        if (argc >= 2) {
            const std::string mode = argv[1];

            if (mode == "console") {
                ConsoleApp app;
                app.run();
                return 0;
            }

            if (mode == "visual") {
                VisualApp app;
                app.run();
                return 0;
            }

            if (mode == "server") {
                ServerApp app;
                app.run();
                return 0;
            }

            if (mode == "client") {
                ClientApp app;
                app.run();
                return 0;
            }
        }

        std::cout << "Usage:\n";
        std::cout << "  main.exe console\n";
        std::cout << "  main.exe visual\n";
        std::cout << "  main.exe server\n";
        std::cout << "  main.exe client\n";

        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr
            << "Error: "
            << exception.what()
            << '\n';

        return 1;
    }
}