add_library(ctd_server_core STATIC
    src/App/ConsoleApp.cpp
    src/Control/Controller.cpp
    src/Engine/GameEngine.cpp
    src/IO/BoardMapper.cpp
    src/IO/BoardParser.cpp
    src/IO/BoardPrinter.cpp
    src/Messaging/EngineMessageHandler.cpp
    src/Messaging/EventBus.cpp
    src/Messaging/GameEventSubscribers.cpp
    src/Messaging/GameStateSnapshotBuilder.cpp
    src/Messaging/MessageBus.cpp
    src/Network/TcpServer.cpp
    src/Realtime/RealTimeArbiter.cpp
    src/Rules/PieceRules.cpp
    src/Rules/RuleEngine.cpp
    src/Server/ClientSession.cpp
    src/Server/GameServer.cpp
    src/Server/ServerApp.cpp
)

target_include_directories(ctd_server_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ctd_server_core PUBLIC ctd_shared)

add_executable(ctd_server main.cpp)
target_link_libraries(ctd_server PRIVATE ctd_server_core)

add_subdirectory(tests)
