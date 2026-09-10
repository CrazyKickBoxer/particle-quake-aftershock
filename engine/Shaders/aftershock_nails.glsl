/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
// Bounded analytic cosmetics: 48 streaks, 8 chips, 48 ring segments per hit.
// No scene simulation, atomics, storage buffers, or gameplay state.
void nailVertex(){
    uint id=uint(gl_InstanceIndex),seed=id+uint(settings.x)*1024u;
    int kind=int(direction_style.w)-4096;
    bool trail=kind==2,chip=!trail && id>=48u && id<56u;
    bool ring=trail || id>=56u;
    float age=origin_age.w,life=trail?.24:chip?1.1:ring?.48:.32+random(seed)*.4;
    float t=age*(.75+.25*exp(-age*7.)); // Visual easing only; nail speed is untouched.
    vec3 n=normalize(emitter_u.xyz),incoming=normalize(direction_style.xyz);
    vec3 tangent=normalize(cross(abs(n.z)>.9?vec3(0,1,0):vec3(0,0,1),n));
    vec3 bitangent=cross(n,tangent),p=origin_age.xyz+n*.6;
    vec3 reflected=reflect(incoming,n);
    vec3 scatter=tangent*(random(seed+1u)*2.-1.)+bitangent*(random(seed+2u)*2.-1.);
    vec3 direction=normalize(reflected*.9+n*(.45+random(seed+3u))+scatter*1.8);
    direction=normalize(direction+n*max(0.,.12-dot(direction,n)));
    vec3 velocity=direction*(chip?35.+random(seed+4u)*55.:130.+random(seed+4u)*280.);
    int corners[6]=int[](0,1,2,0,2,3);int c=corners[gl_VertexIndex%6];
    vec2 uv=vec2(c==1||c==2?1:-1,c>=2?1:-1);
    vec3 color=mix(vec3(.06,1.8,3.2),vec3(4.,1.7,.25),random(seed+7u));
    float alpha=pow(max(0.,1.-age/life),1.3);
    if(ring){
        uint segment=trail?id:(id-56u)%24u;
        float count=trail?32.:24.;
        float angle=(float(segment)+(uv.x+1.)*.5)*6.283185/count;
        float band=trail?.14:.32;
        float radius=trail?2.+t*32.:3.+t*(id>=80u?110.:160.);
        vec3 axis=trail?incoming:n;
        vec3 u=normalize(cross(abs(axis.z)>.9?vec3(0,1,0):vec3(0,0,1),axis));
        vec3 v=cross(axis,u);
        p+= (u*cos(angle)+v*sin(angle))*(radius+uv.y*band);
        p+=axis*(trail?-t*20.:1.+t*12.);
        color=trail?vec3(.08,1.1,1.5):vec3(.15,2.4,3.);
        alpha*=trail?.30:.55;
        out_uv=vec4(.25,.25,0,0);
    }else{
        p+=velocity*t+vec3(0,0,-(chip?100.:35.)*t*t);
        // Keep the whole shape outside the struck wall, including sloped faces.
        float radius=chip?.5+random(seed+8u)*.8:.12+random(seed+8u)*.15;
        if(chip){
            vec3 point,normal;vec2 disc;shapeVertex(3,gl_VertexIndex,seed,point,normal,disc);
            point=shapeRotation(seed)*point;
            p+=n*max(0.,radius*2.-dot(p-origin_age.xyz,n));
            p.z=max(p.z,emitter_v.x+radius*2.);
            p+=point*radius;
            color=mix(vec3(.09,.11,.13),color*.4,pow(max(0.,dot(normal,n)),8.));
            out_uv=vec4(.25,.25,0,0);
        }else{
            vec3 right=normalize(vec3(pc.mvp[0][0],pc.mvp[1][0],pc.mvp[2][0]));
            vec3 up=normalize(vec3(pc.mvp[0][1],pc.mvp[1][1],pc.mvp[2][1]));
            vec3 v=velocity+vec3(0,0,-70.*t);
            vec2 projected=vec2(dot(v,right),dot(v,up));
            vec2 axis=length(projected)>.01?normalize(projected):vec2(1,0);
            vec3 along=right*axis.x+up*axis.y,across=right*(-axis.y)+up*axis.x;
            float extent=min(14.,length(projected)*.035)*min(1.,age*50.);
            float support=abs(dot(along,n))*extent+abs(dot(across,n))*radius;
            p+=n*max(0.,support+.2-dot(p-origin_age.xyz,n));
            p+=along*uv.x*extent+across*uv.y*radius;
            out_uv=vec4((uv+1.)*.25,0,0);
        }
    }
    if(kind==3)color*=1.2;
    if(settings.z>.5){color=min(color,vec3(.8));alpha*=.5;}
    gl_Position=pc.mvp*vec4(p,1);out_fog=gl_Position.w;
    out_color=vec4(color,alpha);
    if(age<0. || age>life || (!chip && gl_VertexIndex>=6)){
        gl_Position=vec4(2,2,2,1);out_color.a=0.;
    }
}
