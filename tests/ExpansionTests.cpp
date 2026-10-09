#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static void set(JerzyMonoAnalogAudioProcessor& p,const juce::String& id,float n){auto* v=p.apvts.getParameter(id);check(v!=nullptr,"Missing parameter");v->setValueNotifyingHost(v->convertTo0to1(n));}
struct Host:juce::AudioPlayHead {
    double beat=0;bool playing=true;
    juce::Optional<PositionInfo> getPosition() const override {PositionInfo p;p.setBpm(120);p.setPpqPosition(beat);p.setIsPlaying(playing);return p;}
};
int main(){try{
    // Six independent voices and bounded stealing; release of a stolen key is harmless.
    jerzy::VoiceEngine poly;poly.prepare(48000,512);poly.setMode(2);jerzy::MonoParameters patch;patch.analogDriftCents=0;poly.setParameters(patch);
    for(int n=48;n<54;++n)poly.noteOn(n,1);check(poly.activeNotes()==6,"Six voices available");
    poly.noteOn(72,1);check(poly.activeNotes()==6,"Seventh note steals one voice");poly.noteOff(48);check(poly.activeNotes()==6,"Stolen key release does not silence replacement");
    for(int n=49;n<54;++n)poly.noteOff(n);poly.noteOff(72);check(poly.activeNotes()==0,"All six notes release");
    poly.setMode(1);poly.oscillatorNoteOn(0,36,1);poly.oscillatorNoteOn(1,67,1);double energy=0;
    for(int i=0;i<4000;++i){const auto y=poly.processSample();check(std::isfinite(y),"Paraphonic finite");energy+=y*y;}check(energy>.01,"Paraphonic audible");
    poly.oscillatorNoteOff(0);energy=0;for(int i=0;i<3000;++i){const auto y=poly.processSample();energy+=y*y;}check(energy>.01,"OSC2 continues when OSC1 releases");
    poly.setMode(0);poly.setMode(1);patch.osc1Level=0;patch.subLevel=0;patch.osc2Level=1;patch.osc2Wave=jerzy::BandLimitedOscillator::Wave::sine;patch.osc2DetuneCents=0;patch.cutoffHz=18000;patch.filterEnvOct=0;patch.keyTrack=0;poly.setParameters(patch);
    poly.oscillatorNoteOn(0,36,1);poly.oscillatorNoteOn(1,67,1);int crossings=0;float previous=0;
    for(int i=0;i<8000;++i){const float y=poly.processSample();if(i>1000 && previous<0 && y>=0)++crossings;previous=y;}check(crossings>50,"OSC2 has independent MIDI pitch, not OSC1 pitch");
    poly.setMode(2);poly.noteOn(60,1,1);poly.noteOn(60,1,2);check(poly.activeNotes()==2,"Identical pitches on separate tracks retain separate voices");poly.noteOff(60,1);check(poly.activeNotes()==1,"Track release does not silence another track's same pitch");
    // Musical gate of four steps survives three rests and releases at the exact boundary.
    JerzyMonoAnalogAudioProcessor p;Host host;p.setPlayHead(&host);p.prepareToPlay(48000,512);
    set(p,"gridSeqOn",1);set(p,"gridLength",8);set(p,"gateT1S0",4);p.setGridStep(0,0,7,true);
    juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;int ons=0,offs=0;long long offSample=-1;
    for(int block=0;block<100;++block){midi.clear();p.processBlock(audio,midi);for(auto e:midi){if(e.getMessage().isNoteOn())++ons;if(e.getMessage().isNoteOff()){++offs;if(offSample<0)offSample=block*512+e.samplePosition;}}host.beat+=512*120.0/(60*48000);}
    check(ons==2,"Eight-step sequence loops");check(offs==1 && offSample==24000,"Four-step gate releases at one quarter-note");
    // One-step loop must never divide by zero; independent OSC2 triplets.
    set(p,"voiceMode",1);set(p,"gridLength",1);set(p,"grid2Length",3);set(p,"grid2Division",8);set(p,"gateT1S0",1);
    p.setGridTrack(1);p.setGridStep(0,0,7,true);p.setGridStep(0,1,6,true);p.setGridStep(0,2,5,true);
    host.beat=0;int first=0,second=0;
    for(int block=0;block<100;++block){midi.clear();p.processBlock(audio,midi);for(auto e:midi)if(e.getMessage().isNoteOn()){if(e.getMessage().getChannel()==1)++first;if(e.getMessage().getChannel()==2)++second;}host.beat+=512*120.0/(60*48000);}
    check(first==9 && second==13,"Independent straight and triplet tracks");
    host.playing=false;midi.clear();p.processBlock(audio,midi);for(auto e:midi)check(!e.getMessage().isNoteOn(),"Stopped host produces no sequence note");
    check(p.getGridPlayColumn()==-1,"Stopped OSC2 cursor");
    juce::MemoryBlock state;p.getStateInformation(state);p.clearGridBank(0);p.setStateInformation(state.getData(),(int)state.getSize());check(p.getGridStep(0,1,6),"OSC2 preset restore");check(p.getGridStepGate(0,0)==1,"Gate preset restore");
    // Stereo audio input reaches nonlinear filter and output, without MIDI in OPEN mode.
    JerzyMonoAnalogAudioProcessor input;input.enableAllBuses();input.prepareToPlay(48000,512);set(input,"audioInOn",1);set(input,"cutoff",6000);
    double left=0,right=0;
    for(int block=0;block<8;++block){for(int i=0;i<512;++i){audio.setSample(0,i,(float)(.3*std::sin((block*512+i)*.02)));audio.setSample(1,i,0);}midi.clear();input.processBlock(audio,midi);for(int i=0;i<512;++i){left+=std::abs(audio.getSample(0,i));right+=std::abs(audio.getSample(1,i));}}
    check(left>1 && right<1e-5,"Stereo AUDIO IN is processed without collapsing channels");
    jerzy::AnalogLFO lfo;lfo.prepare(1000,17);lfo.set(5,jerzy::AnalogLFO::Wave::randomSquare);int transitions=0,lastLength=0;bool varied=false;double prev=lfo.process();
    for(int i=1,last=0;i<3000;++i){const auto v=lfo.process();check(v==1 || v==-1,"Random square remains bipolar");if(v!=prev){const int len=i-last;if(lastLength && len!=lastLength)varied=true;lastLength=len;last=i;++transitions;}prev=v;}
    check(transitions>5 && varied,"Random square has variable pulse lengths");
    std::cout<<"PASS: six voices, stealing, paraphony, long gate, one step, triplet polyrhythm, state, stereo input, random LFO\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
