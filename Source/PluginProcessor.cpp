#include "PluginProcessor.h"
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
: AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  apvts(*this, nullptr, "PARAMS", createLayout()) {}

void JerzyMonoAnalogAudioProcessor::prepareToPlay(double sr, int bs)
{
    currentSampleRate = sr;
    engine.prepare(sr, bs);
    fxChain.prepare(sr, bs);
    resetArpState();
    gridSamplesToNext=0.0;
    gridCurrentStepSamples=0.0;
    arpCurrentStepSamples=0.0;
    lastArpHostStep=-1; lastGridHostStep=-1;
    gridGlobalStep=0;
    gridCurrentNote=-1;
    gridPlayColumn.store(-1);
}

void JerzyMonoAnalogAudioProcessor::resetArpState()
{
    arpSamplesToNext = 0.0;
    arpStep = 0;
    arpCurrentNote = -1;
    arpUpDownPos = 0;
    arpHeldNotes.clear();
    arpLatchedNotes.clear();
    physicalHeldNotes.clear();
}

void JerzyMonoAnalogAudioProcessor::setGridStep(int bank,int column,int row,bool on)
{
    bank=juce::jlimit(0,7,bank); column=juce::jlimit(0,7,column); row=juce::jlimit(0,7,row);
    if(on)
    {
        for(int r=0;r<8;++r)
            gridPattern[(size_t)(bank*64 + r*8 + column)].store(0);
    }
    gridPattern[(size_t)(bank*64 + row*8 + column)].store(on?1:0);
}

bool JerzyMonoAnalogAudioProcessor::getGridStep(int bank,int column,int row) const
{
    bank=juce::jlimit(0,7,bank); column=juce::jlimit(0,7,column); row=juce::jlimit(0,7,row);
    return gridPattern[(size_t)(bank*64 + row*8 + column)].load()!=0;
}

void JerzyMonoAnalogAudioProcessor::clearGridBank(int bank)
{
    bank=juce::jlimit(0,7,bank);
    for(int i=0;i<64;++i) gridPattern[(size_t)(bank*64+i)].store(0);
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
    return juce::jlimit(0,127,gridRootMidiFromChoice()+scales[scale][degree]);
}

void JerzyMonoAnalogAudioProcessor::launchPadNoteOn(int padIndex)
{
    const int note=juce::jlimit(0,127,gridRootNote.load()+juce::jlimit(0,63,padIndex));
    const int prev=launchPressedNote.exchange(note);
    if(prev>=0) engine.noteOff(prev);
    engine.noteOn(note,0.95f);
}

void JerzyMonoAnalogAudioProcessor::launchPadNoteOff(int padIndex)
{
    const int note=juce::jlimit(0,127,gridRootNote.load()+juce::jlimit(0,63,padIndex));
    if(launchPressedNote.load()==note)
    {
        engine.noteOff(note);
        launchPressedNote.store(-1);
    }
}

