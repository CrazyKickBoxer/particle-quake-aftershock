/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
/* Client-only cosmetic events. Never touches edicts, QC, damage, or gameplay RNG. */
cvar_t as_nails = {"as_nails", "1", CVAR_ARCHIVE};
typedef struct
{
	vec3_t	 p, d, n;
	double	 born;
	unsigned seed;
	int		 kind;
	float	 floor;
} as_nail_fx_t;
static as_nail_fx_t as_nail_fx[64];
static unsigned		as_nail_serial;
static struct
{
	vec3_t p, d;
	double seen, emitted;
	int	   entity;
} as_nail_tracks[128];
static void AS_NailsClear (void)
{
	memset (as_nail_fx, 0, sizeof (as_nail_fx));
	memset (as_nail_tracks, 0, sizeof (as_nail_tracks));
	as_nail_serial = 0;
}
static void AS_NailEvent (const vec3_t p, const vec3_t d, const vec3_t n, int kind)
{
	GL_SynchronizeEndRenderingTask ();
	as_nail_fx_t *f = &as_nail_fx[as_nail_serial++ % 64];
	VectorCopy (p, f->p);
	VectorCopy (d, f->d);
	VectorCopy (n, f->n);
	f->floor = p[2] - 512;
	if (kind != 2)
	{
		vec3_t start, end, hit, normal;
		int	   entity;
		VectorMA (p, 2, n, start);
		VectorCopy (start, end);
		end[2] -= 512;
		if (CL_TraceLine (start, end, hit, normal, &entity) < 1 && normal[2] > .5f)
			f->floor = hit[2] + .3f;
	}
	f->born = cl.time;
	f->seed = as_nail_serial & 65535;
	f->kind = kind;
}
void AS_NailTrail (entity_t *ent, const vec3_t previous, int entity)
{
	if (!as_effects.value || !as_nails.value || cl.paused || !ent->model)
		return;
	if (strcmp (ent->model->name, "progs/spike.mdl") && strcmp (ent->model->name, "progs/s_spike.mdl"))
		return;
	vec3_t d;
	VectorSubtract (ent->origin, previous, d);
	float distance = VectorNormalize (d);
	if (distance < .01f || distance > 128)
		return;
	int		 slot = entity % 128;
	qboolean fresh = as_nail_tracks[slot].entity != entity || cl.time - as_nail_tracks[slot].seen > .2;
	VectorCopy (ent->origin, as_nail_tracks[slot].p);
	VectorCopy (d, as_nail_tracks[slot].d);
	as_nail_tracks[slot].seen = cl.time;
	as_nail_tracks[slot].entity = entity;
	if (fresh || cl.time - as_nail_tracks[slot].emitted >= .04)
	{
		AS_NailEvent (ent->origin, d, d, 2);
		as_nail_tracks[slot].emitted = cl.time;
	}
}
void AS_NailImpact (const vec3_t position, qboolean super)
{
	if (!as_effects.value || !as_nails.value || !cl.worldmodel)
		return;
	vec3_t incoming, a, b, hit, normal;
	vec3_t right, up;
	AngleVectors (cl.viewangles, incoming, right, up);
	float best = 160 * 160;
	for (int i = 0; i < 128; ++i)
	{
		if (!as_nail_tracks[i].entity || cl.time - as_nail_tracks[i].seen > .25 || cl.time < as_nail_tracks[i].seen)
			continue;
		vec3_t delta;
		VectorSubtract (position, as_nail_tracks[i].p, delta);
		float score = DotProduct (delta, delta);
		if (score < best && DotProduct (delta, as_nail_tracks[i].d) > -8)
		{
			best = score;
			VectorCopy (as_nail_tracks[i].d, incoming);
		}
	}
	int entity = -1;
	VectorMA (position, -12, incoming, a);
	VectorMA (position, 12, incoming, b);
	float fraction = CL_TraceLine (a, b, hit, normal, &entity);
	if (fraction >= 1 || VectorLength (normal) < .5)
	{
		float nearest = FLT_MAX;
		entity = -1;
		for (int axis = 0; axis < 3; ++axis)
			for (int sign = -1; sign <= 1; sign += 2)
			{
				vec3_t h, n;
				int	   e;
				VectorCopy (position, a);
				VectorCopy (position, b);
				a[axis] -= sign * 8;
				b[axis] += sign * 8;
				if (CL_TraceLine (a, b, h, n, &e) < 1 && VectorLength (n) > .5)
				{
					vec3_t delta;
					VectorSubtract (h, position, delta);
					float distance = DotProduct (delta, delta);
					if (distance < nearest)
					{
						nearest = distance;
						entity = e;
						VectorCopy (h, hit);
						VectorCopy (n, normal);
					}
				}
			}
		if (entity < 0)
		{
			VectorCopy (position, hit);
			VectorScale (incoming, -1, normal);
		}
	}
	if (DotProduct (incoming, normal) > 0)
		VectorInverse (incoming);
	AS_NailEvent (hit, incoming, normal, super ? 3 : 1);
	if (entity >= 0)
		AS_NailScar (hit, normal, entity, super ? 5.5f : 3.8f);
	if (COM_CheckParm ("-test-nails"))
		Con_Printf ("Aftershock nail impact: super=%d surface=%d visual-only\n", super, entity);
}
static void AS_DrawNails (cb_context_t *cbx)
{
	if (!as_nails.value)
		return;
	extern gltexture_t *particletexture3;
	vulkan_globals.vk_cmd_bind_descriptor_sets (
		cbx->cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan_globals.basic_pipeline_layout.handle, 0, 1, &particletexture3->descriptor_set, 0, NULL);
	R_BindPipeline (
		cbx, VK_PIPELINE_BIND_POINT_GRAPHICS,
		R_PipelineForSubpassType (
			cbx->subpass_type, vulkan_globals.aftershock_fx[cbx->pipeline_variant], vulkan_globals.aftershock_fx_oit, vulkan_globals.aftershock_fx_moment,
			vulkan_globals.aftershock_fx_composite));
	for (int i = 0; i < 64; ++i)
	{
		as_nail_fx_t *f = &as_nail_fx[i];
		float		  age = (float)(cl.time - f->born);
		if (!f->kind || age < 0 || age > (f->kind == 2 ? .24f : 1.1f))
			continue;
		vec3_t delta;
		VectorSubtract (f->p, r_origin, delta);
		if (DotProduct (delta, delta) > 1200 * 1200 || (f->kind == 2 && DotProduct (delta, delta) < 48 * 48))
			continue;
		VkBuffer	 vb;
		VkDeviceSize vo;
		float		*v = (float *)R_VertexAllocate (80, &vb, &vo);
		memset (v, 0, 80);
		VectorCopy (f->p, v);
		v[3] = age;
		v[4] = (float)f->seed;
		v[5] = 1;
		v[6] = as_reduced_flashes.value;
		VectorCopy (f->d, v + 8);
		v[11] = 4096 + f->kind;
		VectorCopy (f->n, v + 12);
		v[16] = f->floor;
		vulkan_globals.vk_cmd_bind_vertex_buffers (cbx->cb, 0, 1, &vb, &vo);
		int count = f->kind == 2 ? 32 : 104;
		vulkan_globals.vk_cmd_draw (cbx->cb, 24, count, 0, 0);
		Atomic_AddUInt32 (&rs_particles, count);
	}
}

void AS_NailRipple (float output[4])
{
	memset (output, 0, 16);
	output[3] = -1;
	if (!as_nails.value || !as_effects.value || as_reduced_flashes.value)
		return;
	float best = FLT_MAX;
	for (int i = 0; i < 64; ++i)
	{
		as_nail_fx_t *f = &as_nail_fx[i];
		float		  age = (float)(cl.time - f->born);
		if (!f->kind || age < 0 || age > .35f)
			continue;
		vec3_t delta;
		VectorSubtract (f->p, r_origin, delta);
		float score = DotProduct (delta, delta) * (f->kind == 2 ? 4 : 1);
		if (score < best)
		{
			best = score;
			VectorCopy (f->p, output);
			output[3] = age;
		}
	}
}
