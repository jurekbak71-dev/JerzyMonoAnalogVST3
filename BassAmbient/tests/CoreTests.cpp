#include "Core.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

static int checks=0;
void check(bool pass,const std::string& name) { ++checks;if(!pass) throw std::runtime_error(name); }
double energy(jerzy::Instrument& synth,const jerzy::Parameters& p,int samples,double sr,double& ppq) {
    double total=0;for(int i=0;i<samples;++i) {
        auto out=synth.next(p,ppq);ppq+=120/(60*sr);
        check(std::isfinite(out.l)&&std::isfinite(out.r),"finite audio");
        check(std::abs(out.l)<=1.00001&&std::abs(out.r)<=1.00001,"safe output");
        total+=out.l*out.l+out.r*out.r;
    }return total;
}
int main() {
 try {
    using namespace jerzy;
    Envelope env;env.prepare(48000);env.on();
    double v=0;for(int k=0;k<480;++k)v=env.next(.01,.1,.5,.1);
    check(v>.98,"ADSR attack");for(int k=0;k<5000;++k)v=env.next(.01,.1,.5,.1);
    check(std::abs(v-.5)<.01,"ADSR sustain");env.off();for(int k=0;k<5000;++k)v=env.next(.01,.1,.5,.1);
    check(!env.active()&&v==0,"ADSR release");
    Parameters p;
    check(scaleOffset(-1,0)==-1,"negative scale degrees");check(scaleOffset(7,1)==12,"scale octave");
    for(int style=0;style<8;++style) { p.style=style;for(int i=0;i<64;++i) {
        auto a=makeStep(p,i,16),b=makeStep(p,i+16,16);
        check(a.degree==b.degree&&a.rest==b.rest&&a.slide==b.slide,"repeatable phrase");
    }}
    p.style=0; p.evolve=true;bool different=false;
    for(int i=0;i<16;++i) { auto a=makeStep(p,i,16),b=makeStep(p,i+16,16);different|=a.degree!=b.degree||a.rest!=b.rest||a.slide!=b.slide; }
    check(different,"phrase evolution");p.evolve=false;
    check(divisionBeats(4)==1.0/3,"triplet division");
    {
        DelayLine line;line.prepare(192000);
        line.push(1);
        check(std::isfinite(line.read(std::nextafter(1.0,2.0))),"fractional delay wrap boundary");
    }
    for(double sr:{44100.0,48000.0,96000.0}) for(int model=0;model<4;++model) {
        Instrument s;s.prepare(sr);p=Parameters{};p.model=model;p.direct=true;double ppq=0;
        s.beginBlock(0,120,true);s.noteOn(36,1,1,p,0);
        check(energy(s,p,int(sr*.3),sr,ppq)>.001,"audible bass model");
        s.noteOff(36,1,p,ppq);check(s.currentRoot()==-1,"note-off root");
        energy(s,p,int(sr*.2),sr,ppq);
    }
    {
        Instrument s;s.prepare(48000);p=Parameters{};
        s.noteOn(36,1,1,p,0);s.noteOn(43,1,.7,p,.1);check(s.currentRoot()==43,"last-note priority");
        s.noteOff(43,1,p,.2);check(s.currentRoot()==36,"held note fallback");
        s.controller(1,64,127,p,.2);s.noteOff(36,1,p,.3);check(s.currentRoot()==36,"sustain pedal");
        s.controller(1,64,0,p,.4);check(s.currentRoot()==-1,"sustain pedal release");
        p.latch=true;s.noteOn(40,2,.8,p,.4);s.noteOff(40,2,p,.5);check(s.currentRoot()==40,"latch");
        s.allOff();check(s.currentRoot()==-1,"panic defeats latch");
        s.beginBlock(1,120,true);s.noteOn(36,1,1,p,1);s.beginBlock(0,120,true);check(s.currentRoot()==-1,"loop seek clears stale notes");
        s.noteOn(36,1,1,p,0);s.beginBlock(.01,120,false);check(s.currentRoot()==-1,"transport stop");
    }
    {
        Instrument a,b;a.prepare(48000);b.prepare(48000);p=Parameters{};p.bass=false;p.pad=true;p.padAttack=.1;p.evolution=.7;
        a.noteOn(48,1,1,p,0);b.noteOn(48,1,1,p,0);
        double diff=0,total=0;for(int i=0;i<96000;++i) {
            auto x=a.next(p,double(i)/24000),y=b.next(p,double(i)/24000);
            check(std::abs(x.l-y.l)<1e-12&&std::abs(x.r-y.r)<1e-12,"deterministic ambient");
            diff+=std::abs(x.l-x.r);total+=std::abs(x.l)+std::abs(x.r);
        }
        check(total>.1&&diff>.1,"audible stereo ambient");
    }
    {
        FXRack rack;rack.prepare(48000);p=Parameters{};p.fxDrive=false;p.fxChorus=false;p.fxReverb=false;p.fxWidth=false;p.delayMode=2;p.delayMix=.7;
        double tail=0;for(int i=0;i<60000;++i) { auto x=rack.process(i==0?Stereo{1,0}:Stereo{},p,120);check(std::isfinite(x.l)&&std::isfinite(x.r),"FX finite");if(i>1000)tail+=std::abs(x.l)+std::abs(x.r); }
        check(tail>.01,"delay impulse tail");
        p.order={{5,5,-1,80,1,1}};p.fxGrain=true;p.freeze=true;
        for(int i=0;i<1000;++i) { auto x=rack.process({},p,120);check(std::isfinite(x.l),"invalid FX order sanitised"); }
    }
    {
        Instrument s;s.prepare(48000);p=Parameters{};p.meter=1;p.numerator=7;p.denominator=8;p.division=1;p.density=1;
        s.noteOn(36,1,1,p,0);s.next(p,0);check(s.stepPosition()==0,"first step");
        s.next(p,.5);check(s.stepPosition()==1,"eighth-note step");s.next(p,3.5);check(s.stepPosition()==7,"7/8 wrap timeline");
        p.restart=true;s.noteOn(43,1,1,p,4.1);s.next(p,4.1);check(s.stepPosition()==0,"root restart anchor");
    }
    // Check routing with sample-for-sample comparisons, not only parameter storage.
    auto compareRacks=[](Parameters a,Parameters b,int samples=24000) {
        Instrument x,y;x.prepare(48000);y.prepare(48000);
        x.noteOn(48,1,1,a,0);y.noteOn(48,1,1,b,0);double difference=0;
        for(int i=0;i<samples;++i) {
            auto u=x.next(a,double(i)/24000),v=y.next(b,double(i)/24000);
            difference+=std::abs(u.l-v.l)+std::abs(u.r-v.r);
        }
        return difference;
    };
    {
        Parameters a;a.direct=true;a.bassFX=1;auto b=a;
        b.padRack.fxSaturation=1;b.padRack.order={{5,4,3,2,1,0}};
        check(compareRacks(a,b)==0,"ambient rack cannot change solo bass");
        a.bass=false;a.pad=true;a.padAttack=.05;b=a;b.fxSaturation=1;b.order={{5,4,3,2,1,0}};
        check(compareRacks(a,b)==0,"bass rack cannot change solo ambient");
        b=a;b.padRack.fxSaturation=1;
        check(compareRacks(a,b)>.1,"ambient FX controls affect ambient audio");
        a=Parameters{};a.direct=true;b=a;b.masterRack.fxSaturation=1;
        check(compareRacks(a,b)==0,"master amount zero is dry");
        a.masterFX=1;b=a;b.masterRack.fxSaturation=1;
        check(compareRacks(a,b)>.1,"master rack affects layer sum");
        a.pad=true;a.padAttack=.05;a.masterRack.fxGrain=true;a.masterRack.freeze=true;
        b=a;b.masterRack.order={{5,4,3,2,1,0}};
        check(std::isfinite(compareRacks(a,b)),"three racks granular freeze remain finite");
    }
    {
        Parameters a;a.lockNotes=true;a.lockRhythm=true;a.phraseSeed=a.seed;a.rhythmSeed=a.seed;
        Parameters b=a;b.seed=901;b.mutation=5;
        for(int i=0;i<32;++i){auto x=makeStep(a,i,32),y=makeStep(b,i,32);check(x.degree==y.degree&&x.rest==y.rest&&x.octave==y.octave,"locked note/rhythm survive generation");}
        a=Parameters{};a.variation=.25;b=a;b.mutation=1;int changes=0;
        for(int i=0;i<64;++i){auto x=makeStep(a,i,64),y=makeStep(b,i,64);changes+=x.degree!=y.degree||x.rest!=y.rest||x.slide!=y.slide;}
        check(changes>0&&changes<32,"mutation preserves most of motif");
        a.style=5;a.substyle=0;b=a;b.substyle=2;changes=0;
        for(int i=0;i<32;++i){auto x=makeStep(a,i,32),y=makeStep(b,i,32);changes+=x.degree!=y.degree||x.rest!=y.rest;}
        check(changes>0,"rock substyles change actual phrase");
    }
    {
        Instrument synth;synth.prepare(48000);Parameters q;q.pad=true;q.backgroundOn=false;q.bass=false;q.eventsOn=true;q.rhythmOn=true;q.krellRate=.125;q.krellAttack=.005;q.krellRelease=.1;q.rhythmDensity=1;
        struct Counts {int bass=0,rhythm=0,events=0;} counts;
        synth.observerContext=&counts;synth.observer=[](void* c,int,int channel,double velocity,double){if(velocity>0){auto& n=*static_cast<Counts*>(c);if(channel==1)++n.bass;else if(channel==2)++n.rhythm;else if(channel==3)++n.events;}};
        synth.setPreview(false,true,36,q,0);double ppq=0;
        check(energy(synth,q,96000,48000,ppq)>.01,"independent rhythm and Krell preview audible");
        check(counts.bass==0&&counts.rhythm>=8&&counts.events>3,"ambient preview has no bass MIDI; both generators operate");
        synth.setPreview(false,false,36,q,ppq);int before=counts.rhythm+counts.events;
        energy(synth,q,24000,48000,ppq);check(counts.rhythm+counts.events==before,"preview stop ends new events");
        check(synth.currentRoot()==-1,"preview stop releases root");
    }
    {
        Parameters a;a.bass=false;a.pad=true;a.backgroundOn=false;a.rhythmOn=true;a.rhythmDensity=1;a.padFX=0;
        auto b=a;b.rhythmOn=false;check(compareRacks(a,b)>.1,"rhythmic chord layer is independent of pad");
        a.rhythmOn=false;a.eventsOn=true;a.krellAttack=.005;a.krellRate=.125;b=a;b.krellIndependence=0;
        check(compareRacks(a,b)>.01,"Krell oscillator independence changes audio");
        a=Parameters{};a.model=3;a.direct=true;a.cutoff=7000;b=a;b.articulation=1;
        check(compareRacks(a,b)>.1,"finger and pick string models differ");
        a.bass=false;a.pad=true;a.padAttack=.05;a.artifactRate=1;a.artifactDepth=1;b=a;b.artifactDepth=0;
        check(compareRacks(a,b,96000)>.1,"artifacts alter dry ambient");
    }
    // CPU sanity benchmark, release build only: report, don't depend on machine speed.
    Instrument s;s.prepare(48000);p=Parameters{};p.pad=true;p.fxGrain=true;s.noteOn(36,1,1,p,0);double ppq=0;
    auto start=std::chrono::steady_clock::now();auto total=energy(s,p,48000,48000,ppq);
    check(total>.01,"combined layers");
    std::cout<<"PASS "<<checks<<" checks. 1 second combined audio rendered in "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<" seconds\n";
    return 0;
 } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n";return 1; }
}
