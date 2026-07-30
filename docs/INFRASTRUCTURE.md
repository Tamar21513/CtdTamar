# CTD Local Infrastructure

Docker Compose provides PostgreSQL, Redis, and the native C++ API Gateway.
PostgreSQL stores users; Redis stores expiring authentication sessions.
The C++ chess server remains authoritative for all game rules.

## Configure local values

Run from the repository root in Windows PowerShell:

```powershell
Copy-Item .env.example .env
```

Replace the placeholder PostgreSQL password in `.env`. Never commit `.env`.

## Build and start

```powershell
docker compose config
docker compose build --no-cache
docker compose up -d postgres redis
docker compose up -d cpp-gateway
docker compose ps
```

The Gateway applies its idempotent users-table migration at startup. It does
not drop tables or data.

## Health

```powershell
Invoke-RestMethod http://127.0.0.1:8000/health |
    ConvertTo-Json -Depth 5
```

Expected healthy response:

```json
{"cpp_gateway":"healthy","postgresql":"healthy","redis":"healthy"}
```

## C++ tests

```powershell
ctest --test-dir build -C Release --output-on-failure
```

Do not use `docker compose down -v` during normal development because it
deletes persistent PostgreSQL and Redis volumes.

## Logs and shutdown

```powershell
docker compose logs --follow cpp-gateway
docker compose down
```
