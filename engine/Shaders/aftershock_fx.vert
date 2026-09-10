/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
#version 460
#extension GL_GOOGLE_include_directive : enable
#include "aftershock_shapes.glsl"
layout(push_constant) uniform PushConsts { mat4 mvp; vec3 fog_color; float fog_density; } pc;
layout(location=0) in vec4 origin_age;
layout(location=1) in vec4 settings;
layout(location=2) in vec4 direction_style;
layout(location=3) in vec4 emitter_u;
layout(location=4) in vec4 emitter_v;
layout(location=0) out vec4 out_uv;
layout(location=1) out vec4 out_color;
layout(location=2) out float out_fog;
uint hash(uint x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
float random(uint x){return float(hash(x)&65535u)/65535.;}
#include "aftershock_nails.glsl"
void main(){
    if(direction_style.w>=4096.){nailVertex();return;}
    uint id=uint(gl_InstanceIndex),seed=id+uint(settings.x)*1024u;
    bool ambient=settings.y<0.; int mode=int(direction_style.w)%16,structure=int(direction_style.w)/16;
    float strength=abs(settings.y);
    float age=origin_age.w;
    bool smoke=(id%(structure==11?64u:16u))==0u;
    float life=(smoke?3.5+random(seed)*2.:.6+random(seed)*2.8)*sqrt(strength);
    if(!smoke && structure==11)life=(.45+random(seed)*1.1)*sqrt(strength);
    if(smoke && structure==11)life=(.75+random(seed)*.55)*sqrt(strength);
    if(ambient){life=2.+random(seed)*3.;age=mod(age+random(seed+12u)*life,life);}
    vec3 dir=normalize(vec3(random(seed+1u)*2.-1.,random(seed+2u)*2.-1.,random(seed+3u)*2.-.6));
    dir=normalize(dir+direction_style.xyz*(ambient?0.:1.8));
    float speed=(smoke?30.+random(seed+4u)*55.:90.+random(seed+4u)*260.)*sqrt(strength);
    if(smoke && structure==11)speed*=.45;
    vec3 position=origin_age.xyz+dir*speed*(smoke?(1.-exp(-age*1.5))/1.5:age);
    position.z+=smoke?age*9.:-120.*age*age;
    // Bounded local floor approximation uploaded per event; decorative only.
    if(!ambient && !smoke && position.z<settings.w)position.z=settings.w+abs(position.z-settings.w)*.12;
    float size=(smoke?(6.+age*15.)*(.6+random(seed+5u)):(.3+random(seed+5u)*1.1))*sqrt(strength);
    if(!smoke && structure==11)size*=.5;
    if(smoke && structure==11)size=(2.+age*3.5)*(.7+random(seed+5u)*.5)*sqrt(strength);
    if(mode==6){position.xy+=vec2(age*18.,sin(age*2.+float(id))*5.);size*=.65;}
    if(mode==10 || mode==13){
        float a=age*(mode==13?2.:.7)+random(seed)*6.283;
        position=origin_age.xyz+vec3(cos(a),sin(a),sin(a*.5))*speed*age*.35;
        position.z+=age*12.;size*=.75;
    }
    if(mode==11 && !smoke)size*=.7;
    if(mode==5 && !smoke && (id%3u)==0u){position.z+=age*age*100.;size*=1.3;}
    if(mode==11 && smoke){position.z=origin_age.z+age*4.;size*=1.6;}
    if(mode==13 && !ambient && (id%3u)==0u){
        float angle=random(seed+1u)*6.283185;
        float ring=age*(45.+float(id%4u)*18.);
        position=origin_age.xyz+vec3(cos(angle)*ring,sin(angle)*ring,float(id%4u)*8.+age*3.);
    }
    if(ambient){
        float a=sqrt(random(seed+7u)),b=random(seed+8u);
        position=origin_age.xyz+emitter_u.xyz*a*(1.-b)+emitter_v.xyz*a*b;
        float rise=(mode==6?sin(age*2.)*1.5:age*(mode==5?5.:2.));
        position+=direction_style.xyz*(.35+rise);
        size=.20+random(seed+9u)*.50; smoke=false;
        if(mode==7 || mode==11)size*=1.5;
        if(mode==8){position+=direction_style.xyz*sin(age*3.+a*12.)*2.;size*=1.6;}
        if(mode==10 || mode==13){position+=vec3(sin(age+a*6.),cos(age+b*6.),sin(age*2.))*3.;size*=1.3;}
        if(mode==12){position+=emitter_u.xyz*(sin(age*3.)*.018);size*=.8;}
    }
    vec3 arc=vec3(1,0,0);
    if(mode==12){
        uint group=id/12u+uint(settings.x)*1024u;
        float along=(float(id%12u)+.5)/12.;
        vec3 a,b;
        if(ambient){
            float u=random(group+1u)*.65,v=random(group+2u)*.35;
            a=origin_age.xyz+emitter_u.xyz*u+emitter_v.xyz*v+direction_style.xyz*.6;
            b=origin_age.xyz+emitter_u.xyz*random(group+3u)*.65+emitter_v.xyz*random(group+4u)*.35+direction_style.xyz*2.;
        }else{
            a=origin_age.xyz+vec3(random(group+1u)-.5,random(group+2u)-.5,random(group+3u)-.3)*age*180.;
            b=origin_age.xyz+vec3(random(group+4u)-.5,random(group+5u)-.5,random(group+6u)-.3)*age*180.;
        }
        arc=b-a; position=mix(a,b,along)+vec3(0,0,sin(along*35.+origin_age.w*2.)*1.2);
        size=max(.2,length(arc)/24.); smoke=false;
    }
    vec2 corner=vec2((gl_VertexIndex==1 || gl_VertexIndex==2)?1:-1,gl_VertexIndex>=2?1:-1);
    vec4 p=pc.mvp*vec4(position,1);
    vec2 projection=vec2(length(vec3(pc.mvp[0][0],pc.mvp[1][0],pc.mvp[2][0])),length(vec3(pc.mvp[0][1],pc.mvp[1][1],pc.mvp[2][1])));
    vec2 shape=vec2(1);
    if(mode==7)shape=vec2(.45,1.8);
    if(mode==11)shape=vec2(.22,2.6);
    if(mode==8)shape=vec2(.55,1.5);
    if(mode==9)shape=vec2(1.4,.45);
    if(mode==12)shape=vec2(1.2,.07);
    vec2 offset=corner*size*shape;
    if(mode==7 || mode==9){float a=age*3.+random(seed)*6.;offset=mat2(cos(a),sin(a),-sin(a),cos(a))*offset;}
    if(mode==12){vec2 d=(pc.mvp*vec4(arc,0)).xy;d=length(d)>.001?normalize(d):vec2(1,0);offset=d*offset.x+vec2(-d.y,d.x)*offset.y;}
    p.xy+=offset*projection;gl_Position=p;out_fog=p.w;out_uv=vec4((corner+1.)*.25,0,0);
    float geometryLight=1.,geometryAlpha=1.;
    vec2 smokeCorner=corner;
    if(structure>0 && !ambient){
        int actual=structure==11?1:structure;
        if(structure==10){int choices[4]=int[](2,3,5,1);actual=choices[int(randomShape(seed)*4.)];}
        // Dust uses an eight-sided disc with interpolated soft opacity.
        if(smoke)actual=1;
        vec3 point,normal;vec2 disc;
        if(structure==11){int ids[6]=int[](0,1,2,0,2,3);int c=ids[gl_VertexIndex%6];disc=vec2(c==1||c==2?1:-1,c>=2?1:-1);point=vec3(disc,0);normal=vec3(0,0,1);}
        else shapeVertex(actual,gl_VertexIndex,seed,point,normal,disc);
        if(smoke || structure==11)smokeCorner=disc;
        // Continuous angular motion, avoiding per-frame random rotations.
        mat3 rotation=shapeRotation(seed);
        float angle=age*(1.+randomShape(seed)*4.);
        mat3 spin=mat3(cos(angle),sin(angle),0,-sin(angle),cos(angle),0,0,0,1);
        point=spin*rotation*point;normal=spin*rotation*normal;
        p=pc.mvp*vec4(position+point*size*(smoke?1.:2.3),1);
        gl_Position=p;out_fog=p.w;out_uv=vec4(structure==11?(disc+1.)*.25:vec2(.25),0,0);
        geometryLight=.45+.65*max(0.,dot(normal,normalize(vec3(-.4,-.6,1))));
        if(actual==1)geometryAlpha=structure==11?.7:1.-length(disc);
    }
    if((smoke || structure==11) && !ambient){
        // Smoke is a camera-facing puff, not a tumbling opaque wafer. The
        // support radius accounts for the entire billboard on sloped ground.
        vec3 right=normalize(vec3(pc.mvp[0][0],pc.mvp[1][0],pc.mvp[2][0]));
        vec3 up=normalize(vec3(pc.mvp[0][1],pc.mvp[1][1],pc.mvp[2][1]));
        vec3 ground=length(emitter_u.xyz)>.5?normalize(emitter_u.xyz):vec3(0,0,1);
        float groundDist=length(emitter_u.xyz)>.5?emitter_u.w:settings.w;
        float extent=size;
        if(structure==11 && !smoke){
            vec3 velocity=dir*speed+vec3(0,0,-240.*age);
            vec2 projected=vec2(dot(velocity,right),dot(velocity,up));
            vec2 axis=length(projected)>.01?normalize(projected):vec2(1,0);
            vec3 along=right*axis.x+up*axis.y;
            up=right*(-axis.y)+up*axis.x;right=along;
            extent=max(size,min(9.,length(projected)*.025))*min(1.,age*30.);
            size*=.55;
        }
        float support=extent*abs(dot(right,ground))+size*abs(dot(up,ground));
        position+=ground*max(0.,support+.25-(dot(position,ground)-groundDist));
        vec3 vertex=position+right*smokeCorner.x*extent+up*smokeCorner.y*size;
        p=pc.mvp*vec4(vertex,1);gl_Position=p;out_fog=p.w;
        geometryLight=1.;
        if(structure==11)out_uv=vec4((smokeCorner+1.)*.25,0,0);
    }
    float fade=clamp(1.-age/life,0,1);
    vec3 color=smoke?mix(vec3(.22,.19,.16),vec3(.58,.48,.36),random(seed+6u)):mix(vec3(1.,.2,.025),vec3(2.,1.3,.45),random(seed+6u));
    if(mode>=4){
        const vec3 palette[10]=vec3[10](vec3(.52,.45,.32),vec3(1.6,.28,.025),vec3(.84,.60,.27),vec3(.28,.86,1.1),vec3(.52,.025,.09),vec3(.85,.26,.055),vec3(.15,.9,.66),vec3(.65,.86,1.),vec3(.15,.55,1.4),vec3(.65,.32,1.));
        color=palette[clamp(mode-4,0,9)]*(.65+random(seed+6u)*.6);
        if(smoke)color*=.4;
        if(mode==13)color=mix(color,vec3(.20,.65,1.),random(seed+15u));
    }
    if(structure==11)color=mix(vec3(.025,.75,1.4),vec3(1.4,.38,.025),step(.62,randomShape(seed)));
    if(settings.z>0.5 && !smoke)color=min(color,vec3(.8));
    out_color=vec4(color*geometryLight,geometryAlpha*fade*(smoke?(structure==11?.045:.09):1.)*min(age*(ambient?3.:15.),1.));
    if(age>life || age<0.){gl_Position=vec4(2,2,2,1);out_color.a=0;}
}
