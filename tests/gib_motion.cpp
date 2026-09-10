// SPDX-License-Identifier: GPL-2.0-or-later
#include "aftershock_gib_motion.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
struct Fixture {
  int floor = 0, wall = 0;
  float wallX = 180;
};
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void trace(const float *a, const float *b, float radius, as_gib_hit *h,
           void *context) {
  auto &f = *static_cast<Fixture *>(context);
  h->fraction = 1;
  h->entity = 0;
  std::memcpy(h->p, b, 12);
  h->n[0] = h->n[1] = 0;
  h->n[2] = 1;
  if (b[2] < radius && b[2] < a[2])
    h->fraction = std::fmax(0.f, (a[2] - radius) / (a[2] - b[2]));
  if (b[0] > f.wallX - radius && b[0] > a[0]) {
    float t = std::fmax(0.f, (f.wallX - radius - a[0]) / (b[0] - a[0]));
    if (t < h->fraction) {
      h->fraction = t;
      h->n[0] = -1;
      h->n[2] = 0;
    }
  }
  for (int k = 0; k < 3; ++k)
    h->p[k] = a[k] + (b[k] - a[k]) * h->fraction;
}
void contact(const as_gib_body *, const as_gib_hit *h, float speed,
             void *context) {
  auto &f = *static_cast<Fixture *>(context);
  if (speed > 20) {
    if (h->n[2] > .5)
      ++f.floor;
    else
      ++f.wall;
  }
}
int main() {
  try {
    uint32_t seed = 42;
    float origin[3] = {0, 0, 40}, forward[3] = {1, 0, 0};
    float mean = 0;
    for (int i = 0; i < 4096; ++i) {
      as_gib_body b{};
      AS_GibLaunch(&b, origin, forward, &seed);
      require(b.v[0] > 0, "Gibs must travel with the projectile");
      mean += b.v[0];
    }
    require(mean / 4096 > 300, "Missing directional momentum");
    Fixture f;
    as_gib_body b{{0, 0, 12}, {600, 25, -80}, 2};
    float landed = -1, maxSlide = 0;
    for (int step = 0; step < 1200; ++step) {
      AS_GibStep(&b, 1.f / 120, trace, contact, &f);
      require(std::isfinite(b.p[0]) && std::isfinite(b.p[2]),
              "Nonfinite gib state");
      require(b.p[0] <= f.wallX - 1.99f && b.p[2] >= 1.99f,
              "Gib tunneled through room");
      if (f.floor && landed < 0)
        landed = b.p[0];
      if (landed >= 0)
        maxSlide = std::fmax(maxSlide, b.p[0] - landed);
    }
    require(f.floor > 0 && f.wall > 0,
            "Missing floor/wall collision callbacks");
    require(maxSlide > 50, "Floor collision lost forward slide");
    require(std::fabs(b.v[0]) < 2 && std::fabs(b.v[1]) < 2,
            "Gib never settled");
    // A sweep must catch a wall even at extreme speed and consume the rebound
    // time.
    as_gib_body fast{{0, 0, 64}, {100000, 0, 0}, 1};
    AS_GibStep(&fast, 1.f / 120, trace, contact, &f);
    require(fast.p[0] < 179.01f && fast.v[0] < 0,
            "High-speed wall sweep failed");
    as_gib_body a{{0, 0, 50}, {120, 0, -50}, 1}, c = a;
    Fixture x, y;
    for (int step = 0; step < 300; ++step) {
      AS_GibStep(&a, 1.f / 120, trace, contact, &x);
      AS_GibStep(&c, 1.f / 120, trace, contact, &y);
    }
    require(std::memcmp(&a, &c, sizeof(a)) == 0,
            "Simulation is not repeatable");
    std::printf(
        "PASS: 4096 forward launches; floor slide %.1f units; wall/floor "
        "contacts; settling; high-speed sweep; deterministic replay\n",
        maxSlide);
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "FAIL: %s\n", e.what());
    return 1;
  }
}
