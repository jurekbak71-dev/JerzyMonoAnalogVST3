#include "PluginProcessor.h"
#include "SequencerClock.h"
#include "PluginEditor.h"

namespace
{
class HostAutomatableFloat final : public juce::AudioParameterFloat
{
public:
    using juce::AudioParameterFloat::AudioParameterFloat;
    bool isAutomatable() const override { return true; }
};
class HostAutomatableBool final : public juce::AudioParameterBool
{
public:
    using juce::AudioParameterBool::AudioParameterBool;
    bool isAutomatable() const override { return true; }
};
class HostAutomatableChoice final : public juce::AudioParameterChoice
{
public:
    using juce::AudioParameterChoice::AudioParameterChoice;
    bool isAutomatable() const override { return true; }
};
}

JerzyMonoAnalogAudioProcessor::JerzyMonoAnalogAudioProcessor()
: AudioProcessor(BusesProperties().withInput("Audio In", juce::AudioChannelSet::stereo(), false).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  apvts(*this, nullptr, "PARAMS", createLayout()) {
    for(int track=0;track<2;++track)for(int step=0;step<64;++step)
        stepGateParameters[(size_t)track][(size_t)step]=apvts.getRawParameterValue("gateT"+juce::String(track+1)+"S"+juce::String(step));
}

void JerzyMonoAnalogAudioProcessor::prepareToPlay(double sr, int bs)
{
    currentSampleRate = sr;
    engine.prepare(sr, bs);gateLfo.prepare(sr,0x7115u);tracks={};previousPpq=0;previousHostPlaying=false;
    fxChain.prepare(sr, bs);
    resetArpState();
    arpHeldNotes.ensureStorageAllocated(128);
    arpLatchedNotes.ensureStorageAllocated(128);
    physicalHeldNotes.ensureStorageAllocated(128);
    inputMidiScratch.ensureSize(4096);
    generatedMidiScratch.ensureSize(4096);
    gridSamplesToNext=0.0;
    gridCurrentStepSamples=0.0;
    arpCurrentStepSamples=0.0;
    lastArpHostStep=-1; lastGridHostStep=-1;
    gridGlobalStep=0;
    gridCurrentNote=-1;
    gridArpNote=-1; launchArpNote=-1; launchCurrentNote=-1;
    gridCurrentNoteRouted=false; launchCurrentNoteRouted=false;
    launchPressedNote.store(-1); gridArpNeedsRestart=false;
    gridStepNote=-1;gridStepActive=false;gridRatchetIndex=0;gridRatchetCount=1;
    gridPlayColumn.store(-1);
}

void JerzyMonoAnalogAudioProcessor::resetArpState()
{
    arpSamplesToNext = 0.0;
    arpStep = 0;
    arpCurrentNote = -1;
    arpUpDownPos = 0;
    arpLastRandomIndex = -1;
    arpHeldNotes.clear();
    arpLatchedNotes.clear();
    physicalHeldNotes.clear();
}

void JerzyMonoAnalogAudioProcessor::setGridStep(int bank,int column,int row,bool on)
{
    bank=juce::jlimit(0,7,bank); column=juce::jlimit(0,7,column); row=juce::jlimit(0,7,row);
    auto& pattern=gridTrack.load()==0?gridPattern:gridPattern2;
    if(on)
    {
        for(int r=0;r<8;++r)
            pattern[(size_t)(bank*64 + r*8 + column)].store(0);
    }
    pattern[(size_t)(bank*64 + row*8 + column)].store(on?1:0);
}

bool JerzyMonoAnalogAudioProcessor::getGridStep(int bank,int column,int row) const
{
    bank=juce::jlimit(0,7,bank); column=juce::jlimit(0,7,column); row=juce::jlimit(0,7,row);
    return (gridTrack.load()==0?gridPattern:gridPattern2)[(size_t)(bank*64 + row*8 + column)].load()!=0;
}

void JerzyMonoAnalogAudioProcessor::clearGridBank(int bank)
{
    bank=juce::jlimit(0,7,bank);
    for(int i=0;i<64;++i) (gridTrack.load()==0?gridPattern:gridPattern2)[(size_t)(bank*64+i)].store(0);
}

int JerzyMonoAnalogAudioProcessor::getGridStepGate(int bank,int column) const
{
    return (int)stepGateParameters[(size_t)gridTrack.load()][(size_t)(juce::jlimit(0,7,bank)*8+juce::jlimit(0,7,column))]->load();
}

int JerzyMonoAnalogAudioProcessor::gridRootMidiFromChoice() const
{
    return juce::jlimit(24,84,24 + getChoiceIndex("gridRoot"));
}

int JerzyMonoAnalogAudioProcessor::gridNoteForRow(int row) const
{
    static constexpr int scales[8][8] =
    {
        {0,1,2,3,4,5,6,7},          // Chromatic
        {0,2,4,5,7,9,11,12},        // Major
        {0,2,3,5,7,8,10,12},        // Natural Minor
        {0,2,3,5,7,9,10,12},        // Dorian
        {0,1,3,5,7,8,10,12},        // Phrygian
        {0,2,4,5,7,9,10,12},        // Mixolydian
        {0,2,4,7,9,12,14,16},       // Major Pentatonic
        {0,3,5,7,10,12,15,17}       // Minor Pentatonic
    };
    const int scale = juce::jlimit(0,7,getChoiceIndex("gridScale"));
    const int degree = juce::jlimit(0,7,7-row);
    const int gridRoot=gridRootMidiFromChoice();
    const bool midiTranspose=apvts.getRawParameterValue("gridMidiTrigger")->load()>0.5f && gridMidiRunning.load();
    const int base=midiTranspose?gridTriggerNote.load():gridRoot;
    const int octave=getChoiceIndex("gridOctave")-2;
    return juce::jlimit(0,127,base+scales[scale][degree]+12*octave);
}

void JerzyMonoAnalogAudioProcessor::launchPadNoteOn(int padIndex)
{
    const int note=juce::jlimit(0,127,gridRootNote.load()+juce::jlimit(0,63,padIndex));
    launchPressedNote.store(note);
}

void JerzyMonoAnalogAudioProcessor::launchPadNoteOff(int padIndex)
{
    const int note=juce::jlimit(0,127,gridRootNote.load()+juce::jlimit(0,63,padIndex));
    if(launchPressedNote.load()==note)
        launchPressedNote.store(-1);
}

void JerzyMonoAnalogAudioProcessor::startGridNote(int note,float velocity,int sampleOffset,juce::MidiBuffer& generatedMidi,double hostPpq,bool routeToArp)
{
    gridCurrentNote=note;
    gridCurrentNoteRouted=routeToArp;
    if(routeToArp)
    {
        gridArpNote=note;
        arpGridPpqOrigin=hostPpq;
        gridArpNeedsRestart=true;
    }
    else
    {
        engine.noteOn(note,velocity);
        generatedMidi.addEvent(juce::MidiMessage::noteOn(1,note,velocity),sampleOffset);
    }
}

void JerzyMonoAnalogAudioProcessor::stopGridNote(int sampleOffset,juce::MidiBuffer& generatedMidi)
{
    if(gridCurrentNote<0) return;
    if(gridCurrentNoteRouted) gridArpNote=-1;
    else
    {
        engine.noteOff(gridCurrentNote);
        generatedMidi.addEvent(juce::MidiMessage::noteOff(1,gridCurrentNote),sampleOffset);
    }
    gridCurrentNote=-1;
    gridCurrentNoteRouted=false;
}

void JerzyMonoAnalogAudioProcessor::stopLaunchNote(int,juce::MidiBuffer&)
{
    if(launchCurrentNote<0) return;
    if(launchCurrentNoteRouted) launchArpNote=-1;
    else engine.noteOff(launchCurrentNote);
    launchCurrentNote=-1;
    launchCurrentNoteRouted=false;
}

void JerzyMonoAnalogAudioProcessor::processLaunchPadSample(int sampleOffset,juce::MidiBuffer& generatedMidi,double hostPpq,bool routeToArp)
{
    const int pressed=gridMode.load()==(int)GridMode::launch?launchPressedNote.load():-1;
    if(pressed==launchCurrentNote && (pressed<0 || launchCurrentNoteRouted==routeToArp)) return;
    stopLaunchNote(sampleOffset,generatedMidi);
    if(pressed<0) return;
    launchCurrentNote=pressed;
    launchCurrentNoteRouted=routeToArp;
    if(routeToArp)
    {
        launchArpNote=pressed;
        arpGridPpqOrigin=hostPpq;
        gridArpNeedsRestart=true;
    }
    else engine.noteOn(pressed,0.95f);
}

void JerzyMonoAnalogAudioProcessor::stopTrack(int track,int offset,juce::MidiBuffer& midi)
{
    auto& state=tracks[(size_t)track];
    if(state.note<0)return;
    if(state.routed) {if(track==0)gridArpNote=-1;else gridArpNote2=-1;}
    else {
        if(voiceMode==1)engine.oscillatorNoteOff(track);else engine.noteOff(state.note,track+1);
        midi.addEvent(juce::MidiMessage::noteOff(track+1,state.note),offset);
    }
    state.note=-1;state.routed=false;
}
void JerzyMonoAnalogAudioProcessor::processGridSequencerSample(double bpm,int offset,juce::MidiBuffer& midi,double ppq,bool hasPpq,bool playing)
{
    static constexpr double q[]={4,2,1,.5,.25,.125,2.0/3,1.0/3,1.0/6,1.5,.75,.375};
    const bool run=gridSettings.armed && getGridMode()==GridMode::sequencer
                   && (!gridSettings.midiTrigger || gridMidiRunning.load())
                   && (!gridSettings.hostSync || (hasPpq && playing));
    const double delta=bpm/(60.0*currentSampleRate);
    for(int track=0;track<2;++track) {
        auto& state=tracks[(size_t)track];
        if(!run || !state.enabled || (track==1 && voiceMode==0)) {
            stopTrack(track,offset,midi);state.lastStep=-1;state.beat=0;
            playColumns[(size_t)track].store(-1);playBanks[(size_t)track].store(-1);continue;
        }
        const double beat=gridSettings.hostSync?ppq-(gridSettings.midiTrigger?gridHostPpqOrigin:0):state.beat;
        const double quarter=q[juce::jlimit(0,11,state.division)];
        const auto time=jerzy::gridTime(beat,quarter,state.swing);
        const bool fresh=time.step!=state.lastStep;
        const int step=jerzy::gridIndex(time.step,state.length,state.direction,track);
        const int ratchets=gridSettings.ratchets;
        const int ratchet=juce::jmin(ratchets-1,(int)(time.phase*ratchets));
        if(state.note>=0 && beat+1e-10>=state.endBeat)stopTrack(track,offset,midi);
        if(fresh) {
            state.lastStep=time.step;state.ratchet=-1;state.stepNote=-1;
            playColumns[(size_t)track].store(step%8);playBanks[(size_t)track].store(step/8);
            const auto& pattern=track==0?gridPattern:gridPattern2;
            if((jerzy::gridHash(time.step,track)%10000u)<(uint32_t)(gridSettings.probability*10000))
                for(int row=0;row<8;++row)if(pattern[(size_t)(step/8*64+row*8+step%8)].load()) {state.stepNote=gridNoteForRow(row);break;}
        }
        if(state.stepNote>=0 && (fresh || ratchet!=state.ratchet)) {
            state.ratchet=ratchet;
            // Rests do not cancel long gates. A new note replaces only its own track.
            stopTrack(track,offset,midi);state.note=state.stepNote;state.routed=gridSettings.routeToArp;
            if(state.routed) {
                if(track==0)gridArpNote=state.note;else gridArpNote2=state.note;
                arpGridPpqOrigin=ppq;gridArpNeedsRestart=true;
            } else {
                if(voiceMode==1)engine.oscillatorNoteOn(track,state.note,gridSettings.velocity);
                else engine.noteOn(state.note,gridSettings.velocity,track+1);
                midi.addEvent(juce::MidiMessage::noteOn(track+1,state.note,gridSettings.velocity),offset);
            }
            const int gate=(int)stepGateParameters[(size_t)track][(size_t)step]->load();
            const bool exact=gridSettings.noteGate;
            const double duration=exact?quarter*juce::jlimit(1.0,16.0,gate+gateModulation*gateModDepth):time.duration*gridSettings.gate;
            // Ratchets divide the selected duration; the next ratchet retriggers it.
            state.endBeat=beat+duration/ratchets;
        }
        state.beat+=delta;
    }
}
int JerzyMonoAnalogAudioProcessor::getChoiceIndex(const char* id) const
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
        return p->getIndex();
    return (int) apvts.getRawParameterValue(id)->load();
}

