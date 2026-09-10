# Particle Quake launcher

Open `bin/ParticleQuakeLauncher.exe` or the `Particle Quake Launcher.lnk` shortcut in the project folder.

The redesigned native Windows launcher groups settings across ten searchable pages. Yellow **?** buttons beside each setting open an explanation with dependencies, performance implications, and console names where applicable. Click a bubble to dismiss it. Keyboard navigation is supported, and focused settings scroll into view.

## Pages

- **Game & launch:** Quake data, expansion, mod, map and difficulty.
- **Display:** window mode, resolutions through 4K, FOV, VSync, frame cap, MSAA, gamma and contrast.
- **Particle surfaces:** renderer, all 12 structures, 14 material palettes, density and layers.
- **Neon & reflections:** fidelity, reflections, Prism, reflection strength/roughness and glow.
- **Destruction & effects:** world mode, progressive destruction, structure toughness, structural blast radius, destruction presets and numeric radius/damage, effects, camera shake, reduced flashes, particle amount and nail ricochets. See [Progressive destruction](PROGRESSIVE_DESTRUCTION.md) for supported geometry and tuning.
- **Holo Physics:** enable, budget, spring settling, blast/suction, sprays, collision and restitution.
- **Gore & death:** directional gibs, goo, Holo gore, cap/lifetime/gravity and death dissolution.
- **Wakes & atmosphere:** projectile/player wakes, lightning, liquids, teleports, dust, shudder, turbulence and debris lights.
- **Audio & mouse:** sound/music volume and mouse sensitivity.
- **Advanced:** diagnostics, Holo overlay and extra engine/mod arguments.

These cover the project's renderer/effect configuration and common game settings. Additional vkQuake/mod console options remain available through **Extra launch arguments**, which are appended last and may override the controls.

## Presets and saving

**Neon Prism** sets its required particle structure, fidelity and reflections, plus the polished reference values. **Holo Fine** enables cosmetic Holo Physics and its balanced budget/settling/dust settings. Neither button changes difficulty, map, world destruction mode, audio or unrelated display settings.

**Reset this page** resets only the selected category and preserves the Quake data path. **Save settings** saves without launching. The exact command stays visible and can be copied. Launch validates numeric ranges, integer-only budgets, command length and the Quake PAK header before starting the game.

Existing `aftershock.ini` selections remain compatible. Additional options use the `Settings` section. The existing per-user fallback is retained when the executable folder is not writable. Changes take effect on the next launch; this launcher does not remotely change a running game.

The old `vkquake_launcher.exe` was open during installation, so the new build is also installed as `ParticleQuakeLauncher.exe`. Close the old launcher before using the new one to avoid two windows saving competing selections. Future full builds update both executable names when they are not in use.

## Verification

Release build passed. `--selftest` checks Windows/Unicode argument quoting, PAK directory bounds, structure/layer/fidelity combinations, numeric rejection, presets, INI round trips, search, all ten pages, scrolling, help content and layouts at 96/144 DPI. Screenshots are written beside the tested executable. Its dedicated test INI does not modify the user's settings; the installed user's INI hash was verified unchanged.

`scripts/test_launcher_smoke.py` starts a separate 90-frame game run using the generated configuration, an isolated user directory, and muted audio output. The generated launch passed with 153 arguments, exit code 0, and no unknown commands. This is launcher integration validation, not a new renderer performance benchmark.

## Quality presets

Use **Quality presets** in the header, then select a level under **Display > Quality preset**. Selections apply immediately to the launcher settings, ready for the next launch. The selector recognizes saved settings automatically and shows **Custom** after a managed value changes.

| Preset | Density / layers | Effect amount | Reflections | Holo active budget | Gore cap |
| --- | --- | --- | --- | --- | --- |
| Low | Play / 1 | 1x | Off | Off (16,384 configured) | 2,048 |
| Medium | Play / 2 | 1x | Off | 16,384 | 4,096 |
| High (recommended) | Fine / 3 | 2x | On | 65,536 | 20,000 |
| Ultra | Showcase / 3 | 3x | On | 131,072 | 32,768 |
| Ultra Max | Showcase / 4 | 4x | On | 262,144 | 65,536 |

Low disables fidelity, directional gibs and goo. Medium adds fidelity and modest Holo Physics. High and above add debris lights and ceiling dust, with larger impact sprays and longer settling at higher tiers. All tiers request MSAA off for the particle renderer. Reflections require the Neon Cathedral structure and fidelity.

Presets retain the selected structure, palette, Prism/reflection styling, resolution, FPS cap, sound, controls and gameplay/world mode. Ultra Max is a quality option, not an FPS guarantee. All five profiles passed launcher validation, Custom detection and checks that unrelated settings remain unchanged.
