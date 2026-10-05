#include "PluginEditor.h"

namespace
{
constexpr auto RED=0xffdf7654, GREEN=0xff9faf7c, YELLOW=0xffe3b65c;
constexpr auto PANEL=0xff292b29, EDGE=0xff777164;
static juce::Colour C(juce::uint32 x){return juce::Colour(x);}
static const juce::Colour lcdBg=C(0xffd5cdb8), lcdText=C(0xff24231f);
static juce::String midiNoteName(int note)
{
    static const char* names[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    note=juce::jlimit(0,127,note);
    return juce::String(names[note%12])+juce::String(note/12-1);
}

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
    setColour(juce::Slider::textBoxOutlineColourId,C(0xff8d794f));
    setColour(juce::ComboBox::textColourId,lcdText);
    setColour(juce::ComboBox::backgroundColourId,lcdBg);
    setColour(juce::ComboBox::outlineColourId,C(0xff8d794f));
    setColour(juce::PopupMenu::backgroundColourId,C(0xffd7ddd4));
    setColour(juce::PopupMenu::textColourId,lcdText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,C(0xffadb6ad));
    setColour(juce::Label::textColourId,lcdText);
}

void JerzyLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float p,float a0,float a1,juce::Slider&)
{
    auto b=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(5.0f);
    const float d=juce::jmin(b.getWidth(),b.getHeight());auto r=juce::Rectangle<float>(d,d).withCentre(b.getCentre());const auto cc=r.getCentre();
    g.setColour(C(0xff050505).withAlpha(.8f));g.fillEllipse(r.translated(0.0f,d*.045f));
    juce::ColourGradient rim(C(0xffddd7c7),cc.x, r.getY(),C(0xff292b2c),cc.x,r.getBottom(),false);rim.addColour(.48,C(0xff777a79));g.setGradientFill(rim);g.fillEllipse(r);
    g.setColour(C(0xff0e0f0f));g.drawEllipse(r,juce::jmax(1.0f,d*.035f));
    auto face=r.reduced(d*.105f);juce::ColourGradient body(C(0xffb98a43),face.getX(),face.getY(),C(0xff25221b),face.getRight(),face.getBottom(),false);body.addColour(.38,C(0xff6e5735));body.addColour(.72,C(0xff39352c));g.setGradientFill(body);g.fillEllipse(face);
    g.setColour(C(0xffd5b46f).withAlpha(.75f));g.drawEllipse(face,d*.018f);
    auto inner=face.reduced(d*.18f);juce::ColourGradient cap(C(0xffd6d0bf),inner.getX(),inner.getY(),C(0xff555653),inner.getRight(),inner.getBottom(),false);cap.addColour(.5,C(0xff92928b));g.setGradientFill(cap);g.fillEllipse(inner);
    const float a=a0+p*(a1-a0),tickR=d*.49f;
    for(int i=0;i<=20;++i){const float t=(float)i/20.0f,angle=a0+t*(a1-a0);juce::Point<float> p1(cc.x+std::sin(angle)*(tickR-d*.035f),cc.y-std::cos(angle)*(tickR-d*.035f));juce::Point<float> p2(cc.x+std::sin(angle)*(tickR+(i%5==0?d*.005f:-d*.01f)),cc.y-std::cos(angle)*(tickR+(i%5==0?d*.005f:-d*.01f)));g.setColour(t<=p?C(0xffe6b755):C(0xff77746a));g.drawLine(p1.x,p1.y,p2.x,p2.y,juce::jmax(1.0f,d*.012f));}
    juce::Path arc;arc.addCentredArc(cc.x,cc.y,tickR,tickR,0.0f,a0,a,true);g.setColour(C(0xffe0ad4e));g.strokePath(arc,juce::PathStrokeType(juce::jmax(2.0f,d*.035f)));
    juce::Path pointer;pointer.startNewSubPath(cc.x,cc.y-d*.10f);pointer.lineTo(cc.x,cc.y-d*.39f);pointer.applyTransform(juce::AffineTransform::rotation(a,cc.x,cc.y));
    g.setColour(C(0xff20180a));g.strokePath(pointer,juce::PathStrokeType(juce::jmax(4.0f,d*.09f),juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    g.setColour(C(0xffffd36e));g.strokePath(pointer,juce::PathStrokeType(juce::jmax(2.0f,d*.04f),juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
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
    auto r=getLocalBounds().toFloat().reduced(8.0f);r.removeFromLeft(24.0f);r.removeFromTop(20.0f);
    const float gap=5.0f;
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
    g.fillAll(C(0xff20221f));
    auto r=getLocalBounds().toFloat().reduced(8.0f);r.removeFromLeft(24.0f);r.removeFromTop(20.0f);
    const float gap=5.0f;
    const float pw=(r.getWidth()-gap*7.0f)/8.0f;
    const float ph=(r.getHeight()-gap*7.0f)/8.0f;
    const int playCol=proc.getGridPlayColumn();

    g.setColour(C(0xffe4d6b8));g.setFont(juce::Font(juce::FontOptions("Arial",juce::jlimit(9.0f,13.0f,ph*.25f),juce::Font::bold)));
    for(int col=0;col<8;++col){auto x=r.getX()+col*(pw+gap);g.drawText(juce::String::formatted("%02d",col+1),(int)x,1,(int)pw,17,juce::Justification::centred);}

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
            if(col==0){
                const auto rowName=mode==JerzyMonoAnalogAudioProcessor::GridMode::sequencer?midiNoteName(proc.getGridRowNote(row)):juce::String("R")+juce::String(row+1);
                g.setColour(C(0xfff2e8d2));g.setFont(juce::Font(juce::FontOptions("Arial",juce::jlimit(8.0f,12.0f,ph*.22f),juce::Font::bold)));
                g.drawText(rowName,0,(int)cell.getY(),22,(int)cell.getHeight(),juce::Justification::centred);
            }
            if(mode==JerzyMonoAnalogAudioProcessor::GridMode::sequencer && proc.getGridStep(bank,col,row)){
                g.setColour(C(0xff1c1a15));g.setFont(juce::Font(juce::FontOptions("Arial",juce::jlimit(8.0f,12.0f,ph*.23f),juce::Font::bold)));
                g.drawText("ON",cell.toNearestInt(),juce::Justification::centred);
            }
            if(mode==JerzyMonoAnalogAudioProcessor::GridMode::launch){
                const int launchNote=proc.getGridRootNote()+(7-row)*8+col;
                g.setColour(C(0xfff7efdb));g.setFont(juce::Font(juce::FontOptions("Arial",juce::jlimit(8.0f,11.0f,ph*.20f),juce::Font::bold)));
                g.drawText(midiNoteName(launchNote),cell.toNearestInt(),juce::Justification::centred);
            }
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

MonoFxPanel::MonoFxPanel(JerzyMonoAnalogAudioProcessor& p):proc(p)
{
    static const char* ids[17]={"fxCompThreshold","fxCompRatio","fxCompAttack","fxCompRelease","fxCompDrive","fxDelayFeedback","fxDelayMix","fxReverbSize","fxReverbDamping","fxReverbMix","fxWidth","fxChorusRate","fxChorusDepth","fxChorusMix","fxChorusFeedback","fxRotaryRate","fxRotaryDepth"};
    static const char* labels[17]={"THRESHOLD","RATIO","ATTACK","RELEASE","DRIVE","FEEDBACK","MIX","ROOM","DAMP","MIX","WIDTH","RATE","DEPTH","MIX","FB","FREE RATE","DEPTH"};
    for(int i=0;i<17;++i)
    {
        auto& s=knobs[(size_t)i];s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,64,18);
        s.setNumDecimalPlacesToDisplay(0);
        s.textFromValueFunction=[i](double v){
            switch(i){case 0:return juce::String(juce::roundToInt(v))+" dB";case 1:return juce::String(juce::roundToInt(v))+":1";case 2:case 3:return v<1.0?juce::String(juce::roundToInt(v*1000.0))+" ms":juce::String(juce::roundToInt(v))+" s";case 4:return juce::String(juce::roundToInt(v*18.0))+" dB";case 11:case 15:return juce::String(juce::roundToInt(v))+" Hz";case 10:return juce::String(juce::roundToInt(v*100.0))+" %";default:return juce::String(juce::roundToInt(v*100.0))+" %";}
        };
        s.setColour(juce::Slider::textBoxTextColourId,C(0xff24231f));s.setColour(juce::Slider::textBoxBackgroundColourId,C(0xffd5cdb8));
        s.setColour(juce::Slider::rotarySliderFillColourId,C(RED));s.setColour(juce::Slider::rotarySliderOutlineColourId,C(0xff34383c));
        addAndMakeVisible(s);knobAttachments[(size_t)i]=std::make_unique<SliderAttachment>(proc.apvts,ids[i],s);
        knobLabels[(size_t)i].setText(labels[i],juce::dontSendNotification);knobLabels[(size_t)i].setJustificationType(juce::Justification::centred);knobLabels[(size_t)i].setColour(juce::Label::textColourId,lcdBg);addAndMakeVisible(knobLabels[(size_t)i]);
    }
    static const char* toggleIds[6]={"fxCompOn","fxDelayOn","fxReverbOn","fxWidthOn","fxChorusOn","fxRotaryOn"};
    static const char* toggleNames[6]={"COMP / LIMIT","DELAY","RVERB STEREO","STEREO WIDER","CHORUS / FLANGER","ROTARY STEREO"};
    const juce::Colour colors[6]={C(RED),C(YELLOW),C(GREEN),C(GREEN),C(YELLOW),C(RED)};
    for(int i=0;i<6;++i){auto& b=enabled[(size_t)i];b.setButtonText(toggleNames[i]);b.setColour(juce::ToggleButton::tickColourId,colors[i]);addAndMakeVisible(b);buttonAttachments[(size_t)i]=std::make_unique<ButtonAttachment>(proc.apvts,toggleIds[i],b);}
    rotarySync.setButtonText("TEMPO SYNC");rotarySync.setColour(juce::ToggleButton::tickColourId,C(GREEN));addAndMakeVisible(rotarySync);
    buttonAttachments[6]=std::make_unique<ButtonAttachment>(proc.apvts,"fxRotarySync",rotarySync);
    static const juce::StringArray choices[4]={
        {"1/1","1/2","1/4","1/8","1/16","1/32","1/4T","1/8T","1/16T","1/4D","1/8D","1/16D"},
        {"MONO","STEREO","PING-PONG"},
        {"JUNO","CHORUS","FLANGER"},
        {"1/1","1/2","1/4","1/8","1/16","1/32","1/4T","1/8T","1/16T","1/4D","1/8D","1/16D"}
    };
    const char* modeIds[4]={"fxDelayDivision","fxDelayMode","fxChorusMode","fxRotaryDivision"};
    for(int i=0;i<4;++i){modes[(size_t)i].addItemList(choices[i],1);addAndMakeVisible(modes[(size_t)i]);modeAttachments[(size_t)i]=std::make_unique<ComboAttachment>(proc.apvts,modeIds[i],modes[(size_t)i]);}
    orderSlot.addItemList({"SLOT 1","SLOT 2","SLOT 3","SLOT 4","SLOT 5","SLOT 6"},1);orderSlot.setSelectedItemIndex(0,juce::dontSendNotification);addAndMakeVisible(orderSlot);
    moveUp.setButtonText("MOVE UP");moveDown.setButtonText("MOVE DOWN");addAndMakeVisible(moveUp);addAndMakeVisible(moveDown);
    chainLabel.setColour(juce::Label::textColourId,lcdBg);chainLabel.setJustificationType(juce::Justification::centredLeft);addAndMakeVisible(chainLabel);
    orderSlot.onChange=[this]{refreshOrder();};
    moveUp.onClick=[this]{proc.moveFx(orderSlot.getSelectedItemIndex(),orderSlot.getSelectedItemIndex()-1);refreshOrder();};
    moveDown.onClick=[this]{proc.moveFx(orderSlot.getSelectedItemIndex(),orderSlot.getSelectedItemIndex()+1);refreshOrder();};
    rotarySync.onClick=[this]{const bool sync=rotarySync.getToggleState();modes[3].setEnabled(sync);knobs[15].setEnabled(!sync);};
    const bool sync=proc.apvts.getRawParameterValue("fxRotarySync")->load()>0.5f;modes[3].setEnabled(sync);knobs[15].setEnabled(!sync);
    refreshOrder();
}

void MonoFxPanel::refreshOrder()
{
    auto order=proc.getFxOrder();juce::String names[6]={"COMP","DELAY","RVERB","WIDER","CHORUS","ROTARY"};
    juce::String line="OUTPUT CHAIN  ";
    for(int i=0;i<6;++i){if(i)line<<"  >  ";line<<names[order[(size_t)i]];}
    chainLabel.setText(line,juce::dontSendNotification);
    moveUp.setEnabled(orderSlot.getSelectedItemIndex()>0);moveDown.setEnabled(orderSlot.getSelectedItemIndex()<5);
    repaint();
}

void MonoFxPanel::paint(juce::Graphics& g)
{
    g.fillAll(C(0xff050607));
    const int cardW=(getWidth()-32)/3,cardH=(getHeight()-72)/2;
    const char* titles[6]={"01  COMPRESSOR / LIMITER + DRIVE","02  TEMPO DELAY","03  STEREO REVERB","04  MID / SIDE WIDTH","05  JUNO CHORUS / FLANGER","06  ROTARY SPEAKER"};
    for(int i=0;i<6;++i)
    {
        const int col=i%3,row=i/3;
        auto r=juce::Rectangle<float>((float)(8+col*(cardW+8)),(float)(56+row*(cardH+8)),(float)cardW,(float)cardH);
        g.setColour(C(PANEL));g.fillRoundedRectangle(r,5.0f);g.setColour(C(EDGE));g.drawRoundedRectangle(r,5.0f,1.2f);
        auto hd=r.removeFromTop(28.0f).reduced(7.0f,2.0f);g.setColour(lcdBg);g.fillRoundedRectangle(hd,2.0f);g.setColour(lcdText);
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),juce::jlimit(8.0f,11.0f,(float)cardW*0.024f),juce::Font::bold)));
        g.drawFittedText(titles[i],hd.toNearestInt().reduced(3,0),juce::Justification::centredLeft,1);
    }
}

