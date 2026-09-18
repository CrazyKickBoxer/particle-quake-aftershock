# Particle Quake: Aftershock — Console Variable Reference

This covers every console variable this fork adds on top of stock vkQuake.
There are two independent systems:

- **`as_*`** — the point-cloud renderer, particle "structures," material
  styles, gore, and the optional PhysX/Blast destruction sandbox.
- **`r_holo` / `r_holo_phys_*` / `holo_physics*`** — "Holo Physics," a
  separate, purely cosmetic GPU disturbance layer that reacts to combat
  visually without touching gameplay, collision, or the destruction
  sandbox below. See `docs/HOLO_PHYSICS.md` for how it's built.

**Note on the destruction sandbox:** `as_worldmode 1` is not purely
cosmetic. Destroyed geometry is clipped against by the engine's real
movement/trace code (`AS_ClipDebris`, hooked into `world.c`), so fallen
rubble can physically block a real player or monster. Only the Holo
Physics layer below is renderer-only in the strict sense (no `SV_Move`,
no `SV_TraceLine`, no RNG, no QuakeC).

## Renderer, particle structure & destruction sandbox (`as_*`)

| cvar | default | range / values | description |
|---|---|---|---|
| `as_renderer` | `0` | 0=classic, 1=particle | Selects the renderer. Mirrored with `r_holo`. |
| `as_worldmode` | `0` | 0=faithful, 1=destruction (read-only; use `as_mode`) | Faithful Quake vs. the PhysX/Blast destruction sandbox. |
| `as_density` | `4` | 1–12 | Surface sample spacing; lower = denser. Needs a map reload. |
| `as_style` | `1` | 0–13, see style table below | Material/color treatment for structure particles. |
| `as_radius` | `130` | 1–1024 | Destruction blast radius (Quake units). |
| `as_damage` | `6` | 0–100 | Destruction damage value (independent of Holo Physics impulses). |
| `as_progressive` | `1` | boolean | Multi-hit progressive wall damage vs. one-shot panel removal. |
| `as_rocket_hits` | `4` | 2–12 | Nominal direct-hit toughness for progressive destruction. |
| `as_chip_radius` | `72` | 24–160 | Local damage radius for one progressive-destruction hit. |
| `as_effects` | `1` | boolean | Master switch for the combat effect layer (particles + gore). |
| `as_shake` | `0.35` | 0–2 | Camera-kick strength near explosions. |
| `as_reduced_flashes` | `0` | boolean | Freezes animated surface highlights for a calmer look. |
| `as_particle_amount` | `2` | 1–4 | Transient burst-particle count: 2048/4096/6144/8192 per explosion. |
| `as_structure` | `11` | 0–11, see structure table below | Particle-structure geometry replacing flat surface samples. |
| `as_fidelity` | `1` | boolean | Enhanced HDR/bloom/temporal/SSR post pipeline (Neon Cathedral). |
| `as_reflections` | `1` | boolean | Screen-space floor reflections (needs `as_fidelity` + Neon Cathedral). |
| `as_neon_prism` | `0` | boolean | Neon Prism cosmetic tuning switch; normally set via `neon_prism`. |
| `as_reflection_strength` | `1` | 0–4 | Reflected light intensity. |
| `as_reflection_roughness` | `0.20` | 0.03–0.7 | Reflection blur/roughness; lower = sharper. |
| `as_neon_glow` | `1` | 0–2 | Luminous halo/glow intensity. |
| `as_neon_npc_texture` | `1` | boolean | Tints each monster's points from its own MDL skin instead of one flat magenta silhouette, so the point cloud maps the texture. Requires Neon Cathedral; weapons and pickups stay cyan. |
| `as_npc_solid` | `1` | boolean | Monsters draw as their normal textured model while alive and only become a point cloud from the first death-animation frame, so death reads as the body bursting apart. Detected read-only from the animation frame name, the same test the death dissolve uses. 0 keeps monsters as a point cloud at all times. The outward burst itself comes from `r_holo_phys_death`, so without Holo Physics a dying monster turns to points without the blast. |
| `as_npc_sat` | `1.6` | 0–4 | Saturation push for skin-tinted monster splats. 1 = the skin's own saturation, 0 = greyscale. Needs `as_neon_npc_texture 1`. |
| `as_npc_lift` | `0.85` | 0.2–1 | Value curve for skin-tinted splats. 1 = linear (maximum contrast); lower lifts dark skins out of mud but flattens light/dark detail. |
| `as_npc_gain` | `0.85` | 0–4 | Overall exposure for skin-tinted splats, applied *instead of* the fidelity ×3, so it means the same thing with `as_fidelity` on or off. Above ~1.3 bright texels start clipping to white and detail flattens. |
| `as_npc_detail` | `2.5` | 1–6 | Splat spacing on monsters/models, in Quake units. Lower = denser point cloud = more skin detail, at a **quadratic** cost in samples. Baked at model load, so it needs a map reload. Past the 250k-per-model cap a model silently falls back to classic triangles — watch for `Aftershock: model sample limit` in the console. |
| `as_layers` | `3` | 1–4 | Independently jittered depth layers of surface particles. |
| `as_nails` | `1` | boolean | Cosmetic nail-impact ricochets/scars (visual only). |
| `as_smw` | `0` | boolean | Super Mario World post-process: chunky pixel cells, 15-bit (32,768 colour) quantisation and hard black tile borders on real edges. Keeps the full gamut - it grades colour rather than snapping to a fixed palette. Works over either renderer. |
| `as_smw_pixel` | `4` | 1–16 | Pixel cell size for `as_smw`, in screen pixels. Larger = chunkier. |
| `as_smw_outline` | `1` | 0–3 | Black tile-border thickness for `as_smw`, in screen pixels. 0 disables borders. |
| `as_smw_edge` | `0.10` | 0–1 | How different two neighbouring cells must look before a black border is drawn. Lower outlines more detail; 0 disables borders. |
| `as_smw_saturate` | `1.30` | 0–3 | Colour punch for `as_smw`. 1 keeps the scene's own colours, 0 is greyscale, higher pushes cartoon vibrancy. |
| `as_gibs` | `1` | boolean | Directional gib motion/simulation (0 = stock gib rendering). |
| `as_gib_particles` | `1` | boolean | A killed enemy comes apart into physics particles that explode outward, collide with the world and bounce, instead of throwing large gib meshes. The gib bodies still simulate, so blood trails and decals are unchanged; only the chunky mesh is dropped. The burst comes from `r_holo_phys_death`, and bounciness from `r_holo_phys_restitution`. |
| `as_goo` | `1` | boolean | Surface blood/goo splash decals and slide trails. |

