#include "PluginEditor.h"

namespace
{
constexpr auto RED=0xffff3030, GREEN=0xff48ef62, YELLOW=0xffffc928;
constexpr auto PANEL=0xff0c0e10, EDGE=0xff34383c;
static juce::Colour C(juce::uint32 x){return juce::Colour(x);}
static const juce::Colour lcdBg=C(0xffc9d0c6), lcdText=C(0xff111613);

static void drawLed(juce::Graphics& g, juce::Point<float> c, float r, juce::Colour col)
{
    g.setColour(col.withAlpha(0.16f)); g.fillEllipse(c.x-r*2.2f,c.y-r*2.2f,r*4.4f,r*4.4f);
    g.setColour(col); g.fillEllipse(c.x-r,c.y-r,r*2.0f,r*2.0f);
}
}

JerzyLookAndFeel::JerzyLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId,lcdText);
    setColour(juce::Slider::textBoxBackgroundColourId,lcdBg);
    setColour(juce::Slider::textBoxOutlineColourId,C(0xff596159));
    setColour(juce::ComboBox::textColourId,lcdText);
    setColour(juce::ComboBox::backgroundColourId,lcdBg);
    setColour(juce::ComboBox::outlineColourId,C(0xff596159));
    setColour(juce::PopupMenu::backgroundColourId,C(0xffd7ddd4));
    setColour(juce::PopupMenu::textColourId,lcdText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,C(0xffadb6ad));
    setColour(juce::Label::textColourId,lcdText);
}

void JerzyLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float p,float a0,float a1,juce::Slider&)
{
    auto b=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(5.0f);
    const float d=juce::jmin(b.getWidth(),b.getHeight());
    auto r=juce::Rectangle<float>(d,d).withCentre(b.getCentre());
    auto cc=r.getCentre();
    juce::ColourGradient grad(C(0xff30353a),r.getX(),r.getY(),C(0xff030405),r.getRight(),r.getBottom(),false);
    g.setGradientFill(grad);g.fillEllipse(r);
    g.setColour(C(0xff555b60));g.drawEllipse(r,juce::jmax(1.0f,d*0.018f));
    const float a=a0+p*(a1-a0);
    juce::Path q;q.startNewSubPath(cc.x,cc.y-d*.10f);q.lineTo(cc.x,cc.y-d*.38f);
    q.applyTransform(juce::AffineTransform::rotation(a,cc.x,cc.y));
    g.setColour(C(RED));g.strokePath(q,juce::PathStrokeType(juce::jmax(2.0f,d*.035f)));
}

void JerzyLookAndFeel::drawLinearSlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float minPos,float maxPos,juce::Slider::SliderStyle st,juce::Slider& s)
{
    if(st!=juce::Slider::LinearVertical){juce::LookAndFeel_V4::drawLinearSlider(g,x,y,w,h,pos,minPos,maxPos,st,s);return;}
    auto r=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h);
    auto track=juce::Rectangle<float>(r.getCentreX()-2.5f,r.getY()+5.0f,5.0f,r.getHeight()-10.0f);
    g.setColour(C(0xff050607));g.fillRoundedRectangle(track,2.5f);
    g.setColour(C(0xff4a5055));g.drawRoundedRectangle(track,2.5f,1.0f);
    auto k=juce::Rectangle<float>(r.getX()+3.0f,pos-5.0f,r.getWidth()-6.0f,10.0f);
    g.setColour(C(0xff171a1d));g.fillRoundedRectangle(k,2.0f);
    g.setColour(C(RED));g.drawRoundedRectangle(k,2.0f,1.4f);
}

void JerzyLookAndFeel::drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool,bool)
{
    auto r=b.getLocalBounds().toFloat().reduced(1.0f);
    const auto col=b.findColour(juce::ToggleButton::tickColourId);
    g.setColour(b.getToggleState()?col.withAlpha(.28f):C(0xff101214));g.fillRoundedRectangle(r,4.0f);
    g.setColour(b.getToggleState()?col:C(0xff454a4f));g.drawRoundedRectangle(r,4.0f,1.0f);
    g.setColour(lcdBg);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),juce::jmax(9.0f,r.getHeight()*.32f),juce::Font::bold)));
    g.drawFittedText(b.getButtonText(),b.getLocalBounds().reduced(3),juce::Justification::centred,1,0.75f);
}

void JerzyLookAndFeel::drawComboBox(juce::Graphics& g,int w,int h,bool,int,int,int,int,juce::ComboBox&)
{
    auto r=juce::Rectangle<float>(0,0,(float)w,(float)h).reduced(1.0f);
    g.setColour(lcdBg);g.fillRoundedRectangle(r,3.0f);
    g.setColour(C(0xff596159));g.drawRoundedRectangle(r,3.0f,1.0f);
    juce::Path p;float cx=w-12.0f,cy=h*.5f;p.startNewSubPath(cx-3,cy-2);p.lineTo(cx,cy+2);p.lineTo(cx+3,cy-2);
    g.setColour(lcdText);g.strokePath(p,juce::PathStrokeType(1.2f));
}
juce::Font JerzyLookAndFeel::getComboBoxFont(juce::ComboBox& b)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),juce::jmax(9.0f,b.getHeight()*.34f),juce::Font::bold));
}
juce::Font JerzyLookAndFeel::getLabelFont(juce::Label& l)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),juce::jmax(8.0f,l.getHeight()*.40f),juce::Font::bold));
}
void JerzyLookAndFeel::positionComboBoxText(juce::ComboBox& b,juce::Label& l)
{
    l.setBounds(6,1,b.getWidth()-22,b.getHeight()-2);l.setFont(getComboBoxFont(b));
}

int JerzyMonoAnalogAudioProcessorEditor::PadGrid::padAt(juce::Point<float> p) const
{
    auto r=getLocalBounds().toFloat().reduced(8.0f);
    const float gap=7.0f;
    const float pw=(r.getWidth()-gap*7.0f)/8.0f;
    const float ph=(r.getHeight()-gap*7.0f)/8.0f;
    int col=(int)((p.x-r.getX())/(pw+gap));
    int row=(int)((p.y-r.getY())/(ph+gap));
    if(col<0||col>7||row<0||row>7)return -1;
    auto cell=juce::Rectangle<float>(r.getX()+col*(pw+gap),r.getY()+row*(ph+gap),pw,ph);
    return cell.contains(p)?row*8+col:-1;
}

