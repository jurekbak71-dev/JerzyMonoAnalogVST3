#include "PluginProcessor.h"
#include "PluginEditor.h"

std::vector<ParameterSpec> parameterSpecs() {
    const jerzy::Parameters p;
    std::vector<ParameterSpec> out;
    auto choices=[](const char* text) { return juce::StringArray::fromTokens(text,"|",""); };
#define BOOL(f,l,g) out.push_back({#f,l,g,1,0,1,double(p.f),1,{}});
#define FLOAT(f,a,b,s,l,g) out.push_back({#f,l,g,0,a,b,p.f,s,{}});
#define INT(f,a,b,l,g) out.push_back({#f,l,g,2,a,b,double(p.f),1,{}});
#define CHOICE(f,l,g,c) out.push_back({#f,l,g,3,0,0,double(p.f),1,choices(c)});
#define CHOICE_DEN(f,l,g) out.push_back({#f,l,g,3,0,0,1,1,choices("2|4|8|16")});
#include "Parameters.def"
#undef BOOL
#undef FLOAT
#undef INT
#undef CHOICE
#undef CHOICE_DEN
    for(int i=0;i<6;++i) out.push_back({"order"+juce::String(i),"FX slot "+juce::String(i+1),3,3,0,0,double(i),1,choices("Drive / Comp|Chorus|Delay|Granular|Reverb|Width")});
    // Keep original FX IDs for BASS; append independent AMBIENT and MASTER IDs.
    const auto original=out;
    for(const auto& prefix:juce::StringArray{"pad_","master_"}) for(auto spec:original) if(spec.group==3) {
        spec.id=prefix+spec.id;
        spec.label=(prefix=="pad_"?"Ambient ":"Master ")+spec.label;
        out.push_back(std::move(spec));
    }
    out.push_back({"masterFX","Master FX amount",4,0,0,1,0,1,{}});
    return out;
}

juce::AudioProcessorValueTreeState::ParameterLayout BassAmbientProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    for(const auto& s:parameterSpecs()) {
        juce::ParameterID id{s.id,1};
        if(s.kind==1) result.add(std::make_unique<juce::AudioParameterBool>(id,s.label,s.initial>.5));
        else if(s.kind==2) result.add(std::make_unique<juce::AudioParameterInt>(id,s.label,int(s.minimum),int(s.maximum),int(s.initial)));
        else if(s.kind==3) result.add(std::make_unique<juce::AudioParameterChoice>(id,s.label,s.choices,int(s.initial)));
        else result.add(std::make_unique<juce::AudioParameterFloat>(id,s.label,juce::NormalisableRange<float>(float(s.minimum),float(s.maximum),0,float(s.skew)),float(s.initial)));
    }
    return result;
}

BassAmbientProcessor::BassAmbientProcessor()
    : AudioProcessor(BusesProperties().withOutput("Stereo output",juce::AudioChannelSet::stereo(),true)),
      state(*this,nullptr,"JerzyBassAmbient",layout()) {
    for(const auto& s:parameterSpecs()) {values.push_back(state.getRawParameterValue(s.id));hostParameters.push_back(state.getParameter(s.id));}
    for(auto& scene:sceneData)scene.resize(values.size());
    instrument.observer=[](void* context,int n,int ch,double v,double duration) {
        auto* self=static_cast<BassAmbientProcessor*>(context);
        self->capture.note(n,ch,v,duration<0?duration:duration*self->captureTempo/60);
    };instrument.observerContext=this;
}

jerzy::Parameters BassAmbientProcessor::readParameters() const {
    jerzy::Parameters p;size_t i=0;
#define BOOL(f,l,g) p.f=values[i++]->load()>.5f;
#define FLOAT(f,a,b,s,l,g) p.f=values[i++]->load();
#define INT(f,a,b,l,g) p.f=int(std::lround(values[i++]->load()));
#define CHOICE(f,l,g,c) p.f=int(std::lround(values[i++]->load()));
#define CHOICE_DEN(f,l,g) { constexpr int d[]{2,4,8,16};p.f=d[juce::jlimit(0,3,int(std::lround(values[i++]->load())))]; }
#include "Parameters.def"
#undef BOOL
#undef FLOAT
#undef INT
#undef CHOICE
#undef CHOICE_DEN
    for(int k=0;k<6;++k) p.order[size_t(k)]=int(std::lround(values[i++]->load()));
    for(auto* rack:{&p.padRack,&p.masterRack}) {
#define BOOL(f,l,g) rack->f=values[i++]->load()>.5f;
#define FLOAT(f,a,b,s,l,g) rack->f=values[i++]->load();
#define CHOICE(f,l,g,c) rack->f=int(std::lround(values[i++]->load()));
#include "FXParameters.def"
#undef BOOL
#undef FLOAT
#undef CHOICE
        for(int k=0;k<6;++k) rack->order[size_t(k)]=int(std::lround(values[i++]->load()));
    }
    p.masterFX=values[i++]->load();
    return p;
}

