#include "PluginProcessor.h"
#include <iostream>
#include <memory>
#include <stdexcept>
static void check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
static void set(JerzyMonoAnalogAudioProcessor& p,const char* id,float v){auto* q=p.apvts.getParameter(id);check(q,"Missing parameter");q->setValueNotifyingHost(q->convertTo0to1(v));}
struct Host:juce::AudioPlayHead {double beat=0;bool playing=true;juce::Optional<PositionInfo>getPosition()const override{PositionInfo p;p.setBpm(120);p.setPpqPosition(beat);p.setIsPlaying(playing);return p;}};
static double render(JerzyMonoAnalogAudioProcessor& p,juce::MidiBuffer& midi,int n,double* right=nullptr){juce::AudioBuffer<float>b(2,n);b.clear();p.processBlock(b,midi);double e=0,r=0;for(int i=0;i<n;++i){check(std::isfinite(b.getSample(0,i))&&std::isfinite(b.getSample(1,i)),"Finite output");e+=b.getSample(0,i)*b.getSample(0,i);r+=b.getSample(1,i)*b.getSample(1,i);}if(right)*right=r;return e;}
int main(){try{
 auto storage=std::make_unique<JerzyMonoAnalogAudioProcessor>();auto& p=*storage;
 check(p.getBusCount(true)==1 && p.getBus(true,0)->isEnabledByDefault() && p.getTotalNumInputChannels()==2,"Active stereo AUDIO IN bus");
 check(p.apvts.getParameter("arpPattern")->getNumSteps()==9 && p.apvts.getParameter("arpRhythm")->getNumSteps()==7,"Legacy normalized automation preserves choice counts");
 p.prepareToPlay(48000,512);set(p,"voiceMode",1);set(p,"arpOn",1);set(p,"arpHostSync",0);set(p,"osc1Pan",-1);set(p,"osc2Pan",1);set(p,"subLevel",0);set(p,"noiseLevel",0);
 juce::MidiBuffer m;m.addEvent(juce::MidiMessage::noteOn(1,60,1.0f),0);double right=0,left=render(p,m,2048,&right);
 check(left>.001 && right>.001,"Shared ARP must sound on BOTH oscillators in para mode");
 bool ch1=false,ch2=false;for(auto e:m)if(e.getMessage().isNoteOn()){ch1|=e.getMessage().getChannel()==1;ch2|=e.getMessage().getChannel()==2;}check(ch1&&ch2,"Shared para ARP records both MIDI channels");
 set(p,"arpRouting",1);set(p,"arp2HostSync",0);set(p,"arpDivision",4);set(p,"arp2Division",6);set(p,"arpRetrigger",0);set(p,"arp2Retrigger",0);
 m.clear();m.addEvent(juce::MidiMessage::noteOn(2,72,1.0f),0);
 int n1=0,n2=0;for(int b=0;b<100;++b){if(b)m.clear();render(p,m,512);for(auto e:m)if(e.getMessage().isNoteOn()){auto note=e.getMessage();if(note.getChannel()==1){++n1;check(note.getNoteNumber()==60,"ARP1 pool isolation");}if(note.getChannel()==2){++n2;check(note.getNoteNumber()==72,"ARP2 pool isolation");}}}
 check(n1>=8&&n2>=3&&n1>n2,"Independent straight/triplet clocks");
 m.clear();m.addEvent(juce::MidiMessage::noteOff(1,60),0);render(p,m,512);
 n2=0;for(int b=0;b<70;++b){m.clear();render(p,m,512);for(auto e:m)if(e.getMessage().isNoteOn()){check(e.getMessage().getChannel()!=1,"Released ARP1 must remain silent");if(e.getMessage().getChannel()==2)++n2;}}check(n2>0,"OSC2 survives channel1 release");
 m.clear();m.addEvent(juce::MidiMessage::allNotesOff(2),0);render(p,m,512);for(int b=0;b<70;++b){m.clear();render(p,m,512);for(auto e:m)check(!e.getMessage().isNoteOn(),"Panic clears ARP latch and held keys");}
 auto gridStorage=std::make_unique<JerzyMonoAnalogAudioProcessor>();auto& g=*gridStorage;g.prepareToPlay(48000,512);set(g,"gridHostSync",0);set(g,"gridSeqOn",1);set(g,"gridMidiTrigger",0);g.setGridStep(0,0,7,true);
 m.clear();m.addEvent(juce::MidiMessage::noteOn(1,65,1.0f),0);render(g,m,512);check(g.getGridRootNote()==65,"ROOT follows MIDI with trigger OFF");bool found=false;for(auto e:m)if(e.getMessage().isNoteOn())found|=e.getMessage().getNoteNumber()==65;check(found,"GRID transposes from Piano Roll note");
 m.clear();m.addEvent(juce::MidiMessage::noteOn(2,69,1.0f),0);render(g,m,512);check(g.getGridRootNote()==69,"Latest MIDI root");m.clear();m.addEvent(juce::MidiMessage::noteOff(2,69),0);render(g,m,512);check(g.getGridRootNote()==65,"Root returns to earlier held note");set(g,"gridRootFollow",0);m.clear();render(g,m,512);check(g.getGridRootNote()==48,"Manual root still available");
 auto audioStorage=std::make_unique<JerzyMonoAnalogAudioProcessor>();auto& a=*audioStorage;a.prepareToPlay(48000,512);set(a,"audioInOn",1);set(a,"osc1Level",0);set(a,"osc2Level",0);set(a,"subLevel",0);set(a,"noiseLevel",0);
 juce::AudioBuffer<float> input(2,512);input.clear();input.setSample(0,0,.5f);m.clear();a.processBlock(input,m);double l=0,r=0;for(int i=0;i<512;++i){l+=input.getSample(0,i)*input.getSample(0,i);r+=input.getSample(1,i)*input.getSample(1,i);}check(l>1e-7&&r<1e-12,"AUDIO IN left-only impulse preserves stereo isolation");
 auto syncStorage=std::make_unique<JerzyMonoAnalogAudioProcessor>();auto& s=*syncStorage;s.prepareToPlay(48000,512);Host host;s.setPlayHead(&host);set(s,"arpOn",1);set(s,"arpRouting",1);m.clear();m.addEvent(juce::MidiMessage::noteOn(1,60,1.0f),0);m.addEvent(juce::MidiMessage::noteOn(2,72,1.0f),0);render(s,m,512);host.beat+=512.0/24000;host.playing=false;m.clear();render(s,m,512);for(auto e:m)check(!e.getMessage().isNoteOn(),"Host STOP silences both synced arps");host.beat=8;host.playing=true;m.clear();render(s,m,512);int ons=0;for(auto e:m)if(e.getMessage().isNoteOn())++ons;check(ons==2,"Seek/restart resets both arps without dropping held notes");
 auto routedStorage=std::make_unique<JerzyMonoAnalogAudioProcessor>();auto& d=*routedStorage;d.prepareToPlay(48000,512);
 set(d,"arpOn",1);set(d,"arpRouting",1);set(d,"arpHostSync",0);set(d,"arp2HostSync",0);set(d,"gridSeqOn",1);set(d,"gridHostSync",0);set(d,"gridToArp",1);set(d,"grid2On",1);set(d,"gridRootFollow",0);set(d,"gridLength",1);set(d,"grid2Length",1);set(d,"gridVelocity",.2f);set(d,"arpVelocity",.8f);set(d,"arp2Velocity",.6f);
 d.setGridTrack(0);d.setGridStep(0,0,7,true);d.setGridTrack(1);d.setGridStep(0,0,5,true);
 m.clear();render(d,m,512);ch1=ch2=false;for(auto event:m)if(event.getMessage().isNoteOn()){auto note=event.getMessage();if(note.getChannel()==1){ch1=true;check(note.getNoteNumber()==48 && std::abs(note.getFloatVelocity()-.8f)<.02f,"GRID1 -> ARP1");}else if(note.getChannel()==2){ch2=true;check(note.getNoteNumber()==52 && std::abs(note.getFloatVelocity()-.6f)<.02f,"GRID2 -> ARP2");}}
 check(ch1&&ch2,"Both GRID lanes trigger their own ARP");
 juce::MemoryBlock state;d.getStateInformation(state);set(d,"arpRouting",0);d.setStateInformation(state.getData(),(int)state.getSize());check(d.apvts.getRawParameterValue("arpRouting")->load()==1,"Dual routing preset roundtrip");
 jerzy::ArpLane lane;lane.prepare(19);lane.settings.enabled=true;lane.settings.sync=false;lane.settings.retrigger=false;
 int keys[]={48,60,72};lane.keys(keys,3,true,0);lane.settings.pattern=10;for(int i=0;i<100;++i){auto event=lane.process(100,120,i*.02,false,true,-1,-1,-1);if(event.on>=0)check(event.on==48 || event.on==60 || event.on==72,"Random walk remains inside held note pool");}
 lane.settings.probability=0;lane.restart(0);for(int i=0;i<200;++i)check(lane.process(100,120,i*.02,false,true,-1,-1,-1).on<0,"Probability zero produces no notes");
 for(int rhythm:{8,9}){int hits=0;for(int step=0;step<8;++step)hits+=jerzy::ArpLane::rhythmGate(rhythm,step);check(hits==(rhythm==8?3:5),"Euclidean hit count");}
 // Regression: all chorus modes must produce bounded, finite delayed audio.
 for(int mode=0;mode<3;++mode) {
     MonoFxChain fx;fx.prepare(48000,512);
     set(a,"fxChorusOn",1);set(a,"fxChorusMode",(float)mode);
     set(a,"fxChorusMix",1);set(a,"fxChorusDepth",1);
     set(a,"fxChorusFeedback",.7f);
     double wetEnergy=0;float peak=0;
     for(int block=0;block<24;++block) {
         juce::AudioBuffer<float> wave(2,512);wave.clear();
         if(block==0){wave.setSample(0,0,.7f);wave.setSample(1,0,-.7f);}
         fx.process(wave,a.apvts,120);
         for(int i=0;i<512;++i)for(int ch=0;ch<2;++ch){
             const float v=wave.getSample(ch,i);
             check(std::isfinite(v),"Chorus sample finite");
             peak=juce::jmax(peak,std::abs(v));wetEnergy+=(double)v*v;
         }
     }
     check(wetEnergy>1e-5 && peak<2.0f,"Chorus mode emits stable wet signal");
 }
 // Regression: fast chorus automation and bypass toggling must stay finite.
 for(int mode=0;mode<3;++mode) {
     MonoFxChain fx;fx.prepare(48000,512);
     set(a,"fxChorusOn",1);set(a,"fxChorusMode",(float)mode);
     set(a,"fxChorusFeedback",.80f);
     double e=0;float maxDelta=0,previous=0;
     for(int block=0;block<96;++block) {
         set(a,"fxChorusOn",block%17==0?0.0f:1.0f);
         set(a,"fxChorusMix",block%3==0?1.0f:0.35f);
         set(a,"fxChorusDepth",(block%10)/9.0f);
         juce::AudioBuffer<float> wave(2,256);
         for(int i=0;i<256;++i){
             const float x=.17f*(float)std::sin(juce::MathConstants<double>::twoPi*(220.0*(block*256+i)/48000.0));
             wave.setSample(0,i,x);wave.setSample(1,i,x);
         }
         fx.process(wave,a.apvts,120);
         for(int i=0;i<256;++i) {
             const float v=wave.getSample(0,i);
             check(std::isfinite(v),"Automated Chorus stays finite");
             maxDelta=juce::jmax(maxDelta,std::abs(v-previous));previous=v;e+=(double)v*v;
         }
     }
     check(e>1 && maxDelta<1.5f,"Chorus automation does not produce runaway samples");
 }
 std::cout<<"PASS: shared/dual ARP stereo, independent clocks and MIDI pools, note-off, panic, root following, manual root, AUDIO IN bus/stereo, host stop/seek\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
