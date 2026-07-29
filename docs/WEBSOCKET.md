# Stage 3C Authenticated WebSocket

Stage 3C adds an authenticated WebSocket foundation to the FastAPI API
Gateway. It does not carry chess moves and does not replace the existing C++
TCP transport.

## Authentication

Register and log in through the existing HTTP endpoints before opening the
WebSocket. Login sets the HttpOnly `ctd_session` cookie. Browser WebSocket
handshakes to the same host send this cookie automatically; JavaScript cannot
and should not read the cookie.

Local URL:

```text
ws://127.0.0.1:8000/ws
```

Production deployments behind HTTPS must use `wss://` and
`SESSION_COOKIE_SECURE=true`.

The gateway resolves the cookie through the existing Redis session store and
loads the user from PostgreSQL. It never trusts a `user_id` sent by the client.
Missing, invalid, or expired sessions are rejected before normal message
processing with WebSocket close code `4401`.

## Messages

After a successful handshake, the server sends:

```json
{
  "type": "connected",
  "user": {
    "id": "user UUID",
    "username": "player_one"
  }
}
```

Ping:

```json
{"type":"ping"}
```

Pong:

```json
{"type":"pong"}
```

Malformed JSON receives:

```json
{
  "type": "error",
  "code": "invalid_json",
  "message": "Message must be valid JSON"
}
```

Unknown message types receive:

```json
{
  "type": "error",
  "code": "unsupported_message_type",
  "message": "Message type is not supported"
}
```

Disconnecting the WebSocket does not log the user out or delete the Redis
session.

## Validation

Run the isolated API Gateway integration suite:

```powershell
docker compose config
docker compose --profile test build api-gateway-test
docker compose --profile test run --rm api-gateway-test pytest -q
```

The test profile uses only `postgres-test` and `redis-test`.

## Current limitations

Stage 3D adds in-memory lobby, room, color-assignment, and spectator metadata;
see `docs/GAME_ROOMS.md`. It still does not implement chess messages,
board state, or communication with the C++ chess server. The C++ desktop
client continues to use the existing realtime TCP protocol.
