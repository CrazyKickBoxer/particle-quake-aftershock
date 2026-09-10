# Progressive structural destruction

Rockets now chip eligible interior walls and progressively weaken their structural bonds. Pillars and bridge slabs can break into physical chunks as their connections fail. Damage accumulates instead of removing an entire panel on the first explosion. This uses the existing local singleplayer **Destruction sandbox** mode; cosmetic Holo Physics remains a separate system.

## Controls

In the launcher, select Destruction sandbox as the world mode. Under **Destruction & effects**, enable **Progressive destruction** and adjust **Structure toughness** and **Structural blast radius**. Each control has a yellow question-mark help bubble.

| Console variable | Default | Meaning |
| --- | --- | --- |
| `as_progressive` | `1` | Enable progressive damage and structural eligibility rules. Reload the map after changing this to rebuild geometry and supports. |
| `as_rocket_hits` | `4` | Nominal toughness, clamped to 2–12. Material, distance and support connections change the actual number of hits. |
| `as_chip_radius` | `72` | Local damage radius in Quake units, clamped to 24–160 and limited by `as_radius`. |

Broad walls can shed one small chip immediately. Narrow load-bearing structures resist that early chip so their entire support does not disappear on the first rocket. Subsequent hits weaken nearby bonds; metal is tougher than stone, wood is weaker, and connections to permanent geometry are reinforced. Aim at remaining edges to widen a breach: rockets can pass through an existing opening without damaging the original impact point again.

Vertical structures attach where their ends actually meet solid supports. Bridge slabs attach at the ends of their span. Supports that are themselves destructible connect through the fracture graph, allowing unsupported sections to fall rather than remaining permanently pinned.

## Geometry limits

Selection is deliberately conservative: paired, axis-aligned rectangular BSP surfaces must enclose solid material with exposed air on both sides. The outer shell, main floor, low steps, sky, liquids and ambiguous geometry stay protected. Simple interior walls, rectangular pillars and thin raised bridge slabs qualify when their BSP faces meet these checks. Complex subdivided or sloped structures and moving brush entities are not generally supported. This is not semantic recognition of every wall in every map.

Load a fresh map after installing this update. The structural geometry hash changed, so old structural damage sidecars may be rejected safely. New saves preserve accumulated bond damage and detached body state.

## Validation

- All five CTest targets passed, including structural physics, pressure, GPU contracts, gib motion and Holo Physics contracts.
- Physics wall fixture: one detached chunk after the first progressive hit, 32 after repeated damage; partially damaged state survived save/load.
- Pillar and bridge fixtures resisted the first hit, then released chunks after repeated damage.
- Generated BSP arena: five eligible structures and 129 chunks; the surrounding shell, main floor and low steps were excluded. Ten localized wall impulses increased detached chunks from 1 to 9.
- The actual rocket-launcher fixture recorded two explosion events and one detached chunk, with zero PhysX errors. Its legacy walk-through check did not pass because the chipped opening remained too small; this is not evidence of a traversable breach.
- Screenshots of the progressive wall test were inspected for a growing opening and detached debris.

Run `scripts/test_progressive.py` with the project's Python runtime for the generated arena. Optional panel indices are 0 (wall), 1 (pillar), 2/3 (bridge supports), and 4 (bridge slab).
