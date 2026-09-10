# Feature status

States: **Verified** means an automated or inspected evidence case passed on the reference machine. **Implemented** means code exists but broader acceptance is outstanding. **Limited** identifies a supported subset. **Not started** means the build does not provide that feature. Upstream functionality is identified explicitly.

| Area | State | Behavior / evidence |
|---|---|---|
| Neon Cathedral reference preset | Verified / Limited | Cyan/gold emissive grains, BSP boundaries, magenta MDL silhouettes, sky darkening, filtered grains, HDR bloom, temporal reconstruction, and optional screen-space floor reflections; existing maps/models |
| Ten layered particle structures | Verified / Limited | Real disc/mesh silhouettes, deterministic irregular placement, 1–4 layers, world/brush/rubble surfaces and GPU explosion fragments; see PARTICLE_STRUCTURES.md |
| Native Windows engine and launcher | Verified | MSVC Release builds; launcher data validation, quoting, Unicode arguments and preview self-test |
| Independent renderer/world choices | Verified | Live renderer switch retains 36 detached chunks; world switch reloads |
| Actual stock-map full wall breach | Verified | `test-engine.ps1`: real rockets remove e1m1 panel 9; WALK crosses in both renderers |
| Partial breach and rubble pushing | Verified | Original arena: 43 of 77 cells detach; WALK crosses in both renderers |
| PhysX CPU + Blast integration | Verified | SDK source builds, real actor motion, contacts, sleeping, repeated damage; no fallback integrator |
| Support loss and secondary breakup | Verified | Three-cell tower drops as one unsupported compound, then breaks into three moving bodies |
| Collision subtraction | Verified / Limited | Original hull solids are regionally subtracted for detached cells; overlapping opening rectangles avoid player-size seams |
| Dynamic debris queries | Verified / Limited | Sweep and overlap-escape tests; normal WALK and small-rubble pushing; broad monster/trigger scenarios still pending |
| Moving brush collision | Verified / Limited | Cooked local triangle meshes, kinematic poses; a moving platform lifts a fragment in the adapter test; campaign door interactions need broader tests |
| Conservative visibility | Implemented | Whole-map conservative PVS in sandbox; native surface/frustum tests remain; no portal connectivity optimizer |
| Save/load and reset | Verified | Matching Quake save + versioned/checksummed structure state, exact saved poses, wrong-map/corrupt-data rejection, Unicode file test |
| BSP surface splats | Verified / Limited | Four bytes/sample, original texture and lightmap coordinates, plane-aligned indexed quads, persistent Vulkan buffer, seam samples |
| Packing / shader ABI | Verified | 65,536 exact coordinate pairs, 0.0625-texel/axis maximum encoding error; binary SPIR-V vertex type, descriptor and alias UBO reflection |
| Oversized sample patches | Limited | Current face-local encoding rejects out-of-range surfaces; automatic patch splitting remains unfinished |
| Sample cache | Verified | Version/hash/checksum validation, bounded lengths, atomic replacement; invalid cache regeneration test |
| MDL animated splats | Implemented / Limited | Cached triangle/barycentric samples, GPU keyframe interpolation and native skin/fullbright lighting; opaque native Quake alias models |
| Moving brush splats | Implemented / Limited | Opaque brush entities use native entity matrices and shared samples |
| Sky, liquids, sprites, translucent/extended models | Limited | Preserved through native vkQuake rendering; not converted to splats |
| GPU culling and indirect splat draws | Not started | CPU surface visibility and per-face instanced draws; no 64/128-sample GPU indirect benchmark |
| GPU explosion dust/sparks/grit | Verified / Limited | Inspected real-rocket frame sequence; bounded 32-event analytic vertex simulation; contact-driven secondary effects; floor approximation |
| Compute particle pools / full collision field | Not started | Cosmetic particles use analytic shader trajectories, not compute simulation or full scene collision |
| Fourteen appearance styles | Implemented / Limited | Four original styles plus ten animated materials with surface motes and matching GPU bursts; [mode guide](PARTICLE_MODES.md). Material changes are cosmetic, not new fracture solvers |
| Camera accessibility | Implemented / Limited | Copied render-view shake with zero setting; reduced additional effect flashes; stock Quake flashes remain |
| Directional gibs and goo | Verified / Limited | 120-Hz cosmetic swept collision, low bounce and floor drag; copied rocket momentum; native Quake gib meshes and visual proxies; clipped wall/floor splashes and slide trails. Static and moving BSP attachment; no rubble decals or fluid solver |
| HDR bloom, temporal reconstruction, floor reflections | Verified / Limited | RGBA16F scene, four-scale bloom, camera/depth/color history rejection, rough screen-space reflections and local color spill; separate live toggles; see PARTICLE_FIDELITY.md |
| Refraction and attached crack/scorch decals | Not started | Not represented as implemented effects |
| Trails, lightning, sky/liquid animation | Limited | Original behavior retained; added blood droplets and material-specific GPU effects; no complete replacement |
| Fracture materials | Limited | Deterministic box-grid cells, density heuristics for stone/metal/wood; no Voronoi hierarchy, glass authoring or wood-grain patterns |
| Fracture/cooking disk caches | Not started | Authoring/cooking happens on load, outside explosion frames |
| Pressure/resource checks | Verified / Limited | Five 1,024-body cycles; no PhysX errors or outstanding allocator blocks on shutdown; not a full VRAM/presentation stress pass |
| Photomode | Not started | Capture sequences and fixed test camera exist; no dedicated pause/step/free-camera UI |
| PhysX GPU / interop | Not started | Explicitly unavailable; CPU path tested on an RTX 2050, not yet on an AMD/Intel graphics machine |
| Mission packs / mods / multiplayer | Limited | Native filesystem and faithful engine paths preserved; launcher selection exists; full compatibility acceptance pending |

The full original prompt remains the product backlog. This table must accompany the build so the verified wall-breach milestone is not mistaken for completion of every renderer, art, compatibility and product requirement.

## Layered particle geometry

Ten structure modes now provide discs, spheres, angular chips, cubes, closed flakes, splinters, curved ribbons, lobed blobs, hollow toruses, and mixed debris. They use stable irregular sampling and 1–4 depth layers on opaque BSP surfaces and structural rubble, plus matching GPU explosion fragments. Material colors remain independent. See [structures](PARTICLE_STRUCTURES.md) for coverage, LOD behavior, and simulation limits.
