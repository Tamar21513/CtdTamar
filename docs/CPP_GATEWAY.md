# Native C++ API and WebSocket Gateway

Stage 3G replaces the provisional Python runtime with a native C++ service.
The authoritative chess server remains separate and unchanged.

`GatewayApp` owns configuration, shared services, Asio workers, and shutdown.
`HttpServer` uses asynchronous Boost.Asio/Beast operations. `UserRepository`
uses libpqxx with the existing users table. `PasswordHasher` uses libsodium
Argon2id. `SessionStore` uses redis-plus-plus with `ctd:session:` keys and TTL.
`WebSocketHub` manages delivery, and the mutex-protected `RoomManager` owns
public/hidden rooms, deterministic colors, spectators, and cleanup.

The Stage 3B–3D endpoint, cookie, WebSocket, and lobby JSON shapes are
preserved. Identity comes from the `ctd_session` cookie, never client JSON.
Cookies use HttpOnly, SameSite=Lax, Path=/, configured Max-Age, and optional
Secure. Passwords, hashes, and raw tokens are never logged.

`migrations/001_create_users.sql` mirrors revision `20260729_01`. Startup uses
idempotent schema creation and preserves existing rows.

```powershell
C:\Users\User\vcpkg\vcpkg.exe install --triplet x64-windows
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target ctd_cpp_gateway
docker compose up --build -d
docker compose ps
Invoke-RestMethod http://127.0.0.1:8000/health
```

Compose runs `cpp-gateway`, PostgreSQL, and Redis; only localhost port 8000 is
published. This stage does not allocate game servers, route moves, or
synchronize boards.
