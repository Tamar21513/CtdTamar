# CTD

CTD is a networked Kung Fu Chess project with an authoritative C++ game
server, an OpenCV client, and a local Docker Compose infrastructure stack.

## Repository layout

- `server/api_gateway` — FastAPI health service.
- `server/game_server` — authoritative engine, rules, timing, TCP server, and
  server-side tests.
- `client/game_client` — network client, OpenCV UI, renderer, animations, and
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

- `build/server/game_server/Release/ctd_server.exe`
- `build/client/game_client/Release/ctd_client.exe`

## Run

```powershell
.\build\server\game_server\Release\ctd_server.exe
.\build\client\game_client\Release\ctd_client.exe
```

Compatibility modes:

```powershell
.\build\server\game_server\Release\ctd_server.exe console
.\build\client\game_client\Release\ctd_client.exe text
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and
[docs/INFRASTRUCTURE.md](docs/INFRASTRUCTURE.md).
