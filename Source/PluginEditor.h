#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class JerzyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    JerzyLookAndFeel();
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider::SliderStyle,juce::Slider&) override;
    void drawToggleButton(juce::Graphics&,juce::ToggleButton&,bool,bool) override;
    void drawComboBox(juce::Graphics&,int,int,bool,int,int,int,int,juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getLabelFont(juce::Label&) override;
    void positionComboBoxText(juce::ComboBox&,juce::Label&) override;
};

class ResetSlider : public juce::Slider
{
public:
    void setNeutralValue(double v){neutralValue=v;}
    void mouseDown(const juce::MouseEvent& e) override
    {
        if(e.mods.isRightButtonDown())
        {
            setValue(juce::jlimit(getMinimum(),getMaximum(),neutralValue),juce::sendNotificationSync);
            return;
        }
        juce::Slider::mouseDown(e);
    }
private:
    double neutralValue=0.0;
};

class JerzyMonoAnalogAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit JerzyMonoAnalogAudioProcessorEditor(JerzyMonoAnalogAudioProcessor&);
    ~JerzyMonoAnalogAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment=juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment=juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Section{juce::String title;juce::Colour led;juce::Rectangle<float> bounds;};

    class PadGrid: public juce::Component
    {
    public:
        explicit PadGrid(JerzyMonoAnalogAudioProcessor& p):proc(p){}
        void setMode(JerzyMonoAnalogAudioProcessor::GridMode m){mode=m;repaint();}
        void setBank(int b){bank=juce::jlimit(0,7,b);repaint();}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void refresh(){repaint();}
    private:
        int padAt(juce::Point<float>) const;
        JerzyMonoAnalogAudioProcessor& proc;
        JerzyMonoAnalogAudioProcessor::GridMode mode=JerzyMonoAnalogAudioProcessor::GridMode::sequencer;
        int bank=0;
        int heldPad=-1;
    };

    class OutputMeter: public juce::Component
    {
    public:
        void setLevel(float v){level=juce::jlimit(0.0f,1.0f,v);repaint();}
        void paint(juce::Graphics&) override;
    private: float level=0.0f;
    };

    void timerCallback() override;
    void setupKnob(ResetSlider&,const juce::String&,const juce::String& unit={},double neutral=0.0);
    void setupEnvSlider(ResetSlider&,const juce::String&,const juce::String& unit={},double neutral=0.0);
    void setupCombo(juce::ComboBox&,const juce::StringArray&);
    void setupToggle(juce::ToggleButton&,const juce::String&,juce::Colour);
    void place(juce::Component&,float,float,float,float);
    void addSection(const juce::String&,juce::Colour,float,float,float,float);
    void drawSection(juce::Graphics&,const Section&) const;
    void drawLabelBox(juce::Graphics&,const juce::String&,float,float,float) const;
    void drawEnvelope(juce::Graphics&,juce::Rectangle<float>,bool) const;
    void setArpPanelVisible(bool);
    void setMainPage(bool pads);
    void setSynthControlsVisible(bool);
    void updateGridControls();
    float scale() const noexcept {return getWidth()/1440.0f;}
    float topHeight() const noexcept {return 720.0f*scale();}

    JerzyMonoAnalogAudioProcessor& proc;
    JerzyLookAndFeel look;
    std::vector<Section> sections;

    juce::Label title,subtitle,preset;
    juce::TextButton arpPanelButton;
    juce::TextButton pageButton;
    juce::ToggleButton gridSeqOn,gridMidiTrigger;
    juce::TextButton gridModeButton,gridClearButton;
    juce::ComboBox gridBankBox,gridDivision,gridRoot,gridScale,gridBanks,gridDirection;
    ResetSlider gridGate,gridSwing,gridVelocity;
    PadGrid padGrid;

    juce::ComboBox filterMode;
    juce::ComboBox osc1Wave,osc1Oct,osc2Wave,osc2Oct,subWave;
    juce::ComboBox lfoWave,lfoDivision,glideMode,priority;
    juce::ComboBox arpDivision,arpPattern,arpRhythm,arpOctaves;

    ResetSlider osc1Level,pulseWidth,osc2Level,detune,subLevel,noiseLevel,mixDrive,drift;
    ResetSlider cutoff,resonance,filterDrive,filterEnv,keyTrack,modEnvPitch,modEnvPWM;
    ResetSlider aA,aD,aS,aR,fA,fD,fS,fR;
    ResetSlider lfoRate,lfoPitch,lfoFilter,lfoPWM,lfoAmp,lfoFade;
    ResetSlider glide,outDrive,master;
    ResetSlider arpGate,arpSwing,arpVelocity;

    juce::ToggleButton legato,retrigger,lfoSync;
    juce::ToggleButton arpOn,arpLatch,arpRetrigger;
    OutputMeter outputMeter;

    std::unique_ptr<ComboAttachment> osc1WaveA,osc1OctA,osc2WaveA,osc2OctA,subWaveA,lfoWaveA,lfoDivisionA,glideModeA,priorityA;
    std::unique_ptr<ComboAttachment> arpDivisionA,arpPatternA,arpRhythmA,arpOctavesA;
    std::unique_ptr<ComboAttachment> gridDivisionA,gridRootA,gridScaleA,gridBanksA,gridDirectionA;
    std::unique_ptr<SliderAttachment> osc1LevelA,pulseWidthA,osc2LevelA,detuneA,subLevelA,noiseLevelA,mixDriveA,driftA;
    std::unique_ptr<ComboAttachment> filterModeA;
    std::unique_ptr<SliderAttachment> modEnvPitchA,modEnvPWMA;
    std::unique_ptr<SliderAttachment> cutoffA,resonanceA,filterDriveA,filterEnvA,keyTrackA;
    std::unique_ptr<SliderAttachment> aAA,aDA,aSA,aRA,fAA,fDA,fSA,fRA;
    std::unique_ptr<SliderAttachment> lfoRateA,lfoPitchA,lfoFilterA,lfoPWMA,lfoAmpA,lfoFadeA;
    std::unique_ptr<SliderAttachment> glideA,outDriveA,masterA,arpGateA,arpSwingA,arpVelocityA,gridGateA,gridSwingA,gridVelocityA;
    std::unique_ptr<ButtonAttachment> legatoA,retriggerA,lfoSyncA,arpOnA,arpLatchA,arpRetriggerA,gridSeqOnA,gridMidiTriggerA;

    bool arpPanelOpen=false;
    bool padsPage=false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyMonoAnalogAudioProcessorEditor)
};

