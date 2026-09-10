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
void main(){
    vec4 source=texture(diffuse_tex,uv);
    if((u.flags&128u)!=0u && source.a<.666)discard;
    vec3 c=source.rgb*color.rgb*2.;
    if((u.flags&1u)!=0u)c+=texture(fullbright_tex,uv).rgb;
    if(neon!=0u){float r=length(corner);if(r>1.)discard;
        bool fidelity=(u.flags&256u)!=0u;
        float variance=fidelity?dot(fwidth(corner),fwidth(corner))/12.:0.;
        float core=exp(-r*r*9./(1.+18.*variance))/(1.+18.*variance);
        float coverage=fidelity?1.-smoothstep(1.-length(fwidth(corner))*.5,1.+length(fwidth(corner))*.5,r):1.;
        c=color.rgb*(core+exp(-r*r*3./(1.+6.*variance))/(1.+6.*variance)*.18)*color.a*1.9*coverage;
        c+=vec3(1)*pow(core,4.)*.15;
        if(fidelity)c*=3.;
    }
    float f=clamp(exp(-pc.fog_density*pc.fog_density*fog*fog),0,1);
    result=vec4(mix(pc.fog_color*(neon!=0u?.03:1.),c,f),u.alpha);
}