### `as_style` — 14 material styles (`aftershock.frag`)

`faithful, enhanced, inferno, inferno-color, living-stone, volcanic, sandstorm, crystal, corrupted-flesh, industrial-rust, spectral, frozen-ruins, electric-grid, cosmic-dust`

Disabled in the UI when `as_structure` is 11 (Neon Cathedral), which uses its own palette. All of these are fragment-shader color/brightness effects only — none of them move a vertex.

### `as_structure` — particle structure geometry

| Value | Structure |
|---:|---|
| 0 | Original square samples |
| 1 | Soft Dust Cloud |
| 2 | Solid Beads |
| 3 | Broken Stone Chips |
| 4 | Scattered Microvoxels |
| 5 | Layered Flakes |
| 6 | Splinter Field |
| 7 | Fiber Bundles |
| 8 | Goo Blobs |
| 9 | Hollow Rings |
| 10 | Mixed Debris Cloud |
| 11 | Neon Cathedral (reference preset, own palette) |

## Command-line launch flags

| Flag | Cvar(s) set | Values |
|---|---|---|
| `-renderer <name>` | `as_renderer` | `classic` \| `particle` |
| `-worldmode <name>` | `as_worldmode` | `faithful` \| `destruction` |
| `-style <name>` | `as_style` | one of the 14 style names |
| `-density <name>` | `as_density` | `play`→4, `fine`→3, `showcase`→2 |
| `-physics <name>` | validated only | `off` \| `physx-cpu` (`physx-gpu` is rejected) |
| `-destruction-preset <name>` | `as_radius`, `as_damage` | `restrained`→90/3, `cinematic`→130/6, `cataclysm`→170/9 |

## Console commands (renderer/destruction)

- `as_explode` — diagnostic explosion along the view direction
- `as_inspect` — eligible panels, bodies, and physics diagnostics
- `as_reset` — reload the current map
- `as_mode faithful|destruction` — change world mode and reload
- `neon_prism [0]` — preset: enables Neon Cathedral + fidelity + reflections together
- `as_gore_stats` — prints gib/goo pool counters

