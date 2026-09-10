# Implementation notes

## Simulation and ownership

`engine/Quake/aftershock.c` connects vkQuake's C engine to the C API in `src/aftershock_physics.h`. The C++ adapter owns PhysX and Blast resources. World queries, visuals, and structural simulation share committed damage state. A visual particle never becomes a structural actor.

The adapter uses 32 Quake units per physics unit, Z up, and gravity of -800 Quake units/s². CPU PhysX advances at 120 Hz. The accumulator clamps incoming duration to 0.1 seconds and limits catch-up to 12 steps; excess elapsed time is dropped rather than creating an unbounded stall. Render transforms are copied after completed simulation. Before the next server update, the previous asynchronous render task is joined. This makes the current snapshot immutable for render jobs; transform interpolation and a lock-free multi-buffer publication scheme remain future work.

QuakeC writes to explosion temp-entity streams are observed on the server. Up to 64 pending events carry copied origin, radius, damage and deterministic seed. The next boundary applies Blast bond damage with distance falloff and structural/environment occlusion. Unsupported islands detach as compound PhysX bodies. Further bond failures split a moving compound while retaining its pose and velocity. Up to four sufficiently energetic contacts per step can cause secondary damage; up to 64 copied contacts feed decorative effects.

Closed box shapes and static triangle meshes are created/cooked during map load. Moving brush meshes are cooked once per precached model, then instantiated as kinematic triangle actors and updated from server entity poses. Quake remains responsible for player/brush collision and trigger logic. Those kinematic actors are excluded from the rubble overlay query to avoid testing brush collision twice. Models dynamically introduced after precaching and broader mod-specific brush behavior need additional coverage.

## Geometry and collision

The automatic extractor pairs matching axis-aligned rectangular BSP faces. Candidate thickness must be 4–64 units, tangential dimensions at least 48 units. It samples solid contents at five depths and at cell corners, verifies empty space immediately beyond both exterior faces, and rejects overlapping candidates. It subdivides into approximately 24-unit cells; vertical panels anchor along their base, horizontal slabs around their perimeter. These are inferred structures, not recovered original editor brushes.

Each detached cell retains six closed visual faces and a rigid transform. Original exterior UV mappings remain tied to that cell; new interior faces use generated planar mappings and approximate ambient shading. Fragment dimensions use a small collision shrink to prevent explosive self-overlap. Moving fragments do not carry a valid original lightmap at their new location; their simpler shading is intentional and currently visible.

Authoritative Quake hulls retain their original partition and substitute empty-volume decision trees for detached material. Openings use overlapping maximal empty rectangles, then account for each hull's extents. Disjoint rectangle erosion was incorrect for irregular openings because it created invisible seams. Copies preserve the original hulls for reset and leave brush entities/contents handling in the native engine.

Dynamic rubble box sweeps overlay the modified world query. Minimum-translation normals allow the player to escape or slide along an initial overlap. Only the explicit player movement path pushes small rubble; read-only traces never apply impulses. Bodies above the 250-unit physics mass threshold remain substantial obstacles. The player is not a full PhysX character controller, and crushing interactions/monster pushing are not fully modeled.

Sandbox visibility ignores the original restrictive PVS for world/entity exposure. This is conservative and can cost performance on large maps. Faithful mode restores native visibility.

## GPU layouts

Static samples use two unsigned 16-bit face-local texture coordinates packed into one `uint32_t`. The quantum is 1/8 of a source texture texel; per-axis maximum nearest-encoding error is 1/16 texel. World-space error also depends on the inverse texture axes: a conservative bound is `0.0625 * (length(S_inverse) + length(T_inverse))` Quake units. This is not a universal 1/16-world-unit error claim.

Each face has 96 bytes of metadata: six float4 values for origin/footprint, two position axes, texture mapping, lightmap mapping and material parameters. Metadata is transiently copied into the native frame upload arena, while the sample buffer remains device-local. Four indexed vertex evaluations expand each sample into a plane-aligned quad. Border samples and a footprint based on spacing improve close/grazing coverage. Arbitrarily oversized faces are currently rejected instead of overflowing.

Opaque native Quake MDL samples use 16 bytes: three source seam-vertex indices and packed barycentric weights/tessellation. The shader reads the original keyframe VBO as storage data and interpolates source poses. The alias UBO is 116 bytes; its reflected offsets are asserted against compiled SPIR-V. `src/aftershock_layout.h` supplies the CPU sample/surface layout and Vulkan attribute offsets. `tests/contracts.cpp` checks SPIR-V instructions directly, not shader-source punctuation.

Cosmetic explosions use 32 metadata slots, up to 1,024 instances each (32,768 maximum evaluated particles). Analytic vertex trajectories generate smoke, grit and sparks, with a per-event floor estimate. They share native transparency/fog pipelines. Event metadata is small, but this does not establish a measured total 64-MiB effect budget. Full scene collision, compute compaction, depth-softened smoke, bloom and overdraw telemetry remain unimplemented.

## Persistence, caches, and resources

Sample caches use schema 4, geometry/UV/lightmap-layout/search-path identity, density, payload bounds and a checksum. Cache writes use atomic replacement and UTF-8-to-Windows wide-path conversion. A future extractor change must bump the cache version. Fracture and cooking data currently rebuild on load.

Structural saves use schema 2: map identity, event history, final bond health, body mapping, full poses/velocities/sleep state and checksum. Loading reconstructs Blast topology from final damage before restoring body state. Wrong map/schema/counts, invalid floats and corrupt checksums are rejected. Header/checksum failures leave current state intact; late structural incompatibility may require reloading the map. Saving a faithful game over an earlier destruction slot removes its obsolete sidecar.

Hard capacities: 8,192 structural chunks, 512 inferred panels, 64 pending explosion events, 65,536 saved event records, 32 cosmetic event slots, and 16,384 benchmark interval samples. A fragment is not silently removed to satisfy a visual budget. There is no separately measured preset-specific active-body scheduler yet; avoid treating the hard chunk cap as a supported frame-rate target.

The PhysX allocator reports requested live/peak bytes and verifies zero outstanding allocations after full shutdown in the pressure test. These counters exclude Blast/std::vector allocations, Quake allocations, graphics resources, textures, staging and render targets. Do not present them as total process memory or VRAM.
