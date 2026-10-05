#include "PluginEditor.h"

namespace {
const juce::Colour background(0xff10141b),panel(0xff1c2430),accent(0xff5edbc1),text(0xffe6ecf4),muted(0xff96a9bc);
}
class BassAmbientEditor::Theme final : public juce::LookAndFeel_V4 {
public:
    Theme() {
        setColour(juce::Slider::textBoxTextColourId,text);
        setColour(juce::Slider::textBoxBackgroundColourId,background);
        setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        setColour(juce::ComboBox::backgroundColourId,background);
        setColour(juce::ComboBox::textColourId,text);
        setColour(juce::ComboBox::outlineColourId,muted.withAlpha(.4f));
        setColour(juce::PopupMenu::backgroundColourId,panel);
        setColour(juce::PopupMenu::textColourId,text);
        setColour(juce::TextButton::buttonColourId,panel);
        setColour(juce::TextButton::buttonOnColourId,accent.withAlpha(.25f));
        setColour(juce::TextButton::textColourOffId,text);
        setColour(juce::TextButton::textColourOnId,accent);
        setColour(juce::ToggleButton::textColourId,text);
        setColour(juce::ToggleButton::tickColourId,accent);
        setColour(juce::Label::textColourId,text);
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float value,float start,float end,juce::Slider&) override {
        auto bounds=juce::Rectangle<float>(float(x),float(y),float(w),float(h)).reduced(8);
        float size=std::min(bounds.getWidth(),bounds.getHeight()),radius=size*.5f;
        auto centre=bounds.getCentre();auto dial=juce::Rectangle<float>(size,size).withCentre(centre);
        g.setColour(background);g.fillEllipse(dial.reduced(4));
        juce::Path track;track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
        g.setColour(muted.withAlpha(.25f));g.strokePath(track,juce::PathStrokeType(3));
        juce::Path active;float angle=start+value*(end-start);
        active.addCentredArc(centre.x,centre.y,radius,radius,0,start,angle,true);
        g.setColour(accent);g.strokePath(active,juce::PathStrokeType(3));
        juce::Path pointer;pointer.startNewSubPath(0,-radius*.35f);pointer.lineTo(0,-radius*.72f);
        g.strokePath(pointer,juce::PathStrokeType(3),juce::AffineTransform::rotation(angle).translated(centre.x,centre.y));
    }
};

class BassAmbientEditor::Control final : public juce::Component {
    ParameterSpec spec;
    juce::Label label;
    juce::Slider slider;
    juce::ComboBox choice;
    juce::ToggleButton toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> choiceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleAttachment;
public:
    Control(BassAmbientProcessor& p,const ParameterSpec& s):spec(s) {
        label.setText(s.label,juce::dontSendNotification);label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(juce::FontOptions(13)));addAndMakeVisible(label);
        if(s.kind==1) {
            toggle.setButtonText("ON");addAndMakeVisible(toggle);
            toggleAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,s.id,toggle);
        } else if(s.kind==3) {
            choice.addItemList(s.choices,1);addAndMakeVisible(choice);
            choiceAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,s.id,choice);
        } else {
            slider.setSliderStyle(s.kind==2?juce::Slider::LinearHorizontal:juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,22);
            slider.setNumDecimalPlacesToDisplay(s.kind==2?0:3);
            slider.setTooltip(s.label+" — automatable in FL Studio");
            addAndMakeVisible(slider);
            sliderAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,s.id,slider);
            slider.setDoubleClickReturnValue(true,s.initial);
        }
    }
    void paint(juce::Graphics& g) override {
        g.setColour(panel);g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(3),8);
    }
    void resized() override {
        auto bounds=getLocalBounds().reduced(10);label.setBounds(bounds.removeFromTop(30));
        if(spec.kind==1) toggle.setBounds(bounds.withSizeKeepingCentre(80,32));
        else if(spec.kind==3) choice.setBounds(bounds.withSizeKeepingCentre(bounds.getWidth(),32));
        else slider.setBounds(bounds);
    }
};