void JerzyMonoAnalogAudioProcessorEditor::PadGrid::paint(juce::Graphics& g)
{
    g.fillAll(C(0xff07090b));
    auto r=getLocalBounds().toFloat().reduced(8.0f);
    const float gap=7.0f;
    const float pw=(r.getWidth()-gap*7.0f)/8.0f;
    const float ph=(r.getHeight()-gap*7.0f)/8.0f;
    const int playCol=proc.getGridPlayColumn();

    for(int row=0;row<8;++row)
    {
        for(int col=0;col<8;++col)
        {
            const int raw=row*8+col;
            auto cell=juce::Rectangle<float>(r.getX()+col*(pw+gap),r.getY()+row*(ph+gap),pw,ph);
            juce::Colour colr;

            if(mode==JerzyMonoAnalogAudioProcessor::GridMode::sequencer)
            {
                const bool on=proc.getGridStep(bank,col,row);
                colr=on?juce::Colour::fromHSV((float)(7-row)/9.0f,0.90f,0.95f,1.0f):C(0xff182027);
                if(playCol==col)
                    colr=on?C(YELLOW):C(0xff5d5314);
            }
            else
            {
                const float hue=(float)((7-row)*8+col)/64.0f;
                colr=juce::Colour::fromHSV(hue,0.82f,raw==heldPad?1.0f:0.58f,1.0f);
            }

            g.setColour(colr.withAlpha(0.20f));g.fillRoundedRectangle(cell.expanded(4.0f),7.0f);
            g.setColour(colr);g.fillRoundedRectangle(cell,6.0f);
            g.setColour(juce::Colours::white.withAlpha(0.22f));g.drawRoundedRectangle(cell.reduced(1.0f),5.0f,1.0f);
        }
    }
}

void JerzyMonoAnalogAudioProcessorEditor::PadGrid::mouseDown(const juce::MouseEvent& e)
{
    const int raw=padAt(e.position);if(raw<0)return;
    const int row=raw/8,col=raw%8;
    if(mode==JerzyMonoAnalogAudioProcessor::GridMode::sequencer)
    {
        const bool now=!proc.getGridStep(bank,col,row);
        proc.setGridStep(bank,col,row,now);
        repaint();
    }
    else
    {
        const int launchIndex=(7-row)*8+col;
        heldPad=raw;
        proc.launchPadNoteOn(launchIndex);
        repaint();
    }
}

void JerzyMonoAnalogAudioProcessorEditor::PadGrid::mouseDrag(const juce::MouseEvent& e)
{
    if(mode!=JerzyMonoAnalogAudioProcessor::GridMode::launch)return;
    const int raw=padAt(e.position);
    if(raw<0||raw==heldPad)return;
    if(heldPad>=0)
    {
        const int oldRow=heldPad/8,oldCol=heldPad%8;
        proc.launchPadNoteOff((7-oldRow)*8+oldCol);
    }
    heldPad=raw;
    const int row=raw/8,col=raw%8;
    proc.launchPadNoteOn((7-row)*8+col);
    repaint();
}

void JerzyMonoAnalogAudioProcessorEditor::PadGrid::mouseUp(const juce::MouseEvent&)
{
    if(mode==JerzyMonoAnalogAudioProcessor::GridMode::launch && heldPad>=0)
    {
        const int row=heldPad/8,col=heldPad%8;
        proc.launchPadNoteOff((7-row)*8+col);
        heldPad=-1;
        repaint();
    }
}

void JerzyMonoAnalogAudioProcessorEditor::OutputMeter::paint(juce::Graphics& g)
{
    auto r=getLocalBounds().toFloat().reduced(1);g.setColour(C(0xff070809));g.fillRoundedRectangle(r,3);
    const int n=16;const float gap=2.0f,seg=(r.getWidth()-gap*(n-1))/n;
    for(int i=0;i<n;++i)
    {
        const float t=(i+1)/(float)n;auto rr=juce::Rectangle<float>(r.getX()+i*(seg+gap),r.getY()+2,seg,r.getHeight()-4);
        auto col=t<.60f?C(GREEN):(t<.82f?C(YELLOW):C(RED));g.setColour(t<=level?col:col.withAlpha(.12f));g.fillRoundedRectangle(rr,1);
    }
}

