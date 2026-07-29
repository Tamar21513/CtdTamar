find_package(OpenCV CONFIG REQUIRED
    COMPONENTS core imgproc imgcodecs highgui
)
find_package(Boost CONFIG REQUIRED COMPONENTS json)
find_package(Threads REQUIRED)

add_library(ctd_client_core STATIC
    src/App/VisualApp.cpp
    src/Client/ClientApp.cpp
    src/Client/ClientGameState.cpp
    src/Client/GameClient.cpp
    src/Client/NetworkController.cpp
    src/Graphics/AnimatedPiece.cpp
    src/Graphics/AnimationLibrary.cpp
    src/Graphics/AnimationPlayer.cpp
    src/Graphics/GraphicsHelper.cpp
    src/Graphics/img.cpp
    src/Graphics/Renderer.cpp
    src/Graphics/VisualMotion.cpp
    src/Graphics/VisualSnapshotBuilder.cpp
    src/Graphics/VisualState.cpp
    src/Graphics/VisualStateMachine.cpp
    src/Network/ApiClient.cpp
    src/Network/LobbyClient.cpp
    src/Network/LobbyProtocol.cpp
    src/Network/SessionCookieJar.cpp
    src/Network/WebSocketClient.cpp
)

target_include_directories(ctd_client_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(ctd_client_core
    PUBLIC
        ctd_shared
        Boost::json
        Threads::Threads
        ${OpenCV_LIBS}
)

if(WIN32)
    target_compile_definitions(
        ctd_client_core
        PUBLIC _WIN32_WINNT=0x0A00
    )
endif()

function(ctd_copy_runtime_dependencies target_name)
    if(WIN32)
        add_custom_command(
            TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target_name}>
                $<TARGET_FILE_DIR:${target_name}>
            COMMAND_EXPAND_LISTS
        )
    endif()
endfunction()

add_executable(ctd_client main.cpp)
target_link_libraries(ctd_client PRIVATE ctd_client_core)
ctd_copy_runtime_dependencies(ctd_client)

add_custom_command(TARGET ctd_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:ctd_client>/assets
)

add_executable(
    ctd_client_asset_smoke
    tests/asset_smoke.cpp
)
target_link_libraries(
    ctd_client_asset_smoke
    PRIVATE ctd_client_core
)
ctd_copy_runtime_dependencies(ctd_client_asset_smoke)
add_custom_command(
    TARGET ctd_client_asset_smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:ctd_client_asset_smoke>/assets
)

add_executable(
    ctd_client_transport_tests
    tests/transport_tests.cpp
)
target_link_libraries(
    ctd_client_transport_tests
    PRIVATE ctd_client_core
)
ctd_copy_runtime_dependencies(ctd_client_transport_tests)
add_test(
    NAME ctd_client_transport_tests
    COMMAND ctd_client_transport_tests
)
