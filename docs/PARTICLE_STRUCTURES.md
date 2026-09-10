# Particle structures

The launcher’s **Particle structure** control changes particle geometry, placement, and depth. **Material colors** remains a separate control. The current default is [Neon Cathedral](NEON_CATHEDRAL.md), with **Fine density** and **3 layers**. Select **Particle surfaces** as the renderer.

| `as_structure` | Structure | Geometry |
|---:|---|---|
| 0 | Original square samples | Original regular, plane-aligned sample footprints |
| 1 | Soft Dust Cloud | Randomly sized eight-sided circular discs with soft, stippled edges, scattered through the surface layers |
| 2 | Solid Beads | Small smooth-lit sphere meshes with random sizes and packing |
| 3 | Broken Stone Chips | Irregular, solid angular chips with independently varied vertices and rotations |
| 4 | Scattered Microvoxels | Solid six-faced cubes with different sizes, rotations, spacing, and depth |
| 5 | Layered Flakes | Thin, closed hexagonal wafers with irregular outlines, overlapping like scales |
| 6 | Splinter Field | Long, narrow three-dimensional wedges pointing in different directions |
| 7 | Fiber Bundles | Pairs of curved, crossing ribbon strips with varied phases and directions |
| 8 | Goo Blobs | Smooth-lit, lobed three-dimensional blobs with overlapping silhouettes and wet highlights |
| 9 | Hollow Rings | Torus meshes with actual center holes, varied orientations, and overlapping layers |
| 11 | Neon Cathedral | Reference-inspired cyan/gold pinpoints, actual surface boundaries, magenta MDL monsters, and screen-space glow |
| 10 | Mixed Debris Cloud | Beads, chips, flakes, and dust together; larger pieces in the inner layer and smaller pieces above |

The new shapes apply to supported opaque BSP walls, floors, ceilings, moving brushes, structural rubble surfaces, and added explosion fragments. MDL characters retain the existing model-particle renderer. Original Quake sprite/trail effects retain their native rendering.

## Controls

```text
as_renderer 1
as_structure 9   // hollow rings; 1..10 select the new structures, 0 restores squares
as_layers 3      // 1..4 independently scattered depth layers
as_style 0       // source colors; 1..13 select the existing material treatments
```

Structure, layers, and colors change live. They do not reload the map, move collision geometry, or repair broken walls. **Surface density** changes the source sampling at the next map load. **Effect particle count** controls airborne burst counts independently. The launcher saves the structure and layer selections and includes them in its command preview.

## Placement and rendering

Each surface has four independently jittered sample sets. Centers are quantized to one eighth of a source texel and tested against the actual convex face after quantization. A stateless hash selects positions, scale, rotation, and depth without touching Quake's gameplay RNG. Camera movement and time do not reshuffle wall particles. Source-local coordinates keep the layers attached to moving brushes and rotating PhysX rubble. Switching shapes preserves the scatter points.

Meshes have surface normals and directional shading. A continuous, opaque sample substrate preserves solid wall coverage between the irregular pieces. Individual particles can overlap or overhang face edges slightly; collision still follows the underlying BSP or structural chunk.

World surfaces farther than 192 and 384 units from their expanded bounds use progressively shorter, deterministically shuffled sample prefixes and fewer layers. Beyond 768 units they retain the opaque substrate. This bounds the geometry cost; detail transitions can be visible. Chunk bounds and back-facing chunk surfaces are culled. Dense settings, rings, beads, and four layers cost more GPU time.

Dust uses stippled opacity on solid surfaces so it works with all engine transparency modes; its airborne discs use interpolated alpha. Explosion meshes tumble continuously and retain the existing directional burst trajectories. Their small GPU particles are decorative and use the existing approximate event-floor bounce. Large wall pieces use CPU PhysX/Blast. Gibs use the swept collision, floor sliding, and clipped goo-mark system described in [particle modes and goo](PARTICLE_MODES.md).

Goo blobs overlap and have lobed silhouettes; this is not an implicit fluid surface or a fluid solver. Fibers are curved visual meshes, not simulated connected ropes. Wafers and splinters move with their parent rubble chunk; they are not separately allocated PhysX bodies. These limits keep the geometry modes independent of authoritative gameplay.

## Verification

`scripts/test-structures.ps1` compares all ten modes plus the original samples using the same source colors, camera, map, and explosion. It writes angled before/during/after captures, checks the scatter fingerprint across modes and cold/warm cache, and rejects engine or PhysX errors. Captures are in `build/results/structures`; proprietary Quake imagery is excluded from distributable packages.

The GPU contract test reflects the new vertex input layout and checks repeatable, irregular, independently layered sampling inside a triangular surface. Existing physics, pressure, and gib-motion tests remain part of CTest. See [structure validation](STRUCTURE_VALIDATION.md) for results from the built executable.

## Fidelity upgrade

The launcher now exposes **High fidelity (Neon)** and **Floor reflections**. Mesh structures use blue-noise placement and smoother distance transitions; Neon adds filtered grains, denser monster silhouettes, temporal reconstruction, HDR bloom, and rough screen-space reflections. See [controls and limitations](PARTICLE_FIDELITY.md).