JerzyMonoAnalogAudioProcessorEditor::JerzyMonoAnalogAudioProcessorEditor(JerzyMonoAnalogAudioProcessor& p)
:AudioProcessorEditor(&p),proc(p),padGrid(p)
{
    setLookAndFeel(&look);setOpaque(true);setResizable(true,true);setResizeLimits(1000,500,1920,1200);getConstrainer()->setFixedAspectRatio(2.0);setSize(1200,600);

    title.setText("JERZY MONO ANALOG",juce::dontSendNotification);title.setColour(juce::Label::textColourId,lcdText);title.setColour(juce::Label::backgroundColourId,lcdBg);title.setJustificationType(juce::Justification::centredLeft);
    subtitle.setText("ANALOG MODELING SYNTH",juce::dontSendNotification);subtitle.setColour(juce::Label::textColourId,lcdText);subtitle.setColour(juce::Label::backgroundColourId,lcdBg);subtitle.setJustificationType(juce::Justification::centred);
    preset.setText("VECTOR LCD GUI",juce::dontSendNotification);preset.setColour(juce::Label::textColourId,lcdText);preset.setColour(juce::Label::backgroundColourId,lcdBg);preset.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(title);addAndMakeVisible(subtitle);addAndMakeVisible(preset);

    arpPanelButton.setButtonText("ARP V");
    arpPanelButton.onClick=[this]{setArpPanelVisible(!arpPanelOpen);};
    addAndMakeVisible(arpPanelButton);

    pageButton.setButtonText("PADS");
    pageButton.onClick=[this]{setMainPage(!padsPage);};
    addAndMakeVisible(pageButton);

    setupToggle(gridSeqOn,"SEQ PLAY",C(GREEN));
    setupToggle(gridMidiTrigger,"MIDI TRIG",C(YELLOW));
    setupCombo(gridDivision,{"1/1","1/2","1/4","1/8","1/16","1/32","1/4T","1/8T","1/16T","1/4D","1/8D","1/16D"});
    setupCombo(gridRoot,{"C1","C#1","D1","D#1","E1","F1","F#1","G1","G#1","A1","A#1","B1","C2","C#2","D2","D#2","E2","F2","F#2","G2","G#2","A2","A#2","B2","C3","C#3","D3","D#3","E3","F3","F#3","G3","G#3","A3","A#3","B3","C4","C#4","D4","D#4","E4","F4","F#4","G4","G#4","A4","A#4","B4","C5","C#5","D5","D#5","E5","F5","F#5","G5","G#5","A5","A#5","B5","C6"});
    setupCombo(gridScale,{"CHROMATIC","MAJOR","NAT MINOR","DORIAN","PHRYGIAN","MIXOLYDIAN","MAJOR PENT","MINOR PENT"});
    setupCombo(gridBanks,{"1 BANK / 8 STEPS","2 BANKS / 16 STEPS","3 BANKS / 24 STEPS","4 BANKS / 32 STEPS","5 BANKS / 40 STEPS","6 BANKS / 48 STEPS","7 BANKS / 56 STEPS","8 BANKS / 64 STEPS"});
    setupKnob(gridGate,"GATE","",0.75);
    setupCombo(gridDirection,{"FORWARD","REVERSE","PING-PONG","RANDOM"});
    setupKnob(gridSwing,"SWING","",0.0);setupKnob(gridVelocity,"VELOCITY","",0.95);
    setupKnob(arpSwing,"SWING","",0.0);setupKnob(arpVelocity,"VELOCITY","",0.9);
    gridModeButton.setButtonText("MODE: SEQ");
    gridModeButton.onClick=[this]
    {
        const auto next=proc.getGridMode()==JerzyMonoAnalogAudioProcessor::GridMode::sequencer
            ? JerzyMonoAnalogAudioProcessor::GridMode::launch
            : JerzyMonoAnalogAudioProcessor::GridMode::sequencer;
        proc.setGridMode(next);
        padGrid.setMode(next);
        updateGridControls();
    };
    addAndMakeVisible(gridModeButton);

    gridClearButton.setButtonText("CLEAR BANK");
    gridClearButton.onClick=[this]{proc.clearGridBank(proc.getGridBank());padGrid.repaint();};
    addAndMakeVisible(gridClearButton);

    gridBankBox.addItemList({"BANK 1","BANK 2","BANK 3","BANK 4","BANK 5","BANK 6","BANK 7","BANK 8"},1);
    gridBankBox.setSelectedItemIndex(0,juce::dontSendNotification);
    gridBankBox.onChange=[this]
    {
        const int b=juce::jmax(0,gridBankBox.getSelectedItemIndex());
        proc.setGridBank(b);padGrid.setBank(b);
    };
    addAndMakeVisible(gridBankBox);
    addAndMakeVisible(padGrid);

    setupCombo(osc1Wave,{"SINE","TRIANGLE","SAW","SQUARE"});setupCombo(osc1Oct,{"16'","8'","4'","2'","1'"});
    setupCombo(osc2Wave,{"SINE","TRIANGLE","SAW","SQUARE"});setupCombo(osc2Oct,{"16'","8'","4'","2'","1'"});
    setupCombo(filterMode,{"LADDER 24 dB","LP 12 dB","LP 24 dB","HP 12 dB","HP 24 dB","BP 12 dB","BP 24 dB"});
    setupCombo(subWave,{"SINE","SQUARE"});
    setupCombo(lfoWave,{"SINE","TRIANGLE","SAW","SQUARE","S&H"});
    setupCombo(lfoDivision,{"1/1","1/2","1/4","1/8","1/16","1/32","1/4T","1/8T","1/16T","1/4D","1/8D","1/16D"});
    setupCombo(glideMode,{"ALWAYS","LEGATO"});setupCombo(priority,{"LAST","LOW","HIGH"});
    setupCombo(arpDivision,{"1/1","1/2","1/4","1/8","1/16","1/32","1/4T","1/8T","1/16T","1/4D","1/8D","1/16D"});
    setupCombo(arpPattern,{"UP","DOWN","UP-DOWN","RANDOM","AS PLAYED"});
    setupCombo(arpRhythm,{"STRAIGHT","EVERY 2","3-3-2","SYNCOPATED"});
    setupCombo(arpOctaves,{"1 OCT","2 OCT","3 OCT","4 OCT"});

    setupToggle(legato,"LEGATO",C(GREEN));setupToggle(retrigger,"RETRIGGER",C(RED));setupToggle(lfoSync,"HOST SYNC",C(YELLOW));
    setupToggle(arpOn,"ARP ON",C(GREEN));setupToggle(arpLatch,"LATCH",C(YELLOW));setupToggle(arpRetrigger,"RETRIGGER",C(RED));
    addAndMakeVisible(outputMeter);

    addSection("OSC 1",C(GREEN),15,75,245,215);addSection("OSC 2",C(YELLOW),265,75,265,215);addSection("SUB / NOISE",C(RED),535,75,210,215);
    addSection("MIXER / DRIVE",C(RED),750,75,190,215);addSection("FILTER",C(GREEN),945,75,480,215);
    addSection("AMP ENV",C(GREEN),15,300,255,205);addSection("MOD ENV",C(YELLOW),275,300,255,205);addSection("LFO",C(RED),535,300,890,205);
    addSection("PLAY MODE",C(YELLOW),15,515,720,175);addSection("OUTPUT",C(GREEN),740,515,685,175);

    auto&s=proc.apvts;
    filterModeA=std::make_unique<ComboAttachment>(s,"filterMode",filterMode);
    modEnvPitchA=std::make_unique<SliderAttachment>(s,"modEnvPitch",modEnvPitch);
    modEnvPWMA=std::make_unique<SliderAttachment>(s,"modEnvPWM",modEnvPWM);
    osc1WaveA=std::make_unique<ComboAttachment>(s,"osc1Wave",osc1Wave);osc1OctA=std::make_unique<ComboAttachment>(s,"osc1Oct",osc1Oct);
    osc2WaveA=std::make_unique<ComboAttachment>(s,"osc2Wave",osc2Wave);osc2OctA=std::make_unique<ComboAttachment>(s,"osc2Oct",osc2Oct);
    subWaveA=std::make_unique<ComboAttachment>(s,"subWave",subWave);lfoWaveA=std::make_unique<ComboAttachment>(s,"lfoWave",lfoWave);
    lfoDivisionA=std::make_unique<ComboAttachment>(s,"lfoDivision",lfoDivision);glideModeA=std::make_unique<ComboAttachment>(s,"glideMode",glideMode);priorityA=std::make_unique<ComboAttachment>(s,"priority",priority);
    arpDivisionA=std::make_unique<ComboAttachment>(s,"arpDivision",arpDivision);arpPatternA=std::make_unique<ComboAttachment>(s,"arpPattern",arpPattern);arpRhythmA=std::make_unique<ComboAttachment>(s,"arpRhythm",arpRhythm);arpOctavesA=std::make_unique<ComboAttachment>(s,"arpOctaves",arpOctaves);

    osc1LevelA=std::make_unique<SliderAttachment>(s,"osc1Level",osc1Level);pulseWidthA=std::make_unique<SliderAttachment>(s,"pw",pulseWidth);osc2LevelA=std::make_unique<SliderAttachment>(s,"osc2Level",osc2Level);detuneA=std::make_unique<SliderAttachment>(s,"detune",detune);
    subLevelA=std::make_unique<SliderAttachment>(s,"subLevel",subLevel);noiseLevelA=std::make_unique<SliderAttachment>(s,"noiseLevel",noiseLevel);mixDriveA=std::make_unique<SliderAttachment>(s,"mixDrive",mixDrive);driftA=std::make_unique<SliderAttachment>(s,"drift",drift);
    cutoffA=std::make_unique<SliderAttachment>(s,"cutoff",cutoff);resonanceA=std::make_unique<SliderAttachment>(s,"resonance",resonance);filterDriveA=std::make_unique<SliderAttachment>(s,"filterDrive",filterDrive);filterEnvA=std::make_unique<SliderAttachment>(s,"filterEnv",filterEnv);keyTrackA=std::make_unique<SliderAttachment>(s,"keyTrack",keyTrack);
    aAA=std::make_unique<SliderAttachment>(s,"aA",aA);aDA=std::make_unique<SliderAttachment>(s,"aD",aD);aSA=std::make_unique<SliderAttachment>(s,"aS",aS);aRA=std::make_unique<SliderAttachment>(s,"aR",aR);
    fAA=std::make_unique<SliderAttachment>(s,"fA",fA);fDA=std::make_unique<SliderAttachment>(s,"fD",fD);fSA=std::make_unique<SliderAttachment>(s,"fS",fS);fRA=std::make_unique<SliderAttachment>(s,"fR",fR);
    lfoRateA=std::make_unique<SliderAttachment>(s,"lfoRate",lfoRate);lfoPitchA=std::make_unique<SliderAttachment>(s,"lfoPitch",lfoPitch);lfoFilterA=std::make_unique<SliderAttachment>(s,"lfoFilter",lfoFilter);lfoPWMA=std::make_unique<SliderAttachment>(s,"lfoPWM",lfoPWM);lfoAmpA=std::make_unique<SliderAttachment>(s,"lfoAmp",lfoAmp);lfoFadeA=std::make_unique<SliderAttachment>(s,"lfoFade",lfoFade);
    glideA=std::make_unique<SliderAttachment>(s,"glide",glide);outDriveA=std::make_unique<SliderAttachment>(s,"outDrive",outDrive);masterA=std::make_unique<SliderAttachment>(s,"master",master);arpGateA=std::make_unique<SliderAttachment>(s,"arpGate",arpGate);
    legatoA=std::make_unique<ButtonAttachment>(s,"legato",legato);retriggerA=std::make_unique<ButtonAttachment>(s,"retrigger",retrigger);lfoSyncA=std::make_unique<ButtonAttachment>(s,"lfoSync",lfoSync);
    arpOnA=std::make_unique<ButtonAttachment>(s,"arpOn",arpOn);arpLatchA=std::make_unique<ButtonAttachment>(s,"arpLatch",arpLatch);arpRetriggerA=std::make_unique<ButtonAttachment>(s,"arpRetrigger",arpRetrigger);
    gridSeqOnA=std::make_unique<ButtonAttachment>(s,"gridSeqOn",gridSeqOn);
    gridMidiTriggerA=std::make_unique<ButtonAttachment>(s,"gridMidiTrigger",gridMidiTrigger);
    gridDivisionA=std::make_unique<ComboAttachment>(s,"gridDivision",gridDivision);
    gridRootA=std::make_unique<ComboAttachment>(s,"gridRoot",gridRoot);
    gridScaleA=std::make_unique<ComboAttachment>(s,"gridScale",gridScale);
    gridBanksA=std::make_unique<ComboAttachment>(s,"gridBanks",gridBanks);
    gridDirectionA=std::make_unique<ComboAttachment>(s,"gridDirection",gridDirection);
    gridGateA=std::make_unique<SliderAttachment>(s,"gridGate",gridGate);
    gridSwingA=std::make_unique<SliderAttachment>(s,"gridSwing",gridSwing);
    gridVelocityA=std::make_unique<SliderAttachment>(s,"gridVelocity",gridVelocity);
    arpSwingA=std::make_unique<SliderAttachment>(s,"arpSwing",arpSwing);
    arpVelocityA=std::make_unique<SliderAttachment>(s,"arpVelocity",arpVelocity);

    setupKnob(osc1Level,"LEVEL","",0.0);setupKnob(pulseWidth,"PULSE WIDTH","",0.5);
    setupKnob(osc2Level,"LEVEL","",0.0);setupKnob(detune,"DETUNE","ct",0.0);
    setupKnob(subLevel,"SUB LEVEL","",0.0);setupKnob(noiseLevel,"NOISE","",0.0);
    setupKnob(mixDrive,"MIX DRIVE","",0.0);setupKnob(drift,"DRIFT","ct",0.0);
    setupKnob(cutoff,"CUTOFF","Hz",20.0);setupKnob(resonance,"RESONANCE","",0.0);setupKnob(filterDrive,"FILTER DRIVE","",0.0);setupKnob(filterEnv,"ENV AMOUNT","oct",0.0);setupKnob(keyTrack,"KEY TRACK","",0.0);
    setupEnvSlider(aA,"ATTACK","s",0.0);setupEnvSlider(aD,"DECAY","s",0.0);setupEnvSlider(aS,"SUSTAIN","",0.0);setupEnvSlider(aR,"RELEASE","s",0.0);
    setupEnvSlider(fA,"ATTACK","s",0.0);setupEnvSlider(fD,"DECAY","s",0.0);setupEnvSlider(fS,"SUSTAIN","",0.0);setupEnvSlider(fR,"RELEASE","s",0.0);
    setupKnob(lfoRate,"RATE","Hz",0.03);setupKnob(lfoPitch,"PITCH","ct",0.0);setupKnob(lfoFilter,"FILTER","oct",0.0);setupKnob(lfoPWM,"PWM","",0.0);setupKnob(lfoAmp,"AMP","",0.0);setupKnob(lfoFade,"FADE IN","s",0.0);
    setupKnob(glide,"GLIDE","s",0.0);setupKnob(outDrive,"OUTPUT DRIVE","",0.0);setupKnob(master,"MASTER","",0.8);
    setupKnob(modEnvPitch,"MOD ENV PITCH","st",0.0);setupKnob(modEnvPWM,"MOD ENV PWM","",0.0);
    setupKnob(arpGate,"GATE","",0.72);

    modEnvPitch.setSliderStyle(juce::Slider::LinearHorizontal);modEnvPitch.setTextBoxStyle(juce::Slider::TextBoxRight,false,65,22);
    modEnvPWM.setSliderStyle(juce::Slider::LinearHorizontal);modEnvPWM.setTextBoxStyle(juce::Slider::TextBoxRight,false,65,22);
    modEnvPWM.textFromValueFunction=[](double v){return juce::String(v*100.0,0)+" %";};
    modEnvPWM.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue()*0.01;};modEnvPWM.updateText();
    setArpPanelVisible(false);
    setMainPage(false);
    startTimerHz(20);
}

