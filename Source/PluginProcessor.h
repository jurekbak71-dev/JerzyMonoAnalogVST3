#pragma once
#include <JuceHeader.h>
#include "AnalogDSP.h"
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
    const juce::String getName() const override { return "Jerzy Mono Analog Grid"; }
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
    int getGridPlayColumn() const noexcept { return gridPlayColumn.load(); }
    int getGridRootNote() const noexcept { return gridRootNote.load(); }
    int getGridRowNote(int row) const { return gridNoteForRow(row); }
    void setGridRootNote(int n) noexcept { gridRootNote.store(juce::jlimit(24,84,n)); }
private:
    int getChoiceIndex(const char* id) const;
    int chooseArpNote(int pattern, int step);
    bool arpRhythmGate(int rhythm, int step) const;
    void resetArpState();
    void processGridSequencerSample(double bpm,int sampleOffset,juce::MidiBuffer& generatedMidi,double hostPpq,bool hostHasPpq,bool hostPlaying);
    int gridNoteForRow(int row) const;
    int gridRootMidiFromChoice() const;
    bool isGridMidiRunning() const noexcept { return gridMidiRunning.load(); }
    jerzy::MonoAnalogEngine engine;
    MonoFxChain fxChain;
    std::atomic<float> outputMeter { 0.0f };
    double currentSampleRate = 44100.0;
    double arpSamplesToNext = 0.0;
    int arpStep = 0;
    int arpCurrentNote = -1;
    int arpUpDownPos = 0;
    int arpLastRandomIndex = -1;
    juce::Array<int> arpHeldNotes;
    juce::Array<int> arpLatchedNotes;
    juce::Array<int> physicalHeldNotes;
    std::mt19937 arpRng { 0x51a7u };

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
    double arpCurrentStepSamples = 0.0;
    int gridGlobalStep = 0;
    int lastArpHostStep = -1;
    int lastGridHostStep = -1;
    int gridCurrentNote = -1;
    int gridRatchetCount = 1;
    int gridRatchetIndex = 0;
    int gridStepNote = -1;
    bool gridStepActive = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyMonoAnalogAudioProcessor)
};
