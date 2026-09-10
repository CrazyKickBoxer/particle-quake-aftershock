# Geometry and compatibility

Legacy single-blast measurements with the Play density and CPU PhysX follow. Counts include only structures accepted by the automatic extractor. See [Progressive destruction](PROGRESSIVE_DESTRUCTION.md) for the newer multi-hit damage, pillar eligibility and support rules.

| Map | Panels | Structural cells | BSP splats | Evidence |
|---|---:|---:|---:|---|
| e1m1 | 14 | 200 | 1,921,605 | Actual rockets remove panel 9 completely; normal WALK crosses in both renderers |
| start | 6 | 82 | 1,964,371 | Synthetic explosion, 16 detached, zero PhysX errors |
| e1m2 | 15 | 288 | 2,163,914 | Synthetic explosion, 9 detached, zero PhysX errors |
| e1m3 | 17 | 403 | 2,087,050 | Synthetic explosion, 38 detached, zero PhysX errors |
| Original procedural arena | 1 | 77 | 322,190 | Actual rockets detach 43 cells; partial breach and rubble pushing; WALK crosses in both renderers |

These are targeted scenarios, not claims that every structure or campaign path has been tested. The procedural arena has an original texture, wall, floor, stairs, hulls, and entities generated in C++; no game map is distributed.

| Geometry or content | Current behavior |
|---|---|
| Matching closed axis-aligned rectangular walls | Eligible if bounds, thickness, solid-space and opposite-empty-space tests pass |
| Matching horizontal slabs | Eligible; progressive mode anchors at supported span ends |
| Rectangular interior pillars | Eligible in progressive mode when paired BSP faces and exposure checks pass |
| Slopes, arches, irregular convex cells, complex pillars | Preserved intact; no general volume reconstruction/fracture |
| Outer seal / one-sided solid-to-void faces | Preserved intact |
| Sky / liquids | Native Quake rendering and contents; not structural fragments |
| Moving brush doors and platforms | Native gameplay handling, particle rendering for opaque brush faces, kinematic PhysX obstacle for rubble; not destructible |
| Monsters, pickups, projectiles | Native engine entities and shared modified world traces; targeted weapon/player cases verified, exhaustive interaction matrix pending |
| Stock Quake | Tested on the user's supplied original PAKs; no redistribution of them |
| Mission packs / custom mods | Launcher and original search order supported; gameplay/destruction acceptance unverified |
| Demos / unmodified multiplayer | Faithful path retained; destruction mode rejects these sessions |
| MD3 / MD5 / translucent model extensions | Native triangle fallback |

Texture names provide density hints, not authoritative material metadata. Current fragments are regular closed boxes. There are no authored protected-mechanism sidecar profiles, irregular fracture tools, glass rules or wood-grain structures. Campaign progression remains experimental in the sandbox.