JerzyMonoAnalogAudioProcessorEditor::~JerzyMonoAnalogAudioProcessorEditor(){stopTimer();setLookAndFeel(nullptr);}

void JerzyMonoAnalogAudioProcessorEditor::setupKnob(ResetSlider& k,const juce::String& name,const juce::String& unit,double neutral)
{
    k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,80,20);
    k.setNumDecimalPlacesToDisplay(1);
    k.textFromValueFunction=[unit](double v){return juce::String(v,1)+(unit.isEmpty()?"":" "+unit);};
    k.setNeutralValue(neutral);k.setTooltip(name);k.updateText();addAndMakeVisible(k);
}
void JerzyMonoAnalogAudioProcessorEditor::setupEnvSlider(ResetSlider& k,const juce::String& name,const juce::String& unit,double neutral)
{
    k.setSliderStyle(juce::Slider::LinearVertical);
    k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,54,22);
    k.setNumDecimalPlacesToDisplay(1);
    k.textFromValueFunction=[unit](double v){
        if(unit == "s") return v < 1.0 ? juce::String(v * 1000.0, v < 0.01 ? 1 : 0) + " ms" : juce::String(v,2) + " s";
        return juce::String(v * 100.0,0) + " %";
    };
    k.valueFromTextFunction=[unit](const juce::String& text){
        const double v=text.getDoubleValue();
        if(unit == "s") return text.containsIgnoreCase("ms") ? v * 0.001 : v;
        return v * 0.01;
    };
    k.setNeutralValue(neutral);k.setTooltip(name);k.updateText();addAndMakeVisible(k);
}
void JerzyMonoAnalogAudioProcessorEditor::setupCombo(juce::ComboBox& b,const juce::StringArray& items){b.addItemList(items,1);addAndMakeVisible(b);}
void JerzyMonoAnalogAudioProcessorEditor::setupToggle(juce::ToggleButton& b,const juce::String& t,juce::Colour col){b.setButtonText(t);b.setColour(juce::ToggleButton::tickColourId,col);addAndMakeVisible(b);}
void JerzyMonoAnalogAudioProcessorEditor::place(juce::Component& c,float x,float y,float w,float h){const float sc=scale();c.setBounds(juce::roundToInt(x*sc),juce::roundToInt(y*sc),juce::roundToInt(w*sc),juce::roundToInt(h*sc));}
void JerzyMonoAnalogAudioProcessorEditor::addSection(const juce::String&t,juce::Colour led,float x,float y,float w,float h){sections.push_back({t,led,{x,y,w,h}});}

