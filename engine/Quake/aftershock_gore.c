/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
#include "quakedef.h"
#include "aftershock.h"
#include "../../src/aftershock_gib_motion.h"
#include "../../src/aftershock_physics.h"

#define AS_GIBS		  384
#define AS_MARKS	  768
#define AS_MARK_VERTS 48
cvar_t		  as_gibs = {"as_gibs", "1", CVAR_ARCHIVE};
cvar_t		  as_gib_particles = {"as_gib_particles", "1", CVAR_ARCHIVE};
cvar_t		  as_goo = {"as_goo", "1", CVAR_ARCHIVE};
extern cvar_t as_effects, as_particle_amount;
typedef struct
{
	as_gib_body body;
	double		born, marktime;
	float		life, angle, spin;
	vec3_t		lastmark;
	int			active, drop, model_drawn;
	unsigned	generation;
	qmodel_t   *native_model;
	byte		native_scale;
} as_gib_t;
typedef struct
{
	vec3_t	  vertices[AS_MARK_VERTS], center, normal;
	double	  born;
	int		  count, entity, scar;
	qmodel_t *model;
} as_mark_t;
static as_gib_t	 gibs[AS_GIBS];
static as_mark_t marks[AS_MARKS];
static unsigned	 gibserial, markserial;
static uint32_t	 goreseed = 0x51414b45;
static double	 goretime, accumulator, supporttime;
static struct
{
	vec3_t origin, direction;
	double time;
} rockets[64];
static unsigned rocketserial;
static unsigned spawned, wallhits, floorhits, smears, splats, rejected;
static int		testframes;
#define AS_DEATH_OWNERS 8192
static struct { qmodel_t *model; qboolean alive, replaced; } death_owners[AS_DEATH_OWNERS];
static unsigned deathbursts;
static unsigned death_capture_frames;
static struct
{
	int		  entity, slot;
	unsigned  generation;
	qmodel_t *model;
	double	  seen;
} native[128];
extern entity_t *CL_NewTempEntity (void);

