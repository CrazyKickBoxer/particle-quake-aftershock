/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
#version 460
#ifdef HOLO_PHYSICS
#extension GL_GOOGLE_include_directive : enable
#include "holo_physics_draw.glsl"
#endif
layout(push_constant) uniform PushConsts { mat4 mvp; vec3 fog_color; float fog_density; float alpha; uint instance_base; } pc;
layout(location=0) in uint packed;
layout(location=1) in vec4 origin;
layout(location=2) in vec4 saxis;
layout(location=3) in vec4 taxis;
layout(location=4) in vec4 tex;
layout(location=5) in vec4 lightmap;
layout(location=6) in vec4 surface_params;
layout(location=0) out vec4 out_texcoords;
layout(location=1) out float out_fog_frag_coord;
layout(location=2) out vec2 out_corner;
layout(location=3) flat out int out_style;
layout(location=4) flat out vec2 out_material;
layout(location=5) flat out vec4 out_grain;
layout(location=6) out vec4 out_shape;
layout(location=7) out vec4 out_accent;
void main(){
    out_accent=vec4(0);
    out_shape=vec4(0);
    vec2 samplepos=vec2(packed&65535u,packed>>16);
    vec3 center=origin.xyz+saxis.xyz*samplepos.x+taxis.xyz*samplepos.y;
#ifdef HOLO_PHYSICS
    center=holoDisplace(center,pc.instance_base+uint(gl_InstanceIndex));
#endif
    vec2 corner=vec2((gl_VertexIndex==1 || gl_VertexIndex==2)?1:-1,(gl_VertexIndex>=2)?1:-1);
    // Plane-aligned footprints retain the actual surface depth at grazing angles.
    vec4 p=pc.mvp*vec4(center+(saxis.xyz*corner.x+taxis.xyz*corner.y)*origin.w,1);
    gl_Position=p; out_texcoords=vec4(tex.xy+samplepos*tex.zw+samplepos.yx*surface_params.xy,lightmap.xy+samplepos*lightmap.zw);out_fog_frag_coord=p.w;
    out_corner=corner;out_style=int(saxis.w);
    out_material=surface_params.zw;
    // Stable source coordinates keep material detail attached to moving rubble.
    uint seed=packed; seed^=seed>>16; seed*=0x7feb352du; seed^=seed>>15; seed*=0x846ca68bu; seed^=seed>>16;
    out_grain=vec4(samplepos*.125,taxis.w,float(seed&65535u)/65535.);
}
