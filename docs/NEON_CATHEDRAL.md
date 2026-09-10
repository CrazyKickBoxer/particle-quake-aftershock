# Neon Cathedral

The reference-inspired preset is **Particle structure → 11 / Neon Cathedral (reference)** in the launcher. It selects Particle surfaces, Fine density, and three layers. The preset supplies its own cyan/gold/magenta palette, so the Material colors dropdown is disabled while it is selected. The original ten structures remain available.

```text
as_renderer 1
as_structure 11
as_layers 3
as_fidelity 1
as_reflections 1
```

For a fresh command-line launch, add `-density fine`. Density changes require a map reload; structure changes apply live.

## Rendering

- Fine, circular, emissive grains use filtered footprints and stable blue-noise offsets in several shallow depth layers. A fixed hash selects points independently of animation time and camera movement.
- An additional boundary stream follows actual BSP polygon edges. Source material detail and staggered masonry lines add smaller luminous accents. Warm materials and selected boundary orientations produce gold; the main architecture is cyan.
- A nearly black opaque substrate preserves wall occlusion and readable architecture. Sky surfaces become dark blue-black while this preset is active.
- Supported Quake MDL monsters use magenta point silhouettes. Weapons and other supported MDL objects use cyan; view-weapon grains are smaller. Animation retains native interpolated poses.
- High fidelity uses HDR scene buffers, temporal reconstruction, four-scale bloom, contact shading, color spill, and optional screen-space floor reflections before HUD composition. The launcher has separate **High fidelity (Neon)** and **Floor reflections** checkboxes; both can also be toggled live in the console. See [Particle fidelity](PARTICLE_FIDELITY.md).
- Added explosion grains use soft cyan/gold sprites. Large rubble still uses PhysX/Blast; directional gibs, sliding, goo splashes, and structural saves retain their existing systems.

The preset uses the user's existing Quake architecture, characters, and assets. It does not recreate the reference's cathedral or armored character. Reflections use visible screen geometry and fade when their source leaves the screen. Ray tracing, depth of field, and fluid surfaces are not implemented. Liquids and unsupported model formats retain their native rendering. Temporal rejection reduces shimmer but does not eliminate it on fast animated silhouettes.

## Verification

`scripts/test-fidelity.ps1` validates the new HDR/reflection path and live toggles. `scripts/test-neon.ps1` captures e1m2, start, an e1m1 wall breach, and a scaled-rendering case; it covers OIT 0, 1, and 2 and checks the selected preset, layers, and PhysX error counts. GPU contract tests reflect the new model fragment bindings and all six screen-effects variants. The full engine suite also checks real rocket traversal, partial breaches, gameplay equivalence, and save/load.

Current captures are local in `build/results/neon/`; game imagery and original PAK assets are excluded from packages. See `docs/results/neon/` for the recorded build hash, commands, logs, and measurements. Older structure measurements describe the executable hashes printed in those records.
