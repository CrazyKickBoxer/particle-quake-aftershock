# Holo Physics

Holo Physics is a renderer-owned GPU disturbance layer in `bin/vkQuake.exe`.
It is independent of the older CPU/PhysX destruction system.

## Enable and control

```console
holo_physics fine
```

This enables holographic rendering and applies only Holo Physics defaults.
It preserves unrelated graphics settings, including the selected material and
particle structure. `play` and `showcase` are also accepted.

```console
r_holo 1
r_holo_physics 1
menu_holo_physics
holo_physics_sphere 160
r_holo_phys_debug 1
```

The menu is also under **Options → Graphics → Reactive Physics**.
`holo_physics_off` disables the layer; `r_holo 0` selects the stock renderer.
`holo_physics_reset` resets this layer's tuning without changing its enabled
state or unrelated settings. Physics defaults to off until enabled.

## Effects

- Traveling blast fronts, inward suction, critically damped spring recovery.
- Reflected impact cones, semantic hot flashes, medium-strength super spikes.
- Blood debris, coarse surface collision, persistent fading surface stains.
- GPU snapshots of Quake MDL monster samples on death, last-to-fade cores,
  and stronger dissolution when a nearby gib supports the death observation.
- Lightning beam cylinders, perpendicular projectile/gib wakes, player floor wakes.
- Lava rings, player/entity liquid transitions, teleport implosion and release.
- Sparse ceiling drizzle, restrained structural shake, curl wind for free debris.
- Up to two debris lights, or four at showcase budget, using free slots in the
  existing dynamic-light buffer. Native game lights retain priority.

All 14 material styles in this checkout and Neon structure mode use the same
physics implementation. Semantic palette colors are in
`engine/Shaders/holo_palette.glsl`. This checkout's style names differ from the
Cyber/Plasma/Inferno Pointcloud names used in the request; no existing style was
renamed or replaced.

## Tuning

All listed values are live. Budget changes rebuild the renderer's physics
resources and clear transient effects. The other controls do not reallocate
buffers. Initial activation builds resources once for the current map.

| Cvar suffix after `r_holo_phys_` | Default |
|---|---:|
| budget | 65536 |
| collide / restitution / settle | 1 / 0.25 / 1.2 |
| blast / blast_radius / suction | 1 / 1 / 0.35 |
| spray / spray_count | 1 / 96 |
| beam / beam_radius | 1 / 24 |
| gore / gore_max / gore_life / gore_gravity | 1 / 20000 / 45 / 1 |
| death / death_life | 1 / 1.2 |
| wake / player_wake | 0.7 / 0.5 |
| liquid / teleport | 1 / 1 |
| dust / shake | 0.8 / 0.4 |
| turbulence / turbulence_scale | 0.6 / 48 |
| debris_light / debug | 1 / 0 |

| Preset | Active budget | Gore cap | Settle | Dust |
|---|---:|---:|---:|---:|
| play | 16384 | 4096 | 1.0 | 0 |
| fine | 65536 | 20000 | 1.2 | 0.8 |
| showcase | 131072 | 20000 | 1.8 | 0.8 |

The maximum active budget is 262144. The preallocated stain pool supports up to
65536 stains without reallocating when its live cap changes. Stains stop
simulating on contact; a draw-compaction pass expires them and counts live stains.

## Architecture and safety

The original packed sample buffers stay unchanged. GPU-generated immutable home
positions refer back to their stable sample IDs. Physics uses 48-byte sparse
records, two compact active-index lists, a free-slot stack, one occupancy bit per
source sample, and a bounded source-to-slot hash table. State updates are ordered
on the existing Vulkan queue with compute/draw/transfer barriers. Uploads and
readback counters use the existing frame fences; particles never return to the CPU.

The CPU appends bounded 64-byte impulses to a 64-event queue. Temp-entity hooks
observe already-decoded positions and retain the original stock effects and
message reads. Read-only entity observations supply wakes, water transitions and
death hints. Dynamic lights can refine a matching blast radius. Dedicated hashes
supply cosmetic seeds; Holo Physics never consumes Quake's RNG.

A GPU surface-bounds pass compacts candidate work into an indirect dispatch before
point promotion. Only active points are integrated. Conservative event bounds
select the reactive shader for already CPU-drawn surfaces, matching this
checkout's existing surface-draw architecture. Unaffected surfaces use the
original shader. Original stable jitter and particle shapes are retained.

The collision field is a static, GPU-built hash grid with averaged normals and
packed plane distances, approximately 16 units per cell. It is derived from the
source samples, not gameplay traces. No `SV_Move`, gameplay `TraceLine`, or
framebuffer feedback is involved. Resource cleanup handles map changes,
disconnects and backwards time jumps on save/demo reloads. Console/game/demo
pauses freeze the physics clock; integration clamps frame delta to 50 ms.

## Measured validation and limits

See `HOLO_PHYSICS_VALIDATION.md` for reproducible tests and measurements.

- Moving doors, lifts and platforms are excluded from the static collision grid.
  Thin walls and corners are approximate at the grid resolution.
- Temp-entity packets often contain no incoming direction. A nearby observed
  spike can supply it; otherwise the spray uses the sampled surface normal.
- Death recognition is best-effort for Quake MDL death/die frame names and nearby
  gib models. Custom monster conventions may omit dissolution. Gameplay entities
  and their animation state remain untouched.
- Protocol-specific temp entities absent from this engine are not invented.
  Native blood particle messages supply the blood observer.
- Collision sampling is included in physics-compute timing. The separate grid
  timer measures map construction, not an isolated per-frame collision cost.
- The home-position buffer adds 16 bytes per source sample, in addition to the
  budget-sized sparse state. This is required by the packed sample format in
  this checkout; total memory is not solely proportional to the active budget.

These are cosmetic particles. They do not replace Quake's simulation, networking,
collision, AI, damage, entity lifetime, or QuakeC.
