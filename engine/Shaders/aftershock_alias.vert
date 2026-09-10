/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
#version 460
layout(push_constant) uniform PC {mat4 mvp;vec3 fog_color;float fog_density;} pc;
layout(set=2,binding=0) uniform UBO {mat4 model;vec3 shade;float blend;vec3 light;float alpha;uint flags;uint pose1;uint pose2;uint st;uint samples;} u;
layout(set=3,binding=0,std430) readonly buffer Mesh {uint data[];} mesh;
layout(location=0) out vec2 uv;
layout(location=1) out vec4 color;
layout(location=2) out float fog;
layout(location=3) out vec2 particle_corner;
layout(location=4) flat out uint neon;
vec3 position(uint base,uint vertex){uint a=mesh.data[base+vertex*3],b=mesh.data[base+vertex*3+1];return vec3(a&65535u,a>>16,b&65535u)/257.;}
vec3 normal(uint base,uint vertex){uint n=mesh.data[base+vertex*3+2];return vec3(int(n<<24)>>24,int(n<<16)>>24,int(n<<8)>>24)/127.;}
vec2 texcoord(uint vertex){return vec2(uintBitsToFloat(mesh.data[u.st+vertex*2]),uintBitsToFloat(mesh.data[u.st+vertex*2+1]));}
float lighting(vec3 n){float d=dot(n,u.shade);return d<0.?1.+d*(13./44.):1.+d;}
void main(){
    neon=(u.flags>>4)&3u;particle_corner=vec2(0);
    uint instance=uint(gl_InstanceIndex),layer=0;
    if((u.flags&0x130u)==0x130u){layer=instance%2u;instance/=2u;}
    uint base=u.samples+instance*4;
    uvec3 ids=uvec3(mesh.data[base],mesh.data[base+1],mesh.data[base+2]);uint packed=mesh.data[base+3];
    vec2 b=vec2(packed&4095u,(packed>>12)&4095u)/4095.;vec3 weights=vec3(1.-b.x-b.y,b);
    vec3 a=mix(position(u.pose1,ids.x),position(u.pose2,ids.x),u.blend),c=mix(position(u.pose1,ids.y),position(u.pose2,ids.y),u.blend),d=mix(position(u.pose1,ids.z),position(u.pose2,ids.z),u.blend);
    vec2 corner=vec2((gl_VertexIndex==1 || gl_VertexIndex==2)?1:-1,gl_VertexIndex>=2?1:-1);
    vec2 footprint=max(b+corner*(.6/float(packed>>24)),vec2(0));
    if(footprint.x+footprint.y>1.)footprint/=footprint.x+footprint.y;
    vec3 p=a*(1.-footprint.x-footprint.y)+c*footprint.x+d*footprint.y;
    gl_Position=pc.mvp*u.model*vec4(p,1);fog=gl_Position.w;
    uv=texcoord(ids.x)*weights.x+texcoord(ids.y)*weights.y+texcoord(ids.z)*weights.z;
    vec3 n1=normal(u.pose1,ids.x)*weights.x+normal(u.pose1,ids.y)*weights.y+normal(u.pose1,ids.z)*weights.z;
    vec3 n2=normal(u.pose2,ids.x)*weights.x+normal(u.pose2,ids.y)*weights.y+normal(u.pose2,ids.z)*weights.z;
    color=vec4(u.light*((u.flags&2u)!=0u?1.:mix(lighting(n1),lighting(n2),u.blend)),1);
    if(neon!=0u){
        uint h=packed+ids.x*179u+ids.y*3181u+layer*0x9e3779b9u;h^=h>>16;h*=0x7feb352du;h^=h>>15;
        vec2 jitter=vec2(float(h&255u),float((h>>8)&255u))/255.-.5;
        vec2 bary=max(b+jitter*.55/float(packed>>24),vec2(0));
        if(bary.x+bary.y>1.)bary/=bary.x+bary.y;
        p=a*(1.-bary.x-bary.y)+c*bary.x+d*bary.y;
        gl_Position=pc.mvp*u.model*vec4(p,1);
        float radius=(.12+float(h&255u)/255.*.13)*((u.flags&64u)!=0u?.24:1.);
        vec2 projection=vec2(length(vec3(pc.mvp[0][0],pc.mvp[1][0],pc.mvp[2][0])),length(vec3(pc.mvp[0][1],pc.mvp[1][1],pc.mvp[2][1])));
        gl_Position.xy+=corner*max(projection*radius,vec2(gl_Position.w*.0011));
        fog=gl_Position.w;particle_corner=corner;
        color=vec4(neon==3u?vec3(1.,.015,.42):vec3(.015,.68,1.),.6+float(h&255u)/255.);
        if((u.flags&256u)!=0u){
            vec3 worldNormal=normalize(transpose(inverse(mat3(u.model)))*mix(n1,n2,u.blend));
            // Camera position follows from the homogeneous inverse projection.
            vec4 eye=inverse(pc.mvp)*vec4(0,0,1,0);
            vec3 view=normalize(eye.xyz/eye.w-(u.model*vec4(p,1)).xyz);
            float rim=pow(1.-abs(dot(worldNormal,view)),2.);
            color.rgb*=.72+rim*1.5;
            // Expand only the silhouette to make distant limbs readable.
            gl_Position.xy+=corner*projection*radius*rim*.35;
        }
    }

}
