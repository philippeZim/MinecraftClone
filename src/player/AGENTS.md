# player module

Free-fly FPS camera. **Input-agnostic**: the host fills `player_input` each frame, so
this never touches sokol or the window.

Public contract: `player.h`
- `player_init(start_pos)`
- `player_update(in, dt)` — `in` holds held-key flags + mouse delta.
- `player_view()` → view matrix; `player_eye()` → position.

Deps: `core` (hmath.h) only. State is a single `static` struct in the `.c`.

Likely next work: gravity + AABB-vs-voxel collision (will add a `world` dep), block
picking/raycast.
