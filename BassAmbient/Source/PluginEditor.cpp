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
    BassAmbientProcessor& processor;
    int index, selectedModule=0;
    juce::String prefix;
    struct Item { std::unique_ptr<Control> control; int module=-1; };
    std::vector<Item> controls;
    std::array<juce::TextButton,6> modules, left, right;
    juce::Label description, generatorHeading;
    int generatorStart=-1;
    bool isFX() const { return index>=2; }
    void swapSlots(int a,int b) {
        if(b<0||b>=6) return;
        auto* pa=processor.state.getParameter(prefix+"order"+juce::String(a));
        auto* pb=processor.state.getParameter(prefix+"order"+juce::String(b));
        float va=pa->getValue(),vb=pb->getValue();
        pa->beginChangeGesture();pb->beginChangeGesture();
        pa->setValueNotifyingHost(vb);pb->setValueNotifyingHost(va);
        pa->endChangeGesture();pb->endChangeGesture();updateOrder();
    }
    int slotModule(int slot) const {
        return juce::jlimit(0,5,int(processor.state.getRawParameterValue(prefix+"order"+juce::String(slot))->load()));
    }
public:
    std::function<void()> layoutChanged;
    Page(BassAmbientProcessor& p,int page):processor(p),index(page),prefix(page==3?"pad_":page==4?"master_":"") {
        const char* descriptions[]{
            "BASS: synth controls, followed by the MIDI phrase generator. One Piano Roll note supplies the root.",
            "AMBIENT: two engines per voice, overlapping envelopes and HOLD / FLOW / KRELL. Shared scale and seed below.",
            "BASS FX: independent processing before the layer sum. Select a module to edit; arrows change its position.",
            "AMBIENT FX: independent processing before the layer sum. Select a module to edit; arrows change its position.",
            "MASTER FX: processes the combined layers, before output level and soft limiter. Amount 0 = dry master."};
        description.setText(descriptions[page],juce::dontSendNotification);
        description.setColour(juce::Label::textColourId,muted);addAndMakeVisible(description);
        generatorHeading.setText("PHRASE GENERATOR / MIDI",juce::dontSendNotification);
        generatorHeading.setColour(juce::Label::textColourId,accent);
        if(index==0) addAndMakeVisible(generatorHeading);
        int module=-1;
        for(auto spec:parameterSpecs()) {
            bool include=false;int itemModule=-1;
            if(index==0) include=spec.group==0||spec.group==1;
            else if(index==1) include=spec.group==2||spec.id=="scale"||spec.id=="seed";
            else {
                const bool own=prefix.isEmpty()?(!spec.id.startsWith("pad_")&&!spec.id.startsWith("master_")):spec.id.startsWith(prefix);
                if(spec.group==3&&own) {
                    auto id=spec.id.substring(prefix.length());
                    if(id.startsWith("order")) continue;
                    if(id=="fxDrive") module=0;
                    if(id=="fxChorus") module=1;
                    if(id=="fxDelay") module=2;
                    if(id=="fxGrain") module=3;
                    if(id=="fxReverb") module=4;
                    if(id=="fxWidth") module=5;
                    itemModule=module;include=true;
                    if(index==3) spec.label=spec.label.fromFirstOccurrenceOf("Ambient ",false,false);
                    if(index==4) spec.label=spec.label.fromFirstOccurrenceOf("Master ",false,false);
                }
                include|=spec.id==(index==2?"bassFX":index==3?"padFX":"masterFX");
                if(index==4) include|=spec.id=="master"||spec.id=="limiter";
            }
            if(!include) continue;
            if(index==0&&spec.group==1&&generatorStart<0) generatorStart=int(controls.size());
            auto control=std::make_unique<Control>(processor,spec);
            control->setComponentID(spec.id);addAndMakeVisible(*control);
            controls.push_back({std::move(control),itemModule});
        }
        if(isFX()) for(int k=0;k<6;++k) {
            addAndMakeVisible(modules[size_t(k)]);
            modules[size_t(k)].onClick=[this,k] {
                selectedModule=slotModule(k);updateOrder();
                if(layoutChanged) layoutChanged();
            };
            left[size_t(k)].setButtonText("<");right[size_t(k)].setButtonText(">");
            addAndMakeVisible(left[size_t(k)]);addAndMakeVisible(right[size_t(k)]);
            left[size_t(k)].onClick=[this,k]{swapSlots(k,k-1);};
            right[size_t(k)].onClick=[this,k]{swapSlots(k,k+1);};
            left[size_t(k)].setEnabled(k>0);right[size_t(k)].setEnabled(k<5);
        }
        updateOrder();
    }
    void updateOrder() {
        if(!isFX()) return;
        const char* names[]{"DRIVE / COMP","CHORUS","DELAY","GRANULAR","REVERB","WIDTH"};
        for(int k=0;k<6;++k) {
            const int id=slotModule(k);
            modules[size_t(k)].setButtonText(juce::String(k+1)+"  "+names[id]);
            modules[size_t(k)].setToggleState(id==selectedModule,juce::dontSendNotification);
        }
    }
    int arrange(int width,bool apply) {
        const int columns=std::max(3,width/155),cell=width/columns;
        int top=isFX()?130:50,count=0;
        for(size_t k=0;k<controls.size();++k) {
            if(int(k)==generatorStart) {
                top+=((count+columns-1)/columns)*148;
                if(apply) generatorHeading.setBounds(12,top,width-24,36);
                top+=40;count=0;
            }
            auto& item=controls[k];bool visible=!isFX()||item.module<0||item.module==selectedModule;
            if(apply) item.control->setVisible(visible);
            if(!visible) continue;
            if(apply) item.control->setBounds((count%columns)*cell,top+(count/columns)*148,cell,142);
            ++count;
        }
        return top+((count+columns-1)/columns)*148;
    }
    int requiredHeight(int width) { return arrange(width,false); }
    void resized() override {
        description.setBounds(10,0,getWidth()-20,44);
        if(isFX()) {
            const int w=getWidth()/6;
            for(int k=0;k<6;++k) {
                const int x=k*w;
                modules[size_t(k)].setBounds(x+2,50,w-4,34);
                left[size_t(k)].setBounds(x+10,88,(w-24)/2,25);
                right[size_t(k)].setBounds(x+14+(w-24)/2,88,(w-24)/2,25);
            }
        }
        arrange(getWidth(),true);
    }
};

