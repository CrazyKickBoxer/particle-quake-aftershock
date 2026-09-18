/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Josh Nicholls
 * Included by aftershock.c. Owns cosmetic GPU state, never game objects. */
#include "../../src/holo_physics_contract.h"
#include "../Shaders/Compiled/holo_0.c"
#include "../Shaders/Compiled/holo_1.c"
#include "../Shaders/Compiled/holo_2.c"
#include "../Shaders/Compiled/holo_3.c"
#include "../Shaders/Compiled/holo_4.c"
#include "../Shaders/Compiled/holo_5.c"
#include "../Shaders/Compiled/holo_6.c"
#include "../Shaders/Compiled/holo_7.c"
#include "../Shaders/Compiled/holo_8.c"
#include "../Shaders/Compiled/holo_9.c"
#include "../Shaders/Compiled/holo_10.c"
#include "../Shaders/Compiled/holo_11.c"
#include "../Shaders/Compiled/holo_12.c"
#include "../Shaders/Compiled/holo_13.c"
#include "../Shaders/Compiled/holo_14.c"
#include "../Shaders/Compiled/holo_15.c"
#include "../Shaders/Compiled/holo_16.c"
static cvar_t hp_enabled={"r_holo_physics","0",CVAR_ARCHIVE};
static cvar_t hp_holo={"r_holo","0",CVAR_ARCHIVE};
static cvar_t hp_budget={"r_holo_phys_budget","65536",CVAR_ARCHIVE};
static cvar_t hp_settle={"r_holo_phys_settle","1.2",CVAR_ARCHIVE};
static cvar_t hp_blast={"r_holo_phys_blast","1",CVAR_ARCHIVE};
static cvar_t hp_radius={"r_holo_phys_blast_radius","1",CVAR_ARCHIVE};
static cvar_t hp_suction={"r_holo_phys_suction","0.35",CVAR_ARCHIVE};
static cvar_t hp_turbulence={"r_holo_phys_turbulence","0.6",CVAR_ARCHIVE};
static cvar_t hp_debug={"r_holo_phys_debug","0",CVAR_ARCHIVE};
static cvar_t hp_beam={"r_holo_phys_beam","1",CVAR_ARCHIVE};
static cvar_t hp_beam_radius={"r_holo_phys_beam_radius","24",CVAR_ARCHIVE};
static cvar_t hp_wake={"r_holo_phys_wake","0.7",CVAR_ARCHIVE};
static cvar_t hp_player_wake={"r_holo_phys_player_wake","0.5",CVAR_ARCHIVE};
static cvar_t hp_liquid={"r_holo_phys_liquid","1",CVAR_ARCHIVE};
static cvar_t hp_teleport={"r_holo_phys_teleport","1",CVAR_ARCHIVE};
static cvar_t hp_dust={"r_holo_phys_dust","0.8",CVAR_ARCHIVE};
static cvar_t hp_shake={"r_holo_phys_shake","0.4",CVAR_ARCHIVE};
static cvar_t hp_debris_light={"r_holo_phys_debris_light","1",CVAR_ARCHIVE};
static struct { qmodel_t *model; vec3_t position; qboolean valid,liquid; } hp_observed[8192];
static qboolean hp_was_inwater;
static float hp_next_player_wake;
static cvar_t hp_spray={"r_holo_phys_spray","1",CVAR_ARCHIVE};
static cvar_t hp_spray_count={"r_holo_phys_spray_count","96",CVAR_ARCHIVE};
static cvar_t hp_collide={"r_holo_phys_collide","1",CVAR_ARCHIVE};
static cvar_t hp_restitution={"r_holo_phys_restitution","0.45",CVAR_ARCHIVE};
static cvar_t hp_turbulence_scale={"r_holo_phys_turbulence_scale","48",CVAR_ARCHIVE};
static hp_queue_t hp_pending;
static hp_impulse_t hp_events[64];
static uint32_t hp_new_events;
static uint32_t hp_events_count,hp_capacity,hp_sources,hp_surface_count,hp_promote_groups,hp_input;
static float hp_clock,hp_dt,hp_awake_until,hp_gore_until,hp_light_until;
static qboolean hp_gore_draw_enabled;
// Conservative event bounds select a shader variant for already CPU-drawn
// surfaces. This never evaluates or updates individual particles on the CPU.
static struct { vec3_t mins,maxs;float until; } hp_draw_bounds[256];
static float hp_bounds_overflow_until;
static void HP_TrackBounds(const hp_impulse_t *e) {
    uint32_t slot=256;
    for(uint32_t i=0;i<256;++i) if(hp_draw_bounds[i].until<=hp_clock) { slot=i;break; }
    float until=hp_clock+e->duration+5;
    if(slot==256) { hp_bounds_overflow_until=q_max(hp_bounds_overflow_until,until);return; }
    float radius=e->radius;
    if(e->type==HP_BLOOD || e->type==HP_DEATH) radius=q_max(radius,512);
    for(int k=0;k<3;++k) {
        float end=(e->type==HP_WAKE || e->type==HP_BEAM)?e->direction[k]:e->origin[k];
        hp_draw_bounds[slot].mins[k]=q_min(e->origin[k],end)-radius;
        hp_draw_bounds[slot].maxs[k]=q_max(e->origin[k],end)+radius;
    }
    hp_draw_bounds[slot].until=until;
}
static cvar_t hp_gore={"r_holo_phys_gore","1",CVAR_ARCHIVE};
static cvar_t hp_gore_max={"r_holo_phys_gore_max","20000",CVAR_ARCHIVE};
static cvar_t hp_gore_life={"r_holo_phys_gore_life","45",CVAR_ARCHIVE};
static cvar_t hp_gore_gravity={"r_holo_phys_gore_gravity","1",CVAR_ARCHIVE};
static double hp_previous_time;
static qboolean hp_initialized,hp_draw_enabled,hp_reset_pending;
static qboolean hp_test_fired;
static VkDescriptorSetLayout hp_set_layout;
static VkDescriptorPool hp_pool;
static VkDescriptorSet hp_set;
static VkPipelineLayout hp_compute_layout,hp_death_layout;
static VkDescriptorSetLayout hp_mesh_layout;
static VkDescriptorSet hp_mesh_sets[2][32];
static cvar_t hp_death={"r_holo_phys_death","1.6",CVAR_ARCHIVE};
static cvar_t hp_death_life={"r_holo_phys_death_life","3",CVAR_ARCHIVE};
typedef struct { float model[16];uint32_t mesh[4];float parameters[4];uint32_t info[4]; } hp_death_job_t;
_Static_assert(sizeof(hp_death_job_t)==112,"Holo death metadata stride");
static hp_death_job_t hp_death_jobs[32];
static VkBuffer hp_death_meshes[32];
static atomic_uint32_t hp_death_pending;
static struct { hp_death_job_t job;VkBuffer mesh;qmodel_t *model;float seen; } hp_last_alias[8192];
static qmodel_t *hp_gib_seen[8192];
static void HP_GibDeaths(void);
static struct { qmodel_t *model;qboolean dead; } hp_dead_owners[8192];
static VkPipeline hp_compute[17];
static VkBuffer hp_buffers[12];
static vulkan_memory_t hp_memory[12];
static size_t hp_sizes[12];
static float hp_grid_origin_cell[4];
#define HP_GRID_CAPACITY 1048576u
static VkBuffer hp_readback[2];
static vulkan_memory_t hp_read_memory[2];
static uint32_t *hp_read_words[2];
static qboolean hp_read_ready[2];
static uint32_t hp_active_count,hp_overflow_count,hp_stuck_count,hp_evictions;
static VkQueryPool hp_queries;
static qboolean hp_query_ready[2][3];
static int hp_query_frame;
static float hp_compute_ms,hp_grid_ms,hp_gore_ms;
static FILE *hp_profile;

