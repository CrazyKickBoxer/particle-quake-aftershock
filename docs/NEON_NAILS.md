# Neon architecture and nail ricochets

Enable particle rendering, structure 11, and `as_fidelity 1` for the architectural upgrades. `as_reflections 1` keeps floor reflections enabled. `as_nails 1` enables nail cosmetics; `toggle as_nails` switches them at runtime. `as_effects 0` disables the new weapon effects, and `as_reduced_flashes 1` suppresses pressure refraction and wall pulses and limits spark brightness.

The five selected upgrades are:

1. Architectural edge hierarchy: map-load adjacency identifies shared coplanar BSP cuts and excludes them from the bright edge stream. Actual creases and open boundaries retain highlights. Surface texture detail remains visible.
2. Layered neon materials: amber inner grains, restrained pale middle grains, and cyan outer grains occupy separate stable depth layers.
5. Wall shockwaves: nearby rocket explosions send a thin expanding band of light and slight displacement across the existing wall grains. Each surface uses one active explosion to keep shader work bounded.
6. Molten fracture rims: newly detached chunks expose interior edges that cool from pale white through orange to cyan over three seconds. This is visual state; the existing destruction simulation is unchanged.
10. Needle sparks: Neon explosion sparks stretch along their projected velocity, with narrow widths and capped lengths. Their complete billboard footprint stays above the event's local floor plane.

## Nail effects

Native `TE_SPIKE` and `TE_SUPERSPIKE` events trigger cosmetic ricochets. Recent client projectile positions provide the incoming direction; read-only world/brush traces recover the contact normal. When the protocol provides no recent projectile, view direction and bounded axis probes supply a fallback. The actual projectile never ricochets or slows down.

Each impact draws 48 luminous streaks, eight small faceted debris chunks, and two expanding pressure rings. Sparks scatter into the surface's outward hemisphere with a reflected-direction bias. Their animation eases independently of gameplay time. Moving nails leave short-lived circular pressure trails. The fidelity compositor refracts background pixels around one nearby pressure wave, reusing its existing resolve sample rather than adding another screen pass. HDR emissions feed the existing bloom and light-spill effects.

Temporary irregular scars are clipped to the struck BSP face and follow moving brush models. They fade after six seconds and work with gibs and goo disabled. The crater is a surface illusion plus detached cosmetic chips: it does not cut collision holes or trigger structural damage. Chips use analytic motion with a struck-plane constraint and a sampled floor height, not full scene rigid-body collision.

Budgets: 64 shared trail/impact events, 128 reserved scar slots, at most 104 instances per impact, and a 1,200-unit event draw range. Trails emit at most once per 40 ms per tracked projectile and are hidden within 48 units of the camera. No gameplay RNG calls, new physics bodies, or global time scaling are used. These budgets limit cost; unchanged FPS on every GPU is not guaranteed.

## Verification

`scripts/test-nails.ps1` fires the real stock nailgun and super nailgun, compares normalized gameplay state with cosmetics disabled/enabled, exercises all three transparency modes, and checks that no structural chunks detach. It also disables gibs and goo to exercise independent scar rendering. Captures, commands, timing JSON, and logs are written to `build/results/nails`.

The existing structural physics, pressure, GPU layout, gib motion, real-rocket, and fidelity tests cover the shared paths. Vulkan validation requires a separately installed validation layer; a normal smoke test is not equivalent to a validation-layer run.

The verified Release executable has SHA-256 `409C0D175BCBF5E62E45F642D86EAA8D846C044B735FB24562F634FF4F5823C0`. All four CTests, both real-rocket traversal cases, and seven fidelity cases passed. Real nail/supernail shots produced gameplay hash `659fe4b2ffc0a54d` under all four nail test configurations, with zero structural detachments. At 1280×720, the matched OIT-1 run measured 4.542 ms mean GPU time with nail cosmetics off and 4.619 ms on. These short, capture-inclusive local measurements are not an FPS guarantee or a before/after benchmark of the five architectural changes. Evidence is in `docs/results/neon-nails`.
