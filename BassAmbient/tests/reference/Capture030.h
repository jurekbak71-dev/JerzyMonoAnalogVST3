// Frozen 0.3.0: 7ddfe62; symbols renamed and unused editor factory stubbed only.
#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>

// Bounded, single-writer capture. Export uses atomic reads; the audio callback never waits.
class ReferenceCapture030 {
    struct Frame { std::atomic<float> l{0},r{0}; std::atomic<double> beat{0}; };
    struct Event {
        std::atomic<uint64_t> sequence{0};std::atomic<double> beat{0},duration{0};
        std::atomic<int> note{0},channel{1};std::atomic<float> velocity{0};
    };
    std::unique_ptr<Frame[]> frames;
    static constexpr size_t eventCapacity=32768;
    std::unique_ptr<Event[]> events;
    size_t capacity=0;double sampleRate=48000,clock=0;
    uint64_t written=0,eventWritten=0;
    std::atomic<uint64_t> published{0},publishedEvents{0};
    std::atomic<double> publishedBeat{0},tempo{120};
public:
    struct Note {double beat,duration;int note,channel;float velocity;};
    struct Snapshot { juce::AudioBuffer<float> audio;std::vector<Note> notes;double startBeat=0,endBeat=0,bpm=120,sr=48000; };
    void prepare(double sr) {
        sampleRate=sr;capacity=size_t(sr*30);frames=std::make_unique<Frame[]>(capacity);
        written=eventWritten=0;clock=0;published=0;publishedEvents=0;publishedBeat=0;
        events=std::make_unique<Event[]>(eventCapacity);
    }
    void note(int n,int channel,double velocity,double duration) {
        auto& e=events[size_t(eventWritten%eventCapacity)];
        e.sequence.store(0,std::memory_order_release);
        e.beat.store(clock);e.note.store(n);e.channel.store(channel);
        e.velocity.store(float(velocity));e.duration.store(duration);
        e.sequence.store(++eventWritten,std::memory_order_release);
        publishedEvents.store(eventWritten,std::memory_order_release);
    }
    void push(float l,float r,double bpm) {
        if(!capacity)return;
        auto& f=frames[size_t(written%capacity)];f.l.store(l,std::memory_order_relaxed);f.r.store(r,std::memory_order_relaxed);f.beat.store(clock,std::memory_order_relaxed);
        clock+=bpm/(60*sampleRate);++written;
        tempo.store(bpm,std::memory_order_relaxed);publishedBeat.store(clock,std::memory_order_relaxed);
        published.store(written,std::memory_order_release);
    }
    bool snapshot(Snapshot& out,double beats=16) const {
        uint64_t end=published.load(std::memory_order_acquire);
        if(end==0||!capacity)return false;
        const uint64_t usable=uint64_t(capacity)-uint64_t(sampleRate*.5);
        uint64_t first=end>usable?end-usable:0,start=end;
        double endBeat=publishedBeat.load(),begin=std::max(0.0,endBeat-beats);
        while(start>first&&frames[size_t((start-1)%capacity)].beat.load()>=begin)--start;
        if(start==end)return false;
        out.sr=sampleRate;out.bpm=tempo.load();out.startBeat=frames[size_t(start%capacity)].beat.load();out.endBeat=endBeat;
        out.audio.setSize(2,int(end-start));
        for(uint64_t k=start;k<end;++k) {const auto& f=frames[size_t(k%capacity)];out.audio.setSample(0,int(k-start),f.l.load());out.audio.setSample(1,int(k-start),f.r.load());}
        if(published.load(std::memory_order_acquire)-start>capacity)return false;
        auto eEnd=publishedEvents.load(std::memory_order_acquire),eStart=eEnd>eventCapacity?eEnd-eventCapacity:0;
        for(uint64_t k=eStart;k<eEnd;++k) {
            const auto& e=events[size_t(k%eventCapacity)];if(e.sequence.load(std::memory_order_acquire)!=k+1)return false;
            Note n{e.beat.load(),e.duration.load(),e.note.load(),e.channel.load(),e.velocity.load()};
            if(e.sequence.load(std::memory_order_acquire)!=k+1)return false;
            if(n.beat<=endBeat)out.notes.push_back(n);
        }
        return true;
    }
};
