# Structure validation

Executable SHA-256: `0B85CC7AEC8FA19EC7EF6FFFA53AD25EC8E27815E6D92D41A5A7E87B7882A56C`.

Reference machine: RTX 2050 / Ryzen 5 7535HS, Windows x64 Release. All structure comparisons use source colors, three layers, Play density, 1280 x 720, an angled e1m1 wall view, two-times burst effects, threaded rendering, and a 60 host-frame cap. Each includes three screenshot readbacks and a 36-piece synthetic wall breach. These timings include capture overhead and are not presented frame rates.

| ID | Structure | Host frames/s | GPU frame mean ms | Detached |
|---:|---|---:|---:|---:|
| 0 | original | 58.644 | 5.614 | 36 |
| 1 | dust | 57.833 | 7.257 | 36 |
| 2 | beads | 54.402 | 16.827 | 36 |
| 3 | chips | 58.544 | 7.667 | 36 |
| 4 | microvoxels | 58.619 | 8.83 | 36 |
| 5 | flakes | 58.375 | 10.149 | 36 |
| 6 | splinters | 58.154 | 7.649 | 36 |
| 7 | fibers | 39.267 | 10.78 | 36 |
| 8 | goo | 53.597 | 16.973 | 36 |
| 9 | rings | 55.031 | 16.462 | 36 |
| 10 | mixed | 57.87 | 12.519 | 36 |

Validation passed:

- All ten new modes and original samples: 33 before/explosion/aftermath captures; each intact shape visually reviewed with explosion spot checks; 36 detached chunks and zero PhysX errors in every case.
- Identical scatter fingerprint across all mode choices and cold/warm source cache: `33c839dd97aee283`.
- Live switching through all 11 choices and all four layer counts under OIT 0, 1, and 2, with maximum effects and reduced flashes: no geometry rebuild, 36-piece breach preserved.
- Four CTest suites pass, including deterministic irregular sampling within a triangular face, reflected structure vertex attributes, structural physics, pressure, and swept gib motion.
- Full engine regression: real rockets and WALK traversal in both renderers; partial breach and rubble pushing in the arena; faithful gameplay hash `caa13ac253fcfc0d`; structural save/load and renderer switching.
- Cold, corrupt, regenerated, and warm cache cases pass.
- Launcher self-test checks all 44 structure/layer command combinations, preview, PAK directory bounds, quoting and Unicode. Native hidden-window control painting is not a full visual UI test.

The source scatter buffer for e1m1 uses 2,875,420 bytes, in addition to the existing 7,686,420-byte regular sample buffer. Per-draw structure metadata is 112 bytes. These are not total engine VRAM figures.

See [capture-free 1080p measurements](MEASUREMENTS.md) for the default Mixed Debris Cloud setting. Mesh modes do not add fluid simulation, connected rope physics, or per-grain PhysX actors. [Structure guide](PARTICLE_STRUCTURES.md).

Recorded: 2026-09-08T12:14:16.4285089-07:00.
