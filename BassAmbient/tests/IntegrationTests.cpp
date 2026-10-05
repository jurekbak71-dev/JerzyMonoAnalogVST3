#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>

void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
class Host final : public juce::AudioPlayHead {
public:
    double ppq=0,bpm=120;bool playing=true;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo p;p.setPpqPosition(ppq);p.setBpm(bpm);p.setIsPlaying(playing);p.setTimeSignature(TimeSignature{7,8});return p;
    }
};
int main() {
    try {
        juce::ScopedJuceInitialiser_GUI init;
        BassAmbientProcessor processor;Host host;processor.setPlayHead(&host);processor.prepareToPlay(48000,128);
        auto set=[&](const char* id,float v) { auto* p=processor.state.getParameter(id);require(p!=nullptr,"parameter missing");p->setValueNotifyingHost(p->convertTo0to1(v)); };
        auto specs=parameterSpecs();require(processor.getParameters().size()==int(specs.size()),"host parameter count");
        for(const auto& s:specs) require(processor.state.getRawParameterValue(s.id)!=nullptr,"unbound parameter");
        juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1,36,1.0f),32);processor.processBlock(audio,midi);
        float early=0,late=0;for(int i=0;i<128;++i) { if(i<32)early+=std::abs(audio.getSample(0,i));else late+=std::abs(audio.getSample(0,i)); }
        require(early==0&&late>0,"sample-offset MIDI note-on");require(processor.rootDisplay.load()==36,"host MIDI root");
        host.ppq+=128.0/24000;midi.addEvent(juce::MidiMessage::noteOff(1,36),64);processor.processBlock(audio,midi);
        require(processor.rootDisplay.load()==-1,"sample-offset MIDI note-off");
        set("cutoff",731);set("seed",4321);set("pad",1);set("order0",4);set("order4",0);
        juce::MemoryBlock state;processor.getStateInformation(state);set("cutoff",12000);set("seed",1);
        processor.setStateInformation(state.getData(),int(state.getSize()));
        require(std::abs(processor.state.getRawParameterValue("cutoff")->load()-731)<.01f,"cutoff state restore");
        require(processor.state.getRawParameterValue("seed")->load()==4321,"seed state restore");
        require(processor.readParameters().order[0]==4,"FX order state restore");
        for(int k=0;k<6;++k) { processor.applyPreset(k);auto p=processor.readParameters();require(p.seed==71,"factory preset complete reset"); }
        set("delayMix",.11f);set("pad_delayMix",.37f);set("master_delayMix",.61f);
        set("pad_order0",4);set("pad_order4",0);set("masterFX",.7f);
        processor.getStateInformation(state);processor.applyPreset(0);
        processor.setStateInformation(state.getData(),int(state.getSize()));
        auto racks=processor.readParameters();
        require(std::abs(racks.delayMix-.11)<1e-5&&std::abs(racks.padRack.delayMix-.37)<1e-5&&std::abs(racks.masterRack.delayMix-.61)<1e-5,"three independent FX states");
        require(racks.padRack.order[0]==4&&racks.order[0]==0&&racks.masterRack.order[0]==0,"three independent FX orders");
        // Rebuild an authentic schema-1 tree: only legacy IDs, shared settings.
        auto old=processor.state.copyState();old.setProperty("schemaVersion",1,nullptr);
        for(int i=old.getNumChildren()-1;i>=0;--i) {
            auto id=old.getChild(i).getProperty("id").toString();
            if(id.startsWith("pad_")||id.startsWith("master_")||id=="masterFX") old.removeChild(i,nullptr);
        }
        juce::MemoryBlock legacy;auto xml=old.createXml();juce::AudioProcessor::copyXmlToBinary(*xml,legacy);
        processor.setStateInformation(legacy.getData(),int(legacy.getSize()));racks=processor.readParameters();
        require(std::abs(racks.padRack.delayMix-racks.delayMix)<1e-6&&racks.masterFX==0,"legacy shared FX migration with dry master");
        processor.applyPreset(0);
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        auto tab=[&](const juce::String& name) {
            for(int i=0;i<editor->getNumChildComponents();++i)
                if(auto* button=dynamic_cast<juce::TextButton*>(editor->getChildComponent(i)))
                    if(button->getButtonText()==name&&button->onClick) {button->onClick();return;}
            throw std::runtime_error("main page missing");
        };
        juce::ComboBox* selector=nullptr;
        for(int i=0;i<editor->getNumChildComponents();++i)
            if(editor->getChildComponent(i)->getComponentID()=="rackSelector") selector=dynamic_cast<juce::ComboBox*>(editor->getChildComponent(i));
        require(selector!=nullptr,"FX rack selector missing");
        auto snapshot=[&](const juce::String& suffix) {
            auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true,1);
            require(image.isValid()&&image.getWidth()==editor->getWidth()&&image.getHeight()==editor->getHeight(),"resizable GUI snapshot");
            auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("gui-"+suffix+".png").createOutputStream();
            require(stream!=nullptr,"GUI snapshot output");juce::PNGImageFormat png;
            require(png.writeImageToStream(image,*stream),"GUI PNG encoding");
        };
        const std::array<std::pair<int,int>,3> sizes{{{840,600},{1040,740},{1440,1000}}};
        for(const auto& size:sizes) {
            editor->setSize(size.first,size.second);
            for(const auto& name:juce::StringArray{"BASS","AMBIENT","FX"}) {
                tab(name);require(selector->isVisible()==(name=="FX"),"rack selector visibility");
                if(name!="FX") snapshot(name+"-"+juce::String(size.first));
                else for(int rack=1;rack<=3;++rack) {
                    selector->setSelectedId(rack,juce::sendNotificationSync);
                    snapshot("FX-"+juce::String(rack)+"-"+juce::String(size.first));
                }
            }
        }
        // Reordering one rack through its actual GUI leaves the others untouched.
        tab("FX");selector->setSelectedId(2,juce::sendNotificationSync);
        juce::Viewport* viewport=nullptr;
        for(int i=0;i<editor->getNumChildComponents();++i)
            if(auto* v=dynamic_cast<juce::Viewport*>(editor->getChildComponent(i))) viewport=v;
        require(viewport!=nullptr,"page viewport");auto* page=viewport->getViewedComponent();
        bool moved=false;
        for(int i=0;i<page->getNumChildComponents();++i)
            if(auto* button=dynamic_cast<juce::TextButton*>(page->getChildComponent(i)))
                if(button->getButtonText()==">"&&button->isEnabled()) {button->onClick();moved=true;break;}
        racks=processor.readParameters();require(moved&&racks.padRack.order[0]==1&&racks.padRack.order[1]==0&&racks.order[0]==0&&racks.masterRack.order[0]==0,"GUI independent rack reorder");
        bool delaySelected=false;
        for(int i=0;i<page->getNumChildComponents();++i)
            if(auto* button=dynamic_cast<juce::TextButton*>(page->getChildComponent(i)))
                if(button->getButtonText().contains("DELAY")) {button->onClick();delaySelected=true;break;}
        require(delaySelected,"FX module navigation");
        bool attached=false;
        for(int i=0;i<page->getNumChildComponents();++i) {
            auto* control=page->getChildComponent(i);
            if(control->getComponentID()=="pad_delayMix") {
                require(control->isVisible(),"selected module control visible");
                for(int k=0;k<control->getNumChildComponents();++k)
                    if(auto* slider=dynamic_cast<juce::Slider*>(control->getChildComponent(k))) {slider->setValue(.63,juce::sendNotificationSync);attached=true;}
            }
        }
        racks=processor.readParameters();require(attached&&std::abs(racks.padRack.delayMix-.63)<1e-5&&std::abs(racks.delayMix-.2)<1e-5,"rack GUI connected to correct DSP parameters");
        snapshot("AMBIENT-DELAY");
        processor.releaseResources();processor.setPlayHead(nullptr);
        std::cout<<"PASS MIDI, three-rack routing state, legacy migration, GUI attachments, reorder and 15 page/rack snapshots\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n";return 1; }
}