bool JerzyMonoAnalogAudioProcessor::arpRhythmGate(int rhythm, int step) const
{
    switch (rhythm)
    {
        case 1: return (step % 2) == 0;                           // every 2
        case 2: { static constexpr int m[8]={1,0,1,1,0,1,0,1}; return m[step & 7] != 0; } // 3-3-2 feel
        case 3: { static constexpr int m[8]={1,1,0,1,0,1,1,0}; return m[step & 7] != 0; } // syncopated
        case 4: { static constexpr int m[8]={1,0,0,1,0,0,1,0}; return m[step & 7] != 0; } // clave
        case 5: { static constexpr int m[8]={1,0,1,0,0,1,0,0}; return m[step & 7] != 0; } // 5 over 8
        case 6: { static constexpr int m[8]={1,0,1,1,0,1,1,0}; return m[step & 7] != 0; } // rolling
        default:return true;
    }
}

int JerzyMonoAnalogAudioProcessor::chooseArpNote(int pattern, int step)
{
    std::array<int,132> notes{};
    int n=0;
    for(int note:arpLatchedNotes) notes[(size_t)n++]=note;
    if(gridArpNote2>=0 && !arpLatchedNotes.contains(gridArpNote2) && gridArpNote2!=gridArpNote && gridArpNote2!=launchArpNote)notes[(size_t)n++]=gridArpNote2;
    if(gridArpNote>=0 && !arpLatchedNotes.contains(gridArpNote)) notes[(size_t)n++]=gridArpNote;
    if(launchArpNote>=0 && !arpLatchedNotes.contains(launchArpNote) && launchArpNote!=gridArpNote) notes[(size_t)n++]=launchArpNote;
    if(n==0) return -1;
    // Keep insertion order for "As Played"; pitch modes use sorted notes.
    if (pattern != 4) std::sort(notes.begin(), notes.begin()+n);

    switch (pattern)
    {
        case 1: return notes[(n - 1 - (step % n) + n) % n]; // down
        case 2:
        {
            if (n == 1) return notes[0];
            const int span = n * 2 - 2;
            const int p = step % span;
            return notes[p < n ? p : span - p];
        }
        case 3:
        {
            int index=0;
            if(n>1){std::uniform_int_distribution<int>d(0,n-2);index=d(arpRng);if(index>=arpLastRandomIndex)++index;}
            arpLastRandomIndex=index;
            return notes[index];
        }
        case 4: return notes[step % n]; // as played fallback
        case 5: return notes[(step / 2) % n]; // double up
        case 6: return notes[(n - 1 - ((step / 2) % n) + n) % n]; // double down
        case 7: // inclusive up/down
        {
            if(n==1)return notes[0];
            const int span=n*2;const int p=step%span;
            return notes[p<n?p:span-1-p];
        }
        case 8: // converge from both ends
        {
            const int low=(step/2)%( (n+1)/2 );const int high=n-1-low;return notes[(step&1)?high:low];
        }
        default:return notes[step % n]; // up
    }
}

