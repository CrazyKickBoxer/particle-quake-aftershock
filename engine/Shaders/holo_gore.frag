/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
#version 460
#extension GL_GOOGLE_include_directive : enable
#include "holo_palette.glsl"
layout(location=0) in vec2 corner;
layout(location=1) in float opacity;
layout(location=2) flat in int style;
layout(location=3) flat in int slot;
layout(location=0) out vec4 color;
void main() {
    float r=dot(corner,corner);if(r>1.)discard;
    vec3 blood=holoPalette(style,slot,vec3(.5));
    float luminance=dot(blood,vec3(.2126,.7152,.0722));
    blood=mix(vec3(luminance*.6),blood,opacity);
    color=vec4(blood*(.65+exp(-r*16.)*.65),opacity*(1.-smoothstep(.4,1.,r)));
}
