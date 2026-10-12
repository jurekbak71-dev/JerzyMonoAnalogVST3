#pragma once
#include <JuceHeader.h>
#include "AnalogDSP.h"
#include "VoiceEngine.h"
#include "ArpLane.h"
#include "GridClock.h"
#include "MonoFxChain.h"

class JerzyMonoAnalogAudioProcessor : public juce::AudioProcessor
{
public:
    JerzyMonoAnalogAudioProcessor();
    ~JerzyMonoAnalogAudioProcessor() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float getOutputMeter() const noexcept { return outputMeter.load(); }
    std::array<int, MonoFxChain::count> getFxOrder() const { return fxChain.getOrder(); }
    void moveFx(int from,int to) { fxChain.move(from,to); }

    enum class GridMode { sequencer = 0, launch = 1 };
    void setGridMode(GridMode m) noexcept { gridMode.store((int)m); }
    GridMode getGridMode() const noexcept { return (GridMode)gridMode.load(); }
    void setGridBank(int b) noexcept { gridBank.store(juce::jlimit(0,7,b)); }
    int getGridBank() const noexcept { return gridBank.load(); }
    void setGridStep(int bank,int column,int row,bool on);
    bool getGridStep(int bank,int column,int row) const;
    void clearGridBank(int bank);
    void launchPadNoteOn(int padIndex);
    void launchPadNoteOff(int padIndex);
    int getGridPlayColumn() const noexcept { return playColumns[(size_t)gridTrack.load()].load(); }
    int getGridPlayBank() const noexcept { return playBanks[(size_t)gridTrack.load()].load(); }
    void setGridTrack(int track) noexcept { gridTrack.store(juce::jlimit(0,1,track)); }
    int getGridTrack() const noexcept { return gridTrack.load(); }
    void selectGridStep(int step) noexcept { selectedGridStep.store(juce::jlimit(0,63,step)); }
    int getSelectedGridStep() const noexcept { return selectedGridStep.load(); }
    int getGridStepGate(int bank,int column) const;
    int getGridRootNote() const noexcept { return gridRootNote.load(); }
    int getGridRowNote(int row) const { return gridNoteForRow(row); }
    void setGridRootNote(int n) noexcept { gridRootNote.store(juce::jlimit(24,84,n)); }
private:
    int getChoiceIndex(const char* id) const;
    std::array<jerzy::ArpLane,2> arps;
    std::array<int,2> arpOutputMask {0,0};
    std::array<std::array<uint64_t,128>,16> midiKeys {};
    uint64_t midiSerial=0;
    bool dualArp=false,previousArpOn=false;
    std::array<std::array<std::atomic<float>*,12>,2> arpParams {};
    std::atomic<int> launchPressedPad {-1};
    std::atomic<int> followRoot {-1};
    void updateHeldKeys(bool noteOn,double ppq,int channel=-1);
    void arpOff(int lane,int note,int offset,juce::MidiBuffer& midi);
    void arpOnNote(int lane,int note,float velocity,int offset,juce::MidiBuffer& midi);
    void restartGridArp(int track,double ppq);
    void resetArpState();
    void processGridSequencerSample(double bpm,int sampleOffset,juce::MidiBuffer& generatedMidi,double hostPpq,bool hostHasPpq,bool hostPlaying);
    void processLaunchPadSample(int sampleOffset,juce::MidiBuffer& generatedMidi,double hostPpq,bool routeToArp);
    void stopLaunchNote(int sampleOffset,juce::MidiBuffer& generatedMidi);
    int gridNoteForRow(int row) const;
    int gridRootMidiFromChoice() const;
    bool isGridMidiRunning() const noexcept { return gridMidiRunning.load(); }
    jerzy::VoiceEngine engine;
    MonoFxChain fxChain;
    juce::MidiBuffer inputMidiScratch, generatedMidiScratch;
    std::atomic<float> outputMeter { 0.0f };
    double currentSampleRate = 44100.0;
    juce::Array<int> physicalHeldNotes;

    std::array<std::atomic<uint8_t>, 8*8*8> gridPattern {};
    std::atomic<int> gridMode { 0 };
    std::atomic<int> gridBank { 0 };
    std::atomic<int> gridRootNote { 48 };
    std::atomic<int> gridTriggerNote { 48 };
    double gridHostPpqOrigin = 0.0;
    std::atomic<int> gridActiveBanks { 8 };
    std::atomic<bool> gridMidiRunning { false };
    std::atomic<int> gridMidiHeldCount { 0 };
    std::atomic<int> gridPlayColumn { -1 };
    std::atomic<int> launchPressedNote { -1 };
    double gridSamplesToNext = 0.0;
    double gridCurrentStepSamples = 0.0;
    int gridGlobalStep = 0;
    int lastGridHostStep = -1;
    int gridArpNote = -1;
    int gridArpNote2 = -1;
    int launchCurrentNote = -1;
    bool launchCurrentNoteRouted = false;
    int launchArpNote = -1;
    bool gridArpNeedsRestart = false;
    struct GridSettings
    {
        bool armed = false, midiTrigger = false, hostSync = false, routeToArp = false, noteGate = true;
        int totalSteps = 8, division = 0, ratchets = 1, direction = 0;
        double swing = 0.0, gate = 0.75;
        float velocity = 0.95f, probability = 1.0f;
    } gridSettings;
    int gridRatchetCount = 1;
    int gridRatchetIndex = 0;
    int gridStepNote = -1;
    bool gridStepActive = false;
    std::atomic<int> gridTrack {0}, selectedGridStep {0};
    std::array<std::atomic<int>,2> playColumns {{{-1},{-1}}}, playBanks {{{-1},{-1}}};
    std::array<std::atomic<uint8_t>,512> gridPattern2 {};
    std::array<std::array<std::atomic<float>*,64>,2> stepGateParameters {};
    struct TrackState {
        int lastStep=-1,note=-1,stepNote=-1,ratchet=-1,division=4,length=64,direction=0;
        double beat=0,endBeat=0,swing=0;
        bool routed=false,enabled=true;
    };
    std::array<TrackState,2> tracks;
    jerzy::AnalogLFO gateLfo;
    double gateModulation=0, gateModDepth=0, previousPpq=0;
    bool previousHostPlaying=false;
    int voiceMode=0;
    void stopTrack(int track,int offset,juce::MidiBuffer& midi);
    std::atomic<bool> restoredState {false};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyMonoAnalogAudioProcessor)
};

