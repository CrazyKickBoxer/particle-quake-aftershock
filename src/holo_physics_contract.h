/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Josh Nicholls
 * Renderer-owned wire formats. No Quake or Vulkan dependency. */
#ifndef HOLO_PHYSICS_CONTRACT_H
#define HOLO_PHYSICS_CONTRACT_H
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string.h>

enum { HP_FREE=1, HP_STUCK=2, HP_GORE=4, HP_DEBRIS=8, HP_DUST=16, HP_SPARK=32 };
enum { HP_SHOCKWAVE, HP_SPRAY, HP_BLOOD, HP_BEAM, HP_SPLASH, HP_IMPLODE, HP_WAKE, HP_DEATH, HP_DEBUG };
enum { HP_NEUTRAL, HP_HOT, HP_WHITE_HOT, HP_BLOOD_SLOT, HP_COOL };
enum { HP_MAX_IMPULSES=64, HP_MIN_BUDGET=1024, HP_MAX_BUDGET=262144 };
typedef struct hp_particle_s {
    float position[3]; uint32_t point_id;
    float velocity[3]; float free_timer;
    uint32_t flags, seed; float free_lifetime; uint32_t source_ref;
} hp_particle_t;
typedef struct hp_impulse_s {
    float origin[3], radius;
    float direction[3], strength;
    float normal[3], duration;
    uint32_t type, color_slot, seed; float birth_time;
} hp_impulse_t;
typedef struct hp_queue_s {
    hp_impulse_t events[HP_MAX_IMPULSES];
    uint32_t count, dropped, serial;
} hp_queue_t;
typedef struct hp_preset_s { uint32_t budget, gore_cap; float settle, dust; } hp_preset_t;
static const hp_preset_t hp_presets[3] = {
    {16384,4096,1.0f,0}, {65536,20000,1.2f,.8f}, {131072,20000,1.8f,.8f}
};
#ifdef __cplusplus
#define HP_ASSERT static_assert
#else
#define HP_ASSERT _Static_assert
#endif
HP_ASSERT(sizeof(hp_particle_t)==48,"Holo particle std430 stride");
HP_ASSERT(offsetof(hp_particle_t,velocity)==16,"Holo velocity offset");
HP_ASSERT(offsetof(hp_particle_t,flags)==32,"Holo flags offset");
HP_ASSERT(sizeof(hp_impulse_t)==64,"Holo impulse std430 stride");
HP_ASSERT(offsetof(hp_impulse_t,type)==48,"Holo event header offset");

static inline uint32_t HP_Hash(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15;
    x *= 0x846ca68bu; return x ^ (x >> 16);
}
static inline int HP_QueuePush(hp_queue_t *q, hp_impulse_t e) {
    if (!isfinite(e.radius) || !isfinite(e.strength) || !isfinite(e.duration) ||
        !isfinite(e.birth_time) || e.radius<=0 || e.strength<=0 || e.duration<=0 || e.type>HP_DEBUG) return 0;
    for(int i=0;i<3;++i) if(!isfinite(e.origin[i]) || !isfinite(e.direction[i]) || !isfinite(e.normal[i])) return 0;
    e.seed=HP_Hash(++q->serial);
    if(q->count<HP_MAX_IMPULSES) { q->events[q->count++]=e; return 1; }
    uint32_t weakest=0;
    for(uint32_t i=1;i<q->count;++i)
        if(q->events[i].strength<q->events[weakest].strength) weakest=i;
    ++q->dropped;
    if(e.strength<=q->events[weakest].strength) return 0;
    q->events[weakest]=e; return 1;
}
/* Drain preserves the private sequence; only a map/reset clears it. */
static inline uint32_t HP_QueueDrain(hp_queue_t *q,hp_impulse_t *out) {
    uint32_t n=q->count; memcpy(out,q->events,n*sizeof(*out)); q->count=0; return n;
}
/* Exact solution over a substep with stiffness held constant. Also used by
 * numerical contract tests, never as a CPU particle simulation. */
static inline void HP_SpringReference(float *x,float *v,float omega,float dt) {
    float c=*v+omega**x, decay=expf(-omega*dt);
    *v=(*v-omega*c*dt)*decay; *x=(*x+c*dt)*decay;
}
#endif
