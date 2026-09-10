/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
// Semantic Holo palette adapter. All effect colours live here, not in physics.
vec3 holoPalette(int style,int slot,vec3 neutral) {
    vec3 hot=neutral/max(.05,max(neutral.r,max(neutral.g,neutral.b)));
    if(style==2 || style==5) hot=vec3(1.4,.33,.055);
    else if(style==10) hot=vec3(.12,.55,.43);
    else if(style==12) hot=vec3(.07,.48,.95);
    else if(style==13) hot=vec3(.42,.13,.8);
    else if(style==14) hot=vec3(.015,.69,1.);
    if(slot==0) return neutral;
    if(slot==1) return hot*2.0;
    if(slot==2) return mix(hot,vec3(1),.82)*3.0;
    if(slot==3) return style==10?vec3(.20,.025,.10):style==13?vec3(.30,.015,.12):vec3(.32,.012,.025);
    return mix(hot,vec3(.15,.45,.8),.5);
}
vec3 holoFlash(vec3 neutral,int style,float heat) {
    if(heat<=0.0) return neutral;
    return mix(neutral,holoPalette(style,heat>.75?2:1,neutral),clamp(heat,0.,1.));
}
