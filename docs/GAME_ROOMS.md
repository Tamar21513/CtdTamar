# Stage 3D Game Rooms and Lobby

Stage 3G ports this behavior to the mutex-protected native C++ `RoomManager`
without changing the validated message shapes.

Stage 3D provides backend-only lobby and game-room state through the
authenticated `/ws` endpoint. It does not add a desktop lobby UI or connect
rooms to the C++ chess server.

## Public waiting rooms

Create a public room:

```json
{"type":"create_room","visibility":"public"}
```

The response contains `room_created` with the room UUID, waiting status, and
host identity. Public waiting rooms appear in `waiting_rooms` lobby snapshots.
A different authenticated user joins with:

```json
{"type":"join_room","room_id":"room UUID"}
```

## Hidden waiting rooms

Create a hidden room:

```json
{"type":"create_room","visibility":"hidden"}
```

The creator receives a six-character room code using characters chosen to
avoid common visual ambiguity. The code is an invitation locator, not an
authentication credential. Hidden waiting rooms and their codes never appear
in public lobby snapshots.

Join by code:

```json
{"type":"join_hidden_room","room_code":"ABC234"}
```

Codes are case-insensitive and become invalid when used or when the waiting
host disconnects.

## Game start and colors

The second player activates the room. The host is always white and the guest
is always black. Both receive:

```json
{
  "type": "game_started",
  "room_id": "room UUID",
  "color": "white",
  "opponent": {
    "id": "opponent UUID",
    "username": "opponent name"
  }
}
```

After activation, both public and formerly hidden rooms appear under
`active_games`.

## Lobby snapshots and live updates

Request a snapshot once:

```json
{"type":"get_lobby"}
```

Subscribe to updates:

```json
{"type":"subscribe_lobby"}
```

Subscription immediately returns a `lobby_snapshot`. A fresh full snapshot is
sent whenever visible state changes: a public room is created or removed, a
room activates, an active room is removed, or the spectator count changes.
Creating a hidden waiting room does not emit a lobby update.

Snapshots expose only public user and room information. They never expose
cookies, Redis keys, WebSocket objects, or hidden waiting-room codes.

## Spectators

Watch an active game:

```json
{"type":"watch_game","room_id":"room UUID"}
```

The `watching_game` response includes white, black, and the current spectator
count. Waiting rooms cannot be watched, players cannot spectate their own
game, and the same connection cannot be registered twice as a spectator.
No board snapshot or gameplay stream exists yet.

## Disconnect policy

- A waiting host disconnect removes the room immediately.
- A hidden room code becomes invalid when its room is removed.
- An active player disconnect removes the room immediately.
- The opponent receives `opponent_disconnected`.
- Spectators receive a `room_status` event with status `removed`.
- A spectator disconnect removes only that spectator and updates the count.
- Room cleanup never deletes an authentication session.
- Reconnection is outside Stage 3D.

## Duplicate participation

A user may host or play in only one room at a time. A host cannot join their
own room, and an active player cannot occupy another seat. Stable structured
errors include `already_in_room`, `cannot_join_own_room`,
`room_unavailable`, and `already_watching`.

## In-memory limitation

Rooms, subscriptions, and spectator state live only inside one API Gateway
process. They are lost on restart, and multiple Gateway instances would have
separate lobbies. Shared Redis or database room state is future work.

## Stage 3E desktop assumptions

The future lobby UI is desktop-first, with a minimum viewport around
1200x700 and a recommended viewport of 1366x768 or larger. It will render a
responsive grid of game cards rather than a numbered text list.

Public waiting cards can show host, waiting status, creation time, and a
`Join Game` action. Active cards can show white, black, live status,
spectator count, and `Watch Live`. The UI will also provide public/hidden
creation and join-by-code actions. No temporary HTML or desktop UI is included
in this stage.

## Non-goals

Stage 3D does not implement chess moves, board state, C++ integration,
gameplay transport, resign, rematch, chat, history, persistent rooms,
Redis-backed lobby state, multi-instance scaling, or UI.

## Validation

```powershell
docker compose config --quiet
ctest --test-dir build -C Release --output-on-failure
```