static struct { float origin[4],s[4],t[4]; uint32_t range[4]; float mins[4],maxs[4]; } *hp_surfaces;
typedef struct { uint32_t budget,sources,events,input; float dt,time,settle,turbulence,suction,gravity,epsilon_position,epsilon_speed;
    float spray_count,point_step,pad0,pad1;
    float grid_origin_cell[4]; uint32_t grid_capacity,collide; float restitution,turbulence_scale; uint32_t gore_cap,gore_enabled; float gore_life,gore_gravity;float dust,beam_crawl,death_enabled,shake; } hp_push_t;
_Static_assert(sizeof(hp_push_t)==128,"Holo push constants");
static cvar_t *hp_cvars[]={&hp_holo,&hp_enabled,&hp_budget,&hp_settle,&hp_blast,&hp_radius,&hp_suction,&hp_turbulence,&hp_debug,&hp_spray,&hp_spray_count,&hp_collide,&hp_restitution,&hp_turbulence_scale,&hp_gore,&hp_gore_max,&hp_gore_life,&hp_gore_gravity,&hp_death,&hp_death_life,&hp_beam,&hp_beam_radius,&hp_wake,&hp_player_wake,&hp_liquid,&hp_teleport,&hp_dust,&hp_shake,&hp_debris_light};
static float HP_Value(cvar_t *v,float low,float high,float fallback) {
    return isfinite(v->value)?CLAMP(low,v->value,high):fallback;
}
static void HP_Check(VkResult r) { if(r!=VK_SUCCESS) Sys_Error("Holo Physics Vulkan error %d",r); }
static void HP_HoloChanged(cvar_t *v) { Cvar_SetValueQuick(&as_renderer,v->value!=0); }
static void HP_RendererChanged(cvar_t *v) { Cvar_SetValueQuick(&hp_holo,v->value!=0); }
static void HP_EnableChanged(cvar_t *v) {
    (void)v;hp_reset_pending=true;hp_awake_until=hp_gore_until=hp_light_until=0;
    hp_events_count=hp_new_events=0;memset(&hp_pending,0,sizeof(hp_pending));
    hp_active_count=hp_stuck_count=0;memset(hp_read_ready,0,sizeof(hp_read_ready));
    memset(hp_dead_owners,0,sizeof(hp_dead_owners));memset(hp_last_alias,0,sizeof(hp_last_alias));
}
static void HP_Off_f(void) { Cvar_SetValueQuick(&hp_enabled,0); }
static void HP_Defaults_f(void) {
    for(int i=0;i<countof(hp_cvars);++i) if(hp_cvars[i]!=&hp_holo && hp_cvars[i]!=&hp_enabled)
        Cvar_SetQuick(hp_cvars[i],hp_cvars[i]->default_string);
}
static void HP_On_f(void) {
    int preset=1;
    if(Cmd_Argc()>1) { if(!strcmp(Cmd_Argv(1),"play"))preset=0;
        else if(!strcmp(Cmd_Argv(1),"showcase"))preset=2;
        else if(strcmp(Cmd_Argv(1),"fine")) { Con_Printf("holo_physics [play|fine|showcase]\n");return; } }
    HP_Defaults_f();Cvar_SetValueQuick(&hp_budget,(float)hp_presets[preset].budget);
    Cvar_SetValueQuick(&hp_gore_max,(float)hp_presets[preset].gore_cap);Cvar_SetValueQuick(&hp_settle,hp_presets[preset].settle);
    Cvar_SetValueQuick(&hp_dust,hp_presets[preset].dust);Cvar_SetValueQuick(&hp_holo,1);Cvar_SetValueQuick(&hp_enabled,1);
}

