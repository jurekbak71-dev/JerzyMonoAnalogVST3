#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace jerzy {
struct GridTime {int step;double start,duration,phase;};
inline GridTime gridTime(double beat,double division,double swing) {
    const int pair=(int)std::floor(beat/(division*2));
    const double start=pair*division*2,first=division*(1+swing);
    const bool odd=beat>=start+first;
    const double position=start+(odd?first:0),duration=odd?division*2-first:first;
    return {pair*2+(odd?1:0),position,duration,std::clamp((beat-position)/duration,0.0,1.0)};
}
inline uint32_t gridHash(int n,int track) {uint32_t x=(uint32_t)n^((uint32_t)track*0x9e3779b9u)^0x51a7u;x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
inline int gridIndex(int tick,int length,int direction,int track) {
    const int n=(tick%length+length)%length;
    if(direction==1)return length-1-n;
    if(direction==2 && length>1){const int span=length*2-2,k=(tick%span+span)%span;return k<length?k:span-k;}
    if(direction==3)return (int)(gridHash(tick,track)%(uint32_t)length);
    return n;
}
}
