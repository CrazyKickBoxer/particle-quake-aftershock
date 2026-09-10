# Particle Quake: Aftershock

Build a Quake particle renderer with full PhysX wall destruction

Build a playable extension of **vkQuake** that renders Quake as dense, beautifully lit fields of world-space splats and makes its architecture physically destructible. Use **NVIDIA PhysX for rigid-body simulation** and **NVIDIA Blast for fracture authoring and damage-driven chunk separation**.

The defining moment: a rocket hits a stone wall. A sharp flash reveals cracks radiating through the masonry. The wall ruptures through its thickness, throwing textured slabs, smaller fragments, and dust into the room. An unsupported section above the breach tears free a moment later. The chunks tumble, collide, shed grit, and settle into persistent rubble. Through the dust, the player can see another room, shoot through the opening, and walk through it. The entire sequence preserves Quake's scale, textures, atmosphere, and fast combat.

**This sequence is a required gameplay milestone. Full wall destruction is the central feature.**

Read the entire specification before coding. Preserve its boundaries, implement in demonstrable increments, and distinguish proposed behavior, implemented behavior, and verified behavior. Make sound implementation choices without repeatedly asking about routine details.

## 1. Product and scope

Ship a Windows x64 build first, using Vulkan and the existing vkQuake build workflow. Keep platform-specific code isolated for later Linux support.

Deliver:

- `vkquake.exe` with independently selectable renderer and world mode.
- `vkquake_launcher.exe` with game-data discovery, graphics controls, and destruction presets.
- A GPU splat renderer covering world geometry, animated models, brush entities, sky, liquids, and effects.
- A working PhysX and Blast destruction system with persistent structural state.
- Automated checks, reproducible captures, benchmark commands, and honest documentation.

