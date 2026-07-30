# Stage 3G authoritative game bridge

The current runtime keeps Python/FastAPI as the authenticated API and
WebSocket Gateway. Chess rules remain exclusively in the existing C++
`ctd_server` process.

## Runtime path

```text
native OpenCV client
  -> authenticated WebSocket /ws
  -> Python Gateway match router
  -> internal TCP connection
  -> authoritative C++ GameServer/GameEngine
```

The Gateway creates two internal TCP connections for an active room. The
host connection is white and the guest connection is black. It forwards
move coordinates and sequence numbers, then publishes only snapshots and
accept/reject reasons returned by C++.

Stage 3G deliberately supports one active match at a time because the
existing C++ server owns one `GameEngine`. A second room activation receives
`match_unavailable`. Concurrent games require a later allocator/process
model.

## Windows and Docker Desktop startup

Build the repository, then start the authoritative server before activating
a room:

```powershell
cmake --build build --config Release --target ctd_server
.\build\server\chess_server\Release\ctd_server.exe 5050
```

In another PowerShell window:

```powershell
Copy-Item .env.example .env
docker compose up -d --build
docker compose ps
Invoke-RestMethod http://127.0.0.1:8000/health
```

The defaults use `host.docker.internal:5050`. Override
`CTD_GAME_SERVER_HOST` and `CTD_GAME_SERVER_PORT` when the C++ server runs
elsewhere.

Finally launch two native clients:

```powershell
.\build\client\desktop_client\Release\ctd_client.exe lobby
```

The second player changes the room to active. Both clients then receive the
same authoritative initial snapshot and open the board automatically.
Spectators receive the latest snapshot and subsequent revisions without
move controls.

The room bridge does not currently restore a match after its player TCP
connections are torn down. Disconnect cleanup releases the single server
allocation.
