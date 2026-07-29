# Stage 3F Native Desktop Authentication and Lobby UI

Stage 3F adds authentication and lobby screens to the existing C++/OpenCV
desktop client. It uses the Stage 3E HTTP/WebSocket transport and the Stage 3D
room protocol. It does not send chess moves through WebSocket and does not
pretend that a lobby room has an allocated Game Server.

## Run modes

The native lobby is now the default:

```powershell
.\build\client\desktop_client\Release\ctd_client.exe
```

The existing visual chess client remains available explicitly:

```powershell
.\build\client\desktop_client\Release\ctd_client.exe game
.\build\client\desktop_client\Release\ctd_client.exe game 127.0.0.1 5000
```

The existing text client remains available with `text`.

## UI architecture

- `LobbyApp` owns the OpenCV window and event loop. It polls network events,
  dispatches keyboard and mouse input, renders one complete off-screen frame,
  and calls `imshow` once per iteration.
- `LobbyApplicationState` owns the explicit screen, modal, pending action, and
  UI-facing room data.
- `LobbyController` converts typed actions into `LobbyClient` operations,
  performs authentication off the UI thread, consumes typed events on the UI
  thread, and clears transient password input.
- `LobbyLayout` calculates all major rectangles, card columns, card sizes, and
  hit targets from the current desktop window size.
- `LobbyInputMapper` maps clicks to typed actions. It does not perform network
  requests.
- `LobbyRenderer` receives a read-only `LobbyViewModel` and returns a
  `cv::Mat`. It has no access to passwords, cookies, sockets, GameEngine, or
  mutable room state.
- `LobbyTransport` is a small testable adapter around the Stage 3E
  `LobbyClient`.

The authoritative C++ `GameEngine` remains the only source of chess rules.
The API Gateway and desktop UI do not decide move legality.

## Screen states

The application uses one `LobbyScreen` enum:

- `Authentication`
- `Connecting`
- `Lobby`
- `WaitingRoom`
- `HiddenRoomWaiting`
- `RoomReady`
- `SpectatorPlaceholder`
- `Error`

Modals and pending actions also use enums. Disconnected state disables
network-dependent actions instead of pretending an operation succeeded.

## Text input

Text input reuses the existing OpenCV `waitKeyEx` approach. This is the
smallest solution already present in the project and works on both Windows and
Linux without adding WinAPI code or another UI framework.

The controller owns the transient password buffer. The view model exposes only
its length for masking. The buffer is overwritten and cleared after submission
or logout. Passwords and the session cookie are never rendered or logged. The
session cookie remains private to `SessionCookieJar`.

## Layout

The UI is desktop-first:

- minimum usable render size: approximately `1200x700`
- recommended size: `1366x768` or larger
- three card columns at 1200 and 1366 pixels
- four card columns from 1600 pixels
- one visible card row per section, with bounded wheel paging

The header, waiting section, live section, authentication panel, modals,
buttons, and card grids are calculated centrally. Room UUIDs are retained only
as action identifiers and are not used as card titles.

## Authentication flow

Login and registration are separate actions. Registration performs the
explicit register request and then login; a failed login never creates an
account.

1. The user enters username and password.
2. The controller starts the selected REST operation off the rendering thread.
3. Login verifies identity through `/auth/me` in `LobbyClient`.
4. The authenticated WebSocket connects with the internal cookie.
5. The client waits for `connected`.
6. It sends `subscribe_lobby`.
7. The first `lobby_snapshot` opens the Lobby screen.

Structured failures are mapped to safe user messages. Raw server responses,
stack traces, passwords, and tokens are not shown.

## Room and spectator flow

Public room creation sends `create_room` with `public`. The waiting screen
does not optimistically start a match.

Hidden room creation sends `create_room` with `hidden` and shows the returned
six-character invitation code only to its creator. Hidden waiting rooms never
come from the public snapshot and therefore cannot appear in the public grid.

Joining public rooms uses the internal room UUID. Joining hidden rooms
normalizes and validates the invitation code. Pending actions block duplicate
clicks.

Active room cards show white, black, `LIVE`, spectator count, and Watch.
`watching_game` opens an honest spectator placeholder stating that board
streaming is not connected.

`game_started` stores the room, assigned color, and opponent, then opens a
Room Ready placeholder. A later Game Allocator/Game Server bridge must connect
that room to an authoritative engine.

## WebSocket events

The UI consumes typed Stage 3E events:

- `connected`
- `pong`
- `lobby_snapshot`
- `room_created`
- `game_started`
- `watching_game`
- `opponent_disconnected`
- `room_status`
- structured `error`

A snapshot replaces the previous waiting and active lists. Network worker
threads never call OpenCV.

## Local security limitation

The Docker development environment uses `http://127.0.0.1:8000` and
`ws://127.0.0.1:8000/ws`. These connections are not encrypted. Production
must add certificate validation and use HTTPS/WSS.

## Manual validation

Start infrastructure:

```powershell
docker compose up -d
docker compose ps
```

Start three native clients:

```powershell
.\build\client\desktop_client\Release\ctd_client.exe
```

Checklist:

1. Register and log in as three isolated test users.
2. Confirm an invalid login remains on Authentication with a safe error.
3. With client A, create a public room.
4. With client B, confirm its card appears and join it.
5. Confirm A receives white, B receives black, and both see Room Ready.
6. Confirm the active room appears under Live Rooms.
7. Create a hidden room and confirm it is absent from public waiting cards.
8. Join it from another client using the six-character code.
9. With client C, watch the active room and confirm the spectator placeholder
   and updated spectator count.
10. Verify empty states, logout, disconnected Gateway state, waiting-host
    cleanup, and opponent-disconnected messaging.
11. Resize to approximately 1200x700, 1366x768, and a wider desktop.
12. Run `ctd_client.exe game` and confirm the existing chess visuals still
    open against the C++ server.

## Limitations

Stage 3F does not implement board streaming, gameplay over WebSocket, room
persistence, match reconnection, Game Allocator, C++ Game Server bridging,
NATS, Redis PubSub, Kubernetes, ratings, history, or fake move simulation.
