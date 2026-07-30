if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.24)
    project(CTD_CPP_GATEWAY LANGUAGES CXX)
    enable_testing()
endif()

find_package(Boost CONFIG REQUIRED COMPONENTS json)
find_package(Threads REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)
find_path(SODIUM_INCLUDE_DIR sodium.h REQUIRED)
find_library(SODIUM_LIBRARY NAMES sodium libsodium REQUIRED)

add_library(ctd_gateway_core STATIC
    src/Config/GatewayConfig.cpp
    src/Auth/AuthService.cpp
    src/Auth/PasswordHasher.cpp
    src/Database/UserRepository.cpp
    src/Redis/SessionStore.cpp
    src/Rooms/RoomManager.cpp
    src/App/GatewayApp.cpp
    src/Http/HttpRouter.cpp
    src/Http/HttpServer.cpp
    src/WebSocket/WebSocketHub.cpp
)
set_target_properties(
    ctd_gateway_core
    PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
)

add_executable(
    ctd_gateway_tests
    tests/gateway_tests.cpp
)
set_target_properties(
    ctd_gateway_tests
    PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
)
target_link_libraries(ctd_gateway_tests PRIVATE ctd_gateway_core)
add_test(NAME ctd_gateway_tests COMMAND ctd_gateway_tests)

target_include_directories(ctd_gateway_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/cpp/include
)

target_link_libraries(ctd_gateway_core
    PUBLIC
        Boost::json
        Threads::Threads
        libpqxx::pqxx
        ${SODIUM_LIBRARY}
        redis++::redis++
)
target_include_directories(
    ctd_gateway_core PUBLIC ${SODIUM_INCLUDE_DIR}
)

add_executable(ctd_cpp_gateway src/main.cpp)
target_link_libraries(ctd_cpp_gateway PRIVATE ctd_gateway_core)
set_target_properties(
    ctd_cpp_gateway
    PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
)

if(WIN32)
    target_compile_definitions(
        ctd_gateway_core PUBLIC _WIN32_WINNT=0x0A00
    )
endif()
