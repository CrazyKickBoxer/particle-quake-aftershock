# Particle modes, projected gibs and goo

For actual particle silhouettes, random packing, and depth layers, use the separate [Particle structure control](PARTICLE_STRUCTURES.md). The styles below control colors and material effects.

Choose **Particle surfaces**, then an **Material colors** in the launcher. The appearance can also change live through `as_style`. All ten additions apply to opaque BSP walls/floors, moving brushes and structural rubble. Their matching explosion effects work with either renderer. They retain the underlying collision geometry and source textures; they do not turn the map into a fluid simulation.

| ID | Appearance / `-style` value | Surface and burst behavior |
|---|---|---|
| 4 | Living Stone / `living-stone` | Traveling stone ripples, granular shading and airborne stone grit |
| 5 | Volcanic / `volcanic` | Dark crust, breathing orange seams, rising embers and hot droplets |
| 6 | Sandstorm / `sandstorm` | Animated dune ridges, fine grains and sideways drifting dust |
| 7 | Crystal / `crystal` | Facets, diagonal luminous veins, glints and rotating long shards |
| 8 | Corrupted Flesh / `corrupted-flesh` | Slow organic pulse, branching fibers, red spores and stretched fragments |
| 9 | Industrial Rust / `industrial-rust` | Oxidized paint chips, pitting, hot flecks and tumbling flat flakes |
| 10 | Spectral / `spectral` | Moving green contours, ghostly motes and curling burst trajectories |
| 11 | Frozen Ruins / `frozen-ruins` | Frost lattice, blue facets, narrow ice needles and low mist |
| 12 | Electric Grid / `electric-grid` | Running charge along surface channels and segmented blue arcs |
| 13 | Cosmic Dust / `cosmic-dust` | Star grains, drifting nebula bands, spiral clouds and expanding rings |

The original IDs remain 0 Faithful, 1 Enhanced, 2 Inferno, 3 Inferno Color. Mode changes require no map reload. Surface density changes take effect when a map loads.

## More particles

The launcher has separate **Surface density** and **Effect particle count** settings. Fine and Showcase increase the persistent wall sample count. Effects have four levels:

| `as_particle_amount` | GPU burst particles per full explosion | Maximum nearby surface motes |
|---|---:|---:|
| 1 | 2,048 | 6,144 |
| 2 (default) | 4,096 | 12,288 |
| 3 | 6,144 | 18,432 |
| 4 | 8,192 | 24,576 |

These are bounded submitted instances, not all necessarily alive or visible simultaneously. There are at most 32 burst emitters and 48 nearby visible surface triangle emitters. Higher settings increase graphics cost. `as_effects 0` disables additional airborne effects and gore. `as_reduced_flashes 1` freezes animated surface highlights and limits additional particle brightness; stock Quake lighting remains native.

## Gib motion and surface goo

Rocket explosion handling copies the projectile's travel direction before QuakeC removes it. Cosmetic gib and droplet launches retain forward momentum with independent random spread and upward lift. Remote clients/demos infer direction from recently observed rockets; explosions without a matching projectile use a radial burst.

Gibs run a fixed 120-Hz cosmetic simulation, with swept probes against current BSP walls and moving brushes, plus queries against real PhysX rubble. Impacts consume the remaining timestep, use low restitution, and preserve tangential motion. Floor drag makes chunks slide, smear and eventually settle. Large fragments use the user's already precached Quake gib models; mods without those assets get a small procedural fragment. Native flying gib entities also receive cosmetic motion proxies. Authoritative edicts and the gameplay RNG are left untouched.

Fast impacts create irregular blood/goo splashes. Wall splashes add short downward runs; floor contacts leave elongated smears. Decal polygons are clipped to the struck BSP face, stored in brush-local coordinates, and revalidated against their supporting geometry. Marks fade and are removed when their supporting wall opens. Dynamic rubble is collidable but does not receive attached goo decals. Drips are projected marks, not simulated liquid flow.

`as_gibs 0` disables added gib motion and restores native gib rendering. `as_goo 0` keeps fragments but removes added surface marks. The master particle-effects switch disables both. The pools are capped at 384 cosmetic bodies, 128 native-gib associations and 768 marks. Small droplets live up to 2.5 seconds, chunks 8–12 seconds, and marks 24 seconds. Effects clear on map changes, loads and demo rewinds; they are not part of the structural save sidecar.

## Validation

`scripts/test-particle-modes.ps1` runs all ten appearances through an e1m1 wall explosion, writes before/during/after captures, and tests a procedural room for forward launches, wall splashes and floor smears. `aftershock_gib_tests` checks 4,096 forward launches, swept high-speed impacts, a long ground slide, settling and deterministic replay. `as_gore_stats` exposes counters during play.

The engine keeps stock blood/trail events alongside the enhancement. Gameplay-affecting gib interactions retain native Quake behavior; cosmetic proxy positions are visual only. Arbitrary geometry fracture, PhysX GPU simulation, fluid meshing and HDR bloom remain outside this implementation.
