/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
#version 460
#extension GL_GOOGLE_include_directive : enable
#include "aftershock_shapes.glsl"
#include "aftershock_blue_noise.glsl"
#ifdef HOLO_PHYSICS
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
layout(location=7) in vec4 normal_mode;
layout(location=8) in vec4 wave;
layout(location=9) in vec4 accent;
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
    int mode=int(normal_mode.w)%16,layer=(int(normal_mode.w)/16)%16;
    vec2 samplepos=vec2(packed&65535u,packed>>16);
    // Local source coordinates survive chunk translation/rotation, including the seed.
    uint seed=shapeHash(packed+uint(layer)*0x9e3779b9u+floatBitsToUint(tex.x)+floatBitsToUint(tex.y));
    if(mode==11){
        bool boundary=(int(normal_mode.w)&256)!=0;
        bool fidelity=(int(normal_mode.w)&512)!=0;
        float step=origin.w/.58;
        if(!boundary)samplepos+=(vec2(randomShape(seed+81u),randomShape(seed+92u))-.5)*step*.8;
        if(fidelity && !boundary){
            ivec2 cell=ivec2(floor(vec2(packed&65535u,packed>>16)/step));
            uint shift=shapeHash(uint(layer)*0x9e3779b9u+floatBitsToUint(tex.x)+floatBitsToUint(tex.y));
            uint index=((uint(cell.x)+(shift&15u))&15u)+16u*((uint(cell.y)+((shift>>4)&15u))&15u);
            samplepos=(vec2(cell)+blueNoisePoints[index])*step;
        }
        vec3 normal=normalize(normal_mode.xyz);
        vec3 center=origin.xyz+saxis.xyz*samplepos.x+taxis.xyz*samplepos.y;
        center+=normal*(.06+float(layer)*.22+randomShape(seed+33u)*.25);
        if(fidelity) {
            float distance=length(center-wave.xyz);
            float ring=wave.w>=0.?exp(-pow((distance-wave.w*240.)/5.,2.))*max(0.,1.-wave.w/1.3):0.;
            float rim=1.-smoothstep(4.,12.,min(min(samplepos.x,samplepos.y),min(128.-samplepos.x,128.-samplepos.y)));
            out_accent=vec4(float(layer)+1.,ring,accent.x*rim,accent.w);
            center+=normal*ring*.6;
        }
        int corners[6]=int[](0,1,2,0,2,3);int c=corners[gl_VertexIndex%6];
        vec2 corner=vec2(c==1||c==2?1:-1,c>=2?1:-1);
        float radius=boundary?.35:.16+randomShape(seed+18u)*.22;
        if(accent.w>.5) {
            radius*=boundary?1.12:1.08;

        }
#ifdef HOLO_PHYSICS
        center=holoDisplace(center,pc.instance_base+uint(gl_InstanceIndex));
#endif
        vec4 clip=pc.mvp*vec4(center,1);
        if(accent.w>.5 && !boundary && randomShape(seed+517u)>.97)
            radius*=1.+1.4*(1.-smoothstep(24.,150.,clip.w));
        vec2 projection=vec2(length(vec3(pc.mvp[0][0],pc.mvp[1][0],pc.mvp[2][0])),length(vec3(pc.mvp[0][1],pc.mvp[1][1],pc.mvp[2][1])));
        clip.xy+=corner*max(vec2(radius)*projection,vec2(clip.w*(boundary?.0015:.00085)));
        gl_Position=clip;out_fog_frag_coord=clip.w;
        out_texcoords=vec4(tex.xy+samplepos*tex.zw+samplepos.yx*surface_params.xy,lightmap.xy+samplepos*lightmap.zw);
        out_corner=corner;out_style=14;out_material=surface_params.zw;
        out_grain=vec4(samplepos*.125,taxis.w,randomShape(seed));out_shape=vec4(normal,(boundary?12:11)+(fidelity?10:0));
        if(!boundary && (randomShape(seed+771u)>(fidelity?.70:.48)))gl_Position=vec4(2,2,2,1);
        return;
    }
    int actual=mode;
    if(mode==10){int choices[4]=int[](2,3,5,1);actual=choices[int(randomShape(seed)*4.)];}
    vec3 p,n;vec2 disc;shapeVertex(actual,gl_VertexIndex,seed,p,n,disc);
    vec3 normal=normalize(normal_mode.xyz),s=normalize(saxis.xyz),t=normalize(cross(normal,s));
    mat3 basis=mat3(s,t,normal),rotation=shapeRotation(seed);
    // Flakes lie mostly along the wall; fibers cross and arch out from it.
    if(actual==1||actual==5||actual==7){float a=randomShape(seed+12u)*TAU;
        rotation=mat3(cos(a),sin(a),.12*sin(a),-sin(a),cos(a),.12*cos(a),0,0,1);}
    float radius=origin.w*sqrt(length(saxis.xyz)*length(taxis.xyz))*(.55+randomShape(seed+1u)*.75);
    radius=min(radius,4.);
    if(mode==10)radius*=layer==0?1.25:layer==1?.8:.5;
    vec3 center=origin.xyz+saxis.xyz*samplepos.x+taxis.xyz*samplepos.y;
    center+=normal*radius*(.4+float(layer)*.55+randomShape(seed+2u)*.4);
#ifdef HOLO_PHYSICS
    center=holoDisplace(center,pc.instance_base+uint(gl_InstanceIndex));
#endif
    vec4 clip=pc.mvp*vec4(center+basis*rotation*p*radius,1);
    gl_Position=clip;out_fog_frag_coord=clip.w;
    out_texcoords=vec4(tex.xy+samplepos*tex.zw+samplepos.yx*surface_params.xy,lightmap.xy+samplepos*lightmap.zw);
    out_corner=actual==1?disc:p.xy;out_style=int(saxis.w);out_material=surface_params.zw;
    out_grain=vec4(samplepos*.125,taxis.w,randomShape(seed));
    out_shape=vec4(normalize(basis*rotation*n),float(actual));
}
