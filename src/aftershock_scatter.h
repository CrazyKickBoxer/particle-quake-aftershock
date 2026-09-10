/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
#ifndef AFTERSHOCK_SCATTER_H
#define AFTERSHOCK_SCATTER_H
#include "aftershock_blue_noise.h"
#include "aftershock_layout.h"

/* Stateless randomness: neither the game RNG nor the camera enters the seed. */
static inline uint32_t AS_ScatterHash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  return x ^ (x >> 16);
}
static inline float AS_ScatterRandom(uint32_t x) {
  return (AS_ScatterHash(x) & 0xffffffu) / 16777216.f;
}
static inline int AS_ScatterInside(const float (*poly)[2], int n, float u,
                                   float v) {
  int sign = 0;
  for (int i = 0; i < n; ++i) {
    int j = (i + 1) % n;
    float cross = (poly[j][0] - poly[i][0]) * (v - poly[i][1]) -
                  (poly[j][1] - poly[i][1]) * (u - poly[i][0]);
    if (fabsf(cross) < .0001f)
      continue;
    int current = cross > 0 ? 1 : -1;
    if (sign && current != sign)
      return 0;
    sign = current;
  }
  return sign != 0;
}
/* Toroidal blue-noise point per cell, independently translated per layer,
 * clipped after packing.
 */
static inline int AS_ScatterCell(const float (*poly)[2], int n,
                                 const float low[2], const float high[2], int x,
                                 int y, int nx, int ny, uint32_t face,
                                 int layer, as_sample_t *out) {
  uint32_t shift = AS_ScatterHash(face + 0x9e3779b9u * (layer + 1));
  uint32_t index =
      ((x + (shift & 15u)) & 15u) + 16u * ((y + ((shift >> 4) & 15u)) & 15u);
  float u = low[0] + (high[0] - low[0]) * (x + as_blue_noise[index][0]) / nx;
  float v = low[1] + (high[1] - low[1]) * (y + as_blue_noise[index][1]) / ny;
  as_sample_t p;
  if (!AS_PackSample(u, v, &p) ||
      !AS_ScatterInside(poly, n, (p & 65535u) * .125f, (p >> 16) * .125f))
    return 0;
  *out = p;
  return 1;
}
#endif