class BassAmbientEditor::Page final : public juce::Component {
    BassAmbientProcessor& processor;int group;
    std::vector<std::unique_ptr<Control>> controls;
    std::array<juce::Label,6> fxLabels;
    std::array<juce::TextButton,6> left,right;
    juce::Label description;
    void swapSlots(int a,int b) {
        if(b<0||b>=6) return;
        auto* pa=processor.state.getParameter("order"+juce::String(a));
        auto* pb=processor.state.getParameter("order"+juce::String(b));
        float va=pa->getValue(),vb=pb->getValue();
        pa->beginChangeGesture();pb->beginChangeGesture();pa->setValueNotifyingHost(vb);pb->setValueNotifyingHost(va);pa->endChangeGesture();pb->endChangeGesture();
        updateOrder();
    }
public:
    Page(BassAmbientProcessor& p,int index):processor(p),group(index) {
        static constexpr const char* descriptions[]{
            "Analog-inspired bass voices. 303 accent / slide, tuned 808, dual-oscillator classic bass.",
            "A held Piano Roll note supplies the root. DIRECT bypasses the phrase generator. Scale belongs to this instrument.",
            "Two engines per voice. HOLD sustains, FLOW overlaps chords, KRELL creates evolving textures.",
            "Output FX for both layers. Move modules with arrows. Bass and pad have independent FX amounts.",
            "Layer FX amounts and final output safety. Soft limiter is a saturating peak guard, not a look-ahead limiter."};
        description.setText(descriptions[index],juce::dontSendNotification);description.setColour(juce::Label::textColourId,muted);addAndMakeVisible(description);
        for(const auto& s:parameterSpecs()) if(s.group==index&&!s.id.startsWith("order")) {
            auto control=std::make_unique<Control>(processor,s);addAndMakeVisible(*control);controls.push_back(std::move(control));
        }
        if(group==3) for(int k=0;k<6;++k) {
            addAndMakeVisible(fxLabels[size_t(k)]);fxLabels[size_t(k)].setJustificationType(juce::Justification::centred);
            left[size_t(k)].setButtonText("<");right[size_t(k)].setButtonText(">");
            addAndMakeVisible(left[size_t(k)]);addAndMakeVisible(right[size_t(k)]);
            left[size_t(k)].onClick=[this,k]{swapSlots(k,k-1);};right[size_t(k)].onClick=[this,k]{swapSlots(k,k+1);};
            left[size_t(k)].setEnabled(k>0);right[size_t(k)].setEnabled(k<5);
        }
        updateOrder();
    }
    void updateOrder() {
        if(group!=3) return;
        const char* names[]{"DRIVE / COMP","CHORUS","DELAY","GRANULAR","REVERB","WIDTH"};
        auto p=processor.readParameters();
        for(int k=0;k<6;++k) fxLabels[size_t(k)].setText(juce::String(k+1)+"  "+names[juce::jlimit(0,5,p.order[size_t(k)])],juce::dontSendNotification);
    }
    int requiredHeight(int width) const {
        int columns=std::max(3,width/155);
        return 50+(group==3?80:0)+int((controls.size()+size_t(columns)-1)/size_t(columns))*148;
    }
    void resized() override {
        description.setBounds(10,0,getWidth()-20,44);
        int top=50;
        if(group==3) {
            int w=getWidth()/6;
            for(int k=0;k<6;++k) {
                int x=k*w;fxLabels[size_t(k)].setBounds(x+2,top,w-4,30);
                left[size_t(k)].setBounds(x+10,top+33,(w-24)/2,25);
                right[size_t(k)].setBounds(x+14+(w-24)/2,top+33,(w-24)/2,25);
            }
            top+=80;
        }
        int columns=std::max(3,getWidth()/155),width=getWidth()/columns;
        for(size_t k=0;k<controls.size();++k) controls[k]->setBounds(int(k%size_t(columns))*width,top+int(k/size_t(columns))*148,width,142);
    }
};

