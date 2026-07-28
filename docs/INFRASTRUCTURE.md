# CTD Local Infrastructure

Stage 3A provides PostgreSQL, Redis, and a minimal API Gateway for local
development. The API Gateway currently exposes only `GET /health`. The C++
GameEngine remains authoritative for every game rule.

## Prerequisite

Install Docker Desktop for Windows and start it before running these commands.
Run all commands from the project root in Windows PowerShell.

## Configure local values

```powershell
Copy-Item .env.example .env
```

Edit `.env` and replace the placeholder PostgreSQL password. The `.env` file
is ignored by Git and must not be committed.

## Validate and start

```powershell
docker compose config
docker compose up --build --detach
docker compose ps
```

Wait until `postgres`, `redis`, and `api-gateway` all report `healthy`.

## Call the health endpoint

```powershell
Invoke-RestMethod http://127.0.0.1:8000/health | ConvertTo-Json -Compress
```

Expected response:

```json
{"api_gateway":"healthy","postgresql":"healthy","redis":"healthy"}
```

If `API_GATEWAY_PORT` is changed in `.env`, use that port in the URL.

## View logs

```powershell
docker compose logs --follow
```

To view one service:

```powershell
docker compose logs --follow api-gateway
```

## Run API Gateway tests

```powershell
docker build --target test --tag ctd-api-gateway-test ./server/api_gateway
docker run --rm ctd-api-gateway-test
```

## Restart and verify persistence

```powershell
docker compose restart
docker compose ps
Invoke-RestMethod http://127.0.0.1:8000/health | ConvertTo-Json -Compress
```

## Stop without deleting persistent data

```powershell
docker compose down
```

Do not use `docker compose down -v` during normal testing. The `-v` option
deletes the named PostgreSQL and Redis volumes.
