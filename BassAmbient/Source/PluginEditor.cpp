#include "PluginEditor.h"

namespace {
juce::Colour sectionColour(int section) {return juce::Colour(section==0?0xffeab35d:section==1?0xff5edbc1:0xffbf91f3);}
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
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float value,float start,float end,juce::Slider& slider) override {
        auto bounds=juce::Rectangle<float>(float(x),float(y),float(w),float(h)).reduced(8);
        float size=std::min(bounds.getWidth(),bounds.getHeight()),radius=size*.5f;
        auto centre=bounds.getCentre();auto dial=juce::Rectangle<float>(size,size).withCentre(centre);
        auto colour=slider.findColour(juce::Slider::rotarySliderFillColourId);
        juce::DropShadow(juce::Colours::black.withAlpha(.75f),5,{0,3}).drawForRectangle(g,dial.reduced(5).toNearestInt());
        juce::ColourGradient metal(juce::Colour(0xff69727e),centre.x-radius,centre.y-radius,juce::Colour(0xff171d26),centre.x+radius,centre.y+radius,false);
        g.setGradientFill(metal);g.fillEllipse(dial.reduced(4));
        g.setColour(juce::Colour(0xff8792a0).withAlpha(.5f));g.drawEllipse(dial.reduced(4),1);
        juce::ColourGradient face(juce::Colour(0xff424e5e),centre.x,centre.y-radius,juce::Colour(0xff1b222c),centre.x,centre.y+radius,false);
        g.setGradientFill(face);g.fillEllipse(dial.reduced(8));
        juce::Path track;track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
        g.setColour(muted.withAlpha(.25f));g.strokePath(track,juce::PathStrokeType(3));
        juce::Path active;float angle=start+value*(end-start);
        active.addCentredArc(centre.x,centre.y,radius,radius,0,start,angle,true);
        g.setColour(colour);g.strokePath(active,juce::PathStrokeType(3));
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
    Control(BassAmbientProcessor& p,const ParameterSpec& s,int section):spec(s) {
        slider.setColour(juce::Slider::rotarySliderFillColourId,sectionColour(section));
        label.setText(s.label,juce::dontSendNotification);label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(juce::FontOptions(14)));addAndMakeVisible(label);
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
            slider.setTooltip(s.label+" - automatable in FL Studio");
            addAndMakeVisible(slider);
            sliderAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,s.id,slider);
            // APVTS installs its own formatter: override it after attachment creation.
            slider.textFromValueFunction=[s](double value) {
                return juce::String(value,s.kind==2?0:(s.maximum>=100?1:3));
            };
            slider.updateText();
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
    int index, selectedModule=0,selectedSection=0;
    std::array<juce::TextButton,4> sections;
    juce::String prefix;
    struct Item { std::unique_ptr<Control> control; int module=-1,section=0; };
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
            "BASS  /  SOUND - PHRASE - PERFORMANCE",
            "AMBIENT  /  BACKGROUND + RHYTHM + KRELL",
            "BASS FX  /  INDEPENDENT RACK",
            "AMBIENT FX  /  INDEPENDENT RACK",
            "MASTER FX  /  COMBINED LAYERS"};
        description.setText(descriptions[page],juce::dontSendNotification);
        description.setColour(juce::Label::textColourId,muted);addAndMakeVisible(description);
        generatorHeading.setText("PHRASE GENERATOR / MIDI",juce::dontSendNotification);
        generatorHeading.setColour(juce::Label::textColourId,accent);
        if(!isFX()) for(int i=0;i<(index==0?3:4);++i) {
            const char* bassNames[]{"SOUND","PHRASE","PERFORMANCE"};
            const char* padNames[]{"BACKGROUND","RHYTHM","KRELL","EVOLUTION"};
            auto& button=sections[size_t(i)];button.setButtonText(index==0?bassNames[i]:padNames[i]);
            button.setColour(juce::TextButton::textColourOnId,sectionColour(index));
            button.setColour(juce::TextButton::buttonOnColourId,sectionColour(index).withAlpha(.2f));
            button.onClick=[this,i]{selectedSection=i;if(layoutChanged)layoutChanged();};addAndMakeVisible(button);
        }
        int module=-1;
        for(auto spec:parameterSpecs()) {
            bool include=false;int itemModule=-1,itemSection=0;
            if(index==0) {include=spec.group==0||spec.group==1||spec.group==8;itemSection=spec.group==0?0:spec.group==1?1:2;}
            else if(index==1) {
                include=spec.group==2||spec.group==5||spec.group==6||spec.group==7||spec.group==8||spec.id=="scale"||spec.id=="seed";
                itemSection=spec.group==5?1:spec.group==6?2:(spec.group==7||spec.group==8||spec.id=="scale"||spec.id=="seed")?3:0;
            }
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

            auto control=std::make_unique<Control>(processor,spec,index==0?0:index==1?1:2);
            control->setComponentID(spec.id);addAndMakeVisible(*control);
            controls.push_back({std::move(control),itemModule,itemSection});
        }
        if(index==0) {
            std::stable_sort(controls.begin(),controls.end(),[](const auto& a,const auto& b){
                auto rank=[](const auto& item){auto id=item.control->getComponentID();return id=="source"||id=="styleFamily"?0:1;};
                return rank(a)<rank(b);
            });
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
        if(!isFX()) {
            for(auto& item:controls) {
                auto id=item.control->getComponentID();
                if(id=="model")item.control->setEnabled(processor.state.getRawParameterValue("source")->load()<.5f);
                if(id=="style")item.control->setEnabled(processor.state.getRawParameterValue("styleFamily")->load()<.5f);
            }
            return;
        }
        const char* names[]{"DRIVE / COMP","CHORUS","DELAY","GRANULAR","REVERB","WIDTH"};
        for(int k=0;k<6;++k) {
            const int id=slotModule(k);
            modules[size_t(k)].setButtonText(juce::String(k+1)+"  "+names[id]);
            modules[size_t(k)].setToggleState(id==selectedModule,juce::dontSendNotification);
        }
    }
    int arrange(int width,bool apply) {
        const int columns=std::max(3,width/155),cell=width/columns;
        int top=isFX()?130:92,count=0;
        for(size_t k=0;k<controls.size();++k) {
            if(int(k)==generatorStart) {
                top+=((count+columns-1)/columns)*148;
                if(apply) generatorHeading.setBounds(12,top,width-24,36);
                top+=40;count=0;
            }
            auto& item=controls[k];bool visible=isFX()?(item.module<0||item.module==selectedModule):item.section==selectedSection;
            if(apply) item.control->setVisible(visible);
            if(!visible) continue;
            if(apply) item.control->setBounds((count%columns)*cell,top+(count/columns)*148,cell,142);
            ++count;
        }
        return top+((count+columns-1)/columns)*148;
    }
    int requiredHeight(int width) { return arrange(width,false); }
    void resized() override {
        description.setBounds(10,0,getWidth()-20,38);
        if(!isFX()) {
            int count=index==0?3:4,cell=getWidth()/count;
            for(int i=0;i<count;++i) {sections[size_t(i)].setBounds(i*cell+3,44,cell-6,36);sections[size_t(i)].setToggleState(i==selectedSection,juce::dontSendNotification);}
        }
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
    presets.addItemList({"Italo Analog","Acid Machine","808 Foundation","Lush Flow","Krell Dust","Bass + Atmosphere","Rock Pick Bass","Gothic Bass","Living Landscape","Rhythmic Piano","Krell Laboratory"},1);
    presets.setText("Factory scenes",juce::dontSendNotification);presets.onChange=[this]{processor.applyPreset(presets.getSelectedId()-1);};addAndMakeVisible(presets);
    generate.onClick=[this]{processor.generate(false);};addAndMakeVisible(generate);
    mutate.onClick=[this]{processor.generate(true);};addAndMakeVisible(mutate);
    undo.onClick=[this]{processor.undoGeneration();};addAndMakeVisible(undo);
    panic.onClick=[this]{processor.panicNow();};addAndMakeVisible(panic);
    bassPlay.onClick=[this]{processor.auditionBass=!processor.auditionBass.load();};addAndMakeVisible(bassPlay);
    padPlay.onClick=[this]{processor.auditionPad=!processor.auditionPad.load();};addAndMakeVisible(padPlay);
    bassPlay.setColour(juce::TextButton::textColourOnId,sectionColour(0));padPlay.setColour(juce::TextButton::textColourOnId,sectionColour(1));
    midiExport.onClick=[this]{exportPerformance(true);};wavExport.onClick=[this]{exportPerformance(false);};
    addAndMakeVisible(midiExport);addAndMakeVisible(wavExport);
    storeScene.onClick=[this]{storing=!storing;storeScene.setToggleState(storing,juce::dontSendNotification);};addAndMakeVisible(storeScene);
    for(int k=0;k<4;++k) {auto& button=scenes[size_t(k)];button.setButtonText(juce::String::charToString(char('A'+k)));
        button.onClick=[this,k]{if(storing){processor.saveScene(k);storing=false;storeScene.setToggleState(false,juce::dontSendNotification);}else processor.requestScene(k);};addAndMakeVisible(button);}
    addAndMakeVisible(status);
    status.setColour(juce::Label::textColourId,muted);
    // setResizeLimits may invoke resized(): pages must exist before applying it.
    setResizable(true,true);setResizeLimits(840,600,1680,1200);
    setSize(1120,820);selectPage(0);startTimerHz(20);
}
BassAmbientEditor::~BassAmbientEditor() { stopTimer();viewport.setViewedComponent(nullptr,false);setLookAndFeel(nullptr); }
BassAmbientEditor::Page& BassAmbientEditor::currentPage() { return *pages[size_t(selected==2?2+selectedRack:selected)]; }
void BassAmbientEditor::selectPage(int index) {
    selected=index;for(int k=0;k<3;++k) {tabs[size_t(k)].setToggleState(k==index,juce::dontSendNotification);tabs[size_t(k)].setColour(juce::TextButton::textColourOnId,sectionColour(k));tabs[size_t(k)].setColour(juce::TextButton::buttonOnColourId,sectionColour(k).withAlpha(.2f));}
    rackSelector.setVisible(index==2);
    viewport.setViewedComponent(&currentPage(),false);resized();viewport.setViewPosition(0,0);
}
void BassAmbientEditor::paint(juce::Graphics& g) {
    g.fillAll(background);g.setColour(text);g.setFont(juce::Font(juce::FontOptions(22).withStyle("Bold")));
    g.drawText("JERZY / BASS AMBIENT",20,10,300,35,juce::Justification::centredLeft);
    g.setColour(muted);g.setFont(juce::Font(juce::FontOptions(12)));
    g.drawText("0.3.0  |  GENERATIVE INSTRUMENT",22,44,300,20,juce::Justification::centredLeft);
}
void BassAmbientEditor::resized() {
    int right=getWidth()-20;
    panic.setBounds(right-62,22,62,34);undo.setBounds(right-130,22,62,34);mutate.setBounds(right-214,22,78,34);generate.setBounds(right-310,22,90,34);
    presets.setBounds(330,22,std::max(145,getWidth()-670),34);
    bassPlay.setBounds(20,78,116,32);padPlay.setBounds(142,78,142,32);
    storeScene.setBounds(300,78,60,32);
    for(int i=0;i<4;++i)scenes[size_t(i)].setBounds(366+i*38,78,34,32);
    midiExport.setBounds(right-192,78,92,32);wavExport.setBounds(right-92,78,92,32);
    int tabWidth=(getWidth()-260)/3;for(int k=0;k<3;++k)tabs[size_t(k)].setBounds(20+k*tabWidth,126,tabWidth-6,38);
    rackSelector.setBounds(getWidth()-232,126,212,38);
    viewport.setBounds(16,176,getWidth()-32,getHeight()-220);
    auto& page=currentPage();int w=viewport.getWidth()-18;page.setSize(w,page.requiredHeight(w));page.resized();
    status.setBounds(20,getHeight()-36,getWidth()-40,26);
}
void BassAmbientEditor::timerCallback() {
    processor.synchroniseLocks();
    bassPlay.setButtonText(processor.auditionBass.load()?"BASS STOP":"BASS START");padPlay.setButtonText(processor.auditionPad.load()?"AMBIENT STOP":"AMBIENT START");
    bassPlay.setToggleState(processor.auditionBass.load(),juce::dontSendNotification);padPlay.setToggleState(processor.auditionPad.load(),juce::dontSendNotification);
    bassPlay.setEnabled(!processor.hostRunning.load());padPlay.setEnabled(!processor.hostRunning.load());
    for(int k=0;k<4;++k){scenes[size_t(k)].setToggleState(processor.activeScene.load()==k,juce::dontSendNotification);scenes[size_t(k)].setTooltip(processor.hasScene(k)?"Recall at next bar":"STORE, then select this scene");}
    int root=processor.rootDisplay.load();float level=processor.peak.load();
    juce::String note=root<0?"--":juce::MidiMessage::getMidiNoteName(root,true,true,3);
    juce::String db=level<.00001f?"-inf":juce::String(juce::Decibels::gainToDecibels(level),1);
    status.setText("MIDI root: "+note+"    |    Output peak: "+db+" dBFS    |    Capture: last 16 beats / max 30 s    |    STORE + A-D: save scene",juce::dontSendNotification);
    for(int k=0;k<5;++k) pages[size_t(k)]->updateOrder();
}

void BassAmbientEditor::exportPerformance(bool midi) {
    chooser=std::make_unique<juce::FileChooser>(midi?"Save last 16 beats as MIDI":"Save last 16 beats as WAV",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(midi?"BassAmbient.mid":"BassAmbient.wav"),midi?"*.mid":"*.wav");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,
        [safe=juce::Component::SafePointer<BassAmbientEditor>(this),midi](const juce::FileChooser& c){
            if(!safe)return;auto file=c.getResult();if(file==juce::File{})return;
            juce::String error;
            if(!safe->processor.exportCapture(file,midi,error))juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,"Capture",error);
        });
}
