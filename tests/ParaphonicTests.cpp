#include "VoiceEngine.h"
#include <iostream>
#include <memory>
#include <stdexcept>

static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static double energy(jerzy::VoiceEngine& voice,int count) {
    double result=0;for(int i=0;i<count;++i){const double y=voice.processSample();check(std::isfinite(y),"Finite paraphonic output");result+=y*y;}
    return result/count;
}
int main(){try{
    auto storage=std::make_unique<jerzy::VoiceEngine>();auto& voice=*storage;
    jerzy::MonoParameters p;p.osc1Wave=p.osc2Wave=jerzy::BandLimitedOscillator::Wave::sine;
    p.osc1Level=1;p.osc2Level=p.subLevel=p.noiseLevel=0;p.analogDriftCents=0;
    p.cutoffHz=18000;p.filterEnvOct=p.keyTrack=p.mixerDrive=p.outputDrive=0;
    p.ampAttack=.1;p.ampDecay=.1;p.ampSustain=.25;p.ampRelease=.4;
    voice.prepare(48000,512);voice.setMode(1);voice.setParameters(p);voice.midiNoteOn(1,69,1);
    const auto early=energy(voice,480);energy(voice,3360);const auto peak=energy(voice,960);
    energy(voice,6000);const auto sustain=energy(voice,2400);
    check(peak>early*10,"AMP Attack shapes the paraphonic audio");
    check(sustain<peak*.15 && sustain>peak*.025,"AMP Decay and Sustain shape paraphonic audio");
    voice.midiNoteOff(1,69);energy(voice,1440);const auto tail=energy(voice,1440);
    check(tail>sustain*.05,"Last note retains oscillator source during AMP Release");
    energy(voice,21000);check(energy(voice,2400)<sustain*1e-5,"AMP Release eventually reaches silence");
    p.ampAttack=p.ampDecay=.001;p.ampSustain=1;p.ampRelease=.005;
    voice.setParameters(p);voice.midiNoteOn(1,69,1);energy(voice,4800);voice.midiNoteOff(1,69);
    energy(voice,9600);check(energy(voice,2400)<sustain*1e-5,"Short AMP Release also works after filter/DC settling");

    // Channel addressing must be independent of arrival order and pitch identity.
    voice.prepare(48000,512);voice.setMode(1);p.osc1Level=0;p.osc2Level=1;p.ampRelease=.1;voice.setParameters(p);
    voice.midiNoteOn(1,69,1);check(energy(voice,4800)<1e-12,"CH1 cannot trigger OSC2");
    voice.midiNoteOn(2,69,1);const auto second=energy(voice,4800);check(second>.001,"CH2 explicitly triggers OSC2");
    voice.midiNoteOff(1,69);check(energy(voice,4800)>second*.8,"CH1 same-pitch note-off does not release OSC2");
    voice.midiNoteOff(2,69);energy(voice,9600);check(energy(voice,2400)<second*1e-5,"CH2 releases its own oscillator");
    voice.midiNoteOn(3,69,1);check(energy(voice,4800)<1e-12,"Unmapped channels do not steal para oscillators");
    voice.midiNoteOn(2,48,1);energy(voice,4800);voice.midiNoteOn(2,72,1);energy(voice,4800);
    voice.midiNoteOff(2,72);int crossings=0;float previous=0;
    for(int i=0;i<4800;++i){const float y=voice.processSample();if(previous<0 && y>=0)++crossings;previous=y;}
    check(crossings>=12 && crossings<=14,"OSC2 returns to its own previously held MIDI key");
    voice.midiNoteOff(1,48);check(energy(voice,4800)>second*.8,"Unrelated note-off leaves OSC2 playing");
    voice.midiAllNotesOff(2);energy(voice,9600);check(energy(voice,2400)<second*1e-5,"Channel flush releases held keys");
    voice.midiNoteOn(2,69,1);energy(voice,4800);voice.midiNoteOff(2,69);
    voice.midiAllSoundOff(1);check(energy(voice,960)>second*.01,"CH1 panic preserves OSC2 release tail");
    voice.midiAllSoundOff(2);energy(voice,9600);check(energy(voice,2400)<second*1e-5,"CH2 panic cuts its own tail");

    // Retrigger is shared; legato is intentionally a shared-envelope phrase.
    voice.prepare(48000,512);voice.setMode(1);p.osc1Level=p.osc2Level=.5;p.ampSustain=0;
    p.ampDecay=.02;p.retrigger=true;voice.setParameters(p);voice.midiNoteOn(1,60,1);energy(voice,4800);
    check(energy(voice,2400)<1e-10,"Zero sustain reaches silence while key is held");
    voice.midiNoteOn(2,67,1);check(energy(voice,960)>.0001,"OSC2 note retriggers shared AMP when enabled");
    std::cout<<"PASS: paraphonic AMP ADSR, release tails, channel addressing, same pitches, held-key fallback, flush, retrigger\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
