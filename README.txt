Particle Quake: Aftershock
==========================

This is a fork of vkQuake that replaces world/model geometry with a
GPU point-cloud renderer, adds a cosmetic "Holo Physics" GPU disturbance
layer, and adds an optional PhysX/Blast wall-destruction sandbox.

You need your own copy of Quake. This is an ENGINE, not a game - it
contains no Quake maps, textures, sounds, or other game data.

1. Get your game data
----------------------
Buy Quake on Steam or GOG, or download the free official shareware
episode from id Software. Either way you need a folder containing an
`id1` directory with `pak0.pak` (and `pak1.pak` for the full game).

2. Install
----------
Unzip this archive anywhere. Then either:

  - Open vkquake_launcher.exe and click "Find Steam/GOG" - it checks
    the registry and known library folders for an existing Quake
    install and fills in the data folder automatically. Nothing is
    downloaded or changed; it only looks. If you don't own a copy,
    click "Get shareware Quake" to open the official free shareware
    release on the Internet Archive in your browser.
  - Or copy your `id1` folder next to vkQuake.exe, so you have:
      <this folder>\id1\pak0.pak
      <this folder>\id1\pak1.pak
  - Or launch with -basedir pointing at your existing Quake install:
      vkQuake.exe -basedir "C:\Program Files (x86)\Steam\steamapps\common\Quake"

No Vulkan SDK or other developer tools are required to play - only a
Vulkan 1.1-capable GPU driver (any GPU from roughly the last decade).

3. Launch
---------
Run vkQuake.exe directly, or vkquake_launcher.exe for a menu with
renderer/quality presets and destruction-sandbox options.

First-run check
----------------
From a normal launch, open the console (usually the ~ or ` key) and run:

    r_holo 1
    as_style inferno

The world should redraw as dense fields of colored points instead of
textured triangles. Then run:

    r_holo 0

to confirm stock vkQuake rendering returns exactly as before. See
HOLO.md for the full list of renderer/destruction console variables,
including the destruction sandbox (`as_worldmode 1`) and the cosmetic
Holo Physics disturbance layer (`r_holo_physics 1`).

License and source
-------------------
GPL-2.0-or-later. See LICENSE.txt and NOTICE.md. Source, including
everything added on top of vkQuake, is published alongside this build.