BassAmbientEditor::BassAmbientEditor(BassAmbientProcessor& p):AudioProcessorEditor(p),processor(p),theme(std::make_unique<Theme>()) {
    setLookAndFeel(theme.get());
    const char* names[]{"BASS","GENERATOR","AMBIENT","FX RACK","OUTPUT"};
    for(int k=0;k<5;++k) {
        tabs[size_t(k)].setButtonText(names[k]);tabs[size_t(k)].onClick=[this,k]{selectPage(k);};addAndMakeVisible(tabs[size_t(k)]);
        pages[size_t(k)]=std::make_unique<Page>(processor,k);
    }
    addAndMakeVisible(viewport);viewport.setScrollBarsShown(true,false);viewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
    presets.addItemList({"Italo Analog","Acid Machine","808 Foundation","Lush Flow","Krell Dust","Bass + Atmosphere"},1);
    presets.setText("Factory scenes",juce::dontSendNotification);presets.onChange=[this]{processor.applyPreset(presets.getSelectedId()-1);};addAndMakeVisible(presets);
    mutate.onClick=[this] {
        if(auto* seed=processor.state.getParameter("seed")) { auto next=juce::Random::getSystemRandom().nextInt(65535)+1;seed->beginChangeGesture();seed->setValueNotifyingHost(seed->convertTo0to1(float(next)));seed->endChangeGesture(); }
    };addAndMakeVisible(mutate);
    panic.onClick=[this] { // Use a state restore to request an audio-thread-safe panic.
        juce::MemoryBlock state;processor.getStateInformation(state);processor.setStateInformation(state.getData(),int(state.getSize()));
    };addAndMakeVisible(panic);addAndMakeVisible(status);
    status.setColour(juce::Label::textColourId,muted);
    // setResizeLimits may invoke resized(): pages must exist before applying it.
    setResizable(true,true);setResizeLimits(840,600,1680,1200);
    setSize(1040,740);selectPage(0);startTimerHz(20);
}
BassAmbientEditor::~BassAmbientEditor() { stopTimer();viewport.setViewedComponent(nullptr,false);setLookAndFeel(nullptr); }
void BassAmbientEditor::selectPage(int index) {
    selected=index;for(int k=0;k<5;++k) tabs[size_t(k)].setToggleState(k==index,juce::dontSendNotification);
    viewport.setViewedComponent(pages[size_t(index)].get(),false);resized();viewport.setViewPosition(0,0);
}
void BassAmbientEditor::paint(juce::Graphics& g) {
    g.fillAll(background);g.setColour(text);g.setFont(juce::Font(juce::FontOptions(22).withStyle("Bold")));
    g.drawText("JERZY  /  BASS AMBIENT",20,12,330,35,juce::Justification::centredLeft);
    g.setColour(muted);g.setFont(juce::Font(juce::FontOptions(12)));
    g.drawText("0.1.0  •  PIANO ROLL ROOT  •  HOST SYNC",22,47,340,20,juce::Justification::centredLeft);
}
void BassAmbientEditor::resized() {
    presets.setBounds(getWidth()-460,22,220,34);mutate.setBounds(getWidth()-228,22,100,34);panic.setBounds(getWidth()-116,22,96,34);
    int tabWidth=(getWidth()-40)/5;for(int k=0;k<5;++k) tabs[size_t(k)].setBounds(20+k*tabWidth,80,tabWidth-6,38);
    viewport.setBounds(16,130,getWidth()-32,getHeight()-174);
    auto& page=*pages[size_t(selected)];int w=viewport.getWidth()-18;page.setSize(w,page.requiredHeight(w));
    status.setBounds(20,getHeight()-36,getWidth()-40,26);
}
void BassAmbientEditor::timerCallback() {
    int root=processor.rootDisplay.load();float level=processor.peak.load();
    juce::String note=root<0?"—":juce::MidiMessage::getMidiNoteName(root,true,true,3);
    juce::String db=level<.00001f?"−inf":juce::String(juce::Decibels::gainToDecibels(level),1);
    status.setText("MIDI root: "+note+"    |    Output peak: "+db+" dBFS    |    Double-click knob: reset    |    FX tab: scroll for all processors",juce::dontSendNotification);
    pages[3]->updateOrder();
}
