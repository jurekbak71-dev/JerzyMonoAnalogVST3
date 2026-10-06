#include "PluginProcessor.h"
#include "reference/PluginProcessor030.h"
#include <chrono>
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
        require(dynamic_cast<juce::AudioParameterChoice*>(processor.state.getParameter("model"))->choices.size()==3,"legacy model automation range");
        require(dynamic_cast<juce::AudioParameterChoice*>(processor.state.getParameter("style"))->choices.size()==5,"legacy style automation range");
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
        for(int k=0;k<11;++k) { processor.applyPreset(k);auto p=processor.readParameters();require(p.seed==71,"factory preset complete reset"); }
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
        // Independent audition and host takeover, then real file export round-trips.
        host.playing=false;processor.applyPreset(0);set("bass",0);set("pad",0);
        processor.auditionBass=true;processor.auditionPad=false;
        double previewEnergy=0;
        for(int block=0;block<400;++block){midi.clear();processor.processBlock(audio,midi);previewEnergy+=audio.getMagnitude(0,128);}
        require(previewEnergy>.01&&processor.rootDisplay.load()==36,"bass preview works with disabled layer and stopped host");
        host.playing=true;host.ppq=0;midi.addEvent(juce::MidiMessage::noteOn(1,43,.9f),0);processor.processBlock(audio,midi);
        require(!processor.auditionBass.load()&&processor.rootDisplay.load()==43,"host takes over audition without double root");
        host.playing=false;processor.processBlock(audio,midi);processor.auditionPad=true;
        set("rhythmOn",1);set("eventsOn",1);set("backgroundOn",0);set("rhythmDensity",1);
        for(int block=0;block<200;++block)processor.processBlock(audio,midi);
        auto midiFile=juce::File::getCurrentWorkingDirectory().getChildFile("capture-test.mid");
        auto wavFile=juce::File::getCurrentWorkingDirectory().getChildFile("capture-test.wav");juce::String error;
        require(processor.exportCapture(midiFile,true,error),"MIDI capture export");
        require(processor.exportCapture(wavFile,false,error),"audio capture export");
        juce::FileInputStream input(midiFile);juce::MidiFile result;require(result.readFrom(input)&&result.getNumTracks()==1,"valid MIDI round trip");
        bool notes=false;auto* track=result.getTrack(0);for(int i=0;i<track->getNumEvents();++i)notes|=track->getEventPointer(i)->message.isNoteOn();
        require(notes,"capture contains performed notes");
        juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(wavFile),true));
        require(reader&&reader->sampleRate==48000&&reader->numChannels==2&&reader->lengthInSamples>1000,"valid stereo WAV capture");
        // Scene snapshots survive session reload and are applied on a bar, including FX switches.
        set("cutoff",654);set("pad_fxGrain",1);set("pad_order0",3);processor.saveScene(0);
        processor.getStateInformation(state);set("cutoff",8000);processor.setStateInformation(state.getData(),int(state.getSize()));
        require(processor.hasScene(0),"saved scene survives project state");set("cutoff",8000);
        host.playing=true;host.ppq=1;processor.processBlock(audio,midi);processor.requestScene(0);
        host.ppq=1.1;processor.processBlock(audio,midi);require(processor.readParameters().cutoff>7900,"scene waits for bar");
        host.ppq=3.499;processor.processBlock(audio,midi);require(std::abs(processor.readParameters().cutoff-654)<.1,"scene recalled at 7/8 bar boundary");
        require(processor.readParameters().padRack.fxGrain&&processor.readParameters().padRack.order[0]==3,"scene recalls FX configuration");
        processor.generate(false);processor.undoGeneration();require(std::abs(processor.readParameters().cutoff-654)<.1,"generation undo preserves state");
        // Inspect every ambient subpanel at minimum size.
        editor->setSize(840,600);tab("AMBIENT");page=viewport->getViewedComponent();
        for(const auto& name:juce::StringArray{"BACKGROUND","RHYTHM","KRELL","EVOLUTION"}) {
            for(int i=0;i<page->getNumChildComponents();++i)if(auto* button=dynamic_cast<juce::TextButton*>(page->getChildComponent(i)))if(button->getButtonText()==name)button->onClick();
            for(int i=0;i<page->getNumChildComponents();++i)for(int j=i+1;j<page->getNumChildComponents();++j) {
                auto* a=page->getChildComponent(i);auto* b=page->getChildComponent(j);
                if(a->isVisible()&&b->isVisible()&&!a->getComponentID().isEmpty()&&!b->getComponentID().isEmpty())require(!a->getBounds().intersects(b->getBounds()),"no control overlap");
            }
            snapshot("AMBIENT-"+name+"-840");
        }
        // Identical timeline at different host block sizes produces identical audio.
        auto render=[](int blockSize) {
            auto synth=std::make_unique<BassAmbientProcessor>();Host timeline;synth->setPlayHead(&timeline);
            auto put=[&](const char* id,float value){auto* parameter=synth->state.getParameter(id);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));};
            put("pad",1);put("eventsOn",1);put("rhythmOn",1);put("padAttack",.05f);put("krellRate",.125f);
            synth->prepareToPlay(48000,blockSize);std::vector<float> samples; samples.reserve(12000);
            for(int offset=0;offset<12000;) {
                int length=std::min(blockSize,12000-offset);juce::AudioBuffer<float> block(2,length);juce::MidiBuffer events;
                if(offset==0)events.addEvent(juce::MidiMessage::noteOn(1,36,1.0f),0);
                timeline.ppq=double(offset)/24000;synth->processBlock(block,events);
                for(int i=0;i<length;++i)samples.push_back(block.getSample(0,i));offset+=length;
            }
            synth->setPlayHead(nullptr);return samples;
        };
        auto small=render(64),large=render(511);double difference=0;
        for(size_t i=0;i<small.size();++i)difference=std::max(difference,double(std::abs(small[i]-large[i])));
        require(difference<1e-5,"block-size independent MIDI and generative rendering");
        // Full 0.3.0 processor comparison, including moving controls and scene changes.
        double fullPeakError=0;
        for(int preset: {0,6,8,9,10}) {
            auto original=std::make_unique<ReferenceProcessor030>();
            auto optimised=std::make_unique<BassAmbientProcessor>();Host clock;
            original->setPlayHead(&clock);optimised->setPlayHead(&clock);
            original->applyPreset(preset);optimised->applyPreset(preset);
            original->prepareToPlay(48000,128);optimised->prepareToPlay(48000,128);
            original->saveScene(0);optimised->saveScene(0);
            double oldTime=0,newTime=0;
            auto automate=[&](const char* id,float value) {
                auto* a=original->state.getParameter(id);auto* b=optimised->state.getParameter(id);
                a->setValueNotifyingHost(a->convertTo0to1(value));b->setValueNotifyingHost(b->convertTo0to1(value));
            };
            for(int block=0;block<1000;++block) {
                juce::AudioBuffer<float> a(2,128),b(2,128);juce::MidiBuffer inputA,inputB;
                if(block==0)inputA.addEvent(juce::MidiMessage::noteOn(1,36,.9f),17);
                if(block==130){automate("cutoff",873);automate("pad_chorusDepth",.82f);automate("padLevel",.61f);automate("masterFX",.28f);}
                if(block==220)inputA.addEvent(juce::MidiMessage::pitchWheel(1,9800),63);
                if(block==300){original->requestScene(0);optimised->requestScene(0);}
                if(block==800)inputA.addEvent(juce::MidiMessage::noteOff(1,36),54);
                if(block==850)inputA.addEvent(juce::MidiMessage::controllerEvent(1,120,0),11);
                clock.ppq=double(block*128)/24000;inputB=inputA;
                auto t=std::chrono::steady_clock::now();original->processBlock(a,inputA);
                oldTime+=std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count();
                t=std::chrono::steady_clock::now();optimised->processBlock(b,inputB);
                newTime+=std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count();
                for(int ch=0;ch<2;++ch)for(int sample=0;sample<128;++sample)
                    fullPeakError=std::max(fullPeakError,double(std::abs(a.getSample(ch,sample)-b.getSample(ch,sample))));
            }
            std::cout<<"Full processor preset "<<preset<<": "<<oldTime<<" -> "<<newTime<<" s, CPU reduction "<<100*(1-newTime/oldTime)<<"%\n";
            // MIDI-only export must remain byte-identical without allocating an audio snapshot.
            auto oldMidi=juce::File::getCurrentWorkingDirectory().getChildFile("reference-capture.mid");
            auto newMidi=juce::File::getCurrentWorkingDirectory().getChildFile("optimised-capture.mid");
            require(original->exportCapture(oldMidi,true,error)&&optimised->exportCapture(newMidi,true,error),"reference MIDI exports");
            juce::MemoryBlock oldBytes,newBytes;oldMidi.loadFileAsData(oldBytes);newMidi.loadFileAsData(newBytes);
            require(oldBytes==newBytes,"identical captured MIDI after optimisation");
            original->setPlayHead(nullptr);optimised->setPlayHead(nullptr);
        }
        std::cout<<"Full processor peak null error: "<<fullPeakError<<"\n";
        require(fullPeakError<1e-6,"unchanged processor audio with automation and scenes");
        processor.releaseResources();processor.setPlayHead(nullptr);
        std::cout<<"PASS MIDI, three-rack routing state, legacy migration, GUI attachments, reorder and 15 page/rack snapshots\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n";return 1; }
}
