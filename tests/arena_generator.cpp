// SPDX-License-Identifier: GPL-2.0-or-later
// Original procedural Quake BSP29 arena. No game data or map compiler required.
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
using Bytes=std::vector<uint8_t>;
using Vec=std::array<float,3>;
struct Box{Vec lo,hi;};
#pragma pack(push,1)
struct Plane{float normal[3],distance;int32_t type;};
struct Node{int32_t plane;int16_t child[2],lo[3],hi[3];uint16_t face,count;};
struct Clip{int32_t plane;int16_t child[2];};
struct Face{uint16_t plane,side;int32_t edge;uint16_t count,texture;uint8_t styles[4];int32_t light;};
struct Leaf{int32_t contents,visibility;int16_t lo[3],hi[3];uint16_t first,count;uint8_t ambient[4];};
struct TextureInfo{float uv[2][4];int32_t texture,flags;};
struct Model{float lo[3],hi[3],origin[3];int32_t heads[4],leaves,first,count;};
#pragma pack(pop)
static_assert(sizeof(Node)==24 && sizeof(Leaf)==28 && sizeof(Face)==20 && sizeof(Model)==64);
template<class T>void append(Bytes &bytes,const T &v){size_t n=bytes.size();bytes.resize(n+sizeof(T));std::memcpy(bytes.data()+n,&v,sizeof(T));}
template<class T>Bytes encode(const std::vector<T> &v){Bytes b(v.size()*sizeof(T));if(!b.empty())std::memcpy(b.data(),v.data(),b.size());return b;}
int main(int argc,char **argv){
 if(argc!=2 && argc!=3){std::cerr<<"Usage: aftershock_arena_generator output.bsp\n";return 1;}
 const Box bounds{{-288,-544,-32},{288,544,288}};
 std::vector<Box> boxes={
  {{-288,-544,-32},{288,544,0}},{{-288,-544,256},{288,544,288}},
  {{-288,-544,0},{-256,544,256}},{{256,-544,0},{288,544,256}},
  {{-256,-544,0},{256,-512,256}},{{-256,512,0},{256,544,256}},
  {{-128,-8,0},{128,8,160}},
  {{-224,144,0},{-160,208,8}},{{-224,208,0},{-160,272,16}},{{-224,272,0},{-160,336,24}}};
 if(argc==3 && std::string(argv[2])=="--progressive"){
  boxes.push_back({{96,144,0},{128,176,144}});
  boxes.push_back({{-96,320,96},{128,368,112}});
  boxes.push_back({{-128,320,0},{-96,368,112}});
  boxes.push_back({{128,320,0},{160,368,112}});
 }
 std::vector<Plane> planes;std::vector<Node> nodes;std::vector<Clip> clips;
 std::vector<Vec> vertices;std::vector<std::array<uint16_t,2>> edges;std::vector<int32_t> surfedges;std::vector<Face> faces;std::vector<TextureInfo> texinfo;
 const int corners[6][4]={{0,4,6,2},{1,3,7,5},{0,1,5,4},{2,6,7,3},{0,2,3,1},{4,5,7,6}};
 auto makePlane=[&](int axis,float distance){Plane p{};p.normal[axis]=1;p.distance=distance;p.type=axis;planes.push_back(p);return int(planes.size()-1);};
 auto renderBox=[&](const Box &box,int inside,int outside,bool visible){int base=int(nodes.size());nodes.resize(base+6);
  for(int k=0;k<6;++k){int axis=k/2;Node &node=nodes[base+k];node.plane=makePlane(axis,(k&1)?box.hi[axis]:box.lo[axis]);node.child[k&1]=int16_t(k==5?inside:base+k+1);node.child[(k&1)^1]=int16_t(outside);
   for(int a=0;a<3;++a){node.lo[a]=int16_t(bounds.lo[a]);node.hi[a]=int16_t(bounds.hi[a]);}
   if(visible){node.face=uint16_t(faces.size());node.count=1;Face f{};f.plane=uint16_t(node.plane);f.side=(k&1)?0:1;f.edge=int32_t(surfedges.size());f.count=4;f.texture=uint16_t(texinfo.size());std::memset(f.styles,255,4);f.light=-1;
    int first=int(vertices.size());for(int c=0;c<4;++c){int code=corners[k][c];Vec v;for(int a=0;a<3;++a)v[a]=(code>>a)&1?box.hi[a]:box.lo[a];vertices.push_back(v);}
    for(int c=0;c<4;++c){surfedges.push_back(int32_t(edges.size()));edges.push_back({uint16_t(first+c),uint16_t(first+(c+1)%4)});}
    TextureInfo t{};t.uv[0][axis==0?1:0]=1;t.uv[1][axis==2?1:2]=1;texinfo.push_back(t);faces.push_back(f);
   }
  }return base;
 };
 // Node zero bounds the map. The outside remains solid, protecting the seal.
 renderBox(bounds,-2,-1,false);int root=-2;
 for(const auto &box:boxes)root=renderBox(box,-1,root,true);
 nodes[5].child[1]=int16_t(root);
 auto collision=[&](Vec mins,Vec maxs){int base=int(clips.size());
  auto add=[&](Box box,int inside,int outside){int start=int(clips.size());clips.resize(start+6);for(int k=0;k<6;++k){int axis=k/2;clips[start+k].plane=makePlane(axis,(k&1)?box.hi[axis]:box.lo[axis]);clips[start+k].child[k&1]=int16_t(k==5?inside:start+k+1);clips[start+k].child[(k&1)^1]=int16_t(outside);}return start;};
  Box interior=bounds;for(int a=0;a<3;++a){interior.lo[a]-=mins[a];interior.hi[a]-=maxs[a];}add(interior,-1,-2);int current=-1;
  for(auto box:boxes){for(int a=0;a<3;++a){box.lo[a]-=maxs[a];box.hi[a]-=mins[a];}current=add(box,-2,current);}clips[base+5].child[1]=int16_t(current);return base;
 };
 Model model{};for(int a=0;a<3;++a){model.lo[a]=bounds.lo[a];model.hi[a]=bounds.hi[a];}model.heads[0]=0;model.heads[1]=collision({-16,-16,-24},{16,16,32});model.heads[2]=collision({-32,-32,-24},{32,32,64});model.heads[3]=model.heads[2];model.leaves=1;model.count=int(faces.size());
 std::array<Bytes,15> lumps;
 const char entities[]=R"({
"classname" "worldspawn"
"message" "Aftershock - original structural test arena"
}
{
"classname" "info_player_start"
"origin" "0 -200 24"
"angle" "90"
}
{
"classname" "weapon_rocketlauncher"
"origin" "0 -168 16"
}
{
"classname" "item_rockets"
"origin" "0 -144 16"
"spawnflags" "1"
}
)";
 lumps[0]=Bytes(entities,entities+sizeof(entities));lumps[1]=encode(planes);
 append(lumps[2],int32_t(1));append(lumps[2],int32_t(8));char name[16]="as_stone";for(char c:name)append(lumps[2],c);append(lumps[2],uint32_t(64));append(lumps[2],uint32_t(64));uint32_t offset=40;for(int mip=0;mip<4;++mip){append(lumps[2],offset);offset+=(64>>mip)*(64>>mip);}for(int mip=0;mip<4;++mip){int size=64>>mip;for(int y=0;y<size;++y)for(int x=0;x<size;++x)append(lumps[2],uint8_t(((x<<mip)%32==0 || (y<<mip)%16==0)?16:24+((x^y)&3)));}
 lumps[3]=encode(vertices);lumps[4]={1};lumps[5]=encode(nodes);lumps[6]=encode(texinfo);lumps[7]=encode(faces);lumps[9]=encode(clips);
 Leaf solid{};solid.contents=-2;solid.visibility=-1;append(lumps[10],solid);Leaf empty{};empty.contents=-1;empty.visibility=0;empty.count=uint16_t(faces.size());for(int a=0;a<3;++a){empty.lo[a]=int16_t(bounds.lo[a]);empty.hi[a]=int16_t(bounds.hi[a]);}append(lumps[10],empty);
 for(uint16_t i=0;i<faces.size();++i)append(lumps[11],i);lumps[12]=encode(edges);lumps[13]=encode(surfedges);append(lumps[14],model);
 Bytes file;append(file,int32_t(29));int32_t cursor=124;for(const auto &lump:lumps){append(file,cursor);append(file,int32_t(lump.size()));cursor+=int32_t(lump.size());}for(const auto &lump:lumps)file.insert(file.end(),lump.begin(),lump.end());
 std::filesystem::path destination=std::filesystem::u8path(argv[1]);std::filesystem::create_directories(destination.parent_path());std::ofstream output(destination,std::ios::binary);output.write(reinterpret_cast<const char *>(file.data()),file.size());if(!output)return 2;
 std::cout<<"Generated original BSP29 arena: "<<faces.size()<<" faces, "<<file.size()<<" bytes\n";return 0;
}
