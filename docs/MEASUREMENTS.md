# Measured engine throughput

Windows x64 Release; CPU PhysX; Enhanced / Mixed Debris Cloud / 3 layers / Play / Cinematic; 1920 x 1080 windowed; v-sync off; host cap 60; threaded renderer. Capture I/O disabled. See VALIDATION.md for methodology and hardware.

Measured executable SHA-256: `0B85CC7AEC8FA19EC7EF6FFFA53AD25EC8E27815E6D92D41A5A7E87B7882A56C`.

| Scenario | Host frames/s | p50 ms | p95 ms | p99 ms | GPU frame mean ms | PhysX step mean ms | Detached | PhysX peak bytes |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| e1m1 rockets, Classic | 56.671 | 16.664 | 19.416 | 48.091 | 6.891 | 0.439 | 36 | 2462839 |
| e1m1 rockets, Particle | 54.594 | 16.729 | 25.911 | 56.628 | 16.399 | 0.447 | 36 | 2462839 |
| start, synthetic explosion, Particle | 60.051 | 16.669 | 17.161 | 17.455 | 11.819 | 0.368 | 16 | 2398376 |
| e1m2, synthetic explosion, Particle | 53.833 | 18.378 | 20.943 | 25.309 | 17.938 | 0.419 | 9 | 2376346 |
| e1m3, synthetic explosion, Particle | 60.048 | 16.672 | 16.928 | 17.107 | 5.297 | 0.454 | 38 | 2731452 |

All five scenarios reported zero PhysX errors. These are short engine-frame measurements, not presented frame rates. The 60 presented-FPS target, isolated GPU pass costs, and total VRAM/effect budgets remain unverified. PhysX allocator values exclude Blast, Quake and graphics memory.

Recorded: 2026-09-08 12:14:15 -07:00.