int JerzyMonoAnalogAudioProcessor::arpNoteCount() const
{
    int count=arpLatchedNotes.size();
    if(gridArpNote2>=0 && !arpLatchedNotes.contains(gridArpNote2) && gridArpNote2!=gridArpNote && gridArpNote2!=launchArpNote)++count;
    if(gridArpNote>=0 && !arpLatchedNotes.contains(gridArpNote)) ++count;
    if(launchArpNote>=0 && !arpLatchedNotes.contains(launchArpNote) && launchArpNote!=gridArpNote) ++count;
    return count;
}

bool JerzyMonoAnalogAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto in=l.getMainInputChannelSet(),out=l.getMainOutputChannelSet();
    return (out==juce::AudioChannelSet::stereo() || out==juce::AudioChannelSet::mono())
        && (in.isDisabled() || in==juce::AudioChannelSet::mono() || in==juce::AudioChannelSet::stereo());
}

static jerzy::BandLimitedOscillator::Wave waveFrom(float v)
{
    switch ((int) v) { case 0: return jerzy::BandLimitedOscillator::Wave::sine; case 1: return jerzy::BandLimitedOscillator::Wave::triangle; case 2: return jerzy::BandLimitedOscillator::Wave::saw; default: return jerzy::BandLimitedOscillator::Wave::square; }
}
static jerzy::AnalogLFO::Wave lfoWaveFrom(float v)
{
    switch ((int) v) { case 0: return jerzy::AnalogLFO::Wave::sine; case 1: return jerzy::AnalogLFO::Wave::triangle; case 2: return jerzy::AnalogLFO::Wave::saw; case 3: return jerzy::AnalogLFO::Wave::square; case 5: return jerzy::AnalogLFO::Wave::randomSquare; default: return jerzy::AnalogLFO::Wave::sampleHold; }
}

void JerzyMonoAnalogAudioProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    generatedMidiScratch.clear();
    // Input and output may alias: read each input sample before overwriting output.

    jerzy::MonoParameters p;
    p.osc1Wave = waveFrom(apvts.getRawParameterValue("osc1Wave")->load());
    p.osc2Wave = waveFrom(apvts.getRawParameterValue("osc2Wave")->load());
    p.subWave = waveFrom(apvts.getRawParameterValue("subWave")->load()) == jerzy::BandLimitedOscillator::Wave::sine ? jerzy::BandLimitedOscillator::Wave::sine : jerzy::BandLimitedOscillator::Wave::square;
    p.osc1Octave = (int) apvts.getRawParameterValue("osc1Oct")->load() - 2;
    p.osc2Octave = (int) apvts.getRawParameterValue("osc2Oct")->load() - 2;
    p.osc1Level = apvts.getRawParameterValue("osc1Level")->load();
    p.osc2Level = apvts.getRawParameterValue("osc2Level")->load();
    p.subLevel = apvts.getRawParameterValue("subLevel")->load();
    p.noiseLevel = apvts.getRawParameterValue("noiseLevel")->load();
    p.osc2DetuneCents = apvts.getRawParameterValue("detune")->load();
    p.pulseWidth = apvts.getRawParameterValue("pw")->load();
    p.mixerDrive = apvts.getRawParameterValue("mixDrive")->load();
    p.cutoffHz = apvts.getRawParameterValue("cutoff")->load();
    p.resonance = apvts.getRawParameterValue("resonance")->load();
    p.filterDrive = apvts.getRawParameterValue("filterDrive")->load();
    p.filterMode = static_cast<jerzy::FilterMode>(getChoiceIndex("filterMode"));
    p.modEnvPitch = apvts.getRawParameterValue("modEnvPitch")->load();
    p.modEnvPWM = apvts.getRawParameterValue("modEnvPWM")->load();
    p.modEnvOsc2Pitch = apvts.getRawParameterValue("modEnvOsc2Pitch")->load();
    p.modEnvResonance = apvts.getRawParameterValue("modEnvResonance")->load();
    p.modEnvMixDrive = apvts.getRawParameterValue("modEnvMixDrive")->load();
    p.modEnvAmp = apvts.getRawParameterValue("modEnvAmp")->load();
    p.filterEnvOct = apvts.getRawParameterValue("filterEnv")->load();
    p.keyTrack = apvts.getRawParameterValue("keyTrack")->load();
    p.filterAttack = apvts.getRawParameterValue("fA")->load();
    p.filterDecay = apvts.getRawParameterValue("fD")->load();
    p.filterSustain = apvts.getRawParameterValue("fS")->load();
    p.filterRelease = apvts.getRawParameterValue("fR")->load();
    p.ampAttack = apvts.getRawParameterValue("aA")->load();
    p.ampDecay = apvts.getRawParameterValue("aD")->load();
    p.ampSustain = apvts.getRawParameterValue("aS")->load();
    p.ampRelease = apvts.getRawParameterValue("aR")->load();
    p.glideSeconds = apvts.getRawParameterValue("glide")->load();
    p.lfoWave = lfoWaveFrom(apvts.getRawParameterValue("lfoWave")->load());
    p.lfoRate = apvts.getRawParameterValue("lfoRate")->load();

    double bpm = 120.0, hostPpqStart = 0.0;
    bool hostHasPpq = false, hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto hostBpm = pos->getBpm()) bpm = *hostBpm;
            if (auto ppq = pos->getPpqPosition()) { hostPpqStart = *ppq; hostHasPpq = true; }
            hostPlaying = pos->getIsPlaying();
        }

    if (apvts.getRawParameterValue("lfoSync")->load() > 0.5f)
    {
        static constexpr double cyclesPerQuarter[] =
        { 0.25,0.5,1.0,2.0,4.0,8.0,1.5,3.0,6.0,2.0/3.0,4.0/3.0,8.0/3.0 };
        const int idx = juce::jlimit(0,11,getChoiceIndex("lfoDivision"));
        p.lfoRate = (bpm / 60.0) * cyclesPerQuarter[idx];
    }

    p.lfoPitchCents = apvts.getRawParameterValue("lfoPitch")->load();
    p.lfoFilterOct = apvts.getRawParameterValue("lfoFilter")->load();
    p.lfoPWM = apvts.getRawParameterValue("lfoPWM")->load();
    p.lfoAmp = apvts.getRawParameterValue("lfoAmp")->load();
    p.lfoFadeSeconds = apvts.getRawParameterValue("lfoFade")->load();
    p.outputDrive = apvts.getRawParameterValue("outDrive")->load();
    p.master = apvts.getRawParameterValue("master")->load();
    p.analogDriftCents = apvts.getRawParameterValue("drift")->load();
    gridRootNote.store(gridRootMidiFromChoice());
    p.legato = apvts.getRawParameterValue("legato")->load() > 0.5f;
    p.retrigger = apvts.getRawParameterValue("retrigger")->load() > 0.5f;
    const int pr = getChoiceIndex("priority");
    p.priority = pr == 1 ? jerzy::NotePriority::low : (pr == 2 ? jerzy::NotePriority::high : jerzy::NotePriority::last);
    p.glideMode = getChoiceIndex("glideMode") > 0 ? jerzy::GlideMode::legatoOnly : jerzy::GlideMode::always;
    const int nextMode=getChoiceIndex("voiceMode");
    if(nextMode!=voiceMode || restoredState.exchange(false)) {
        for(int t=0;t<2;++t){stopTrack(t,0,generatedMidiScratch);tracks[(size_t)t].lastStep=-1;}
        voiceMode=nextMode;engine.setMode(voiceMode);
    }
    engine.setAudioInput(apvts.getRawParameterValue("audioInOn")->load()>.5f,getChoiceIndex("audioInGate")==0,apvts.getRawParameterValue("audioInGain")->load());
    engine.setParameters(p);
    gateLfo.set(p.lfoRate,p.lfoWave);gateModDepth=apvts.getRawParameterValue("lfoGate")->load();

    gridSettings.armed=apvts.getRawParameterValue("gridSeqOn")->load()>0.5f;
    gridSettings.noteGate=apvts.getRawParameterValue("gridNoteGate")->load()>.5f;
    gridSettings.midiTrigger=apvts.getRawParameterValue("gridMidiTrigger")->load()>0.5f;
    gridSettings.hostSync=apvts.getRawParameterValue("gridHostSync")->load()>0.5f;
    gridSettings.routeToArp=apvts.getRawParameterValue("gridToArp")->load()>0.5f
                            && apvts.getRawParameterValue("arpOn")->load()>0.5f;
    gridActiveBanks.store(1+getChoiceIndex("gridBanks"));
    gridSettings.totalSteps=gridActiveBanks.load()*8;
    for(int t=0;t<2;++t) {
        auto& state=tracks[(size_t)t];
        const int exact=(int)apvts.getRawParameterValue(t==0?"gridLength":"grid2Length")->load();
        state.length=exact>0?exact:gridSettings.totalSteps;
        state.division=getChoiceIndex(t==0?"gridDivision":"grid2Division");
        state.direction=getChoiceIndex(t==0?"gridDirection":"grid2Direction");
        state.swing=apvts.getRawParameterValue(t==0?"gridSwing":"grid2Swing")->load();
        state.enabled=t==0 || apvts.getRawParameterValue("grid2On")->load()>.5f;
    }
    gridSettings.division=getChoiceIndex("gridDivision");
    gridSettings.ratchets=1+getChoiceIndex("gridRatchet");
    gridSettings.direction=getChoiceIndex("gridDirection");
    gridSettings.swing=juce::jlimit(0.0f,0.49f,apvts.getRawParameterValue("gridSwing")->load());
    gridSettings.gate=juce::jlimit(0.05f,0.98f,apvts.getRawParameterValue("gridGate")->load());
    gridSettings.velocity=juce::jlimit(0.01f,1.0f,apvts.getRawParameterValue("gridVelocity")->load());
    gridSettings.probability=juce::jlimit(0.0f,1.0f,apvts.getRawParameterValue("gridProbability")->load());

    const bool arpOn = apvts.getRawParameterValue("arpOn")->load() > 0.5f;
    const bool arpLatch = apvts.getRawParameterValue("arpLatch")->load() > 0.5f;
    const bool arpRetrig = apvts.getRawParameterValue("arpRetrigger")->load() > 0.5f;
    const int arpPattern = getChoiceIndex("arpPattern");
    const int arpRhythm = getChoiceIndex("arpRhythm");
    const int arpDiv = getChoiceIndex("arpDivision");
    const int arpOctaves = 1 + getChoiceIndex("arpOctaves");
    const float arpGate = apvts.getRawParameterValue("arpGate")->load();
    const float arpSwing = juce::jlimit(0.0f,0.49f,apvts.getRawParameterValue("arpSwing")->load());
    const float arpVelocity = juce::jlimit(0.01f,1.0f,apvts.getRawParameterValue("arpVelocity")->load());
    const bool arpHostSync = apvts.getRawParameterValue("arpHostSync")->load()>0.5f;

    static constexpr double quarterMult[] =
    { 4.0,2.0,1.0,0.5,0.25,0.125,2.0/3.0,1.0/3.0,1.0/6.0,1.5,0.75,0.375 };
    const double stepSamples = currentSampleRate * (60.0 / juce::jmax(1.0,bpm)) * quarterMult[juce::jlimit(0,11,arpDiv)];
    const double arpQuarter=quarterMult[juce::jlimit(0,11,arpDiv)];
    const double blockEnd=hostPpqStart+b.getNumSamples()*bpm/(60.0*currentSampleRate);
    const bool seek=hostHasPpq && (hostPlaying!=previousHostPlaying || (hostPlaying && std::abs(hostPpqStart-previousPpq)>1e-5));
    if(seek) {for(int t=0;t<2;++t){stopTrack(t,0,generatedMidiScratch);tracks[(size_t)t].lastStep=-1;}}
    previousPpq=blockEnd;previousHostPlaying=hostPlaying;
    auto& inputMidi=inputMidiScratch;
    auto& generatedMidi=generatedMidiScratch;
    inputMidi.swapWith(midi);
    midi.clear();
    auto midiIt = inputMidi.cbegin();
    bool hasMidi = midiIt != inputMidi.cend();
    juce::MidiMessageMetadata ev;
    if (hasMidi) ev = *midiIt;

    for (int s = 0; s < b.getNumSamples(); ++s)
    {
        gateModulation=gateLfo.process();
        const double samplePpq=hostPpqStart+((double)s*bpm)/(60.0*currentSampleRate);
        while (hasMidi && ev.samplePosition <= s)
        {
            const auto m = ev.getMessage();
            if (m.isNoteOn())
            {
                const int note = m.getNoteNumber();
                const bool gridMidiTrig = gridSettings.armed && gridSettings.midiTrigger
                                       && gridMode.load()==(int)GridMode::sequencer;

                if (gridMidiTrig)
                {
                    if (!physicalHeldNotes.contains(note))
                        physicalHeldNotes.add(note);
                    gridMidiHeldCount.store(physicalHeldNotes.size());
                    gridMidiRunning.store(true);
                    gridTriggerNote.store(note);
                    gridHostPpqOrigin=samplePpq;
                    for(int t=0;t<2;++t){stopTrack(t,s,generatedMidi);tracks[(size_t)t].lastStep=-1;}
                    lastGridHostStep=-1;
                    stopGridNote(s,generatedMidi);
                    gridGlobalStep=0;
                    gridSamplesToNext=0.0;
                }
                else
                {
                    if (!physicalHeldNotes.contains(note))
                        physicalHeldNotes.add(note);

                if (arpOn)
                {
                    if (arpLatch && physicalHeldNotes.size() == 1)
                        arpLatchedNotes.clear();

                    if (!arpHeldNotes.contains(note)) arpHeldNotes.add(note);
                    if (!arpLatchedNotes.contains(note)) arpLatchedNotes.add(note);
                    if (arpRetrig) { arpStep = 0; arpSamplesToNext = 0.0; }
                }
                else engine.noteOn(note, m.getFloatVelocity());
                }
            }
            else if (m.isNoteOff())
            {
                const int note = m.getNoteNumber();
                const bool gridMidiTrig = gridSettings.armed && gridSettings.midiTrigger
                                       && gridMode.load()==(int)GridMode::sequencer;

                if(gridMidiTrig)
                {
                    physicalHeldNotes.removeAllInstancesOf(note);
                    gridMidiHeldCount.store(physicalHeldNotes.size());
                    if(physicalHeldNotes.isEmpty())
                    {
                        gridMidiRunning.store(false);
                        stopGridNote(s,generatedMidi);
                        gridPlayColumn.store(-1);
                    }
                    else
                    {
                        gridTriggerNote.store(physicalHeldNotes.getLast());
                        gridHostPpqOrigin=samplePpq;
                        for(int t=0;t<2;++t){stopTrack(t,s,generatedMidi);tracks[(size_t)t].lastStep=-1;}
                        lastGridHostStep=-1;
                        gridGlobalStep=0;gridSamplesToNext=0.0;
                        stopGridNote(s,generatedMidi);
                    }
                }
                else
                {
                    physicalHeldNotes.removeAllInstancesOf(note);

                if (arpOn)
                {
                    arpHeldNotes.removeAllInstancesOf(note);
                    if (!arpLatch)
                        arpLatchedNotes.removeAllInstancesOf(note);
                }
                else engine.noteOff(note);
                }
            }

            ++midiIt;
            hasMidi = midiIt != inputMidi.cend();
            if (hasMidi) ev = *midiIt;
        }

        processGridSequencerSample(bpm,s,generatedMidi,samplePpq,hostHasPpq,hostPlaying);
        processLaunchPadSample(s,generatedMidi,samplePpq,gridSettings.routeToArp);
        if(gridArpNeedsRestart)
        {
            arpStep=0; arpSamplesToNext=0.0; lastArpHostStep=-1;
            gridArpNeedsRestart=false;
        }

        const bool arpHostClock=arpHostSync && hostHasPpq && hostPlaying;
        const bool arpClockRunning=!arpHostSync || arpHostClock || gridArpNote>=0 || gridArpNote2>=0 || launchArpNote>=0;
        bool arpHostStepChanged=false;
        if(arpHostClock)
        {
            const double origin=(gridArpNote>=0 || gridArpNote2>=0 || launchArpNote>=0)?arpGridPpqOrigin:0.0;
            const int hostStep=jerzy::swungStepAtPpq(samplePpq-origin,arpQuarter,arpSwing);
            if(hostStep!=lastArpHostStep){lastArpHostStep=hostStep;arpStep=juce::jmax(0,hostStep);arpSamplesToNext=0.0;arpHostStepChanged=true;}
        }
        else if(arpHostSync && !arpClockRunning)
        {
            lastArpHostStep=-1;arpSamplesToNext=0.0;
            if(arpCurrentNote>=0){engine.noteOff(arpCurrentNote);generatedMidi.addEvent(juce::MidiMessage::noteOff(1,arpCurrentNote),s);arpCurrentNote=-1;}
        }

        if (!arpOn && arpCurrentNote >= 0)
        {
            engine.noteOff(arpCurrentNote);
            generatedMidi.addEvent(juce::MidiMessage::noteOff(1,arpCurrentNote),s);
            arpCurrentNote=-1;
        }

        if (arpOn && arpClockRunning)
        {
            if (arpNoteCount()==0)
            {
                if (arpCurrentNote >= 0) {
                    engine.noteOff(arpCurrentNote);
                    generatedMidi.addEvent(juce::MidiMessage::noteOff(1,arpCurrentNote),s);
                    arpCurrentNote = -1;
                }
            }
            else
            {
                if ((arpHostClock && arpHostStepChanged) || (!arpHostClock && arpSamplesToNext <= 0.0))
                {
                    if (arpCurrentNote >= 0) {
                        engine.noteOff(arpCurrentNote);
                        generatedMidi.addEvent(juce::MidiMessage::noteOff(1,arpCurrentNote),s);
                        arpCurrentNote = -1;
                    }

                    arpCurrentStepSamples=stepSamples*((arpStep&1)?(1.0-arpSwing):(1.0+arpSwing));
                    if (arpRhythmGate(arpRhythm, arpStep))
                    {
                        const int base = chooseArpNote(arpPattern, arpStep);
                        if (base >= 0)
                        {
                            const int octave = (arpStep / juce::jmax(1,arpNoteCount())) % arpOctaves;
                            arpCurrentNote = juce::jlimit(0,127,base + octave * 12);
                            engine.noteOn(arpCurrentNote, arpVelocity);
                            generatedMidi.addEvent(juce::MidiMessage::noteOn(1,arpCurrentNote,arpVelocity),s);
                        }
                    }
                    ++arpStep;
                    arpSamplesToNext += arpCurrentStepSamples;
                }

                if (arpCurrentNote >= 0 && arpSamplesToNext <= arpCurrentStepSamples*(1.0-juce::jlimit(0.05f,0.98f,arpGate)))
                {
                    engine.noteOff(arpCurrentNote);
                    generatedMidi.addEvent(juce::MidiMessage::noteOff(1,arpCurrentNote),s);
                    arpCurrentNote = -1;
                }

                arpSamplesToNext -= 1.0;
            }
        }

        const float inputL=getTotalNumInputChannels()>0?b.getSample(0,s):0;
        const float inputR=getTotalNumInputChannels()>1?b.getSample(1,s):inputL;
        const float y=engine.processSample();
        for(int ch=0;ch<getTotalNumOutputChannels();++ch)b.setSample(ch,s,y+engine.processInput(ch,ch==0?inputL:inputR));
        const float ay = std::abs(y);
        const float old = outputMeter.load();
        outputMeter.store(ay > old ? ay : old * 0.9975f);
    }
    fxChain.process(b,apvts,bpm);
    midi.addEvents(generatedMidi,0,b.getNumSamples(),0);
    inputMidi.clear();
}

