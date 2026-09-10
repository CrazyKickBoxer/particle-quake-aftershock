# 60-second Aftershock showcase

Output: `exports/ParticleQuake-Aftershock-60s.mp4`.

The six ten-second chapters show neon architecture, nail ricochets, a physical wall breach, directional gore, neon enemies, and a second breach with a moving camera. They are rendered from the actual engine using the player's Quake data. The gore chapter stages cosmetic explosion events in the procedural test arena; the weapon chapters fire native Quake weapons.

The export is 1920×1080 at 30 frames per second, H.264 video with stereo AAC audio. The sound combines the engine's spatialized sound effects and ambience with an original, quiet synthesized electronic bed. Capture uses the SDL dummy audio device in child processes so the offline mix does not play through desktop speakers while recording.

`scripts/record_showcase.py` drives the optional `-showcase 1..6` choreography, captures exactly 300 PNG frames and ten seconds of PCM per chapter, adds titles, encodes each chapter, and assembles the reel. `--preview` records sparse inspection frames; `--scene N` rerenders one chapter; `--assemble` rebuilds the final edit from existing chapters. It requires FFmpeg and Python with NumPy. The supplied procedural arena must already exist at `build/userdata/id1/maps/aftershock_arena.bsp`.

The `-capture-audio` path advances the mixer by 1,470 stereo samples per showcase tick at 44.1 kHz. This is independent of disk and GPU delays. Normal gameplay does not use the capture clock or choreography. Frame captures are deleted only after their chapter encodes successfully; audio, commands, logs, preview stills, and chapter MP4s remain in `build/showcase`.
