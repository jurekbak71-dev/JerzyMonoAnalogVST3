#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>

static void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

static void set(JerzyMonoAnalogAudioProcessor& processor, const char* id, float value)
{
    auto* parameter=processor.apvts.getParameter(id);
    check(parameter!=nullptr,"Parameter missing");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

static juce::MidiBuffer render(JerzyMonoAnalogAudioProcessor& processor, int samples)
{
    juce::AudioBuffer<float> audio(2,samples);
    juce::MidiBuffer midi;
    processor.processBlock(audio,midi);
    return midi;
}

int main()
{
    try
    {
        JerzyMonoAnalogAudioProcessor p;
        p.prepareToPlay(44100.0,512);
        set(p,"arpOn",1.0f);
        set(p,"arpHostSync",0.0f);
        set(p,"gridToArp",1.0f);
        set(p,"gridHostSync",0.0f);
        set(p,"gridSeqOn",1.0f);
        set(p,"gridVelocity",0.2f);
        set(p,"arpVelocity",0.8f);
        set(p,"gridDivision",5.0f);
        set(p,"gridGate",0.05f);
        set(p,"gridNoteGate",0.0f); // Explicit legacy percent-gate regression.
        p.setGridStep(0,0,7,true);
        auto events=render(p,512);
        int ons=0,offs=0;
        for(const auto event:events)
        {
            const auto message=event.getMessage();
            if(message.isNoteOn())
            {
                ++ons;
                check(std::abs(message.getFloatVelocity()-0.8f)<0.02f,"GRID note bypassed ARP velocity");
            }
            if(message.isNoteOff()) ++offs;
        }
        check(ons==1 && offs==1,"GRID must produce a single ARP note and release it with the gate");

        p.setGridMode(JerzyMonoAnalogAudioProcessor::GridMode::launch);
        set(p,"arpHostSync",1.0f); // A pad still plays when transport is stopped.
        p.launchPadNoteOn(4);
        events=render(p,256);
        ons=0;
        for(const auto event:events) if(event.getMessage().isNoteOn()) ++ons;
        check(ons==1,"Launch pad must trigger ARP");
        p.launchPadNoteOff(4);
        events=render(p,256);
        offs=0;
        for(const auto event:events) if(event.getMessage().isNoteOff()) ++offs;
        check(offs==1,"Launch pad must release ARP note");

        set(p,"gridToArp",0.0f);
        p.launchPadNoteOn(5);
        events=render(p,256);
        check(events.isEmpty(),"Direct Launch playback must not add outgoing MIDI");
        p.launchPadNoteOff(5);
        render(p,256);

        JerzyMonoAnalogAudioProcessor direct;
        direct.prepareToPlay(44100.0,512);
        set(direct,"gridHostSync",0.0f);
        set(direct,"gridSeqOn",1.0f);
        set(direct,"gridVelocity",0.2f);
        direct.setGridStep(0,0,7,true);
        events=render(direct,256);
        ons=0;
        for(const auto event:events) if(event.getMessage().isNoteOn())
        {
            ++ons;
            check(std::abs(event.getMessage().getFloatVelocity()-0.2f)<0.02f,"Legacy GRID velocity changed");
        }
        check(ons==1,"Direct GRID playback changed");
        std::cout << "GRID and Launch ARP routing OK\n";
        return 0;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

