# Itch.io page copy

## Title

Particle Quake: Aftershock

## Tagline (under 60 characters)

Quake as light and rubble: point-cloud renderer + real destruction

## Short description (card, ~200 characters)

A free vkQuake fork that renders the world as GPU point clouds with 14
material styles, a cosmetic reactive-physics layer, and real PhysX/Blast
wall destruction. Bring your own copy of Quake.

---

## Page body

**Particle Quake: Aftershock** is a fork of vkQuake (the Vulkan Quake
source port) that redraws every wall, floor, and monster as a dense
field of colored points instead of textured triangles, adds a cosmetic
GPU layer that makes those points react to explosions and gunfire, and
adds an optional physics sandbox where rockets can genuinely fracture
and remove wall sections. It's free, source-available, and built for
awareness, not revenue.

> ### ⚠ You need your own copy of Quake
>
> This is an **engine**, not a game. The download contains no Quake
> maps, textures, sounds, or other game data — just the modified
> executable and its source. To play, you need `id1/pak0.pak` (and
> `pak1.pak` for the full game) from your own legally obtained copy of
> Quake: the Steam or GOG release, or the free official shareware
> episode from id Software. Point the engine at that folder and you're
> playing.

### Install

1. Download and unzip `ParticleQuake-Aftershock-v0.1.0-win64.zip` anywhere.
2. Run `vkquake_launcher.exe` and click **Find Steam/GOG** — it checks
   the registry and known library folders for an existing Quake
   install (nothing is downloaded or changed). Don't own a copy? Click
   **Get shareware Quake** to open the official free shareware release
   on the Internet Archive. Or copy your `id1` folder next to
   `vkQuake.exe` yourself, or launch with `-basedir "<path to your
   Quake install>"`.
3. Run `vkQuake.exe` directly, or `vkquake_launcher.exe` for a menu
   with renderer and destruction-sandbox presets.
4. No Vulkan SDK or dev tools needed — just a Vulkan 1.1-capable GPU
   driver.

### Controls / cvars quick-start

Open the console (`~` or `` ` ``) and try:

```
r_holo 1              // switch to the point-cloud renderer
as_style 2              // pick one of 14 material styles, 0..13 (2 = inferno)
as_structure 11        // Neon Cathedral preset (cyan/gold architecture)
holo_physics fine       // enable the cosmetic reactive-physics layer
as_mode destruction    // enable the PhysX/Blast wall-destruction sandbox (reloads the map)
r_holo 0               // back to stock vkQuake rendering
```

Full reference for every added console variable, including ranges and
defaults, is in `HOLO.md` in the download.

### System requirements

- Windows 10/11, 64-bit
- A Vulkan 1.1-capable GPU and driver (most GPUs from the last ~10 years)
- Your own copy of Quake (see above)
- Measured on: AMD Ryzen 5 7535HS, NVIDIA GeForce RTX 2050 — see
  `RELEASE_REPORT.md` in the download for actual measured frame times.
  Not yet tested on AMD/Intel GPUs.

### License and source

GPL-2.0-or-later, same as vkQuake. Full source for this fork, including
everything added on top of vkQuake, is published at:
`https://github.com/CrazyKickBoxer/particle-quake-aftershock`

### Credits

- **id Software** — original Quake and its GPL-licensed engine release
- **Quakespasm developers** — the source port vkQuake builds on
- **Axel Gneiting** — vkQuake
- **NVIDIA** — PhysX and Blast (BSD-3-Clause), used for the destruction sandbox
- Renderer, particle, and destruction additions by Josh Nicholls

---

## Tags

`fps`, `source-port`, `quake`, `graphics`, `vulkan`, `experimental`, `physics`, `destruction`, `retro`, `open-source`

---

## Capture list

Six screenshots and one 20–30s video loop, chosen for visual impact at
thumbnail size (strong silhouettes, high color contrast, readable at
small size):

1. **e1m1, panel 9 wall breach, mid-explosion** — the signature moment:
   camera facing the wall as the rocket hits, cracks radiating, dust
   and point-cloud debris mid-flight, revealed room glowing behind the
   breach. `as_mode destruction`, `as_style enhanced` or `inferno`.
2. **e1m1, same breach, aftermath** — same framing, seconds later:
   settled rubble, player standing in the new opening looking through
   to the far room. Shows the "you can walk through it" payoff clearly.
3. **`start` (the hub map), Neon Cathedral preset** — `as_structure 11`,
   cyan/gold architecture with a magenta monster silhouette in a
   doorway or archway. High contrast, reads instantly as "this is not
   normal Quake" at thumbnail size.
4. **e1m2, a corridor or open room, `as_style volcanic` or `spectral`**
   — a close-up on a textured wall section showing the animated
   material style clearly (breathing orange seams, or moving green
   contours), player close enough that individual points are visible.
5. **e1m3, monster death with Holo Physics** — `r_holo_phys_death 1`,
   a monster mid-dissolution into its point-cloud "GPU snapshot" cast,
   ideally back-lit or against a dark corridor for contrast.
6. **Mixed Debris Cloud structure, wide shot** — `as_structure 10`,
   a wide shot of a rubble-strewn room after multiple explosions,
   showing scattered debris density and layered depth without a
   player model in frame (good for a establishing/environment shot).

**Video loop (20–30s):** the e1m1 rocket wall-breach sequence start to
finish — approach, rocket impact, wall rupture with flying debris,
settle, then the player walking through the new opening into the
revealed room. This is the exact scripted sequence the launcher's
"Rocket demo setup → Launch Aftershock" option already runs, and
`exports/ParticleQuake-Aftershock-60s.mp4` in the repo is an existing
capture of it — check whether it already fits this loop before
re-recording.