void JerzyMonoAnalogAudioProcessor::processGridSequencerSample(double bpm, int sampleOffset, juce::MidiBuffer& generatedMidi, double hostPpq, bool hostHasPpq, bool hostPlaying)
{
    const bool armed = apvts.getRawParameterValue("gridSeqOn")->load()>0.5f;
    const bool midiTrigger = apvts.getRawParameterValue("gridMidiTrigger")->load()>0.5f;
    const bool running = armed && (!midiTrigger || gridMidiRunning.load());
    const bool hostSync=apvts.getRawParameterValue("gridHostSync")->load()>0.5f;
    gridActiveBanks.store(1 + getChoiceIndex("gridBanks"));
    gridRootNote.store(gridRootMidiFromChoice());
    const int totalSteps=juce::jlimit(8,64,gridActiveBanks.load()*8);

    if(!running || (hostSync && (!hostHasPpq || !hostPlaying)))
    {
        if(gridCurrentNote>=0){
            engine.noteOff(gridCurrentNote);
            generatedMidi.addEvent(juce::MidiMessage::noteOff(1,gridCurrentNote),sampleOffset);
            gridCurrentNote=-1;
        }
        gridPlayColumn.store(-1);
        if(hostSync){lastGridHostStep=-1;gridSamplesToNext=0.0;}
        return;
    }

    static constexpr double q[]={4.0,2.0,1.0,0.5,0.25,0.125,2.0/3.0,1.0/3.0,1.0/6.0,1.5,0.75,0.375};
    const int div=getChoiceIndex("gridDivision");
    const double stepQuarter=q[juce::jlimit(0,11,div)];
    const double stepSamples=currentSampleRate*(60.0/juce::jmax(1.0,bpm))*stepQuarter;
    bool gridHostStepChanged=false;
    if(hostSync && hostHasPpq)
    {
        const int hostStep=(int)std::floor((hostPpq+1.0e-9)/stepQuarter);
        if(hostStep!=lastGridHostStep){lastGridHostStep=hostStep;gridGlobalStep=juce::jmax(0,hostStep)%totalSteps;gridSamplesToNext=stepSamples;gridHostStepChanged=true;}
    }
    const double gate=juce::jlimit(0.05f,0.98f,apvts.getRawParameterValue("gridGate")->load());
    const double swing=hostSync ? 0.0 : juce::jlimit(0.0f,0.49f,apvts.getRawParameterValue("gridSwing")->load());
    const float velocity=juce::jlimit(0.01f,1.0f,apvts.getRawParameterValue("gridVelocity")->load());
    const int direction=getChoiceIndex("gridDirection");

    if((hostSync && gridHostStepChanged) || (!hostSync && gridSamplesToNext<=0.0))
    {
        if(gridCurrentNote>=0){
            engine.noteOff(gridCurrentNote);
            generatedMidi.addEvent(juce::MidiMessage::noteOff(1,gridCurrentNote),sampleOffset);
            gridCurrentNote=-1;
        }

        int step=gridGlobalStep % totalSteps;
        if(direction==1) step=totalSteps-1-step;
        else if(direction==2 && totalSteps>1)
        {
            const int span=2*totalSteps-2;
            const int pos=gridGlobalStep%span;
            step=pos<totalSteps?pos:span-pos;
        }
        else if(direction==3)
        {
            std::uniform_int_distribution<int> d(0,totalSteps-1);
            step=d(arpRng);
        }
        const double currentStepSamples=stepSamples*((gridGlobalStep&1)?(1.0-swing):(1.0+swing));
        const int bank=step/8;
        const int col=step&7;
        gridPlayColumn.store(col);

        for(int row=0;row<8;++row)
        {
            if(gridPattern[(size_t)(bank*64+row*8+col)].load()!=0)
            {
                gridCurrentNote=gridNoteForRow(row);
                engine.noteOn(gridCurrentNote,velocity);
                generatedMidi.addEvent(juce::MidiMessage::noteOn(1,gridCurrentNote,velocity),sampleOffset);
                break;
            }
        }

        gridGlobalStep=(gridGlobalStep+1)%totalSteps;
        gridCurrentStepSamples=currentStepSamples;
        gridSamplesToNext+=currentStepSamples;
    }

    if(gridCurrentNote>=0 && gridSamplesToNext<=gridCurrentStepSamples*(1.0-gate))
    {
        engine.noteOff(gridCurrentNote);
        generatedMidi.addEvent(juce::MidiMessage::noteOff(1,gridCurrentNote),sampleOffset);
        gridCurrentNote=-1;
    }

    gridSamplesToNext-=1.0;
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
        default:return true;
    }
}

int JerzyMonoAnalogAudioProcessor::chooseArpNote(int pattern, int step)
{
    auto notes = arpLatchedNotes;
    if (notes.isEmpty()) return -1;
    // Keep insertion order for "As Played"; pitch modes use sorted notes.
    if (pattern != 4) std::sort(notes.begin(), notes.end());
    const int n = notes.size();

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
            std::uniform_int_distribution<int> d(0, n - 1);
            return notes[d(arpRng)];
        }
        case 4: return notes[step % n]; // as played fallback
        default:return notes[step % n]; // up
    }
}

bool JerzyMonoAnalogAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