juce::AudioProcessorEditor* JerzyMonoAnalogAudioProcessor::createEditor() { return new JerzyMonoAnalogAudioProcessorEditor(*this); }

void JerzyMonoAnalogAudioProcessor::getStateInformation(juce::MemoryBlock& mb)
{
    auto state = apvts.copyState();
    state.setProperty("gridMode",gridMode.load(),nullptr);
    state.setProperty("gridBank",gridBank.load(),nullptr);
    state.setProperty("gridRoot",gridRootNote.load(),nullptr);
    juce::String bits;
    for(size_t i=0;i<gridPattern.size();++i) bits << (gridPattern[i].load() ? "1" : "0");
    state.setProperty("gridPattern",bits,nullptr);
    juce::String bits2;for(auto& cell:gridPattern2)bits2<<(cell.load()?"1":"0");
    state.setProperty("gridPattern2",bits2,nullptr);state.setProperty("gridTrack",gridTrack.load(),nullptr);
    juce::String order; for(int x:fxChain.getOrder()){if(order.isNotEmpty())order<<",";order<<x;} state.setProperty("fxOrder",order,nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml()); copyXmlToBinary(*xml, mb);
}
void JerzyMonoAnalogAudioProcessor::setStateInformation(const void* d, int n)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(d, n));
    if(xml)
    {
        auto st=juce::ValueTree::fromXml(*xml);
        if(st.hasProperty("gridMode")) gridMode.store((int)st["gridMode"]);
        if(st.hasProperty("gridBank")) gridBank.store((int)st["gridBank"]);
        if(st.hasProperty("gridRoot")) gridRootNote.store((int)st["gridRoot"]);
        if(st.hasProperty("gridPattern"))
        {
            auto bits=st["gridPattern"].toString();
            const int count=juce::jmin((int)gridPattern.size(),bits.length());
            for(int i=0;i<count;++i) gridPattern[(size_t)i].store(bits[i]=='1'?1:0);
        }
        auto bits2=st["gridPattern2"].toString();
        for(int i=0;i<512;++i)gridPattern2[(size_t)i].store(i<bits2.length() && bits2[i]=='1'?1:0);
        setGridTrack((int)st.getProperty("gridTrack",0));
        st.removeProperty("gridPattern2",nullptr);st.removeProperty("gridTrack",nullptr);
        st.removeProperty("gridMode",nullptr);st.removeProperty("gridBank",nullptr);st.removeProperty("gridRoot",nullptr);st.removeProperty("gridPattern",nullptr);
        if(st.hasProperty("fxOrder")){auto tokens=juce::StringArray::fromTokens(st["fxOrder"].toString(),",","");std::array<int,MonoFxChain::count> order{0,1,2,3,4,5};if(tokens.size()==MonoFxChain::count){for(int i=0;i<MonoFxChain::count;++i)order[(size_t)i]=tokens[i].getIntValue();fxChain.setOrder(order);}st.removeProperty("fxOrder",nullptr);}
        // An old preset must reset destinations added in 0.4 instead of inheriting
        // whatever the previously loaded preset left in the current processor.
        for(const auto* id : {"filterMode", "modEnvPitch", "modEnvPWM", "gridDirection", "gridSwing", "gridVelocity", "arpSwing", "arpVelocity", "gridHostSync", "arpHostSync", "fxCompOn", "fxCompThreshold", "fxCompRatio", "fxCompAttack", "fxCompRelease", "fxCompDrive", "fxDelayOn", "fxDelayDivision", "fxDelayMode", "fxDelayFeedback", "fxDelayMix", "fxReverbOn", "fxReverbSize", "fxReverbDamping", "fxReverbMix", "fxWidthOn", "fxWidth", "fxChorusOn", "fxChorusMode", "fxChorusRate", "fxChorusDepth", "fxChorusMix", "fxChorusFeedback", "fxRotaryOn", "fxRotarySync", "fxRotaryDivision", "fxRotaryRate", "fxRotaryDepth", "gridOctave", "gridProbability", "gridRatchet", "modEnvOsc2Pitch", "modEnvResonance", "modEnvMixDrive", "modEnvAmp", "gridToArp"})
        {
            if(!st.getChildWithProperty("id",id).isValid())
            {
                const float defaultValue = juce::String(id)=="gridOctave" ? 2.0f
                                         : juce::String(id)=="fxCompThreshold" ? -18.0f
                                         : juce::String(id)=="fxCompRatio" ? 4.0f
                                         : juce::String(id)=="fxCompAttack" ? 0.01f
                                         : juce::String(id)=="fxCompRelease" ? 0.12f
                                         : juce::String(id)=="fxDelayDivision" || juce::String(id)=="fxRotaryDivision" ? 4.0f
                                         : juce::String(id)=="fxDelayFeedback" ? 0.35f
                                         : juce::String(id)=="fxDelayMix" || juce::String(id)=="fxReverbMix" || juce::String(id)=="fxChorusMix" ? 0.25f
                                         : juce::String(id)=="fxReverbSize" || juce::String(id)=="fxWidth" || juce::String(id)=="fxChorusDepth" || juce::String(id)=="fxRotaryDepth" ? 0.6f
                                         : juce::String(id)=="fxReverbDamping" ? 0.35f
                                         : juce::String(id)=="fxChorusRate" ? 0.25f
                                         : juce::String(id)=="fxRotaryRate" ? 1.0f
                                         : juce::String(id)=="fxRotarySync" ? 1.0f
                                         : juce::String(id)=="fxChorusFeedback" ? 0.15f
                                         : juce::String(id)=="gridVelocity" ? 0.95f
                                         : juce::String(id)=="gridProbability" ? 1.0f
                                         : juce::String(id)=="arpVelocity" ? 0.9f
                                         : (juce::String(id)=="gridHostSync" || juce::String(id)=="arpHostSync") ? 1.0f : 0.0f;
                juce::ValueTree parameter("PARAM");
                parameter.setProperty("id",id,nullptr);parameter.setProperty("value",defaultValue,nullptr);
                st.addChild(parameter,-1,nullptr);
            }
        }
        for(auto* base:getParameters())if(auto* param=dynamic_cast<juce::RangedAudioParameter*>(base)) {
            const auto id=param->getParameterID();
            if(!st.getChildWithProperty("id",id).isValid()) {
                juce::ValueTree v("PARAM");v.setProperty("id",id,nullptr);
                v.setProperty("value",id=="gridNoteGate"?0.0f:param->convertFrom0to1(param->getDefaultValue()),nullptr);st.addChild(v,-1,nullptr);
            }
        }
        apvts.replaceState(st);restoredState.store(true);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout JerzyMonoAnalogAudioProcessor::createLayout()
{
    using P = HostAutomatableFloat; using B = HostAutomatableBool; using C = HostAutomatableChoice;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<C>("osc1Wave","OSC1 Wave",juce::StringArray{"Sine","Triangle","Saw","Square"},2));
    l.add(std::make_unique<C>("osc2Wave","OSC2 Wave",juce::StringArray{"Sine","Triangle","Saw","Square"},2));
    l.add(std::make_unique<C>("subWave","Sub Wave",juce::StringArray{"Sine","Square"},1));
    l.add(std::make_unique<C>("osc1Oct","OSC1 Octave",juce::StringArray{"16'","8'","4'","2'","1'"},2));
    l.add(std::make_unique<C>("osc2Oct","OSC2 Octave",juce::StringArray{"16'","8'","4'","2'","1'"},2));
    l.add(std::make_unique<P>("osc1Level","OSC1 Level",0.0f,1.0f,0.75f));
    l.add(std::make_unique<P>("osc2Level","OSC2 Level",0.0f,1.0f,0.55f));
    l.add(std::make_unique<P>("subLevel","Sub Level",0.0f,1.0f,0.25f));
    l.add(std::make_unique<P>("noiseLevel","Noise Level",0.0f,1.0f,0.0f));
    l.add(std::make_unique<P>("detune","OSC2 Detune",-50.0f,50.0f,7.0f));
    l.add(std::make_unique<P>("pw","Pulse Width",0.05f,0.95f,0.5f));
    l.add(std::make_unique<P>("mixDrive","Mixer Drive",0.0f,1.0f,0.18f));
    l.add(std::make_unique<P>("cutoff","Cutoff",juce::NormalisableRange<float>(20.0f,20000.0f,0.0f,0.22f),1800.0f));
    l.add(std::make_unique<P>("resonance","Resonance",0.0f,1.15f,0.15f));
    l.add(std::make_unique<P>("filterDrive","Filter Drive",0.0f,1.0f,0.12f));
    l.add(std::make_unique<P>("filterEnv","Filter Env",-6.0f,6.0f,2.5f));
    l.add(std::make_unique<P>("keyTrack","Key Track",0.0f,1.0f,0.25f));
    auto sec=[](const char* id,const char* nm,float def,float max){return std::make_unique<P>(id,nm,juce::NormalisableRange<float>(0.0005f,max,0.0f,0.3f),def);};
    l.add(sec("fA","Filter Attack",0.002f,10.0f)); l.add(sec("fD","Filter Decay",0.22f,15.0f));
    l.add(std::make_unique<P>("fS","Filter Sustain",0.0f,1.0f,0.2f)); l.add(sec("fR","Filter Release",0.18f,20.0f));
    l.add(sec("aA","Amp Attack",0.005f,10.0f)); l.add(sec("aD","Amp Decay",0.18f,15.0f));
    l.add(std::make_unique<P>("aS","Amp Sustain",0.0f,1.0f,0.75f)); l.add(sec("aR","Amp Release",0.22f,20.0f));
    l.add(std::make_unique<P>("glide","Glide",juce::NormalisableRange<float>(0.0f,2.0f,0.0f,0.35f),0.0f));
    l.add(std::make_unique<C>("glideMode","Glide Mode",juce::StringArray{"Always","Legato"},1));
    l.add(std::make_unique<C>("priority","Note Priority",juce::StringArray{"Last","Low","High"},0));
    l.add(std::make_unique<C>("lfoWave","LFO Wave",juce::StringArray{"Sine","Triangle","Saw","Square","S&H","Random Square"},0));
    l.add(std::make_unique<B>("lfoSync","LFO Tempo Sync",false));
    l.add(std::make_unique<C>("lfoDivision","LFO Division",
        juce::StringArray{"1/1","1/2","1/4","1/8","1/16","1/32","1/4T","1/8T","1/16T","1/4D","1/8D","1/16D"},3));
    l.add(std::make_unique<P>("lfoRate","LFO Rate",juce::NormalisableRange<float>(0.03f,30.0f,0.0f,0.25f),2.0f));
    l.add(std::make_unique<P>("lfoPitch","LFO Pitch",0.0f,100.0f,0.0f));
    l.add(std::make_unique<P>("lfoFilter","LFO Filter",0.0f,4.0f,0.0f));
    l.add(std::make_unique<P>("lfoPWM","LFO PWM",0.0f,1.0f,0.0f));
    l.add(std::make_unique<P>("lfoAmp","LFO Amp Mod",0.0f,1.0f,0.0f));
    l.add(std::make_unique<P>("lfoFade","LFO Fade In",juce::NormalisableRange<float>(0.0f,5.0f,0.0f,0.35f),0.0f));
    l.add(std::make_unique<P>("outDrive","Output Drive",0.0f,1.0f,0.12f));
    l.add(std::make_unique<P>("master","Master",0.0f,1.0f,0.8f));
    l.add(std::make_unique<P>("drift","Analog Drift",0.0f,6.0f,2.0f));
    l.add(std::make_unique<B>("legato","Legato",true)); l.add(std::make_unique<B>("retrigger","Retrigger",false));
    l.add(std::make_unique<B>("arpOn","Arpeggiator On",false));
    l.add(std::make_unique<C>("arpDivision","Arp Division",juce::StringArray{"1/1","1/2","1/4","1/8","1/16","1/32","1/4T","1/8T","1/16T","1/4D","1/8D","1/16D"},4));
    l.add(std::make_unique<C>("arpPattern","Arp Pattern",juce::StringArray{"Up","Down","UpDown","Random","As Played","Up x2","Down x2","UpDown Inclusive","Converge"},0));
    l.add(std::make_unique<C>("arpRhythm","Arp Rhythm",juce::StringArray{"Straight","Every 2","3-3-2","Syncopated","Clave","5 over 8","Rolling"},0));
    l.add(std::make_unique<C>("arpOctaves","Arp Octaves",juce::StringArray{"1","2","3","4"},0));
    l.add(std::make_unique<P>("arpGate","Arp Gate",0.05f,0.98f,0.72f));
    l.add(std::make_unique<B>("arpLatch","Arp Latch",false));
    l.add(std::make_unique<B>("arpRetrigger","Arp Retrigger",true));
    l.add(std::make_unique<B>("gridSeqOn","Grid Sequencer On",false));
    l.add(std::make_unique<C>("gridDivision","Grid Division",juce::StringArray{"1/1","1/2","1/4","1/8","1/16","1/32","1/4T","1/8T","1/16T","1/4D","1/8D","1/16D"},4));
    l.add(std::make_unique<P>("gridGate","Grid Gate",0.05f,0.98f,0.75f));
    l.add(std::make_unique<C>("gridRoot","Grid Root Note",juce::StringArray{"C1","C#1","D1","D#1","E1","F1","F#1","G1","G#1","A1","A#1","B1","C2","C#2","D2","D#2","E2","F2","F#2","G2","G#2","A2","A#2","B2","C3","C#3","D3","D#3","E3","F3","F#3","G3","G#3","A3","A#3","B3","C4","C#4","D4","D#4","E4","F4","F#4","G4","G#4","A4","A#4","B4","C5","C#5","D5","D#5","E5","F5","F#5","G5","G#5","A5","A#5","B5","C6"},24));
    l.add(std::make_unique<C>("gridScale","Grid Scale",juce::StringArray{"Chromatic","Major","Natural Minor","Dorian","Phrygian","Mixolydian","Major Pent","Minor Pent"},1));
    l.add(std::make_unique<C>("gridBanks","Grid Length",juce::StringArray{"1 bank / 8 steps","2 banks / 16 steps","3 banks / 24 steps","4 banks / 32 steps","5 banks / 40 steps","6 banks / 48 steps","7 banks / 56 steps","8 banks / 64 steps"},7));
    l.add(std::make_unique<B>("gridMidiTrigger","Grid MIDI Trigger",false));
    // Append new IDs so existing parameter indices and automation stay intact.
    l.add(std::make_unique<C>("filterMode","Filter Mode",juce::StringArray{"Ladder 24 dB","LP 12 dB","LP 24 dB","HP 12 dB","HP 24 dB","BP 12 dB","BP 24 dB"},0));
    l.add(std::make_unique<P>("modEnvPitch","Mod Env Pitch",-24.0f,24.0f,0.0f));
    l.add(std::make_unique<P>("modEnvPWM","Mod Env PWM",-1.0f,1.0f,0.0f));
    l.add(std::make_unique<C>("gridDirection","Grid Direction",juce::StringArray{"Forward","Reverse","Ping-Pong","Random"},0));
    l.add(std::make_unique<P>("gridSwing","Grid Swing",0.0f,0.49f,0.0f));
    l.add(std::make_unique<P>("gridVelocity","Grid Velocity",0.01f,1.0f,0.95f));
    l.add(std::make_unique<P>("arpSwing","Arp Swing",0.0f,0.49f,0.0f));
    l.add(std::make_unique<P>("arpVelocity","Arp Velocity",0.01f,1.0f,0.9f));
    l.add(std::make_unique<B>("gridHostSync","Grid Host Sync",true));
    l.add(std::make_unique<B>("arpHostSync","Arp Host Sync",true));
    // Output effects: appended IDs keep existing host automation and preset IDs stable.
    l.add(std::make_unique<B>("fxCompOn","FX Compressor On",false));
    l.add(std::make_unique<P>("fxCompThreshold","FX Comp Threshold",-36.0f,0.0f,-18.0f));
    l.add(std::make_unique<P>("fxCompRatio","FX Comp Ratio",1.0f,20.0f,4.0f));
    l.add(std::make_unique<P>("fxCompAttack","FX Comp Attack",juce::NormalisableRange<float>(0.0005f,0.1f,0.0f,0.35f),0.01f));
    l.add(std::make_unique<P>("fxCompRelease","FX Comp Release",juce::NormalisableRange<float>(0.005f,1.0f,0.0f,0.35f),0.12f));
    l.add(std::make_unique<P>("fxCompDrive","FX Comp Drive",0.0f,1.0f,0.0f));
    l.add(std::make_unique<B>("fxDelayOn","FX Delay On",false));
    l.add(std::make_unique<C>("fxDelayDivision","FX Delay Division",juce::StringArray{"1/1","1/2","1/4","1/8","1/16","1/32","1/4T","1/8T","1/16T","1/4D","1/8D","1/16D"},4));
    l.add(std::make_unique<C>("fxDelayMode","FX Delay Mode",juce::StringArray{"Mono","Stereo","Ping-Pong"},1));
    l.add(std::make_unique<P>("fxDelayFeedback","FX Delay Feedback",0.0f,0.94f,0.35f));
    l.add(std::make_unique<P>("fxDelayMix","FX Delay Mix",0.0f,1.0f,0.25f));
    l.add(std::make_unique<B>("fxReverbOn","FX Reverb On",false));
    l.add(std::make_unique<P>("fxReverbSize","FX Reverb Size",0.0f,1.0f,0.6f));
    l.add(std::make_unique<P>("fxReverbDamping","FX Reverb Damping",0.0f,1.0f,0.35f));
    l.add(std::make_unique<P>("fxReverbMix","FX Reverb Mix",0.0f,1.0f,0.25f));
    l.add(std::make_unique<B>("fxWidthOn","FX Stereo Width On",false));
    l.add(std::make_unique<P>("fxWidth","FX Stereo Width",0.0f,2.0f,0.6f));
    l.add(std::make_unique<B>("fxChorusOn","FX Chorus On",false));
    l.add(std::make_unique<C>("fxChorusMode","FX Chorus Mode",juce::StringArray{"Juno Chorus","Chorus","Flanger"},0));
    l.add(std::make_unique<P>("fxChorusRate","FX Chorus Rate",0.05f,8.0f,0.25f));
    l.add(std::make_unique<P>("fxChorusDepth","FX Chorus Depth",0.0f,1.0f,0.6f));
    l.add(std::make_unique<P>("fxChorusMix","FX Chorus Mix",0.0f,1.0f,0.25f));
    l.add(std::make_unique<P>("fxChorusFeedback","FX Flanger Feedback",-0.85f,0.85f,0.15f));
    l.add(std::make_unique<B>("fxRotaryOn","FX Rotary On",false));
    l.add(std::make_unique<B>("fxRotarySync","FX Rotary Sync",true));
    l.add(std::make_unique<C>("fxRotaryDivision","FX Rotary Division",juce::StringArray{"1/1","1/2","1/4","1/8","1/16","1/32","1/4T","1/8T","1/16T","1/4D","1/8D","1/16D"},4));
    l.add(std::make_unique<P>("fxRotaryRate","FX Rotary Rate",0.1f,8.0f,1.0f));
    l.add(std::make_unique<P>("fxRotaryDepth","FX Rotary Depth",0.0f,1.0f,0.6f));
    l.add(std::make_unique<C>("gridOctave","Grid Octave",juce::StringArray{"-2 oct","-1 oct","0 oct","+1 oct","+2 oct"},2));
    l.add(std::make_unique<P>("gridProbability","Grid Probability",0.0f,1.0f,1.0f));
    l.add(std::make_unique<C>("gridRatchet","Grid Ratchet",juce::StringArray{"1","2","3","4"},0));
    l.add(std::make_unique<P>("modEnvOsc2Pitch","Mod Env OSC2 Pitch",-24.0f,24.0f,0.0f));
    l.add(std::make_unique<P>("modEnvResonance","Mod Env Resonance",-1.0f,1.0f,0.0f));
    l.add(std::make_unique<P>("modEnvMixDrive","Mod Env Mixer Drive",-1.0f,1.0f,0.0f));
    l.add(std::make_unique<P>("modEnvAmp","Mod Env Amp",-1.0f,1.0f,0.0f));
    l.add(std::make_unique<B>("gridToArp","GRID to Arpeggiator",false));
    l.add(std::make_unique<C>("voiceMode","Voice Mode",juce::StringArray{"Mono","Paraphonic","Poly 6"},0));
    l.add(std::make_unique<B>("audioInOn","Audio Input On",false));
    l.add(std::make_unique<P>("audioInGain","Audio Input Gain",0.0f,2.0f,1.0f));
    l.add(std::make_unique<C>("audioInGate","Audio Input Gate",juce::StringArray{"Open","MIDI / Grid Gate"},0));
    l.add(std::make_unique<P>("lfoGate","LFO Gate Depth (steps)",-15.0f,15.0f,0.0f));
    l.add(std::make_unique<P>("gridLength","OSC1 Exact Length (0=bank chain)",juce::NormalisableRange<float>(0,64,1),0));
    l.add(std::make_unique<P>("grid2Length","OSC2 Exact Length",juce::NormalisableRange<float>(1,64,1),64));
    l.add(std::make_unique<B>("grid2On","OSC2 Sequencer On",true));
    l.add(std::make_unique<B>("gridNoteGate","Note Gate Steps (off=legacy percent)",true));
    l.add(std::make_unique<C>("grid2Division","OSC2 Division",juce::StringArray{"1/1","1/2","1/4","1/8","1/16","1/32","1/4T","1/8T","1/16T","1/4D","1/8D","1/16D"},4));
    l.add(std::make_unique<C>("grid2Direction","OSC2 Direction",juce::StringArray{"Forward","Reverse","Ping-Pong","Random"},0));
    l.add(std::make_unique<P>("grid2Swing","OSC2 Swing",0.0f,.49f,0.0f));
    for(int t=0;t<2;++t)for(int step=0;step<64;++step)
        l.add(std::make_unique<P>("gateT"+juce::String(t+1)+"S"+juce::String(step),"OSC"+juce::String(t+1)+" Step "+juce::String(step+1)+" Gate",juce::NormalisableRange<float>(1,16,1),1));
    return l;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JerzyMonoAnalogAudioProcessor(); }