void JerzyMonoAnalogAudioProcessorEditor::drawSection(juce::Graphics& g,const Section& sec) const
{
    const float sc=scale();auto r=juce::Rectangle<float>(sec.bounds.getX()*sc,sec.bounds.getY()*sc,sec.bounds.getWidth()*sc,sec.bounds.getHeight()*sc);
    g.setColour(C(PANEL));g.fillRoundedRectangle(r,5*sc);g.setColour(C(EDGE));g.drawRoundedRectangle(r,5*sc,juce::jmax(1.0f,1.2f*sc));
    auto hdr=r.removeFromTop(29*sc).reduced(5*sc,3*sc);g.setColour(lcdBg);g.fillRoundedRectangle(hdr,2*sc);g.setColour(lcdText);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),juce::jmax(8.0f,12.0f*sc),juce::Font::bold)));
    g.drawFittedText(sec.title,hdr.toNearestInt().reduced((int)(25*sc),0),juce::Justification::centredLeft,1,.75f);drawLed(g,{hdr.getX()+12*sc,hdr.getCentreY()},3.5f*sc,sec.led);
}
void JerzyMonoAnalogAudioProcessorEditor::drawLabelBox(juce::Graphics& g,const juce::String&t,float x,float y,float w) const
{
    const float sc=scale();auto r=juce::Rectangle<float>(x*sc,y*sc,w*sc,16*sc);g.setColour(lcdBg);g.fillRoundedRectangle(r,2*sc);g.setColour(lcdText);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),juce::jmax(7.0f,9.0f*sc),juce::Font::bold)));
    g.drawFittedText(t,r.toNearestInt().reduced(2,0),juce::Justification::centred,1,.72f);
}
void JerzyMonoAnalogAudioProcessorEditor::drawEnvelope(juce::Graphics&g,juce::Rectangle<float>r,bool filt) const
{
    const float sc=scale();
    r=juce::Rectangle<float>(r.getX()*sc,r.getY()*sc,r.getWidth()*sc,r.getHeight()*sc);
    const double a=(filt?fA:aA).getValue(), d=(filt?fD:aD).getValue();
    const double sustain=(filt?fS:aS).getValue(), release=(filt?fR:aR).getValue();
    const double hold=0.25;
    const double total=a+d+hold+release;
    auto point=[&](double time,double value){return juce::Point<float>(r.getX()+r.getWidth()*static_cast<float>(time/total),r.getBottom()-r.getHeight()*static_cast<float>(value));};
    juce::Path p;p.startNewSubPath(point(0.0,0.0));
    for(int i=1;i<=48;++i){const double t=i/48.0;p.lineTo(point(a*t,jerzy::envelopeCurve(t,true)));}
    for(int i=1;i<=48;++i){const double t=i/48.0;p.lineTo(point(a+d*t,1.0+(sustain-1.0)*jerzy::envelopeCurve(t,false)));}
    p.lineTo(point(a+d+hold,sustain));
    for(int i=1;i<=48;++i){const double t=i/48.0;p.lineTo(point(a+d+hold+release*t,sustain*(1.0-jerzy::envelopeCurve(t,false))));}
    g.setColour((filt?C(YELLOW):C(GREEN)).withAlpha(.85f));g.strokePath(p,juce::PathStrokeType(juce::jmax(1.0f,1.4f*sc)));
}

void JerzyMonoAnalogAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(C(0xff050607));const float sc=scale();g.setColour(C(0xff15181b));g.fillRect(0,0,getWidth(),juce::roundToInt(64*sc));
    if(!padsPage)
    {
        for(const auto&s:sections)drawSection(g,s);
        drawEnvelope(g,{35,330,215,34},false);drawEnvelope(g,{295,330,215,34},true);
    }
    else
    {
        auto outer=juce::Rectangle<float>(15*sc,75*sc,1410*sc,615*sc);
        g.setColour(C(PANEL));g.fillRoundedRectangle(outer,6*sc);
        g.setColour(C(EDGE));g.drawRoundedRectangle(outer,6*sc,1.2f*sc);
        auto hdr=outer.removeFromTop(34*sc).reduced(5*sc,3*sc);
        g.setColour(lcdBg);g.fillRoundedRectangle(hdr,2*sc);g.setColour(lcdText);
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),12*sc,juce::Font::bold)));
        g.drawText("8x8 RGB GRID / 64-STEP SEQUENCER / LAUNCHPAD",hdr.toNearestInt().reduced(20,0),juce::Justification::centredLeft);
    }

    if(!padsPage)
    {

    // Row 1 labels
    drawLabelBox(g,"WAVE",30,108,105);drawLabelBox(g,"OCTAVE",145,108,95);drawLabelBox(g,"LEVEL",35,263,90);drawLabelBox(g,"PULSE WIDTH",145,263,90);
    drawLabelBox(g,"WAVE",285,108,110);drawLabelBox(g,"OCTAVE",405,108,100);drawLabelBox(g,"LEVEL",290,263,95);drawLabelBox(g,"DETUNE",410,263,95);
    drawLabelBox(g,"SUB WAVE",555,108,170);drawLabelBox(g,"SUB LEVEL",555,263,80);drawLabelBox(g,"NOISE",645,263,80);
    drawLabelBox(g,"MIX DRIVE",770,263,75);drawLabelBox(g,"DRIFT",855,263,65);
    drawLabelBox(g,"CUTOFF",958,263,82);drawLabelBox(g,"RESONANCE",1048,263,82);drawLabelBox(g,"FILTER DRIVE",1138,263,82);drawLabelBox(g,"ENV AMOUNT",1228,263,82);drawLabelBox(g,"KEY TRACK",1318,263,82);

    drawLabelBox(g,"FILTER MODE",958,108,442);
    drawLabelBox(g,"ENV > PITCH",960,334,190);drawLabelBox(g,"ENV > PWM",1170,334,190);

    // ENV labels
    drawLabelBox(g,"A",32,477,48);drawLabelBox(g,"D",92,477,48);drawLabelBox(g,"S",152,477,48);drawLabelBox(g,"R",212,477,48);
    drawLabelBox(g,"A",292,477,48);drawLabelBox(g,"D",352,477,48);drawLabelBox(g,"S",412,477,48);drawLabelBox(g,"R",472,477,48);

    // LFO
    drawLabelBox(g,"WAVE",555,334,120);drawLabelBox(g,"DIVISION",685,334,120);drawLabelBox(g,"SYNC",815,334,110);
    drawLabelBox(g,"RATE",555,482,120);drawLabelBox(g,"PITCH",690,482,120);drawLabelBox(g,"FILTER",825,482,120);drawLabelBox(g,"PWM",960,482,120);drawLabelBox(g,"AMP",1095,482,120);drawLabelBox(g,"FADE IN",1230,482,120);

    // Play and output
    drawLabelBox(g,"GLIDE",35,665,105);drawLabelBox(g,"GLIDE MODE",165,550,145);drawLabelBox(g,"NOTE PRIORITY",325,550,145);
    drawLabelBox(g,"OUTPUT LEVEL",770,550,210);drawLabelBox(g,"OUTPUT DRIVE",1040,665,110);drawLabelBox(g,"MASTER",1190,665,110);

    }
    if(!padsPage && arpPanelOpen)
    {
        auto r=juce::Rectangle<float>(15*sc,720*sc,1410*sc,165*sc);g.setColour(C(PANEL));g.fillRoundedRectangle(r,5*sc);g.setColour(C(EDGE));g.drawRoundedRectangle(r,5*sc,1.2f*sc);
        auto hdr=r.removeFromTop(30*sc).reduced(5*sc,3*sc);g.setColour(lcdBg);g.fillRoundedRectangle(hdr,2*sc);g.setColour(lcdText);
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),12*sc,juce::Font::bold)));g.drawText("ARPEGGIATOR",hdr.toNearestInt().reduced((int)(25*sc),0),juce::Justification::centredLeft);
        drawLed(g,{hdr.getX()+12*sc,hdr.getCentreY()},3.5f*sc,C(GREEN));
        drawLabelBox(g,"DIVISION",170,752,150);drawLabelBox(g,"PATTERN",335,752,180);drawLabelBox(g,"RHYTHM",530,752,180);drawLabelBox(g,"OCTAVES",725,752,130);drawLabelBox(g,"GATE",870,855,110);
    }

    if(padsPage)
    {
        drawLabelBox(g,"MODE",35,112,140);
        drawLabelBox(g,"BANK",190,112,120);
        drawLabelBox(g,"LENGTH",325,112,175);
        drawLabelBox(g,"TEMPO DIVISION",515,112,145);
        drawLabelBox(g,"SCALE",675,112,160);
        drawLabelBox(g,"ROOT NOTE",850,112,130);
        drawLabelBox(g,"GATE",35,178,110);
        drawLabelBox(g,"PLAY",165,178,120);
        drawLabelBox(g,"MIDI START / GATE",305,178,150);
        drawLabelBox(g,"EDIT",475,178,150);
    }
}

