/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
#ifndef AFTERSHOCK_H
#define AFTERSHOCK_H
#include "../../src/aftershock_layout.h"
extern cvar_t as_renderer, as_worldmode, as_density, as_style, as_radius, as_damage;
void HP_CreateLayout(void);
void HP_DestroyLayout(void);
void HP_Run(VkCommandBuffer cb,int frame_index);
void HP_Event(int type,const vec3_t origin,const vec3_t direction,float radius,float strength);
void HP_TempEntity(int type,const vec3_t origin);
void HP_Beam(const vec3_t start,const vec3_t end);
void HP_DrawDebug(cb_context_t *cbx);
int HP_CopyLights(VkCommandBuffer cb,VkBuffer target,VkDeviceSize offset,int available);
qboolean HP_ObserveAlias(entity_t *entity,aliashdr_t *hdr,const float *matrix,float blend,uint32_t pose1,uint32_t pose2);
int AS_ShowcaseTick(void);
void AS_NailRipple(float output[4]);
extern cvar_t as_fidelity, as_reflections, as_nails;
extern cvar_t as_neon_prism, as_reflection_strength, as_reflection_roughness, as_neon_glow;
extern cvar_t as_neon_npc_texture, as_npc_sat, as_npc_lift, as_npc_gain, as_npc_detail, as_npc_solid, as_layer_falloff, as_gib_shards;
extern cvar_t as_smw, as_smw_pixel, as_smw_outline, as_smw_edge, as_smw_saturate;
void AS_NailTrail(entity_t *ent, const vec3_t previous, int entity);
void AS_NailImpact(const vec3_t position, qboolean super);
void AS_NailScar(const vec3_t position, const vec3_t normal, int entity, float radius);
void AS_FidelityPrepare (end_rendering_parms_t *parms);
void AS_FidelityDraw (cb_context_t *cbx, end_rendering_parms_t *parms);
qboolean AS_Neon (void);
qboolean AS_IsMonsterModel (const char *name);
void		  AS_Init (void);
void		  AS_ApplyArguments (void);
void		  AS_NewMap (void);
void		  AS_ServerStep (double dt);
void		  AS_BeforeServer (void);
void		  AS_TestInput (void);
void		  AS_PushDebris (edict_t *player);
void		  AS_Frame (void);
void		  AS_Shutdown (void);
void		  AS_ClearMap (void);
void		  AS_SaveWorld (const char *path);
void		  AS_RequestLoad (const char *path);
void		  AS_TestCamera (void);
void		  AS_VisualExplosion (const vec3_t position);
void		  AS_DrawEffects (cb_context_t *cbx);
void		  AS_GoreInit (void);
void		  AS_GoreClear (void);
void		  AS_GoreDirection (const vec3_t origin, vec3_t direction);
void		  AS_GoreExplosion (const vec3_t origin, const vec3_t direction);
void		  AS_GoreBlood (const vec3_t origin, const vec3_t direction, int count);
void		  AS_GoreTrail (const vec3_t start, const vec3_t end);
void		  AS_GoreUpdate (void);
qboolean	  AS_GoreNative (entity_t *entity, int number);
void		  AS_DrawGore (cb_context_t *cbx);
void		  AS_WriteByte (sizebuf_t *dest, int value);
void		  AS_WriteCoord (sizebuf_t *dest, float value);
qboolean	  AS_SurfaceReplaced (const msurface_t *surface);
void		  AS_DrawWorld (cb_context_t *cbx, int index);
qboolean	  AS_DrawBrush (cb_context_t *cbx, qmodel_t *model, entity_t *ent, texchain_t chain);
void		  AS_DrawChunks (cb_context_t *cbx);
void		  AS_ClipDebris (trace_t *trace, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs);
qboolean AS_GoreReplacedDeath(entity_t *entity);
#endif