static void HP_EventSlot(int type,const vec3_t origin,const vec3_t direction,float radius,float strength,int color_slot) {
    if(!hp_enabled.value || !as_renderer.value) return;
    hp_impulse_t e={0}; VectorCopy(origin,e.origin);
    if(direction) { VectorCopy(direction,e.direction); VectorCopy(direction,e.normal); }
    static const float style_force[15]={.8f,1,1.2f,1.15f,.85f,1.15f,.9f,.85f,1.05f,1,.7f,.8f,1,1,1};
    int style=AS_Neon()?14:(int)HP_Value(&as_style,0,13,1);
    e.radius=radius; e.strength=strength*style_force[style]; e.type=type;
    e.duration=HP_Value(&hp_settle,.1f,5,1.2f); e.birth_time=hp_clock;
    e.color_slot=type==HP_BLOOD?HP_BLOOD_SLOT:type==HP_IMPLODE?HP_COOL:HP_HOT;
    if(type==HP_SPRAY) e.duration=.35f;
    if(type==HP_WAKE || type==HP_BEAM) e.duration=.25f;
    if(type==HP_WAKE) e.color_slot=HP_NEUTRAL;
    if(type==HP_IMPLODE) e.color_slot=HP_COOL;
    if(color_slot>=0) e.color_slot=(uint32_t)color_slot;
    HP_QueuePush(&hp_pending,e);
}
void HP_Event(int type,const vec3_t origin,const vec3_t direction,float radius,float strength) { HP_EventSlot(type,origin,direction,radius,strength,-1); }
void HP_TempEntity(int type,const vec3_t origin) {
    if(!hp_enabled.value || !as_renderer.value) return;
    switch(type) {
    case TE_EXPLOSION: case TE_EXPLOSION2: case TE_TAREXPLOSION:
        HP_Event(HP_SHOCKWAVE,origin,NULL,180*HP_Value(&hp_radius,.1f,4,1),320*HP_Value(&hp_blast,0,8,1)); break;
    case TE_LAVASPLASH: HP_Event(HP_SPLASH,origin,NULL,128,100*HP_Value(&hp_liquid,0,8,1));break;
    case TE_TELEPORT: HP_Event(HP_IMPLODE,origin,NULL,160,200*HP_Value(&hp_teleport,0,8,1));break;
    case TE_GUNSHOT: case TE_SPIKE: case TE_SUPERSPIKE: case TE_KNIGHTSPIKE: case TE_WIZSPIKE:
    {
        vec3_t direction={0};float closest=64*64;
        if(type!=TE_GUNSHOT) for(int i=1;i<q_min(cl.num_entities,8192);++i) {
            if(!hp_observed[i].valid || !hp_observed[i].model || !strstr(hp_observed[i].model->name,"spike")) continue;
            vec3_t delta;VectorSubtract(origin,hp_observed[i].position,delta);float d=DotProduct(delta,delta);
            if(d>.01f && d<closest) { closest=d;VectorCopy(delta,direction);VectorNormalize(direction); }
        }
        float medium=type==TE_SUPERSPIKE || type==TE_KNIGHTSPIKE?1.3f:1;
        HP_EventSlot(HP_SPRAY,origin,direction,12*medium,150*medium*HP_Value(&hp_spray,0,8,1),type==TE_WIZSPIKE?HP_COOL:HP_WHITE_HOT);
        break;
    }
    default: break;
    }
}
static void HP_Sphere_f(void) {
    vec3_t origin,forward,right,up; AngleVectors(cl.viewangles,forward,right,up);
    VectorMA(cl.viewent.origin,96,forward,origin);
    float radius=Cmd_Argc()>1?(float)atof(Cmd_Argv(1)):160;
    HP_Event(HP_DEBUG,origin,NULL,radius,240*HP_Value(&hp_blast,0,8,1));
}
static void HP_Init(void) {

    for(int i=0;i<countof(hp_cvars);++i) Cvar_RegisterVariable(hp_cvars[i]);
    Cvar_SetCallback(&hp_holo,HP_HoloChanged); Cvar_SetCallback(&hp_enabled,HP_EnableChanged);
    Cmd_AddCommand("holo_physics",HP_On_f); Cmd_AddCommand("holo_physics_reset",HP_Defaults_f);
    Cmd_AddCommand("holo_physics_off",HP_Off_f); Cmd_AddCommand("holo_physics_sphere",HP_Sphere_f);
}
static void HP_Clear(void) {
    if(hp_profile) { fclose(hp_profile);hp_profile=NULL; }
    hp_draw_enabled=false; hp_gore_draw_enabled=false;hp_gore_until=0;hp_initialized=false; hp_reset_pending=false; hp_capacity=0;
    hp_active_count=hp_overflow_count=hp_stuck_count=hp_evictions=0;
    hp_light_until=0;
    memset(hp_draw_bounds,0,sizeof(hp_draw_bounds));hp_bounds_overflow_until=0;hp_next_player_wake=0;
    hp_compute_ms=hp_gore_ms=hp_grid_ms=0;
    memset(&hp_pending,0,sizeof(hp_pending)); hp_events_count=0; hp_clock=0; hp_previous_time=0; hp_awake_until=0;
    hp_test_fired=false;Atomic_StoreUInt32(&hp_death_pending,0);memset(hp_dead_owners,0,sizeof(hp_dead_owners));memset(hp_observed,0,sizeof(hp_observed));memset(hp_last_alias,0,sizeof(hp_last_alias));memset(hp_gib_seen,0,sizeof(hp_gib_seen));hp_was_inwater=false;
    if(hp_pool) { vkDeviceWaitIdle(vulkan_globals.device); vkDestroyDescriptorPool(vulkan_globals.device,hp_pool,NULL); hp_pool=VK_NULL_HANDLE; }
    hp_set=VK_NULL_HANDLE;
    vkDestroyQueryPool(vulkan_globals.device,hp_queries,NULL);hp_queries=VK_NULL_HANDLE;memset(hp_query_ready,0,sizeof(hp_query_ready));
    for(int i=0;i<2;++i) if(hp_readback[i]) {
        vkUnmapMemory(vulkan_globals.device,hp_read_memory[i].handle);
        R_FreeBuffer(hp_readback[i],&hp_read_memory[i],NULL); hp_readback[i]=VK_NULL_HANDLE;
        hp_read_ready[i]=false; hp_read_words[i]=NULL;
    }
    for(int i=0;i<12;++i) if(hp_buffers[i]) { R_FreeBuffer(hp_buffers[i],&hp_memory[i],NULL); hp_buffers[i]=VK_NULL_HANDLE; }
}
void HP_CreateLayout(void) {
    if(hp_set_layout) return;
    VkDescriptorSetLayoutBinding bindings[12]={0};
    for(int i=0;i<12;++i) { bindings[i].binding=i; bindings[i].descriptorCount=1;
        bindings[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT|VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT; }
    VkDescriptorSetLayoutCreateInfo info={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount=12,.pBindings=bindings};
    HP_Check(vkCreateDescriptorSetLayout(vulkan_globals.device,&info,NULL,&hp_set_layout));
    VkDescriptorSetLayout layouts[6]={vulkan_globals.single_texture_set_layout.handle,
        vulkan_globals.single_texture_set_layout.handle,vulkan_globals.single_texture_set_layout.handle,
        vulkan_globals.mboit_input_attachment_set_layout.handle,vulkan_globals.bmodel_instances_set_layout.handle,hp_set_layout};
    VkPushConstantRange push={VK_SHADER_STAGE_ALL_GRAPHICS,0,88};
    VkPipelineLayoutCreateInfo p={.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,.setLayoutCount=6,
        .pSetLayouts=layouts,.pushConstantRangeCount=1,.pPushConstantRanges=&push};
    HP_Check(vkCreatePipelineLayout(vulkan_globals.device,&p,NULL,&vulkan_globals.holo_world_layout.handle));
    vulkan_globals.holo_world_layout.push_constant_range=push;
    vulkan_globals.holo_world_layout.mboit_input_attachment_set=3;
    push=(VkPushConstantRange){VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(hp_push_t)};
    p.setLayoutCount=1;p.pSetLayouts=&hp_set_layout;
    HP_Check(vkCreatePipelineLayout(vulkan_globals.device,&p,NULL,&hp_compute_layout));
    VkDescriptorSetLayoutBinding mesh_binding={0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,NULL};
    info.bindingCount=1;info.pBindings=&mesh_binding;
    HP_Check(vkCreateDescriptorSetLayout(vulkan_globals.device,&info,NULL,&hp_mesh_layout));
    VkDescriptorSetLayout death_layouts[2]={hp_set_layout,hp_mesh_layout};
    p.setLayoutCount=2;p.pSetLayouts=death_layouts;
    HP_Check(vkCreatePipelineLayout(vulkan_globals.device,&p,NULL,&hp_death_layout));
    const unsigned char *code[]={holo_0_spv,holo_1_spv,holo_2_spv,holo_3_spv,holo_4_spv,holo_5_spv,holo_6_spv,holo_7_spv,holo_8_spv,holo_9_spv,holo_10_spv,holo_11_spv,holo_12_spv,holo_13_spv,holo_14_spv,holo_15_spv,holo_16_spv};
    size_t sizes[]={sizeof(holo_0_spv),sizeof(holo_1_spv),sizeof(holo_2_spv),sizeof(holo_3_spv),sizeof(holo_4_spv),sizeof(holo_5_spv),sizeof(holo_6_spv),sizeof(holo_7_spv),sizeof(holo_8_spv),sizeof(holo_9_spv),sizeof(holo_10_spv),sizeof(holo_11_spv),sizeof(holo_12_spv),sizeof(holo_13_spv),sizeof(holo_14_spv),sizeof(holo_15_spv),sizeof(holo_16_spv)};
    for(int i=0;i<17;++i) {
        VkShaderModule module;
        VkShaderModuleCreateInfo m={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,.codeSize=sizes[i],.pCode=(const uint32_t*)code[i]};
        HP_Check(vkCreateShaderModule(vulkan_globals.device,&m,NULL,&module));
        VkComputePipelineCreateInfo c={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,.module=module,.pName="main"},.layout=i==11?hp_death_layout:hp_compute_layout};
        HP_Check(vkCreateComputePipelines(vulkan_globals.device,VK_NULL_HANDLE,1,&c,NULL,&hp_compute[i]));
        vkDestroyShaderModule(vulkan_globals.device,module,NULL);
    }
}
void HP_DestroyLayout(void) {
    HP_Clear();
    for(int i=0;i<17;++i) { vkDestroyPipeline(vulkan_globals.device,hp_compute[i],NULL); hp_compute[i]=VK_NULL_HANDLE; }
    vkDestroyPipelineLayout(vulkan_globals.device,hp_death_layout,NULL);hp_death_layout=VK_NULL_HANDLE;
    vkDestroyDescriptorSetLayout(vulkan_globals.device,hp_mesh_layout,NULL);hp_mesh_layout=VK_NULL_HANDLE;
    vkDestroyPipelineLayout(vulkan_globals.device,hp_compute_layout,NULL); hp_compute_layout=VK_NULL_HANDLE;
    vkDestroyPipelineLayout(vulkan_globals.device,vulkan_globals.holo_world_layout.handle,NULL);
    memset(&vulkan_globals.holo_world_layout,0,sizeof(vulkan_globals.holo_world_layout));
    vkDestroyDescriptorSetLayout(vulkan_globals.device,hp_set_layout,NULL);hp_set_layout=VK_NULL_HANDLE;
}
static int HP_SurfaceSort(const void *a,const void *b) {
    uint32_t x=((const uint32_t*)a)[12],y=((const uint32_t*)b)[12]; return (x>y)-(x<y);
}
static void HP_Barrier(VkCommandBuffer cb) {
    VkMemoryBarrier b={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_INDIRECT_COMMAND_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_TRANSFER_READ_BIT};
    vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,1,&b,0,NULL,0,NULL);
}
static void HP_Dispatch(VkCommandBuffer cb,int pass,hp_push_t *p,uint32_t groups) {
    vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_COMPUTE,hp_compute[pass]);
    vkCmdPushConstants(cb,hp_compute_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(*p),p);
    if(pass==3) vkCmdDispatchIndirect(cb,hp_buffers[2],113*4);
    else if(pass==2 || pass==8) vkCmdDispatchIndirect(cb,hp_buffers[2],16);
    else vkCmdDispatch(cb,q_min(groups,65535u),(groups+65534u)/65535u,1);
    HP_Barrier(cb);
}
static void HP_Allocate(VkCommandBuffer cb,uint32_t capacity) {
    // Surface metadata only; home positions are reconstructed on the GPU.
    hp_capacity=capacity; hp_sources=(uint32_t)((as_static_sample_bytes+as_scatter_bytes)/4);
    hp_surface_count=0; hp_surfaces=Mem_Alloc(as_model->nummodelsurfaces*6*sizeof(*hp_surfaces));
    for(int i=0;i<as_model->nummodelsurfaces;++i) {
        as_face_t *f=&as_faces[i]; if(!f->count) continue;
        for(int layer=-1;layer<5;++layer) {
            if(layer==4? !f->architectural_count : layer>=0 && !f->scatter_count[layer]) continue;
            memcpy(hp_surfaces[hp_surface_count].origin,f->gpu.origin,16);
            memcpy(hp_surfaces[hp_surface_count].s,f->gpu.saxis,16);
            memcpy(hp_surfaces[hp_surface_count].t,f->gpu.taxis,16);
            msurface_t *surface=&as_model->surfaces[i];
            for(int axis=0;axis<3;++axis) { hp_surfaces[hp_surface_count].mins[axis]=FLT_MAX;hp_surfaces[hp_surface_count].maxs[axis]=-FLT_MAX; }
            for(int edge=0;edge<surface->numedges;++edge) {
                int index=as_model->surfedges[surface->firstedge+edge];
                const float *v=as_model->vertexes[as_model->edges[abs(index)].v[index<0?1:0]].position;
                for(int axis=0;axis<3;++axis) {
                    hp_surfaces[hp_surface_count].mins[axis]=q_min(hp_surfaces[hp_surface_count].mins[axis],v[axis]-2);
                    hp_surfaces[hp_surface_count].maxs[axis]=q_max(hp_surfaces[hp_surface_count].maxs[axis],v[axis]+2);
                }
            }
            float sign=(surface->flags&SURF_PLANEBACK)?-1.f:1.f;
            hp_surfaces[hp_surface_count].origin[3]=surface->plane->normal[0]*sign;
            hp_surfaces[hp_surface_count].s[3]=surface->plane->normal[1]*sign;
            hp_surfaces[hp_surface_count].t[3]=surface->plane->normal[2]*sign;
            hp_surfaces[hp_surface_count].range[0]=(uint32_t)(layer<0?f->offset/4:as_static_sample_bytes/4+(layer==4?f->architectural_offset:f->scatter_offset[layer])/4);
            hp_surfaces[hp_surface_count].range[1]=layer<0?f->count:layer==4?f->architectural_count:f->scatter_count[layer]; ++hp_surface_count;
        }
    }
    qsort(hp_surfaces,hp_surface_count,sizeof(*hp_surfaces),HP_SurfaceSort);
    hp_promote_groups=0;
    for(uint32_t i=0;i<hp_surface_count;++i) {
        hp_surfaces[i].range[2]=hp_promote_groups;
        hp_surfaces[i].range[3]=(hp_surfaces[i].range[1]+255)/256;
        hp_promote_groups+=hp_surfaces[i].range[3];
    }
    size_t sizes[12]={capacity*48ull,hp_sources*16ull,(128ull+capacity*3ull+65536ull+hp_promote_groups)*4,((hp_sources+31ull)/32)*4,
        64*sizeof(hp_impulse_t),hp_sources*4ull,hp_surface_count*sizeof(*hp_surfaces),capacity*16ull,HP_GRID_CAPACITY*32ull,65536ull*48,32*sizeof(hp_death_job_t),4*48};
    for(int i=0;i<12;++i) { hp_sizes[i]=sizes[i];
        R_CreateBuffer(&hp_buffers[i],&hp_memory[i],sizes[i],VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,0,NULL,NULL,"Holo Physics"); }
    R_StagingUploadBuffer(hp_buffers[6],sizes[6],(const byte*)hp_surfaces); Mem_Free(hp_surfaces); hp_surfaces=NULL;
    VkDescriptorPoolSize poolsize={VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,76};
    VkDescriptorPoolCreateInfo pool={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,.maxSets=65,.poolSizeCount=1,.pPoolSizes=&poolsize};
    HP_Check(vkCreateDescriptorPool(vulkan_globals.device,&pool,NULL,&hp_pool));
    VkDescriptorSetAllocateInfo alloc={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,.descriptorPool=hp_pool,.descriptorSetCount=1,.pSetLayouts=&hp_set_layout};
    HP_Check(vkAllocateDescriptorSets(vulkan_globals.device,&alloc,&hp_set));
    alloc.pSetLayouts=&hp_mesh_layout;
    for(int frame=0;frame<2;++frame) for(int j=0;j<32;++j) HP_Check(vkAllocateDescriptorSets(vulkan_globals.device,&alloc,&hp_mesh_sets[frame][j]));
    for(int i=0;i<12;++i) { VkDescriptorBufferInfo bi={hp_buffers[i],0,sizes[i]};
        VkWriteDescriptorSet w={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=hp_set,.dstBinding=i,.descriptorCount=1,
            .descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&bi}; vkUpdateDescriptorSets(vulkan_globals.device,1,&w,0,NULL); }
    VkBufferCopy copy={0,0,as_static_sample_bytes}; vkCmdCopyBuffer(cb,as_sample_buffer,hp_buffers[5],1,&copy);
    copy=(VkBufferCopy){0,as_static_sample_bytes,as_scatter_bytes}; if(copy.size) vkCmdCopyBuffer(cb,as_scatter_buffer,hp_buffers[5],1,&copy);
    HP_Barrier(cb);
    vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,hp_compute_layout,0,1,&hp_set,0,NULL);
    hp_push_t p={capacity,hp_sources,hp_surface_count,0,0,0,1.2f,0,0,0,.015f,.05f};
    float span=0;
    for(int i=0;i<3;++i) { hp_grid_origin_cell[i]=floorf(as_model->mins[i]/16)*16-16;span=q_max(span,as_model->maxs[i]-hp_grid_origin_cell[i]+16); }
    hp_grid_origin_cell[3]=q_max(16,ceilf(span/1024));
    memcpy(p.grid_origin_cell,hp_grid_origin_cell,16);p.grid_capacity=HP_GRID_CAPACITY;
    HP_Dispatch(cb,5,&p,(hp_sources+63)/64);
    if(vulkan_globals.device_properties.limits.timestampComputeAndGraphics) {
        VkQueryPoolCreateInfo query={.sType=VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,.queryType=VK_QUERY_TYPE_TIMESTAMP,.queryCount=12};
        HP_Check(vkCreateQueryPool(vulkan_globals.device,&query,NULL,&hp_queries));
        vkCmdResetQueryPool(cb,hp_queries,0,12);
        vkCmdWriteTimestamp(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,hp_queries,hp_query_frame*6+2);
    }
    HP_Dispatch(cb,6,&p,HP_GRID_CAPACITY/64);
    HP_Dispatch(cb,7,&p,(hp_sources+63)/64);
    HP_Dispatch(cb,13,&p,HP_GRID_CAPACITY/64);
    if(hp_queries) { vkCmdWriteTimestamp(cb,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,hp_queries,hp_query_frame*6+3);hp_query_ready[hp_query_frame][1]=true; }
    HP_Dispatch(cb,0,&p,(q_max(q_max(capacity*2,65536u),(hp_sources+31)/32)+63)/64);
    hp_input=0;hp_initialized=true;
    for(int i=0;i<2;++i) {
        R_CreateBuffer(&hp_readback[i],&hp_read_memory[i],128,VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,0,NULL,NULL,"Holo counters");
        HP_Check(vkMapMemory(vulkan_globals.device,hp_read_memory[i].handle,0,128,0,(void**)&hp_read_words[i]));
    }
}
static void HP_Frame(void) {
    if(COM_CheckParm("-test-holo-lifecycle")) {
        static uint32_t frame;++frame;
        if(frame==60) Cbuf_AddText("save holo-validation\n");
        if(frame==80 || frame==130 || frame==230) Cbuf_AddText("holo_physics_sphere 320\n");
        if(frame==110) Cbuf_AddText("load holo-validation\n");
        if(frame==160) Cbuf_AddText("disconnect\n");
        if(frame==175) Cbuf_AddText("map e1m2\n");
        if(frame==360) Cbuf_AddText("quit\n");
    }
    // Disconnection and save/demo time rewinds retire renderer resources only
    // after the previous submission is finished. No client state is changed.
    if(hp_initialized && (cls.signon!=SIGNONS || cl.worldmodel!=as_model || cl.time<hp_previous_time)) {
        GL_SynchronizeEndRenderingTask();HP_Clear();hp_previous_time=cl.time;
    }
    if(COM_CheckParm("-holo-verify") && cls.demoplayback && cls.signon==SIGNONS) {
        uint64_t rng=COM_RandState(),hash=UINT64_C(1469598103934665603);
        hash=AS_Hash(hash,&cl.time,sizeof(cl.time));hash=AS_Hash(hash,cl.stats,sizeof(cl.stats));
        for(int i=1;i<cl.num_entities;++i) {
            const entity_t *e=&cl.entities[i];
            hash=AS_Hash(hash,e->msg_origins,sizeof(e->msg_origins));
            hash=AS_Hash(hash,e->msg_angles,sizeof(e->msg_angles));
            hash=AS_Hash(hash,&e->frame,sizeof(e->frame));hash=AS_Hash(hash,&e->msgtime,sizeof(e->msgtime));
            if(e->model) hash=AS_Hash(hash,e->model->name,strlen(e->model->name));
        }
        Con_Printf("Holo demo state %.6f %016llx rng=%016llx\n",cl.time,(unsigned long long)hash,(unsigned long long)rng);
    }
    if(COM_CheckParm("-test-holo-menu") || COM_CheckParm("-test-holo-pause")) {
        static uint32_t frame; ++frame;
        if(frame==40 && COM_CheckParm("-test-holo-menu")) Cbuf_AddText("menu_holo_physics\n");
        if((frame==40 || frame==80) && COM_CheckParm("-test-holo-pause")) Cbuf_AddText("toggleconsole\n");
        if(frame==65 && COM_CheckParm("-test-holo-menu")) Cbuf_AddText("screenshot png 90 \"holo-menu.png\"\n");
        if(COM_CheckParm("-test-holo-pause") && (frame==45 || frame==75 || frame==100))
            Con_Printf("Holo pause frame=%u clock=%.6f dt=%.6f\n",frame,hp_clock,hp_dt);
    }
    if(COM_CheckParm("-test-holo-cycle")) {
        static uint32_t frame; ++frame;
        if(frame==50) Cvar_SetValueQuick(&hp_budget,2048);
        if(frame==80) Cvar_SetValueQuick(&hp_gore,0);
        if(frame==100) Cvar_SetValueQuick(&hp_gore,1);
        if(frame==120) Cvar_SetValueQuick(&hp_enabled,0);
        if(frame==140) Cvar_SetValueQuick(&hp_enabled,1);
        if(frame==160) Cvar_SetValueQuick(&hp_holo,0);
        if(frame==180) Cvar_SetValueQuick(&hp_holo,1);
        if(frame==200) HP_Defaults_f();
        if(frame%20==0) Con_Printf("Holo live controls frame=%u enabled=%.0f renderer=%.0f budget=%.0f gore=%.0f\n",frame,hp_enabled.value,as_renderer.value,hp_budget.value,hp_gore.value);
    }
    double delta=cl.time-hp_previous_time;
    if(delta<0) hp_reset_pending=true;
    hp_previous_time=cl.time;
    hp_dt=(!hp_enabled.value || !as_renderer.value || cl.paused || (cls.demoplayback && cls.demospeed==0) || key_dest==key_console || cls.signon!=SIGNONS)?0:(float)CLAMP(0,delta,.05);
    hp_clock+=hp_dt;
    if(hp_dt>0 && hp_enabled.value && as_renderer.value) {
        for(int i=1;i<q_min(cl.num_entities,8192);++i) {
            const entity_t *e=&cl.entities[i];if(!e->model) { hp_observed[i].valid=false;continue; }
            const char *name=e->model->name;
            qboolean projectile=strstr(name,"missile.mdl") || strstr(name,"grenade.mdl") || strstr(name,"spike.mdl") || strstr(name,"gib") || strstr(name,"/h_");
            qboolean liquid=false;int contents=CONTENTS_EMPTY;
            if(i!=cl.viewentity && e->model->type==mod_alias && cl.worldmodel) {
                // Renderer BSP classification observes a transition; it is not
                // used to move, collide, or otherwise modify this entity.
                vec3_t sample;VectorCopy(e->origin,sample);
                contents=Mod_PointInLeaf(sample,cl.worldmodel)->contents;
                liquid=contents==CONTENTS_WATER || contents==CONTENTS_SLIME || contents==CONTENTS_LAVA;
            }
            if(hp_observed[i].valid && hp_observed[i].model==e->model) {
                vec3_t movement;VectorSubtract(e->origin,hp_observed[i].position,movement);float distance=VectorLength(movement);
                if(projectile && distance>.5f && distance<256) HP_Event(HP_WAKE,hp_observed[i].position,e->origin,18,35*HP_Value(&hp_wake,0,4,.7f));
                if(i!=cl.viewentity && liquid!=hp_observed[i].liquid && distance>0 && distance<128)
                    HP_EventSlot(HP_SPLASH,e->origin,NULL,48,40*HP_Value(&hp_liquid,0,4,1),contents==CONTENTS_LAVA?HP_HOT:HP_COOL);
                if(i==cl.viewentity && hp_clock>=hp_next_player_wake && distance>2 && distance<32 && fabsf(movement[2])<1) {
                    hp_next_player_wake=hp_clock+.08f;
                    vec3_t feet,end;VectorCopy(e->origin,feet);feet[2]-=24;VectorCopy(feet,end);end[0]+=.01f;
                    HP_Event(HP_WAKE,feet,end,24,12*HP_Value(&hp_player_wake,0,4,.5f)*q_min(1,distance/5));
                }
            }
            hp_observed[i].model=e->model;VectorCopy(e->origin,hp_observed[i].position);hp_observed[i].valid=true;hp_observed[i].liquid=liquid;
        }
        if(cl.inwater!=hp_was_inwater && cl.viewentity<cl.num_entities) HP_EventSlot(HP_SPLASH,cl.entities[cl.viewentity].origin,NULL,72,50*HP_Value(&hp_liquid,0,4,1),HP_COOL);
        hp_was_inwater=cl.inwater;
    }
    if(COM_CheckParm("-test-holo") && hp_clock>.25f && !hp_test_fired) {
        hp_test_fired=true; vec3_t point; VectorCopy(cl.entities[cl.viewentity].origin,point); point[2]+=24;
        if(COM_CheckParm("-test-holo-gore")) { vec3_t forward,right,up;AngleVectors(cl.viewangles,forward,right,up);VectorMA(point,120,forward,point); }
        int arg=COM_CheckParm("-test-holo-event");
        int type=arg && arg+1<com_argc?CLAMP(0,atoi(com_argv[arg+1]),HP_DEBUG):COM_CheckParm("-test-holo-gore")?HP_BLOOD:COM_CheckParm("-test-holo-blast")?HP_SHOCKWAVE:HP_DEBUG;
        vec3_t end;VectorCopy(point,end);end[0]+=160;
        HP_Event(type,point,(type==HP_BEAM || type==HP_WAKE)?end:NULL,320,360);
        Con_Printf("Holo Physics debug impulse at %.3f\n",hp_clock);
    }
    // Preserve wavefronts through suction; paused events do not accumulate.
    uint32_t n=0;
    for(uint32_t i=0;i<hp_events_count;++i) if(hp_clock-hp_events[i].birth_time<((hp_events[i].type==HP_SHOCKWAVE || hp_events[i].type==HP_IMPLODE)?.45f:.0001f)) hp_events[n++]=hp_events[i];
    hp_events_count=n;
    hp_impulse_t incoming[64]; n=HP_QueueDrain(&hp_pending,incoming); hp_new_events=n;
    for(uint32_t i=0;i<n;++i) {
        incoming[i].birth_time=hp_clock;
        if(incoming[i].type==HP_SHOCKWAVE) for(int light=0;light<MAX_DLIGHTS;++light) {
            const dlight_t *dl=&cl_dlights[light];vec3_t light_delta;VectorSubtract(dl->origin,incoming[i].origin,light_delta);
            if(dl->die>=cl.time && DotProduct(light_delta,light_delta)<16 && dl->radius>0)
                incoming[i].radius=q_max(incoming[i].radius,q_min(incoming[i].radius*1.5f,dl->radius*.55f*HP_Value(&hp_radius,.1f,4,1)));
        }
        HP_TrackBounds(&incoming[i]);
        if(hp_events_count<64) hp_events[hp_events_count++]=incoming[i];
        else { uint32_t weakest=0; for(uint32_t j=1;j<64;++j) if(hp_events[j].strength<hp_events[weakest].strength) weakest=j;
            if(incoming[i].strength>hp_events[weakest].strength) hp_events[weakest]=incoming[i]; }
        hp_awake_until=hp_clock+incoming[i].duration+5;
        if(incoming[i].type==HP_BLOOD || incoming[i].type==HP_DEATH) hp_gore_until=hp_clock+93;
        if(incoming[i].type==HP_BLOOD || incoming[i].type==HP_DEATH || incoming[i].type==HP_SPLASH || incoming[i].type==HP_IMPLODE) hp_light_until=q_max(hp_light_until,hp_clock+3);
        if(incoming[i].type==HP_SPLASH || incoming[i].type==HP_IMPLODE) hp_gore_until=q_max(hp_gore_until,hp_clock+incoming[i].duration+3);
    }
}
void HP_Run(VkCommandBuffer cb,int frame_index) {
    int profile_arg=COM_CheckParm("-holo-profile");
    if(!hp_profile && profile_arg>0 && profile_arg+1<com_argc) {
        hp_profile=fopen(com_argv[profile_arg+1],"w");if(hp_profile) fputs("clock,active,budget,compute_ms,gore_ms,total_gpu_ms\n",hp_profile);
    }
    hp_query_frame=frame_index;
    if(hp_queries) for(int timer=0;timer<3;++timer) if(hp_query_ready[frame_index][timer]) {
        uint64_t stamp[2];if(vkGetQueryPoolResults(vulkan_globals.device,hp_queries,frame_index*6+timer*2,2,sizeof(stamp),stamp,8,VK_QUERY_RESULT_64_BIT)==VK_SUCCESS) {
            float ms=(float)((double)(stamp[1]-stamp[0])*vulkan_globals.device_properties.limits.timestampPeriod/1e6);
            if(timer==0) { hp_compute_ms=ms;
                if(hp_profile) fprintf(hp_profile,"%.4f,%u,%u,%.5f,%.5f,%.5f\n",hp_clock,hp_active_count,hp_capacity,hp_compute_ms,hp_gore_ms,rs_gputime_us/1000.);
            } else if(timer==1) hp_grid_ms=ms;else hp_gore_ms=ms;
        } hp_query_ready[frame_index][timer]=false;
    }
    hp_draw_enabled=false;hp_gore_draw_enabled=false;
    if(!hp_enabled.value || !as_renderer.value || cls.signon!=SIGNONS || cl.worldmodel!=as_model || !as_faces || !as_static_sample_bytes) return;
    uint32_t capacity=(uint32_t)HP_Value(&hp_budget,HP_MIN_BUDGET,HP_MAX_BUDGET,65536);
    if(hp_initialized && capacity!=hp_capacity) HP_Clear();
    if(!hp_initialized) HP_Allocate(cb,capacity);
    HP_GibDeaths();
    uint32_t deaths=q_min(32u,Atomic_LoadUInt32(&hp_death_pending));
    Atomic_StoreUInt32(&hp_death_pending,0);
    if(deaths) {
        for(uint32_t i=0;i<deaths;++i) { hp_impulse_t e={0};VectorCopy(hp_death_jobs[i].model+12,e.origin);e.type=HP_DEATH;e.duration=hp_death_jobs[i].parameters[2];HP_TrackBounds(&e); }
        if(hp_debug.value) Con_Printf("Holo GPU death snapshots: %u\n",deaths);
        hp_awake_until=hp_clock+HP_Value(&hp_death_life,.2f,4,1.2f)+5;
        hp_gore_until=hp_clock+94;
        hp_light_until=hp_clock+HP_Value(&hp_death_life,.2f,4,1.2f)*1.25f;
    }
    if(hp_read_ready[frame_index]) {
        uint32_t *stats=hp_read_words[frame_index]; hp_active_count=stats[stats[10]&1u];hp_overflow_count=stats[3];hp_stuck_count=stats[21];hp_evictions=stats[7];
        static int last_tick=-1;
        if((hp_debug.value || COM_CheckParm("-test-holo")) && last_tick!=as_tick && as_tick%30==0) {
            last_tick=as_tick; Con_Printf("Holo GPU: active=%u free=%u promoted=%u dropped=%u stains=%u evicted=%u time=%.3f\n",hp_active_count,stats[2],stats[8],stats[3],stats[21],stats[7],hp_clock);
        }
    }
    hp_gore_draw_enabled=hp_clock<hp_gore_until;
    if(hp_queries) { vkCmdResetQueryPool(cb,hp_queries,frame_index*6+4,2); }

    if(!hp_reset_pending && deaths==0 && hp_events_count==0 && hp_active_count==0 && hp_clock>.5f) {
        if(hp_gore_draw_enabled) {
            HP_Barrier(cb);uint32_t zero=0;vkCmdUpdateBuffer(cb,hp_buffers[2],84,4,&zero);
            vkCmdUpdateBuffer(cb,hp_buffers[2],48,4,&hp_clock);
            uint32_t style=AS_Neon()?14:(uint32_t)HP_Value(&as_style,0,13,1);vkCmdUpdateBuffer(cb,hp_buffers[2],60,4,&style);HP_Barrier(cb);
            hp_push_t idle={0};idle.budget=hp_capacity;idle.time=hp_clock;
            idle.gore_cap=(uint32_t)HP_Value(&hp_gore_max,0,65536,20000);idle.gore_enabled=hp_gore.value!=0;idle.gore_life=HP_Value(&hp_gore_life,1,90,45);
            vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,hp_compute_layout,0,1,&hp_set,0,NULL);
            HP_Dispatch(cb,14,&idle,1024);
            VkBufferCopy copy={0,0,128};HP_Barrier(cb);vkCmdCopyBuffer(cb,hp_buffers[2],hp_readback[frame_index],1,&copy);hp_read_ready[frame_index]=true;
        }
        hp_awake_until=0; hp_compute_ms=0;return;
    }
    HP_Barrier(cb);
    vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,hp_compute_layout,0,1,&hp_set,0,NULL);
    hp_push_t p={hp_capacity,hp_sources,hp_events_count,hp_input,hp_dt,hp_clock,
        HP_Value(&hp_settle,.1f,5,1.2f),HP_Value(&hp_turbulence,0,8,.6f),HP_Value(&hp_suction,0,1,.35f),0,.015f,.05f};
    p.pad0=(float)hp_new_events;
    p.spray_count=HP_Value(&hp_spray_count,0,200,96); p.point_step=HP_Value(&as_density,.25f,16,4);
    memcpy(p.grid_origin_cell,hp_grid_origin_cell,16);p.grid_capacity=HP_GRID_CAPACITY;
    p.collide=hp_collide.value!=0;p.restitution=HP_Value(&hp_restitution,0,1,.25f);
    p.turbulence_scale=HP_Value(&hp_turbulence_scale,8,512,48);
    p.gore_cap=(uint32_t)HP_Value(&hp_gore_max,0,65536,20000);p.gore_enabled=hp_gore.value!=0;
    p.gore_life=HP_Value(&hp_gore_life,1,90,45);p.gore_gravity=HP_Value(&hp_gore_gravity,0,4,1);
    p.dust=HP_Value(&hp_dust,0,2,.8f);p.beam_crawl=.15f;p.death_enabled=hp_death.value>0;p.shake=HP_Value(&hp_shake,0,1,.4f);
    if(hp_reset_pending) { HP_Dispatch(cb,0,&p,(q_max(q_max(hp_capacity*2,65536u),(hp_sources+31)/32)+63)/64);
        hp_input=0;p.input=0;hp_reset_pending=false; hp_events_count=0;p.events=0;hp_awake_until=0;
        hp_active_count=hp_stuck_count=0;memset(hp_read_ready,0,sizeof(hp_read_ready)); }
    if(hp_clock>=hp_awake_until) return;
    if(hp_events_count) { vkCmdUpdateBuffer(cb,hp_buffers[4],0,hp_events_count*sizeof(hp_impulse_t),hp_events); HP_Barrier(cb); }
    if(cl.viewentity<cl.num_entities) { vkCmdUpdateBuffer(cb,hp_buffers[2],96,12,cl.entities[cl.viewentity].origin);HP_Barrier(cb); }
    if(hp_queries) { vkCmdResetQueryPool(cb,hp_queries,frame_index*6,2);vkCmdWriteTimestamp(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,hp_queries,frame_index*6); }
    HP_Dispatch(cb,1,&p,2);
    if(hp_new_events) { HP_Dispatch(cb,8,&p,0); HP_Dispatch(cb,9,&p,1); }
    HP_Dispatch(cb,2,&p,0);
    if(hp_events_count) {
        qboolean world=false,spawn=false;
        for(uint32_t i=0;i<hp_events_count;++i) {
            uint32_t type=hp_events[i].type;
            float age=hp_clock-hp_events[i].birth_time;
            world|=type!=HP_BLOOD && type!=HP_DEATH && (type==HP_SHOCKWAVE?age<=.12f+hp_dt:age<=.0001f);
            spawn|=type==HP_BLOOD || type==HP_DEATH || type==HP_SPLASH || type==HP_IMPLODE;
        }
        if(world) { p.pad1=(float)hp_surface_count;HP_Dispatch(cb,15,&p,(hp_surface_count+63)/64);HP_Dispatch(cb,16,&p,1);HP_Dispatch(cb,3,&p,0); }
        if(spawn) HP_Dispatch(cb,10,&p,hp_events_count);
    }
    if(deaths) {
        vkCmdUpdateBuffer(cb,hp_buffers[10],0,deaths*sizeof(hp_death_job_t),hp_death_jobs);HP_Barrier(cb);
        for(uint32_t j=0;j<deaths;++j) {
            VkDescriptorBufferInfo mesh={hp_death_meshes[j],0,VK_WHOLE_SIZE};
            VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=hp_mesh_sets[frame_index][j],.dstBinding=0,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&mesh};
            vkUpdateDescriptorSets(vulkan_globals.device,1,&write,0,NULL);
            vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,hp_death_layout,1,1,&hp_mesh_sets[frame_index][j],0,NULL);
            p.pad1=(float)j;HP_Dispatch(cb,11,&p,(q_min(hp_death_jobs[j].info[0],hp_capacity/8)+32+63)/64);
        }
    }
    HP_Dispatch(cb,4,&p,1);
    if(hp_gore_draw_enabled) HP_Dispatch(cb,14,&p,1024);
    hp_input=1-hp_input;hp_draw_enabled=true;
    uint32_t style=AS_Neon()?14:(uint32_t)HP_Value(&as_style,0,13,1);
    vkCmdUpdateBuffer(cb,hp_buffers[2],60,4,&style);HP_Barrier(cb);
    uint32_t light_count=0;vkCmdUpdateBuffer(cb,hp_buffers[2],108,4,&light_count);HP_Barrier(cb);
    if(hp_debris_light.value && hp_clock<hp_light_until) { p.input=1-hp_input;HP_Dispatch(cb,12,&p,1); }
    if(hp_queries) { vkCmdWriteTimestamp(cb,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,hp_queries,frame_index*6+1);hp_query_ready[frame_index][0]=true; }
    VkBufferCopy stats_copy={0,0,128};
    VkMemoryBarrier transfer={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT,.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT};
    vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&transfer,0,NULL,0,NULL);
    vkCmdCopyBuffer(cb,hp_buffers[2],hp_readback[frame_index],1,&stats_copy);hp_read_ready[frame_index]=true;
}
static qboolean HP_Bind(cb_context_t *cbx,VkBuffer sample_buffer,size_t offset) {
    if(!hp_draw_enabled || (sample_buffer!=as_sample_buffer && sample_buffer!=as_scatter_buffer)) return false;
    uint32_t id=(uint32_t)(offset/4+(sample_buffer==as_scatter_buffer?as_static_sample_bytes/4:0));
    vkCmdBindDescriptorSets(cbx->cb,VK_PIPELINE_BIND_POINT_GRAPHICS,vulkan_globals.holo_world_layout.handle,5,1,&hp_set,0,NULL);
    vkCmdPushConstants(cbx->cb,vulkan_globals.holo_world_layout.handle,VK_SHADER_STAGE_ALL_GRAPHICS,84,4,&id);
    return true;
}
static qboolean HP_SurfaceActive(const as_face_t *face) {
    if(!hp_draw_enabled) return false;
    if(hp_clock<hp_bounds_overflow_until) return true;
    for(uint32_t i=0;i<256;++i) if(hp_draw_bounds[i].until>hp_clock) {
        int k;
        for(k=0;k<3;++k) if(face->bounds_max[k]<hp_draw_bounds[i].mins[k] || face->bounds_min[k]>hp_draw_bounds[i].maxs[k]) break;
        if(k==3) return true;
    }
    return false;
}


