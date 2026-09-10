# Holo Physics validation

Measured on Windows 11, NVIDIA RTX 2050, Ryzen 5 7535HS. These are local regression
measurements, not a guarantee for every map, mod, resolution or GPU.

## Combat benchmark

Stock `timedemo demo1`, 969 timed frames, 1280×720, Fine source density, Neon
structure, three layers, fidelity and reflections enabled. The older CPU physics,
gibs, goo and screen shake were disabled in every case to isolate this layer.
Presets vary Holo Physics settings while keeping the same graphics settings.

| Holo preset | Timedemo FPS | Whole-frame time | Physics compute mean | Gore draw mean |
|---|---:|---:|---:|---:|
| Off | 164.5 | 6.079 ms | Disabled | Disabled |
| Play | 152.4 | 6.562 ms | 0.164 ms | 0.004 ms |
| Fine | 152.5 | 6.557 ms | 0.167 ms | 0.005 ms |
| Showcase | 151.1 | 6.618 ms | 0.171 ms | 0.006 ms |

Fine added approximately **7.9% whole-frame time** in this run. An earlier repeat
measured 168.8 FPS off and 157.1 FPS Fine (7.4% additional frame time). The latest
observed active peaks were 11667 / 12058 / 15065 for Play / Fine / Showcase.
These measurements meet the requested 10% target for this demo; they do not
establish a worst-case bound at a completely saturated 65536-particle pool.

Compute and gore means come from completed GPU timestamps, and only include
frames in which those passes ran. Whole-frame time is 1000 divided by timedemo
throughput. Collision sampling is included in compute. Grid construction is
measured separately at map initialization and excluded from the steady-state
physics-compute figure. Initial activation allocates and builds GPU resources.

Raw commands, logs, timestamp CSVs and a summary are under `build/holo-profile/`.
Reproduce with:

```powershell
python scripts/profile_holo.py off fine play showcase
```

Run engine tests sequentially: vkQuake writes a shared `qconsole.log`.

## Correctness evidence

- All five CTest targets passed: structural physics, pressure, GPU contracts,
  gib motion and Holo contracts. Holo contracts cover packed layouts, bounded
  priority admission, nonfinite rejection, private seeds, paused stepping and
  spring convergence.
- The deterministic E1M1 server checksum at tick 200 was
  `535e6aadd4feefa0` with the layer off, with a blast, with blood, and while cycling
  the renderer's live controls.
- All 970 observed demo snapshots matched between enabled and disabled runs:
  client time, network entity origins/angles/frames/message times/model names and
  player stats. The stock cosmetic RNG is logged separately: even repeated off
  timedemos can differ in that RNG, so it is not represented as a deterministic
  network-state check. Holo Physics does not consume it.
- Before the synthetic blast, the compared scene region was pixel-identical
  with physics enabled and disabled. Disturbed points retired and returned to
  original rendering by approximately three seconds. After recovery, residual
  screenshot differences were below 0.001 mean 8-bit channel value, consistent
  with temporal image history; original anchor positions are not rewritten.
- A blood fixture produced surface contacts, stopped their motion, and retained
  stains after all free blood particles expired. A one-second gore lifetime
  test reduced the live stain count from 39 to 20 to zero without a lifetime-end
  opacity jump.
- Console pause held the physics clock at `0.616667` with dt zero at both test
  frames 45 and 75. Integration resumed after the console closed.
- Live control cycling exercised 65536 → 2048 → 65536 capacity, gore off/on,
  physics off/on, and holographic renderer off/on. The 2048 budget stayed capped.
- All 14 existing material styles passed their rendering/promotion smoke tests.
  Spray, beam, splash, implosion and wake fixtures also passed. Retained style
  images are in `build/holo-matrix/`; the Reactive Physics menu was visually
  inspected with its toggles, sliders and numeric values visible.
- The stock demo exercised actual GPU monster-death snapshots. No game entity
  is deleted or converted by the observer.
- Save/load in E1M1, disconnect, and a subsequent E1M2 load completed with fresh
  renderer state and no Vulkan/host errors. The retained lifecycle log is
  `build/holo-lifecycle/engine.log`.

Useful commands:

```powershell
python scripts/test_holo.py --blast
python scripts/test_holo.py --off
python scripts/test_holo.py --gore --gore-life 1
python scripts/test_holo.py --pause --blast
python scripts/test_holo.py --cycle --blast
python scripts/test_holo_matrix.py
python scripts/test_holo_lifecycle.py
python scripts/profile_holo.py off-verify fine-verify
ctest --test-dir build/aftershock -C Release --output-on-failure
```

The test executables use a separate `build/` user directory and installed Quake
data. Screenshots are explicit validation artifacts, not a renderer feedback
mechanism. No production effect reads the framebuffer.

## Remaining scope limits

Static coarse collision excludes moving brush models and approximates thin
geometry. Nonstandard monster death frames and unrecognized protocol events can
omit a cosmetic effect. Precise separate occupancy-sampling GPU time is not
reported; it is part of the measured physics compute interval. A Vulkan validation
layer run and multiplayer packet-capture comparison have not been performed.