Call the asset pipeline **Quake-native: PAK/BSP/MDL/SPR**. Quake's compiled BSP contains its map geometry and texture data; do not design this around Doom WADs or sector logic. Preserve the engine's own virtual filesystem and mod search order. Consult the [original BSP definitions](https://raw.githubusercontent.com/id-Software/Quake/master/WinQuake/bspfile.h) and the [vkQuake project](https://github.com/Novum/vkQuake).

Use the user's legally supplied game assets. Do not download replacement texture packs, copyrighted maps, models, or sounds. Generated fracture interiors, collision volumes, dust masks, and material parameters are permitted: they are procedural runtime data necessary for destruction. Preserve original textures on original exterior surfaces.

## 2. Separate renderer choice from world behavior

Support this configuration:

```text
-renderer classic | particle
-worldmode faithful | destruction
-physics off | physx-cpu | physx-gpu
-style faithful | enhanced | inferno | inferno-color
-density play | fine | showcase
-destruction-preset restrained | cinematic | cataclysm
```

These are required interfaces to implement, not claims that upstream vkQuake already accepts them. Reject invalid combinations with an actionable explanation.

### Faithful world

Keep original Quake gameplay, collision, networking, and demo playback unchanged. Visual effects consume copied engine events and read-only snapshots. Never write gameplay state, alter engine RNG consumption, change weapon aim, or feed decorative physics back into the server.

Surface chips, sparks, dust, and visual debris may be dramatic, but they must preserve a believable solid boundary wherever original collision remains. A persistent, passable wall breach belongs in destruction mode.

Camera shake affects a copied render view. Keep the engine's normal input and aiming behavior. Renderer switches and visual quality changes must leave gameplay checksums unchanged.

### Destruction world

Provide a separate local single-player sandbox with authoritative destructible geometry. Walls, floors, ceilings, pillars, arches, and bridges can fracture where the geometry is eligible. Destruction changes traversal, sight lines, projectile collision, and cover.

The destruction system owns structural state and publishes it to gameplay collision and rendering. It must never depend on whether a surface was visible, which renderer was selected, how many particles were drawn, or the display frame rate.

Preserve Quake's movement and weapon handling as closely as possible. Treat campaign progression as experimental when breaking original architecture; provide immediate level reset and checkpoint recovery. Sky boundaries, the outer map seal, and identified critical mechanisms remain protected by explicit rules. Explain those rules in the launcher and expose protected regions in a debug view.

Ordinary demos and unmodified multiplayer are supported in faithful mode. Reject destruction mode when joining an unmodified server. Custom destruction replay and save formats are separate from original Quake demos.

Both renderers must display the same destructible world state. In destruction mode, the classic renderer draws surviving surfaces, new interior faces, and fragment meshes; the particle renderer draws their splat representation. Switching renderers must not rebuild or heal the world.

Require a map reload when changing world mode or physics backend. Turning off decorative effects must leave existing structural damage intact. A separate Reset World action restores the original map.

## 3. PhysX and Blast integration

Use PhysX as the production rigid-body backend. Do not silently replace it with Jolt or a homemade integrator.

PhysX handles body motion and contacts. Blast provides destruction structures, fracture tools, and damage processing; its core does not itself supply a renderer or collision scene. Integrate the libraries through explicit adapters. See [NVIDIA's Blast documentation](https://nvidia-omniverse.github.io/PhysX/blast/index.html).

Provide:

- **PhysX CPU:** the required complete gameplay path, usable with supported Vulkan graphics hardware from any vendor.
- **PhysX GPU:** an optional accelerated path selected after capability checks and validation. PhysX GPU simulation uses CUDA; Vulkan support alone does not establish support. Follow the [PhysX GPU simulation requirements](https://nvidia-omniverse.github.io/PhysX/physx/5.7.0/docs/GPURigidBodies.html).
- **Physics off:** permitted for faithful mode and renderer debugging. Destruction mode requires a functioning structural physics backend.

Build against pinned, mutually compatible PhysX and Blast revisions. Use the SDK directly; do not require an Omniverse application, USD scene workflow, editor, or service to run the game. Build a small C-compatible boundary around the C++ physics code where it integrates with engine C code.

Verify dependency and redistribution terms from the pinned source trees, including optional GPU components and transitive libraries. Preserve vkQuake's GPL notices and all required third-party notices. Do not assume a moving branch keeps the same license: NVIDIA has [announced a planned PhysX license change](https://github.com/NVIDIA-Omniverse/PhysX/discussions/501). Record the actual revision and its license; resolve compatibility before linking or distributing it.

Use a fixed physics timestep independent of rendering, with a bounded accumulator, an explicit overload policy, and interpolated render transforms. Establish Quake-unit conversion, handedness, gravity, mass scale, tolerances, and collision margins centrally.

Pre-cook static meshes and fragment convex collision shapes. Keep fracture authoring and expensive cooking out of explosion frames. Handle moving brush models in their own coordinate systems.

Use immutable, frame-tagged transform snapshots for the renderer. Start with an explicit CPU upload path. Add CUDA/Vulkan interop only after verifying device matching, resource ownership, supported APIs, synchronization, and an actual performance benefit. Never treat a CUDA pointer as a Vulkan buffer.

Do not promise identical PhysX trajectories across CPU and GPU backends, architectures, or SDK versions. Make fracture seeds and event ordering repeatable; use recorded authoritative state when exact replay is required.

## 4. Full wall destruction

### 4.1 Recover usable solid volumes

A rendered BSP face is a surface, not automatically a closed wall with thickness. A compiled BSP also does not preserve the original editor brushes as a simple list. Build a validated destruction representation from the solid-space partition and the visible boundary geometry.

At load time or during cache generation:

1. Identify connected candidate structures and assign stable IDs.
2. Reconstruct bounded solid cells or closed local volumes from BSP planes and solid contents; use robust tolerances and a shared boundary policy.
3. Where exact reconstruction is unsuitable, generate a bounded structural shell with explicit inferred thickness. Mark the result as generated geometry.
4. Reject ambiguous, degenerate, overlapping, or unbounded candidates with a visible diagnostic. Preserve those regions as intact geometry.
5. Preserve original face IDs, texture coordinates, material assignments, and lighting references on the exterior.
6. Generate closed interior surfaces and collision shapes. Holes need visible thickness, jagged rims, and valid back faces.

Support automatic processing of eligible geometry on stock maps. Optional sidecar profiles can refine materials, supports, thickness, and protected regions, but destruction must not depend entirely on hand-tagged demonstration walls.

Never pretend there is a room behind every surface. A breach can open into existing empty space or excavate a bounded cavity inside solid material. Preserve the outer world seal and prevent unbounded views into the void.

### 4.2 Author real fragments and supports

Generate a deterministic fracture hierarchy per structure using cached, material-sensitive patterns:

- Stone: irregular blocks, coarse breaks, grit, and heavy dust.
- Brick: masonry-scale sections with weaker mortar-like connections.
- Wood: elongated splits aligned with a chosen grain direction.
- Metal: plate-like pieces, strong attachments, and sparks. Treat plastic deformation as a separate optional feature.
- Glass: thin angular fragments only where a material override identifies glass.

Use texture names and explicit configuration as heuristics for material selection; textures do not contain authoritative physical properties.

Create fragment adjacency, support anchors, bond strength, mass, center of mass, and inertia data. Keep connected supported regions intact. When damage removes support, detach the connected unsupported island as a body or bounded group. Permit secondary fracture when falling rubble hits other structures.

Fracture hierarchy depth, secondary damage, and active body counts must be bounded. Do not create one PhysX body per splat. Most geometry stays static or attached until it breaks.

### 4.3 An explosion must change the wall

For a rocket, grenade, explosive entity, or scripted destruction event:

1. Create one authoritative event with a stable ID, tick, origin, source, radius, energy, and deterministic seed.
2. Query nearby structures and evaluate damage with distance falloff, material resistance, and occlusion.
3. Accumulate local damage so repeated hits widen and deepen the affected region.
4. Break failed connections and identify newly unsupported pieces.
5. Remove detached material from the intact render representation and the authoritative collision representation.
6. Activate matching PhysX fragments with impulse and torque at appropriate points. Account for mass and avoid explosive self-overlap.
7. Expose the interior faces that the break makes visible.
8. Emit smaller chips, dust, sparks, and decals from the actual fracture and contact locations.
9. Process bounded secondary impacts and collapse events.
10. Commit the new structure, collision, and visibility state together at a simulation boundary.

Retain correspondence between the original wall and every visible fragment. A point on a slab follows that slab's transform; it does not independently drift away.

Repeated explosions must enlarge an existing breach, destroy a complete eligible wall section, and bring down an unsupported neighboring section. Damage must persist when leaving the room, changing renderers, saving, and loading.

### 4.4 Collision must agree with the opening

Create a shared world-query interface for:

- Player movement, steps, sliding, ground checks, and head clearance.
- Monster movement, projectile sweeps, hitscan traces, and line-of-sight tests.
- Radius-damage occlusion, pickups, moving platforms, and entity contact handling.
- Structural debris and relevant gameplay interactions.

In faithful mode, delegate to original Quake collision. In destruction mode, query surviving static solids and the authoritative dynamic structures. Preserve required trace fields, contents semantics, trigger behavior, and brush-entity interactions.

Do not merely combine an unchanged BSP hit with a PhysX hit and choose the closest: the old BSP would still block the new hole. Replace or subtract the damaged solid regions in the query representation used by gameplay. A whole-world replacement or a regional overlay is acceptable only after its player, projectile, and monster behavior is verified.

Separate structural rubble from cosmetic debris. Large persistent rubble can block movement or provide cover. Small cosmetic chips must not snag the player. Preserve meaningful collision when sleeping bodies are merged or simplified; never silently clear a passage because a visual budget was exceeded.

### 4.5 Visibility must agree with the opening

The original PVS was built for the intact map. Newly connected rooms may fall outside it.

Keep original PVS acceleration in faithful mode and wherever it remains conservative. After destructive topology changes, use a conservative visibility fallback that includes newly exposed geometry and entities. Begin with frustum-only culling across the affected map if necessary, then optimize with tested region connectivity or conservative visibility unions.

Invalidate stale occlusion information after a breach. A depth or occlusion proxy must reflect the current surviving geometry. Hidden entities must become visible through the same opening as the room around them.

### 4.6 Rubble lifecycle

Use explicit states: attached, active rigid body, sleeping rigid body, settled persistent structure, and disposable cosmetic debris.

Keep large slabs long enough to tell the story of the explosion. Reuse stable render data and update transforms. Fade small debris coherently over time; never make an entire room's rubble disappear at a budget boundary.

Cap secondary fracture depth and release resources on reset, reload, and shutdown. Include structural state and required rigid-body state in versioned sandbox saves. Reject incompatible saves with a useful message instead of silently rebuilding an intact map.

## 5. Particle rendering

Use world-space surface samples rendered as camera-facing or surface-aware splats. The scene must remain solid, readable, and recognizably Quake.

The pipeline is:

```text
User PAK / loose assets
    -> BSP / MDL / SPR extraction
    -> versioned surface, sample, collision, and fracture caches
    -> GPU buffers
    -> mode-appropriate visibility + frustum + chunk culling
    -> indirect indexed splat draws
    -> HDR lighting + effects + bloom + tone mapping
```

### Surface samples and memory

Sample world faces in their own plane, deriving texture and lightmap coordinates from the original face mappings. Split surfaces into bounded patches with stable IDs and shared metadata.

Target **four bytes per static sample** where a documented patch-local encoding can represent the sample without loss beyond the selected quantization tolerance. Store transforms, textures, lightmaps, and material properties per patch or chunk. Derive texture lookup coordinates from the reconstructed sample position where possible.

Prove the packing with encode/decode tests, range checks, and worst-case error measurements. Split oversized patches rather than silently overflowing. If a required representation needs more storage, document it explicitly and include all metadata in the memory budget. Animated samples, structural metadata, free particles, and rigid-body records have separate layouts.

Use fixed-size sample chunks as the main culling unit. Benchmark 64- and 128-sample options. Moving fragments use local sample coordinates plus a shared rigid transform, with bounds updated from that transform.

### Coverage and depth

Preserve an opaque wall boundary at every distance and grazing angle. Determine splat footprints from sample spacing, projected surface derivatives, quantization, and any stable jitter. Include conservative coverage at patch borders and corners.

Do not impose a screen-space size ceiling that makes a nearby wall sample smaller than the area it represents. Coverage rules for opaque surfaces differ from size limits for sparks and smoke. Use seam samples, conservative rasterization techniques, or a documented depth-support pass if required; any supporting geometry must follow destruction state.

Store surface depth and normals consistently enough to avoid billboards bleeding through nearby geometry. Compare close-up corners, narrow doors, grazing views, slopes, and thin structures against the classic renderer.

Stable hashing may vary color, sample position, or size within the coverage budget. Animated motion on intact surfaces must be a smooth spatial field. Keep it tangential unless a specific effect deliberately uses normal displacement with conservative bounds. Fracture and rigid-body motion are separate, explicit geometry changes.

### Lighting and animation

Sample baked lightmaps at each world sample and apply every supported light style. Preserve engine dynamic lights, animated textures, fullbright behavior, palette lookup, and gamma semantics. Do not advertise software-renderer pixel identity without a validated equivalent lighting path; use vkQuake's classic renderer as the primary comparison baseline.

For moving chunks, distinguish inherited baked appearance from lighting at their new position. Blend toward an appropriate ambient/light-probe estimate and current dynamic lights. New fracture interiors need plausible material color and illumination; they do not have original lightmap texels. Document that approximation.

Bake alias-model sample locations once using triangle IDs and barycentric coordinates. Interpolate source keyframes on the GPU, respecting skin seams, skin animation, and engine interpolation rules. Preserve sprites and moving brush entities as supported asset types.

Quake MDL animation stores mesh frames, not a skeletal rig. Ragdolls require a separate authored or inferred rig and are an optional later feature. Implement actual mesh-based gibs first. See the [original alias-model format](https://raw.githubusercontent.com/id-Software/Quake/master/WinQuake/modelgen.h).

Implement sky and liquid appearance from the engine's data. Keep faithful liquid warping in texture coordinates; any enhanced geometric ripple is a bounded visual effect with tested boundary coverage.

### GPU execution

Render indexed quads with four vertex evaluations per splat and a reusable index buffer. Generate visibility lists and indirect draw arguments on the GPU. Do not require same-frame count readback to submit draws. Provide a supported fallback for unavailable indirect-count features.

Define CPU/shader layouts from one schema or generated shared definitions. Assert sizes, offsets, strides, descriptor bindings, and buffer capacities against SPIR-V reflection. Do not use shader-source semicolon counting as the layout contract.

Handle compute-to-draw barriers, indirect argument visibility, depth ownership, resource retirement, and frames in flight explicitly. Tag asynchronous debug readback with its originating frame. Use dynamic rendering only where the selected Vulkan baseline and upstream integration support it.

## 6. Make destruction spectacular and readable

The visual hierarchy is: impact, wall failure, moving mass, airborne fragments, settling dust, aftermath. The player should understand what broke and where the new route is.

Every major explosion should combine:

- A brief HDR flash that lights nearby surfaces and fragments without a sustained whiteout.
- A traveling pressure front with coherent dust and restrained refraction.
- A visible fracture opening that leads the main debris release.
- Heavy slabs with convincing momentum, smaller chips with sharper motion, and fine grit trailing both.
- Dust emitted from new fracture surfaces and later collision contacts, with depth-aware intersection fading.
- Secondary contact sparks for suitable materials and contact-driven crumble.
- Directional render-view shake with distance falloff, independent controls, and no aim drift.
- Scorching, cracks, and contact marks attached to their actual surface or fragment.
- A readable aftermath: exposed thickness, missing wall, settled rubble, lingering low-density dust.

Avoid identical radial fountains for every material. Let geometry, mass, impact direction, and supports determine the breakup. Bias cosmetic particles only after the physical event is established.

Use bounded Vulkan compute pools for smoke, dust, sparks, embers, trails, and small non-authoritative particles. Compact active work, track high-water marks, and avoid scanning full capacity when idle. GPU decorative particles must query an uploaded or derived collision representation; they cannot directly call CPU BSP traversal functions.

Ship faithful, enhanced, inferno, and inferno-color styles. Inferno uses strong contrast, readable contour accents, emissive effects, and controlled bloom. Inferno-color keeps original texture and model hues. Keep all styles recognizable as Quake.

Include rocket and grenade trails, lightning effects, torch embers, lava heat, mesh gibs, and optional gore decals. Use original supplied sounds or generated audio only where redistribution is permitted; derive any additional impact response from documented material rules.

Offer reduced flashes, camera-shake strength down to zero, chromatic aberration off by default, and separate gore controls. Photomode may pause and single-step the local destruction sandbox, move a free camera, and capture high-resolution stills or sequences.

## 7. Performance and frame pacing

Choose and record an actual reference machine before making performance claims. All numbers below are **engineering targets**, not measurements:

- Play preset: target 60 presented frames per second at 1920 × 1080 on the declared reference machine during the standard destruction scenario.
- Inactive decorative effects: target no more than 0.3 ms added GPU time over the equivalent renderer configuration; measure CPU cost separately.
- Decorative particle allocations: target no more than 64 MiB at Play, reporting structural geometry, PhysX, fracture caches, and render targets separately.

Set measured preset budgets for active structural bodies, contacts, newly activated fragments per tick, fracture jobs, particle counts, dust overdraw, and VRAM. Define bounds before adding each feature. Do not assume a full structural collapse will fit the original prompt's small decorative-effects budget.

Graceful overload should first reduce cosmetic spawn counts, secondary cosmetic detail, dust resolution, and optional post-processing. Preserve authoritative breach state and gameplay collision. Queue bounded fracture work in stable order; expose delays in diagnostics. Preserve structural pieces as connected groups when a finer split would exceed the active-body budget.

Monitor PhysX capacity and allocation errors. Failures must produce a clear diagnostic and recoverable session state; silently losing contacts or chunks is unacceptable.

Report CPU frame time, physics step time, fracture processing time, GPU pass time, actual frame throughput, presentation timing when available, frame-time percentiles, and memory high-water marks. Distinguish submitted/present-call counts from measured display presentation. A reciprocal GPU duration is not delivered frame rate.

Keep input and rendering decoupled from simulation ticks. Process mouse-button transitions even without mouse movement. Verify pause, alt-tab, resize, v-sync changes, save/load, and long frames without stuck input or an unbounded physics catch-up loop.

## 8. Launcher and defaults

Build a separate, DPI-aware launcher using plain Win32 or another justified lightweight toolkit. It assembles an inspectable command line and starts the game; it owns no game state.

Provide:

- Data discovery limited to reasonable locations, header-checked `pak0.pak`, Browse, and clear missing-data errors.
- Mission pack and mod selection through the engine's filesystem rules, including documented extra archive support if implemented.
- Map, skill, and supported single-player or multiplayer options.
- Renderer, world mode, resolution, display mode, v-sync, FOV, style, density, and bloom.
- PhysX CPU/GPU selection with actual availability and fallback explanation.
- Restrained, Cinematic, and Cataclysm destruction presets, plus advanced material damage, support failure, secondary fracture, dust, and rubble controls.
- Individual visual-effect switches and a master decorative-effects switch.
- Reduced flashes, camera shake, gore, and reset controls.
- A collapsed Debug group for counters, geometry inspection, physics logs, and Vulkan validation.
- An extra arguments field and a live preview of the exact command line.

Persist settings to an INI next to the executable when writable, with a documented user-config fallback. Include Defaults. Validate Windows argument quoting and Unicode paths; use UTF-8 source and the appropriate Unicode Win32 APIs.

Default to Particle + Enhanced + Play density + Cinematic destruction settings when launching the explicitly labeled Destruction Sandbox. Offer a prominent Faithful Quake choice for ordinary campaign play, mods, demos, and multiplayer. Keep debug overlays off by default.

## 9. Verification must prove the experience

Implement repeatable launch and capture hooks early. Proposed command interfaces:

```text
-sim-checksum-tick <n>
-screenshot <path> -capture-tick <n>
-capture-sequence <directory> -capture-start-tick <n> -capture-ticks <n>
-test-explosion-tick <n> -test-explosion-origin <x,y,z>
-test-scenario <name> -test-seed <n>
-frames <n>
-counters
-validate-geometry
-benchmark-json <path>
```

Use simulation ticks for event placement and document capture timing, camera state, interpolation, fixed seed, backend, and build revision. Captures of exact PhysX motion require recorded state or a specified supported replay configuration; fixed random seeds alone are insufficient.

Required checks:

1. **Faithful determinism:** compare normalized gameplay-state hashes at fixed ticks with both renderers and visual features on/off. Hash relevant authoritative fields and RNG state where available, never raw pointers, padding, wall clocks, or renderer caches. Demo playback may have no live server state; test client demo state separately from a controlled local-server replay.
2. **Geometry correctness:** validate sample containment, winding, patch borders, generated volume closure, fragment seams, collision correspondence, and coverage. Treat boundary tolerances explicitly; points exactly on BSP planes are not reliably classified by a single naive point-contents check.
3. **Wall breach:** fire an actual weapon at an eligible stock-map wall, create a persistent opening, see through it, shoot through it, and walk through it in both renderers.
4. **Full section loss:** repeated impacts remove an entire eligible wall section without leaving stale fragments or invisible collision.
5. **Support collapse:** remove a support and observe the attached upper section fall, collide, and settle.
6. **Visibility:** reveal a room and entities formerly excluded by intact-map visibility; exercise camera movement through the opening.
7. **State persistence:** save and load a damaged area with matching topology and required rubble state; reset restores the initial map.
8. **Backend coverage:** verify the complete CPU path. Validate GPU mode on available supported hardware; label it unverified where hardware is unavailable.
9. **Budget pressure:** repeat explosions until configured limits are reached; check frame-time tails, memory plateau, useful diagnostics, and preserved collision.
10. **Integration:** test brush doors/platforms, water boundaries, stairs, thin walls, sloped faces, monster traces, trigger volumes, map changes, and launcher settings.
11. **GPU contracts:** assert packed layouts, reflection data, capacity bounds, synchronization, and cache invalidation.

Use both a redistributable procedural test arena and actual user-supplied maps. A synthetic arena can validate the pipeline, but it cannot substitute for stock-map extraction and destruction. Label synthetic events in capture captions.

Record moving features as frame sequences. Inspect them visually in addition to counters. A counter proving that bodies exist does not prove that an explosion looks correct.

## 10. Implementation order and milestone gates

Keep the work playable. Each milestone must have a launch command and concrete evidence. If a milestone fails, repair it before building layers that depend on it.

**Milestone 1 — Baseline:** build unmodified vkQuake, identify the integration points, capture classic output, establish timing and normalized state checks, and add renderer/world-mode selection.

**Milestone 2 — Solid particle world:** extract and cache BSP samples, implement watertight rendering, original textures, lightmaps, light styles, basic visibility, and runtime renderer comparison.

**Milestone 3 — Early destruction proof:** integrate CPU PhysX and Blast. Extract one eligible wall from a real user-supplied BSP, reconstruct its volume, fracture it, and render moving pieces in both renderers. Build the procedural arena to isolate failures.

**Milestone 4 — Real playable breach:** connect actual weapon events, replace damaged collision, repair conservative visibility, and prove that the player and shots traverse the opening. This is the first flagship gate; decorative polish must not displace it.

**Milestone 5 — Structural destruction:** generalize automatic extraction, persistent damage, repeated breaches, supports, complete wall-section failure, secondary impacts, rubble lifecycle, and sandbox saves. Validate several distinct stock-map structures and publish the supported/excluded cases.

**Milestone 6 — Complete Quake rendering:** finish alias animation, brush models, sprites, sky, liquids, fullbright handling, dynamic lights, and gameplay readability.

**Milestone 7 — Spectacle:** implement the complete explosion sequence, contact-driven effects, trails, gibs, HDR bloom, styles, and accessible controls. Capture before/after sequences with the same camera and event timing.

**Milestone 8 — Product:** finish the launcher, presets, reset and photomode tools, error handling, and documentation. Implement optional GPU PhysX acceleration after the CPU path is solid, then validate synchronization and fallback.

**Milestone 9 — Acceptance:** run determinism, destruction, persistence, memory-pressure, presentation, and visual checks. Publish measured results, defects, and remaining limitations.

Prefer small coherent commits when working in an initialized repository. Include relevant verification and measurements only when actually run. Inspect staged files so commits never include user game data or derived assets.

## 11. Data, documentation, and honesty

Never bundle or commit user `.pak`, `.wad`, `.bsp`, textures, models, extracted assets, or derived caches. Test geometry must be original procedural data or have explicit redistribution rights. Captures of supplied game content stay outside the distributable source package unless separately authorized.

Store caches in a writable user cache directory. Key them by asset content hashes, mod/search-path identity, extractor/schema versions, density, fracture seed and configuration, physics cooking version, and platform-sensitive format details. Validate headers and lengths; reject corrupt caches. Use atomic replacement and rebuild on any incompatible change.

Maintain:

- A feature table with Not started, Implemented, Verified, and Limited states, plus evidence links.
- Build and run instructions from a clean checkout, including exact dependency revisions.
- Performance results with hardware, OS, driver, backend, presets, scenario, commands, and capture revision.
- A bug journal recording symptoms, actual causes, fixes, and regression checks.
- A supported-geometry and mod-compatibility matrix.
- License notices and a dependency inventory.
- A limitations section covering inferred thickness, approximate interior lighting, unsupported structures, campaign implications, and unverified hardware.

Never invent FPS, latency, particle counts, memory use, compatibility, or successful test results. Targets belong in a separate column from measurements. Do not repeat performance anecdotes from another project as evidence for this one.

## 12. Definition of done

The required release is complete when:

- Both renderers run and preserve the same authoritative world state when switched.
- Faithful mode passes its gameplay-state checks with visual features enabled and disabled.
- The destruction sandbox automatically processes supported geometry from user-supplied maps.
- Actual weapons create persistent holes through eligible walls that players and projectiles can traverse.
- Complete eligible wall sections can be destroyed and unsupported connected structures collapse.
- The breach has real interior surfaces, correct collision, and correct visibility in both renderers.
- PhysX chunks visibly tumble, collide, and settle; meaningful rubble persists and survives save/load.
- The flagship explosion sequence is visually verified from impact through aftermath.
- All four appearance choices, documented presets, and the launcher work.
- The CPU physics path works without NVIDIA GPU acceleration. Optional GPU acceleration is honestly labeled according to its validation status.
- The required tests pass, budgets have real measurements, and no user game assets or derived caches are included in source control or the executable distribution.

Optional features include skeletal ragdolls, advanced metal deformation, dynamic indirect lighting, elaborate volumetrics, and destruction multiplayer. They must not postpone the required playable wall destruction.

**Start by inspecting the workspace and building the baseline. Reach the real wall-breach milestone early. Continue until the required release behavior is implemented and verified, or identify the exact external blocker with completed evidence.**
