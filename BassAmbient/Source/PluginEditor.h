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
    std::array<juce::TextButton,5> tabs;
    juce::Viewport viewport;
    juce::ComboBox presets;
    juce::TextButton mutate{"MUTATE"},panic{"PANIC"};
    juce::Label status;
    int selected=0;
    void selectPage(int);
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassAmbientEditor)
};
