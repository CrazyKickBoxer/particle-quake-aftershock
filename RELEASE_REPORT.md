# Particle Quake: Aftershock — how it works

Particle Quake: Aftershock is a fork of vkQuake, the Vulkan-based Quake
source port. In its default mode it redraws every wall, floor, ceiling,
and monster in the game as a dense field of small colored points
instead of textured triangles — Quake played as if the whole level were
made of light. On top of that it adds two independent, optional
systems: a cosmetic GPU layer ("Holo Physics") that lets those points
visibly react to explosions, gunfire, and blood without touching
gameplay, and a separate CPU physics sandbox (PhysX + NVIDIA Blast)
that can genuinely fracture and remove wall sections so a rocket blast
opens a real, walkable hole. This report covers what's actually in the
code, not what was originally planned for it.

## How the point renderer actually works

It's simpler than a compute-driven pipeline, and it's worth being exact
about that rather than describing something more elaborate than what
ships. There is no per-point GPU culling pass, no storage buffer of
per-point structs, and no indirect draw for the base renderer. Instead:

Visibility is decided per *surface*, on the CPU, reusing the engine's
existing PVS/frustum bitmask (`aftershock.c`, `AS_DrawWorld`). Once a
surface is visible, every point on it goes out in one ordinary
instanced draw call — `vkCmdDrawIndexed` with six indices (one quad)
and an instance count equal to that surface's point count. There's no
per-point visibility test.

Each point is a single 4-byte value: a packed (u, v) texel coordinate
on that surface, at 1/8-texel precision (`src/aftershock_layout.h`,
`as_sample_t`). It rides a Vulkan *vertex* buffer, not a storage
buffer — fetched by fixed-function instanced vertex pulling, one
`uint32_t` per instance. Per-surface data (origin, texture axes,
lightmap info — 96 bytes, `as_surface_gpu_t`) is uploaded once per draw
call and shared by every point on that surface. Point density comes
from the `as_density` cvar (1–12 texels of spacing), tessellated per
surface with no global point cap; for e1m1 at the default "Play"
density the persistent point buffer measures 7,686,420 bytes with
529,536 bytes of surface metadata alongside it (`docs/VALIDATION.md`).
Results are cached to disk keyed by a hash of the geometry and density
setting, so re-opening a map doesn't re-tessellate it.

Holo Physics, the separate cosmetic layer, is where the
compute/storage-buffer/indirect-dispatch architecture actually lives.
It copies the same packed points into a real SSBO and runs 17 compute
shader variants (`local_size_x = 64`) over 12 buffers, including a
48-byte-per-particle sparse state buffer, a 16-byte immutable
home-position buffer, a 65,536-slot preallocated stain pool, and a
1,048,576-slot static hash grid (≈16 units/cell) built once from level
geometry for collision. A GPU pass tests every surface's bounds against
queued impulses and compacts the result into an indirect dispatch
before the "point promotion" pass runs — that's the one place in the
project where GPU-side compaction and `vkCmdDispatchIndirect` genuinely
happen, for three of the seventeen passes. It's gated entirely behind
`r_holo_physics`; with it off, the renderer above is all that's active.

## Styles

There are 14 material styles (`as_style`), from `faithful` (no
modification at all) through `enhanced`, `inferno`, and eleven more —
`living-stone`, `volcanic`, `sandstorm`, `crystal`, `corrupted-flesh`,
`industrial-rust`, `spectral`, `frozen-ruins`, `electric-grid`, and
`cosmic-dust`. Every one of them is a fragment-shader effect: a
palette recolor plus a slow, large-scale animated term — a traveling
ripple for `living-stone`, a breathing orange seam pattern for
`volcanic`, a moving green contour for `spectral`, and so on. None of
them move a vertex. A separate "Neon Cathedral" structure mode (cyan
and gold, magenta monsters) uses its own dedicated palette instead of
these 14.

## No animated noise on anchored points

Static world points never get time-based motion. Their jitter — small
positional and rotational offsets that make a wall look like grit
instead of a perfect grid — comes entirely from a hash of the point's
own fixed identity (its packed sample value combined with its surface
offset), never from the clock or the camera. The only thing that
*does* move a static point is Holo Physics actually claiming it for a
real displacement; everything else stays put. Time only ever drives
color animation in the fragment shader (the style effects above).

The reason is straightforward: a wall built from a million points, all
independently animated by a time-varying noise function, reads as
shimmering static rather than architecture — the eye can't parse
"wall" out of something that never holds still. Pinning position to a
stable per-point hash gives the wall a fixed identity to look at, and
lets time-based motion be reserved for effects that are supposed to
move (blast disturbance, style color animation). We didn't find
evidence in this codebase of an earlier animated-noise version that
got ripped out — no dead code, no reverted comments — so this is
presented as a design rule applied from the start, not a war story.

## Renderer-only means Holo Physics, specifically

Holo Physics is renderer-only in the strict sense: it never calls
`SV_Move` or `SV_TraceLine`, never touches the QuakeC interpreter, and
never reads or advances Quake's gameplay RNG — verified directly
against the source, not just documented. It observes already-decoded
network events (explosions, blood, teleports) read-only.

That constraint does **not** extend to the separate PhysX/Blast
destruction sandbox (`as_worldmode 1`). Fractured, detached wall chunks
are hooked directly into the engine's real per-entity trace function
(`AS_ClipDebris`, called from `world.c`), so fallen rubble genuinely
blocks movement and line-of-sight for the player and monsters — that's
a deliberate, load-bearing gameplay effect, not a rendering trick, and
it's why destruction mode is its own opt-in world mode rather than
always on.

## Performance

Measured on the reference machine (AMD Ryzen 5 7535HS, NVIDIA GeForce
RTX 2050, driver 596.36, Windows 11), 1920×1080 windowed, v-sync off,
Enhanced style / Mixed Debris Cloud structure / 3 layers / Play density
/ Cinematic destruction preset:

| Scenario | Host FPS | p95 frame time | GPU frame mean |
|---|---:|---:|---:|
| e1m1, actual rockets, Classic renderer | 56.7 | 19.4 ms | 6.9 ms |
| e1m1, actual rockets, Particle renderer | 54.6 | 25.9 ms | 16.4 ms |
| e1m2, synthetic explosion, Particle | 53.8 | 20.9 ms | 17.9 ms |
| e1m3, synthetic explosion, Particle | 60.0 | 16.9 ms | 5.3 ms |

These are engine host-frame measurements (`host_fps`), not measured
display presentation, and GPU numbers are whole-frame timestamps, not
isolated per-pass costs. Full methodology is in `docs/VALIDATION.md`.

## Known limitations

There's no GPU-side culling or indirect draw for the base point
renderer — every visible surface's points are submitted whether or not
they'd individually be occluded. Sky, liquids, sprites, and translucent
or extended models still render through vkQuake's original path, not
as points. The destruction sandbox's collision subtraction is regional,
not exact, and moving platforms/doors are excluded from the Holo
Physics collision grid. AMD/Intel GPUs haven't been tested — the
reference hardware above is the only machine this has been measured
on. See `docs/FEATURES.md` for the complete verified/implemented/
limited/not-started breakdown.
