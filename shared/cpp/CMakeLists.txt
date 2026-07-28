add_library(ctd_shared STATIC
    src/Core/Board.cpp
    src/Core/Piece.cpp
    src/Core/Position.cpp
    src/Network/Protocol.cpp
    src/Network/TcpConnection.cpp
)

target_include_directories(ctd_shared
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32)
    target_link_libraries(ctd_shared PUBLIC Ws2_32)
endif()
