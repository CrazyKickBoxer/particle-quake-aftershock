// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Josh Nicholls
// Triangle meshes, shared by wall instances and flying explosion fragments.
const float TAU=6.28318530718;
uint shapeHash(uint x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
float randomShape(uint x){return float(shapeHash(x)&0xffffffu)/16777216.;}
vec3 octPoint(int i){return i==0?vec3(0,0,1):i==1?vec3(0,0,-1):vec3(cos(float(i-2)*TAU/4.),sin(float(i-2)*TAU/4.),0);}
int shapeCount(int mode){return mode==1?24:mode==3||mode==6?24:mode==4?36:mode==5?72:mode==7?96:192;}
void shapeVertex(int mode,int vertex,uint seed,out vec3 p,out vec3 n,out vec2 disc){
    p=vec3(0);n=vec3(0,0,1);disc=vec2(0);
    if(vertex>=shapeCount(mode))return; // Degenerate padding in the mixed draw.
    int tri=vertex/3,k=vertex%3;
    if(mode==1){
        float a=float(tri+(k==2?1:0))*TAU/8.;
        disc=k==0?vec2(0):vec2(cos(a),sin(a));p=vec3(disc,0);
    }else if(mode==3||mode==6){
        int ids[3]=int[](tri<4?0:1,2+tri%4,2+(tri+1)%4);
        vec3 q[3];
        for(int j=0;j<3;++j){q[j]=octPoint(ids[j]);q[j]*=.65+.7*randomShape(seed+uint(ids[j])*179u);}
        if(mode==6){for(int j=0;j<3;++j)q[j]*=vec3(.17,.23,2.8);}
        p=q[k];n=normalize(cross(q[1]-q[0],q[2]-q[0]));if(dot(n,p)<0)n=-n;
    }else if(mode==4){
        int face=vertex/6,axis=face/2,u=(axis+1)%3,v=(axis+2)%3;
        int corners[6]=int[](0,1,2,0,2,3);int c=corners[vertex%6];
        p[axis]=(face%2==0?-.72:.72);p[u]=(c==1||c==2?.72:-.72);p[v]=(c>=2?.72:-.72);
        n=vec3(0);n[axis]=face%2==0?-1:1;
    }else if(mode==5){
        // Closed hexagonal wafer: irregular outline, front, back, and thin sides.
        int face=tri/6,seg=tri%6;
        int indices[6]=int[](0,1,2,0,2,3);
        if(face<2){float a=float(seg+(k==2?1:0))*TAU/6.;
            float r=.7+.4*randomShape(seed+uint((seg+(k==2?1:0))%6)*31u);
            p=vec3(k==0?vec2(0):vec2(cos(a),sin(a))*r,face==0?.075:-.075);n=vec3(0,0,face==0?1:-1);
        }else{int edge=(tri-12)/2,c=indices[(tri%2)*3+k];int e=(edge+(c==1||c==2?1:0))%6;
            float a=float(e)*TAU/6.,r=.7+.4*randomShape(seed+uint(e)*31u);
            p=vec3(vec2(cos(a),sin(a))*r,c>=2?.075:-.075);n=vec3(cos(a),sin(a),0);
        }
    }else if(mode==7){
        // Two crossing, curved strips; neither is a camera-facing square.
        int ribbon=vertex/48,segment=(vertex%48)/6;
        int indices[6]=int[](0,1,2,0,2,3);int c=indices[vertex%6];
        float t=(float(segment)+(c>=2?1.:0.))/8.,side=c==1||c==2?1.:-1.;
        float phase=randomShape(seed)*TAU;
        p=vec3((t-.5)*4.,sin(t*TAU+phase)*.42,cos(t*TAU*.7+phase)*.34);
        if(ribbon==0){p.y+=side*.13;n=normalize(vec3(-cos(t*TAU+phase)*.66,1,1));}
        else{p.z+=side*.13;n=normalize(vec3(sin(t*TAU*.7+phase)*.37,-1,1));}
    }else{
        int cell=vertex/6,ring=cell/8,segment=cell%8;
        int ids[6]=int[](0,1,2,0,2,3);int c=ids[vertex%6];
        float a=(float(segment)+(c==1||c==2?1.:0.))*TAU/8.;
        float b=(float(ring)+(c>=2?1.:0.))*(mode==9?TAU:TAU*.5)/4.;
        if(mode==9){n=vec3(cos(a)*cos(b),sin(a)*cos(b),sin(b));p=vec3(cos(a),sin(a),0)*.77+n*.23;}
        else{n=vec3(cos(a)*sin(b),sin(a)*sin(b),cos(b));p=n;
            if(mode==8){float lobe=1.+.22*sin(a*3.+randomShape(seed)*TAU)*sin(b)*sin(b);p*=lobe;p*=vec3(1.25,1.,.78);}
        }
    }
}
mat3 shapeRotation(uint seed){
    vec3 axis=normalize(vec3(randomShape(seed+3u),randomShape(seed+9u),randomShape(seed+19u))-.5);
    float a=randomShape(seed+27u)*TAU,c=cos(a),s=sin(a),t=1.-c;
    return mat3(t*axis.x*axis+c*vec3(1,0,0)+s*vec3(0,axis.z,-axis.y),
                t*axis.y*axis+c*vec3(0,1,0)+s*vec3(-axis.z,0,axis.x),
                t*axis.z*axis+c*vec3(0,0,1)+s*vec3(axis.y,-axis.x,0));
}
