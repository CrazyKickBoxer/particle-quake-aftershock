/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Josh Nicholls
 * Deterministic cosmetic motion. No engine RNG, edicts or gameplay state. */
#ifndef AFTERSHOCK_GIB_MOTION_H
#define AFTERSHOCK_GIB_MOTION_H
#include <math.h>
#include <stdint.h>
typedef struct {
  float p[3], v[3], radius;
} as_gib_body;
typedef struct {
  float fraction, p[3], n[3];
  int entity;
} as_gib_hit;
typedef void (*as_gib_trace_fn)(const float *, const float *, float,
                                as_gib_hit *, void *);
typedef void (*as_gib_contact_fn)(const as_gib_body *, const as_gib_hit *,
                                  float, void *);
static uint32_t AS_GibHash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  return x ^ (x >> 16);
}
static float AS_GibRandom(uint32_t *seed) {
  *seed = AS_GibHash(*seed + 1);
  return (*seed & 65535u) / 65535.f;
}
static void AS_GibLaunch(as_gib_body *body, const float *origin,
                         const float *direction, uint32_t *seed) {
  float speed = 180 + AS_GibRandom(seed) * 260;
  for (int a = 0; a < 3; ++a) {
    body->p[a] = origin[a];
    body->v[a] = direction[a] * speed + (AS_GibRandom(seed) * 2 - 1) * 135;
  }
  body->v[2] += 100 + AS_GibRandom(seed) * 110;
}
// Call at 120 Hz. Sweeps consume the remaining timestep after every impact.
static void AS_GibStep(as_gib_body *b, float dt, as_gib_trace_fn trace,
                       as_gib_contact_fn contact, void *context) {
  b->v[2] -= 800 * dt;
  float left = dt;
  for (int bump = 0; bump < 4 && left > 0.00001f; ++bump) {
    float end[3];
    for (int a = 0; a < 3; ++a)
      end[a] = b->p[a] + b->v[a] * left;
    as_gib_hit h;
    trace(b->p, end, b->radius, &h, context);
    for (int a = 0; a < 3; ++a)
      b->p[a] = h.p[a];
    if (h.fraction >= 1)
      break;
    float vn = b->v[0] * h.n[0] + b->v[1] * h.n[1] + b->v[2] * h.n[2];
    if (contact && vn < 0)
      contact(b, &h, -vn, context);
    if (vn < 0) {
      float bounce = (h.n[2] > .55f && -vn < 150) ? 0 : .16f;
      for (int a = 0; a < 3; ++a)
        b->v[a] -= (1 + bounce) * vn * h.n[a];
    }
    if (h.n[2] > .55f) {
      // Viscous floor drag retains the forward slide, then settles.
      float drag = expf(-2.3f * dt);
      b->v[0] *= drag;
      b->v[1] *= drag;
      if (b->v[0] * b->v[0] + b->v[1] * b->v[1] < 4)
        b->v[0] = b->v[1] = 0;
    }
    for (int a = 0; a < 3; ++a)
      b->p[a] += .035f * h.n[a];
    left *= 1 - h.fraction;
  }
}
#endif