static void HP_DrawGore(cb_context_t *cbx) {
    if(!hp_gore_draw_enabled || !hp_set) return;
    if(hp_queries) vkCmdWriteTimestamp(cbx->cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,hp_queries,hp_query_frame*6+4);
    R_BindPipeline(cbx,VK_PIPELINE_BIND_POINT_GRAPHICS,vulkan_globals.holo_gore[cbx->pipeline_variant]);
    vkCmdBindDescriptorSets(cbx->cb,VK_PIPELINE_BIND_POINT_GRAPHICS,vulkan_globals.holo_world_layout.handle,5,1,&hp_set,0,NULL);
    for(uint32_t mode=0;mode<2;++mode) {
        if(mode==1 && !hp_gore.value) continue;
        vkCmdPushConstants(cbx->cb,vulkan_globals.holo_world_layout.handle,VK_SHADER_STAGE_ALL_GRAPHICS,84,4,&mode);
        vkCmdDrawIndirect(cbx->cb,hp_buffers[2],mode?80:64,1,16);
    }
    if(hp_queries) { vkCmdWriteTimestamp(cbx->cb,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,hp_queries,hp_query_frame*6+5);hp_query_ready[hp_query_frame][2]=true; }
}

qboolean HP_ObserveAlias(entity_t *entity,aliashdr_t *hdr,const float *matrix,float blend,uint32_t pose1,uint32_t pose2) {
    if(!hp_enabled.value || !as_renderer.value || hp_death.value<=0 || !hdr->aftershock_samples || hdr->poseverttype!=PV_QUAKE1) return false;
    if(entity==&cl.viewent || entity<cl.entities || entity>=cl.entities+cl.num_entities) return false;
    size_t owner=(size_t)(entity-cl.entities);if(owner>=8192 || entity->frame<0 || entity->frame>=hdr->numframes) return false;
    if(strstr(entity->model->name,"gib") || strstr(entity->model->name,"/h_")) return false;
    const char *name=hdr->frames[entity->frame].name;
    qboolean dead=!strncmp(name,"death",5) || !strncmp(name,"die",3);
    if(hp_dt<=0) return dead && hp_dead_owners[owner].model==entity->model && hp_dead_owners[owner].dead && AS_GoreReplacedDeath(entity);
    if(hp_dead_owners[owner].model!=entity->model) { hp_dead_owners[owner].model=entity->model;hp_dead_owners[owner].dead=false; }
    hp_death_job_t snapshot={0};memcpy(snapshot.model,matrix,64);
    snapshot.mesh[0]=pose1;snapshot.mesh[1]=pose2;snapshot.mesh[2]=(uint32_t)hdr->vbostofs/4;snapshot.mesh[3]=hdr->aftershock_offset;
    snapshot.parameters[0]=blend;snapshot.parameters[1]=95*HP_Value(&hp_death,0,8,1);snapshot.parameters[2]=HP_Value(&hp_death_life,.2f,4,1.2f);
    snapshot.info[0]=hdr->aftershock_samples;snapshot.info[1]=HP_Hash((uint32_t)owner^(uint32_t)(cl.time*1000));
    if(!dead) {
        hp_dead_owners[owner].dead=false;hp_last_alias[owner].job=snapshot;hp_last_alias[owner].mesh=hdr->vertex_buffer;
        hp_last_alias[owner].model=entity->model;hp_last_alias[owner].seen=hp_clock;return false;
    }
    if(hp_dead_owners[owner].dead) return AS_GoreReplacedDeath(entity);
    uint32_t job=Atomic_AddUInt32(&hp_death_pending,1);if(job>=32) return false;
    hp_death_jobs[job]=snapshot;hp_death_meshes[job]=hdr->vertex_buffer;hp_dead_owners[owner].dead=true;
    return false;
}
static void HP_GibDeaths(void) {
    if(hp_dt<=0 || !hp_enabled.value || !as_renderer.value || hp_death.value<=0) return;
    for(int i=1;i<q_min(cl.num_entities,8192);++i) {
        const entity_t *gib=&cl.entities[i];
        if(!gib->model) { hp_gib_seen[i]=NULL;continue; }
        if(!strstr(gib->model->name,"gib") && !strstr(gib->model->name,"/h_")) continue;
        if(hp_gib_seen[i]==gib->model) continue;hp_gib_seen[i]=gib->model;
        int best=-1;float distance=96*96;
        for(int owner=1;owner<q_min(cl.num_entities,8192);++owner) {
            if(!hp_last_alias[owner].model || hp_dead_owners[owner].dead || hp_clock-hp_last_alias[owner].seen>.25f) continue;
            const entity_t *old=&cl.entities[owner];
            if(old->model==hp_last_alias[owner].model && old->msgtime==cl.mtime[0]) continue;
            vec3_t delta;VectorSubtract(gib->origin,hp_last_alias[owner].job.model+12,delta);float d=DotProduct(delta,delta);
            if(d<distance) { distance=d;best=owner; }
        }
        if(best<0) continue;
        // Validate the renderer resource before using a disappearing actor's mesh.
        aliashdr_t *hdr=(aliashdr_t*)Mod_Extradata(hp_last_alias[best].model);
        if(!hdr || hdr->vertex_buffer!=hp_last_alias[best].mesh) continue;
        uint32_t job=Atomic_AddUInt32(&hp_death_pending,1);if(job>=32) continue;
        hp_death_jobs[job]=hp_last_alias[best].job;hp_death_jobs[job].parameters[1]*=2.4f;
        hp_death_jobs[job].parameters[2]*=1.25f;hp_death_meshes[job]=hp_last_alias[best].mesh;
        hp_dead_owners[best].dead=true;
    }
}

