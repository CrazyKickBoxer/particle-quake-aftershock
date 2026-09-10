/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
/* Opt-in offline showcase choreography. Normal play never enters these paths. */
static vec3_t as_show_origin, as_show_angles, as_show_target;
static int as_show_start=-1;
static int as_demo_capture_frame=-1;
int AS_ShowcaseTick(void) { if(COM_CheckParm("-capture-demo") && cls.demoplayback && cls.signon==SIGNONS)return as_demo_capture_frame; return COM_CheckParm("-showcase") && cls.signon==SIGNONS && as_show_start>=0 ? as_tick-as_show_start : -1; }
static int AS_ShowcaseScene(void)
{
    int p=COM_CheckParm("-showcase");
    return p && p+1<com_argc ? CLAMP(1,atoi(com_argv[p+1]),6) : 0;
}
static void AS_ShowcaseAim(const vec3_t from,const vec3_t to,vec3_t angles)
{
    vec3_t d;VectorSubtract(to,from,d);
    angles[0]=-atan2f(d[2],sqrtf(d[0]*d[0]+d[1]*d[1]))*180.f/M_PI;
    angles[1]=atan2f(d[1],d[0])*180.f/M_PI;angles[2]=0;
}
static qboolean AS_ShowcaseInput(void)
{
    int scene=AS_ShowcaseScene();
    if(!scene || cls.signon!=SIGNONS) return false;
    edict_t *player=EDICT_NUM(1);
    qboolean initialize=as_show_start<0;
    if(initialize) as_show_start=as_tick;
    int tick=AS_ShowcaseTick();
    if(initialize) {
        VectorCopy(player->v.origin,as_show_origin);as_show_origin[2]+=22;
        VectorCopy(player->v.v_angle,as_show_angles);
        if(scene==1)as_show_angles[1]=270;
        vec3_t f,r,u;AngleVectors(as_show_angles,f,r,u);VectorMA(as_show_origin,220,f,as_show_target);
        player->v.flags=(int)player->v.flags|FL_GODMODE;
        player->v.items=(int)player->v.items|IT_NAILGUN|IT_SUPER_NAILGUN|IT_ROCKET_LAUNCHER;
        player->v.ammo_nails=500;player->v.ammo_rockets=100;
        player->v.movetype=MOVETYPE_NOCLIP;
        if((scene==2 || scene==3 || scene==6) && as_num_panels) {
            as_panel_t *p=&as_panels[CLAMP(0,as_test_panel,as_num_panels-1)];
            for(int a=0;a<3;++a) as_show_target[a]=(p->mins[a]+p->maxs[a])*.5f;
            as_show_target[p->axis]=p->mins[p->axis];
            VectorCopy(as_show_target,as_show_origin);
            as_show_origin[p->axis]-=125;
            as_show_origin[p->u]-=scene==6?35:20;
            as_show_origin[2]=p->mins[2]+62;
            AS_ShowcaseAim(as_show_origin,as_show_target,as_show_angles);
            VectorCopy(as_show_origin,player->v.origin);player->v.origin[2]-=22;
            player->v.impulse=scene==2?4:7;
        }
        if(scene==4) {
            vec3_t origin={0,-155,62},target={0,10,38};VectorCopy(origin,as_show_origin);VectorCopy(target,as_show_target);
            AS_ShowcaseAim(as_show_origin,as_show_target,as_show_angles);
            VectorCopy(as_show_origin,player->v.origin);player->v.origin[2]-=22;
        }
        if(scene==5) {
            float closest=FLT_MAX;
            for(int i=2;i<qcvm->num_edicts;++i) {
                edict_t *e=EDICT_NUM(i);
                if(e->free || strncmp(PR_GetString(e->v.classname),"monster_",8))continue;
                vec3_t d;VectorSubtract(e->v.origin,as_show_origin,d);
                float distance=DotProduct(d,d);
                if(distance<closest) {closest=distance;VectorCopy(e->v.origin,as_show_target);as_show_target[2]+=20;}
            }
            AS_ShowcaseAim(as_show_origin,as_show_target,as_show_angles);
        }
        SV_LinkEdict(player,false);
        Con_Printf("Showcase scene %d camera %g %g %g target %g %g %g\n",scene,as_show_origin[0],as_show_origin[1],as_show_origin[2],as_show_target[0],as_show_target[1],as_show_target[2]);
    }
    
    VectorCopy(vec3_origin,player->v.velocity);
    vec3_t aim;VectorCopy(as_show_target,aim);
    if(scene==2) {aim[0]+=sinf(tick*.055f)*15;aim[2]+=sinf(tick*.041f)*12;}
    vec3_t eye;VectorCopy(player->v.origin,eye);eye[2]+=22;
    AS_ShowcaseAim(eye,aim,player->v.v_angle);
    VectorCopy(player->v.v_angle,player->v.angles);player->v.fixangle=1;
    if(scene==2 && tick==165)player->v.impulse=5;
    player->v.button0=scene==2?((tick>=65&&tick<135)||(tick>=185&&tick<300)):
        (scene==3 || scene==6)?(tick>=85&&tick<190):0;
    return true;
}
static void AS_ShowcaseCamera(void)
{
    int tick=AS_ShowcaseTick();
    int scene=AS_ShowcaseScene();if(!scene || tick<0)return;
    float t=CLAMP(0,(tick-31)/300.f,1),ease=t*t*(3-2*t);
    vec3_t f,r,u;AngleVectors(as_show_angles,f,r,u);
    VectorCopy(as_show_origin,r_refdef.vieworg);
    VectorMA(r_refdef.vieworg,(ease-.5f)*(scene==2?20:40),r,r_refdef.vieworg);
    if(scene==1 || scene==5)VectorMA(r_refdef.vieworg,ease*24,f,r_refdef.vieworg);
    r_refdef.vieworg[2]+=sinf(t*M_PI)*(scene==6?12:5);
    if(scene==6 && t>.6)VectorMA(r_refdef.vieworg,(t-.6)*80,f,r_refdef.vieworg);
    AS_ShowcaseAim(r_refdef.vieworg,as_show_target,r_refdef.viewangles);
}
static void AS_ShowcaseEffects(void)
{
    int tick=AS_ShowcaseTick();
    if(AS_ShowcaseScene()!=4 || (tick!=80 && tick!=170 && tick!=255))return;
    vec3_t p={-18,-10,42},d={.2f,1,.15f};
    if(tick==170){p[0]=25;p[1]=35;p[2]=24;}
    if(tick==255){p[0]=-35;p[1]=15;p[2]=30;}
    AS_VisualExplosion(p);
    AS_GoreExplosion(p,d);
    S_LocalSound("weapons/r_exp3.wav");
}
