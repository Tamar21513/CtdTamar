# Server Design

## Summary

Kung Fu Chess (CTD) is a real-time chess variant with a native C++/OpenCV
desktop client and a two-process server backend. This document describes the
server-side architecture **as it is actually implemented on this branch**
(`stage3g-match-countdown`) today. It is intentionally scoped much smaller
than a previously-discussed long-term scalable design (separate API/WebSocket
Gateways, a real Matchmaker, a Game Allocator managing many Game Server
shards, a NATS/Redis event bus, Kubernetes, Agones). That design remains a
plausible future direction, not something partially built and half-wired in
this codebase. Each section below states plainly what exists, what doesn't,
and why not yet.

## Current architecture

```text
                 HTTP (register/login/logout/me)
Native C++/OpenCV  --------------------------------->  api_gateway
desktop client                                         (Python / FastAPI)
                 <---------------------------------
                 authenticated WebSocket /ws              |   |
                 (lobby, rooms, match routing,             |   |
                  countdown, move/jump forwarding)          |   |
                                                              |   |
                                        psycopg (sync) -------+   |
                                              v                    |
                                         PostgreSQL                |
                                         (users table)             |
                                                                     |
                                        redis-py -------------------+
                                              v
                                            Redis
                                     (session tokens, TTL)

api_gateway  ---- internal TCP, fixed host:port ---->  chess_server
             (one connection per player, forwards          (C++,
              move/jump, relays snapshots)              authoritative
                                                          GameEngine)
```

`chess_server` is started and run **manually**, outside Docker Compose, as a
single standalone process (`ctd_server.exe <port>`). `api_gateway` reaches it
via `CTD_GAME_SERVER_HOST` / `CTD_GAME_SERVER_PORT` (default
`host.docker.internal:5050`). It is not a Compose service, has no health
check from Compose's perspective, and there is exactly one of it.

## Components

### `api_gateway` (Python / FastAPI, `server/api_gateway`)

One process, defined as the `api-gateway` service in `compose.yaml`, owns:

- **Auth** (`app/auth/`) — register/login/logout/me over plain HTTP.
  Passwords are hashed with Argon2id (`app/auth/passwords.py`) and never
  stored or logged in plaintext. Sessions are opaque tokens stored in Redis
  under `ctd:session:<token>` with a TTL; the token lives only in an
  `HttpOnly` cookie.
- **Rooms and lobby** (`app/rooms/`) — `RoomManager` is a plain in-memory
  Python object (dicts guarded by one `asyncio.Lock`), not backed by Redis or
  Postgres. Waiting rooms, hidden-room codes, active rooms, and spectator
  sets all live only inside this one process's memory and are lost on
  restart.
- **Match routing/allocation** (`app/matches/`) — `SingleMatchAllocator`
  (`allocator.py`) is a boolean semaphore guarded by one `asyncio.Lock`: it
  allows exactly one active match at a time, full stop. `MatchManager`
  (`manager.py`) owns the 3.8-second countdown, forwards moves/jumps to the
  bridge, and republishes snapshots/results to both players and any
  spectators. `GameServerBridge` (`bridge.py`) opens two `asyncio` TCP
  connections (one per color) to the one configured `chess_server` and
  translates the existing TCP JSON protocol; it implements no chess rules.
- **WebSocket fan-out** (`app/websocket/router.py`) — the single
  authenticated `/ws` endpoint per user handles lobby subscription, room
  create/join/watch, and move/jump requests, and pushes lobby snapshots to
  every subscribed connection it holds in memory.
- **`/health`** (`app/health.py`, `app/main.py`) — reports Postgres and Redis
  reachability as a simple two-dependency check; it says nothing about
  `chess_server` reachability.

### `chess_server` (C++, `server/chess_server`)

A separate, manually-run TCP process. It owns the authoritative `GameEngine`:
move/jump legality, real-time piece cooldowns, captures, scores, snapshots,
and the wire protocol described in `docs/SERVER_BRIDGE.md`. It has no
knowledge of HTTP, WebSockets, users, sessions, or rooms — the gateway is the
only thing that talks to it, over one fixed TCP port, one instance, one
`GameEngine`.

### `server/cpp_gateway`

Present in the tree but **dormant on this branch**: it contains only a
`vcpkg_installed/` directory (fetched dependency artifacts), no `src/`,
`include/`, or `CMakeLists.txt` of its own. It is not built, not referenced
by `compose.yaml`, and not part of the active runtime path. The native C++
Gateway prototype it represents is paused and preserved on the separate
`stage3g-cpp-gateway` branch (see `docs/ARCHITECTURE.md`); nothing on this
branch depends on it or should be assumed to be active because the directory
exists.

### Where state actually lives

| Data | Where | Survives restart? |
|---|---|---|
| User accounts, password hashes | PostgreSQL (`users` table) | Yes |
| Session tokens | Redis (`ctd:session:<token>`, TTL) | Yes (until TTL) |
| Waiting/active rooms, hidden-room codes, spectator sets, lobby subscribers | In-memory in `api_gateway` (`RoomManager`) | No |
| Active match state, countdown timers, move-sequence ownership | In-memory in `api_gateway` (`MatchManager`) | No |
| Board state, move legality, scores | In-memory in `chess_server`'s `GameEngine` | No |

