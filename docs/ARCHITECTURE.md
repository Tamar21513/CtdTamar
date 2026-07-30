# CTD Architecture

## Server

`server/chess_server` owns every authoritative decision: move validation,
rules, timing, captures, scores, snapshots, lifecycle events, and TCP client
sessions. It links to `ctd_shared` and Winsock, but not OpenCV.

`server/api_gateway` is the active Python/FastAPI service. It owns HTTP
authentication, authenticated WebSockets, lobby updates, rooms, and spectator
metadata. PostgreSQL stores users and persistent data; Redis stores sessions
and temporary room/session state. The Gateway does not make chess-rule
decisions.

The native C++ Gateway prototype is paused and preserved on the separate
`stage3g-cpp-gateway` branch.

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
