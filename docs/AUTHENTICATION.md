# Stage 3B Authentication

Stage 3B adds server-side registration, login, authenticated-user lookup, and
logout to the FastAPI API Gateway. It does not connect the C++ client to
authentication. The existing realtime TCP transport is unchanged; WebSocket
transport belongs to a later stage.

## Environment variables

- `POSTGRES_DB`, `POSTGRES_USER`, `POSTGRES_PASSWORD`
- `POSTGRES_HOST`, `POSTGRES_PORT`
- `REDIS_HOST`, `REDIS_PORT`, `REDIS_DB`
- `SESSION_COOKIE_NAME` (default `ctd_session`)
- `SESSION_TTL_SECONDS` (default `3600`)
- `SESSION_COOKIE_SECURE` (`false` locally; use `true` with production HTTPS)
- `SESSION_COOKIE_SAMESITE` (default `lax`)

Copy the public template, then replace only local placeholder values:

```powershell
Copy-Item .env.example .env
```

## Username and password policy

Usernames are normalized with Unicode NFKC normalization and Unicode-aware
case folding for lookup and uniqueness. Surrounding whitespace is removed from
the stored display name. Usernames must contain 3–32 Unicode letters or
numbers, with `_`, `-`, and `.` also allowed. Control characters and other
punctuation are rejected.

Passwords must contain 10–128 characters and cannot be whitespace-only.
Characters are not silently trimmed. Passwords are hashed with Argon2id using
`argon2-cffi`; plaintext passwords are never stored.

## Migrations

```powershell
docker compose run --rm api-gateway alembic upgrade head
```

When the service is running:

```powershell
docker compose exec api-gateway alembic upgrade head
```

## Authentication flow

Register:

```powershell
Invoke-RestMethod `
  -Method Post `
  -Uri http://127.0.0.1:8000/auth/register `
  -ContentType "application/json" `
  -Body '{"username":"player_one","password":"StrongPassword123!"}'
```

Login and retain the cookie:

```powershell
$session = New-Object Microsoft.PowerShell.Commands.WebRequestSession
Invoke-RestMethod `
  -Method Post `
  -Uri http://127.0.0.1:8000/auth/login `
  -ContentType "application/json" `
  -Body '{"username":"player_one","password":"StrongPassword123!"}' `
  -WebSession $session
```

Current user:

```powershell
Invoke-RestMethod `
  -Method Get `
  -Uri http://127.0.0.1:8000/auth/me `
  -WebSession $session
```

Logout:

```powershell
Invoke-RestMethod `
  -Method Post `
  -Uri http://127.0.0.1:8000/auth/logout `
  -WebSession $session
```

The session token exists only in an HttpOnly cookie. Redis stores a small JSON
record under `ctd:session:<token>` with an atomic TTL. Tokens are generated
cryptographically and are never returned in JSON or logged.

Each register/login/logout request is logged (console and the rotating file
at `LOG_DIR`, default `/app/logs/api_gateway.log`) with only the username and
outcome, e.g. `auth_login username=player_one outcome=success` or
`outcome=invalid_credentials`. The password, `confirm_password`, the Argon2id
hash, and the session token/cookie value are never written to this log or
any other.

Login returns the same `401 Invalid username or password` response for an
unknown username, wrong password, or malformed stored hash. `/auth/me` returns
`401 Not authenticated` for all invalid or expired sessions. Logout is
idempotent and always returns `204`.

## Tests and TTL inspection

```powershell
docker compose --profile test run --rm api-gateway-test pytest -q
docker compose exec redis redis-cli --scan --pattern "ctd:session:*"
```

Use the returned key directly inside Redis CLI when checking `TTL`, but do not
copy the full token into logs or reports.
