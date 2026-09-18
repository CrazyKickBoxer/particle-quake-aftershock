/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
#include "quakedef.h"
#include "aftershock.h"
#include "../../src/aftershock_physics.h"
#include "../../src/aftershock_scatter.h"

#define AS_MAX_CHUNKS 8192
#define AS_MAX_PANELS 512
typedef struct
{
	as_sample_t		*samples;
	int				 count;
	size_t			 offset;
	as_surface_gpu_t gpu;
	size_t			 scatter_offset[4];
	int				 scatter_count[4];
	int				 edge_count;
    size_t architectural_offset;
    int architectural_count;
	vec3_t			 bounds_min, bounds_max;
} as_face_t;
typedef struct
{
	as_surface_gpu_t surface;
	float			 normal_mode[4];
    float wave[4], accent[4];
} as_structure_gpu_t;
_Static_assert (sizeof (as_structure_gpu_t) == 144, "Structure GPU layout");
static VkBuffer		   as_scatter_buffer;
static vulkan_memory_t as_scatter_memory;
static VkDeviceSize	   as_scatter_grid_offset;
static size_t		   as_scatter_bytes;
static VkBuffer		   as_sample_buffer;
static vulkan_memory_t as_sample_memory;
static VkBuffer		   as_grid_buffer;
static vulkan_memory_t as_grid_memory;
typedef struct
{
	int	   a, b, axis, u, v, first, count, nx, ny;
	vec3_t mins, maxs;
} as_panel_t;
cvar_t		  as_renderer = {"as_renderer", "0", CVAR_ARCHIVE};
cvar_t		  as_worldmode = {"as_worldmode", "0", CVAR_ROM};
cvar_t		  as_density = {"as_density", "4", CVAR_ARCHIVE};
cvar_t		  as_style = {"as_style", "1", CVAR_ARCHIVE};
cvar_t		  as_radius = {"as_radius", "130", CVAR_ARCHIVE};
cvar_t		  as_damage = {"as_damage", "6", CVAR_ARCHIVE};
cvar_t as_progressive = {"as_progressive", "1", CVAR_ARCHIVE};
cvar_t as_rocket_hits = {"as_rocket_hits", "4", CVAR_ARCHIVE};
cvar_t as_chip_radius = {"as_chip_radius", "72", CVAR_ARCHIVE};
cvar_t		  as_effects = {"as_effects", "1", CVAR_ARCHIVE};
cvar_t		  as_shake = {"as_shake", "0.35", CVAR_ARCHIVE};
cvar_t		  as_reduced_flashes = {"as_reduced_flashes", "0", CVAR_ARCHIVE};
cvar_t		  as_particle_amount = {"as_particle_amount", "2", CVAR_ARCHIVE};
cvar_t		  as_structure = {"as_structure", "11", CVAR_ARCHIVE};
cvar_t as_fidelity = {"as_fidelity", "1", CVAR_ARCHIVE};
cvar_t as_reflections = {"as_reflections", "1", CVAR_ARCHIVE};
cvar_t as_neon_prism = {"as_neon_prism", "0", CVAR_ARCHIVE};
cvar_t as_reflection_strength = {"as_reflection_strength", "1", CVAR_ARCHIVE};
cvar_t as_reflection_roughness = {"as_reflection_roughness", "0.20", CVAR_ARCHIVE};
cvar_t as_neon_glow = {"as_neon_glow", "1", CVAR_ARCHIVE};
cvar_t as_neon_npc_texture = {"as_neon_npc_texture", "1", CVAR_ARCHIVE};
cvar_t as_smw = {"as_smw", "0", CVAR_ARCHIVE};
cvar_t as_smw_pixel = {"as_smw_pixel", "4", CVAR_ARCHIVE};
cvar_t as_smw_outline = {"as_smw_outline", "1", CVAR_ARCHIVE};
cvar_t as_smw_edge = {"as_smw_edge", "0.10", CVAR_ARCHIVE};
cvar_t as_smw_saturate = {"as_smw_saturate", "1.30", CVAR_ARCHIVE};
cvar_t		  as_layers = {"as_layers", "3", CVAR_ARCHIVE};
extern cvar_t as_gibs, as_goo;
// Neon fidelity swaps the whole screen-effects compute pass out for its own
// (r_passes.c: "if (parms->fidelity) AS_FidelityDraw"), so SMW - which lives in
// that pass - never runs while fidelity is on. Stash the setting on the way in
// and hand it back on the way out instead of silently clobbering it. -1 means
// nothing is stashed, so repeat enables don't overwrite the saved value.
static float as_smw_stashed_fidelity = -1.0f;
static void	 AS_SMWChanged (cvar_t *v)
{
	if (v->value != 0)
	{
		if (as_smw_stashed_fidelity < 0)
			as_smw_stashed_fidelity = as_fidelity.value;
		Cvar_SetValueQuick (&as_fidelity, 0);
	}
	else if (as_smw_stashed_fidelity >= 0)
	{
		Cvar_SetValueQuick (&as_fidelity, as_smw_stashed_fidelity);
		as_smw_stashed_fidelity = -1.0f;
	}
}
static int	  AS_StructureMode (void)
{
	return isfinite (as_structure.value) ? CLAMP (0, (int)as_structure.value, 11) : 10;
}
qboolean AS_Neon (void)
{
	return as_renderer.value && AS_StructureMode () == 11;
}
static struct
{
	vec3_t	 origin;
	vec3_t	 direction;
	double	 born;
	float	 strength;
	float	 floor;
    float floor_plane[4];
	uint32_t seed;
} as_fx[32];
static struct
{
	vec3_t	 origin, direction;
	double	 born;
	qboolean used;
} as_launches[32];
static unsigned		 as_launch_serial;
static void			 AS_VisualExplosionImpl (const vec3_t position, qboolean gore);
static uint32_t		 as_fx_serial;
static int			 as_fx_count;
static qmodel_t		*as_model;
static as_face_t	*as_faces;
static int			*as_face_panel;
static as_panel_t	 as_panels[AS_MAX_PANELS];
static int			 as_num_panels, as_num_chunks;
static as_chunk_desc as_chunks[AS_MAX_CHUNKS];
static as_chunk_pose as_poses[AS_MAX_CHUNKS];
static double as_fracture_born[AS_MAX_CHUNKS];
#include "aftershock_nails.inc"
static hull_t		 as_original_hulls[MAX_MAP_HULLS];
static mclipnode_t	*as_nodes[MAX_MAP_HULLS];
static mplane_t		*as_planes[MAX_MAP_HULLS];
static int			 as_last_detached, as_tick, as_frames, as_capture_tick, as_end_frames;
static qboolean		 as_ready;
static char			 as_capture_path[MAX_OSPATH];
static char			 as_pending_load[MAX_OSPATH];
static uint64_t		 as_map_hash;
static struct
{
	vec3_t	 origin;
	float	 radius, damage;
	unsigned seed;
} as_events[64];
static int		as_event_count;
static int		as_test_explosion_tick, as_test_panel;
static qboolean as_test_crossed, as_test_shot_through;
static int		as_checksum_tick, as_save_tick, as_load_tick, as_sequence_start, as_sequence_step;
static int		as_switch_tick, as_gpu_count, as_walk_ticks = 120;
static double	as_gpu_total, as_physics_total;
static int		as_physics_steps;
static size_t	as_static_sample_bytes;
static char		as_sequence[MAX_OSPATH], as_benchmark[MAX_OSPATH];
static double	as_begin_time, as_frame_times[16384], as_last_frame;
static int		as_frame_count;
static uint64_t as_geometry_hash, as_source_hash;
static float	as_saved_novis;
static qboolean as_novis_override;
static void		AS_Inspect_f (void);
static struct
{
	sizebuf_t *dest;
	int		   state, coords;
	vec3_t	   p;
} as_wire;

#include "aftershock_showcase.inc"