BassAmbientEditor::BassAmbientEditor(BassAmbientProcessor& p):AudioProcessorEditor(p),processor(p),theme(std::make_unique<Theme>()) {
    setLookAndFeel(theme.get());
    const char* names[]{"BASS","AMBIENT","FX"};
    for(int k=0;k<3;++k) {
        tabs[size_t(k)].setButtonText(names[k]);tabs[size_t(k)].onClick=[this,k]{selectPage(k);};addAndMakeVisible(tabs[size_t(k)]);
    }
    for(int k=0;k<5;++k) {
        pages[size_t(k)]=std::make_unique<Page>(processor,k);
        pages[size_t(k)]->layoutChanged=[this]{resized();};
    }
    rackSelector.addItemList({"BASS rack","AMBIENT rack","MASTER rack"},1);
    rackSelector.setComponentID("rackSelector");rackSelector.setSelectedId(1,juce::dontSendNotification);
    rackSelector.onChange=[this] { selectedRack=rackSelector.getSelectedId()-1;selectPage(2); };
    addAndMakeVisible(rackSelector);
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
BassAmbientEditor::Page& BassAmbientEditor::currentPage() { return *pages[size_t(selected==2?2+selectedRack:selected)]; }
void BassAmbientEditor::selectPage(int index) {
    selected=index;for(int k=0;k<3;++k) tabs[size_t(k)].setToggleState(k==index,juce::dontSendNotification);
    rackSelector.setVisible(index==2);
    viewport.setViewedComponent(&currentPage(),false);resized();viewport.setViewPosition(0,0);
}
void BassAmbientEditor::paint(juce::Graphics& g) {
    g.fillAll(background);g.setColour(text);g.setFont(juce::Font(juce::FontOptions(22).withStyle("Bold")));
    g.drawText("JERZY  /  BASS AMBIENT",20,12,330,35,juce::Justification::centredLeft);
    g.setColour(muted);g.setFont(juce::Font(juce::FontOptions(12)));
    g.drawText("0.2.0  •  PIANO ROLL ROOT  •  HOST SYNC",22,47,340,20,juce::Justification::centredLeft);
}
void BassAmbientEditor::resized() {
    presets.setBounds(getWidth()-460,22,220,34);mutate.setBounds(getWidth()-228,22,100,34);panic.setBounds(getWidth()-116,22,96,34);
    int tabWidth=(getWidth()-260)/3;for(int k=0;k<3;++k) tabs[size_t(k)].setBounds(20+k*tabWidth,80,tabWidth-6,38);
    rackSelector.setBounds(getWidth()-232,80,212,38);
    viewport.setBounds(16,130,getWidth()-32,getHeight()-174);
    auto& page=currentPage();int w=viewport.getWidth()-18;page.setSize(w,page.requiredHeight(w));page.resized();
    status.setBounds(20,getHeight()-36,getWidth()-40,26);
}
void BassAmbientEditor::timerCallback() {
    int root=processor.rootDisplay.load();float level=processor.peak.load();
    juce::String note=root<0?"—":juce::MidiMessage::getMidiNoteName(root,true,true,3);
    juce::String db=level<.00001f?"−inf":juce::String(juce::Decibels::gainToDecibels(level),1);
    status.setText("MIDI root: "+note+"    |    Output peak: "+db+" dBFS    |    Double-click knob: reset    |    FX: choose rack, then module",juce::dontSendNotification);
    for(int k=2;k<5;++k) pages[size_t(k)]->updateOrder();
}
