# Block textures

Real 16×16 block textures loaded at startup. Each `*.png` here is git-ignored.
The engine loads each file if present and otherwise falls back to a procedural tile, so the
game always runs. Loaded textures are gently lightened (`LIGHTEN` in `src/render/render.c`,
default `0.10`; set `0` for the textures untouched) and can be tinted via `TINT`.

## Files in use

| File         | Block               | Source file                          |
|--------------|---------------------|--------------------------------------|
| `grass.png`  | grass (top face)    | `default_grass.png`                  |
| `dirt.png`   | dirt + grass sides  | `default_dirt.png`                   |
| `stone.png`  | stone               | `default_stone.png`                  |
| `sand.png`   | sand                | `default_sand.png`                   |
| `water.png`  | water (translucent) | `default_water_source_animated.png`  |
| `wood.png`   | tree trunk          | `default_tree.png`                   |
| `leaves.png` | leaves              | `default_leaves.png`                 |

Non-16² PNGs are nearest-resampled; animated strips (e.g. water) use their first frame.

## Credit / licence

These textures are from **Minetest Game** (https://github.com/minetest/minetest_game),
licensed **CC BY-SA 3.0**. Keep this attribution if you redistribute the project with them.
To use different textures (including your own Minecraft asset copy), just replace the PNGs
above with matching filenames.