void BassAmbientProcessor::prepareToPlay(double sampleRate,int) {
    sr=sampleRate;instrument.prepare(sr);current=readParameters();localPPQ=0;resetRequested=false;capture.prepare(sr);haveScenePPQ=false;
}
bool BassAmbientProcessor::isBusesLayoutSupported(const BusesLayout& buses) const {
    return buses.getMainInputChannelSet().isDisabled()&&buses.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();
}

void BassAmbientProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    if(resetRequested.exchange(false)) instrument.reset();
    audio.clear();auto target=readParameters();
    // Structural changes are immediate; continuous controls ramp at audio rate.
    auto previous=current;current=target;
#define BOOL(f,l,g)
#define FLOAT(f,a,b,s,l,g) current.f=previous.f;
#define INT(f,a,b,l,g)
#define CHOICE(f,l,g,c)
#define CHOICE_DEN(f,l,g)
#include "Parameters.def"
#undef BOOL
#undef FLOAT
#undef INT
#undef CHOICE
#undef CHOICE_DEN
    current.padRack=previous.padRack;current.masterRack=previous.masterRack;
    // Adopt discrete fields immediately, retaining only continuous values for the ramp.
    for(int rack=0;rack<2;++rack) {
        auto& c=rack==0?current.padRack:current.masterRack;
        const auto& t=rack==0?target.padRack:target.masterRack;
#define BOOL(f,l,g) c.f=t.f;
#define FLOAT(f,a,b,s,l,g)
#define CHOICE(f,l,g,c_) c.f=t.f;
#include "FXParameters.def"
#undef BOOL
#undef FLOAT
#undef CHOICE
        c.order=t.order;
    }
    current.masterFX=previous.masterFX;
    double tempo=target.previewTempo,ppq=localPPQ;bool playing=false;int num=4,den=4;
    if(auto* head=getPlayHead()) if(auto position=head->getPosition()) {
        if(auto t=position->getBpm()) if(position->getIsPlaying()) tempo=*t;
        if(auto t=position->getPpqPosition()) ppq=*t;
        if(auto t=position->getTimeSignature()) { num=t->numerator;den=t->denominator; }
        playing=position->getIsPlaying();
        // While stopped, retain a local clock for live playing / preview.
        if(!playing) ppq=localPPQ;
    }
    tempo=juce::jlimit(20.0,400.0,tempo);captureTempo=tempo;hostNum=num;hostDen=den;
    if(playing) {auditionBass=false;auditionPad=false;}
    hostRunning=playing;
    instrument.beginBlock(ppq,tempo,playing);
    // Preview controls belong to the editor, never to saved host automation.
    instrument.setPreview(auditionBass.load(),auditionPad.load(),target.previewNote,target,ppq);
    if(!playing&&requestedScene.load()>=0) {applyQueuedScene();target=readParameters();}
    const double increment=tempo/(60*sr),smoothing=1-std::exp(-1/(sr*.02));
    auto event=midi.cbegin();const auto end=midi.cend();float maximum=0;
    for(int sample=0;sample<audio.getNumSamples();++sample) {
#define BOOL(f,l,g)
#define FLOAT(f,a,b,s,l,g) current.f+=(target.f-current.f)*smoothing;
#define INT(f,a,b,l,g)
#define CHOICE(f,l,g,c)
#define CHOICE_DEN(f,l,g)
#include "Parameters.def"
#undef BOOL
#undef FLOAT
#undef INT
#undef CHOICE
#undef CHOICE_DEN
        for(int rack=0;rack<2;++rack) {
            auto& c=rack==0?current.padRack:current.masterRack;
            const auto& t=rack==0?target.padRack:target.masterRack;
#define BOOL(f,l,g)
#define FLOAT(f,a,b,s,l,g) c.f+=(t.f-c.f)*smoothing;
#define CHOICE(f,l,g,c_)
#include "FXParameters.def"
#undef BOOL
#undef FLOAT
#undef CHOICE
        }
        current.masterFX+=(target.masterFX-current.masterFX)*smoothing;
        const double position=ppq+sample*increment;
        double bar=double(num)*4/std::max(1,den);
        if(playing&&requestedScene.load()>=0&&(!haveScenePPQ||std::floor(position/bar)!=std::floor(previousScenePPQ/bar))) {
            applyQueuedScene();target=readParameters();
            // Adopt switches immediately at the bar boundary; smooth continuous parameters below.
#define BOOL(f,l,g) current.f=target.f;
#define FLOAT(f,a,b,s,l,g)
#define INT(f,a,b,l,g) current.f=target.f;
#define CHOICE(f,l,g,c) current.f=target.f;
#define CHOICE_DEN(f,l,g) current.f=target.f;
#include "Parameters.def"
#undef BOOL
#undef FLOAT
#undef INT
#undef CHOICE
#undef CHOICE_DEN
        }
        previousScenePPQ=position;haveScenePPQ=true;
        while(event!=end&&(*event).samplePosition<=sample) {
            auto m=(*event).getMessage();int ch=m.getChannel();
            if(m.isNoteOn()) instrument.noteOn(m.getNoteNumber(),ch,m.getFloatVelocity(),current,position);
            else if(m.isNoteOff()) instrument.noteOff(m.getNoteNumber(),ch,current,position);
            else if(m.isPitchWheel()) instrument.pitchBend(ch,m.getPitchWheelValue());
            else if(m.isController()) instrument.controller(ch,m.getControllerNumber(),m.getControllerValue(),current,position);
            else if(m.isAllSoundOff()) instrument.reset();
            else if(m.isAllNotesOff()) instrument.allOff();
            ++event;
        }
        auto renderParameters=current;
        if(!playing) {renderParameters.bass=current.bass||auditionBass.load();renderParameters.pad=current.pad||auditionPad.load();}
        auto output=instrument.next(renderParameters,position,num,den);
        capture.push(float(output.l),float(output.r),tempo);
        audio.setSample(0,sample,float(output.l));audio.setSample(1,sample,float(output.r));
        maximum=std::max(maximum,float(std::max(std::abs(output.l),std::abs(output.r))));
    }
    localPPQ=ppq+audio.getNumSamples()*increment;midi.clear();
    rootDisplay=instrument.currentRoot();peak=maximum;
}

