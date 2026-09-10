# Neon Prism

An optional enhancement to the existing Neon particle structure, inspired by the
cyan/gold corridor reference. Built into `bin/vkQuake.exe`.

```console
neon_prism
```

Enables Neon, three layers, fidelity, and reflections. Holo Physics is independent
and retains its current setting. This preset adds brighter architectural edges,
sparse larger foreground glints, sharper floor reflections, stronger reflected
light, and slightly wider glow. It adds no particles or reflection render targets.

Live controls:

```console
as_reflection_strength 2.4
as_reflection_roughness 0.10
as_neon_glow 1.15
as_reflections 0
```

Strength ranges from 0–4; roughness from 0.03–0.7; glow from 0–2. Lower roughness
gives sharper reflected detail. `as_reflections 1` restores reflections.
`neon_prism 0` restores the original Neon optical tuning. `as_neon_prism` itself
toggles the edge/glint and polished-floor treatment without resetting other knobs.

The reflections use the existing GPU scene/depth textures. Rays can reflect only
geometry available on screen, and fade at screen edges; this is not an off-screen
ray-traced mirror. No CPU framebuffer readback is used.

Validation: Release build passed, and classic / Prism / Prism without reflections
were captured in E1M1 at 1280×720 on RTX 2050, Fine density, three layers, 4× MSAA.
Mean whole-frame GPU times were 6.146 / 6.335 / 5.988 ms respectively. These short
stationary tests are not a performance guarantee for other scenes. The test script
is `scripts/test_neon_prism.py`; captures and logs are in `build/neon-prism/`.