void MonoFxPanel::resized()
{
    const auto area=getLocalBounds();
    const int width=area.getWidth(),height=area.getHeight();
    constexpr int margin=8,gap=8,topBar=48;
    const int cardW=(width-2*margin-2*gap)/3;
    const int cardH=(height-topBar-2*margin-gap)/2;
    const int controlH=juce::jmin(112,cardH-64);
    const int knobLabelH=15;
    orderSlot.setBounds(10,9,112,30);moveUp.setBounds(130,9,82,30);moveDown.setBounds(220,9,96,30);
    chainLabel.setBounds(326,9,juce::jmax(0,width-336),30);
    for(int i=0;i<6;++i)
    {
        const int col=i%3,row=i/3;
        const int x=margin+col*(cardW+gap),y=topBar+margin+row*(cardH+gap);
        enabled[(size_t)i].setBounds(x+6,y+31,cardW-12,24);
    }
    auto knob=[&](int index,int x,int y,int w,int h)
    {
        h=juce::jmax(68,h);
        knobLabels[(size_t)index].setBounds(x,y,w,knobLabelH);
        knobs[(size_t)index].setTextBoxStyle(juce::Slider::TextBoxBelow,false,juce::jmax(42,w-4),16);
        knobs[(size_t)index].setBounds(x,y+knobLabelH,w,h-knobLabelH);
    };
    const int controlY=topBar+margin+31+24+4;
    const int secondY=controlY+29;
    const int row2ControlY=controlY+cardH+gap;
    const int row2SecondY=row2ControlY+29;
    const int usableAfterCombo=juce::jmax(68,controlH-29);
    const int x0=margin,x1=margin+cardW+gap,x2=margin+2*(cardW+gap);
    const int fiveW=(cardW-18)/5;
    for(int k=0;k<5;++k)knob(k,x0+4+k*(fiveW+2),controlY,fiveW,controlH);
    const int comboW=(cardW-30)/2;
    modes[0].setBounds(x1+6,controlY,comboW,25);modes[1].setBounds(x1+18+comboW,controlY,comboW,25);
    const int pairW=(cardW-32)/2;
    knob(5,x1+8,secondY,pairW,usableAfterCombo);knob(6,x1+24+pairW,secondY,pairW,usableAfterCombo);
    const int triW=(cardW-30)/3;
    for(int k=0;k<3;++k)knob(7+k,x2+6+k*(triW+6),controlY,triW,controlH);
    knob(10,x0+(cardW-116)/2,row2ControlY,116,controlH);
    modes[2].setBounds(x1+6,row2ControlY,(cardW-18),25);
    const int fourW=(cardW-28)/4;
    for(int k=0;k<4;++k)knob(11+k,x1+4+k*(fourW+4),row2SecondY,fourW,usableAfterCombo);
    const int rotaryComboW=(cardW-30)/2;
    rotarySync.setBounds(x2+6,row2ControlY,rotaryComboW,25);
    modes[3].setBounds(x2+18+rotaryComboW,row2ControlY,rotaryComboW,25);
    knob(15,x2+8,row2SecondY,pairW,usableAfterCombo);knob(16,x2+24+pairW,row2SecondY,pairW,usableAfterCombo);
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
:AudioProcessorEditor(&p),proc(p),padGrid(p),fxPanel(p)
{
    setLookAndFeel(&look);setOpaque(true);setResizable(true,true);setResizeLimits(1000,500,1920,1200);getConstrainer()->setFixedAspectRatio(2.0);setSize(1200,600);

    title.setText("JERZY MONO ANALOG",juce::dontSendNotification);title.setColour(juce::Label::textColourId,lcdText);title.setColour(juce::Label::backgroundColourId,lcdBg);title.setJustificationType(juce::Justification::centredLeft);
    subtitle.setText("ANALOG MODELING SYNTH",juce::dontSendNotification);subtitle.setColour(juce::Label::textColourId,lcdText);subtitle.setColour(juce::Label::backgroundColourId,lcdBg);subtitle.setJustificationType(juce::Justification::centred);
    preset.setText("VECTOR LCD GUI",juce::dontSendNotification);preset.setColour(juce::Label::textColourId,lcdText);preset.setColour(juce::Label::backgroundColourId,lcdBg);preset.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(title);addAndMakeVisible(subtitle);addAndMakeVisible(preset);

    const juce::String pageNames[6]={"OSC / MIX","FILTER / ENV","MOD","GRID","ARP","FX"};
    for(int i=0;i<6;++i){auto& b=pageButtons[(size_t)i];b.setButtonText(pageNames[i]);b.onClick=[this,i]{setMainPage(i);};addAndMakeVisible(b);}

    setupToggle(gridSeqOn,"SEQ PLAY",C(GREEN));
    setupToggle(gridMidiTrigger,"MIDI TRIG",C(YELLOW));
    setupToggle(gridHostSync,"HOST SYNC",C(GREEN));
    setupCombo(gridDivision,{"1/1","1/2","1/4","1/8","1/16","1/32","1/4T","1/8T","1/16T","1/4D","1/8D","1/16D"});
    setupCombo(gridRoot,{"C1","C#1","D1","D#1","E1","F1","F#1","G1","G#1","A1","A#1","B1","C2","C#2","D2","D#2","E2","F2","F#2","G2","G#2","A2","A#2","B2","C3","C#3","D3","D#3","E3","F3","F#3","G3","G#3","A3","A#3","B3","C4","C#4","D4","D#4","E4","F4","F#4","G4","G#4","A4","A#4","B4","C5","C#5","D5","D#5","E5","F5","F#5","G5","G#5","A5","A#5","B5","C6"});
    setupCombo(gridScale,{"CHROMATIC","MAJOR","NAT MINOR","DORIAN","PHRYGIAN","MIXOLYDIAN","MAJOR PENT","MINOR PENT"});
    setupCombo(gridBanks,{"1 BANK / 8 STEPS","2 BANKS / 16 STEPS","3 BANKS / 24 STEPS","4 BANKS / 32 STEPS","5 BANKS / 40 STEPS","6 BANKS / 48 STEPS","7 BANKS / 56 STEPS","8 BANKS / 64 STEPS"});
    setupKnob(gridGate,"GATE","%",0.75);
    setupCombo(gridDirection,{"FORWARD","REVERSE","PING-PONG","RANDOM"});
    setupCombo(gridOctave,{"-2 OCT","-1 OCT","0 OCT","+1 OCT","+2 OCT"});
    setupKnob(gridSwing,"SWING","%",0.0);setupKnob(gridVelocity,"VELOCITY","%",0.95);
    setupKnob(gridProbability,"PROBABILITY","%",1.0);setupCombo(gridRatchet,{"1","2","3","4"});
    setupKnob(arpSwing,"SWING","%",0.0);setupKnob(arpVelocity,"VELOCITY","%",0.9);
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
    addAndMakeVisible(fxPanel);fxPanel.setVisible(false);

    setupCombo(osc1Wave,{"SINE","TRIANGLE","SAW","SQUARE"});setupCombo(osc1Oct,{"16'","8'","4'","2'","1'"});
    setupCombo(osc2Wave,{"SINE","TRIANGLE","SAW","SQUARE"});setupCombo(osc2Oct,{"16'","8'","4'","2'","1'"});
    setupCombo(filterMode,{"LADDER 24 dB","LP 12 dB","LP 24 dB","HP 12 dB","HP 24 dB","BP 12 dB","BP 24 dB"});
    setupCombo(subWave,{"SINE","SQUARE"});
    setupCombo(lfoWave,{"SINE","TRIANGLE","SAW","SQUARE","S&H"});
    setupCombo(lfoDivision,{"1/1","1/2","1/4","1/8","1/16","1/32","1/4T","1/8T","1/16T","1/4D","1/8D","1/16D"});
    setupCombo(glideMode,{"ALWAYS","LEGATO"});setupCombo(priority,{"LAST","LOW","HIGH"});
    setupCombo(arpDivision,{"1/1","1/2","1/4","1/8","1/16","1/32","1/4T","1/8T","1/16T","1/4D","1/8D","1/16D"});
    setupCombo(arpPattern,{"UP","DOWN","UP-DOWN","RANDOM","AS PLAYED","UP x2","DOWN x2","UP-DOWN INCL.","CONVERGE"});
    setupCombo(arpRhythm,{"STRAIGHT","EVERY 2","3-3-2","SYNCOPATED","CLAVE","5 OVER 8","ROLLING"});
    setupCombo(arpOctaves,{"1 OCT","2 OCT","3 OCT","4 OCT"});

    setupToggle(legato,"LEGATO",C(GREEN));setupToggle(retrigger,"RETRIGGER",C(RED));setupToggle(lfoSync,"HOST SYNC",C(YELLOW));
    setupToggle(arpOn,"ARP ON",C(GREEN));setupToggle(arpLatch,"LATCH",C(YELLOW));setupToggle(arpRetrigger,"RETRIGGER",C(RED));setupToggle(arpHostSync,"HOST SYNC",C(GREEN));
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
    gridHostSyncA=std::make_unique<ButtonAttachment>(s,"gridHostSync",gridHostSync);
    arpHostSyncA=std::make_unique<ButtonAttachment>(s,"arpHostSync",arpHostSync);
    gridDivisionA=std::make_unique<ComboAttachment>(s,"gridDivision",gridDivision);
    gridRootA=std::make_unique<ComboAttachment>(s,"gridRoot",gridRoot);
    gridScaleA=std::make_unique<ComboAttachment>(s,"gridScale",gridScale);
    gridBanksA=std::make_unique<ComboAttachment>(s,"gridBanks",gridBanks);
    gridDirectionA=std::make_unique<ComboAttachment>(s,"gridDirection",gridDirection);
    gridOctaveA=std::make_unique<ComboAttachment>(s,"gridOctave",gridOctave);
    gridGateA=std::make_unique<SliderAttachment>(s,"gridGate",gridGate);
    gridSwingA=std::make_unique<SliderAttachment>(s,"gridSwing",gridSwing);
    gridVelocityA=std::make_unique<SliderAttachment>(s,"gridVelocity",gridVelocity);
    gridProbabilityA=std::make_unique<SliderAttachment>(s,"gridProbability",gridProbability);
    gridRatchetA=std::make_unique<ComboAttachment>(s,"gridRatchet",gridRatchet);
    arpSwingA=std::make_unique<SliderAttachment>(s,"arpSwing",arpSwing);
    arpVelocityA=std::make_unique<SliderAttachment>(s,"arpVelocity",arpVelocity);

    setupKnob(osc1Level,"LEVEL","%",0.0);setupKnob(pulseWidth,"PULSE WIDTH","%",0.5);
    setupKnob(osc2Level,"LEVEL","%",0.0);setupKnob(detune,"DETUNE","ct",0.0);
    setupKnob(subLevel,"SUB LEVEL","%",0.0);setupKnob(noiseLevel,"NOISE","%",0.0);
    setupKnob(mixDrive,"MIX DRIVE","dB",0.0);setupKnob(drift,"DRIFT","ct",0.0);
    setupKnob(cutoff,"CUTOFF","Hz",20.0);setupKnob(resonance,"RESONANCE","%",0.0);setupKnob(filterDrive,"FILTER DRIVE","dB",0.0);setupKnob(filterEnv,"ENV AMOUNT","oct",0.0);setupKnob(keyTrack,"KEY TRACK","%",0.0);
    setupEnvSlider(aA,"ATTACK","s",0.0);setupEnvSlider(aD,"DECAY","s",0.0);setupEnvSlider(aS,"SUSTAIN","",0.0);setupEnvSlider(aR,"RELEASE","s",0.0);
    setupEnvSlider(fA,"ATTACK","s",0.0);setupEnvSlider(fD,"DECAY","s",0.0);setupEnvSlider(fS,"SUSTAIN","",0.0);setupEnvSlider(fR,"RELEASE","s",0.0);
    setupKnob(lfoRate,"RATE","Hz",0.03);setupKnob(lfoPitch,"PITCH","ct",0.0);setupKnob(lfoFilter,"FILTER","oct",0.0);setupKnob(lfoPWM,"PWM","%",0.0);setupKnob(lfoAmp,"AMP","%",0.0);setupKnob(lfoFade,"FADE IN","s",0.0);
    setupKnob(glide,"GLIDE","s",0.0);setupKnob(outDrive,"OUTPUT DRIVE","dB",0.0);setupKnob(master,"MASTER","%",0.8);
    setupKnob(modEnvPitch,"MOD ENV PITCH","st",0.0);setupKnob(modEnvPWM,"MOD ENV PWM","",0.0);
    setupKnob(arpGate,"GATE","%",0.72);

    modEnvPitch.setSliderStyle(juce::Slider::LinearHorizontal);modEnvPitch.setTextBoxStyle(juce::Slider::TextBoxRight,false,65,22);
    modEnvPWM.setSliderStyle(juce::Slider::LinearHorizontal);modEnvPWM.setTextBoxStyle(juce::Slider::TextBoxRight,false,65,22);
    modEnvPWM.textFromValueFunction=[](double v){return juce::String(v*100.0,0)+" %";};
    modEnvPWM.valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue()*0.01;};modEnvPWM.updateText();
    setMainPage(0);
    startTimerHz(20);
}