static void AS_Log (const char *text)
{
	Con_Printf ("%s\n", text);
}
static uint64_t AS_Hash (uint64_t h, const void *data, size_t bytes)
{
	const byte *p = data;
	for (size_t k = 0; k < bytes; ++k)
	{
		h ^= p[k];
		h *= UINT64_C (1099511628211);
	}
	return h;
}
static uint64_t AS_GeometryHash (void)
{
	uint64_t h = AS_Hash (UINT64_C (1469598103934665603), as_model->name, strlen (as_model->name));
	h = AS_Hash (h, com_gamedir, strlen (com_gamedir));
	for (int i = 0; i < as_model->numsurfaces; ++i)
	{
		msurface_t *s = &as_model->surfaces[i];
		if (!s->polys)
			continue;
		h = AS_Hash (h, s->polys->verts, s->polys->numverts * VERTEXSIZE * sizeof (float));
		h = AS_Hash (h, s->texinfo->vecs, sizeof (s->texinfo->vecs));
		h = AS_Hash (h, s->texturemins, sizeof (s->texturemins));
		h = AS_Hash (h, s->plane->normal, sizeof (vec3_t));
		h = AS_Hash (h, &s->plane->dist, sizeof (float));
		h = AS_Hash (h, s->texinfo->texture->name, strlen (s->texinfo->texture->name));
		uint32_t fields[] = {s->flags, s->light_s, s->light_t, s->lightmaptexturenum, s->texinfo->texture->width, s->texinfo->texture->height};
		h = AS_Hash (h, fields, sizeof (fields));
	}
	return h;
}
static uint64_t AS_StructureHash (void)
{
	/* Structural saves must not depend on transient lightmap atlas placement,
	   render UVs or which renderer was active when a map loaded. */
	const uint32_t schema = 4;
	uint64_t	   h = AS_Hash (UINT64_C (1469598103934665603), &schema, sizeof (schema));
	h = AS_Hash (h, as_model->name, strlen (as_model->name));
	for (int i = 0; i < as_model->nummodelsurfaces; ++i)
	{
		msurface_t *s = &as_model->surfaces[i];
		h = AS_Hash (h, s->plane->normal, sizeof (vec3_t));
		h = AS_Hash (h, &s->plane->dist, sizeof (float));
		h = AS_Hash (h, s->texinfo->vecs, sizeof (s->texinfo->vecs));
		h = AS_Hash (h, s->texinfo->texture->name, strlen (s->texinfo->texture->name));
		if (s->polys)
		{
			h = AS_Hash (h, &s->polys->numverts, sizeof (s->polys->numverts));
			for (int j = 0; j < s->polys->numverts; ++j)
				h = AS_Hash (h, s->polys->verts[j], sizeof (vec3_t));
		}
	}
	return h;
}
static void AS_CachePath (char *path, size_t size)
{
	char folder[MAX_OSPATH];
	q_snprintf (folder, sizeof (folder), "%s/aftershock-cache", host_parms->userdir);
	Sys_mkdir (folder);
	q_snprintf (path, size, "%s/samples-v4-%016llx.bin", folder, (unsigned long long)as_geometry_hash);
}
static qboolean AS_ReadCache (size_t *total)
{
	char path[MAX_OSPATH];
	AS_CachePath (path, sizeof (path));
	FILE *f = AS_OpenFile (path, "rb");
	if (!f)
		return false;
	uint32_t header[4];
	uint64_t hash, expected, sum = UINT64_C (1469598103934665603);
	qboolean valid = fread (header, sizeof (header), 1, f) == 1 && fread (&hash, 8, 1, f) == 1 && header[0] == 0x41534348 && header[1] == 4 &&
					 header[2] == (uint32_t)as_model->numsurfaces && header[3] < 64000000 && hash == as_geometry_hash;
	*total = 0;
	for (int i = 0; valid && i < as_model->numsurfaces; ++i)
	{
		as_face_t *face = &as_faces[i];
		uint32_t   count;
		valid = fread (&count, 4, 1, f) == 1 && count <= header[3] - *total && fread (&face->gpu, sizeof (face->gpu), 1, f) == 1;
		if (!valid)
			break;
		for (int k = 0; k < 24; ++k)
			if (!isfinite (((float *)&face->gpu)[k]))
				valid = false;
		if (!valid)
			break;
		face->count = count;
		face->offset = *total * 4;
		face->samples = Mem_Alloc (count * 4);
		valid = fread (face->samples, 4, count, f) == count;
		sum = AS_Hash (sum, &count, 4);
		sum = AS_Hash (sum, &face->gpu, sizeof (face->gpu));
		sum = AS_Hash (sum, face->samples, count * 4);
		*total += count;
	}
	valid = valid && *total == header[3] && fread (&expected, 8, 1, f) == 1 && expected == sum && fgetc (f) == EOF;
	fclose (f);
	if (!valid)
	{
		for (int i = 0; i < as_model->numsurfaces; ++i)
		{
			Mem_Free (as_faces[i].samples);
			memset (&as_faces[i], 0, sizeof (as_faces[i]));
		}
		*total = 0;
		Con_Printf ("Aftershock: rejected corrupt sample cache\n");
	}
	else
		Con_Printf ("Aftershock: validated sample cache hit\n");
	return valid;
}
static void AS_WriteCache (size_t total)
{
	char path[MAX_OSPATH], temporary[MAX_OSPATH];
	AS_CachePath (path, sizeof (path));
	q_snprintf (temporary, sizeof (temporary), "%s.tmp", path);
	FILE *f = AS_OpenFile (temporary, "wb");
	if (!f)
		return;
	uint32_t header[4] = {0x41534348, 4, (uint32_t)as_model->numsurfaces, (uint32_t)total};
	uint64_t sum = UINT64_C (1469598103934665603);
	qboolean valid = fwrite (header, sizeof (header), 1, f) == 1 && fwrite (&as_geometry_hash, 8, 1, f) == 1;
	for (int i = 0; valid && i < as_model->numsurfaces; ++i)
	{
		as_face_t *face = &as_faces[i];
		uint32_t   count = face->count;
		valid = fwrite (&count, 4, 1, f) == 1 && fwrite (&face->gpu, sizeof (face->gpu), 1, f) == 1 && fwrite (face->samples, 4, count, f) == count;
		sum = AS_Hash (sum, &count, 4);
		sum = AS_Hash (sum, &face->gpu, sizeof (face->gpu));
		sum = AS_Hash (sum, face->samples, count * 4);
	}
	valid = fwrite (&sum, 8, 1, f) == 1 && valid;
	valid = fclose (f) == 0 && valid;
	if (valid)
		AS_AtomicReplace (temporary, path);
	else
		AS_RemoveFile (temporary);
}
static void AS_Bounds (msurface_t *s, vec3_t mins, vec3_t maxs)
{
	for (int a = 0; a < 3; ++a)
	{
		mins[a] = FLT_MAX;
		maxs[a] = -FLT_MAX;
	}
	if (!s->polys)
		return;
	for (int i = 0; i < s->polys->numverts; ++i)
		for (int a = 0; a < 3; ++a)
		{
			mins[a] = q_min (mins[a], s->polys->verts[i][a]);
			maxs[a] = q_max (maxs[a], s->polys->verts[i][a]);
		}
}
static qboolean AS_Rect (msurface_t *s, int axis)
{
	if (!s->polys || s->polys->numverts != 4 || s->flags & (SURF_DRAWTILED | SURF_DRAWFENCE))
		return false;
	vec3_t mins, maxs;
	AS_Bounds (s, mins, maxs);
	for (int i = 0; i < 4; ++i)
		for (int a = 0; a < 3; ++a)
			if (a != axis && fabsf (s->polys->verts[i][a] - mins[a]) > 0.1f && fabsf (s->polys->verts[i][a] - maxs[a]) > 0.1f)
				return false;
	return true;
}
static void AS_ExtractPanels (void)
{
	int total = as_model->nummodelsurfaces;
	for (int orientation = 0; orientation < 2; ++orientation)
		for (int i = 0; i < total && as_num_panels < AS_MAX_PANELS; ++i)
		{
			msurface_t *s = &as_model->surfaces[i];
			int			axis = s->plane->type;
			if (axis > 2 || (orientation == 0 ? axis == 2 : axis != 2) || as_face_panel[i] >= 0 || !AS_Rect (s, axis))
				continue;
			int	   u = axis == 2 ? 0 : 1 - axis, v = axis == 2 ? 1 : 2;
			vec3_t lo, hi;
			AS_Bounds (s, lo, hi);
			if (hi[u] - lo[u] < (as_progressive.value ? 24 : 48) || hi[v] - lo[v] < (as_progressive.value && axis == 2 ? 24 : 48))
				continue;
			for (int j = i + 1; j < total; ++j)
			{
				msurface_t *t = &as_model->surfaces[j];
				if (as_face_panel[j] >= 0 || t->plane->type != axis || !AS_Rect (t, axis) || !((s->flags ^ t->flags) & SURF_PLANEBACK))
					continue;
				vec3_t tl, th;
				AS_Bounds (t, tl, th);
				float thickness = fabsf (tl[axis] - lo[axis]);
				if (thickness < 4 || thickness > (as_progressive.value ? 96 : 64) || fabsf (tl[u] - lo[u]) > 0.1f || fabsf (th[u] - hi[u]) > 0.1f || fabsf (tl[v] - lo[v]) > 0.1f ||
					fabsf (th[v] - hi[v]) > 0.1f)
					continue;
				vec3_t middle;
				for (int a = 0; a < 3; ++a)
					middle[a] = (lo[a] + hi[a] + tl[a] + th[a]) * 0.25f;
				if (Mod_PointInLeaf (middle, as_model)->contents != CONTENTS_SOLID)
					continue;
				vec3_t probe;
				VectorCopy (middle, probe);
				probe[axis] = q_min (lo[axis], tl[axis]) - 1;
				if (Mod_PointInLeaf (probe, as_model)->contents != CONTENTS_EMPTY)
					continue;
				probe[axis] = q_max (hi[axis], th[axis]) + 1;
				if (Mod_PointInLeaf (probe, as_model)->contents != CONTENTS_EMPTY)
					continue;
				int		 nx = (int)ceilf ((hi[u] - lo[u]) / 24), ny = (int)ceilf ((hi[v] - lo[v]) / 24);
				qboolean valid = true;
				/* Validate each cell's solid interior and both exposed sides. This
				   excludes void seals, crossing cavities, and overlapping volumes. */
				for (int y = 0; y <= ny && valid; ++y)
					for (int x = 0; x <= nx && valid; ++x)
					{
						probe[u] = CLAMP (lo[u] + 0.25f, lo[u] + (hi[u] - lo[u]) * x / nx, hi[u] - 0.25f);
						probe[v] = CLAMP (lo[v] + 0.25f, lo[v] + (hi[v] - lo[v]) * y / ny, hi[v] - 0.25f);
						for (int depth = 0; depth <= 4; ++depth)
						{
							probe[axis] = q_min (lo[axis], tl[axis]) + 0.25f + (thickness - 0.5f) * depth / 4;
							if (Mod_PointInLeaf (probe, as_model)->contents != CONTENTS_SOLID)
							{
								valid = false;
								break;
							}
						}
						probe[axis] = q_min (lo[axis], tl[axis]) - 0.25f;
						if (Mod_PointInLeaf (probe, as_model)->contents != CONTENTS_EMPTY)
							valid = false;
						probe[axis] = q_max (hi[axis], th[axis]) + 0.25f;
						if (Mod_PointInLeaf (probe, as_model)->contents != CONTENTS_EMPTY)
							valid = false;
					}
				for (int k = 0; k < as_num_panels && valid; ++k)
				{
					qboolean overlaps = true;
					for (int a = 0; a < 3; ++a)
					{
						float low = a == axis ? q_min (lo[a], tl[a]) : lo[a], high = a == axis ? q_max (hi[a], th[a]) : hi[a];
						if (q_min (high, as_panels[k].maxs[a]) - q_max (low, as_panels[k].mins[a]) < 0.1f)
							overlaps = false;
					}
					if (overlaps)
						valid = false;
				}
				if (!valid)
					continue;
				if (as_num_chunks + nx * ny > AS_MAX_CHUNKS)
					break;
				as_panel_t *p = &as_panels[as_num_panels];
				*p = (as_panel_t){.a = i, .b = j, .axis = axis, .u = u, .v = v, .first = as_num_chunks, .count = nx * ny, .nx = nx, .ny = ny};
				VectorCopy (lo, p->mins);
				VectorCopy (hi, p->maxs);
				p->mins[axis] = q_min (lo[axis], tl[axis]);
				p->maxs[axis] = q_max (hi[axis], th[axis]);
				for (int y = 0; y < ny; ++y)
					for (int x = 0; x < nx; ++x)
					{
						as_chunk_desc *c = &as_chunks[as_num_chunks++];
						memset (c, 0, sizeof (*c));
						VectorCopy (p->mins, c->mins);
						VectorCopy (p->maxs, c->maxs);
						c->mins[u] = lo[u] + (hi[u] - lo[u]) * x / nx;
						c->maxs[u] = lo[u] + (hi[u] - lo[u]) * (x + 1) / nx;
						c->mins[v] = lo[v] + (hi[v] - lo[v]) * y / ny;
						c->maxs[v] = lo[v] + (hi[v] - lo[v]) * (y + 1) / ny;
						c->surface = i;
						c->anchored = axis == 2 ? (x == 0 || y == 0 || x == nx - 1 || y == ny - 1) : (y == 0);
                        if(as_progressive.value){
                            // Bridges connect at their span ends, not along every rim cell.
                            // Anchor only where the BSP actually contains supporting solid.
                            int supportAxis=axis==2 ? (nx>=ny?u:v) : v;
                            int cell=axis==2 && nx>=ny?x:y;
                            int cells=axis==2 && nx>=ny?nx:ny;
                            c->anchored=0;
                            if(cell==0 || cell==cells-1){
                                vec3_t support;for(int a=0;a<3;++a)support[a]=(c->mins[a]+c->maxs[a])*.5f;
                                if(cell==0){support[supportAxis]=p->mins[supportAxis]-.5f;c->anchored=Mod_PointInLeaf(support,as_model)->contents==CONTENTS_SOLID;}
                                if(cell==cells-1){support[supportAxis]=p->maxs[supportAxis]+.5f;c->anchored|=Mod_PointInLeaf(support,as_model)->contents==CONTENTS_SOLID;}
                            }
                        }
						c->material = 0;
						const char *name = s->texinfo->texture->name;
						if (strstr (name, "metal") || strstr (name, "tech"))
							c->material = 1;
						else if (strstr (name, "wood"))
							c->material = 2;
					}
				as_face_panel[i] = as_face_panel[j] = as_num_panels++;
                // Remove fully contained end/cap faces as well as the paired faces.
                // Otherwise a broken pillar leaves a floating cap in the original BSP draw.
                for(int cap=0;cap<total;++cap) {
                    if(as_face_panel[cap]>=0)continue;
                    msurface_t *face=&as_model->surfaces[cap];int a=face->plane->type;
                    if(a>2 || !AS_Rect(face,a))continue;
                    vec3_t fl,fh;AS_Bounds(face,fl,fh);
                    if(fabsf(fl[a]-p->mins[a])>.1f && fabsf(fl[a]-p->maxs[a])>.1f)continue;
                    qboolean contained=true;for(int c=0;c<3;++c)if(fl[c]<p->mins[c]-.1f || fh[c]>p->maxs[c]+.1f)contained=false;
                    if(contained)as_face_panel[cap]=as_num_panels-1;
                }
				break;
			}
		}
    if(as_progressive.value)for(int id=0;id<as_num_chunks;++id){
        as_chunk_desc *c=&as_chunks[id];if(!c->anchored)continue;
        as_panel_t *p=&as_panels[as_face_panel[c->surface]];
        int a=p->axis==2?(p->nx>=p->ny?p->u:p->v):p->v;
        c->anchored=0;
        for(int side=0;side<2;++side){
            float edge=side?p->maxs[a]:p->mins[a];
            if(fabsf((side?c->maxs[a]:c->mins[a])-edge)>.1f)continue;
            vec3_t support;for(int k=0;k<3;++k)support[k]=(c->mins[k]+c->maxs[k])*.5f;
            support[a]=edge+(side?.5f:-.5f);
            if(Mod_PointInLeaf(support,as_model)->contents!=CONTENTS_SOLID)continue;
            qboolean destructible=false;
            for(int other=0;other<as_num_chunks && !destructible;++other){
                if(other==id)continue;qboolean inside=true;
                for(int k=0;k<3;++k)if(support[k]<as_chunks[other].mins[k] || support[k]>as_chunks[other].maxs[k])inside=false;
                if(inside)destructible=true;
            }
            // Destructible supports connect through Blast bonds, never to the
            // immortal world node. Removing a pillar can therefore drop its bridge.
            if(!destructible)c->anchored=1;
        }
    }

}
static void AS_ExtractSamples (msurface_t *s, as_face_t *out)
{
	if (!s->polys || (s->flags & SURF_DRAWTILED))
		return;
	glpoly_t  *poly = s->polys;
	float	   step = CLAMP (1, as_density.value, 12);
	texture_t *texture = s->texinfo->texture;
	vec3_t	   ts, tn, st;
	CrossProduct (s->texinfo->vecs[1], s->plane->normal, ts);
	CrossProduct (s->plane->normal, s->texinfo->vecs[0], tn);
	CrossProduct (s->texinfo->vecs[0], s->texinfo->vecs[1], st);
	float determinant = DotProduct (s->texinfo->vecs[0], ts);
	if (fabsf (determinant) < 0.0001f)
		return;
	float low[2] = {FLT_MAX, FLT_MAX}, high[2] = {-FLT_MAX, -FLT_MAX}, uv[128][2];
	if (poly->numverts > 128)
		return;
	for (int i = 0; i < poly->numverts; ++i)
		for (int a = 0; a < 2; ++a)
		{
			uv[i][a] = DotProduct (poly->verts[i], s->texinfo->vecs[a]) + s->texinfo->vecs[a][3] - s->texturemins[a];
			low[a] = q_min (low[a], uv[i][a]);
			high[a] = q_max (high[a], uv[i][a]);
		}
	if (low[0] < -0.125f || low[1] < -0.125f || high[0] > 8191 || high[1] > 8191)
		Host_Error ("Aftershock: surface exceeds packed patch range");
	for (int a = 0; a < 3; ++a)
	{
		out->gpu.origin[a] =
			(ts[a] * (s->texturemins[0] - s->texinfo->vecs[0][3]) + tn[a] * (s->texturemins[1] - s->texinfo->vecs[1][3]) + st[a] * s->plane->dist) /
			determinant;
		out->gpu.saxis[a] = ts[a] / determinant * 0.125f;
		out->gpu.taxis[a] = tn[a] / determinant * 0.125f;
	}
	out->gpu.origin[3] = step * 8 * 0.58f;
	if (!as_model->lightdata)
	{
		out->gpu.params[2] = 1;
		out->gpu.params[3] = 1;
	}
	out->gpu.tex[0] = (float)s->texturemins[0] / texture->width;
	out->gpu.tex[1] = (float)s->texturemins[1] / texture->height;
	out->gpu.tex[2] = 0.125f / texture->width;
	out->gpu.tex[3] = 0.125f / texture->height;
	out->gpu.light[0] = (s->light_s * 16 + 8.0f) / (LMBLOCK_WIDTH * 16);
	out->gpu.light[1] = (s->light_t * 16 + 8.0f) / (LMBLOCK_HEIGHT * 16);
	out->gpu.light[2] = 0.125f / (LMBLOCK_WIDTH * 16);
	out->gpu.light[3] = 0.125f / (LMBLOCK_HEIGHT * 16);
	int nx = (int)ceilf ((high[0] - low[0]) / step), ny = (int)ceilf ((high[1] - low[1]) / step);
	nx = q_max (nx, 1);
	ny = q_max (ny, 1);
	int capacity = (nx + 1) * (ny + 1) + poly->numverts * 1024;
	out->samples = Mem_Alloc (capacity * 4);
	for (int y = 0; y <= ny; ++y)
		for (int x = 0; x <= nx; ++x)
		{
			float u = low[0] + (high[0] - low[0]) * x / nx, v = low[1] + (high[1] - low[1]) * y / ny;
			int	  sign = 0;
			bool  inside = true;
			for (int k = 0; k < poly->numverts; ++k)
			{
				int	  next = (k + 1) % poly->numverts;
				float cross = (uv[next][0] - uv[k][0]) * (v - uv[k][1]) - (uv[next][1] - uv[k][1]) * (u - uv[k][0]);
				if (fabsf (cross) > 0.02f)
				{
					int current = cross > 0 ? 1 : -1;
					if (sign && sign != current)
					{
						inside = false;
						break;
					}
					sign = current;
				}
			}
			if (inside)
			{
				if (!AS_PackSample (q_max (0, u), q_max (0, v), &out->samples[out->count]))
					Host_Error ("Aftershock sample outside patch");
				++out->count;
			}
		}
	for (int k = 0; k < poly->numverts; ++k)
	{
		int	  next = (k + 1) % poly->numverts;
		float dx = uv[next][0] - uv[k][0], dy = uv[next][1] - uv[k][1];
		int	  n = q_max (1, (int)ceilf (sqrtf (dx * dx + dy * dy) / step));
		for (int j = 0; j < n; ++j)
		{
			if (out->count >= capacity)
			{
				capacity *= 2;
				out->samples = Mem_Realloc (out->samples, capacity * 4);
			}
			float u = uv[k][0] + dx * j / n, v = uv[k][1] + dy * j / n;
			if (!AS_PackSample (q_max (0, u), q_max (0, v), &out->samples[out->count]))
				Host_Error ("Aftershock seam sample outside patch");
			++out->count;
		}
	}
}

