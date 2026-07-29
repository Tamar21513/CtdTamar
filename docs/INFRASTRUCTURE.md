# CTD Local Infrastructure

Docker Compose provides PostgreSQL, Redis, and the FastAPI API Gateway.
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
docker compose run --rm api-gateway alembic upgrade head
docker compose up -d api-gateway
docker compose ps
```

For an already-running API Gateway, migrations can also be applied with:

```powershell
docker compose exec api-gateway alembic upgrade head
```

Migrations are explicit. The application does not create or drop tables at
startup.

## Health

```powershell
Invoke-RestMethod http://127.0.0.1:8000/health |
    ConvertTo-Json -Depth 5
```

Expected healthy response:

```json
{"api_gateway":"healthy","postgresql":"healthy","redis":"healthy"}
```

## Isolated integration tests

The test profile uses temporary `postgres-test` and `redis-test` services; it
does not delete data from the normal development services.

```powershell
docker compose --profile test run --rm api-gateway-test pytest -q
docker compose --profile test down
```

Do not use `docker compose down -v` during normal development because it
deletes persistent PostgreSQL and Redis volumes.

## Logs and shutdown

```powershell
docker compose logs --follow api-gateway
docker compose down
```