JerzyMonoAnalogAudioProcessorEditor::~JerzyMonoAnalogAudioProcessorEditor(){stopTimer();setLookAndFeel(nullptr);}

void JerzyMonoAnalogAudioProcessorEditor::setupKnob(ResetSlider& k,const juce::String& name,const juce::String& unit,double neutral)
{
    k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,88,21);
    k.setNumDecimalPlacesToDisplay(0);
    k.textFromValueFunction=[unit,name](double v){
        if(unit=="%")v*=100.0;
        else if(unit=="dB")v*=name=="FILTER DRIVE"?20.0:18.0;
        else if(unit=="s"&&v<1.0)return juce::String(juce::roundToInt(v*1000.0))+" ms";
        return juce::String(juce::roundToInt(v))+(unit.isEmpty()?"":" "+unit);
    };
    k.valueFromTextFunction=[unit,name](const juce::String& text){double v=text.getDoubleValue();if(unit=="%")return v*0.01;if(unit=="dB")return v/(name=="FILTER DRIVE"?20.0:18.0);if(unit=="s"&&text.containsIgnoreCase("ms"))return v*.001;return v;};
    k.setNeutralValue(neutral);k.setTooltip(name);k.updateText();addAndMakeVisible(k);
}
void JerzyMonoAnalogAudioProcessorEditor::setupEnvSlider(ResetSlider& k,const juce::String& name,const juce::String& unit,double neutral)
{
    k.setSliderStyle(juce::Slider::LinearVertical);
    k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,54,22);
    k.setNumDecimalPlacesToDisplay(0);
    k.textFromValueFunction=[unit](double v){
        if(unit == "s") return v < 1.0 ? juce::String(juce::roundToInt(v * 1000.0)) + " ms" : juce::String(juce::roundToInt(v)) + " s";
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
    g.fillAll(C(0xff171817));const float sc=scale();
    juce::ColourGradient face(C(0xff55534c),0,0,C(0xff202221),0,(float)getHeight(),false);g.setGradientFill(face);g.fillAll();
    g.setColour(C(0xff101211));g.fillRect(0,0,getWidth(),juce::roundToInt(69*sc));
    g.setColour(C(0xff9b845b));g.drawLine(0,69*sc,(float)getWidth(),69*sc,1.5f*sc);
    for(const auto&s:sections)drawSection(g,s);
    auto label=[&](const char*t,float x,float y,float w){drawLabelBox(g,t,x,y,w);};
    if(modulePage==0){
        label("WAVE",30,112,130);label("OCTAVE",175,112,130);label("LEVEL",40,145,110);label("PULSE WIDTH",175,145,130);
        label("WAVE",330,112,130);label("OCTAVE",475,112,130);label("LEVEL",340,145,110);label("DETUNE",475,145,130);
        label("SUB / NOISE",610,112,190);label("MIX BUS DRIVE",850,112,220);label("ANALOG DRIFT",1085,112,180);label("SUB LEVEL",625,145,90);label("NOISE",725,145,90);
        label("GLIDE",40,416,110);label("MODE",190,416,130);label("NOTE PRIORITY",350,416,150);label("LEGATO",520,416,120);label("RETRIGGER",660,416,130);
        label("OUTPUT DRIVE",875,416,210);label("MASTER LEVEL",1110,416,230);
    }else if(modulePage==1){
        label("FILTER MODEL",35,112,260);label("CUTOFF",35,158,110);label("RESONANCE",155,158,110);label("FILTER DRIVE",275,158,110);label("ENV DEPTH",395,158,110);label("KEY TRACK",515,158,110);
        label("ENV → PITCH",760,130,250);label("ENV → PWM",1070,130,250);
        label("AMP ENVELOPE  •  ATTACK / DECAY / SUSTAIN / RELEASE",30,365,650);label("FILTER ENVELOPE  •  ATTACK / DECAY / SUSTAIN / RELEASE",740,365,660);
        label("A",35,420,80);label("D",190,420,80);label("S",345,420,80);label("R",500,420,80);
        label("A",755,420,80);label("D",910,420,80);label("S",1065,420,80);label("R",1220,420,80);
    }else if(modulePage==2){
        label("LFO WAVE",35,115,190);label("CLOCK DIVISION",245,115,200);label("TEMPO SYNC",465,115,170);
        label("RATE",45,170,170);label("PITCH",260,170,170);label("FILTER",475,170,170);label("PWM",690,170,170);label("AMP",905,170,170);label("FADE IN",1120,170,170);
        label("ENVELOPE ROUTING",35,395,280);label("PITCH",50,440,110);label("PWM",350,440,110);
        label("MODULATION DEPTHS ARE BIPOLAR WHERE SHOWN  •  AUTOMATABLE FROM FL STUDIO",650,470,700);
    }else if(modulePage==3){
        label("MODE",30,108,135);label("BANK",175,108,110);label("PATTERN LENGTH",295,108,190);label("CLOCK DIVISION",495,108,180);label("SCALE",685,108,190);label("ROOT NOTE",885,108,150);label("DIRECTION",1045,108,175);label("OCTAVE",1230,108,165);
        label("GATE",35,173,100);label("PLAY",155,173,130);label("MIDI TRIGGER",300,173,145);label("HOST SYNC",460,173,140);label("CLEAR",615,173,120);label("VELOCITY",750,173,110);label("SWING",875,173,110);label("GLOBAL PROB.",1000,173,145);label("GLOBAL RATC.",1160,173,130);
        auto r=juce::Rectangle<float>(545*sc,250*sc,870*sc,425*sc);g.setColour(C(0xff242624));g.fillRoundedRectangle(r,7*sc);g.setColour(C(0xff9b845b));g.drawRoundedRectangle(r,7*sc,1.2f*sc);
        g.setColour(C(0xffe9dcbf));g.setFont(juce::Font(juce::FontOptions("Arial",17*sc,juce::Font::bold)));g.drawText("GRID PERFORMANCE",r.toNearestInt().reduced(22*sc,16*sc).withHeight(30*sc),juce::Justification::centredLeft);
        g.setColour(C(0xffe2b45a));g.setFont(juce::Font(juce::FontOptions("Arial",13*sc,juce::Font::bold)));
        g.drawText("64 STEP CELLS  •  UP TO 8 PATTERN BANKS",565*sc,320*sc,810*sc,26*sc,juce::Justification::centredLeft);
        g.drawText("DIRECTION: FORWARD / REVERSE / PING-PONG / RANDOM",565*sc,366*sc,810*sc,26*sc,juce::Justification::centredLeft);
        g.drawText("MIDI TRIGGER TRANSPOSES THE PATTERN TO THE INCOMING NOTE",565*sc,412*sc,810*sc,26*sc,juce::Justification::centredLeft);
        g.drawText("HOST SYNC LOCKS STEPS TO FL STUDIO TEMPO AND TRANSPORT",565*sc,458*sc,810*sc,26*sc,juce::Justification::centredLeft);
        g.drawText("PROBABILITY ADDS VARIATION; RATCHET CREATES REPEATS",565*sc,504*sc,810*sc,26*sc,juce::Justification::centredLeft);
        g.drawText("OCTAVE RANGE: −2 TO +2 OCTAVES  •  SCALE QUANTIZATION",565*sc,550*sc,810*sc,26*sc,juce::Justification::centredLeft);
        g.setColour(C(0xffaeb69a));g.setFont(juce::Font(juce::FontOptions("Arial",12*sc,juce::Font::plain)));
        g.drawText("The first active row in a column plays. Notes are sent to host MIDI output.",565*sc,604*sc,810*sc,40*sc,juce::Justification::centredLeft);
    }else if(modulePage==4){
        label("RUN",35,115,130);label("HOST SYNC",180,115,150);label("LATCH",345,115,130);label("RETRIGGER",490,115,145);
        label("RATE",40,205,180);label("NOTE ORDER",245,205,210);label("RHYTHMIC PATTERN",480,205,220);label("OCTAVE RANGE",725,205,190);
        label("GATE",940,205,135);label("VELOCITY",1090,205,135);label("SWING",1240,205,135);
        g.setColour(C(0xffe6b755));g.setFont(juce::Font(juce::FontOptions("Arial",14*sc,juce::Font::bold)));
        g.drawText("UP • DOWN • PING-PONG • RANDOM WITHOUT REPEATS • AS PLAYED • DOUBLE NOTES • CONVERGE",35*sc,455*sc,1360*sc,28*sc,juce::Justification::centredLeft);
        g.setColour(C(0xffe9e1d0));g.setFont(juce::Font(juce::FontOptions("Arial",12*sc,juce::Font::plain)));
        g.drawText("Clock follows FL Studio transport. Generated notes are sent back as MIDI and can be recorded into the Piano Roll.",35*sc,490*sc,1360*sc,26*sc,juce::Justification::centredLeft);
    }
}