## Holo Physics (`r_holo*`, cosmetic GPU disturbance layer)

| cvar | default | range | description |
|---|---|---|---|
| `r_holo` | `0` | boolean | On/off alias for the particle renderer (mirrors `as_renderer`). |
| `r_holo_physics` | `0` | boolean | Master enable for the disturbance layer. Requires `as_renderer 1`. |
| `r_holo_phys_budget` | `65536` | 1024–262144 | Max active/displaced points. Rebuilds GPU resources when changed. |
| `r_holo_phys_settle` | `1.2` | 0.1–5 | Spring-return/settle time (seconds). |
| `r_holo_phys_blast` | `1` | 0–8 | Outward shockwave force from explosions. |
| `r_holo_phys_blast_radius` | `1` | 0.1–4 | Visual blast-disturbance radius scale. |
| `r_holo_phys_suction` | `0.35` | 0–1 | Inward pulse strength after a blast. |
| `r_holo_phys_turbulence` | `0.6` | 0–8 | Slow wind-force strength on free points. |
| `r_holo_phys_turbulence_scale` | `48` | 8–512 | Spatial scale of the turbulence field. |
| `r_holo_phys_debug` | `0` | boolean | On-screen Holo debug overlay. |
| `r_holo_phys_spray` | `1` | 0–8 | Strength of bullet/spike impact sprays. |
| `r_holo_phys_spray_count` | `96` | 0–200 | Max points per small impact spray. |
| `r_holo_phys_collide` | `1` | boolean | Free-particle collision against the static-world hash grid. |
| `r_holo_phys_restitution` | `0.45` | 0–1 | Bounce restitution for colliding particles, including gib/gore/death particles. 0 makes a particle settle where it first hits, which is how gore behaved before. |
| `r_holo_phys_beam` | `1` | 0–8 | Disturbance strength around lightning/beam effects. |
| `r_holo_phys_beam_radius` | `24` | 4–128 | Beam disturbance radius (Quake units). |
| `r_holo_phys_wake` | `0.7` | 0–4 | Sideways parting-wake from projectiles/gibs. |
| `r_holo_phys_player_wake` | `0.5` | 0–4 | Floor-point scatter from player movement. |
| `r_holo_phys_liquid` | `1` | 0–8 | Lava/liquid-surface splash disturbance. |
| `r_holo_phys_teleport` | `1` | 0–8 | Teleport implosion/release strength. |
| `r_holo_phys_dust` | `0.8` | 0–2 | Sparse falling ceiling-dust strength above explosions. |
| `r_holo_phys_shake` | `0.4` | 0–1 | Local point-field shudder from combat. |
| `r_holo_phys_debris_light` | `1` | boolean | Bright debris particles borrow dynamic-light slots (2, or 4 at Showcase). |
| `r_holo_phys_gore` | `1` | boolean | "Stuck" gore points that adhere and fade instead of springing home. |
| `r_holo_phys_gore_max` | `20000` | 0–65536 | Max persistent/stuck gore points. |
| `r_holo_phys_gore_life` | `45` | 1–90 | Seconds stuck blood remains visible while fading. |
| `r_holo_phys_gore_gravity` | `1` | 0–4 | Gravity multiplier on detached blood points. |
| `r_holo_phys_death` | `1.6` | 0–8 | Outward speed of the particle burst a monster comes apart into when killed. |
| `r_holo_phys_death_life` | `3` | 0.2–4 | Seconds death particles live. They retire early once they settle, so this is an upper bound, not a fixed duration. |

### Holo Physics presets

| Preset | Budget | Gore cap | Settle | Dust |
|---|---:|---:|---:|---:|
| `play` | 16384 | 4096 | 1.0 | 0 |
| `fine` (default) | 65536 | 20000 | 1.2 | 0.8 |
| `showcase` | 131072 | 20000 | 1.8 | 0.8 |

Max active budget is 262144; minimum is 1024.

### Console commands (Holo Physics)

- `holo_physics [play|fine|showcase]` — apply a preset and enable the layer
- `holo_physics_off` — disable the layer
- `holo_physics_reset` — reset tuning cvars to defaults (leaves on/off state alone)
- `holo_physics_sphere [radius]` — fire a debug disturbance event in front of the player (default radius 160)
- `menu_holo_physics` — opens the in-engine "Reactive Physics" settings menu
