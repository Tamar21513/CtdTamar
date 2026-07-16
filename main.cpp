#include "include/App/ConsoleApp.hpp"
#include "include/App/VisualApp.hpp"

#include <iostream>
#include <string>
#include <exception>

int main(int argc, char* argv[]) {
    try {
        if (argc >= 2) {
            std::string mode = argv[1];

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
        }

        std::cout << "Usage:" << std::endl;
        std::cout << "  main.exe console" << std::endl;
        std::cout << "  main.exe visual" << std::endl;

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}