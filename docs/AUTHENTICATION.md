# Stage 3B Authentication

Registration, login, authenticated-user lookup, and logout are implemented by
the native C++ Gateway. The authoritative realtime TCP transport is unchanged.

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

Surrounding ASCII whitespace is removed from usernames. Usernames must contain
3–32 ASCII letters or numbers, with `_`, `-`, and `.` also allowed. Lookup and
uniqueness are ASCII case-insensitive.

Passwords must contain 10–128 characters and cannot be whitespace-only.
Characters are not silently trimmed. Passwords are hashed with libsodium
Argon2id; plaintext passwords are never stored.

## Migrations

The C++ Gateway applies the idempotent schema in
`server/cpp_gateway/migrations/001_create_users.sql` at startup. It reuses the
existing `users` table and never drops data.

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

Login returns the same `401 Invalid username or password` response for an
unknown username, wrong password, or malformed stored hash. `/auth/me` returns
`401 Not authenticated` for all invalid or expired sessions. Logout is
idempotent and always returns `204`.

## Tests and TTL inspection

```powershell
ctest --test-dir build -C Release --output-on-failure
docker compose exec redis redis-cli --scan --pattern "ctd:session:*"
```

Use the returned key directly inside Redis CLI when checking `TTL`, but do not
copy the full token into logs or reports.