void JerzyMonoAnalogAudioProcessorEditor::resized()
{
    const float sc=scale();
    title.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),22*sc,juce::Font::bold)));
    subtitle.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),11*sc,juce::Font::bold)));
    preset.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),10*sc,juce::Font::bold)));
    place(title,20,10,310,40);place(subtitle,515,13,410,35);place(preset,1050,13,250,35);place(pageButton,1310,13,105,35);
    if(!padsPage) place(arpPanelButton,1320,684,95,28);

    if(padsPage)
    {
        place(gridModeButton,35,132,140,34);
        place(gridBankBox,190,132,120,34);
        place(gridBanks,325,132,175,34);
        place(gridDivision,515,132,145,34);
        place(gridScale,675,132,160,34);
        place(gridRoot,850,132,130,34);
    place(gridDirection,1000,132,180,34);

        place(gridGate,35,194,110,52);
        place(gridSeqOn,165,198,120,34);
        place(gridMidiTrigger,305,198,150,34);
        place(gridClearButton,475,198,150,34);
        place(gridVelocity,645,182,100,70);place(gridSwing,755,182,100,70);
        place(padGrid,180,252,1080,420);
        return;
    }

        place(osc1Wave,30,128,105,30);place(osc1Oct,145,128,95,30);place(osc1Level,35,168,90,88);place(pulseWidth,145,168,90,88);
    place(osc2Wave,285,128,110,30);place(osc2Oct,405,128,100,30);place(osc2Level,290,168,95,88);place(detune,410,168,95,88);
    place(subWave,555,128,170,30);place(subLevel,555,168,80,88);place(noiseLevel,645,168,80,88);
    place(mixDrive,770,155,75,100);place(drift,855,155,65,100);

    place(filterMode,958,128,442,30);
    place(cutoff,958,168,82,88);place(resonance,1048,168,82,88);place(filterDrive,1138,168,82,88);place(filterEnv,1228,168,82,88);place(keyTrack,1318,168,82,88);
    place(modEnvPitch,960,354,190,30);place(modEnvPWM,1170,354,190,30);

    place(aA,30,370,54,98);place(aD,90,370,54,98);place(aS,150,370,54,98);place(aR,210,370,54,98);
    place(fA,290,370,54,98);place(fD,350,370,54,98);place(fS,410,370,54,98);place(fR,470,370,54,98);

    place(lfoWave,555,354,120,30);place(lfoDivision,685,354,120,30);place(lfoSync,815,354,110,30);
    place(lfoRate,555,398,120,72);place(lfoPitch,690,398,120,72);place(lfoFilter,825,398,120,72);place(lfoPWM,960,398,120,72);place(lfoAmp,1095,398,120,72);place(lfoFade,1230,398,120,72);

    place(glide,35,555,105,105);place(glideMode,165,572,145,32);place(priority,325,572,145,32);place(legato,500,572,120,32);place(retrigger,630,572,90,32);
    place(outputMeter,770,575,210,28);place(outDrive,1040,550,110,108);place(master,1190,550,110,108);

    if(arpPanelOpen)
    {
        place(arpOn,25,760,115,34);place(arpDivision,150,775,135,32);place(arpPattern,295,775,155,32);place(arpRhythm,460,775,150,32);place(arpOctaves,620,775,110,32);
        place(arpGate,735,760,95,90);place(arpVelocity,840,760,95,90);place(arpSwing,945,760,95,90);
        place(arpLatch,1050,775,115,34);place(arpRetrigger,1180,775,150,34);
    }
}

