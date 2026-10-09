#pragma once
#include "AnalogDSP.h"

namespace jerzy {
// All voices are preallocated. Idle poly voices do no oversampled processing.
class VoiceEngine {
public:
    void prepare(double sr,int block) {
        for(auto& voice:voices) voice.prepare(sr,block);
        for(auto& voice:external) voice.prepare(sr,block);
        notes.fill(-1);ages.fill(0);serial=0;mode=0;paraHeld.fill(-1);
    }
    void setMode(int next) {
        next=juce::jlimit(0,2,next);
        if(next==mode)return;
        for(auto& voice:voices)voice.reset();
        notes.fill(-1);paraHeld.fill(-1);mode=next;
    }
    void setParameters(const MonoParameters& p) {
        params=p;
        for(auto& v:voices)v.setParameters(p);
        auto audio=p;audio.osc1Level=audio.osc2Level=audio.subLevel=audio.noiseLevel=0;
        audio.externalOpen=audioOpen;
        audio.externalProcessing=true;
        for(auto& v:external)v.setParameters(audio);
    }
    void setAudioInput(bool enabled,bool open,float gain) {audioEnabled=enabled;audioOpen=open;audioGain=gain;}
    void noteOn(int note,float velocity,int source=0) {
        if(mode==0) voices[0].noteOn(note,velocity);
        else if(mode==1) {
            int track=paraHeld[0]<0?0:(paraHeld[1]<0?1:(ages[0]<=ages[1]?0:1));
            for(int i=0;i<2;++i)if(paraHeld[(size_t)i]==note)track=i;
            oscillatorNoteOn(track,note,velocity);
        } else {
            int index=-1;
            for(int i=0;i<6;++i)if(notes[(size_t)i]==note && sources[(size_t)i]==source){index=i;break;}
            if(index<0)for(int i=0;i<6;++i)if(notes[(size_t)i]<0 && !voices[(size_t)i].isActive()){index=i;break;}
            if(index<0)for(int i=0;i<6;++i)if(notes[(size_t)i]<0 && (index<0 || ages[(size_t)i]<ages[(size_t)index]))index=i;
            if(index<0)index=(int)std::distance(ages.begin(),std::min_element(ages.begin(),ages.end()));
            auto& voice=voices[(size_t)index];
            if(notes[(size_t)index]>=0)voice.noteOff(notes[(size_t)index]);
            // Polyphonic notes always have their own attack, not mono legato priority.
            auto patch=params;patch.legato=false;patch.retrigger=true;voice.setParameters(patch);
            notes[(size_t)index]=note;sources[(size_t)index]=source;ages[(size_t)index]=++serial;voice.noteOn(note,velocity);
        }
        if(audioEnabled)for(auto& v:external)v.noteOn(note,velocity);
    }
    void noteOff(int note,int source=0) {
        if(mode==0)voices[0].noteOff(note);
        else if(mode==1) {for(int i=0;i<2;++i)if(paraHeld[(size_t)i]==note)oscillatorNoteOff(i);}
        else for(int i=0;i<6;++i)if(notes[(size_t)i]==note && sources[(size_t)i]==source){voices[(size_t)i].noteOff(note);notes[(size_t)i]=-1;}
        if(audioEnabled)for(auto& v:external)v.noteOff(note);
    }
    void oscillatorNoteOn(int track,int note,float velocity) {
        if(mode!=1){noteOn(note,velocity);return;}
        track=juce::jlimit(0,1,track);paraHeld[(size_t)track]=note;ages[(size_t)track]=++serial;
        voices[0].oscillatorNoteOn(track,note,velocity);
        if(audioEnabled)for(auto& v:external)v.noteOn(note,velocity);
    }
    void oscillatorNoteOff(int track) {
        track=juce::jlimit(0,1,track);const int note=paraHeld[(size_t)track];paraHeld[(size_t)track]=-1;
        voices[0].oscillatorNoteOff(track);
        if(audioEnabled && note>=0)for(auto& v:external)v.noteOff(note);
    }
    float processSample() {
        if(mode!=2)return voices[0].processSample();
        float result=0;
        for(auto& voice:voices)if(voice.isActive())result+=voice.processSample();
        return result*.40824829f; // Fixed 1/sqrt(6) headroom: no pumping as voices release.
    }
    float processInput(int channel,float input) {return audioEnabled?external[(size_t)juce::jlimit(0,1,channel)].processSample(input*audioGain):0.0f;}
    int activeNotes() const {if(mode!=2)return 0;return (int)std::count_if(notes.begin(),notes.end(),[](int n){return n>=0;});}
private:
    std::array<MonoAnalogEngine,6> voices;
    std::array<MonoAnalogEngine,2> external;
    std::array<int,6> notes {-1,-1,-1,-1,-1,-1};
    std::array<int,6> sources {};
    std::array<uint64_t,6> ages {};
    std::array<int,2> paraHeld {-1,-1};
    uint64_t serial=0;
    int mode=0;
    MonoParameters params;
    bool audioEnabled=false,audioOpen=true;
    float audioGain=1;
};
}