void HP_Beam(const vec3_t start,const vec3_t end) {
    HP_Event(HP_BEAM,start,end,HP_Value(&hp_beam_radius,4,128,24)*(cl.inwater?2.5f:1),25*HP_Value(&hp_beam,0,8,1));
}

int HP_CopyLights(VkCommandBuffer cb,VkBuffer target,VkDeviceSize offset,int available) {
    if(!hp_enabled.value || !as_renderer.value || !hp_debris_light.value || hp_clock>=hp_light_until || !hp_draw_enabled || available<=0) return 0;
    int count=q_min(available,hp_capacity>=131072?4:2);
    vkCmdUpdateBuffer(cb,hp_buffers[2],108,4,&count);
    VkMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT,.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT};
    vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&barrier,0,NULL,0,NULL);
    VkBufferCopy copy={0,offset,(VkDeviceSize)count*48};vkCmdCopyBuffer(cb,hp_buffers[11],target,1,&copy);
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_UNIFORM_READ_BIT|VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,1,&barrier,0,NULL,0,NULL);
    return count;
}

void HP_DrawDebug(cb_context_t *cbx) {
    if(!hp_enabled.value || !as_renderer.value || !hp_debug.value) return;
    GL_SetCanvas(cbx,CANVAS_TOPLEFT);
    Draw_String(cbx,8,32,va("Holo Physics %u / %u",hp_active_count,hp_capacity));
    Draw_String(cbx,8,40,va("Stuck gore %u / %.0f",hp_stuck_count,HP_Value(&hp_gore_max,0,65536,20000)));
    Draw_String(cbx,8,48,va("Impulses %u  evictions %u",hp_new_events,hp_evictions));
    Draw_String(cbx,8,56,va("Compute %.3f ms  gore %.3f ms",hp_compute_ms,hp_gore_ms));
    Draw_String(cbx,8,64,va("Grid build %.3f ms (map load)",hp_grid_ms));
}
