/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aftershock_physics.h"
#include <cmath>
#include <cstdio>
#include <vector>
int main() {
  std::vector<as_chunk_desc> chunks;
  for (int z = 0; z < 32; ++z)
    for (int x = 0; x < 32; ++x)
      chunks.push_back({{float(x * 16), 0, float(z * 16)},
                        {float((x + 1) * 16), 16, float((z + 1) * 16)},
                        0,
                        0,
                        z == 0 ? 1u : 0u});
  uint64_t firstPeak = 0;
  for (int cycle = 0; cycle < 5; ++cycle) {
    if (!AS_PhysicsInit([](const char *s) { std::puts(s); }) ||
        !AS_PhysicsBuild(chunks.data(), 1024, nullptr, 0))
      return 1;
    float origin[] = {256, -2, 256};
    for (int hit = 0; hit < 16; ++hit)
      AS_PhysicsExplode(origin, 4096, 1000, 123 + hit);
    for (int tick = 0; tick < 60; ++tick)
      AS_PhysicsStep(1. / 120.);
    auto stats = AS_PhysicsStats();
    if (stats.detached != 1024 || stats.bodies != 1024 || stats.errors ||
        !stats.allocator_peak_bytes)
      return 2;
    for (int i = 0; i < 1024; ++i)
      for (float axis : AS_PhysicsPoses()[i].center)
        if (!std::isfinite(axis))
          return 3;
    if (AS_PhysicsBuild(chunks.data(), 8193, nullptr, 0) ||
        AS_PhysicsStats().detached != 1024)
      return 4;
    if (!cycle)
      firstPeak = stats.allocator_peak_bytes;
    if (stats.allocator_peak_bytes > firstPeak + 8 * 1024 * 1024)
      return 5;
    std::printf("cycle=%d bodies=%u events=%u PhysX-live=%llu PhysX-peak=%llu "
                "errors=%u\n",
                cycle, stats.bodies, stats.events,
                (unsigned long long)stats.allocator_live_bytes,
                (unsigned long long)stats.allocator_peak_bytes, stats.errors);
    AS_PhysicsShutdown();
    if (AS_PhysicsStats().allocator_live_bytes != 0)
      return 6;
  }
  std::puts("PASS: 5 cycles, 1024 bodies, 16 repeated explosions/cycle, finite "
            "state, capacity rejection, bounded allocator peak and zero "
            "outstanding PhysX allocations after shutdown");
  return 0;
}