void JerzyMonoAnalogAudioProcessorEditor::resized()
{
    const float sc=scale();
    title.setFont(juce::Font(juce::FontOptions("Arial",20*sc,juce::Font::bold)));
    subtitle.setFont(juce::Font(juce::FontOptions("Arial",10*sc,juce::Font::bold)));
    preset.setFont(juce::Font(juce::FontOptions("Arial",10*sc,juce::Font::bold)));
    place(title,18,8,275,44);place(subtitle,22,42,280,18);place(preset,1210,19,205,28);
    for(int i=0;i<6;++i)place(pageButtons[(size_t)i],320+i*145,15,136,38);
    if(fxPage){place(fxPanel,15,80,1410,615);return;}
    if(modulePage==0){
        place(osc1Wave,30,124,130,30);place(osc1Oct,175,124,130,30);place(osc1Level,35,160,120,126);place(pulseWidth,180,160,120,126);
        place(osc2Wave,330,124,130,30);place(osc2Oct,475,124,130,30);place(osc2Level,335,160,120,126);place(detune,480,160,120,126);
        place(subWave,615,124,185,30);place(subLevel,625,160,90,126);place(noiseLevel,725,160,90,126);
        place(mixDrive,860,160,115,126);place(drift,1000,160,115,126);
        place(outDrive,1175,160,115,126);place(master,1300,160,115,126);
        place(glide,35,465,110,130);place(glideMode,185,470,135,35);place(priority,345,470,150,35);place(legato,525,470,125,35);place(retrigger,675,470,135,35);place(outputMeter,900,490,450,36);
    }else if(modulePage==1){
        place(filterMode,35,124,400,32);place(cutoff,35,176,115,120);place(resonance,165,176,115,120);place(filterDrive,295,176,115,120);place(filterEnv,425,176,115,120);place(keyTrack,555,176,115,120);
        place(modEnvPitch,760,145,260,34);place(modEnvPWM,1070,145,260,34);
        place(aA,35,450,100,155);place(aD,190,450,100,155);place(aS,345,450,100,155);place(aR,500,450,100,155);
        place(fA,755,450,100,155);place(fD,910,450,100,155);place(fS,1065,450,100,155);place(fR,1220,450,100,155);
    }else if(modulePage==2){
        place(lfoWave,35,130,190,35);place(lfoDivision,245,130,200,35);place(lfoSync,465,130,170,35);
        place(lfoRate,35,190,175,125);place(lfoPitch,255,190,175,125);place(lfoFilter,475,190,175,125);place(lfoPWM,695,190,175,125);place(lfoAmp,915,190,175,125);place(lfoFade,1135,190,175,125);
        place(modEnvPitch,75,460,240,42);place(modEnvPWM,390,460,240,42);
    }else if(modulePage==3){
        place(gridModeButton,30,124,135,32);place(gridBankBox,175,124,110,32);place(gridBanks,295,124,190,32);place(gridDivision,495,124,180,32);place(gridScale,685,124,190,32);place(gridRoot,885,124,150,32);place(gridDirection,1045,124,175,32);place(gridOctave,1230,124,165,32);
        place(gridGate,35,184,100,48);place(gridSeqOn,155,188,130,30);place(gridMidiTrigger,300,188,145,30);place(gridHostSync,460,188,140,30);place(gridClearButton,615,188,120,30);
        place(gridVelocity,750,175,110,70);place(gridSwing,875,175,110,70);place(gridProbability,1000,175,145,70);place(gridRatchet,1160,188,110,30);
        place(padGrid,30,250,500,430);
    }else if(modulePage==4){
        place(arpOn,35,130,130,36);place(arpHostSync,180,130,150,36);place(arpLatch,345,130,130,36);place(arpRetrigger,490,130,145,36);
        place(arpDivision,35,225,180,38);place(arpPattern,245,225,210,38);place(arpRhythm,480,225,220,38);place(arpOctaves,725,225,190,38);
        place(arpGate,930,225,140,125);place(arpVelocity,1085,225,140,125);place(arpSwing,1240,225,140,125);
    }
}