static void AS_GoreStats_f (void)
{
	int live = 0, decals = 0;
	for (int i = 0; i < AS_GIBS; ++i)
		live += gibs[i].active;
	for (int i = 0; i < AS_MARKS; ++i)
		decals += marks[i].count > 0;
	Con_Printf (
		"Aftershock gore: spawned=%u wall=%u floor=%u smears=%u splats=%u rejected=%u live=%d marks=%d\n", spawned, wallhits, floorhits, smears, splats,
		rejected, live, decals);
    Con_Printf("Aftershock NPC death bursts: %u\n", deathbursts);
}
qboolean AS_GoreReplacedDeath (entity_t *entity)
{
    if (!as_effects.value || !as_gibs.value || entity < cl.entities || entity >= cl.entities + cl.num_entities) return false;
    size_t owner = entity - cl.entities;
    return owner < AS_DEATH_OWNERS && death_owners[owner].model == entity->model && death_owners[owner].replaced;
}
void AS_GoreInit (void)
{
	Cvar_RegisterVariable (&as_gibs);
	Cvar_RegisterVariable (&as_gib_particles);
	Cvar_RegisterVariable (&as_goo);
	Cmd_AddCommand ("as_gore_stats", AS_GoreStats_f);
}
void AS_GoreClear (void)
{
	memset (gibs, 0, sizeof (gibs));
	memset (marks, 0, sizeof (marks));
	memset (rockets, 0, sizeof (rockets));
	gibserial = markserial = rocketserial = 0;
	goreseed = 0x51414b45;
	goretime = accumulator = supporttime = 0;
	spawned = wallhits = floorhits = smears = splats = rejected = 0;
	testframes = 0;
	memset (native, 0, sizeof (native));
    memset(death_owners,0,sizeof(death_owners)); deathbursts=0; death_capture_frames=0;
}
void AS_GoreDirection (const vec3_t origin, vec3_t direction)
{
	float best = 160 * 160;
	for (int i = 0; i < 64; ++i)
	{
		vec3_t d;
		VectorSubtract (origin, rockets[i].origin, d);
		float distance = DotProduct (d, d);
		if (cl.time - rockets[i].time >= 0 && cl.time - rockets[i].time < .25 && distance < best)
		{
			best = distance;
			VectorCopy (rockets[i].direction, direction);
		}
	}
}
static void AS_SpawnGib (const vec3_t origin, const vec3_t direction, int drop)
{
	as_gib_t *g = &gibs[gibserial++ % AS_GIBS];
	memset (g, 0, sizeof (*g));
	g->generation = gibserial;
	AS_GibLaunch (&g->body, origin, direction, &goreseed);
	g->drop = drop;
	g->body.radius = drop ? .35f + AS_GibRandom (&goreseed) * .4f : 1.2f + AS_GibRandom (&goreseed) * 1.8f;
	g->born = cl.time;
	g->life = drop ? 2.5f : 8 + AS_GibRandom (&goreseed) * 4;
	g->angle = AS_GibRandom (&goreseed) * 6.283185f;
	g->spin = (AS_GibRandom (&goreseed) - .5f) * 18;
	VectorCopy (origin, g->lastmark);
	g->active = 1;
	++spawned;
}
void AS_GoreExplosion (const vec3_t origin, const vec3_t direction)
{
	if (!as_effects.value || !as_gibs.value || !cl.worldmodel)
		return;
	GL_SynchronizeEndRenderingTask ();
	int count = 24 * CLAMP (1, (int)as_particle_amount.value, 4);
	for (int i = 0; i < count; ++i)
		AS_SpawnGib (origin, direction, i % 3 != 0);
}
void AS_GoreBlood (const vec3_t origin, const vec3_t direction, int count)
{
	if (!as_effects.value || !as_gibs.value || !cl.worldmodel)
		return;
	GL_SynchronizeEndRenderingTask ();
	vec3_t dir;
	VectorCopy (direction, dir);
	VectorNormalize (dir);
	for (int i = 0; i < CLAMP (1, count, 16); ++i)
		AS_SpawnGib (origin, dir, 1);
}
void AS_GoreTrail (const vec3_t start, const vec3_t end)
{
	vec3_t dir;
	VectorSubtract (end, start, dir);
	if (VectorNormalize (dir) < 2)
		return;
	AS_GoreBlood (end, dir, 1);
}
qboolean AS_GoreNative (entity_t *entity, int number)
{
	if (!as_effects.value || !as_gibs.value || !entity->model || !(entity->model->flags & (EF_GIB | EF_ZOMGIB)))
		return false;
	GL_SynchronizeEndRenderingTask ();
	int slot = -1, oldest = 0;
	for (int i = 0; i < 128; ++i)
	{
		if (native[i].entity == number && native[i].model == entity->model && cl.time - native[i].seen < .25 && cl.time >= native[i].seen)
		{
			slot = i;
			break;
		}
		if (native[i].seen < native[oldest].seen)
			oldest = i;
	}
	if (slot < 0)
	{
		slot = oldest;
		int index = gibserial % AS_GIBS;
		AS_SpawnGib (entity->origin, vec3_origin, 0);
		as_gib_t *g = &gibs[index];
		g->native_model = entity->model;
		g->native_scale = entity->netstate.scale;
		g->body.radius = 3.5f * ENTSCALE_DECODE (entity->netstate.scale);
		float interval = (float)q_max (.01, cl.mtime[0] - cl.mtime[1]);
		VectorSubtract (entity->msg_origins[0], entity->msg_origins[1], g->body.v);
		VectorScale (g->body.v, 1 / interval, g->body.v);
		if (sv.active && number < sv.qcvm.num_edicts)
		{
			edict_t *ed = (edict_t *)((byte *)sv.qcvm.edicts + number * sv.qcvm.edict_size);
			if (!ed->free)
				VectorCopy (ed->v.velocity, g->body.v);
		}
		if (VectorLength (g->body.v) > 1400)
			VectorScale (g->body.v, 1400 / VectorLength (g->body.v), g->body.v);
		native[slot].entity = number;
		native[slot].model = entity->model;
		native[slot].slot = index;
		native[slot].generation = g->generation;
	}
	native[slot].seen = cl.time;
	// If the cosmetic pool was saturated, leave the authoritative visual available.
	return gibs[native[slot].slot].generation == native[slot].generation;
}

