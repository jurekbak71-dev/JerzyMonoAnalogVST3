#include "AnalogDSP.h"
#include <iostream>
#include <stdexcept>

static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main()
{
    try {
        for(double rate : {44100.0, 48000.0, 96000.0}) {
            jerzy::AnalogADSR e; e.prepare(rate); e.set(.01,.08,.3,.12); e.noteOn();
            const int a=static_cast<int>(std::llround(.01*rate)), d=static_cast<int>(std::llround(.08*rate)), r=static_cast<int>(std::llround(.12*rate));
            double v=0.0;
            for(int i=0;i<a;++i) v=e.process();
            check(std::abs(v-1.0)<1e-8,"Attack duration");
            for(int i=0;i<d;++i) v=e.process();
            check(std::abs(v-.3)<1e-8,"Decay duration");
            e.noteOff();
            for(int i=0;i<r;++i) v=e.process();
            check(v==0.0,"Release duration");
            e.noteOn(); for(int i=0;i<a/2;++i) v=e.process();
            const double previous=v; e.noteOn(); v=e.process();
            check(v>=previous && v-previous<.01,"Retrigger must preserve capacitor level");
            e.noteOff(); for(int i=0;i<r;++i) v=e.process(); check(v==0.0,"Early note-off release");
            e.reset(); e.set(.1,.08,.3,.12); e.noteOn();
            for(int i=0;i<static_cast<int>(rate*.02);++i) v=e.process();
            const double beforeEdit=v; e.set(.5,.08,.3,.12);v=e.process();
            check(v>=beforeEdit && v-beforeEdit<.001,"Live attack edit continuity");
            jerzy::BandLimitedOscillator osc; osc.prepare(rate*4,42); osc.setDriftCents(0); osc.setWave(jerzy::BandLimitedOscillator::Wave::triangle);
            for(double hz : {27.5,110.0,880.0}) {
                osc.setFrequency(hz); double sum=0, square=0;
                for(int i=0;i<static_cast<int>(rate*4);++i) {const double x=osc.process();sum+=x;square+=x*x;check(std::isfinite(x),"VCO finite");}
                check(std::abs(sum/(rate*4))<.08,"Triangle DC");
                check(std::sqrt(square/(rate*4))>.45,"Triangle amplitude");
            }
            jerzy::NonlinearLadder ladder; ladder.prepare(rate*4);
            for(double cut : {20.0,1200.0,20000.0}) for(double resonance : {0.0,.8,1.15}) for(double env : {-6.0,0.0,6.0}) {
                ladder.reset(); ladder.setParams(cut,resonance,1.0,0.0,60.0);
                for(int i=0;i<8000;++i) {
                    const double x=ladder.process(i==0?20.0:0.2*std::sin(i*.2),env);
                    check(std::isfinite(x)&&std::abs(x)<5.0,"Ladder stability");
                }
            }
            jerzy::MonoAnalogEngine engine; engine.prepare(rate,512);
            jerzy::MonoParameters p; p.ampRelease=.01;p.filterRelease=.01;engine.setParameters(p);engine.noteOn(36,1.0f);
            double energy=0;
            for(int i=0;i<8000;++i){double x=engine.processSample();energy+=x*x;check(std::isfinite(x),"Engine finite");}
            check(energy>.01,"Engine audible"); engine.noteOff(36);
            for(int i=0;i<static_cast<int>(rate);++i) engine.processSample();
            check(std::abs(engine.processSample())<1e-5,"Engine tail returns to silence");
        }
        double last=-2;
        for(int i=-1000;i<=1000;++i) {const double y=jerzy::saturateAsymmetric(i*.02);check(y>=last,"Saturation monotonic");last=y;}
        std::cout << "PASS: ADSR timing/retrigger, VCO DC/amplitude, ladder stability, engine tail, saturation\n";
        return 0;
    } catch(const std::exception& e) {std::cerr << e.what() << '\n';return 1;}
}