void JerzyMonoAnalogAudioProcessorEditor::setMainPage(int page)
{
    modulePage=juce::jlimit(0,5,page);padsPage=modulePage==3;fxPage=modulePage==5;sections.clear();
    const juce::Colour amber=C(0xffe2b45a),green=C(0xff81b584),steel=C(0xff8fa4aa);
    if(modulePage==0){addSection("OSCILLATOR 1",green,15,78,300,290);addSection("OSCILLATOR 2",amber,325,78,300,290);addSection("SUB / NOISE",steel,635,78,205,290);addSection("MIXER / ANALOG",amber,850,78,275,290);addSection("OUTPUT STAGE",green,1135,78,290,290);addSection("PLAY MODE / LEVEL",amber,15,380,1410,295);}
    if(modulePage==1){addSection("FILTER / DRIVE",green,15,78,710,245);addSection("ENVELOPE MODULATION",amber,745,78,680,245);addSection("AMPLIFIER ENVELOPE",green,15,335,690,340);addSection("FILTER ENVELOPE",amber,725,335,700,340);}
    if(modulePage==2){addSection("LOW FREQUENCY OSCILLATOR",green,15,78,1410,285);addSection("MODULATION ROUTING",amber,15,380,1410,295);}
    if(modulePage==3){addSection("PERFORMANCE GRID / SEQUENCER",green,15,78,1410,150);}
    if(modulePage==4){addSection("CLOCKED ARPEGGIATOR",green,15,78,1410,600);}
    for(int i=0;i<6;++i){auto& b=pageButtons[(size_t)i];b.setColour(juce::TextButton::buttonColourId,i==modulePage?C(0xff9b7134):C(0xff292b29));b.setColour(juce::TextButton::textColourOffId,C(0xfff0e6d0));b.setColour(juce::TextButton::textColourOnId,C(0xffffffff));}
    std::initializer_list<juce::Component*> all={
        &osc1Wave,&osc1Oct,&osc2Wave,&osc2Oct,&subWave,&filterMode,&lfoWave,&lfoDivision,&glideMode,&priority,&arpDivision,&arpPattern,&arpRhythm,&arpOctaves,&gridDivision,&gridRoot,&gridScale,&gridBanks,&gridDirection,&gridOctave,&gridRatchet,
        &osc1Level,&pulseWidth,&osc2Level,&detune,&subLevel,&noiseLevel,&mixDrive,&drift,&cutoff,&resonance,&filterDrive,&filterEnv,&keyTrack,&modEnvPitch,&modEnvPWM,
        &aA,&aD,&aS,&aR,&fA,&fD,&fS,&fR,&lfoRate,&lfoPitch,&lfoFilter,&lfoPWM,&lfoAmp,&lfoFade,&glide,&outDrive,&master,&arpGate,&arpSwing,&arpVelocity,&gridGate,&gridSwing,&gridVelocity,&gridProbability,
        &legato,&retrigger,&lfoSync,&arpOn,&arpLatch,&arpRetrigger,&arpHostSync,&gridSeqOn,&gridMidiTrigger,&gridHostSync,&gridModeButton,&gridClearButton,&gridBankBox,&padGrid,&outputMeter
    };
    for(auto* c:all)c->setVisible(false);
    fxPanel.setVisible(fxPage);
    auto show=[&](std::initializer_list<juce::Component*> list){for(auto* c:list)c->setVisible(true);};
    if(modulePage==0)show({&osc1Wave,&osc1Oct,&osc2Wave,&osc2Oct,&subWave,&osc1Level,&pulseWidth,&osc2Level,&detune,&subLevel,&noiseLevel,&mixDrive,&drift,&glide,&glideMode,&priority,&legato,&retrigger,&outDrive,&master,&outputMeter});
    if(modulePage==1)show({&filterMode,&cutoff,&resonance,&filterDrive,&filterEnv,&keyTrack,&modEnvPitch,&modEnvPWM,&aA,&aD,&aS,&aR,&fA,&fD,&fS,&fR});
    if(modulePage==2)show({&lfoWave,&lfoDivision,&lfoSync,&lfoRate,&lfoPitch,&lfoFilter,&lfoPWM,&lfoAmp,&lfoFade,&modEnvPitch,&modEnvPWM});
    if(modulePage==3)show({&gridSeqOn,&gridMidiTrigger,&gridHostSync,&gridModeButton,&gridClearButton,&gridBankBox,&gridBanks,&gridDivision,&gridScale,&gridRoot,&gridDirection,&gridOctave,&gridRatchet,&gridGate,&gridSwing,&gridVelocity,&gridProbability,&padGrid});
    if(modulePage==4)show({&arpOn,&arpDivision,&arpPattern,&arpRhythm,&arpOctaves,&arpGate,&arpVelocity,&arpSwing,&arpHostSync,&arpLatch,&arpRetrigger});
    if(modulePage==0){for(auto* c:{(juce::Component*)&glideMode,(juce::Component*)&priority})c->setVisible(true);}
    const int w=getWidth();getConstrainer()->setFixedAspectRatio(2.0);setSize(w,juce::roundToInt(720.0f*(w/1440.0f)));resized();repaint();
}

