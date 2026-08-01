# CTD

CTD is a networked Kung Fu Chess project with an authoritative C++ game
server, an OpenCV client, and a local Docker Compose infrastructure stack.

## Repository layout

- `server/api_gateway` — active Python/FastAPI authentication, WebSocket,
  lobby, and room Gateway.
- `server/chess_server` — authoritative engine, rules, timing, TCP server, and
  server-side tests.
- `client/desktop_client` — network client, OpenCV UI, renderer, animations, and
  visual assets.
- `shared/cpp` — DTOs, protocol serialization, common core types, and TCP
  connection code used by both C++ executables.
- `docs` — architecture, infrastructure, development, and historical test
  documentation.

The experimental native C++ Gateway is paused and preserved separately on the
`stage3g-cpp-gateway` branch; it is not part of the active runtime.

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

### Play from a second computer on the same LAN

Start the server stack as usual (`docker compose up -d`) on the host
machine, then allow inbound TCP on the gateway port (default 8000) through
Windows Firewall on the host machine for at least the Private network
profile. Find the host machine's LAN IP (`ipconfig`), then on the second
computer run:

```powershell
.\build\client\desktop_client\Release\ctd_client.exe lobby 192.168.1.50 8000
```

replacing `192.168.1.50` with the host machine's actual LAN IP. Running
`ctd_client.exe lobby` with no arguments still connects to `127.0.0.1:8000`
for same-machine play.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and
[docs/INFRASTRUCTURE.md](docs/INFRASTRUCTURE.md). The room-to-game startup
order and current single-match boundary are documented in
[docs/SERVER_BRIDGE.md](docs/SERVER_BRIDGE.md).