// Store decals in the struck brush's local coordinates, including rotation.
static void AS_MarkTransform (int entity, const vec3_t in, vec3_t out, int inverse, int vector)
{
	if (!entity)
	{
		VectorCopy (in, out);
		return;
	}
	entity_t *e = &cl.entities[entity];
	vec3_t	  axes[3], p;
	AngleVectors (e->angles, axes[0], axes[1], axes[2]);
	VectorInverse (axes[1]);
	if (inverse)
	{
		if (vector)
			VectorCopy (in, p);
		else
			VectorSubtract (in, e->origin, p);
		for (int a = 0; a < 3; ++a)
			out[a] = DotProduct (p, axes[a]);
	}
	else
	{
		for (int a = 0; a < 3; ++a)
			out[a] = (vector ? 0 : e->origin[a]) + in[0] * axes[0][a] + in[1] * axes[1][a] + in[2] * axes[2][a];
	}
}
static int AS_InsideFace (const glpoly_t *poly, const vec3_t point, const vec3_t normal)
{
	vec3_t center = {0, 0, 0};
	for (int j = 0; j < poly->numverts; ++j)
		VectorAdd (center, poly->verts[j], center);
	VectorScale (center, 1.f / poly->numverts, center);
	for (int j = 0; j < poly->numverts; ++j)
	{
		vec3_t edge, n, d, c;
		VectorSubtract (poly->verts[(j + 1) % poly->numverts], poly->verts[j], edge);
		CrossProduct (edge, normal, n);
		VectorSubtract (point, poly->verts[j], d);
		VectorSubtract (center, poly->verts[j], c);
		if (DotProduct (n, d) * ((DotProduct (n, c) < 0) ? -1 : 1) < -.1f)
			return 0;
	}
	return 1;
}
static void AS_MakeMark (const vec3_t position, const vec3_t normal, const vec3_t velocity, int entity, float radius, int smear)
{
	if ((smear==2 ? !as_nails.value : !as_goo.value) || entity < 0 || entity >= cl.num_entities)
		return;
	qmodel_t *model = entity ? cl.entities[entity].model : cl.worldmodel;
	if (!model || model->type != mod_brush)
		return;
	vec3_t center, n, tangent, bitangent, localvel;
	AS_MarkTransform (entity, position, center, 1, 0);
	AS_MarkTransform (entity, normal, n, 1, 1);
	AS_MarkTransform (entity, velocity, localvel, 1, 1);
	msurface_t *face = NULL;
	for (int i = model->firstmodelsurface; i < model->firstmodelsurface + model->nummodelsurfaces; ++i)
	{
		msurface_t *s = &model->surfaces[i];
		if (!s->polys || (s->flags & (SURF_DRAWSKY | SURF_DRAWTURB)))
			continue;
		if (fabsf (DotProduct (center, s->plane->normal) - s->plane->dist) > 1.5f)
			continue;
		if (fabsf (DotProduct (n, s->plane->normal)) < .99f || !AS_InsideFace (s->polys, center, n))
			continue;
		face = s;
		break;
	}
	if (!face)
	{
		++rejected;
		return;
	}
	VectorMA (localvel, -DotProduct (localvel, n), n, tangent);
	if (!smear || VectorNormalize (tangent) < .1f)
	{
		vec3_t axis = {0, 0, 1};
		if (fabsf (n[2]) > .9f)
		{
			axis[1] = 1;
			axis[2] = 0;
		}
		CrossProduct (axis, n, tangent);
		VectorNormalize (tangent);
	}
	CrossProduct (n, tangent, bitangent);
	vec3_t polygon[AS_MARK_VERTS], scratch[AS_MARK_VERTS];
	int	   count = 24;
	float  phase = AS_GibRandom (&goreseed) * 6.283185f;
	for (int i = 0; i < count; ++i)
	{
		float a = i * (float)(M_PI * 2 / 24), r = radius * (.78f + .12f * sinf (a * 5 + phase) + .1f * cosf (a * 3 - phase));
		// Lobes and pointed satellites form the silhouette, rather than a square decal.
		if (i % 6 == 0 && !smear)
			r *= 1.16f;
		for (int k = 0; k < 3; ++k)
			polygon[i][k] = center[k] + tangent[k] * cosf (a) * r * (smear==1 ? 2.4f : 1) + bitangent[k] * sinf (a) * r;
	}
	// Sutherland-Hodgman clipping against the actual convex BSP face edges.
	glpoly_t *poly = face->polys;
	vec3_t	  mid = {0, 0, 0};
	for (int j = 0; j < poly->numverts; ++j)
		VectorAdd (mid, poly->verts[j], mid);
	VectorScale (mid, 1.f / poly->numverts, mid);
	for (int j = 0; j < poly->numverts && count >= 3; ++j)
	{
		vec3_t edge, clip, delta;
		VectorSubtract (poly->verts[(j + 1) % poly->numverts], poly->verts[j], edge);
		CrossProduct (edge, n, clip);
		VectorNormalize (clip);
		VectorSubtract (mid, poly->verts[j], delta);
		if (DotProduct (clip, delta) < 0)
			VectorInverse (clip);
		float dist = DotProduct (clip, poly->verts[j]);
		int	  next = 0;
		for (int k = 0; k < count; ++k)
		{
			float *a = polygon[k], *b = polygon[(k + 1) % count];
			float  da = DotProduct (a, clip) - dist, db = DotProduct (b, clip) - dist;
			if (da >= 0 && next < AS_MARK_VERTS)
			{
				VectorCopy (a, scratch[next]);
				++next;
			}
			if ((da >= 0) != (db >= 0) && next < AS_MARK_VERTS)
			{
				float f = da / (da - db);
				for (int c = 0; c < 3; ++c)
					scratch[next][c] = a[c] + (b[c] - a[c]) * f;
				++next;
			}
		}
		count = next;
		memcpy (polygon, scratch, count * sizeof (vec3_t));
	}
	if (count < 3)
		return;
	as_mark_t *m = &marks[smear==2 ? AS_MARKS-128+(markserial++ % 128) : markserial++ % (AS_MARKS-128)];
	m->entity = entity;
	m->model = model;
	m->born = cl.time;
    m->scar=smear==2;
	m->count = count;
	VectorCopy (n, m->normal);
	VectorMA (center, .12f, n, m->center);
	for (int j = 0; j < count; ++j)
		VectorMA (polygon[j], .12f, n, m->vertices[j]);
	++splats;
	if (smear)
		++smears;
}
void AS_NailScar(const vec3_t p, const vec3_t n, int entity, float radius)
{ AS_MakeMark(p,n,vec3_origin,entity,radius,2); }
static void AS_GibTrace (const float *start, const float *end, float radius, as_gib_hit *hit, void *context)
{
	(void)context;
	hit->fraction = 1;
	hit->entity = 0;
	VectorCopy (end, hit->p);
	hit->n[0] = hit->n[1] = 0;
	hit->n[2] = 1;
	// Seven swept probes give the small cosmetic chunks a bounded collision volume.
	for (int ray = 0; ray < 7; ++ray)
	{
		vec3_t a, b, p, n, offset = {0, 0, 0};
		int	   entity;
		if (ray)
			offset[(ray - 1) / 2] = ((ray & 1) ? 1 : -1) * radius;
		VectorAdd (start, offset, a);
		VectorAdd (end, offset, b);
		float f = CL_TraceLine (a, b, p, n, &entity);
		if (f < hit->fraction)
		{
			hit->fraction = f;
			hit->entity = entity;
			VectorSubtract (p, offset, hit->p);
			VectorCopy (n, hit->n);
		}
	}
	// Structural rubble is owned by PhysX; sweep that same scene as well.
	float  f;
	vec3_t n, mins = {-radius, -radius, -radius}, maxs = {radius, radius, radius};
	if (as_worldmode.value && AS_PhysicsSweep (start, end, mins, maxs, &f, n) && f < hit->fraction)
	{
		hit->fraction = f;
		hit->entity = -1;
		VectorCopy (n, hit->n);
		for (int a = 0; a < 3; ++a)
			hit->p[a] = start[a] + (end[a] - start[a]) * f;
	}
}
static void AS_GibContact (const as_gib_body *body, const as_gib_hit *hit, float speed, void *context)
{
	as_gib_t *g = context;
	vec3_t	  d, point;
	VectorSubtract (body->p, g->lastmark, d);
	if (hit->n[2] > .55f)
	{
		if (speed > 20)
			++floorhits;
	}
	else if (speed > 30)
		++wallhits;
	if (cl.time - g->marktime < .055 || (speed < 30 && DotProduct (d, d) < 16))
		return;
	VectorMA (body->p, -body->radius, hit->n, point);
	int smear = hit->n[2] > .55f && speed < 100;
	AS_MakeMark (point, hit->n, body->v, hit->entity, smear ? body->radius : CLAMP (2, body->radius * 2 + speed * .02f, 15), smear);
	// Wall impacts sprout gravity-aligned runs and small satellite drops.
	if (!smear && fabsf (hit->n[2]) < .4f && speed > 90)
	{
		vec3_t down = {0, 0, -1}, drip;
		for (int i = 1; i <= 3; ++i)
		{
			VectorMA (point, i * 3.f, down, drip);
			AS_MakeMark (drip, hit->n, down, hit->entity, body->radius * (1.2f - i * .2f), 1);
		}
	}
	VectorCopy (body->p, g->lastmark);
	g->marktime = cl.time;
	if (g->drop)
		g->active = 0;
	if (speed > 60)
		g->spin *= .45f;
}
// Observe animation transitions on the client thread. Never change entity state,
// and never spawn chunks from the parallel alias-rendering task.
static void AS_GoreObserveDeaths (void)
{
    for (int i=1; i<q_min(cl.num_entities,AS_DEATH_OWNERS); ++i) {
        entity_t *e=&cl.entities[i];
        if (!e->model || e->msgtime != cl.mtime[0] || e->model->type != mod_alias) {
            memset(&death_owners[i],0,sizeof(death_owners[i])); continue;
        }
        if (e->model != death_owners[i].model) {
            memset(&death_owners[i],0,sizeof(death_owners[i])); death_owners[i].model=e->model;
        }
        if (i<=cl.maxclients || (e->model->flags & (EF_GIB|EF_ZOMGIB)) || strstr(e->model->name,"/h_")) continue;
        aliashdr_t *hdr=(aliashdr_t*)Mod_Extradata(e->model);
        if (!hdr || hdr->poseverttype != PV_QUAKE1 || e->frame<0 || e->frame>=hdr->numframes) continue;
        const char *frame=hdr->frames[e->frame].name;
        qboolean dead=!strncmp(frame,"death",5) || !strncmp(frame,"die",3);
        if (!dead) { death_owners[i].alive=true; death_owners[i].replaced=false; continue; }
        // A corpse first seen on load is not a new kill. Emit once per live->dead transition.
        if (!death_owners[i].alive) continue;
        death_owners[i].alive=false;
        if (!as_gibs.value || !as_renderer.value) continue;
        vec3_t origin,direction;VectorCopy(e->origin,origin);origin[2]+=12;
        AS_GoreDirection(origin,direction);
        int count=12*CLAMP(1,(int)as_particle_amount.value,4);
        for(int n=0;n<count;++n)AS_SpawnGib(origin,direction,n%3==2);
        death_owners[i].replaced=true;++deathbursts;
        if (COM_CheckParm("-test-death-gibs")) Con_Printf("Aftershock NPC death gib burst: entity=%d count=%d\n",i,count);
    }
}
void AS_GoreUpdate (void)
{
	GL_SynchronizeEndRenderingTask ();
	if (!cl.worldmodel || !as_effects.value || (!as_gibs.value && !as_nails.value))
	{
		AS_GoreClear ();
		return;
	}
	if (cl.time < goretime)
		AS_GoreClear ();
	double dt = CLAMP (0, cl.time - goretime, .1);
	goretime = cl.time;
	if (cl.paused)
		dt = 0;
	if (COM_CheckParm ("-test-gore") && !strcmp (cl.worldmodel->name, "maps/aftershock_arena.bsp") && ++testframes == 45)
	{
		vec3_t p = {0, -100, 18}, d = {0, 1, 0};
		AS_GoreExplosion (p, d);
		p[1] = -20;
		p[2] = 70;
		AS_GoreExplosion (p, d);
		Con_Printf ("Aftershock directional gore fixture emitted\n");
	}
	for (int i = 1; i < cl.num_entities; ++i)
	{
		entity_t *e = &cl.entities[i];
		if (!e->model || !(e->model->flags & EF_ROCKET))
			continue;
		vec3_t dir;
		VectorSubtract (e->msg_origins[0], e->msg_origins[1], dir);
		if (VectorNormalize (dir) < .1)
			continue;
		int n = rocketserial++ % 64;
		VectorCopy (e->origin, rockets[n].origin);
		VectorCopy (dir, rockets[n].direction);
		rockets[n].time = cl.time;
	}
	if (dt>0) AS_GoreObserveDeaths();
    if (COM_CheckParm("-test-death-gibs-capture") && deathbursts && ++death_capture_frames==6)
        Cbuf_AddText("screenshot png 90 death-gibs.png\nas_gore_stats\n");
	accumulator += dt;
	while (accumulator >= 1. / 120)
	{
		for (int i = 0; i < AS_GIBS; ++i)
			if (gibs[i].active)
			{
				as_gib_t *g = &gibs[i];
				if (cl.time - g->born > g->life)
				{
					g->active = 0;
					continue;
				}
				AS_GibStep (&g->body, 1.f / 120, AS_GibTrace, AS_GibContact, g);
				g->angle += g->spin / 120;
			}
		accumulator -= 1. / 120;
	}
	for(int i=0;i<AS_MARKS;++i)
        if(marks[i].scar ? !as_nails.value : !as_goo.value) marks[i].count=0;
	if (cl.time >= supporttime)
	{
		supporttime = cl.time + .2;
		for (int i = 0; i < AS_MARKS; ++i)
			if (marks[i].count)
			{
				as_mark_t *m = &marks[i];
				if (cl.time - m->born > (m->scar?6:24) || (m->entity && (m->entity >= cl.num_entities || cl.entities[m->entity].model != m->model)))
				{
					m->count = 0;
					continue;
				}
				vec3_t p, n, a, b, impact, normal;
				int	   entity;
				AS_MarkTransform (m->entity, m->center, p, 0, 0);
				AS_MarkTransform (m->entity, m->normal, n, 0, 1);
				VectorMA (p, 1, n, a);
				VectorMA (p, -1, n, b);
				if (CL_TraceLine (a, b, impact, normal, &entity) >= 1 || entity != m->entity || DotProduct (n, normal) < .99)
					m->count = 0;
			}
	}
	// Reuse the user's precached Quake gib meshes, with a procedural fallback for mods.
	for (int i = 0; i < AS_GIBS; ++i)
		gibs[i].model_drawn = 0;
	qmodel_t *models[3] = {NULL, NULL, NULL};
	for (int i = 1; i < MAX_MODELS; ++i)
		if (cl.model_precache[i])
		{
			for (int j = 0; j < 3; ++j)
				if (!strcmp (cl.model_precache[i]->name, va ("progs/gib%d.mdl", j + 1)))
					models[j] = cl.model_precache[i];
		}
	for (int i = 0; i < AS_GIBS; ++i)
	{
		as_gib_t *g = &gibs[i];
		qmodel_t *model = g->native_model ? g->native_model : models[i % 3];
		if (!g->active || g->drop || !model)
			continue;
		if (as_gib_particles.value)
			continue; /* leave model_drawn clear: the body draws as a small blob instead */
		entity_t *e = CL_NewTempEntity ();
		if (!e)
			break;
		e->model = model;
		VectorCopy (g->body.p, e->origin);
		e->angles[1] = g->angle * (float)(180 / M_PI);
		e->angles[2] = e->angles[1] * .7f;
		e->netstate.scale = g->native_model ? g->native_scale : ENTSCALE_ENCODE (.30f + g->body.radius * .06f);
		e->alpha = ENTALPHA_ENCODE (CLAMP (0, g->life - (cl.time - g->born), 1));
		g->model_drawn = 1;
	}
}
static void AS_GoreVertex (basicvertex_t *v, const vec3_t p, int r, int g, int b, int alpha)
{
	VectorCopy (p, v->position);
	v->texcoord[0] = v->texcoord[1] = 0;
	v->color[0] = r;
	v->color[1] = g;
	v->color[2] = b;
	v->color[3] = alpha;
}
void AS_DrawGore (cb_context_t *cbx)
{
	if (!as_effects.value)
		return;
	int count = 0;
	for (int i = 0; i < AS_GIBS; ++i)
		if (as_gibs.value && gibs[i].active && !gibs[i].model_drawn)
			count += 48;
	if (as_goo.value || as_nails.value)
		for (int i = 0; i < AS_MARKS; ++i)
			if(marks[i].scar?as_nails.value:as_goo.value) count += marks[i].count * 6;
	if (!count)
		return;
	R_BindPipeline (
		cbx, VK_PIPELINE_BIND_POINT_GRAPHICS,
		R_PipelineForSubpassType (
			cbx->subpass_type, vulkan_globals.particle_pipeline, vulkan_globals.particle_oit_pipeline, vulkan_globals.particle_mboit_moment_pipeline,
			vulkan_globals.particle_mboit_composite_pipeline));
	vulkan_globals.vk_cmd_bind_descriptor_sets (
		cbx->cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan_globals.basic_pipeline_layout.handle, 0, 1, &whitetexture->descriptor_set, 0, NULL);
	VkBuffer	   vb;
	VkDeviceSize   vo;
	basicvertex_t *v = (basicvertex_t *)R_VertexAllocate (count * sizeof (*v), &vb, &vo);
	int			   used = 0;
	if (as_goo.value || as_nails.value)
		for (int i = 0; i < AS_MARKS; ++i)
			if (marks[i].count && (marks[i].scar?as_nails.value:as_goo.value))
			{
				as_mark_t *m = &marks[i];
				float	   age = (float)(cl.time - m->born);
				int		   alpha = (int)(230 * CLAMP (0, ((m->scar?6:24) - age) / (m->scar?2:4), 1));
				for (int j = 0; j < m->count; ++j)
				{
					vec3_t p[3];
					AS_MarkTransform (m->entity, m->center, p[0], 0, 0);
					AS_MarkTransform (m->entity, m->vertices[j], p[1], 0, 0);
					AS_MarkTransform (m->entity, m->vertices[(j + 1) % m->count], p[2], 0, 0);
					for (int side = 0; side < 2; ++side)
						for (int k = 0; k < 3; ++k)
						{
							int q = side ? 2 - k : k;
							if(m->scar) AS_GoreVertex(&v[used++],p[q],q?38:2,q?30:2,q?23:3,alpha);
                            else AS_GoreVertex (&v[used++], p[q], q ? 34 : 80, q ? 1 : 4, q ? 4 : 11, alpha);
						}
				}
			}
	static const int faces[8][3] = {{4, 0, 1}, {4, 1, 2}, {4, 2, 3}, {4, 3, 0}, {5, 1, 0}, {5, 2, 1}, {5, 3, 2}, {5, 0, 3}};
	for (int i = 0; i < AS_GIBS; ++i)
		if (as_gibs.value && gibs[i].active && !gibs[i].model_drawn)
		{
			as_gib_t *g = &gibs[i];
			vec3_t	  points[6];
			float	  radius = g->body.radius;
			for (int j = 0; j < 4; ++j)
			{
				float a = g->angle + j * (float)(M_PI * .5);
				VectorCopy (g->body.p, points[j]);
				points[j][0] += cosf (a) * radius;
				points[j][1] += sinf (a) * radius;
				points[j][2] += sinf (g->angle) * radius * .35f;
			}
			VectorCopy (g->body.p, points[4]);
			VectorCopy (g->body.p, points[5]);
			points[4][2] += radius;
			points[5][2] -= radius * .65f;
			int alpha = (int)(255 * CLAMP (0, g->life - (cl.time - g->born), 1));
			for (int f = 0; f < 8; ++f)
				for (int side = 0; side < 2; ++side)
					for (int k = 0; k < 3; ++k)
					{
						int q = faces[f][side ? 2 - k : k];
						AS_GoreVertex (&v[used++], points[q], f == 1 ? 210 : 95 + f * 9, f == 1 ? 62 : 5 + f, f == 1 ? 56 : 14 + f, alpha);
					}
		}
	vulkan_globals.vk_cmd_bind_vertex_buffers (cbx->cb, 0, 1, &vb, &vo);
	vulkan_globals.vk_cmd_draw (cbx->cb, used, 1, 0, 0);
}
