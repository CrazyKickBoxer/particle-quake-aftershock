# How the particle renderer works

Reference for anyone modifying the point-cloud renderer. Every claim here is
cited to `file:line` in this checkout — verify before relying on it, since line
numbers drift.

There are **three independent systems**. Don't confuse them:

| System | What it draws | Lives in |
|---|---|---|
| **World splats** | BSP surfaces (walls/floors/ceilings) as points | `aftershock.c`, `aftershock.vert/.frag` |
| **Alias splats** | MDL models — monsters, weapons, pickups | `gl_mesh.c`, `r_alias.c`, `aftershock_alias.vert/.frag` |
| **Holo Physics** | A cosmetic GPU disturbance layer over the above | `holo_physics.inc`, `holo_physics.comp` |

Holo Physics is a separate compute simulation and is **not** involved in how a
surface or a monster gets its colour. Ignore it for colour work.

---

## 1. World splats (BSP surfaces)

### Baking (map load)

Each surface is tessellated into a lattice of texel positions. The step comes
from `as_density`, clamped 1–12 (`aftershock.c:496`) — lower is denser. Extra
samples walk polygon edges so seams don't gap.

Each sample is **4 bytes**: a packed `(u, v)` surface-texel coordinate at 1/8
texel precision.

```c
typedef uint32_t as_sample_t;                         // src/aftershock_layout.h:8
AS_LAYOUT_ASSERT(sizeof(as_sample_t) == 4, ...);      // src/aftershock_layout.h:17
```

Per-surface metadata (origin, texture axes, lightmap, params) is a separate
**96-byte** `as_surface_gpu_t` (`src/aftershock_layout.h:18`), uploaded once per
draw call and shared by every point on that surface. The debris/structure
variant adds a third **144-byte** struct.

Results are cached to disk keyed by a geometry+density hash, so reopening a map
doesn't re-tessellate.

### Drawing

Visibility is **per-surface and CPU-side** — it reuses the engine's existing PVS
bitmask plus a frustum cull:

```c
if (((uint32_t *)as_model->surfvis)[i / 32] & (1u << (i % 32)))   // aftershock.c:1177
```

Each visible surface then issues **one ordinary instanced draw**, six indices
(one quad) instanced once per point:

```c
vulkan_globals.vk_cmd_draw_indexed (cbx->cb, 6, f->count, 0, 0, 0);  // aftershock.c:1153
```

> **There is no GPU culling, no compaction, and no indirect draw in this path.**
> The sample buffer is a plain Vulkan **vertex** buffer (not an SSBO), consumed
> by fixed-function instanced vertex fetch. Don't go looking for a compute
> prepass — there isn't one. (Holo Physics does use SSBOs and indirect dispatch,
> but that's the separate system.)

### Colour

Fragment colour is decided in `aftershock.frag` by `as_style` (0–13). All 14
styles are **fragment-shader colour/brightness effects only — none move a
vertex**. Structure mode 11 ("Neon Cathedral") bypasses `as_style` and uses its
own cyan/gold palette.

---

## 2. Alias splats (monsters, weapons — the NPC path)

This is the path that matters for NPC colour.

### Baking (model load, `gl_mesh.c:477-491`)

Only for classic Quake MDL (`PV_QUAKE1`). For each triangle:

1. Measure the longest edge **across all animation poses**.
2. `n = clamp(ceil(longest / 2.5), 1, 64)` subdivisions.
3. Emit a barycentric lattice of `(n+1)(n+2)/2` points.

Each sample is **16 bytes** (4 × `uint32`):

| Word | Contents |
|---|---|
| 0,1,2 | The triangle's three vertex indices |
| 3 | `x/n*4095` (bits 0–11) \| `y/n*4095` (bits 12–23) \| `n` (bits 24–31) |

Hard cap **250,000 samples per model**; exceeding it prints a warning and falls
back to classic triangle rendering (`gl_mesh.c:485`). Samples live in the
model's vertex buffer, exposed to shaders as an SSBO via `aftershock_set`
(`gl_mesh.c:613`).

Note what is *not* stored: no per-frame sample positions, no duplicated
animation buffers. Samples reference pose vertices and are interpolated on the
GPU.

### Vertex shader (`aftershock_alias.vert`)

Per sample, one instanced quad. It:

- Reads the 3 vertex indices + packed barycentric from the SSBO.
- Fetches `pose1`/`pose2` positions and lerps by `u.blend` → **animation**.
- **Computes `uv` by barycentric-interpolating the triangle's three texcoords.**
- In neon mode: hash-jitters the barycentric position, builds a screen-facing
  quad sized by a per-sample random radius, and sets a **flat** colour.

The flat colour is this single line:

```glsl
color = vec4(neon==3u ? vec3(1.,.015,.42) : vec3(.015,.68,1.), .6+float(h&255u)/255.);
```

