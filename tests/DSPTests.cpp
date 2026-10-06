#include "AnalogDSP.h"
#include "SequencerClock.h"
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
            jerzy::ClassicMultimode classic; classic.prepare(rate*4);
            for(double cut : {8.0,1200.0,rate*2.0}) for(double res : {0.0,1.15}) {
                classic.reset();
                for(int i=0;i<16000;++i) {
                    const auto outputs=classic.process(i==0?20.0:std::sin(i*.2),cut,res,1.0);
                    for(double x : outputs) check(std::isfinite(x)&&std::abs(x)<100.0,"Multimode stability");
                }
            }
            auto response=[&](double hz){
                classic.reset();std::array<double,6> energy{};
                const int count=static_cast<int>(rate*4*.1);
                for(int i=0;i<count*2;++i) {
                    const auto outputs=classic.process(.001*std::sin(juce::MathConstants<double>::twoPi*hz*i/(rate*4)),1000.0,0.0,0.0);
                    if(i>=count) for(size_t mode=0;mode<6;++mode) energy[mode]+=outputs[mode]*outputs[mode];
                }
                for(auto& x:energy) x=std::sqrt(x/count);return energy;
            };
            const auto low=response(50),centre=response(1000),high=response(8000),octave=response(4000);
            check(low[0]>high[0]*20 && low[1]>high[1]*200,"LP rejects high frequencies");
            check(high[2]>low[2]*100 && high[3]>low[3]*1000,"HP rejects low frequencies");
            check(centre[4]>low[4]*8 && centre[4]>high[4]*4,"BP response");
            check(high[1]<high[0]*.05 && low[3]<low[2]*.01,"24 dB differs from 12 dB");
            const double slope12=20*std::log10(octave[0]/high[0]),slope24=20*std::log10(octave[1]/high[1]);
            check(slope12>10 && slope12<15 && slope24>21 && slope24<28,"LP octave slopes");
            auto pitch=[&](double depth){
                jerzy::MonoAnalogEngine voice;voice.prepare(rate,512);jerzy::MonoParameters patch;
                patch.osc1Wave=jerzy::BandLimitedOscillator::Wave::sine;patch.osc2Level=patch.subLevel=0;
                patch.analogDriftCents=0;patch.cutoffHz=20000;patch.filterEnvOct=0;patch.filterSustain=1;
                patch.filterAttack=.0005;patch.modEnvPitch=depth;voice.setParameters(patch);voice.noteOn(57,1);
                double previous=0;int crosses=0;
                const int count=static_cast<int>(rate*.2);
                for(int i=0;i<count*2;++i){const double x=voice.processSample();if(i>=count&&previous<0&&x>=0)++crosses;previous=x;}
                return crosses/.2;
            };
            check(std::abs(pitch(12)/pitch(0)-2.0)<.07,"Envelope pitch positive octave");
            check(std::abs(pitch(-12)/pitch(0)-.5)<.05,"Envelope pitch negative octave");
            auto pwmRender=[&](double depth,jerzy::BandLimitedOscillator::Wave wave){
                jerzy::MonoAnalogEngine voice;voice.prepare(rate,512);jerzy::MonoParameters patch;
                patch.osc1Wave=wave;patch.osc2Level=patch.subLevel=0;patch.analogDriftCents=0;
                patch.cutoffHz=20000;patch.filterEnvOct=0;patch.filterSustain=1;patch.filterAttack=.0005;
                patch.modEnvPWM=depth;voice.setParameters(patch);voice.noteOn(57,1);
                std::vector<double> result;for(int i=0;i<4096;++i)result.push_back(voice.processSample());return result;
            };
            auto squareA=pwmRender(0,jerzy::BandLimitedOscillator::Wave::square),squareB=pwmRender(.8,jerzy::BandLimitedOscillator::Wave::square);
            auto sineA=pwmRender(0,jerzy::BandLimitedOscillator::Wave::sine),sineB=pwmRender(.8,jerzy::BandLimitedOscillator::Wave::sine);
            double squareDifference=0,sineDifference=0;
            for(size_t i=0;i<squareA.size();++i){squareDifference+=std::abs(squareA[i]-squareB[i]);sineDifference+=std::abs(sineA[i]-sineB[i]);}
            check(squareDifference>1.0,"Envelope PWM changes pulse sound");
            check(sineDifference<1e-8,"Envelope PWM leaves sine unchanged");
            jerzy::MonoAnalogEngine engine; engine.prepare(rate,512);
            jerzy::MonoParameters p; p.ampRelease=.01;p.filterRelease=.01;engine.setParameters(p);engine.noteOn(36,1.0f);
            double energy=0;
            for(int i=0;i<8000;++i){double x=engine.processSample();energy+=x*x;check(std::isfinite(x),"Engine finite");}
            check(energy>.01,"Engine audible");
            auto renderDrive=[&](double mixer,double output){
                jerzy::MonoAnalogEngine v;v.prepare(rate,512);jerzy::MonoParameters q;
                q.osc1Wave=jerzy::BandLimitedOscillator::Wave::saw;q.osc2Level=.35;q.subLevel=.15;
                q.analogDriftCents=0;q.cutoffHz=20000;q.filterEnvOct=0;q.ampAttack=.001;q.ampDecay=.001;q.ampSustain=1.0;
                q.mixerDrive=mixer;q.outputDrive=output;q.master=.5;v.setParameters(q);v.noteOn(48,1.0f);
                std::vector<double> out;out.reserve(4096);for(int i=0;i<4096;++i)out.push_back(v.processSample());return out;
            };
            auto clean=renderDrive(0.0,0.0),mixDriven=renderDrive(.9,0.0),outDriven=renderDrive(0.0,.9);
            double mixerDelta=0.0,outputDelta=0.0;
            for(size_t i=0;i<clean.size();++i){mixerDelta+=std::abs(clean[i]-mixDriven[i]);outputDelta+=std::abs(clean[i]-outDriven[i]);}
            check(mixerDelta>0.1,"Mixer drive must audibly alter the signal");
            check(outputDelta>0.1,"Output drive must audibly alter the signal");
            auto renderMod=[&](int route){
                jerzy::MonoAnalogEngine v;v.prepare(rate,512);jerzy::MonoParameters q;
                q.osc1Level=.35;q.osc2Level=.75;q.subLevel=0;q.analogDriftCents=0;
                q.cutoffHz=3000;q.resonance=.2;q.filterEnvOct=0;q.mixerDrive=.2;
                q.filterAttack=.0005;q.filterDecay=.01;q.filterSustain=1;
                q.ampAttack=.0005;q.ampDecay=.01;q.ampSustain=1;
                if(route==0)q.modEnvOsc2Pitch=12;
                if(route==1)q.modEnvResonance=.65;
                if(route==2)q.modEnvMixDrive=.75;
                if(route==3)q.modEnvAmp=.8;
                v.setParameters(q);v.noteOn(48,1);
                std::vector<double> out;out.reserve(8192);
                for(int i=0;i<8192;++i){const double x=v.processSample();check(std::isfinite(x),"Modulation remains finite");out.push_back(x);}
                return out;
            };
            const auto unmodulated=renderMod(-1);
            for(int route=0;route<4;++route){
                const auto modulated=renderMod(route);double difference=0;
                for(size_t i=0;i<unmodulated.size();++i)difference+=std::abs(unmodulated[i]-modulated[i]);
                check(difference>.1,"Envelope route must change sound");
            }
            for(int mode=0;mode<7;++mode){p.filterMode=static_cast<jerzy::FilterMode>(mode);p.resonance=1.15;p.modEnvPWM=-1.0;engine.setParameters(p);for(int i=0;i<1024;++i)check(std::isfinite(engine.processSample()),"Filter mode switching");}
            p.resonance=0;p.modEnvPWM=0;engine.setParameters(p);engine.noteOff(36);
            for(int i=0;i<static_cast<int>(rate);++i) engine.processSample();
            check(std::abs(engine.processSample())<1e-5,"Engine tail returns to silence");
        }
        double last=-2;
        for(int i=-1000;i<=1000;++i) {const double y=jerzy::saturateAsymmetric(i*.02);check(y>=last,"Saturation monotonic");last=y;}
        check(jerzy::swungStepAtPpq(0.0,.25,.25)==0,"Swing starts on step zero");
        check(jerzy::swungStepAtPpq(.25,.25,.25)==0,"Swing delays odd step");
        check(jerzy::swungStepAtPpq(.3125,.25,.25)==1,"Swing triggers odd step at delayed boundary");
        check(jerzy::swungStepAtPpq(.5,.25,.25)==2,"Swing preserves paired step duration");
        check(jerzy::swungStepAtPpq(.75,.25,.25)==2,"Swing delays subsequent odd step");
        check(jerzy::swungStepAtPpq(.75,.25,0.0)==3,"Zero swing follows straight PPQ");
        std::cout << "PASS: ADSR timing/retrigger, VCO DC/amplitude, ladder stability, engine tail, saturation\n";
        return 0;
    } catch(const std::exception& e) {std::cerr << e.what() << '\n';return 1;}
}
