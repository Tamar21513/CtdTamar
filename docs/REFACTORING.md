# Project structure

The project is separated by responsibility:

- `Core`: domain values and board state.
- `Rules`: movement and validation rules.
- `Realtime`: time progression, motion steps, and jump events.
- `Engine`: game actions, captures, scoring, promotion, and history.
- `Control`: input interpretation and selection state.
- `IO`: board parsing, printing, and coordinate mapping.
- `Graphics`: animation snapshots, sprites, effects, and rendering.
- `App`: console and visual application loops.

Large application, controller, snapshot, and rendering routines were split into focused helper functions. Every source function has a short English comment above its definition. Project-owned source, test, and documentation text contains no Hebrew.
