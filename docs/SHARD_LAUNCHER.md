# Dynamic game server shards

`app/matches/allocator.py`'s `ShardPoolAllocator` first tries the static
shard list (`CTD_GAME_SERVER_HOST`/`PORT` plus `CTD_GAME_SERVER_SHARDS`).
Once every configured shard is busy, it can optionally ask a small,
separate, native Windows process — the **shard launcher** — to start a
brand new `ctd_server.exe` instance on an unused port, up to
`CTD_MAX_DYNAMIC_SHARDS` (default 50) at a time. That cap exists purely as
a safety net against a bug spawning unbounded processes; it is not meant
to be a meaningful limit on concurrent games.

The launcher must run natively on Windows (not in Docker) because
`ctd_server.exe` is a Windows binary and the Gateway's container is Linux.

## Running it

```powershell
python server\shard_launcher\launcher.py
```

Leave this window open alongside Docker Desktop and any manually-started
`ctd_server.exe` shards. Optional environment variables (set before
running, or export them in the same shell):

- `CTD_SHARD_LAUNCHER_PORT` (default `5100`) — must match
  `CTD_SHARD_LAUNCHER_PORT` in the Gateway's `.env`.
- `CTD_SERVER_EXE_PATH` (default
  `build\server\chess_server\Release\ctd_server.exe`) — path to the built
  binary it should launch.

## Enabling it in the Gateway

In `.env`:
```
CTD_SHARD_LAUNCHER_HOST=host.docker.internal
CTD_SHARD_LAUNCHER_PORT=5100
CTD_MAX_DYNAMIC_SHARDS=50
```
Then `docker compose down && docker compose up -d --build` to pick up the
change. Leaving `CTD_SHARD_LAUNCHER_HOST` empty (the default) disables
this entirely — only the manually pre-started shards are used, exactly
like before this feature existed.

## What this is not

This is a fixed-machine, single-host mechanism — it does not health-check
or replace a shard whose process crashes mid-match (that match simply
ends the way any disconnect does today), and it has nothing to do with
container orchestration. The originally proposed design's real answer to
elastic scaling is Kubernetes + Agones, which is a separate, much larger
undertaking (see `C10`/`C11` in the project tracker) requiring an actual
cluster and a Linux build of `chess_server` — out of scope here.
