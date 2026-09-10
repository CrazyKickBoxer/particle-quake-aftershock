# Bug journal

| Symptom | Cause and fix | Regression evidence |
|---|---|---|
| New CLI hooks appeared not to run | Upstream 256-byte command-line storage truncated long launch commands; expanded to 8,192 bytes | Full automation commands run with maps, captures and result output |
| Particle world invisible with SIMD visibility | Used per-face visibility state that SIMD path did not update; read native surface bitset | Inspected stock-map captures with threaded renderer |
| Particle walls lost coverage at grazing views | Camera-facing surface splats did not follow plane depth; changed to plane-aligned footprints | Inspected walls/corners in stock capture |
| Actual holes still blocked movement | Original BSP solid collision survived destruction; substituted detached regions into authoritative hulls | Full section WALK traversal in both renderers |
| Partial openings had invisible seams | Independently eroded disjoint empty rectangles; switched to overlapping maximal empty rectangles | Arena BSP player-hull probe and partial-breach WALK |
| Fragments could trap an overlapping player | Default initial sweep normal opposed movement regardless of escape direction; use MTD normal and filter escape/tangent motion | Isolated overlap test blocks inward motion, permits outward motion |
| A normal rubble pile stopped the scripted arena walk | Rubble was queried as an obstacle with no player force response; explicit bounded small-body pushing and longer arena traversal test | Partial breach crosses with WALK, without noclip or a test teleport |
| Blast reported invalid actor bond ownership | Fractures for unrelated actors and the external node were mishandled; filter graph membership and external support node | Repeated explosions report zero SDK errors |
| Compound islands could not break after falling | Chunk/body mapping assumed only first detach; preserve transforms/velocities while splitting existing compounds | Three-cell support island becomes three bodies; save/load retains it |
| Shutdown retained PhysX actors | Releasing the scene did not release owned actors; explicitly release dynamic/static/brush actors and meshes | Five pressure cycles end with zero tracked PhysX allocations |
| Doors/platforms did not stop structural rubble | Only static BSP was in PhysX; cook brush meshes and publish kinematic entity transforms | Adapter platform lifts a settled piece and releases it when disabled |
| Initial brush integration crashed on map load | Render-side map setup had no selected Quake VM; access explicit `sv.qcvm` entity storage | Full map/weapon/save regression after the fix |
| Alias splats extended beyond source silhouettes | Full quad footprints crossed triangle edges; constrain barycentric footprints to the triangle | Latest normal-camera weapon capture inspected |
| Original procedural arena rendered dark | No-lightdata BSP was sampled as if it had baked lighting; explicit fullbright metadata and cache schema bump | Arena capture shows original generated surfaces |
| Damage disappeared or physics jumped after load | Event-only reconstruction could diverge after secondary splits; persist bond health and body state with checksums | Exact adapter pose restoration, integrated 36-body load, corrupted payload rejection |
| Explicit renderer/density flags lost to saved config | CLI applied too early; reapply before expanded `+` commands after config load | Command-line path exercised by launcher/tests |
| Reused faithful save inherited stale destruction data | Old structural sidecar survived faithful overwrite; remove obsolete sidecar | Code path and file-removal adapter test; complete in-engine overwrite scenario pending |
| Some save/load runs rejected an unchanged map | Structural identity included transient lightmap atlas and render-UV data; hash source positions/planes/mappings and structural schema separately | Five independent save/load runs pass, including a renderer switch before load |
| Captures went to Steam instead of the requested file | Upstream screenshot routing preferred Steam; explicit automated screenshot option selects file output | Engine captures appear in test user directory |
| Reported throughput included an extra frame | Divided N frames by N-1 measured intervals; count intervals correctly | JSON now explicitly reports host frame intervals, not presented FPS |

Open limitations are tracked in FEATURES.md. In particular: oversized patch splitting, GPU indirect culling, complete splat asset coverage, material fracture hierarchy, full decorative collision, advanced explosion art, photomode, and expanded compatibility/presentation acceptance remain open.
