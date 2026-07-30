# CTD Architecture

## Server

`server/chess_server` owns every authoritative decision: move validation,
rules, timing, captures, scores, snapshots, lifecycle events, and TCP client
sessions. It links to `ctd_shared` and Winsock, but not OpenCV.

`server/cpp_gateway` is an independent native C++ service built on
Boost.Asio, Beast, and JSON. It owns HTTP authentication, Redis sessions,
authenticated WebSockets, lobby state, and rooms. It checks PostgreSQL and
Redis through `GET /health`, but never makes game-rule decisions.

## Client

`client/desktop_client` owns connection management, incoming snapshot handling,
move requests, mouse input, rendering, and animations. Its assets are copied
beside `ctd_client.exe` during the build, and the client resolves them relative
to its executable location.

## Shared C++

`shared/cpp` contains code used by both executables: board and piece data
types, positions, snapshot and protocol DTOs, serialization, common network
defaults, username validation, and the connected TCP socket wrapper.

The TCP listener, game engine, rules, EventBus, and snapshot construction are
server-only. OpenCV, rendering, and animations are client-only.