void JerzyMonoAnalogAudioProcessorEditor::setSynthControlsVisible(bool v)
{
    for(auto* c:{
        (juce::Component*)&arpPanelButton,
        (juce::Component*)&osc1Wave,(juce::Component*)&osc1Oct,(juce::Component*)&osc2Wave,(juce::Component*)&osc2Oct,(juce::Component*)&subWave,
        (juce::Component*)&lfoWave,(juce::Component*)&lfoDivision,(juce::Component*)&glideMode,(juce::Component*)&priority,
        (juce::Component*)&arpDivision,(juce::Component*)&arpPattern,(juce::Component*)&arpRhythm,(juce::Component*)&arpOctaves,
        (juce::Component*)&osc1Level,(juce::Component*)&pulseWidth,(juce::Component*)&osc2Level,(juce::Component*)&detune,(juce::Component*)&subLevel,(juce::Component*)&noiseLevel,
        (juce::Component*)&filterMode,(juce::Component*)&modEnvPitch,(juce::Component*)&modEnvPWM,
        (juce::Component*)&mixDrive,(juce::Component*)&drift,(juce::Component*)&cutoff,(juce::Component*)&resonance,(juce::Component*)&filterDrive,(juce::Component*)&filterEnv,(juce::Component*)&keyTrack,
        (juce::Component*)&aA,(juce::Component*)&aD,(juce::Component*)&aS,(juce::Component*)&aR,(juce::Component*)&fA,(juce::Component*)&fD,(juce::Component*)&fS,(juce::Component*)&fR,
        (juce::Component*)&lfoRate,(juce::Component*)&lfoPitch,(juce::Component*)&lfoFilter,(juce::Component*)&lfoPWM,(juce::Component*)&lfoAmp,(juce::Component*)&lfoFade,
        (juce::Component*)&glide,(juce::Component*)&outDrive,(juce::Component*)&master,(juce::Component*)&arpGate,(juce::Component*)&arpSwing,(juce::Component*)&arpVelocity,
        (juce::Component*)&legato,(juce::Component*)&retrigger,(juce::Component*)&lfoSync,(juce::Component*)&arpOn,(juce::Component*)&arpLatch,(juce::Component*)&arpRetrigger,
        (juce::Component*)&outputMeter
    }) c->setVisible(v);

    if(v)
        setArpPanelVisible(arpPanelOpen);
}

void JerzyMonoAnalogAudioProcessorEditor::setMainPage(bool pads)
{
    padsPage=pads;
    pageButton.setButtonText(pads?"SYNTH":"PADS");

    if(pads)
    {
        getConstrainer()->setFixedAspectRatio(2.0);
        arpPanelOpen=false;
        setSynthControlsVisible(false);
        for(auto* c:{(juce::Component*)&gridSeqOn,(juce::Component*)&gridMidiTrigger,(juce::Component*)&gridModeButton,(juce::Component*)&gridClearButton,(juce::Component*)&gridBankBox,
                     (juce::Component*)&gridBanks,(juce::Component*)&gridDivision,(juce::Component*)&gridScale,(juce::Component*)&gridRoot,(juce::Component*)&gridDirection,(juce::Component*)&gridGate,(juce::Component*)&gridSwing,(juce::Component*)&gridVelocity,(juce::Component*)&padGrid}) c->setVisible(true);
        const int w=getWidth();setSize(w,juce::roundToInt(720.0f*(w/1440.0f)));
    }
    else
    {
        for(auto* c:{(juce::Component*)&gridSeqOn,(juce::Component*)&gridMidiTrigger,(juce::Component*)&gridModeButton,(juce::Component*)&gridClearButton,(juce::Component*)&gridBankBox,
                     (juce::Component*)&gridBanks,(juce::Component*)&gridDivision,(juce::Component*)&gridScale,(juce::Component*)&gridRoot,(juce::Component*)&gridGate,(juce::Component*)&padGrid}) c->setVisible(false);
        setSynthControlsVisible(true);
    }
    resized();repaint();
}

void JerzyMonoAnalogAudioProcessorEditor::updateGridControls()
{
    const bool launch=proc.getGridMode()==JerzyMonoAnalogAudioProcessor::GridMode::launch;
    gridModeButton.setButtonText(launch?"MODE: LAUNCH":"MODE: SEQ");
    gridSeqOn.setEnabled(!launch);
    gridMidiTrigger.setEnabled(!launch);
    gridDivision.setEnabled(!launch);
    gridBanks.setEnabled(!launch);
    gridDirection.setEnabled(!launch);
    gridSwing.setEnabled(!launch);gridVelocity.setEnabled(!launch);
    gridScale.setEnabled(!launch);
    gridRoot.setEnabled(true);
    gridGate.setEnabled(!launch);
    gridBankBox.setEnabled(!launch);
    gridClearButton.setEnabled(!launch);
    padGrid.repaint();
}

void JerzyMonoAnalogAudioProcessorEditor::setArpPanelVisible(bool open)
{
    getConstrainer()->setFixedAspectRatio(open ? 1440.0/900.0 : 2.0);
    arpPanelOpen=open;arpPanelButton.setButtonText(open?"ARP ^":"ARP V");
    for(auto*c:{(juce::Component*)&arpOn,(juce::Component*)&arpDivision,(juce::Component*)&arpPattern,(juce::Component*)&arpRhythm,(juce::Component*)&arpOctaves,(juce::Component*)&arpGate,(juce::Component*)&arpVelocity,(juce::Component*)&arpSwing,(juce::Component*)&arpLatch,(juce::Component*)&arpRetrigger})c->setVisible(open);
    const int w=getWidth();setSize(w,juce::roundToInt((open?900.0f:720.0f)*(w/1440.0f)));resized();repaint();
}

void JerzyMonoAnalogAudioProcessorEditor::timerCallback()
{
    outputMeter.setLevel(proc.getOutputMeter());
    if(!padsPage) repaint();
    const bool sync=lfoSync.getToggleState();lfoRate.setEnabled(!sync);lfoDivision.setEnabled(sync);
    if(padsPage)
    {
        int activeBanks=8;
        if(auto* p=dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter("gridBanks")))
            activeBanks=1+p->getIndex();

        if(gridBankBox.getSelectedItemIndex()>=activeBanks)
        {
            const int b=activeBanks-1;
            gridBankBox.setSelectedItemIndex(b,juce::sendNotificationSync);
            proc.setGridBank(b);
            padGrid.setBank(b);
        }
        padGrid.refresh();
    }
}

