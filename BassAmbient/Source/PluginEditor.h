#pragma once
#include "PluginProcessor.h"

class BassAmbientEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit BassAmbientEditor(BassAmbientProcessor&);
    ~BassAmbientEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    class Theme;
    class Control;
    class Page;
    BassAmbientProcessor& processor;
    std::unique_ptr<Theme> theme;
    std::array<std::unique_ptr<Page>,5> pages;
    std::array<juce::TextButton,3> tabs;
    juce::ComboBox rackSelector;
    juce::Viewport viewport;
    juce::ComboBox presets;
    juce::TextButton generate{"GENERATE"},mutate{"MUTATE"},undo{"UNDO"},panic{"PANIC"};
    juce::TextButton bassPlay{"BASS START"},padPlay{"AMBIENT START"},midiExport{"SAVE MIDI"},wavExport{"SAVE WAV"},storeScene{"STORE"};
    std::array<juce::TextButton,4> scenes;
    bool storing=false;
    std::unique_ptr<juce::FileChooser> chooser;
    void exportPerformance(bool midi);
    juce::Label status;
    int selected=0, selectedRack=0;
    Page& currentPage();
    void selectPage(int);
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassAmbientEditor)
};
