/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
#ifndef AFTERSHOCK_LAYOUT_H
#define AFTERSHOCK_LAYOUT_H
#include <math.h>
#include <stddef.h>
#include <stdint.h>
typedef uint32_t as_sample_t;
typedef struct as_surface_gpu_t {
  float origin[4], saxis[4], taxis[4], tex[4], light[4], params[4];
} as_surface_gpu_t;
#ifdef __cplusplus
#define AS_LAYOUT_ASSERT static_assert
#else
#define AS_LAYOUT_ASSERT _Static_assert
#endif
AS_LAYOUT_ASSERT(sizeof(as_sample_t) == 4, "Static sample stride");
AS_LAYOUT_ASSERT(sizeof(as_surface_gpu_t) == 96, "Surface metadata stride");
AS_LAYOUT_ASSERT(offsetof(as_surface_gpu_t, origin) == 0,
                 "Surface origin offset");
AS_LAYOUT_ASSERT(offsetof(as_surface_gpu_t, saxis) == 16,
                 "Surface S axis offset");
AS_LAYOUT_ASSERT(offsetof(as_surface_gpu_t, taxis) == 32,
                 "Surface T axis offset");
AS_LAYOUT_ASSERT(offsetof(as_surface_gpu_t, tex) == 48,
                 "Surface texture offset");
AS_LAYOUT_ASSERT(offsetof(as_surface_gpu_t, light) == 64,
                 "Surface lightmap offset");
AS_LAYOUT_ASSERT(offsetof(as_surface_gpu_t, params) == 80,
                 "Surface parameters offset");
/* UV units are source texels. Caller chooses the patch origin; no wrapping. */
static inline int AS_PackSample(float u, float v, as_sample_t *out) {
  if (!isfinite(u) || !isfinite(v) || u < 0 || v < 0 || u > 8191.875f ||
      v > 8191.875f)
    return 0;
  *out = (uint32_t)roundf(u * 8.f) | ((uint32_t)roundf(v * 8.f) << 16);
  return 1;
}
#endif
