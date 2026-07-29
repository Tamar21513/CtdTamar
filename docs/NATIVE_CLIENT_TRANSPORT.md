# Stage 3E Native Client Transport

Stage 3E adds authentication and lobby transport to the existing native
C++/OpenCV desktop client. It does not add lobby graphics and does not replace
the existing TCP chess transport.

## Dependencies

The client uses Boost.Beast for HTTP and WebSocket and Boost.JSON for JSON.
They are components of one maintained Boost stack and are installed through
the repository `vcpkg.json` manifest. The previous C++ code had only a custom
TCP connection: it could not perform HTTP requests, process `Set-Cookie`, or
perform an authenticated WebSocket upgrade.

Install and configure on Windows:

```powershell
& "C:\Users\User\vcpkg\vcpkg.exe" install --triplet x64-windows

& "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" `
  -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=C:/Users/User/vcpkg/scripts/buildsystems/vcpkg.cmake
```

Linux uses the same manifest with the platform's vcpkg triplet and a normal
CMake generator.

## Responsibilities

- `ClientTransportConfig` contains the API and WebSocket URLs. Local defaults
  are `http://127.0.0.1:8000` and `ws://127.0.0.1:8000/ws`.
- `SessionCookieJar` accepts `Set-Cookie`, retains only `ctd_session` in
  memory, and never exposes its opaque value to application or UI state.
- `ApiClient` sends register, login, `/auth/me`, and logout requests. It
  returns structured status and never includes passwords in errors or logs.
- `WebSocketClient` attaches the session through the `Cookie` handshake
  header, performs text I/O, and owns its worker thread.
- `LobbyProtocol` serializes typed Stage 3D requests and validates typed
  incoming events.
- `LobbyClient` coordinates authentication, identity verification, realtime
  connection, lobby commands, and polling.

## Authentication sequence

1. The application explicitly calls login.
2. `ApiClient` sends credentials to `/auth/login`.
3. A successful `Set-Cookie` is retained internally.
4. `LobbyClient` verifies the identity through `/auth/me`.
5. The application explicitly connects realtime.
6. `WebSocketClient` sends `Cookie: ctd_session=<opaque-value>` during the
   `/ws` handshake.

The backend intentionally returns the same `401` response for an unknown
username and a wrong password. The native API therefore returns a generic
`invalid_credentials` result. Registration is a separate explicit action;
login failure never creates an account automatically.

Logout disconnects realtime, calls `/auth/logout`, and clears the local cookie
even if the server or network fails. Authentication failure from `/auth/me`
also clears it. Tokens are not placed in URLs, JSON, environment variables,
configuration files, UI state, or logs.

`HttpOnly` is a browser attribute. In this native client, its security intent
is preserved by keeping the value private to the networking classes.

## Threading and event delivery

HTTP operations are isolated behind `ApiClient`. The WebSocket owns one
joinable worker thread running its Asio event loop. Reads and writes execute
on that loop. Parsed application events cross to the caller through a
mutex-protected queue and are consumed with `pollEvent`; networking code never
calls OpenCV or renders UI.

Disconnect requests a normal close and joins the worker. There are no detached
threads. Connection state is one explicit enum: disconnected, connecting,
connected, closing, or failed. Close code `4401` is reported as an
authentication failure.

## Transport security

The current Docker development environment deliberately uses plain
`http://` and `ws://`. This traffic is not encrypted. The transport rejects
HTTPS/WSS today but its URL/configuration boundary leaves room for TLS streams.
A production deployment must add certificate validation and use HTTPS/WSS
before credentials or sessions are sent over a network.

## Relationship to the chess server

This layer covers authentication, lobby, rooms, and spectator metadata only.
`GameClient`, `NetworkController`, and the current TCP protocol are unchanged.
The authoritative C++ `GameEngine` remains the only source of chess rules.
The API Gateway and WebSocket lobby are not authoritative game engines.

## Non-goals

Stage 3E does not implement lobby graphics, room cards, OpenCV dialogs, chess
moves over WebSocket, board synchronization, server allocation, match
reconnection, persistent credentials, or automatic registration.

## Manual validation

Start the local services, then use the transport integration test/client:

```powershell
docker compose up -d
docker compose ps
```

Validate this sequence with an isolated test username:

1. Register and log in.
2. Verify `/auth/me`.
3. Connect `/ws` and receive `connected`.
4. Send `ping` and receive `pong`.
5. Subscribe and receive `lobby_snapshot`.
6. Create a public room and receive `room_created` plus a lobby update.
7. Log out and verify the local cookie is cleared.
8. Attempt an unauthenticated WebSocket and verify rejection.

Run C++ tests with:

```powershell
ctest --test-dir build -C Release --output-on-failure
```
