# Validation record — 2026-09-08

Reference machine: AMD Ryzen 5 7535HS (6 cores / 12 threads), NVIDIA GeForce RTX 2050, driver 596.36, Windows 11 Home build 26200, 25,028,239,360 bytes installed RAM. Backend: CPU PhysX 5.6.0 and Blast at the pinned revision. Native MSVC x64 Release, Vulkan renderer, threaded rendering enabled (`r_tasks 1`). Raw hardware data and measurements are in [results](results).

The newer layered geometry implementation has a separate [structure validation record](STRUCTURE_VALIDATION.md), including all ten shapes, live switching, sample stability, and current performance measurements. Earlier material-mode evidence below remains available.

The later [Neon Cathedral](NEON_CATHEDRAL.md) preset has separate current evidence under `results/neon`; the older records below retain their original build identities.

## Functional checks

- CTest: all four targets passed — structural physics, physics pressure, GPU contracts and gib motion.
- Ten new material modes: automated e1m1 intact/explosion/aftermath captures; 36-cell synthetic breach in each. The dedicated room fixture exercises forward projection, wall impacts and floor smears. See [mode controls and limits](PARTICLE_MODES.md).
- e1m1 real-weapon fixture: complete 36-cell panel removal and normal WALK traversal in Classic and Particle. A final elevated rocket is tracked beyond the original wall while still within the wall's lateral/vertical bounds; this clears the settled rubble without shooting over the original wall.
- Original arena: 43 of 77 cells detached, partial-hole BSP query, rubble pushing and normal WALK traversal pass in both renderers. No noclip or post-destruction teleport is used for traversal.
- Faithful local-server replay: both renderers, additional effects/shake on and off, fixed simulation duration, seed 1234. All four normalized gameplay/RNG hashes at tick 180 match: `caa13ac253fcfc0d`.
- Quake save + sidecar: 36 detached bodies restore; renderer switch preserves damage. Five additional independent save/load runs passed after separating stable structural identity from transient rendering atlas data.
- Cache: cold generation, deliberately corrupted checksum rejection, atomic regeneration, and subsequent validated cache hit.
- Launcher: valid PAK directory bounds, exact command preview, escaped quotes/backslashes and Unicode argument round trips passed. The hidden-window capture does not establish complete visual verification of native combo/edit controls.
- Package: extracted runtime launches against external Quake data; executable SHA-256 matches the manifest; required modified sources are present in the source ZIP; asset/capture/save exclusion scan passes.

The controlled test seed disables upstream's wall-clock-dependent extra RNG advance only when `-test-seed` is explicitly supplied. It does not reseed gameplay every tick. Hashes contain selected authoritative entity fields plus RNG state, quantized to 1/1024; they are not a proof of every possible demo, mod or gameplay path.

## Physics and GPU data checks

The adapter suite checks repeated Blast damage, finite/moving transforms, exact saved positions, wrong-map rejection, corrupt-payload rejection, Unicode save paths, unsupported compound collapse, secondary compound breakup, overlap escape, moving-platform support/release and reset.

The pressure test creates 1,024 structural bodies and applies 16 explosions in each of five cycles. It checks finite poses, zero SDK errors, rejection of capacity overflow without losing current state, bounded allocator peaks, and zero outstanding PhysX allocation bytes after complete shutdown. These are headless adapter tests, not GPU or full-game 1,024-body performance claims. Per-cycle allocator numbers are retained in `results/adapter-test-details.txt`.

Packing tests exercise all 65,536 coordinate values, opposite-axis combinations, rounding boundaries, invalid ranges, infinities and NaNs. Maximum measured quantization error is 0.0625 source texture texels per axis. Binary SPIR-V reflection checks input types/locations, descriptor set/binding pairs, and all alias UBO offsets. Vulkan validation layers and broad device coverage remain outstanding.

## Performance methodology

The standard benchmark uses 1920×1080 windowed output, Enhanced style, Play density, Cinematic damage, CPU PhysX, effects/gibs/goo enabled at 2x particle count, a 60-Hz host cap and v-sync off. `scripts/benchmark.ps1` records 420 engine frames for the e1m1 actual-rocket sequence and 220 for each synthetic geometry case. Capture I/O is disabled in these measurements. JSON and exact commands are retained in `results`. The mode capture suite includes screenshot I/O and must not be used as a clean frame-rate benchmark.

`host_fps` divides the number of measured frame intervals by elapsed wall time. It is **engine host throughput, not measured display presentation**. p50/p95/p99 are host intervals including scheduling/frame limiting. GPU values use the engine's completed whole-frame timestamp results; they are not isolated particle-pass timings. Physics means cover adapter steps, not the entire fracture/extraction/load pipeline. Short runs are useful regression cases, not a full hardware performance characterization.

The current measurement table is generated in [MEASUREMENTS.md](MEASUREMENTS.md) from the retained JSON results. The 60 presented-FPS target is not established by these measurements. Full presentation timing, VRAM high-water marks, isolated inactive-effect overhead, and preset-specific overload budgets remain open acceptance items.

For e1m1 at Play, the static sample buffer is 7,686,420 bytes and the face metadata footprint is 529,536 bytes. CPU sample copies, animated sample buffers, static/frame upload allocations, physics, textures, render targets and other engine data are additional. PhysX requested-allocation live/peak counters exclude Blast and the rest of the process. None of these numbers is a total-VRAM claim.

## Capture timing and limits

`-test-rocket -test-panel 9` places a WALK player and fires the actual rocket launcher. `-test-projectile` adds an elevated shot at ticks 150–164, then restores the view for traversal. `-test-view` copies a fixed panel-facing camera into rendering only. The fixture holds forward velocity starting at tick 180. The arena uses a longer traversal window to push through rubble.

`-capture-tick N -screenshot name.png` captures the rendered frame after reaching the specified server tick. `-capture-sequence prefix -capture-start-tick N -capture-ticks interval` produces `prefix-000NN.png`; the current argument is a filename prefix, not a directory. Paths are interpreted through the engine screenshot/user directory. `-frames N` terminates after sign-on and N host frames. Frame sequences show real motion but do not establish cross-platform deterministic PhysX replay.

Inspected local captures include the intact e1m1 wall, slab release, dust and aftermath, a normal-camera particle weapon view, and the fullbright original procedural arena. Captures contain user-supplied Quake content and are excluded from both archives. No game data is included in either archive.

## Remaining acceptance work

See [FEATURES.md](FEATURES.md). Not yet established: complete GPU splat asset coverage, GPU indirect/culling contracts, arbitrary structural geometry, advanced fracture/art requirements, isolated graphics-memory budgets, presentation timing, a clean-machine rebuild of every upstream codec, AMD/Intel GPU hardware coverage, broad campaign/mod/demo/multiplayer acceptance, and the complete interactive launcher/window/pause/resize matrix.

## Fidelity and reflections

See [PARTICLE_FIDELITY.md](PARTICLE_FIDELITY.md) and `docs/results/fidelity` for the HDR/reflection test matrix, live toggles, motion and video restart, actual MSAA/render-scale checks, GPU contracts, and build identifiers. Earlier Neon and structure measurements describe their recorded executable versions.
