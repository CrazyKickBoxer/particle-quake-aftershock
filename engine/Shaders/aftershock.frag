/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
#version 460
#ifdef HOLO_PHYSICS
#extension GL_GOOGLE_include_directive : enable
#include "holo_palette.glsl"
layout(location=8) in float hpHeat;
layout(location=9) flat in int hpColorSlot;
layout(location=10) in vec3 hpWorld;
struct HoloLight { vec3 origin;float radius;vec3 color;float minlight;vec3 cone;float coneCos; };
layout(std430,set=5,binding=11) readonly buffer HoloLights { HoloLight hpLights[]; };
layout(std430,set=5,binding=2) readonly buffer HoloCounters { uint hpWords[]; };
#endif
layout(push_constant) uniform PushConsts { mat4 mvp; vec3 fog_color; float fog_density; float alpha; } pc;
layout(set=0,binding=0) uniform sampler2D diffuse_tex;
layout(set=1,binding=0) uniform sampler2D lightmap_tex;
layout(set=2,binding=0) uniform sampler2D fullbright_tex;
layout(location=0) in vec4 uv;
layout(location=1) in float fogcoord;
layout(location=2) in vec2 corner;
layout(location=3) flat in int style;
layout(location=4) flat in vec2 material;
layout(location=5) flat in vec4 grain;
layout(location=6) in vec4 shape;
layout(location=7) in vec4 out_accent;
layout(location=0) out vec4 color;
layout(constant_id=0) const bool fullbright=false;
layout(constant_id=4) const bool scaled_lm=false;
void main(){
    vec4 texel=texture(diffuse_tex,uv.xy);if(texel.a<.666)discard;vec3 diffuse=texel.rgb;
    vec3 illumination=texture(lightmap_tex,uv.zw).rgb*(scaled_lm?8:2);
    if(material.y>0.5)illumination=vec3(material.x);
    vec3 c=diffuse*illumination;
    if(fullbright)c+=texture(fullbright_tex,uv.xy).rgb;
    if(style==14){
        bool fidelity=shape.w>20.;
        float mode=fidelity?shape.w-10.:shape.w;
        vec2 source=grain.xy;
        vec2 cell=vec2(source.x+floor(source.y/16.)*16.,source.y);
        vec2 seam=abs(mod(cell+vec2(16,8),vec2(32,16))-vec2(16,8));
        float mortar=1.-smoothstep(.25,1.1,min(seam.x,seam.y));
        vec2 texelSize=1./vec2(textureSize(diffuse_tex,0));
        float edge=length(texture(diffuse_tex,uv.xy+vec2(texelSize.x,0)).rgb-diffuse)+length(texture(diffuse_tex,uv.xy+vec2(0,texelSize.y)).rgb-diffuse);
        float warm=step(.12,diffuse.r-diffuse.b)*step(.065,diffuse.r-diffuse.g);
        if(mode>11.5)warm=max(warm,step(.82,abs(shape.x)));
        vec3 neon=mix(vec3(.015,.69,1.),vec3(1.,.36,.015),warm);
        if(fidelity && out_accent.x>0.) {
            int layer=int(out_accent.x)-1;
            neon=mix(neon,layer==0?vec3(1.,.40,.045):layer==1?vec3(.7,.92,1.):vec3(.015,.75,1.),layer==0?.5:layer==1?.3:.8);
        }
        if(mode<.5){c=diffuse*.008+neon*(.008+mortar*.032+edge*.025);}
        else{
            float r=length(corner);
            vec2 width=fwidth(corner);
            float variance=fidelity?dot(width,width)/12.:0.;
            float coverage=fidelity?1.-smoothstep(1.-length(width)*.5,1.+length(width)*.5,r):1.;
            if(r>1. || coverage<.01)discard;
            float core=exp(-r*r*12./(1.+24.*variance))/(1.+24.*variance);
            float halo=exp(-r*r*3.5/(1.+7.*variance))/(1.+7.*variance);
            float intensity=(.3+pow(grain.w,3.)*2.8+mortar*1.8+edge*1.4)*(core+halo*.35);
            if(mode>11.5)intensity=(core+halo*.5)*mix(4.,6.5,out_accent.w);
            if(out_accent.w>.5 && mode<=11.5)intensity*=1.18;
            c=(neon*intensity+vec3(1,.8,.6)*pow(grain.w,22.)*core*.5)*coverage;
            if(fidelity)c*=3.5*(.72+.28*clamp(dot(normalize(shape.xyz),normalize(vec3(.3,-.4,.85)))*.5+.5,0.,1.));
        }
        c+=vec3(.25,1.8,2.5)*out_accent.y*exp(-dot(corner,corner)*4.);
        if(out_accent.z>0.) {
            float heat=out_accent.z;
            vec3 molten=mix(vec3(.02,.8,1.2),vec3(2.5,.35,.015),smoothstep(.05,.6,heat));
            molten=mix(molten,vec3(4.,3.3,2.4),smoothstep(.7,1.,heat));
            c+=molten*heat*exp(-dot(corner,corner)*3.);
        }
        float fog=clamp(exp(-pc.fog_density*pc.fog_density*fogcoord*fogcoord),0,1);
#ifdef HOLO_PHYSICS
        if(mode>=.5) c=hpColorSlot==4?mix(c,holoPalette(style,4,c),hpHeat):holoFlash(c,style,hpHeat);
        // The Neon material is emissive and normally ignores lightmaps. Apply
        // the very same selected/reserved light records to its luminous points.
        for(uint i=0u;i<min(hpWords[27],4u);++i) {
            float falloff=max(0.,1.-distance(hpWorld,hpLights[i].origin)/max(.01,hpLights[i].radius));
            c+=hpLights[i].color*falloff*falloff*(mode<.5?.012:.35);
        }
#endif
        color=vec4(mix(pc.fog_color*.03,c,fog),1);return;
    }
    if(style>0){
        float edge=smoothstep(0.45,1.0,length(corner));
        if(shape.w<.5)c*=mix(1.08,0.68,edge);
        if(style==2){float l=dot(c,vec3(.2126,.7152,.0722));c=mix(vec3(.08,.015,.11),vec3(1.4,.33,.055),clamp(l*1.4,0,1));}
        if(style==3)c=pow(max(c,0),vec3(.88))*1.12;
        vec2 p=grain.xy; float t=grain.z, seed=grain.w;
        float veins=pow(1.-abs(sin(p.x*.19+sin(p.y*.13)*2.)),14.);
        float glint=pow(max(0.,sin(t*1.7+seed*63.)),16.);
        float facet=clamp(.55+corner.x*.24-corner.y*.18,0.,1.);
        if(style==4){ // Living stone: traveling pressure waves and granular relief.
            float wave=sin(length(p-vec2(96))* .085-t*2.2);
            c*=.78+seed*.35+wave*.14; c+=vec3(.10,.085,.055)*veins;
        }else if(style==5){ // Volcanic: black crust, slowly breathing molten seams.
            c*=vec3(.43,.30,.26); c+=vec3(1.5,.23,.015)*veins*(.65+.25*sin(t+p.y*.03));
            c+=vec3(.35,.07,.005)*glint*step(.91,seed);
        }else if(style==6){ // Sand: moving dune ridges and individual dark grains.
            float ridge=.5+.5*sin(p.y*.27+sin(p.x*.08)*3.-t*.8);
            c=mix(c,vec3(.64,.41,.16)*illumination,.64)*(.72+ridge*.28+seed*.22);
        }else if(style==7){ // Crystal: angular facets, diagonal veins and restrained highlights.
            float cut=pow(1.-abs(sin((p.x+p.y)*.12)),18.);
            c=mix(c,vec3(.12,.42,.55)*illumination,.55)*(.65+facet*.7);
            c+=vec3(.22,.65,.8)*(cut*.55+glint*.25);
        }else if(style==8){ // Corrupted flesh: branching fibers and slow muscular pulse.
            float pulse=.8+.2*sin(t*2.-p.x*.04);
            float fiber=pow(.5+.5*sin(p.y*.4+sin(p.x*.09+t*.4)*3.),8.);
            c=mix(c,vec3(.38,.045,.075)*illumination,.78)*pulse;
            c+=vec3(.20,.015,.045)*fiber; c*=1.-veins*.55;
        }else if(style==9){ // Rust: layered paint chips and pitted oxidized edges.
            c=mix(c,vec3(.48,.13,.025)*illumination,step(.36,seed)*.8);
            c*=.75+facet*.4; c+=vec3(.55,.20,.03)*glint*step(.97,seed);
        }else if(style==10){ // Spectral: translucent-looking luminous contours over solid surfaces.
            float wisp=pow(.5+.5*sin(p.x*.065+p.y*.08-t),5.);
            c=c*.35+vec3(.12,.55,.43)*(.12+wisp*.4+veins*.25);
        }else if(style==11){ // Frozen ruins: frost lattice and blue ice facets.
            float frost=max(veins,pow(1.-abs(sin((p.x-p.y)*.17)),20.));
            c=mix(c,vec3(.48,.66,.74)*illumination,.62)*(.7+facet*.55);
            c+=vec3(.35,.55,.65)*(frost*.65+glint*.12);
        }else if(style==12){ // Electric grid: running charge following orthogonal channels.
            vec2 line=abs(sin(p*.09817477));
            float grid=1.-smoothstep(.02,.15,min(line.x,line.y));
            float charge=.4+.6*pow(.5+.5*sin((p.x+p.y)*.045-t*2.),4.);
            c=c*.48+vec3(.07,.48,.95)*grid*charge;
        }else if(style==13){ // Cosmic dust: dense star grains and drifting nebula bands.
            float cloud=.5+.5*sin(p.x*.027+sin(p.y*.031+t*.18)*3.);
            c=c*.38+mix(vec3(.13,.035,.24),vec3(.035,.17,.3),cloud)*.5;
            c+=(diffuse*.7+vec3(.15,.20,.35))*step(.89,seed)*(.25+glint*.7);
        }
    }
    if(shape.w>.5){
        vec3 n=normalize(shape.xyz),sun=normalize(vec3(-.4,-.6,1));
        c*=.48+.65*max(0.,dot(n,sun));
        if(shape.w>7.5&&shape.w<8.5)c+=vec3(.25)*pow(max(0.,dot(n,sun)),24.);
        if(shape.w<1.5){
            float coverage=1.-smoothstep(.05,1.,length(corner));
            // Stable screen-door soft edges work with all depth/OIT render passes.
            float threshold=fract(dot(floor(gl_FragCoord.xy),vec2(.754877666,.569840296)));
            if(coverage<threshold)discard;
            c=mix(c,c*1.5,coverage*.25);
        }
    }
    float fog=clamp(exp(-pc.fog_density*pc.fog_density*fogcoord*fogcoord),0,1);
#ifdef HOLO_PHYSICS
    c=hpColorSlot==4?mix(c,holoPalette(style,4,c),hpHeat):holoFlash(c,style,hpHeat);
#endif
    color=vec4(mix(pc.fog_color,c,fog),1);
}
