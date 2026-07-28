set(CTD_DOCTEST_SOURCES
    doctest/test_main.cpp
    doctest/test_position_piece.cpp
    doctest/test_board_parser_mapper.cpp
    doctest/test_rules.cpp
    doctest/test_engine_controller.cpp
    doctest/test_realtime_arbiter.cpp
    doctest/test_event_bus.cpp
    doctest/test_username.cpp
)

add_executable(ctd_tests
    ${CTD_DOCTEST_SOURCES}
    ${PROJECT_SOURCE_DIR}/client/game_client/src/Client/ClientGameState.cpp
)

target_include_directories(ctd_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/doctest
    ${PROJECT_SOURCE_DIR}/client/game_client/include
)

target_link_libraries(ctd_tests PRIVATE ctd_server_core)

add_test(NAME ctd_tests COMMAND ctd_tests)

add_executable(ctd_legacy_tests legacy_console_main.cpp)
target_link_libraries(ctd_legacy_tests PRIVATE ctd_server_core)

add_executable(
    ctd_username_server_smoke
    username_server_smoke.cpp
)
target_link_libraries(
    ctd_username_server_smoke
    PRIVATE ctd_server_core
)

add_executable(ctd_protocol_smoke protocol_smoke.cpp)
target_link_libraries(ctd_protocol_smoke PRIVATE ctd_shared)

add_executable(ctd_game_server_smoke game_server_smoke.cpp)
target_link_libraries(ctd_game_server_smoke PRIVATE ctd_server_core)