void JerzyMonoAnalogAudioProcessorEditor::updateGridControls()
{
    const bool launch=proc.getGridMode()==JerzyMonoAnalogAudioProcessor::GridMode::launch;
    gridModeButton.setButtonText(launch?"MODE: LAUNCH":"MODE: SEQ");
    gridSeqOn.setEnabled(!launch);
    gridMidiTrigger.setEnabled(!launch);gridHostSync.setEnabled(!launch);
    gridDivision.setEnabled(!launch);
    gridBanks.setEnabled(!launch);
    gridDirection.setEnabled(!launch);
    gridSwing.setEnabled(!launch && !gridHostSync.getToggleState());gridVelocity.setEnabled(!launch);
    gridProbability.setEnabled(!launch);gridRatchet.setEnabled(!launch);
    gridScale.setEnabled(!launch);
    gridRoot.setEnabled(true);
    gridGate.setEnabled(!launch);
    gridBankBox.setEnabled(!launch);
    gridClearButton.setEnabled(!launch);
    padGrid.repaint();
}

void JerzyMonoAnalogAudioProcessorEditor::timerCallback()
{
    outputMeter.setLevel(proc.getOutputMeter());
    if(!padsPage) repaint();
    const bool sync=lfoSync.getToggleState();lfoRate.setEnabled(!sync);lfoDivision.setEnabled(sync);
    arpSwing.setEnabled(!arpHostSync.getToggleState());
    gridSwing.setEnabled(proc.getGridMode()!=JerzyMonoAnalogAudioProcessor::GridMode::launch && !gridHostSync.getToggleState());
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