static void AS_FreeScatter (void)
{
	if (!as_scatter_buffer)
		return;
	GL_WaitForDeviceIdle ();
	R_FreeBuffer (as_scatter_buffer, &as_scatter_memory, NULL);
	as_scatter_buffer = VK_NULL_HANDLE;
	as_scatter_bytes = 0;
}
static void AS_BuildScatter (void)
{
	AS_FreeScatter ();
	size_t		 count = 0, capacity = 4096;
	as_sample_t *samples = Mem_Alloc (capacity * sizeof (*samples));
	uint64_t	 hash = UINT64_C (1469598103934665603);
    int *owners=Mem_Alloc(as_model->numedges*sizeof(int));
    byte *creases=Mem_Alloc(as_model->numedges);
    for(int e=0;e<as_model->numedges;++e) { owners[e]=-1; creases[e]=1; }
    for(int i=0;i<as_frames;++i) {
        msurface_t *s=&as_model->surfaces[i];
        if(!as_faces[i].count) continue;
        for(int j=0;j<s->numedges;++j) {
            int e=abs(as_model->surfedges[s->firstedge+j]);
            if(owners[e]<0) owners[e]=i;
            else {
                msurface_t *other=&as_model->surfaces[owners[e]];
                if(fabsf(DotProduct(s->plane->normal,other->plane->normal))>.999f) creases[e]=0;
            }
        }
    }
	for (int i = 0; i < as_frames; ++i)
	{
		as_face_t  *f = &as_faces[i];
		msurface_t *s = &as_model->surfaces[i];
		if (!f->count || !s->polys || s->polys->numverts > 128)
			continue;
		float uv[128][2], low[2] = {FLT_MAX, FLT_MAX}, high[2] = {-FLT_MAX, -FLT_MAX};
		int	  n = s->polys->numverts;
		for (int a = 0; a < 3; ++a)
		{
			f->bounds_min[a] = FLT_MAX;
			f->bounds_max[a] = -FLT_MAX;
		}
		for (int j = 0; j < n; ++j)
			for (int a = 0; a < 3; ++a)
			{
				f->bounds_min[a] = q_min (f->bounds_min[a], s->polys->verts[j][a] - 20);
				f->bounds_max[a] = q_max (f->bounds_max[a], s->polys->verts[j][a] + 20);
			}

		for (int j = 0; j < n; ++j)
			for (int a = 0; a < 2; ++a)
			{
				uv[j][a] = DotProduct (s->polys->verts[j], s->texinfo->vecs[a]) + s->texinfo->vecs[a][3] - s->texturemins[a];
				low[a] = q_min (low[a], uv[j][a]);
				high[a] = q_max (high[a], uv[j][a]);
			}
		for (int j = 0; j < n; ++j)
		{
			int	  k = (j + 1) % n;
			float dx = uv[k][0] - uv[j][0], dy = uv[k][1] - uv[j][1];
			f->edge_count += q_max (1, (int)ceilf (sqrtf (dx * dx + dy * dy) / CLAMP (1, as_density.value, 12)));
		}
        f->architectural_offset=count*4;
        for(int j=0;j<s->numedges;++j) {
            int edge=abs(as_model->surfedges[s->firstedge+j]);
            if(!creases[edge] || owners[edge]!=i) continue;
            medge_t *e=&as_model->edges[edge]; float ends[2][2];
            for(int k=0;k<2;++k) for(int a=0;a<2;++a)
                ends[k][a]=DotProduct(as_model->vertexes[e->v[k]].position,s->texinfo->vecs[a])+s->texinfo->vecs[a][3]-s->texturemins[a];
            float dx=ends[1][0]-ends[0][0],dy=ends[1][1]-ends[0][1];
            int steps=q_max(1,(int)ceilf(sqrtf(dx*dx+dy*dy)/CLAMP(1,as_density.value,12)));
            for(int k=0;k<steps;++k) {
                as_sample_t sample; float t=(k+.5f)/steps;
                if(!AS_PackSample(ends[0][0]+dx*t,ends[0][1]+dy*t,&sample)) continue;
                if(count==capacity) { capacity*=2; samples=Mem_Realloc(samples,capacity*4); }
                samples[count++]=sample; ++f->architectural_count;
            }
        }
		float spacing = CLAMP (1, as_density.value, 12) * 3;
		int	  nx = q_max (1, (int)ceilf ((high[0] - low[0]) / spacing)), ny = q_max (1, (int)ceilf ((high[1] - low[1]) / spacing));
		for (int layer = 0; layer < 4; ++layer)
		{
			f->scatter_offset[layer] = count * 4;
			for (int y = 0; y < ny; ++y)
				for (int x = 0; x < nx; ++x)
				{
					as_sample_t sample;
					if (!AS_ScatterCell (uv, n, low, high, x, y, nx, ny, i, layer, &sample))
						continue;
					if (count == capacity)
					{
						capacity *= 2;
						samples = Mem_Realloc (samples, capacity * 4);
					}
					samples[count++] = sample;
					++f->scatter_count[layer];
				}
			size_t first = f->scatter_offset[layer] / 4;
			for (int j = f->scatter_count[layer] - 1; j > 0; --j)
			{
				size_t		other = first + AS_ScatterHash ((uint32_t)j + (uint32_t)i * 9127u + (uint32_t)layer * 65537u) % (j + 1);
				as_sample_t tmp = samples[first + j];
				samples[first + j] = samples[other];
				samples[other] = tmp;
			}
		}
	}
	Mem_Free(owners); Mem_Free(creases);
	hash = AS_Hash (hash, samples, count * 4);
	as_scatter_grid_offset = count * 4;
	if (capacity < count + 256)
	{
		capacity = count + 256;
		samples = Mem_Realloc (samples, capacity * 4);
	}
	const float poly[4][2] = {{0, 0}, {16, 0}, {16, 16}, {0, 16}}, low[2] = {0, 0}, high[2] = {16, 16};
	for (int layer = 0; layer < 4; ++layer)
		for (int y = 0; y < 8; ++y)
			for (int x = 0; x < 8; ++x)
			{
				as_sample_t sample;
				if (!AS_ScatterCell (poly, 4, low, high, x, y, 8, 8, 0x51a9u, layer, &sample))
					Host_Error ("Aftershock chunk scatter rejected");
				samples[count++] = sample;
			}
	R_CreateBuffer (
		&as_scatter_buffer, &as_scatter_memory, count * 4, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, NULL, NULL, "Aftershock layered scatter");
	R_StagingUploadBuffer (as_scatter_buffer, count * 4, (const byte *)samples);
	as_scatter_bytes = count * 4;
	Mem_Free (samples);
	Con_Printf ("Aftershock structure: %zu samples, four stable layers, hash=%016llx\n", count, (unsigned long long)hash);
}
static int AS_StructureVertices (int mode)
{
	return mode == 11 ? 6 : mode == 1 ? 24 : mode == 3 || mode == 6 ? 24 : mode == 4 ? 36 : mode == 5 ? 72 : mode == 7 ? 96 : 192;
}
#include "holo_physics.inc"

static void AS_DrawStructure (
	cb_context_t *cbx, const as_surface_gpu_t *source, const vec3_t normal, const size_t offsets[4], const int counts[4], int full, float scale, float detail,
	VkBuffer sample_buffer, float heat, qboolean reactive)
{
	int mode = AS_StructureMode ();
	if (!mode || !as_scatter_buffer || detail >= 3)
		return;
	qboolean holo=reactive && hp_draw_enabled && (sample_buffer==as_sample_buffer || sample_buffer==as_scatter_buffer);
    R_BindPipeline (cbx, VK_PIPELINE_BIND_POINT_GRAPHICS, holo?vulkan_globals.holo_structure[cbx->pipeline_variant][full]:vulkan_globals.aftershock_structure[cbx->pipeline_variant][full]);
	for (int layer = 0; layer < (scale < 0 ? 1 : q_max (1, CLAMP (1, (int)as_layers.value, 4) - (as_fidelity.value ? 0 : (int)detail))); ++layer)
	{
		if (!counts[layer])
			continue;
		VkBuffer			vb;
		VkDeviceSize		vo;
		as_structure_gpu_t *m = (as_structure_gpu_t *)R_VertexAllocate (sizeof (*m), &vb, &vo);
        memset(m,0,sizeof(*m));
        m->wave[3]=-1;
        m->accent[3]=as_neon_prism.value!=0?1.f:0.f;
        if(mode==11 && as_fidelity.value) {
            float best=FLT_MAX;
            for(int i=0;i<as_fx_count;++i) {
                float age=(float)(cl.time-as_fx[i].born);
                if(as_fx[i].strength<1 || age<0 || age>1.3f || as_reduced_flashes.value) continue;
                vec3_t delta; VectorSubtract(source->origin,as_fx[i].origin,delta);
                float distance=DotProduct(delta,delta);
                if(distance<best) { best=distance; VectorCopy(as_fx[i].origin,m->wave); m->wave[3]=age; }
            }
            m->accent[0]=as_reduced_flashes.value?0:heat;
        }
		m->surface = *source;
		m->surface.origin[3] *= fabsf (scale);
        if(as_fidelity.value && mode!=11)m->surface.origin[3]*=CLAMP(0,(3-detail)*2,1);
		m->surface.saxis[3] = mode == 11 ? 14 : as_style.value;
		VectorCopy (normal, m->normal_mode);
		m->normal_mode[3] = (float)(mode + layer * 16 + (scale < 0 ? 256 : 0) + (as_fidelity.value ? 512 : 0));
		VkBuffer	 buffers[2] = {sample_buffer, vb};
		VkDeviceSize locations[2] = {offsets[layer], vo};
        if(holo) HP_Bind(cbx,sample_buffer,offsets[layer]);
		vulkan_globals.vk_cmd_bind_vertex_buffers (cbx->cb, 0, 2, buffers, locations);
		vulkan_globals.vk_cmd_draw (cbx->cb, AS_StructureVertices (mode), q_max (1, (int)(counts[layer] * powf(.5f,detail))), 0, 0);
		Atomic_AddUInt32 (&rs_particles, q_max (1, (int)(counts[layer] * powf(.5f,detail))));
	}
}

