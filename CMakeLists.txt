cmake_minimum_required(VERSION 3.24)

project(CTD LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_subdirectory(shared/cpp)
add_subdirectory(server/chess_server)
add_subdirectory(client/desktop_client)
