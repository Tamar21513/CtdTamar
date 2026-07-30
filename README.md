# CTD

The native API, authentication, WebSocket, and lobby service lives under
`server/cpp_gateway`. See `docs/CPP_GATEWAY.md`.

CTD is a networked Kung Fu Chess project with an authoritative C++ game
server, an OpenCV client, and a local Docker Compose infrastructure stack.

## Repository layout

- `server/cpp_gateway` — native HTTP, authentication, WebSocket, and lobby
  gateway.
- `server/chess_server` — authoritative engine, rules, timing, TCP server, and
  server-side tests.
- `client/desktop_client` — network client, OpenCV UI, renderer, animations, and
  visual assets.
- `shared/cpp` — DTOs, protocol serialization, common core types, and TCP
  connection code used by both C++ executables.
- `docs` — architecture, infrastructure, development, and historical test
  documentation.

## Configure and build C++

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" `
  -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=C:/Users/User/vcpkg/scripts/buildsystems/vcpkg.cmake

& "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" `
  --build build --config Release --parallel
```

Executables:

- `build/server/chess_server/Release/ctd_server.exe`
- `build/server/cpp_gateway/Release/ctd_cpp_gateway.exe`
- `build/client/desktop_client/Release/ctd_client.exe`

## Run

```powershell
.\build\client\desktop_client\Release\ctd_client.exe
```

The default client opens the native authentication and lobby UI. The existing
authoritative chess server and visual game client remain available explicitly:

```powershell
.\build\server\chess_server\Release\ctd_server.exe
.\build\client\desktop_client\Release\ctd_client.exe game
.\build\server\chess_server\Release\ctd_server.exe console
.\build\client\desktop_client\Release\ctd_client.exe text
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and
[docs/INFRASTRUCTURE.md](docs/INFRASTRUCTURE.md).
