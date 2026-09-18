/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
#version 460
layout(push_constant) uniform PC {mat4 mvp;vec3 fog_color;float fog_density;} pc;
layout(set=0,binding=0) uniform sampler2D diffuse_tex;
layout(set=1,binding=0) uniform sampler2D fullbright_tex;
layout(set=2,binding=0) uniform UBO {mat4 model;vec3 shade;float blend;vec3 light;float alpha;uint flags;} u;
layout(location=0) in vec2 uv;
layout(location=1) in vec4 color;
layout(location=2) in float fog;
layout(location=3) in vec2 corner;
layout(location=4) flat in uint neon;
layout(location=0) out vec4 result;
// as_neon_npc_texture: tint a monster splat from its own MDL skin texel.
// Pure function of the texel - no time, no noise, so particles never swim.
// A flat gain cannot work here: shambler skins are pale and ogre skins are
// dark, so they fail in opposite directions. This is a hue-preserving value
// curve that lifts dark skins much harder than bright ones, which keeps the
// light/dark detail inside the skin readable as a point cloud.
const float NPC_SAT=1.35;   // >1 pushes saturation (the Quake palette is muddy). 1 = off
const float NPC_LIFT=0.55;  // value-curve exponent. 1 = off, lower lifts dark skins more
const float NPC_GAIN=1.0;   // overall multiplier
const float NPC_FB=1.5;     // fullbright texels stay the hottest thing on the monster
vec3 as_npc_tint(vec3 s){
    float l=dot(s,vec3(.2126,.7152,.0722));
    s=max(mix(vec3(l),s,NPC_SAT),vec3(0.));
    float m=max(max(s.r,s.g),max(s.b,.04)); // .04 floor caps the boost at ~4.3x
    return s*(pow(m,NPC_LIFT-1.)*NPC_GAIN);
}
void main(){
    vec4 source=texture(diffuse_tex,uv);
    if((u.flags&128u)!=0u && source.a<.666)discard;
    vec3 fb=vec3(0.);
    if((u.flags&1u)!=0u)fb=texture(fullbright_tex,uv).rgb; // hoisted: one fetch, shared by both paths
    vec3 c=source.rgb*color.rgb*2.+fb;
    if(neon!=0u){float r=length(corner);if(r>1.)discard;
        bool fidelity=(u.flags&256u)!=0u;
        float variance=fidelity?dot(fwidth(corner),fwidth(corner))/12.:0.;
        float core=exp(-r*r*9./(1.+18.*variance))/(1.+18.*variance);
        float coverage=fidelity?1.-smoothstep(1.-length(fwidth(corner))*.5,1.+length(fwidth(corner))*.5,r):1.;
        // Legacy arm is exactly the old expression: tint == color.rgb.
        vec3 tint=color.rgb;
        if(neon==3u&&(u.flags&0x200u)!=0u)tint*=as_npc_tint(source.rgb)+fb*NPC_FB; // color.rgb is white x lighting here
        c=tint*(core+exp(-r*r*3./(1.+6.*variance))/(1.+6.*variance)*.18)*color.a*1.9*coverage;
        c+=vec3(1)*pow(core,4.)*.15;
        if(fidelity)c*=3.;
    }
    float f=clamp(exp(-pc.fog_density*pc.fog_density*fog*fog),0,1);
    result=vec4(mix(pc.fog_color*(neon!=0u?.03:1.),c,f),u.alpha);
}