void AS_ClearMap (void)
{
	GL_SynchronizeEndRenderingTask ();
    HP_Clear();
	AS_FreeScatter ();
	AS_GoreClear ();
    AS_NailsClear();
    memset(as_fracture_born,0,sizeof(as_fracture_born));
	memset (as_launches, 0, sizeof (as_launches));
	as_launch_serial = 0;
	if (as_sample_buffer)
	{
		GL_SynchronizeEndRenderingTask ();
		GL_WaitForDeviceIdle ();
		R_FreeBuffer (as_sample_buffer, &as_sample_memory, NULL);
		as_sample_buffer = VK_NULL_HANDLE;
	}
	if (as_grid_buffer)
	{
		GL_WaitForDeviceIdle ();
		R_FreeBuffer (as_grid_buffer, &as_grid_memory, NULL);
		as_grid_buffer = VK_NULL_HANDLE;
	}
	if (as_model && as_ready)
		for (int h = 0; h < MAX_MAP_HULLS; ++h)
			as_model->hulls[h] = as_original_hulls[h];
	AS_PhysicsShutdown ();
	if (as_faces)
	{
		for (int i = 0; i < as_frames; ++i)
			Mem_Free (as_faces[i].samples);
		Mem_Free (as_faces);
		as_faces = NULL;
	}
	Mem_Free (as_face_panel);
	as_face_panel = NULL;
	as_frames = as_num_panels = as_num_chunks = 0;
	as_static_sample_bytes = 0;
	for (int h = 0; h < MAX_MAP_HULLS; ++h)
	{
		Mem_Free (as_nodes[h]);
		Mem_Free (as_planes[h]);
		as_nodes[h] = NULL;
		as_planes[h] = NULL;
	}
	as_ready = false;
	as_model = NULL;
	as_wire.state = 0;
	as_event_count = 0;
	as_fx_count = 0;
	as_fx_serial = 0;
	if (as_novis_override)
	{
		Cvar_SetValue ("r_novis", as_saved_novis);
		as_novis_override = false;
	}
}
static void AS_RebuildHulls (void)
{
	/* Shared empty-box tree replaces every original solid terminal. Original
	   clip hulls stay intact; box openings are eroded by each actor hull. */
	for (int h = 0; h < MAX_MAP_HULLS; ++h)
	{
		hull_t original = as_original_hulls[h];
		if (!original.clipnodes)
			continue;
		int base = original.lastclipnode + 1, maxboxes = as_num_chunks;
		Mem_Free (as_nodes[h]);
		Mem_Free (as_planes[h]);
		as_nodes[h] = Mem_Alloc ((base + maxboxes * 6) * sizeof (mclipnode_t));
		as_planes[h] = Mem_Alloc ((as_model->numplanes + maxboxes * 6) * sizeof (mplane_t));
		memcpy (as_nodes[h], original.clipnodes, base * sizeof (mclipnode_t));
		memcpy (as_planes[h], original.planes, as_model->numplanes * sizeof (mplane_t));
		int nodecount = base, planecount = as_model->numplanes, root = CONTENTS_SOLID;
		for (int pidx = 0; pidx < as_num_panels; ++pidx)
		{
			as_panel_t *p = &as_panels[pidx];
			int			heights[AS_MAX_CHUNKS] = {0}, stack_height[AS_MAX_CHUNKS], stack_start[AS_MAX_CHUNKS];
			/* Enumerate overlapping maximal empty rectangles before Minkowski
			   erosion. Eroding a disjoint partition leaves invisible seam walls. */
			for (int y = 0; y < p->ny; ++y)
			{
				for (int x = 0; x < p->nx; ++x)
					heights[x] = as_poses[p->first + y * p->nx + x].detached ? heights[x] + 1 : 0;
				int stack_count = 0;
				for (int x = 0; x <= p->nx; ++x)
				{
					int current = x < p->nx ? heights[x] : 0, left = x;
					while (stack_count && stack_height[stack_count - 1] > current)
					{
						--stack_count;
						int ht = stack_height[stack_count];
						left = stack_start[stack_count];
						qboolean extends = y + 1 < p->ny;
						if (extends)
							for (int xx = left; xx < x; ++xx)
								if (!as_poses[p->first + (y + 1) * p->nx + xx].detached)
								{
									extends = false;
									break;
								}
						if (extends)
							continue;
						vec3_t lo, hi;
						VectorCopy (as_chunks[p->first + (y - ht + 1) * p->nx + left].mins, lo);
						VectorCopy (as_chunks[p->first + y * p->nx + x - 1].maxs, hi);
						for (int a = 0; a < 3; ++a)
							if (a == p->axis)
							{
								lo[a] -= original.clip_maxs[a] + 0.125f;
								hi[a] -= original.clip_mins[a] - 0.125f;
							}
							else
							{
								lo[a] -= original.clip_mins[a];
								hi[a] -= original.clip_maxs[a];
							}
						if (lo[p->u] >= hi[p->u] || lo[p->v] >= hi[p->v])
							continue;
						int first = nodecount;
						for (int k = 0; k < 6; ++k)
						{
							int		  a = k / 2;
							mplane_t *plane = &as_planes[h][planecount];
							memset (plane, 0, sizeof (*plane));
							plane->normal[a] = 1;
							plane->type = a;
							plane->dist = (k & 1) ? hi[a] : lo[a];
							mclipnode_t *node = &as_nodes[h][nodecount++];
							node->planenum = planecount++;
							int inside = (k == 5) ? CONTENTS_EMPTY : nodecount;
							node->children[k & 1] = inside;
							node->children[(k & 1) ^ 1] = root;
						}
						root = first;
					}
					if (current && (!stack_count || stack_height[stack_count - 1] < current))
					{
						stack_start[stack_count] = left;
						stack_height[stack_count++] = current;
					}
				}
			}
		}
		for (int n = 0; n < base; ++n)
			for (int k = 0; k < 2; ++k)
				if (as_nodes[h][n].children[k] == CONTENTS_SOLID)
					as_nodes[h][n].children[k] = root;
		as_model->hulls[h] = original;
		as_model->hulls[h].clipnodes = as_nodes[h];
		as_model->hulls[h].planes = as_planes[h];
		as_model->hulls[h].lastclipnode = nodecount - 1;
	}
}
static void AS_UpdateBrushCollisions (void)
{
	for (int i = 1; i < sv.qcvm.num_edicts && i < 8192; ++i)
	{
		edict_t *e = (edict_t *)((byte *)sv.qcvm.edicts + i * sv.qcvm.edict_size);
		int		 model = (int)e->v.modelindex;
		qboolean enabled = !e->free && e->v.solid == SOLID_BSP && model > 1 && model < MAX_MODELS && sv.models[model] && sv.models[model]->type == mod_brush;
		vec3_t	 axes[3];
		AngleVectors (e->v.angles, axes[0], axes[1], axes[2]);
		VectorInverse (axes[1]);
		if (!AS_PhysicsBrushPose (i, model, e->v.origin, axes[0], enabled) && enabled)
			Host_Error ("Aftershock could not synchronize brush entity %d", i);
	}
}
static void AS_CreateBrushCollisions (void)
{
	int meshes = 0;
	for (int model = 2; model < MAX_MODELS; ++model)
	{
		qmodel_t *m = sv.models[model];
		if (!m || m->type != mod_brush)
			continue;
		float *brushTriangles = NULL;
		size_t count = 0;
		for (int i = 0; i < m->nummodelsurfaces; ++i)
		{
			msurface_t *s = &m->surfaces[m->firstmodelsurface + i];
			if (!s->polys)
				continue;
			brushTriangles = Mem_Realloc (brushTriangles, (count + s->polys->numverts - 2) * 9 * sizeof (float));
			for (int j = 1; j < s->polys->numverts - 1; ++j)
			{
				memcpy (brushTriangles + count * 9, s->polys->verts[0], 12);
				memcpy (brushTriangles + count * 9 + 3, s->polys->verts[j], 12);
				memcpy (brushTriangles + count * 9 + 6, s->polys->verts[j + 1], 12);
				++count;
			}
		}
		if (count && !AS_PhysicsBrushMesh (model, brushTriangles, (uint32_t)count))
			Host_Error ("Aftershock could not cook brush model %s", m->name);
		Mem_Free (brushTriangles);
		if (count)
			++meshes;
	}
	AS_UpdateBrushCollisions ();
	Con_Printf ("Aftershock: %d brush collision meshes cooked for doors and platforms\n", meshes);
}
void AS_NewMap (void)
{
	AS_GoreClear ();
    AS_NailsClear();
    memset(as_fracture_born,0,sizeof(as_fracture_born));
	as_fx_count = as_fx_serial = as_launch_serial = 0;
	memset (as_launches, 0, sizeof (as_launches));
	/* The previous model may already have been freed by the engine hunk reset.
	   No old-model dereference is permitted here. */
	if (as_faces)
	{
		for (int i = 0; i < as_frames; ++i)
			Mem_Free (as_faces[i].samples);
		Mem_Free (as_faces);
		as_faces = NULL;
	}
	for (int h = 0; h < MAX_MAP_HULLS; ++h)
	{
		Mem_Free (as_nodes[h]);
		Mem_Free (as_planes[h]);
		as_nodes[h] = NULL;
		as_planes[h] = NULL;
	}
	Mem_Free (as_face_panel);
	as_model = cl.worldmodel;
	as_frames = as_model->numsurfaces;
	as_faces = Mem_Alloc (as_frames * sizeof (*as_faces));
	memset (as_faces, 0, as_frames * sizeof (*as_faces));
	as_face_panel = Mem_Alloc (as_frames * sizeof (int));
	for (int i = 0; i < as_frames; ++i)
		as_face_panel[i] = -1;
	as_num_panels = as_num_chunks = as_last_detached = as_event_count = as_tick = 0;
    as_show_start=-1;
	as_ready = false;
	for (int h = 0; h < MAX_MAP_HULLS; ++h)
		as_original_hulls[h] = as_model->hulls[h];
	as_source_hash = AS_StructureHash ();
	as_geometry_hash = AS_Hash (AS_GeometryHash (), &as_density.value, sizeof (float));
	size_t total = 0;
	if (!AS_ReadCache (&total))
	{
		for (int i = 0; i < as_model->numsurfaces; ++i)
		{
			as_faces[i].offset = total * 4;
			AS_ExtractSamples (&as_model->surfaces[i], &as_faces[i]);
			total += as_faces[i].count;
		}
		AS_WriteCache (total);
	}
	if (as_sample_buffer)
	{
		GL_WaitForDeviceIdle ();
		R_FreeBuffer (as_sample_buffer, &as_sample_memory, NULL);
		as_sample_buffer = VK_NULL_HANDLE;
	}
	if (total)
	{
		as_static_sample_bytes = total * 4;
		byte *packed = Mem_Alloc (total * 4);
		for (int i = 0; i < as_model->numsurfaces; ++i)
			memcpy (packed + as_faces[i].offset, as_faces[i].samples, as_faces[i].count * 4);
		R_CreateBuffer (
			&as_sample_buffer, &as_sample_memory, total * 4, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, NULL, NULL, "Aftershock 4-byte samples");
		R_StagingUploadBuffer (as_sample_buffer, total * 4, packed);
		Mem_Free (packed);
	}
	if (!as_grid_buffer)
	{
		uint32_t grid[289];
		for (int y = 0; y <= 16; ++y)
			for (int x = 0; x <= 16; ++x)
				grid[y * 17 + x] = (x * 8) | ((y * 8) << 16);
		R_CreateBuffer (
			&as_grid_buffer, &as_grid_memory, sizeof (grid), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, NULL, NULL, "Aftershock fragment grid");
		R_StagingUploadBuffer (as_grid_buffer, sizeof (grid), (const byte *)grid);
	}
	AS_BuildScatter ();
	if (as_worldmode.value)
	{
		if (!sv.active || svs.maxclients > 1 || cls.demoplayback)
			Host_Error ("Destruction mode requires local single-player. Use -worldmode faithful.");
		AS_ExtractPanels ();
		float *worldTriangles = NULL;
		size_t count = 0;
		for (int i = 0; i < as_model->nummodelsurfaces; ++i)
		{
			msurface_t *s = &as_model->surfaces[i];
			if (as_face_panel[i] >= 0 || !s->polys || (s->flags & (SURF_DRAWSKY | SURF_DRAWTURB)))
				continue;
			int n = s->polys->numverts - 2;
			worldTriangles = Mem_Realloc (worldTriangles, (count + n) * 9 * sizeof (float));
			for (int j = 1; j < s->polys->numverts - 1; ++j)
			{
				memcpy (worldTriangles + count * 9, s->polys->verts[0], 12);
				memcpy (worldTriangles + count * 9 + 3, s->polys->verts[j], 12);
				memcpy (worldTriangles + count * 9 + 6, s->polys->verts[j + 1], 12);
				++count;
			}
		}
		if (!AS_PhysicsInit (AS_Log) || !AS_PhysicsBuild (as_chunks, as_num_chunks, worldTriangles, (uint32_t)count))
			Host_Error ("Aftershock physics initialization failed");
		Mem_Free (worldTriangles);
		memcpy (as_poses, AS_PhysicsPoses (), as_num_chunks * sizeof (*as_poses));
		as_ready = true;
		if (!as_novis_override)
		{
			as_saved_novis = Cvar_VariableValue ("r_novis");
			as_novis_override = true;
		}
		Cvar_Set ("r_novis", "1");
		as_map_hash = UINT64_C (1469598103934665603);
		const byte *hashbytes = (const byte *)as_chunks;
		for (size_t k = 0; k < as_num_chunks * sizeof (*as_chunks); ++k)
		{
			as_map_hash ^= hashbytes[k];
			as_map_hash *= UINT64_C (1099511628211);
		}
		for (const char *n = as_model->name; *n; ++n)
		{
			as_map_hash ^= (byte)*n;
			as_map_hash *= UINT64_C (1099511628211);
		}
		/* Geometry, UVs, and extractor schema identify the source map as well. */
		as_map_hash = AS_Hash (as_map_hash, &as_source_hash, sizeof (as_source_hash));
		if (as_pending_load[0])
		{
			char requested[MAX_OSPATH];
			q_strlcpy (requested, as_pending_load, sizeof (requested));
			as_pending_load[0] = 0;
			if (!AS_PhysicsLoad (requested, as_map_hash))
				Host_Error ("Aftershock save is missing or incompatible: %s", requested);
			memcpy (as_poses, AS_PhysicsPoses (), as_num_chunks * sizeof (*as_poses));
			AS_RebuildHulls ();
			as_last_detached = AS_PhysicsStats ().detached;
			Con_Printf ("Aftershock restored: detached=%d bodies=%u\n", as_last_detached, AS_PhysicsStats ().bodies);
		}
		AS_CreateBrushCollisions ();
	}
	Con_Printf ("Aftershock: %zu splats, %d closed wall panels, %d structural chunks\n", total, as_num_panels, as_num_chunks);
	if (COM_CheckParm ("-test-scenario"))
		AS_Inspect_f ();
}
qboolean AS_SurfaceReplaced (const msurface_t *s)
{
	return as_ready && s >= as_model->surfaces && s < as_model->surfaces + as_model->nummodelsurfaces && as_face_panel[s - as_model->surfaces] >= 0;
}
static void AS_DrawSurface (cb_context_t *cbx, msurface_t *s, int frame, float detail)
{
	as_face_t *f = &as_faces[s - as_model->surfaces];
	if (!f->count || AS_SurfaceReplaced (s))
		return;
	texture_t *t = R_TextureAnimation (s->texinfo->texture, frame);
	int		   full = t->fullbright != NULL;
    qboolean reactive=HP_SurfaceActive(f);
	R_BindPipeline (cbx, VK_PIPELINE_BIND_POINT_GRAPHICS, reactive?vulkan_globals.holo_splat[cbx->pipeline_variant][full]:vulkan_globals.aftershock_splat[cbx->pipeline_variant][full]);
	VkDescriptorSet sets[3] = {
		t->gltexture->descriptor_set, lightmaps[s->lightmaptexturenum].texture->descriptor_set,
		full ? t->fullbright->descriptor_set : t->gltexture->descriptor_set};
	vulkan_globals.vk_cmd_bind_descriptor_sets (cbx->cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan_globals.world_pipeline_layout.handle, 0, 3, sets, 0, NULL);
	VkBuffer		  vb, ib;
	VkDeviceSize	  vo, io;
	as_surface_gpu_t *metadata = (as_surface_gpu_t *)R_VertexAllocate (sizeof (as_surface_gpu_t), &vb, &vo);
	*metadata = f->gpu;
	metadata->saxis[3] = AS_Neon () ? 14 : AS_StructureMode () ? 0 : as_style.value;
	metadata->taxis[3] = as_reduced_flashes.value ? 0 : (float)cl.time;
	uint16_t	  *indices = (uint16_t *)R_IndexAllocate (12, &ib, &io);
	const uint16_t quad[6] = {0, 1, 2, 0, 2, 3};
	memcpy (indices, quad, sizeof (quad));
	VkBuffer	 buffers[2] = {as_sample_buffer, vb};
	VkDeviceSize offsets[2] = {f->offset, vo};
	vulkan_globals.vk_cmd_bind_vertex_buffers (cbx->cb, 0, 2, buffers, offsets);
	vulkan_globals.vk_cmd_bind_index_buffer (cbx->cb, ib, io, VK_INDEX_TYPE_UINT16);
	if(reactive) HP_Bind(cbx,as_sample_buffer,f->offset);
	vulkan_globals.vk_cmd_draw_indexed (cbx->cb, 6, f->count, 0, 0, 0);
	vec3_t normal;
	VectorScale (s->plane->normal, (s->flags & SURF_PLANEBACK) ? -1 : 1, normal);
	if (AS_Neon ())
	{
		size_t neon_offsets[4] = {f->offset, f->offset, f->offset, f->offset};
		int	   neon_counts[4] = {f->count, f->count, f->count, f->count};
		AS_DrawStructure (cbx, metadata, normal, neon_offsets, neon_counts, full, 1, 0, as_sample_buffer, 0, reactive);
		if (as_fidelity.value ? f->architectural_count>0 : (f->edge_count > 0 && f->edge_count <= f->count))
		{
			neon_offsets[0] = f->offset + (f->count - f->edge_count) * 4;
			neon_counts[0] = as_fidelity.value ? f->architectural_count : f->edge_count;
            if(as_fidelity.value) neon_offsets[0]=f->architectural_offset;
			AS_DrawStructure (cbx, metadata, normal, neon_offsets, neon_counts, full, -1, 0, as_fidelity.value?as_scatter_buffer:as_sample_buffer, 0, reactive);
		}
	}
	else
		AS_DrawStructure (cbx, metadata, normal, f->scatter_offset, f->scatter_count, full, 2.7f, detail, as_scatter_buffer, 0, reactive);
}
void AS_DrawWorld (cb_context_t *cbx, int index)
{
	if (index != 0 || !as_model)
		return;
	for (int i = 0; i < as_model->nummodelsurfaces; ++i)
		if (((uint32_t *)as_model->surfvis)[i / 32] & (1u << (i % 32)))
		{
			as_face_t *f = &as_faces[i];
			if (!f->count || R_CullBox (f->bounds_min, f->bounds_max))
				continue;
			float distance2 = 0;
			for (int a = 0; a < 3; ++a)
			{
				float d = q_max (f->bounds_min[a] - r_origin[a], q_max (0, r_origin[a] - f->bounds_max[a]));
				distance2 += d * d;
			}
			float detail = as_fidelity.value ? CLAMP(0, log2f(q_max(1,sqrtf(distance2)/192)), 3) : distance2 > 768 * 768 ? 3 : distance2 > 384 * 384 ? 2 : distance2 > 192 * 192 ? 1 : 0;
			AS_DrawSurface (cbx, &as_model->surfaces[i], 0, detail);
		}
	AS_DrawChunks (cbx);
    HP_DrawGore(cbx);
}
qboolean AS_DrawBrush (cb_context_t *cbx, qmodel_t *model, entity_t *ent, texchain_t chain)
{
	if (!as_model || model == as_model || model->surfaces != as_model->surfaces)
		return false;
	for (int i = 0; i < model->texofs[TEXTYPE_SKY]; ++i)
	{
		texture_t *t = model->textures[model->usedtextures[i]];
		if (!t)
			continue;
		for (msurface_t *s = t->texturechains[chain]; s; s = s->texturechains[chain])
			if (!(s->flags & SURF_DRAWTILED))
				AS_DrawSurface (cbx, s, ent ? ent->frame : 0, 0);
	}
	return true;
}
static void AS_Transform (const as_chunk_pose *pose, const vec3_t local, vec3_t out)
{
	vec3_t q = {pose->rotation[0], pose->rotation[1], pose->rotation[2]}, t, u;
	CrossProduct (q, local, t);
	VectorScale (t, 2, t);
	CrossProduct (q, t, u);
	for (int a = 0; a < 3; ++a)
		out[a] = pose->center[a] + local[a] + pose->rotation[3] * t[a] + u[a];
}
static void AS_DrawChunkSplats (cb_context_t *cbx, int id)
{
	as_chunk_desc *c = &as_chunks[id];
	as_chunk_pose *pose = &as_poses[id];
	as_panel_t	  *panel = &as_panels[as_face_panel[c->surface]];
	for (int face = 0; face < 6; ++face)
	{
		int	   axis = face / 2, u = (axis + 1) % 3, w = (axis + 2) % 3;
		vec3_t face_normal = {0, 0, 0}, world_normal, to_view;
		face_normal[axis] = (face & 1) ? 1 : -1;
		AS_Transform (pose, face_normal, world_normal);
		VectorSubtract (world_normal, pose->center, world_normal);
		VectorSubtract (r_origin, pose->center, to_view);
		if (DotProduct (world_normal, to_view) < (c->maxs[axis] - c->mins[axis]) * .5f - .05f)
			continue;

		msurface_t *s = &as_model->surfaces[c->surface];
		if (axis == panel->axis)
		{
			msurface_t *other = &as_model->surfaces[panel->b];
			float		coord = (face & 1) ? c->maxs[axis] : c->mins[axis];
			if (fabsf (other->plane->dist - coord) < 0.1f)
				s = other;
		}
		texture_t *t = R_TextureAnimation (s->texinfo->texture, 0);
		int		   full = t->fullbright != NULL;
		R_BindPipeline (cbx, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan_globals.aftershock_splat[cbx->pipeline_variant][full]);
		VkDescriptorSet sets[3] = {
			t->gltexture->descriptor_set, greylightmap->descriptor_set, full ? t->fullbright->descriptor_set : t->gltexture->descriptor_set};
		vulkan_globals.vk_cmd_bind_descriptor_sets (cbx->cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan_globals.world_pipeline_layout.handle, 0, 3, sets, 0, NULL);
		VkBuffer		  vb, ib;
		VkDeviceSize	  vo, io;
		as_surface_gpu_t *m = (as_surface_gpu_t *)R_VertexAllocate (sizeof (*m), &vb, &vo);
		memset (m, 0, sizeof (*m));
		vec3_t local, original;
		for (int a = 0; a < 3; ++a)
			local[a] = -(c->maxs[a] - c->mins[a]) * 0.5f;
		local[axis] *= (face & 1) ? -1 : 1;
		for (int a = 0; a < 3; ++a)
			original[a] = local[a] + (c->mins[a] + c->maxs[a]) * 0.5f;
		AS_Transform (pose, local, m->origin);
		m->origin[3] = 8 * 0.58f;
		local[u] += (c->maxs[u] - c->mins[u]) / 128;
		AS_Transform (pose, local, m->saxis);
		local[u] -= (c->maxs[u] - c->mins[u]) / 128;
		local[w] += (c->maxs[w] - c->mins[w]) / 128;
		AS_Transform (pose, local, m->taxis);
		for (int a = 0; a < 3; ++a)
		{
			m->saxis[a] -= m->origin[a];
			m->taxis[a] -= m->origin[a];
		}
		m->saxis[3] = AS_Neon () ? 14 : AS_StructureMode () ? 0 : as_style.value;
		m->taxis[3] = as_reduced_flashes.value ? 0 : (float)cl.time;
		m->tex[0] = (DotProduct (original, s->texinfo->vecs[0]) + s->texinfo->vecs[0][3]) / t->width;
		m->tex[1] = (DotProduct (original, s->texinfo->vecs[1]) + s->texinfo->vecs[1][3]) / t->height;
		m->tex[2] = s->texinfo->vecs[0][u] * (c->maxs[u] - c->mins[u]) / 128 / t->width;
		m->tex[3] = s->texinfo->vecs[1][w] * (c->maxs[w] - c->mins[w]) / 128 / t->height;
		m->params[0] = s->texinfo->vecs[0][w] * (c->maxs[w] - c->mins[w]) / 128 / t->width;
		m->params[1] = s->texinfo->vecs[1][u] * (c->maxs[u] - c->mins[u]) / 128 / t->height;
		m->params[2] = face == 5 ? 0.85f : face == 4 ? 0.35f : 0.62f;
		m->params[3] = 1;
		if (axis != panel->axis)
		{
			m->tex[0] = original[u] / t->width;
			m->tex[1] = original[w] / t->height;
			m->tex[2] = (c->maxs[u] - c->mins[u]) / 128 / t->width;
			m->tex[3] = (c->maxs[w] - c->mins[w]) / 128 / t->height;
			m->params[0] = m->params[1] = 0;
		}
		uint16_t	  *indices = (uint16_t *)R_IndexAllocate (12, &ib, &io);
		const uint16_t quad[6] = {0, 1, 2, 0, 2, 3};
		memcpy (indices, quad, sizeof (quad));
		VkBuffer	 buffers[2] = {as_grid_buffer, vb};
		VkDeviceSize offsets[2] = {0, vo};
		vulkan_globals.vk_cmd_bind_vertex_buffers (cbx->cb, 0, 2, buffers, offsets);
		vulkan_globals.vk_cmd_bind_index_buffer (cbx->cb, ib, io, VK_INDEX_TYPE_UINT16);
		vulkan_globals.vk_cmd_draw_indexed (cbx->cb, 6, 289, 0, 0, 0);
		vec3_t normal;
		CrossProduct (m->saxis, m->taxis, normal);
		VectorNormalize (normal);
		if (!(face & 1))
			VectorInverse (normal);
		size_t scatter_offsets[4];
		int	   counts[4] = {64, 64, 64, 64};
		for (int layer = 0; layer < 4; ++layer)
			scatter_offsets[layer] = as_scatter_grid_offset + layer * 64 * 4;
		if (AS_Neon ())
		{
			size_t neon_offsets[4] = {0, 0, 0, 0};
			int	   neon_counts[4] = {289, 289, 289, 289};
            float heat=(axis!=panel->axis && as_fracture_born[id]>0)?CLAMP(0,1-(cl.time-as_fracture_born[id])/3.,1):0;
			AS_DrawStructure (cbx, m, normal, neon_offsets, neon_counts, full, 1, 0, as_grid_buffer, heat, false);
		}
		else
			AS_DrawStructure (cbx, m, normal, scatter_offsets, counts, full, 1.8f, 0, as_scatter_buffer, 0, false);
	}
}
void AS_DrawChunks (cb_context_t *cbx)
{
	if (!as_ready)
		return;
	static const int corner[6][4] = {{0, 4, 6, 2}, {1, 3, 7, 5}, {0, 1, 5, 4}, {2, 6, 7, 3}, {0, 2, 3, 1}, {4, 5, 7, 6}};
	for (int i = 0; i < as_num_chunks; ++i)
	{
		vec3_t bounds_min, bounds_max;
		float  radius = 0;
		for (int a = 0; a < 3; ++a)
		{
			float d = (as_chunks[i].maxs[a] - as_chunks[i].mins[a]) * .5f;
			radius += d * d;
		}
		radius = sqrtf (radius) + 20;
		for (int a = 0; a < 3; ++a)
		{
			bounds_min[a] = as_poses[i].center[a] - radius;
			bounds_max[a] = as_poses[i].center[a] + radius;
		}
		if (R_CullBox (bounds_min, bounds_max))
			continue;
		if (as_renderer.value)
		{
			AS_DrawChunkSplats (cbx, i);
			continue;
		}
		as_chunk_desc *c = &as_chunks[i];
		as_chunk_pose *pose = &as_poses[i];
		msurface_t	  *surface = &as_model->surfaces[c->surface];
		texture_t	  *t = R_TextureAnimation (surface->texinfo->texture, 0);
		as_panel_t	  *panel = &as_panels[as_face_panel[c->surface]];
		texture_t	  *textures[6];
		msurface_t	  *sources[6];
		for (int face = 0; face < 6; ++face)
		{
			sources[face] = surface;
			int axis = face / 2;
			if (axis == panel->axis)
			{
				msurface_t *other = &as_model->surfaces[panel->b];
				float		coord = (face & 1) ? c->maxs[axis] : c->mins[axis];
				if (fabsf (other->polys->verts[0][axis] - coord) < .1f)
					sources[face] = other;
			}
			textures[face] = R_TextureAnimation (sources[face]->texinfo->texture, 0);
		}
		R_BindPipeline (cbx, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan_globals.aftershock_solid[cbx->pipeline_variant]);
		vulkan_globals.vk_cmd_bind_descriptor_sets (
			cbx->cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan_globals.basic_pipeline_layout.handle, 0, 1, &t->gltexture->descriptor_set, 0, NULL);
		VkBuffer	   vb, ib;
		VkDeviceSize   vo, io;
		basicvertex_t *v = (basicvertex_t *)R_VertexAllocate (24 * sizeof (*v), &vb, &vo);
		uint16_t	  *idx = (uint16_t *)R_IndexAllocate (36 * sizeof (*idx), &ib, &io);
		for (int face = 0; face < 6; ++face)
			for (int k = 0; k < 4; ++k)
			{
				int	   n = face * 4 + k, code = corner[face][k];
				vec3_t local;
				for (int a = 0; a < 3; ++a)
					local[a] = ((code >> a) & 1 ? 0.5f : -0.5f) * (c->maxs[a] - c->mins[a]);
				AS_Transform (pose, local, v[n].position);
				int axis = face / 2, u = (axis + 1) % 3, w = (axis + 2) % 3;
				v[n].texcoord[0] = (local[u] + (c->maxs[u] + c->mins[u]) * 0.5f) / t->width;
				v[n].texcoord[1] = (local[w] + (c->maxs[w] + c->mins[w]) * 0.5f) / t->height;
				if (axis == panel->axis)
				{
					vec3_t original;
					for (int a = 0; a < 3; ++a)
						original[a] = local[a] + (c->maxs[a] + c->mins[a]) * .5f;
					v[n].texcoord[0] = (DotProduct (original, sources[face]->texinfo->vecs[0]) + sources[face]->texinfo->vecs[0][3]) / textures[face]->width;
					v[n].texcoord[1] = (DotProduct (original, sources[face]->texinfo->vecs[1]) + sources[face]->texinfo->vecs[1][3]) / textures[face]->height;
				}
				int shade = face == 5 ? 220 : face == 4 ? 90 : 160;
				v[n].color[0] = v[n].color[1] = v[n].color[2] = (byte)shade;
				v[n].color[3] = 255;
				if (k == 0)
				{
					int o = face * 6;
					idx[o] = n;
					idx[o + 1] = n + 1;
					idx[o + 2] = n + 2;
					idx[o + 3] = n;
					idx[o + 4] = n + 2;
					idx[o + 5] = n + 3;
				}
			}
		vulkan_globals.vk_cmd_bind_vertex_buffers (cbx->cb, 0, 1, &vb, &vo);
		vulkan_globals.vk_cmd_bind_index_buffer (cbx->cb, ib, io, VK_INDEX_TYPE_UINT16);
		for (int face = 0; face < 6; ++face)
		{
			vulkan_globals.vk_cmd_bind_descriptor_sets (
				cbx->cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan_globals.basic_pipeline_layout.handle, 0, 1, &textures[face]->gltexture->descriptor_set, 0,
				NULL);
			vulkan_globals.vk_cmd_draw_indexed (cbx->cb, 6, 1, face * 6, 0, 0);
		}
	}
}
static void AS_Queue (const vec3_t p)
{
	if (as_ready && as_event_count < 64)
	{
		VectorCopy (p, as_events[as_event_count].origin);
		as_events[as_event_count].radius = as_radius.value;
		as_events[as_event_count].damage = as_damage.value;
		as_events[as_event_count].seed = as_tick * 64 + as_event_count;
		++as_event_count;
	}
}
void AS_WriteByte (sizebuf_t *dest, int value)
{
	if (as_wire.state == 1 && as_wire.dest == dest)
	{
		as_wire.state = (value == TE_EXPLOSION || value == TE_EXPLOSION2) ? 2 : 0;
		as_wire.coords = 0;
		return;
	}
	if (value == svc_temp_entity)
	{
		as_wire.dest = dest;
		as_wire.state = 1;
		as_wire.coords = 0;
	}
	else
		as_wire.state = 0;
}
void AS_WriteCoord (sizebuf_t *dest, float value)
{
	if (as_wire.state == 2 && as_wire.dest == dest)
	{
		as_wire.p[as_wire.coords++] = value;
		if (as_wire.coords == 3)
		{
			AS_Queue (as_wire.p);
			// Copy projectile momentum while QuakeC still owns the projectile.
			// Cosmetics never write edicts or consume the gameplay RNG.
			if (qcvm == &sv.qcvm && pr_global_struct->self)
			{
				edict_t *projectile = PROG_TO_EDICT (pr_global_struct->self);
				int		 n = as_launch_serial++ % 32;
				VectorCopy (as_wire.p, as_launches[n].origin);
				VectorCopy (projectile->v.velocity, as_launches[n].direction);
				VectorNormalize (as_launches[n].direction);
				as_launches[n].born = realtime;
				as_launches[n].used = false;
			}
			as_wire.state = 0;
		}
	}
}
void AS_ServerStep (double dt)
{
	++as_tick;
	if (as_checksum_tick == as_tick)
	{
		uint64_t hash = UINT64_C (1469598103934665603);
		uint64_t rng = COM_RandState ();
		hash = AS_Hash (hash, &rng, sizeof (rng));
		/* Explicit fields only: no addresses, renderer state, or C padding. */
		for (int i = 0; i < qcvm->num_edicts; ++i)
		{
			edict_t *e = EDICT_NUM (i);
			if (e->free)
				continue;
			float values[] = {(float)i,			e->v.origin[0],	   e->v.origin[1],	e->v.origin[2], e->v.velocity[0], e->v.velocity[1],
							  e->v.velocity[2], e->v.angles[0],	   e->v.angles[1],	e->v.angles[2], e->v.health,	  e->v.ammo_shells,
							  e->v.ammo_nails,	e->v.ammo_rockets, e->v.ammo_cells, e->v.weapon,	e->v.items,		  e->v.flags,
							  e->v.movetype,	e->v.solid,		   e->v.frame,		e->v.nextthink, e->v.think};
			for (size_t j = 0; j < sizeof (values) / sizeof (float); ++j)
			{
				int32_t q = (int32_t)roundf (values[j] * 1024);
				for (int k = 0; k < 4; ++k)
				{
					hash ^= ((uint32_t)q >> (k * 8)) & 255;
					hash *= UINT64_C (1099511628211);
				}
			}
		}
		Con_Printf ("Aftershock normalized gameplay tick=%d hash=%016llx entities=%d\n", as_tick, (unsigned long long)hash, qcvm->num_edicts);
	}
    if (COM_CheckParm("-test-fidelity-cycle") && as_tick>=24 && as_tick<=120 && as_tick%24==0) {
        int phase=as_tick/24;
        Cvar_SetValueQuick(&as_fidelity,phase==2?0:1);
        Cvar_SetValueQuick(&as_reflections,phase%2);
        Con_Printf("Aftershock fidelity toggle: fidelity=%g reflections=%g detached=%d tick=%d\n",as_fidelity.value,as_reflections.value,as_last_detached,as_tick);
    }
    if(COM_CheckParm("-test-fidelity-restart") && as_tick==72){
        Cbuf_AddText("vid_restart\n");
        Con_Printf("Aftershock fidelity video restart requested\n");
    }
	if (COM_CheckParm ("-test-structure-cycle") && as_tick >= 12 && as_tick <= 132 && as_tick % 12 == 0)
	{
		int mode = as_tick / 12 - 1;
		Cvar_SetValueQuick (&as_structure, (float)mode);
		Cvar_SetValueQuick (&as_layers, (float)(1 + mode % 4));
		Con_Printf ("Aftershock live structure: mode=%d layers=%d detached=%d (no rebuild)\n", mode, 1 + mode % 4, as_last_detached);
	}
	if (!as_ready)
		return;
	if (as_test_explosion_tick > 0 && as_tick >= as_test_explosion_tick && as_num_panels)
	{
		as_panel_t *p = &as_panels[CLAMP (0, as_test_panel, as_num_panels - 1)];
		vec3_t		org;
		for (int a = 0; a < 3; ++a)
			org[a] = (p->mins[a] + p->maxs[a]) * 0.5f;
		org[p->axis] = p->mins[p->axis] - 2;
        if(COM_CheckParm("-test-smoke-floor")){
            vec3_t end;VectorCopy(org,end);end[2]-=512;
            trace_t floorTrace;memset(&floorTrace,0,sizeof(floorTrace));floorTrace.fraction=1;floorTrace.allsolid=true;
            SV_RecursiveHullCheck(&cl.worldmodel->hulls[0],org,end,&floorTrace,~(1u << -CONTENTS_EMPTY));
            if(floorTrace.fraction<1 && !floorTrace.startsolid)org[2]-=512*floorTrace.fraction-2;
            Con_Printf("Aftershock floor smoke fixture: %g %g %g\n",org[0],org[1],org[2]);
        }
		AS_Queue (org);
		AS_VisualExplosion (org);
		as_test_explosion_tick = 0;
		Con_Printf ("Aftershock synthetic explosion: %g %g %g\n", org[0], org[1], org[2]);
	}
    if(COM_CheckParm("-test-progressive") && as_tick==30)Cbuf_AddText("as_inspect\n");
    if(COM_CheckParm("-test-progressive") && as_num_panels && as_tick>=45 && as_tick<=450 && as_tick%45==0){
      as_panel_t *p=&as_panels[CLAMP(0,as_test_panel,as_num_panels-1)];vec3_t org;
      for(int a=0;a<3;++a)org[a]=(p->mins[a]+p->maxs[a])*.5f;
      org[p->axis]=p->mins[p->axis]-2;AS_Queue(org);AS_VisualExplosion(org);
    }
	AS_UpdateBrushCollisions ();
	for (int i = 0; i < as_event_count; ++i)
		if(as_progressive.value)
            AS_PhysicsExplodeProgressive(as_events[i].origin,q_min(as_events[i].radius,CLAMP(24,as_chip_radius.value,160)),as_events[i].damage,as_events[i].seed,as_rocket_hits.value);
        else AS_PhysicsExplode (as_events[i].origin, as_events[i].radius, as_events[i].damage, as_events[i].seed);
    if(COM_CheckParm("-test-progressive") && as_event_count)Con_Printf("Progressive hit: tick=%d detached=%u total=%d\n",as_tick,AS_PhysicsStats().detached,as_num_chunks);
	as_event_count = 0;
	AS_PhysicsStep (dt);
	if (COM_CheckParm ("-test-rocket") && !as_test_shot_through && as_num_panels && as_tick > 30)
	{
		as_panel_t *p = &as_panels[CLAMP (0, as_test_panel, as_num_panels - 1)];
		for (int i = 2; i < qcvm->num_edicts; ++i)
		{
			edict_t *e = EDICT_NUM (i);
			if (e->free || e->v.movetype != MOVETYPE_FLYMISSILE || e->v.owner != EDICT_TO_PROG (EDICT_NUM (1)))
				continue;
			if (e->v.origin[p->axis] > p->maxs[p->axis] + 16 && e->v.origin[p->u] >= p->mins[p->u] && e->v.origin[p->u] <= p->maxs[p->u] &&
				e->v.origin[p->v] >= p->mins[p->v] && e->v.origin[p->v] <= p->maxs[p->v])
			{
				as_test_shot_through = true;
				Con_Printf ("Aftershock rocket traversal PASS tick=%d entity=%d\n", as_tick, i);
				break;
			}
		}
	}
	as_physics_total += AS_PhysicsStats ().simulation_ms;
	++as_physics_steps;
	as_impact impacts[64];
	uint32_t  impact_count = AS_PhysicsTakeImpacts (impacts, 64);
	if (as_effects.value && impact_count && as_tick % 4 == 0)
	{
		AS_VisualExplosionImpl (impacts[0].origin, false);
		as_fx[(as_fx_serial - 1) % 32].strength = 0.22f;
	}
    for(int i=0;i<as_num_chunks;++i)
        if(!as_poses[i].detached && AS_PhysicsPoses()[i].detached) as_fracture_born[i]=cl.time;
	memcpy (as_poses, AS_PhysicsPoses (), as_num_chunks * sizeof (*as_poses));
	int detached = AS_PhysicsStats ().detached;
	if (detached != as_last_detached)
	{
		AS_RebuildHulls ();
		as_last_detached = detached;
		Con_Printf ("Aftershock: %d detached chunks; collision breach updated\n", detached);
	}
	if (as_tick == as_save_tick)
	{
		as_save_tick = 0;
		Cbuf_AddText ("save aftershock-checkpoint\n");
	}
	if (as_tick == as_load_tick)
	{
		as_load_tick = 0;
		Cbuf_AddText ("load aftershock-checkpoint\n");
	}
	if (as_tick == as_switch_tick)
	{
		as_switch_tick = 0;
		Cvar_SetValueQuick (&as_renderer, as_renderer.value ? 0 : 1);
		Con_Printf ("Aftershock renderer switched: renderer=%g detached=%u (no rebuild)\n", as_renderer.value, AS_PhysicsStats ().detached);
	}
	if (COM_CheckParm ("-test-scenario") && as_num_panels && (as_tick == 10 || as_tick == 100))
	{
		as_panel_t *p = &as_panels[CLAMP (0, as_test_panel, as_num_panels - 1)];
		vec3_t		start, end;
		for (int a = 0; a < 3; ++a)
			start[a] = end[a] = (p->mins[a] + p->maxs[a]) * 0.5f;
		start[p->axis] = p->mins[p->axis] - 40;
		end[p->axis] = p->maxs[p->axis] + 40;
		trace_t check;
		memset (&check, 0, sizeof (check));
		check.allsolid = true;
		check.fraction = 1;
		SV_RecursiveHullCheck (&as_model->hulls[1], start, end, &check, ~(1u << -CONTENTS_EMPTY));
		Con_Printf (
			"Aftershock player-hull probe tick=%d fraction=%.6f startsolid=%d allsolid=%d\n", as_tick, check.fraction, check.startsolid, check.allsolid);
	}
}
/* Join the previous render before publishing the next structural snapshot.
   The render jobs read only as_poses until the next simulation boundary. */
