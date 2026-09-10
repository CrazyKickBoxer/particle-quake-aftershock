# Fidelity measurements

Final executable: `5CD67ED7BBBD680D41A4FEA74B7F17C9CDFF46F4C5C2FC3D9058534B76BBE82F`.

1280x720, Fine density, three layers, 190 host frames, deterministic host step, one capture. RTX 2050. These are whole-frame GPU timestamp averages, not presented frame rates or isolated pass timings.

| Case | GPU timestamp mean (ms) |
|---|---:|
| Fidelity off | 17.428 |
| Fidelity on, reflections off | 19.992 |
| Fidelity and reflections on | 20.667 |
| 4x MSAA, start map | 26.835 |
| Scale 2, OIT 2 | 20.674 |
| Camera motion/cuts and video restart | 18.915 |
| Live toggles with 36-piece breach | 17.023 |

An earlier build/run measured 3.950 / 4.605 / 5.147 ms for the first three cases. The substantial baseline variation means those numbers should not be treated as guaranteed performance. The final matched run adds approximately 3.24 ms from fidelity-off to fidelity-plus-reflections under its conditions.

The matched reflection image comparison changes 79,138 pixels by more than three channel levels. Changes concentrate on the floor region (mean RGB delta 1.73/255), with very little change in the upper region (0.00155/255). Exact comparison data and region definitions are in `reflection-image-comparison.json`.

The runtime captures were inspected for brightness, geometry, motion/cut artifacts, and the launcher layout. Reflection sources outside the view are intentionally unavailable. Shader interface and blue-noise tests passed; this run did not have the Khronos validation layer installed.
