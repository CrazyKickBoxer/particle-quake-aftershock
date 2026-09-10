/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
// Included only in explicitly compiled Holo variants. Off binaries are unchanged.
struct HoloParticle { vec3 position; uint pointId; vec3 velocity; float freeTimer;
    uint flags; uint seed; float freeLifetime; uint sourceRef; };
layout(std430,set=5,binding=0) readonly buffer HoloState { HoloParticle hpState[]; };
layout(std430,set=5,binding=1) readonly buffer HoloHomes { vec4 hpHome[]; };
layout(std430,set=5,binding=3) readonly buffer HoloBits { uint hpBits[]; };
layout(std430,set=5,binding=7) readonly buffer HoloLookup { uvec2 hpLookup[]; };
layout(location=8) out float hpHeat;
layout(location=9) flat out int hpColorSlot;
layout(location=10) out vec3 hpWorld;
uint holoHash(uint x) { x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; return x^(x>>16); }
vec3 holoDisplace(vec3 center,uint id) {
    hpHeat=0.0;
    hpColorSlot=0;
    hpWorld=center;
    if((hpBits[id>>5]&(1u<<(id&31u)))==0u) return center;
    uint capacity=uint(hpLookup.length());
    for(uint i=0u;i<32u;++i) {
        uvec2 entry=hpLookup[(holoHash(id)+i)%capacity];
        if(entry.x==id) {
            hpHeat=pow(clamp(hpState[entry.y].freeTimer/max(.05,hpState[entry.y].freeLifetime),0.,1.),6.);
            hpColorSlot=int((hpState[entry.y].flags>>8u)&7u);
            if(hpColorSlot==0) hpHeat=0.;
            hpWorld=center+(hpState[entry.y].position-hpHome[id].xyz);return hpWorld;
        }
        if(entry.x==0xffffffffu) break;
    }
    return center;
}