void AS_BeforeServer (void)
{
	if (as_ready)
		GL_SynchronizeEndRenderingTask ();
}
void AS_TestInput (void)
{
    if(AS_ShowcaseInput())return;
    if(COM_CheckParm("-test-nails") && cls.signon==SIGNONS) {
        edict_t *player=EDICT_NUM(1);
        if(as_tick==10) {
            player->v.items=(int)player->v.items|IT_NAILGUN|IT_SUPER_NAILGUN;
            player->v.ammo_nails=200; player->v.impulse=4;
            player->v.flags=(int)player->v.flags|FL_GODMODE;
            player->v.v_angle[0]=0; player->v.v_angle[1]=90; player->v.fixangle=1;
        }
        if(as_tick==100) player->v.impulse=5;
        player->v.button0=(as_tick>=30 && as_tick<80) || (as_tick>=115 && as_tick<170);
        return;
    }

	if (!as_ready || !COM_CheckParm ("-test-rocket") || !as_num_panels || cls.signon != SIGNONS)
		return;
	edict_t	   *player = EDICT_NUM (1);
	as_panel_t *p = &as_panels[CLAMP (0, as_test_panel, as_num_panels - 1)];
	if (as_tick == 10)
	{
		for (int a = 0; a < 3; ++a)
			player->v.origin[a] = (p->mins[a] + p->maxs[a]) * 0.5f;
		player->v.origin[p->axis] = p->mins[p->axis] - 140;
		player->v.origin[2] = p->mins[2] + 32;
		memset (player->v.velocity, 0, sizeof (vec3_t));
		memset (player->v.v_angle, 0, sizeof (vec3_t));
		memset (player->v.angles, 0, sizeof (vec3_t));
		player->v.v_angle[1] = player->v.angles[1] = p->axis == 1 ? 90 : 0;
		player->v.fixangle = 1;
		player->v.movetype = MOVETYPE_WALK;
		player->v.flags = (int)player->v.flags | FL_GODMODE;
		player->v.items = (int)player->v.items | IT_ROCKET_LAUNCHER;
		player->v.ammo_rockets = 100;
		player->v.impulse = 7;
		SV_LinkEdict (player, false);
		as_test_crossed = false;
		as_test_shot_through = false;
		Con_Printf ("Aftershock real-weapon fixture: player positioned, WALK movement, rocket launcher\n");
	}
	/* A final elevated shot crosses the opening above the settled rubble. */
	if (COM_CheckParm ("-test-projectile") && as_tick == 130)
	{
		player->v.v_angle[0] = player->v.angles[0] = -20;
		player->v.fixangle = 1;
	}
	if (COM_CheckParm ("-test-projectile") && as_tick == 180)
	{
		player->v.v_angle[0] = player->v.angles[0] = 0;
		player->v.fixangle = 1;
	}
	if ((as_tick >= 30 && as_tick < 120) || (COM_CheckParm ("-test-projectile") && as_tick >= 150 && as_tick < 165))
		player->v.button0 = 1;
	else
		player->v.button0 = 0;
	if (COM_CheckParm ("-test-jump"))
		player->v.button2 = as_tick >= 200 && as_tick < 210;
	if (as_tick >= 180 && as_tick < 180 + as_walk_ticks)
	{
		player->v.velocity[p->axis] = 200;
		player->v.velocity[p->u] = 0;
	}
	if (!as_test_crossed && player->v.origin[p->axis] > p->maxs[p->axis] + 16)
	{
		as_test_crossed = true;
		Con_Printf ("Aftershock WALK traversal PASS tick=%d origin=%g %g %g\n", as_tick, player->v.origin[0], player->v.origin[1], player->v.origin[2]);
	}
	if (as_tick == 190 + as_walk_ticks)
	{
		vec3_t target;
		VectorCopy (player->v.origin, target);
		target[p->axis] = p->maxs[p->axis] + 48;
		trace_t probe;
		memset (&probe, 0, sizeof (probe));
		probe.fraction = 1;
		probe.allsolid = true;
		SV_RecursiveHullCheck (&as_model->hulls[1], player->v.origin, target, &probe, ~(1u << -CONTENTS_EMPTY));
		float  debris_fraction = 1;
		vec3_t debris_normal;
		AS_PhysicsSweep (player->v.origin, target, player->v.mins, player->v.maxs, &debris_fraction, debris_normal);
		Con_Printf (
			"Aftershock traversal probe: origin=%g %g %g BSP=%.6f rubble=%.6f startsolid=%d\n", player->v.origin[0], player->v.origin[1], player->v.origin[2],
			probe.fraction, debris_fraction, probe.startsolid);
		Con_Printf (
			"Aftershock real-weapon result: events=%u detached=%u walk=%s\n", AS_PhysicsStats ().events, AS_PhysicsStats ().detached,
			as_test_crossed ? "PASS" : "FAIL");
		AS_Inspect_f ();
	}
}
void AS_PushDebris (edict_t *player)
{
	if (as_ready)
		AS_PhysicsPush (player->v.origin, player->v.velocity, player->v.mins, player->v.maxs, host_frametime);
}
void AS_ClipDebris (trace_t *trace, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs)
{
	if (!as_ready)
		return;
	float  fraction;
	vec3_t normal;
	if (AS_PhysicsSweep (start, end, mins, maxs, &fraction, normal) && fraction < trace->fraction)
	{
		trace->fraction = fraction;
		for (int a = 0; a < 3; ++a)
			trace->endpos[a] = start[a] + fraction * (end[a] - start[a]);
		VectorCopy (normal, trace->plane.normal);
		trace->plane.dist = DotProduct (normal, trace->endpos);
		trace->ent = qcvm->edicts;
	}
}
static void AS_Explode_f (void)
{
	vec3_t p;
	if (Cmd_Argc () == 4)
	{
		for (int a = 0; a < 3; ++a)
			p[a] = (float)atof (Cmd_Argv (a + 1));
	}
	else
	{
		vec3_t forward;
		AngleVectors (cl.viewangles, forward, NULL, NULL);
		VectorMA (r_refdef.vieworg, 96, forward, p);
	}
	AS_Queue (p);
}
static void AS_Inspect_f (void)
{
	as_stats s = AS_PhysicsStats ();
	Con_Printf (
		"Aftershock: panels=%d chunks=%u detached=%u bodies=%u awake=%u physics=%.3fms errors=%u\n", as_num_panels, s.chunks, s.detached, s.bodies, s.awake,
		s.simulation_ms, s.errors);
	for (int i = 0; i < as_num_panels && i < 20; ++i)
	{
		as_panel_t *p = &as_panels[i];
		Con_Printf (" panel %d: [%g %g %g] -> [%g %g %g]\n", i, p->mins[0], p->mins[1], p->mins[2], p->maxs[0], p->maxs[1], p->maxs[2]);
	}
}
static int AS_CompareDouble (const void *a, const void *b)
{
	double x = *(const double *)a, y = *(const double *)b;
	return (x > y) - (x < y);
}
void AS_Frame (void)
{
    HP_Frame();
	static int frames, last_sequence = -1, previous_tick;
	if (as_tick < previous_tick)
		last_sequence = -1;
	previous_tick = as_tick;
	if (cls.signon != SIGNONS)
		return;
	++frames;
    if(COM_CheckParm("-capture-demo")) {
        if(cls.demoplayback) {
            ++as_demo_capture_frame;
            if(as_demo_capture_frame%q_max(1,as_sequence_step)==0)
                Cbuf_AddText(va("screenshot jpg 93 \"%s-%05d.jpg\"\n",as_sequence,as_demo_capture_frame));
        }
        return;
    }
    if(AS_ShowcaseScene()) {
        static int last=-1;
        int tick=AS_ShowcaseTick();
        if(tick>=0 && tick!=last) {
            last=tick;AS_ShowcaseEffects();
            if(tick>=31 && tick<=330 && (tick-31)%q_max(1,as_sequence_step)==0)
                Cbuf_AddText(va("screenshot png 90 \"%s-%05d.png\"\n",as_sequence,tick));
            if(tick>=as_end_frames)Cbuf_AddText("quit\n");
        }
        return;
    }
	double now = Sys_DoubleTime ();
	if (!as_begin_time)
		as_begin_time = now;
	if (as_last_frame && as_frame_count < 16384)
		as_frame_times[as_frame_count++] = (now - as_last_frame) * 1000;
	as_last_frame = now;
	if (rs_gputime_us)
	{
		as_gpu_total += rs_gputime_us / 1000.0;
		++as_gpu_count;
	}
	int due = last_sequence < as_sequence_start ? as_sequence_start : last_sequence + as_sequence_step;
	if (as_sequence[0] && as_tick >= due)
	{
		last_sequence = due;
		Con_Printf ("Aftershock capture: requested tick=%d observed tick=%d\n", due, as_tick);
		Cbuf_AddText (va ("screenshot png 90 \"%s-%05d.png\"\n", as_sequence, due));
	}
	if (as_capture_tick > 0 && as_tick >= as_capture_tick)
	{
		Cbuf_AddText (va ("screenshot png 90 \"%s\"\n", as_capture_path));
		as_capture_tick = 0;
	}
	if (as_end_frames > 0 && frames >= as_end_frames)
	{
		if (as_benchmark[0])
		{
			FILE *f = AS_OpenFile (as_benchmark, "w");
			if (f)
			{
				qsort (as_frame_times, as_frame_count, sizeof (double), AS_CompareDouble);
				as_stats s = AS_PhysicsStats ();
				fprintf (
					f,
					"{\"measurement\":\"host frames, not display "
					"presentation\",\"frames\":%d,\"seconds\":%.6f,\"host_fps\":%.3f,\"p50_ms\":%.3f,\"p95_ms\":%.3f,\"p99_ms\":%.3f,\"physics_last_ms\":%.3f,"
					"\"bodies\":%u,\"detached\":%u,\"errors\":%u,\"gpu_frame_mean_ms\":%.3f,\"physics_mean_ms\":%.3f,\"static_samples_bytes\":%zu,\"surface_"
					"metadata_bytes\":%zu,\"scatter_bytes\":%zu,\"structure\":%d,\"layers\":%d,\"fidelity\":%d,\"reflections\":%d,\"msaa_samples\":%u,\"render_scale\":%d,\"physx_live_bytes\":%llu,\"physx_peak_bytes\":%llu}\n",
					frames, now - as_begin_time, (frames - 1) / q_max (now - as_begin_time, 0.0001), as_frame_times[as_frame_count / 2],
					as_frame_times[(as_frame_count * 95) / 100], as_frame_times[(as_frame_count * 99) / 100], s.simulation_ms, s.bodies, s.detached, s.errors,
					as_gpu_count ? as_gpu_total / as_gpu_count : 0, as_physics_steps ? as_physics_total / as_physics_steps : 0, as_static_sample_bytes,
					as_model ? as_model->numsurfaces * sizeof (as_surface_gpu_t) : 0, as_scatter_bytes, AS_StructureMode (), CLAMP (1, (int)as_layers.value, 4), as_fidelity.value != 0, as_reflections.value != 0, (unsigned)vulkan_globals.sample_count, render_scale,
					(unsigned long long)s.allocator_live_bytes, (unsigned long long)s.allocator_peak_bytes);
				fclose (f);
			}
			else
				Con_Printf ("Cannot write benchmark: %s\n", as_benchmark);
			as_benchmark[0] = 0;
		}
		as_end_frames = 0;
		Cbuf_AddText ("as_gore_stats\nquit\n");
	}
}
void AS_Shutdown (void)
{
	AS_ClearMap ();
	AS_PhysicsShutdown ();
}
void AS_SaveWorld (const char *path)
{
	char sidecar[MAX_OSPATH];
	q_snprintf (sidecar, sizeof (sidecar), "%s.pqas", path);
	if (!as_ready)
	{
		if (!AS_RemoveFile (sidecar))
			Host_Error ("Could not remove obsolete Aftershock sidecar: %s", sidecar);
		return;
	}
	if (!AS_PhysicsSave (sidecar, as_map_hash))
		Host_Error ("Could not save Aftershock world: %s", sidecar);
}
void AS_RequestLoad (const char *path)
{
	if (!path)
	{
		as_pending_load[0] = 0;
		return;
	}
	q_snprintf (as_pending_load, sizeof (as_pending_load), "%s.pqas", path);
	if (!as_worldmode.value)
	{
		if (Sys_FileType (as_pending_load) != FS_ENT_NONE)
			Host_Error ("This save requires -worldmode destruction");
		as_pending_load[0] = 0;
	}
}
void AS_VisualExplosion (const vec3_t position)
{
	AS_VisualExplosionImpl (position, true);
}
static void AS_VisualExplosionImpl (const vec3_t position, qboolean gore)
{
	if (!as_effects.value)
		return;
	GL_SynchronizeEndRenderingTask ();
	int index = (as_fx_serial++) % 32;
	VectorCopy (position, as_fx[index].origin);
	VectorCopy (vec3_origin, as_fx[index].direction);
	float nearest = 256;
	for (int n = 0; n < 32; ++n)
	{
		vec3_t delta;
		VectorSubtract (position, as_launches[n].origin, delta);
		float d = DotProduct (delta, delta);
		if (!as_launches[n].used && realtime - as_launches[n].born < .5 && d < nearest && VectorLength (as_launches[n].direction) > .5)
		{
			nearest = d;
			VectorCopy (as_launches[n].direction, as_fx[index].direction);
			as_launches[n].used = true;
		}
	}
	if (VectorLength (as_fx[index].direction) < .5)
		AS_GoreDirection (position, as_fx[index].direction);
	if (gore)
		AS_GoreExplosion (position, as_fx[index].direction);
	as_fx[index].born = cl.time;
	as_fx[index].strength = 1;
	as_fx[index].seed = as_fx_serial;
	as_fx_count = q_min (as_fx_count + 1, 32);
	vec3_t start, end;
	VectorCopy (position, start);
	VectorCopy (position, end);
	end[2] -= 512;
	trace_t tr;
	memset (&tr, 0, sizeof (tr));
	tr.fraction = 1;
	tr.allsolid = true;
	if (cl.worldmodel)
		SV_RecursiveHullCheck (&cl.worldmodel->hulls[0], start, end, &tr, ~(1u << -CONTENTS_EMPTY));
	as_fx[index].floor = position[2] - 512 * tr.fraction + 0.5f;
    // Keep a real contact plane for sloped ground, not just its Z height.
    VectorCopy(vec3_origin,as_fx[index].floor_plane);
    as_fx[index].floor_plane[2]=1;
    as_fx[index].floor_plane[3]=as_fx[index].floor;
    if(tr.fraction<1 && !tr.startsolid && tr.plane.normal[2]>.05f){
        VectorCopy(tr.plane.normal,as_fx[index].floor_plane);
        vec3_t hit;VectorCopy(start,hit);hit[2]-=512*tr.fraction;
        as_fx[index].floor_plane[3]=DotProduct(tr.plane.normal,hit)+.25f;
    }

}
void AS_DrawEffects (cb_context_t *cbx)
{
	if (!as_effects.value)
		return;
    AS_DrawNails(cbx);
	extern gltexture_t *particletexture3;
	// Soft droplets/mist stay smooth even when stock Quake uses square particles.
	vulkan_globals.vk_cmd_bind_descriptor_sets (
		cbx->cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan_globals.basic_pipeline_layout.handle, 0, 1,
		&(AS_StructureMode () && !AS_Neon() ? whitetexture : particletexture3)->descriptor_set, 0, NULL);
	for (int i = 0; i < as_fx_count; ++i)
	{
		float age = (float)(cl.time - as_fx[i].born);
		if (age < 0 || age > 5.5f)
			continue;
		R_BindPipeline (
			cbx, VK_PIPELINE_BIND_POINT_GRAPHICS,
			R_PipelineForSubpassType (
				cbx->subpass_type, vulkan_globals.aftershock_fx[cbx->pipeline_variant], vulkan_globals.aftershock_fx_oit, vulkan_globals.aftershock_fx_moment,
				vulkan_globals.aftershock_fx_composite));
		VkBuffer	 vb, ib;
		VkDeviceSize vo, io;
		float		*v = (float *)R_VertexAllocate (80, &vb, &vo);
		memset (v, 0, 80);
		memcpy (v, as_fx[i].origin, 12);
		v[3] = age;
		v[4] = (float)as_fx[i].seed;
		v[5] = as_fx[i].strength;
		v[6] = as_reduced_flashes.value;
		v[7] = as_fx[i].floor;
        memcpy(v+12,as_fx[i].floor_plane,16);
		memcpy (v + 8, as_fx[i].direction, 12);
		v[11] = CLAMP (0, (int)as_style.value, 13) + 16 * AS_StructureMode ();
		uint16_t	  *idx = (uint16_t *)R_IndexAllocate (12, &ib, &io);
		const uint16_t quad[6] = {0, 1, 2, 0, 2, 3};
		memcpy (idx, quad, 12);
		vulkan_globals.vk_cmd_bind_vertex_buffers (cbx->cb, 0, 1, &vb, &vo);
		vulkan_globals.vk_cmd_bind_index_buffer (cbx->cb, ib, io, VK_INDEX_TYPE_UINT16);
		int amount = (as_fx[i].strength < 1 ? 128 : 2048) * CLAMP (1, (int)as_particle_amount.value, 4);
		if (AS_StructureMode ())
			vulkan_globals.vk_cmd_draw (cbx->cb, AS_StructureVertices (AS_StructureMode ()), amount, 0, 0);
		else
			vulkan_globals.vk_cmd_draw_indexed (cbx->cb, 6, amount, 0, 0, 0);
		Atomic_AddUInt32 (&rs_particles, amount);
	}
	vulkan_globals.vk_cmd_bind_descriptor_sets (
		cbx->cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan_globals.basic_pipeline_layout.handle, 0, 1, &particletexture3->descriptor_set, 0, NULL);
	// At most 48 nearby visible face emitters, with uniformly sampled triangles.
	// This extra layer leaves the opaque source surface and its coverage intact.
	if (as_renderer.value && as_style.value >= 4 && as_model && as_faces)
	{
		int emitted = 0;
		for (int i = 0; i < as_model->nummodelsurfaces && emitted < 48; ++i)
		{
			msurface_t *s = &as_model->surfaces[i];
			if (!as_faces[i].count || AS_SurfaceReplaced (s) || !(((uint32_t *)as_model->surfvis)[i / 32] & (1u << (i % 32))))
				continue;
			glpoly_t *p = s->polys;
			if (!p || p->numverts < 3)
				continue;
			vec3_t center = {0, 0, 0}, delta;
			for (int j = 0; j < p->numverts; ++j)
				VectorAdd (center, p->verts[j], center);
			VectorScale (center, 1.0f / p->numverts, center);
			VectorSubtract (center, r_origin, delta);
			if (DotProduct (delta, delta) > 600 * 600)
				continue;
			for (int tri = 1; tri < p->numverts - 1 && emitted < 48; ++tri)
			{
				R_BindPipeline (
					cbx, VK_PIPELINE_BIND_POINT_GRAPHICS,
					R_PipelineForSubpassType (
						cbx->subpass_type, vulkan_globals.aftershock_fx[cbx->pipeline_variant], vulkan_globals.aftershock_fx_oit,
						vulkan_globals.aftershock_fx_moment, vulkan_globals.aftershock_fx_composite));
				VkBuffer	 vb, ib;
				VkDeviceSize vo, io;
				float		*v = (float *)R_VertexAllocate (80, &vb, &vo);
				memset (v, 0, 80);
				memcpy (v, p->verts[0], 12);
				v[3] = as_reduced_flashes.value ? 0 : (float)cl.time;
				v[4] = (float)(i * 16 + tri);
				v[5] = -1;
				v[6] = as_reduced_flashes.value;
				VectorScale (s->plane->normal, (s->flags & SURF_PLANEBACK) ? -1 : 1, v + 8);
				v[11] = CLAMP (4, (int)as_style.value, 13);
				VectorSubtract (p->verts[tri], p->verts[0], v + 12);
				VectorSubtract (p->verts[tri + 1], p->verts[0], v + 16);
				uint16_t	  *idx = (uint16_t *)R_IndexAllocate (12, &ib, &io);
				const uint16_t quad[6] = {0, 1, 2, 0, 2, 3};
				memcpy (idx, quad, 12);
				vulkan_globals.vk_cmd_bind_vertex_buffers (cbx->cb, 0, 1, &vb, &vo);
				vulkan_globals.vk_cmd_bind_index_buffer (cbx->cb, ib, io, VK_INDEX_TYPE_UINT16);
				int count = 128 * CLAMP (1, (int)as_particle_amount.value, 4);
				vulkan_globals.vk_cmd_draw_indexed (cbx->cb, 6, count, 0, 0, 0);
				Atomic_AddUInt32 (&rs_particles, count);
				++emitted;
			}
		}
	}
}
void AS_TestCamera (void)
{
    if(AS_ShowcaseScene()){AS_ShowcaseCamera();return;}
	if (as_effects.value && as_shake.value > 0)
		for (int i = 0; i < as_fx_count; ++i)
		{
			double age = cl.time - as_fx[i].born;
			if (age < 0 || age > .65)
				continue;
			vec3_t delta;
			VectorSubtract (r_refdef.vieworg, as_fx[i].origin, delta);
			float strength = CLAMP (0, as_shake.value, 2) * q_max (0, 1 - VectorLength (delta) / 600) * (float)exp (-age * 7) * as_fx[i].strength;
			r_refdef.viewangles[0] += sinf ((float)age * 71 + as_fx[i].seed) * strength * 3;
			r_refdef.viewangles[2] += sinf ((float)age * 53 + as_fx[i].seed) * strength * 2;
		}
    if(COM_CheckParm("-test-fidelity-motion")) {
        r_refdef.viewangles[1]+=sinf(as_tick*.022f)*18;
        r_refdef.vieworg[1]+=sinf(as_tick*.031f)*12;
        if(as_tick>=100 && as_tick<130)r_refdef.vieworg[2]+=72;
    }
	if (!as_ready || !COM_CheckParm ("-test-view") || !as_num_panels)
		return;
	as_panel_t *p = &as_panels[CLAMP (0, as_test_panel, as_num_panels - 1)];
	for (int a = 0; a < 3; ++a)
		r_refdef.vieworg[a] = (p->mins[a] + p->maxs[a]) * 0.5f;
	r_refdef.vieworg[p->axis] = p->mins[p->axis] - 140;
	memset (r_refdef.viewangles, 0, sizeof (r_refdef.viewangles));
	r_refdef.viewangles[1] = p->axis == 1 ? 90 : 0;
	if (COM_CheckParm ("-test-view-oblique") && p->axis < 2)
	{
		r_refdef.vieworg[1 - p->axis] -= 35;
		r_refdef.vieworg[p->axis] = p->mins[p->axis] - 140;
		r_refdef.viewangles[1] += p->axis == 1 ? -14 : 14;
	}
}
static void AS_Argument (const char *name, cvar_t *var, const char **choices, int count)
{
	int i = COM_CheckParm (name);
	if (!i)
		return;
	if (i + 1 >= com_argc)
		Sys_Error ("%s requires a value", name);
	for (int k = 0; k < count; ++k)
		if (!strcmp (com_argv[i + 1], choices[k]))
		{
			if (var->flags & CVAR_ROM)
				Cvar_SetValueROM (var->name, (float)k);
			else
				Cvar_SetValueQuick (var, (float)k);
			return;
		}
	Sys_Error ("Invalid %s value: %s", name, com_argv[i + 1]);
}
static void AS_Reset_f (void)
{
	if (!sv.active || svs.maxclients != 1)
	{
		Con_Printf ("Reset requires local single-player\n");
		return;
	}
	Cbuf_AddText (va ("map %s\n", sv.name));
}
static void AS_Mode_f (void)
{
	if (Cmd_Argc () != 2 || (strcmp (Cmd_Argv (1), "faithful") && strcmp (Cmd_Argv (1), "destruction")))
	{
		Con_Printf ("as_mode faithful|destruction (reloads current map)\n");
		return;
	}
	if (!sv.active || svs.maxclients != 1)
	{
		Con_Printf ("World mode changes require local single-player\n");
		return;
	}
	Cvar_SetROM ("as_worldmode", !strcmp (Cmd_Argv (1), "destruction") ? "1" : "0");
	AS_Reset_f ();
}
void AS_ApplyArguments (void)
{
	const char *renderers[] = {"classic", "particle"}, *modes[] = {"faithful", "destruction"},
			   *styles[] = {"faithful", "enhanced",		   "inferno",		  "inferno-color", "living-stone", "volcanic",		"sandstorm",
							"crystal",	"corrupted-flesh", "industrial-rust", "spectral",	   "frozen-ruins", "electric-grid", "cosmic-dust"};
	AS_Argument ("-renderer", &as_renderer, renderers, 2);
	AS_Argument ("-worldmode", &as_worldmode, modes, 2);
	AS_Argument ("-style", &as_style, styles, 14);
	int i = COM_CheckParm ("-density");
	if (i && i + 1 >= com_argc)
		Sys_Error ("-density requires play, fine, or showcase");
	if (i && i + 1 < com_argc)
	{
		const char *d = com_argv[i + 1];
		if (!strcmp (d, "play"))
			Cvar_SetValueQuick (&as_density, 4);
		else if (!strcmp (d, "fine"))
			Cvar_SetValueQuick (&as_density, 3);
		else if (!strcmp (d, "showcase"))
			Cvar_SetValueQuick (&as_density, 2);
		else
			Sys_Error ("Invalid -density");
	}
	i = COM_CheckParm ("-physics");
	if (i)
	{
		if (i + 1 >= com_argc)
			Sys_Error ("-physics requires off, physx-cpu, or physx-gpu");
		if (!strcmp (com_argv[i + 1], "physx-gpu"))
			Sys_Error ("This build provides PhysX CPU. Use -physics physx-cpu.");
		if (strcmp (com_argv[i + 1], "off") && strcmp (com_argv[i + 1], "physx-cpu"))
			Sys_Error ("Unknown physics backend: %s", com_argv[i + 1]);
		if (as_worldmode.value && strcmp (com_argv[i + 1], "physx-cpu"))
			Sys_Error ("Destruction requires -physics physx-cpu");
	}
	i = COM_CheckParm ("-destruction-preset");
	if (i)
	{
		if (i + 1 >= com_argc)
			Sys_Error ("-destruction-preset requires restrained, cinematic, or cataclysm");
		const char *presets[] = {"restrained", "cinematic", "cataclysm"};
		int			k;
		for (k = 0; k < 3; ++k)
			if (!strcmp (com_argv[i + 1], presets[k]))
				break;
		if (k == 3)
			Sys_Error ("Unknown destruction preset");
		Cvar_SetValueQuick (&as_radius, (float)(90 + 40 * k));
		Cvar_SetValueQuick (&as_damage, (float)(3 + 3 * k));
	}
}
static void AS_NeonPrism_f(void)
{
    qboolean enabled=Cmd_Argc()<2 || atof(Cmd_Argv(1))!=0;
    Cvar_SetValueQuick(&as_neon_prism,enabled);
    Cvar_SetValueQuick(&as_reflection_strength,enabled?2.4f:1.f);
    Cvar_SetValueQuick(&as_reflection_roughness,enabled?.10f:.20f);
    Cvar_SetValueQuick(&as_neon_glow,enabled?1.15f:1.f);
    if(enabled) {
        Cvar_SetValueQuick(&as_renderer,1);Cvar_SetValueQuick(&as_structure,11);
        Cvar_SetValueQuick(&as_fidelity,1);Cvar_SetValueQuick(&as_reflections,1);
        Cvar_SetValueQuick(&as_layers,3);
    }
}
void AS_Init (void)
{
    HP_Init();
	Cvar_RegisterVariable (&as_renderer);
    Cvar_SetCallback(&as_renderer,HP_RendererChanged);
	Cvar_RegisterVariable (&as_worldmode);
	Cvar_RegisterVariable (&as_density);
	Cvar_RegisterVariable (&as_style);
	Cvar_RegisterVariable (&as_structure);
	Cvar_RegisterVariable (&as_layers);
	Cvar_RegisterVariable (&as_fidelity);
    Cvar_RegisterVariable (&as_nails);
	Cvar_RegisterVariable (&as_reflections);
    Cvar_RegisterVariable(&as_neon_prism);Cvar_RegisterVariable(&as_reflection_strength);
    Cvar_RegisterVariable(&as_reflection_roughness);Cvar_RegisterVariable(&as_neon_glow);
    Cvar_RegisterVariable(&as_neon_npc_texture);
    Cvar_RegisterVariable(&as_smw);Cvar_RegisterVariable(&as_smw_pixel);Cvar_RegisterVariable(&as_smw_outline);
    Cvar_RegisterVariable(&as_smw_edge);Cvar_RegisterVariable(&as_smw_saturate);
    Cvar_SetCallback(&as_smw,AS_SMWChanged);
    Cmd_AddCommand("neon_prism",AS_NeonPrism_f);
	Cvar_RegisterVariable (&as_radius);
	Cvar_RegisterVariable (&as_damage);
    Cvar_RegisterVariable(&as_progressive);Cvar_RegisterVariable(&as_rocket_hits);Cvar_RegisterVariable(&as_chip_radius);
	Cvar_RegisterVariable (&as_effects);
	Cvar_RegisterVariable (&as_shake);
	Cvar_RegisterVariable (&as_reduced_flashes);
	Cvar_RegisterVariable (&as_particle_amount);
	AS_GoreInit ();
	AS_ApplyArguments ();
	int i = COM_CheckParm ("-test-traverse-ticks");
	if (i && i + 1 < com_argc)
		as_walk_ticks = CLAMP (1, atoi (com_argv[i + 1]), 1200);
	i = COM_CheckParm ("-capture-tick");
	if (i && i + 1 < com_argc)
		as_capture_tick = atoi (com_argv[i + 1]);
	i = COM_CheckParm ("-screenshot");
	if (i && i + 1 < com_argc)
		q_strlcpy (as_capture_path, com_argv[i + 1], sizeof (as_capture_path));
	else
		q_strlcpy (as_capture_path, "aftershock", sizeof (as_capture_path));
	i = COM_CheckParm ("-frames");
	if (i && i + 1 < com_argc)
		as_end_frames = atoi (com_argv[i + 1]);
	i = COM_CheckParm ("-test-explosion-tick");
	if (i && i + 1 < com_argc)
		as_test_explosion_tick = atoi (com_argv[i + 1]);
	i = COM_CheckParm ("-test-panel");
	if (i && i + 1 < com_argc)
		as_test_panel = atoi (com_argv[i + 1]);
	i = COM_CheckParm ("-sim-checksum-tick");
	if (i && i + 1 < com_argc)
		as_checksum_tick = atoi (com_argv[i + 1]);
	i = COM_CheckParm ("-test-save-tick");
	if (i && i + 1 < com_argc)
		as_save_tick = atoi (com_argv[i + 1]);
	i = COM_CheckParm ("-test-load-tick");
	if (i && i + 1 < com_argc)
		as_load_tick = atoi (com_argv[i + 1]);
	i = COM_CheckParm ("-test-renderer-tick");
	if (i && i + 1 < com_argc)
		as_switch_tick = atoi (com_argv[i + 1]);
	i = COM_CheckParm ("-capture-sequence");
	if (i && i + 1 < com_argc)
		q_strlcpy (as_sequence, com_argv[i + 1], sizeof (as_sequence));
	as_sequence_start = 1;
	as_sequence_step = 10;
	i = COM_CheckParm ("-capture-start-tick");
	if (i && i + 1 < com_argc)
		as_sequence_start = q_max (1, atoi (com_argv[i + 1]));
	i = COM_CheckParm ("-capture-ticks");
	if (i && i + 1 < com_argc)
		as_sequence_step = q_max (1, atoi (com_argv[i + 1]));
	i = COM_CheckParm ("-benchmark-json");
	if (i && i + 1 < com_argc)
		q_strlcpy (as_benchmark, com_argv[i + 1], sizeof (as_benchmark));
	Cmd_AddCommand ("as_explode", AS_Explode_f);
	Cmd_AddCommand ("as_inspect", AS_Inspect_f);
	Cmd_AddCommand ("as_reset", AS_Reset_f);
	Cmd_AddCommand ("as_mode", AS_Mode_f);
}