static jerzy::BandLimitedOscillator::Wave waveFrom(float v)
{
    switch ((int) v) { case 0: return jerzy::BandLimitedOscillator::Wave::sine; case 1: return jerzy::BandLimitedOscillator::Wave::triangle; case 2: return jerzy::BandLimitedOscillator::Wave::saw; default: return jerzy::BandLimitedOscillator::Wave::square; }
}
static jerzy::AnalogLFO::Wave lfoWaveFrom(float v)
{
    switch ((int) v) { case 0: return jerzy::AnalogLFO::Wave::sine; case 1: return jerzy::AnalogLFO::Wave::triangle; case 2: return jerzy::AnalogLFO::Wave::saw; case 3: return jerzy::AnalogLFO::Wave::square; default: return jerzy::AnalogLFO::Wave::sampleHold; }
}

void JerzyMonoAnalogAudioProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    b.clear();

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
    engine.setParameters(p);

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
    juce::MidiBuffer inputMidi;
    inputMidi.addEvents(midi,0,b.getNumSamples(),0);
    midi.clear();
    juce::MidiBuffer generatedMidi;
    auto midiIt = inputMidi.cbegin();
    bool hasMidi = midiIt != inputMidi.cend();
    juce::MidiMessageMetadata ev;
    if (hasMidi) ev = *midiIt;

    for (int s = 0; s < b.getNumSamples(); ++s)
    {
        const double samplePpq=hostPpqStart+((double)s*bpm)/(60.0*currentSampleRate);
        const bool arpClockRunning=!arpHostSync || (hostHasPpq && hostPlaying);
        bool arpHostStepChanged=false;
        if(arpHostSync && arpClockRunning)
        {
            const int hostStep=(int)std::floor((samplePpq+1.0e-9)/quarterMult[juce::jlimit(0,11,arpDiv)]);
            if(hostStep!=lastArpHostStep){lastArpHostStep=hostStep;arpStep=juce::jmax(0,hostStep);arpSamplesToNext=0.0;arpHostStepChanged=true;}
        }
        else if(arpHostSync)
        {
            lastArpHostStep=-1;arpSamplesToNext=0.0;
            if(arpCurrentNote>=0){engine.noteOff(arpCurrentNote);generatedMidi.addEvent(juce::MidiMessage::noteOff(1,arpCurrentNote),s);arpCurrentNote=-1;}
        }
        while (hasMidi && ev.samplePosition <= s)
        {
            const auto m = ev.getMessage();
            if (m.isNoteOn())
            {
                const int note = m.getNoteNumber();
                const bool gridMidiTrig = apvts.getRawParameterValue("gridSeqOn")->load()>0.5f
                                       && apvts.getRawParameterValue("gridMidiTrigger")->load()>0.5f;

                if (gridMidiTrig)
                {
                    if (!physicalHeldNotes.contains(note))
                        physicalHeldNotes.add(note);
                    gridMidiHeldCount.store(physicalHeldNotes.size());
                    gridMidiRunning.store(true);
                    if(gridCurrentNote>=0){
                        engine.noteOff(gridCurrentNote);
                        generatedMidi.addEvent(juce::MidiMessage::noteOff(1,gridCurrentNote),s);
                        gridCurrentNote=-1;
                    }
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
                const bool gridMidiTrig = apvts.getRawParameterValue("gridSeqOn")->load()>0.5f
                                       && apvts.getRawParameterValue("gridMidiTrigger")->load()>0.5f;

                if(gridMidiTrig)
                {
                    physicalHeldNotes.removeAllInstancesOf(note);
                    gridMidiHeldCount.store(physicalHeldNotes.size());
                    if(physicalHeldNotes.isEmpty())
                    {
                        gridMidiRunning.store(false);
                        if(gridCurrentNote>=0){
                            engine.noteOff(gridCurrentNote);
                            generatedMidi.addEvent(juce::MidiMessage::noteOff(1,gridCurrentNote),s);
                            gridCurrentNote=-1;
                        }
                        gridPlayColumn.store(-1);
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

        if (!arpOn && arpCurrentNote >= 0)
        {
            engine.noteOff(arpCurrentNote);
            generatedMidi.addEvent(juce::MidiMessage::noteOff(1,arpCurrentNote),s);
            arpCurrentNote=-1;
        }

        if (arpOn && arpClockRunning)
        {
            if (arpLatchedNotes.isEmpty())
            {
                if (arpCurrentNote >= 0) {
                    engine.noteOff(arpCurrentNote);
                    generatedMidi.addEvent(juce::MidiMessage::noteOff(1,arpCurrentNote),s);
                    arpCurrentNote = -1;
                }
            }
            else
            {
                if ((arpHostSync && arpHostStepChanged) || (!arpHostSync && arpSamplesToNext <= 0.0))
                {
                    if (arpCurrentNote >= 0) {
                        engine.noteOff(arpCurrentNote);
                        generatedMidi.addEvent(juce::MidiMessage::noteOff(1,arpCurrentNote),s);
                        arpCurrentNote = -1;
                    }

                    arpCurrentStepSamples=stepSamples*(arpHostSync ? 1.0f : ((arpStep&1)?(1.0-arpSwing):(1.0+arpSwing)));
                    if (arpRhythmGate(arpRhythm, arpStep))
                    {
                        const int base = chooseArpNote(arpPattern, arpStep);
                        if (base >= 0)
                        {
                            const int octave = (arpStep / juce::jmax(1,arpLatchedNotes.size())) % arpOctaves;
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

        processGridSequencerSample(bpm,s,generatedMidi,samplePpq,hostHasPpq,hostPlaying);
        const float y = engine.processSample();
        for (int ch = 0; ch < b.getNumChannels(); ++ch) b.setSample(ch, s, y);
        const float ay = std::abs(y);
        const float old = outputMeter.load();
        outputMeter.store(ay > old ? ay : old * 0.9975f);
    }
    fxChain.process(b,apvts,bpm);
    midi.addEvents(generatedMidi,0,b.getNumSamples(),0);
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
        st.removeProperty("gridMode",nullptr);st.removeProperty("gridBank",nullptr);st.removeProperty("gridRoot",nullptr);st.removeProperty("gridPattern",nullptr);
        if(st.hasProperty("fxOrder")){auto tokens=juce::StringArray::fromTokens(st["fxOrder"].toString(),",","");std::array<int,MonoFxChain::count> order{0,1,2,3,4,5};if(tokens.size()==MonoFxChain::count){for(int i=0;i<MonoFxChain::count;++i)order[(size_t)i]=tokens[i].getIntValue();fxChain.setOrder(order);}st.removeProperty("fxOrder",nullptr);}
        // An old preset must reset destinations added in 0.4 instead of inheriting
        // whatever the previously loaded preset left in the current processor.
        for(const auto* id : {"filterMode", "modEnvPitch", "modEnvPWM", "gridDirection", "gridSwing", "gridVelocity", "arpSwing", "arpVelocity", "gridHostSync", "arpHostSync", "fxCompOn", "fxCompThreshold", "fxCompRatio", "fxCompAttack", "fxCompRelease", "fxCompDrive", "fxDelayOn", "fxDelayDivision", "fxDelayMode", "fxDelayFeedback", "fxDelayMix", "fxReverbOn", "fxReverbSize", "fxReverbDamping", "fxReverbMix", "fxWidthOn", "fxWidth", "fxChorusOn", "fxChorusMode", "fxChorusRate", "fxChorusDepth", "fxChorusMix", "fxChorusFeedback", "fxRotaryOn", "fxRotarySync", "fxRotaryDivision", "fxRotaryRate", "fxRotaryDepth"})
        {
            if(!st.getChildWithProperty("id",id).isValid())
            {
                const float defaultValue = juce::String(id)=="fxCompThreshold" ? -18.0f
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
                                         : juce::String(id)=="arpVelocity" ? 0.9f
                                         : (juce::String(id)=="gridHostSync" || juce::String(id)=="arpHostSync") ? 1.0f : 0.0f;
                juce::ValueTree parameter("PARAM");
                parameter.setProperty("id",id,nullptr);parameter.setProperty("value",defaultValue,nullptr);
                st.addChild(parameter,-1,nullptr);
            }
        }
        apvts.replaceState(st);
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
    l.add(std::make_unique<C>("lfoWave","LFO Wave",juce::StringArray{"Sine","Triangle","Saw","Square","S&H"},0));
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
    l.add(std::make_unique<C>("arpPattern","Arp Pattern",juce::StringArray{"Up","Down","UpDown","Random","As Played"},0));
    l.add(std::make_unique<C>("arpRhythm","Arp Rhythm",juce::StringArray{"Straight","Every 2","3-3-2","Syncopated"},0));
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
    return l;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JerzyMonoAnalogAudioProcessor(); }

