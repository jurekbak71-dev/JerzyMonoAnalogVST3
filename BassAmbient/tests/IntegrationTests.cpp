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
        // Actual vector GUI snapshots at minimum, default and large sizes.
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        const std::array<std::pair<int,int>,3> sizes{{{840,600},{1040,740},{1440,1000}}};
        for(const auto& size:sizes) {
            editor->setSize(size.first,size.second);
            auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true,1);
            require(image.isValid()&&image.getWidth()==size.first&&image.getHeight()==size.second,"resizable GUI snapshot");
            auto file=juce::File::getCurrentWorkingDirectory().getChildFile("gui-"+juce::String(size.first)+".png");
            auto stream=file.createOutputStream();require(stream!=nullptr,"GUI snapshot output");
            juce::PNGImageFormat png;require(png.writeImageToStream(image,*stream),"GUI PNG encoding");
        }
        editor->setSize(1040,740);
        for(const auto& tab:juce::StringArray{"AMBIENT","FX RACK"}) {
            bool found=false;
            for(int i=0;i<editor->getNumChildComponents();++i) if(auto* button=dynamic_cast<juce::TextButton*>(editor->getChildComponent(i))) {
                if(button->getButtonText()==tab&&button->onClick) {button->onClick();found=true;break;}
            }
            require(found,"ambient / FX tab navigation");
            auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true,1);
            auto file=juce::File::getCurrentWorkingDirectory().getChildFile("gui-"+tab.removeCharacters(" ")+".png");
            auto stream=file.createOutputStream();require(stream!=nullptr,"tab PNG output");
            juce::PNGImageFormat png;require(png.writeImageToStream(image,*stream),"tab PNG encoding");
        }
        processor.releaseResources();processor.setPlayHead(nullptr);
        std::cout<<"PASS host MIDI offsets, parameter bindings, preset/state restore, FX order and GUI at 3 sizes\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n";return 1; }
}
