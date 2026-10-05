#pragma once
#include <JuceHeader.h>
#include "Core.h"

struct ParameterSpec {
    juce::String id,label; int group=0; int kind=0;
    double minimum=0,maximum=1,initial=0,skew=1;
    juce::StringArray choices;
};
std::vector<ParameterSpec> parameterSpecs();

class BassAmbientProcessor final : public juce::AudioProcessor {
public:
    BassAmbientProcessor();
    void prepareToPlay(double,int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 45; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    jerzy::Parameters readParameters() const;
    juce::AudioProcessorValueTreeState state;
    std::atomic<int> rootDisplay{-1};
    std::atomic<float> peak{0};
    void applyPreset(int preset);
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    std::vector<std::atomic<float>*> values;
    jerzy::Instrument instrument;
    jerzy::Parameters current;
    double sr=48000, localPPQ=0;
    std::atomic<bool> resetRequested{false};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassAmbientProcessor)
};
