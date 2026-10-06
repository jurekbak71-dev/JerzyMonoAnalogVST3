#pragma once
#include <JuceHeader.h>
#include "Core.h"
#include "Capture.h"

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
    std::atomic<bool> auditionBass{false},auditionPad{false};
    std::atomic<bool> hostRunning{false};
    std::atomic<int> requestedScene{-1},activeScene{-1};
    void panicNow() {auditionBass=false;auditionPad=false;resetRequested=true;}
    void generate(bool mutation);
    void undoGeneration();
    void saveScene(int index);
    void requestScene(int index) {requestedScene.store(index);}
    bool hasScene(int index) const;
    bool exportCapture(const juce::File&,bool midi,juce::String& error);
    void synchroniseLocks();
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    std::vector<std::atomic<float>*> values;
    std::vector<juce::RangedAudioParameter*> hostParameters;
    std::array<std::vector<float>,4> sceneData;
    std::array<std::atomic<bool>,4> sceneReady{};
    juce::MemoryBlock undoState;
    std::array<bool,3> lastLocks{};
    PerformanceCapture capture;
    double captureTempo=120,previousScenePPQ=0;
    bool haveScenePPQ=false;
    int hostNum=4,hostDen=4;
    void setParameter(const juce::String&,float);
    void applyQueuedScene();
    jerzy::Instrument instrument;
    jerzy::Parameters current;
    double sr=48000, localPPQ=0;
    std::atomic<bool> resetRequested{false};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassAmbientProcessor)
};
