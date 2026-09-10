# Particle fidelity and reflections

Select **11 / Neon Cathedral (reference)** in the launcher. **High fidelity (Neon)** enables the new rendering path; **Floor reflections** independently enables reflected scene detail. Both start enabled. The launcher saves the switches and shows their exact commands in its preview.

In the Quake console, changes apply immediately:

```text
toggle as_fidelity
toggle as_reflections
```

Explicit settings are `as_fidelity 0/1` and `as_reflections 0/1`. Reflections require fidelity and Neon Cathedral. Switching either setting preserves wall destruction, gibs, and saves. Density still requires a map reload.

## What changed

| Upgrade | Implementation |
|---|---|
| Filtered footprints | Derivative-filtered Gaussian cores and softened coverage on wall/model grains reduce harsh subpixel edges. |
| Temporal stability | Two HDR history images, camera reprojection, depth rejection, neighborhood color clamping, and reduced history weight for moving/reactive pixels. History resets on cuts, map changes, video-resource recreation, and fidelity reactivation. |
| HDR bloom | Scene and intermediate images use RGBA16F. Bright grains retain energy above display white until four-scale bloom and a hue-preserving tone curve are applied before the HUD. |
| Blue-noise placement | A deterministic toroidal best-candidate pattern separates grains, with independently shifted layers. The pattern's minimum spacing exceeds 0.9 cells; tests also check its wrap seam. Mesh modes 1–10 use the pattern with polygon clipping. |
| Normals and contact shading | Surface/model normals control grain shading and monster rims. Depth-derived normals support modest local contact shading and color spill in the fidelity pass. |
| Distance transitions | Mesh modes use continuously reduced, spatially shuffled draw prefixes and shrink out at their far limit. Neon uses filtered footprints plus history instead of abrupt density tiers. |
| Monster silhouettes | Supported hostile MDL models receive two independently jittered samples per original sample, normal-based rim emphasis, and slightly wider silhouettes. |
| Reflections and color spill | Upward-facing surfaces trace up to 32 screen-space steps with five refinement steps. Roughness blur, Fresnel weighting, depth/thickness rejection, and screen-edge fading control reflected light. Nearby visible bright geometry supplies a small contact color-spill approximation. |

The original cyan/gold architectural boundaries, magenta monsters, dark sky, and directional gib/goo systems remain available.

## Practical limits

- Reflections can show only geometry present in the current color/depth images. Off-screen objects, hidden surfaces, and rays obscured by foreground geometry fade out. This is not ray tracing or a second mirrored scene render.
- Temporal rejection uses camera motion and current/previous depth and color. It does not have per-object velocity buffers; rapid animated silhouettes, thin geometry, and disocclusions can still shimmer.
- Contact color spill is a local screen-space approximation, not scene-wide emissive lighting or global illumination. Reflection roughness is inferred from surface orientation, not authored material maps.
- Particle sampling uses a repeating blue-noise tile with per-face/layer shifts. It is not an unbounded Poisson distribution. Neon samples retain the existing BSP source stream and shallow surface offsets.
- Existing Quake assets supply the architecture and characters. Native liquids and unsupported model formats are not converted into particle geometry. Depth of field and fluid meshing are outside this upgrade.
- The HDR intermediate allocation remains present with fidelity disabled; the toggle skips the new temporal/reflection/bloom work. Palette rendering uses the existing palette path instead of the fidelity pass.

## Performance and verification

The launcher starts high-fidelity Neon with MSAA disabled because the filtered/temporal path handles fine-grain smoothing at lower cost. Override it in Additional command-line options with, for example, `+vid_fsaa 4`. MSAA remains supported and has a separate multisampled-depth shader.

`scripts/test-fidelity.ps1` captures matched off/on/reflection cases at 1280×720, Fine density, and three layers. It also exercises OIT 0/1/2, 4× MSAA, render scale 2, camera motion/cuts, video restart, and five live toggle transitions during a 36-piece wall breach. It records actual MSAA and render scale in the benchmark JSON. GPU times include capture frames and are not presented FPS guarantees.

`gpu_contracts` checks both fidelity shader descriptor layouts and all uniform offsets, alongside the existing surface/model contracts and the blue-noise separation test. The launcher self-test checks all four combinations of the two switches as well as its 48 structure/layer combinations.

Measured logs and executable identifiers are in `docs/results/fidelity`. The engine regression suite covers real rocket traversal, partial destruction, rubble pushing, faithful gameplay hashes, and structural save/load. Vulkan validation-layer execution is a separate optional `-Validation` test mode and requires an installed Khronos validation layer.

## Rocket smoke correction

Neon explosion smoke now uses one puff per 64 effect particles instead of one per 16: 75% fewer puffs. Puffs last approximately 0.75�1.3 seconds at full strength, are smaller and half as opaque, and use camera-facing geometry. Native rocket exhaust expires sooner. Neon sparks are also smaller and shorter-lived, removing the large overlapping discs that obscured explosions.

The existing impact trace supplies a ground plane, including its slope. The full smoke/neon-spark billboard footprint stays above that local plane. This is a local impact-plane constraint, not general per-particle collision against every wall, stair, or moving surface.

`-test-smoke-floor` with the existing synthetic explosion fixture exercises an impact two units above ground. The fix was captured under all three transparency modes at maximum effect amount. Real-rocket traversal and the four CTests also passed. Records are in `docs/results/smoke-fix`.
