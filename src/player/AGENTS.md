# player module

FPS controller: walking with gravity + jump + AABB-vs-voxel collision, plus a creative
fly toggle. **Input-agnostic**: host fills `player_input` each frame.

Public contract: `player.h`
- `player_init(feet_start)`, `player_update(in, dt)`, `player_toggle_fly()`.
- `player_view()` → view matrix; `player_eye()` / `player_forward()` for the camera + raycast.

Deps: `core` (hmath) + `world` (collision via `world_solid`). State is one `static` struct.
Collision is per-axis swept AABB (revert axis on overlap); tunables are `#define`s at the top.

Likely next work: step-up onto single blocks, swimming, variable eye height (crouch).