### `compose.yaml` services

`postgres`, `redis`, `api-gateway` (development), plus the `test` profile's
`postgres-test`, `redis-test`, `api-gateway-test` (tmpfs-backed, ephemeral,
used only for `pytest`). That's the complete list. There is no WebSocket
Gateway service, no Matchmaker service, no Game Allocator service, and no
message-bus service.

## Deviations from the proposed scalable design, and why

- **One `api_gateway` process handles both REST and WebSocket traffic — no
  separate WebSocket Gateway.** Splitting them adds a second process, a
  second deployment unit, and (per the point below) a message bus to keep
  them in sync, with no concrete scaling pressure yet to justify it. Today's
  single process handles both without contention.
- **One `chess_server` instance, no sharding, no Game Allocator.**
  `SingleMatchAllocator` enforces "one match at a time" as a matter of
  current architecture, not as a placeholder for future sharding logic. A
  second room activation is rejected outright with `match_unavailable`
  (`docs/SERVER_BRIDGE.md`). There is no matchmaking queue generating
  pressure for more than one concurrent match yet, so a real allocator
  routing matches across N shards doesn't have a problem to solve yet.
- **No NATS/Redis pub-sub between backend services.** Pub-sub exists to fan
  events out *between multiple processes*. With exactly one gateway process
  and one game-server process talking to each other over a direct TCP
  connection, there is nothing else to fan events out to — `RoomManager` and
  `MatchManager` push directly to the WebSocket connections they already
  hold in memory, in-process.
- **No Kubernetes / Agones.** These solve scheduling, health-managed
  restarts, and autoscaling *across multiple instances* of a service.
  Today there is exactly one instance of `api_gateway` and exactly one
  manually-started instance of `chess_server`; `docker compose` already
  covers "start these fixed containers with a restart policy," which is all
  that's needed at this scale.
- **No ELO or real matchmaking.** Room joining today is direct
  (create/join-by-id or join-by-code); there is no skill-based queue. This
  is explicitly called out as later-stage work in `docs/GAME_ROOMS.md`.
- **Rooms and match state are in-memory, not in Redis/Postgres.**
  `docs/GAME_ROOMS.md` already documents this as a known limitation: state
  is lost on restart and a second gateway instance would have its own
  separate lobby. Moving this to Redis is the prerequisite for running more
  than one `api_gateway` instance at all.

## What would change first if we needed to scale

1. **ELO + real matchmaking.** The most likely next stage: a matchmaking
   queue and rating system feeding into room/match creation, still against a
   single `chess_server`. This is orthogonal to the infrastructure changes
   below and doesn't require any of them.
2. **Multiple `chess_server` shards behind a real Game Allocator.** Once
   matchmaking can produce more concurrent matches than one `GameEngine` can
   hold, `SingleMatchAllocator` gets replaced by an allocator that tracks N
   shard processes and assigns rooms to whichever has capacity.
   `GameServerBridge`'s per-match TCP-connection model already generalizes
   to "connect to shard K" with a moderate change; the bigger new piece is
   the allocator's shard bookkeeping itself.
3. **A message bus (NATS or Redis pub-sub), only once there's more than one
   `api_gateway` process to coordinate.** Multiple gateway instances need a
   shared way to (a) keep `RoomManager`/lobby state consistent across
   instances and (b) route a player's WebSocket messages to whichever
   instance is talking to the right `chess_server` shard for their match.
   Introducing a bus before there's a second gateway instance would add
   operational complexity with nothing to coordinate.
4. **Kubernetes/Agones**, once there are enough shard and gateway instances
   that manual `docker compose` process management becomes the bottleneck,
   and once autoscaling or automated shard lifecycle management (Agones'
   specific niche for game-server fleets) is worth the operational cost.

## Logging and observability

- **Client**: `logs/client.log`, next to the executable — app lifecycle
  (start/window-created/main-loop/window-destroyed), auth attempt outcomes
  (username + result reason, never the password), connection state
  transitions, and room/match lifecycle events (create/join/watch, room id,
  match started/cancelled, opponent disconnected). See
  `client/desktop_client/include/Logging/FileLogger.hpp`.
- **Server**: `LOG_DIR/api_gateway.log` (default `/app/logs/api_gateway.log`
  inside the container, mounted via the `api_gateway_logs` Compose volume so
  it survives `docker compose restart`), rotating at 5 MB with 3 backups,
  alongside the existing console output that `docker compose logs` reads.
  Covers auth request outcomes, room create/join/watch/close, match
  allocate/start/finish/cancel, and unhandled exceptions. See
  `server/api_gateway/app/logging_config.py`.
- Neither log ever contains a plaintext password, `confirm_password`, a
  password hash, or a session token/cookie value.
- **`/health`** reports Postgres and Redis reachability only (see above) —
  it is a liveness/dependency check, not a metrics endpoint.
- **Metrics and tracing are not implemented.** There is no Prometheus
  endpoint, no structured metrics export, and no distributed tracing
  anywhere in this codebase today. The file logs above are the only
  observability this system has.