void BassAmbientProcessor::getStateInformation(juce::MemoryBlock& output) {
    auto tree=state.copyState();tree.setProperty("schemaVersion",3,nullptr);
    for(int i=tree.getNumChildren()-1;i>=0;--i)if(tree.getChild(i).hasType("SCENES"))tree.removeChild(i,nullptr);
    juce::ValueTree scenes("SCENES");
    for(int k=0;k<4;++k)if(sceneReady[size_t(k)].load()) {
        juce::ValueTree scene("SCENE");scene.setProperty("slot",k,nullptr);
        for(size_t i=0;i<hostParameters.size();++i)scene.setProperty(hostParameters[i]->paramID,sceneData[size_t(k)][i],nullptr);
        scenes.appendChild(scene,nullptr);
    }
    tree.appendChild(scenes,nullptr);
    if(auto xml=tree.createXml()) copyXmlToBinary(*xml,output);
}
void BassAmbientProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(state.state.getType())) {
        auto tree=juce::ValueTree::fromXml(*xml);
        // Old sessions used a shared FX configuration: duplicate it to AMBIENT.
        if(int(tree.getProperty("schemaVersion",1))<2) {
            for(const auto& spec:parameterSpecs()) if(spec.id.startsWith("pad_")) {
                const auto source=tree.getChildWithProperty("id",spec.id.substring(4));
                auto copy=source.isValid()?source.createCopy():juce::ValueTree("PARAM");
                copy.setProperty("id",spec.id,nullptr);
                if(!source.isValid()) copy.setProperty("value",spec.initial,nullptr);
                tree.appendChild(copy,nullptr);
            }
            for(const auto& spec:parameterSpecs()) if(spec.id.startsWith("master_")||spec.id=="masterFX") {
                juce::ValueTree child("PARAM");child.setProperty("id",spec.id,nullptr);
                child.setProperty("value",spec.initial,nullptr);tree.appendChild(child,nullptr);
            }
        }
        for(int k=0;k<4;++k) {
            sceneReady[size_t(k)]=false;
            auto scene=tree.getChildWithName("SCENES").getChildWithProperty("slot",k);
            if(scene.isValid()) {
                for(size_t i=0;i<hostParameters.size();++i)sceneData[size_t(k)][i]=float(scene.getProperty(hostParameters[i]->paramID,hostParameters[i]->getDefaultValue()));
                sceneReady[size_t(k)]=true;
            }
        }
        state.replaceState(tree);
        lastLocks={{state.getRawParameterValue("lockNotes")->load()>.5f,state.getRawParameterValue("lockRhythm")->load()>.5f,state.getRawParameterValue("lockTimbre")->load()>.5f}};
        resetRequested=true;requestedScene=-1;auditionBass=false;auditionPad=false;
    }
}
void BassAmbientProcessor::applyPreset(int preset) {
    auto set=[&](const char* id,float v) {
        if(auto* parameter=state.getParameter(id)) { parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(v));parameter->endChangeGesture(); }
    };
    // Explicit factory snapshots; all remaining controls return to defaults.
    for(const auto& s:parameterSpecs()) set(s.id.toRawUTF8(),float(s.initial));
    if(preset==0) { set("style",0);set("model",2);set("cutoff",1800);set("density",.85f); }
    if(preset==1) { set("style",2);set("model",0);set("slide",.5f);set("resonance",.7f);set("cutoff",700);set("bassFX",.25f); }
    if(preset==2) { set("style",4);set("model",1);set("bassDecay",.8f);set("bassSustain",.65f);set("bassRelease",.3f);set("drive",.4f); }
    if(preset==3) { set("bass",0);set("pad",1);set("padMode",1);set("engine1",0);set("engine2",2);set("pad_reverbMix",.5f);set("padAttack",3);set("padRelease",9); }
    if(preset==4) { set("bass",0);set("pad",1);set("padMode",2);set("engine1",3);set("engine2",4);set("pad_fxGrain",1);set("pad_grainPitch",7);set("pad_damage",.35f);set("motion",.7f); }
    if(preset==5) { set("bass",1);set("pad",1);set("model",2);set("padLevel",.25f);set("padAttack",4);set("padRelease",8); }
    resetRequested=true;
}
void BassAmbientProcessor::setParameter(const juce::String& id,float value) {
    if(auto* p=state.getParameter(id)) {p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(value));p->endChangeGesture();}
}
void BassAmbientProcessor::synchroniseLocks() {
    const char* locks[]{"lockNotes","lockRhythm","lockTimbre"};
    const char* seeds[]{"phraseSeed","rhythmSeed","timbreSeed"};
    for(size_t i=0;i<3;++i) {
        bool locked=state.getRawParameterValue(locks[i])->load()>.5f;
        if(locked&&!lastLocks[i])setParameter(seeds[i],state.getRawParameterValue("seed")->load());
        lastLocks[i]=locked;
    }
}
void BassAmbientProcessor::generate(bool mutation) {
    synchroniseLocks();getStateInformation(undoState);
    auto random=juce::Random::getSystemRandom().nextInt(65535)+1;
    if(mutation)setParameter("mutation",float((int(state.getRawParameterValue("mutation")->load())+1)%65536));
    else {setParameter("seed",float(random));setParameter("mutation",0);}
    if(!mutation) {
        if(!readParameters().lockRhythm) {setParameter("density",.45f+juce::Random::getSystemRandom().nextFloat()*.5f);setParameter("rhythmDensity",.4f+juce::Random::getSystemRandom().nextFloat()*.5f);}
        if(!readParameters().lockNotes)setParameter("movement",.2f+juce::Random::getSystemRandom().nextFloat()*.7f);
    }
    if(!readParameters().lockTimbre) {
        float strength=mutation?.1f:.4f;
        auto change=[&](const char* id) {float v=state.getRawParameterValue(id)->load();setParameter(id,juce::jlimit(0.0f,1.0f,v+(juce::Random::getSystemRandom().nextFloat()*2-1)*strength));};
        change("padBlend");change("motion");change("krellFM");
    }
    if(!readParameters().lockFX) {
        auto v=state.getRawParameterValue("pad_chorusDepth")->load();
        setParameter("pad_chorusDepth",juce::jlimit(0.0f,1.0f,v+(juce::Random::getSystemRandom().nextFloat()-.5f)*(mutation?.15f:.5f)));
    }
}
void BassAmbientProcessor::undoGeneration() {
    if(undoState.getSize()) {auto previous=undoState;juce::MemoryBlock now;getStateInformation(now);setStateInformation(previous.getData(),int(previous.getSize()));undoState=now;}
}
bool BassAmbientProcessor::hasScene(int index) const {return index>=0&&index<4&&sceneReady[size_t(index)].load();}
void BassAmbientProcessor::saveScene(int index) {
    if(index<0||index>=4)return;
    // Saving snapshots is a UI operation; suspend protects the preallocated scene array.
    const juce::ScopedLock lock(getCallbackLock());
    sceneReady[size_t(index)]=false;
    for(size_t i=0;i<hostParameters.size();++i)sceneData[size_t(index)][i]=hostParameters[i]->getValue();
    sceneReady[size_t(index)]=true;
}
void BassAmbientProcessor::applyQueuedScene() {
    int scene=requestedScene.exchange(-1);if(!hasScene(scene))return;
    for(size_t i=0;i<hostParameters.size();++i)hostParameters[i]->setValueNotifyingHost(sceneData[size_t(scene)][i]);
    activeScene=scene;
}
bool BassAmbientProcessor::exportCapture(const juce::File& file,bool asMidi,juce::String& error) {
    PerformanceCapture::Snapshot snap;
    if(!capture.snapshot(snap,16)) {error="Capture unavailable or overwritten. Play a phrase and retry.";return false;}
    auto stream=file.createOutputStream();if(!stream||!stream->openedOk()){error="Cannot create output file.";return false;}
    stream->setPosition(0);stream->truncate();
    if(asMidi) {
        juce::MidiMessageSequence sequence;
        auto tempoMessage=juce::MidiMessage::tempoMetaEvent(int(60000000/snap.bpm));sequence.addEvent(tempoMessage);
        const double end=(snap.endBeat-snap.startBeat)*960;
        std::array<std::array<bool,128>,16> open{};
        for(const auto& n:snap.notes) {
            int ch=juce::jlimit(1,16,n.channel),note=juce::jlimit(0,127,n.note);
            double tick=(n.beat-snap.startBeat)*960;
            if(n.duration>=0&&n.velocity>0) {
                double off=tick+n.duration*960;
                if(off<=0)continue;
                auto on=juce::MidiMessage::noteOn(ch,note,n.velocity);on.setTimeStamp(std::max(0.0,tick));sequence.addEvent(on);
                auto stop=juce::MidiMessage::noteOff(ch,note);stop.setTimeStamp(std::min(end,off));sequence.addEvent(stop);
            } else {
                if(tick<0) {open[size_t(ch-1)][size_t(note)]=n.velocity>0;continue;}
                auto m=n.velocity>0?juce::MidiMessage::noteOn(ch,note,n.velocity):juce::MidiMessage::noteOff(ch,note);
                m.setTimeStamp(tick);sequence.addEvent(m);
            }
        }
        for(int ch=1;ch<=16;++ch)for(int n=0;n<128;++n) {
            if(open[size_t(ch-1)][size_t(n)]) {auto m=juce::MidiMessage::noteOn(ch,n,.7f);m.setTimeStamp(0);sequence.addEvent(m);}
            auto stop=juce::MidiMessage::noteOff(ch,n);stop.setTimeStamp(end);sequence.addEvent(stop);
        }
        sequence.sort();sequence.updateMatchedPairs();juce::MidiFile midi;midi.setTicksPerQuarterNote(960);midi.addTrack(sequence);
        if(!midi.writeTo(*stream)){error="MIDI export failed.";return false;}
    } else {
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),snap.sr,2,24,{},0));
        if(!writer){error="WAV writer unavailable.";return false;}stream.release();
        if(!writer->writeFromAudioSampleBuffer(snap.audio,0,snap.audio.getNumSamples())){error="WAV export failed.";return false;}
    }
    return true;
}
juce::AudioProcessorEditor* BassAmbientProcessor::createEditor() { return new BassAmbientEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BassAmbientProcessor(); }