- `neon==3` → **magenta** — monsters
- otherwise → **cyan** — weapons and other MDL objects
- alpha carries a per-sample random brightness

### Fragment shader (`aftershock_alias.frag`)

```glsl
vec4 source = texture(diffuse_tex, uv);        // skin IS sampled
vec3 c = source.rgb * color.rgb * 2.;          // textured path
if ((u.flags&1u)!=0u) c += texture(fullbright_tex, uv).rgb;
if (neon != 0u) {
    ...
    c = color.rgb * (core + ...) * color.a * 1.9 * coverage;   // source DISCARDED
    c += vec3(1) * pow(core,4.) * .15;
    if (fidelity) c *= 3.;
}
```

**The non-neon path is already fully textured.** The neon branch recomputes `c`
from the flat `color.rgb` and throws `source` away.

### Flag bits (`r_alias.c:161-169`)

| Bit | Meaning |
|---|---|
| `0x1` | Fullbright texture present |
| `0x2` | Unlit / fullbright lighting |
| `0x10` | Neon alias splat active |
| `0x20` | **Entity is a monster** |
| `0x40` | Viewmodel (radius × 0.24) |
| `0x80` | Alphatest (`discard` if `a < .666`) |
| `0x100` | Fidelity (adds rim lighting, `c *= 3`) |

`neon = (flags>>4)&3` — so `0x10` alone → 1 (cyan), `0x10|0x20` → 3 (magenta).

Monsters are identified by **hardcoded model-name substring** (`r_alias.c:167`):

```c
{"soldier","ogre","demon","dog","knight","hknight","wizard",
 "shalrath","shambler","zombie","fish","boss","oldone","enforcer"}
```

Custom-mod monsters not on this list render cyan, not magenta.

---

## 3. Task: make NPCs use their skin texture instead of flat magenta

**Goal:** NPCs render in full colour derived from their own MDL skin, with the
particles mapping the texture, instead of a single flat magenta.

**The good news:** everything needed is already in place. `uv` is already
computed per sample in the vertex shader, `diffuse_tex` is already bound, and
`source` is already sampled at the top of the fragment shader. The neon branch
just ignores it. This is a small shader change, not a pipeline change.

### Where to edit

`engine/Shaders/aftershock_alias.frag` — the `if (neon != 0u)` branch. Modulate
by `source.rgb` instead of using `color.rgb` alone. Roughly:

```glsl
vec3 tint = (neon == 3u) ? source.rgb : color.rgb;   // monsters use skin, others keep cyan
c = tint * (core + ...) * color.a * 1.9 * coverage;
```

### Gotchas — read these before you start

1. **Keep the `core` gaussian.** That term is what makes each splat read as a
   round glowing point rather than a flat square. Replace the *tint*, not the
   falloff.

2. **Expect it to get darker.** `color.rgb` is a near-1.0 emissive colour;
   `source.rgb` is a palettized Quake skin, mostly much darker. You will
   probably need a gain, and/or a saturation push, or monsters will turn into
   dim mud against the neon architecture. Tune against `shambler` (pale) and
   `ogre` (brown) — they fail in opposite directions.

3. **Preserve `color.a`.** It carries the per-sample random brightness that
   keeps the point cloud from looking uniform.

4. **Don't break the fidelity path.** `0x100` adds rim lighting and `c *= 3`.
   Rim lighting multiplies `color.rgb` in the *vertex* shader
   (`color.rgb *= .72+rim*1.5`), so if you move the tint source to the fragment
   shader the rim term no longer applies to it — decide deliberately whether
   you want that, and don't let it silently drop out.

5. **Leave weapons cyan unless asked.** Gating on `neon == 3u` keeps the
   viewmodel and pickups on the existing look. Changing everything at once
   makes it much harder to judge.

6. **Add a cvar.** Everything else in this renderer is toggleable
   (`as_smw`, `as_fidelity`, `as_nails`…). Follow the pattern: declare in
   `aftershock.c`, extern in `aftershock.h`, register in `AS_Init`, thread into
   the alias UBO as a new flag bit in `r_alias.c:161-169`, and test it in the
   shader. Suggested: `as_neon_npc_texture`, default `1`.

7. **The 250k sample cap still applies.** A high-poly replacement model may
   already be falling back to classic triangles, in which case none of this
   affects it. Check for the `Aftershock: model sample limit` console warning.

### Build and verify

```powershell
.\scripts\build.ps1
```

`aftershock_alias.frag` is compiled once (no `HOLO_PHYSICS` variant, unlike
`aftershock.frag`), so a single build covers it.

**You cannot verify this without game data.** It needs a real `id1/pak0.pak`.
Launch with the Neon Cathedral preset and look at an actual monster:

```
as_renderer 1
as_structure 11
map e1m2
```

A shader compile error fails the build loudly, so a green build means the syntax
is fine — it says nothing about whether it looks right.
