/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
#ifndef AFTERSHOCK_PHYSICS_H
#define AFTERSHOCK_PHYSICS_H
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct as_chunk_desc {
  float mins[3], maxs[3];
  uint32_t surface, material, anchored;
} as_chunk_desc;
typedef struct as_chunk_pose {
  float center[3], rotation[4];
  uint32_t detached, sleeping;
} as_chunk_pose;
typedef struct as_stats {
  uint32_t chunks, bonds, bodies, awake, detached, events, errors;
  double simulation_ms;
  uint64_t allocator_live_bytes, allocator_peak_bytes;
} as_stats;
typedef void (*as_log_fn)(const char *message);
typedef struct as_impact {
  float origin[3], energy;
} as_impact;
int AS_PhysicsInit(as_log_fn log);
void AS_PhysicsShutdown(void);
int AS_PhysicsBuild(const as_chunk_desc *chunks, uint32_t count,
                    const float *triangles, uint32_t triangle_count);
uint32_t AS_PhysicsExplode(const float origin[3], float radius, float damage,
                           uint32_t seed);
uint32_t AS_PhysicsExplodeProgressive(const float origin[3], float radius, float damage, uint32_t seed, float hits);
void AS_PhysicsStep(double seconds);
void AS_PhysicsPush(const float origin[3], const float velocity[3],
                    const float mins[3], const float maxs[3], double seconds);
int AS_PhysicsBrushMesh(uint32_t model, const float *triangles,
                        uint32_t triangle_count);
int AS_PhysicsBrushPose(uint32_t entity, uint32_t model, const float origin[3],
                        const float axes[9], int enabled);
const as_chunk_pose *AS_PhysicsPoses(void);
as_stats AS_PhysicsStats(void);
int AS_PhysicsSweep(const float start[3], const float end[3],
                    const float mins[3], const float maxs[3], float *fraction,
                    float normal[3]);
int AS_PhysicsSave(const char *path, uint64_t map_hash);
int AS_PhysicsLoad(const char *path, uint64_t map_hash);
int AS_AtomicReplace(const char *temporary, const char *target);
int AS_RemoveFile(const char *path);
FILE *AS_OpenFile(const char *path, const char *mode);
uint32_t AS_PhysicsTakeImpacts(as_impact *output, uint32_t capacity);
#ifdef __cplusplus
}
#endif
#endif
