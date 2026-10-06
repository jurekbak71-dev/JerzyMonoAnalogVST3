#include "Core.h"
#include "reference/Core030.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <iostream>
#include <iomanip>

static size_t allocated=0;
void* operator new(std::size_t n) { if(void* p=std::malloc(n?n:1)){allocated+=n;return p;}throw std::bad_alloc(); }
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
template<class P> void setup(P& p,int scene) {
    if(scene==1) {p.source=1;p.direct=true;p.guitarTone=.8;}
    if(scene>=2) {p.pad=true;p.padAttack=.05;p.evolution=.5;p.padRelease=2;}
    if(scene==2) {p.bass=false;p.engine1=3;p.engine2=2;}
    if(scene==3) {p.bass=false;p.backgroundOn=false;p.rhythmOn=true;p.rhythmDensity=1;p.rhythmDivision=3;p.rhythmSound=0;}
    if(scene==4) {p.bass=false;p.padMode=2;p.eventsOn=true;p.rhythmOn=true;p.krellRate=.125;p.krellAttack=.01;p.krellRelease=.5;p.engine1=3;p.engine2=4;}
    if(scene>=5) {p.eventsOn=true;p.rhythmOn=true;p.rhythmDensity=1;p.krellRate=.125;p.source=1;p.styleFamily=1;p.masterFX=.5;p.fxGrain=p.padRack.fxGrain=p.masterRack.fxGrain=true;p.padMode=2;p.rhythmSound=1;}
}
template<class I,class P> double benchmark(int scene,int samples,double& sum) {
    I instrument;P p;setup(p,scene);instrument.prepare(48000);instrument.beginBlock(0,120,true);
    if(scene>=0)instrument.noteOn(36,1,.8,p,0);
    auto start=std::chrono::steady_clock::now();
    for(int i=0;i<samples;++i){auto x=instrument.next(p,double(i)/24000);sum+=x.l+x.r;}
    return std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
}
int main() {
 try {
    size_t beforeBytes=0,afterBytes=0;
    {reference030::Instrument i;allocated=0;i.prepare(48000);beforeBytes=allocated;}
    {jerzy::Instrument i;allocated=0;i.prepare(48000);afterBytes=allocated;}
    require(afterBytes<beforeBytes*.6,"FX memory reduction below 40 percent");
    std::cout<<"DSP buffer allocation 48 kHz: "<<beforeBytes<<" -> "<<afterBytes<<" bytes\n";
    double worst=0,referenceEnergy=0,errorEnergy=0;
    // Automation, scene-style switches, note-off tails, transport reset, CC/bend, and all voice types.
    for(int scene=0;scene<7;++scene) {
        reference030::Instrument a;jerzy::Instrument b;reference030::Parameters pa;jerzy::Parameters pb;
        setup(pa,scene);setup(pb,scene);const double sr=scene==6?96000:48000;
        a.prepare(sr);b.prepare(sr);a.beginBlock(0,120,true);b.beginBlock(0,120,true);
        a.noteOn(36,1,.8,pa,0);b.noteOn(36,1,.8,pb,0);
        double ppq=0,bpm=120;
        const int count=int(sr*3);
        for(int i=0;i<count;++i) {
            if(i==count/4){a.pitchBend(1,9000);b.pitchBend(1,9000);pa.guitarTone=pb.guitarTone=.2;pa.rhythmSound=pb.rhythmSound=3;}
            if(i==count/3){pa.order=pb.order={{5,4,3,2,1,0}};pa.padRack.order=pb.padRack.order={{3,1,5,2,0,4}};}
            if(i==count/2){a.noteOn(43,1,.6,pa,ppq);b.noteOn(43,1,.6,pb,ppq);pa.padRack.freeze=pb.padRack.freeze=true;}
            if(i==count*2/3){a.allOff();b.allOff();}
            if(i%128==0) {a.beginBlock(ppq,bpm,true);b.beginBlock(ppq,bpm,true);}
            if(i>count/5&&i<count/4){pa.cutoff=pb.cutoff=500+double(i%1000);pa.padRack.delayTone=pb.padRack.delayTone=1000+double(i%500);}
            auto x=a.next(pa,ppq);auto y=b.next(pb,ppq);ppq+=bpm/(60*sr);
            require(std::isfinite(y.l)&&std::isfinite(y.r),"finite optimised render");
            worst=std::max({worst,std::abs(x.l-y.l),std::abs(x.r-y.r)});
            referenceEnergy+=x.l*x.l+x.r*x.r;errorEnergy+=(x.l-y.l)*(x.l-y.l)+(x.r-y.r)*(x.r-y.r);
        }
    }
    std::cout<<std::setprecision(12)<<"Full instrument null: peak="<<worst<<" relative error="<<std::sqrt(errorEnergy/referenceEnergy)<<"\n";
    require(worst<1e-8,"0.3.0 null comparison");
    // Buffer bounds: minimum tempo, longest delay, extreme wow/flutter/stereo,
    // granular size/pitch/scatter + freeze, and largest reverb, including ring wrap.
    for(double sr:{44100.0,96000.0}) {
        reference030::FXRack a;jerzy::FXRack b;reference030::FXParameters pa;jerzy::FXParameters pb;
        pa.fxGrain=pb.fxGrain=true;pa.grainSize=pb.grainSize=.6;pa.grainPitch=pb.grainPitch=12;pa.grainScatter=pb.grainScatter=1;
        pa.delayDivision=pb.delayDivision=0;pa.delayMode=pb.delayMode=1;pa.wow=pb.wow=1;pa.flutter=pb.flutter=1;pa.reverbSize=pb.reverbSize=1;
        pa.feedback=pb.feedback=.94;pa.chorusDepth=pb.chorusDepth=1;
        a.prepare(sr);b.prepare(sr);double diff=0;
        for(int i=0;i<int(sr*7);++i) {
            if(i==int(sr*2))pa.freeze=pb.freeze=true;
            if(i==int(sr*4))pa.freeze=pb.freeze=false;
            double v=i<int(sr)?std::sin(i*.017)*.2:0;
            auto x=a.process({v,v*.7},pa,20);auto y=b.process({v,v*.7},pb,20);
            diff=std::max({diff,std::abs(x.l-y.l),std::abs(x.r-y.r)});
        }
        std::cout<<"FX maximum-range null "<<sr<<" Hz: "<<diff<<"\n";require(diff<1e-8,"full FX range and freeze retained");
    }
    const char* names[]{"Idle","Analog bass","Bass guitar","Ambient FM","Rhythm chords","Krell","All layers + FX"};
    double sum=0;
    for(int scene=-1;scene<=5;++scene) {
        double oldTime=benchmark<reference030::Instrument,reference030::Parameters>(scene,96000,sum);
        double newTime=benchmark<jerzy::Instrument,jerzy::Parameters>(scene,96000,sum);
        std::cout<<names[scene+1]<<": "<<oldTime<<" -> "<<newTime<<" seconds; CPU reduction "<<100*(1-newTime/oldTime)<<"%\n";
    }
    require(std::isfinite(sum),"benchmark output");std::cout<<"PASS unchanged DSP quality and full ranges\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
