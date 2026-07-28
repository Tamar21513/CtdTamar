find_package(OpenCV CONFIG REQUIRED
    COMPONENTS core imgproc imgcodecs highgui
)

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
)

target_include_directories(ctd_client_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(ctd_client_core
    PUBLIC
        ctd_shared
        ${OpenCV_LIBS}
)

add_executable(ctd_client main.cpp)
target_link_libraries(ctd_client PRIVATE ctd_client_core)

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
add_custom_command(
    TARGET ctd_client_asset_smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:ctd_client_asset_smoke>/assets
)
