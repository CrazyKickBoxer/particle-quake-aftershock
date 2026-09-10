/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
#version 460
layout(push_constant) uniform PushConsts { mat4 mvp; vec3 fog_color; float fog_density; float alpha; uint mode; } pc;
struct Particle { vec3 position; uint pointId; vec3 velocity; float freeTimer;
    uint flags; uint seed; float freeLifetime; uint sourceRef; };
layout(std430,set=5,binding=0) readonly buffer State { Particle particles[]; };
layout(std430,set=5,binding=2) readonly buffer Lists { uint words[]; };
layout(std430,set=5,binding=9) readonly buffer Gore { Particle stains[]; };
layout(location=0) out vec2 corner;
layout(location=1) out float opacity;
layout(location=2) flat out int style;
layout(location=3) flat out int slot;
void main() {
    gl_Position=vec4(2,2,2,1);corner=vec2(0);opacity=0.;style=int(words[15]);slot=3;
    Particle p;
    if(pc.mode==0u) { uint index=words[128u+words[10]*uint(particles.length())+uint(gl_InstanceIndex)];p=particles[index];if((p.flags&12u)==0u)return;
        slot=(p.flags&64u)!=0u?2:(p.flags&8u)!=0u?max(1,int((p.flags>>8u)&7u)):3; }
    else { p=stains[words[128u+3u*uint(particles.length())+uint(gl_InstanceIndex)]];if((p.flags&2u)==0u)return;p.freeLifetime=uintBitsToFloat(words[28]); }
    float age=pc.mode==0u?0.:max(0.,uintBitsToFloat(words[12])-p.freeTimer);
    if(pc.mode==0u && (p.flags&128u)!=0u && (p.flags&64u)==0u &&
       float(p.seed&65535u)/65535.>pow(clamp(p.freeTimer/p.freeLifetime,0.,1.),2.)) return;
    if(age>=p.freeLifetime) return;
    opacity=pc.mode==0u?clamp(p.freeTimer,0.,1.):1.-smoothstep(0.,p.freeLifetime,age);
    int corners[6]=int[](0,1,2,0,2,3);int c=corners[gl_VertexIndex%6];
    corner=vec2(c==1||c==2?1:-1,c>=2?1:-1);
    float radius=pc.mode==0u?.5: .65+float(p.seed&255u)/255.*1.1;
    vec4 clip=pc.mvp*vec4(p.position,1);
    vec2 projection=vec2(length(vec3(pc.mvp[0][0],pc.mvp[1][0],pc.mvp[2][0])),length(vec3(pc.mvp[0][1],pc.mvp[1][1],pc.mvp[2][1])));
    clip.xy+=corner*max(vec2(radius)*projection,vec2(clip.w*.0007));gl_Position=clip;
}
