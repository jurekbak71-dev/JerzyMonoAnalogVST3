#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <random>
#include <cmath>
#include <vector>
#include "AnalogEnvelope.h"

namespace jerzy
{
static inline double saturateAsymmetric(double x)
{
    // Mild asymmetry gives a little even-harmonic content without hard clipping.
    constexpr double bias = 0.045;
    return (std::tanh(x + bias) - std::tanh(bias)) / (1.0 - std::tanh(bias) * std::tanh(bias));
}

class DCBlocker
{
public:
    void prepare(double fs, double cutoffHz = 7.0)
    {
        const double rc = 1.0 / (2.0 * juce::MathConstants<double>::pi * cutoffHz);
        const double dt = 1.0 / fs;
        r = rc / (rc + dt);
        reset();
    }
    double process(double x)
    {
        const double y = r * (z + x - x1);
        x1 = x; z = y;
        return y;
    }
    void reset() { x1 = z = 0.0; }
private:
    double r = 0.995, x1 = 0.0, z = 0.0;
};

class SmoothRandom
{
public:
    void prepare(double fs, double speedHz, uint32_t seed)
    {
        sampleRate = fs;
        speed = juce::jmax(0.001, speedHz);
        rng.seed(seed);
        current = target = randomValue();
        setNextTarget();
    }
    double next()
    {
        if (++count >= segmentSamples)
            setNextTarget();
        const double t = (double) count / (double) segmentSamples;
        const double s = t * t * (3.0 - 2.0 * t);
        return current + (target - current) * s;
    }
private:
    double randomValue() { return dist(rng); }
    void setNextTarget()
    {
        current = target;
        target = randomValue();
        const double wander = 0.7 + 0.6 * std::abs(randomValue());
        segmentSamples = (int) juce::jmax(16.0, sampleRate / speed * wander);
        count = 0;
    }
    double sampleRate = 44100.0, speed = 0.2, current = 0.0, target = 0.0;
    int count = 0, segmentSamples = 44100;
    std::mt19937 rng;
    std::uniform_real_distribution<double> dist {-1.0, 1.0};
};

class BandLimitedOscillator
{
public:
    enum class Wave { sine, triangle, saw, square };

    void prepare(double fs, uint32_t seed)
    {
        sampleRate = fs;
        rng.seed(seed);
        drift.prepare(fs, 0.11 + (seed & 7u) * 0.009, seed ^ 0x9e3779b9u);
        phase = std::uniform_real_distribution<double>(0.0, 1.0)(rng);
        resetStates();
    }

    void reset()
    {
        // Free-running analogue VCOs do not restart from exactly the same phase.
        phase = std::uniform_real_distribution<double>(0.0, 1.0)(rng);
        resetStates();
    }

    void setWave(Wave w) { wave = w; }
    void setFrequency(double hz) { frequency = juce::jlimit(0.01, sampleRate * 0.45, hz); }
    void setPulseWidth(double p) { pw = juce::jlimit(0.05, 0.95, p); }
    void setDriftCents(double c) { driftCents = juce::jlimit(0.0, 8.0, c); }
    void setKeyErrorCents(double c) { keyErrorCents = juce::jlimit(-18.0, 18.0, c); }

    double getLastFrequency() const { return effectiveFrequency; }

    double process()
    {
        const double cents = keyErrorCents + drift.next() * driftCents;
        const double f = frequency * std::pow(2.0, cents / 1200.0);
        effectiveFrequency = f;
        const double dt = juce::jmin(0.49, f / sampleRate);
        const double p = phase;
        double y = 0.0;

        switch (wave)
        {
            case Wave::saw:
            {
                double a = 2.0 * p - 1.0;
                a -= polyBlep(p, dt);
                // Mild saw-core/waveshaper asymmetry adds low-order analogue harmonics.
                y = a + 0.020 * (a * a - 0.3333333333) - 0.010 * a * a * a;
                break;
            }

            case Wave::square:
            {
                double sq = (p < pw ? 1.0 : -1.0);
                sq += polyBlep(p, dt);
                sq -= polyBlep(wrapped(p - pw), dt);
                // Tiny comparator imbalance avoids a sterile perfectly symmetric pulse.
                y = sq > 0.0 ? sq * 0.986 : sq * 1.014;
                break;
            }

            case Wave::triangle:
            {
                double sq = (p < 0.5 ? 1.0 : -1.0);
                sq += polyBlep(p, dt);
                sq -= polyBlep(wrapped(p + 0.5), dt);
                triState += 4.0 * dt * sq;
                // Correct the integrator toward the matching triangle, independently of pitch.
                const double reference = 1.0 - 4.0 * std::abs(p - 0.5);
                triState += (1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * 2.0 / sampleRate)) * (reference - triState);
                const double t = juce::jlimit(-1.18, 1.18, triState);
                y = std::tanh(1.08 * t) / std::tanh(1.08);
                break;
            }

            case Wave::sine:
            {
                const double s = std::sin(juce::MathConstants<double>::twoPi * p);
                y = s + 0.018 * std::sin(juce::MathConstants<double>::twoPi * 2.0 * p + 0.17);
                break;
            }
        }

        // Finite analogue bandwidth / edge slew at the oversampled rate.
        const double fc = juce::jmin(26000.0, sampleRate * 0.18);
        const double a = 1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * fc / sampleRate);
        edgeState += a * (y - edgeState);

        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        return edgeState * 0.98;
    }

private:
    void resetStates() { triState = 1.0 - 4.0 * std::abs(phase - 0.5); edgeState = 0.0; }

    static double wrapped(double x)
    {
        while (x < 0.0) x += 1.0;
        while (x >= 1.0) x -= 1.0;
        return x;
    }

    static double polyBlep(double t, double dt)
    {
        if (dt <= 0.0) return 0.0;
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    double sampleRate = 44100.0;
    double phase = 0.0, frequency = 110.0, effectiveFrequency = 110.0, pw = 0.5;
    double triState = 0.0, edgeState = 0.0;
    double driftCents = 2.0, keyErrorCents = 0.0;
    Wave wave = Wave::saw;
    SmoothRandom drift;
    std::mt19937 rng;
};

class AnalogLFO
{
public:
    enum class Wave { sine, triangle, saw, square, sampleHold, randomSquare };
    void prepare(double fs, uint32_t seed)
    {
        sampleRate = fs; rng.seed(seed); reset();
    }
    void reset() { phase = 0.0; sh = random(); randomLeft = 0.0; randomLevel = 1.0; }
    void set(double hz, Wave w) { freq = juce::jlimit(0.01, 40.0, hz); wave = w; }
    double process()
    {
        if (wave == Wave::randomSquare)
        {
            if (randomLeft <= 0.0) {
                randomLevel = -randomLevel;
                randomLeft = sampleRate / freq * (0.1 + 1.4 * (random() + 1.0) * 0.5);
            }
            randomLeft -= 1.0;
            return randomLevel;
        }
        double y = 0.0;
        switch (wave)
        {
            case Wave::sine: y = std::sin(juce::MathConstants<double>::twoPi * phase); break;
            case Wave::triangle: y = 1.0 - 4.0 * std::abs(phase - 0.5); break;
            case Wave::saw: y = 2.0 * phase - 1.0; break;
            case Wave::square: y = phase < 0.5 ? 1.0 : -1.0; break;
            case Wave::sampleHold: y = sh; break;
            case Wave::randomSquare: break;
        }
        const double old = phase;
        phase += freq / sampleRate;
        if (phase >= 1.0) phase -= 1.0;
        if (wave == Wave::sampleHold && phase < old) sh = random();
        return y;
    }
private:
    double random() { return dist(rng); }
    double sampleRate = 44100.0, freq = 2.0, phase = 0.0, sh = 0.0;
    double randomLeft = 0.0, randomLevel = 1.0;
    Wave wave = Wave::sine;
    std::mt19937 rng;
    std::uniform_real_distribution<double> dist {-1.0, 1.0};
};

class HalfBandDecimator2x
{
public:
    static constexpr int taps = 63;
    void prepare()
    {
        constexpr double fc = 0.235;
        constexpr int M = taps - 1;
        double sum = 0.0;
        for (int n = 0; n < taps; ++n)
        {
            const double m = (double)n - 0.5 * M;
            const double sinc = (std::abs(m) < 1.0e-12)
                              ? 2.0 * fc
                              : std::sin(2.0 * juce::MathConstants<double>::pi * fc * m)
                                / (juce::MathConstants<double>::pi * m);
            const double w = 0.42 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * n / M)
                                  + 0.08 * std::cos(4.0 * juce::MathConstants<double>::pi * n / M);
            coeff[(size_t)n] = sinc * w;
            sum += coeff[(size_t)n];
        }
        for (auto& c : coeff) c /= sum;
        reset();
    }
    void reset() { history.fill(0.0); write = 0; phase = 0; }
    bool process(double x, double& out)
    {
        history[(size_t)write] = x;
        write = (write + 1) % taps;
        phase ^= 1;
        if (phase != 0) return false;

        double y = 0.0;
        int idx = write;
        for (int i = 0; i < taps; ++i)
        {
            idx = (idx - 1 + taps) % taps;
            y += coeff[(size_t)i] * history[(size_t)idx];
        }
        out = y;
        return true;
    }
private:
    std::array<double, taps> coeff {}, history {};
    int write = 0, phase = 0;
};

class NonlinearLadder
{
public:
    void prepare(double fs)
    {
        sampleRate = fs;
        cutoffDrift.prepare(fs, 0.075, 0x3141592u);
        reset();
    }

    void reset()
    {
        state.fill(0.0);
        dc.prepare(sampleRate, 5.0);
        outputSmooth = 0.0;
    }

    void setParams(double cutoffHz, double resonance01, double drive01, double keyTrack01, double note)
    {
        baseCutoff = juce::jlimit(8.0, 22000.0, cutoffHz);
        resonance = juce::jlimit(0.0, 1.15, resonance01);
        drive = juce::jlimit(0.0, 1.0, drive01);
        keyTrack = juce::jlimit(0.0, 1.0, keyTrack01);
        midiNote = note;
    }

    double process(double x, double envAmountOctaves)
    {
        const double keyOct = (midiNote - 60.0) / 12.0 * keyTrack;
        const double driftMul = 1.0 + cutoffDrift.next() * 0.0015;
        double fc = baseCutoff * std::pow(2.0, keyOct + envAmountOctaves) * driftMul;

        // Small resonance-dependent cutoff pull is musically closer to a transistor ladder.
        fc *= 1.0 + 0.030 * resonance;
        fc = juce::jlimit(8.0, sampleRate * 0.44, fc);

        const double g = std::tan(juce::MathConstants<double>::pi * fc / sampleRate);
        const double G = g / (1.0 + g);

        // Non-linear feedback approaches self oscillation near the top of the control.
        const double rn = juce::jlimit(0.0, 1.0, resonance / 1.15);
        const double k = 4.12 * std::pow(rn, 0.78);

        // Pre-filter overdrive, with gain compensation so colour changes more than loudness.
        const double preGain = 1.0 + 10.0 * drive;
        const double input = warmClip(x * preGain) / std::sqrt(preGain);

        // Solve the instantaneous feedback root with a safeguarded Newton iteration.
        // The cascade is monotonic; [-1.1, 1.1] brackets its bounded output.
        double lo = -1.1, hi = 1.1;
        double estimate = juce::jlimit(lo, hi, state[3]);
        std::array<double,4> next {};
        double out = 0.0;
        for (int iter = 0; iter < 12; ++iter)
        {
            double slope = 0.0;
            evaluateCascade(input - k * estimate, G, next, out, &slope);
            const double error = estimate - out;
            if (std::abs(error) < 1.0e-9) break;
            if (error > 0.0) hi = estimate; else lo = estimate;
            const double candidate = estimate - error / (1.0 + k * slope);
            estimate = candidate > lo && candidate < hi ? candidate : 0.5 * (lo + hi);
        }
        evaluateCascade(input - k * estimate, G, next, out);
        state = next;

        // Keep the natural bass loss of a resonant ladder, but not so much that it sounds thin.
        const double makeup = 1.0 + 0.24 * rn;
        double y = out * makeup;

        const double smoothFc = juce::jmin(30000.0, sampleRate * 0.20);
        const double a = 1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * smoothFc / sampleRate);
        outputSmooth += a * (y - outputSmooth);
        return dc.process(outputSmooth);
    }

private:
    static double warmClip(double x)
    {
        return saturateAsymmetric(x);
    }

    static double transistor(double x, double scale)
    {
        return std::tanh(x * scale);
    }

    void evaluateCascade(double input, double G, std::array<double,4>& next, double& output, double* derivative = nullptr) const
    {
        static constexpr double stageScale[4] = { 1.000, 0.985, 1.012, 0.995 };
        double u = input;
        double slope = 1.0;

        for (int i = 0; i < 4; ++i)
        {
            const double inN = transistor(u, stageScale[i]);
            slope *= G * stageScale[i] * (1.0 - inN * inN);
            const double stN = transistor(state[(size_t)i], stageScale[i]);
            const double v = (inN - stN) * G;
            const double y = state[(size_t)i] + v;
            next[(size_t)i] = y + v;
            u = y;
        }
        output = u;
        if (derivative != nullptr) *derivative = slope;
    }

    double sampleRate = 44100.0;
    double baseCutoff = 1200.0, resonance = 0.0, drive = 0.0, keyTrack = 0.0, midiNote = 60.0;
    double outputSmooth = 0.0;
    std::array<double,4> state {};
    SmoothRandom cutoffDrift;
    DCBlocker dc;
};

enum class FilterMode { ladder24, lp12, lp24, hp12, hp24, bp12, bp24 };

// Trapezoidal state-variable sections. Resonance modifies damping rather than
// adding a delayed feedback sample. The 24 dB outputs cascade two sections.
class ClassicMultimode
{
    struct Outputs { double lp, hp, bp; };
    struct Section
    {
        double z1 = 0.0, z2 = 0.0;
        Outputs process(double input, double g, double k)
        {
            const double a1 = 1.0 / (1.0 + g * (g + k));
            const double v3 = input - z2;
            const double v1 = a1 * (z1 + g * v3);
            const double v2 = z2 + g * v1;
            z1 = 2.0 * v1 - z1; z2 = 2.0 * v2 - z2;
            // Damping-normalised bandpass: unity at centre, narrower as Q rises.
            return {v2, input - k * v1 - v2, k * v1};
        }
        void reset() { z1 = z2 = 0.0; }
    };
public:
    void prepare(double fs) { sampleRate = fs; reset(); }
    void reset() { first12.reset(); first24.reset(); low24.reset(); high24.reset(); band24.reset(); }
    std::array<double, 6> process(double input, double cutoff, double resonance, double drive)
    {
        const double g = std::tan(juce::MathConstants<double>::pi * juce::jlimit(8.0, sampleRate * 0.2, cutoff) / sampleRate);
        const double res = juce::jlimit(0.0, 1.0, resonance / 1.15);
        const double damp = 1.0 - 0.95 * res;
        const double gain = 1.0 + 10.0 * drive;
        input = saturateAsymmetric(input * gain) / std::sqrt(gain);
        const auto a = first12.process(input, g, 1.41421356237 * damp);
        const double damp24 = std::sqrt(damp);
        const auto b = first24.process(input, g, 1.84775906502 * damp24);
        const double k2 = 0.76536686473 * damp24;
        const auto lp = low24.process(b.lp, g, k2);
        const auto hp = high24.process(b.hp, g, k2);
        const auto bp = band24.process(b.bp, g, k2);
        return {a.lp, lp.lp, a.hp, hp.hp, a.bp, bp.bp};
    }
private:
    double sampleRate = 176400.0;
    Section first12, first24, low24, high24, band24;
};

enum class NotePriority { last, low, high };
enum class GlideMode { always, legatoOnly };

struct MonoParameters
{
    BandLimitedOscillator::Wave osc1Wave = BandLimitedOscillator::Wave::saw;
    BandLimitedOscillator::Wave osc2Wave = BandLimitedOscillator::Wave::saw;
    BandLimitedOscillator::Wave subWave = BandLimitedOscillator::Wave::square;
    AnalogLFO::Wave lfoWave = AnalogLFO::Wave::sine;
    int osc1Octave = 0, osc2Octave = 0;
    double osc1Level = 0.75, osc2Level = 0.55, subLevel = 0.25, noiseLevel = 0.0;
    double osc2DetuneCents = 7.0, pulseWidth = 0.5;
    double mixerDrive = 0.18;
    double cutoffHz = 1800.0, resonance = 0.15, filterDrive = 0.12, filterEnvOct = 2.5, keyTrack = 0.25;
    double ampAttack = 0.005, ampDecay = 0.18, ampSustain = 0.75, ampRelease = 0.22;
    FilterMode filterMode = FilterMode::ladder24;
    double modEnvPitch = 0.0, modEnvPWM = 0.0;
    bool externalOpen = false, externalProcessing = false;
    double modEnvOsc2Pitch = 0.0, modEnvResonance = 0.0;
    double modEnvMixDrive = 0.0, modEnvAmp = 0.0;
    double filterAttack = 0.002, filterDecay = 0.22, filterSustain = 0.2, filterRelease = 0.18;
    double glideSeconds = 0.0;
    double lfoRate = 2.0, lfoPitchCents = 0.0, lfoFilterOct = 0.0, lfoPWM = 0.0;
    double lfoAmp = 0.0, lfoFadeSeconds = 0.0;
    double outputDrive = 0.12, master = 0.8, analogDriftCents = 2.0;
    NotePriority priority = NotePriority::last;
    GlideMode glideMode = GlideMode::legatoOnly;
    bool legato = true, retrigger = false;
};

class MonoAnalogEngine
{
public:
    void prepare(double fs, int maximumBlockSize)
    {
        juce::ignoreUnused(maximumBlockSize);
        hostRate = fs;
        osFactor = 4;
        sampleRate = fs * osFactor;
        osc1.prepare(sampleRate, 0x123456u);
        osc2.prepare(sampleRate, 0xabcdefu);
        sub.prepare(sampleRate, 0x77aa55u);
        lfo.prepare(sampleRate, 0x818181u);
        filter.prepare(sampleRate);
        multimode.prepare(sampleRate);
        ampEnv.prepare(sampleRate);
        filterEnv.prepare(sampleRate);
        outputDC.prepare(sampleRate); mixerDC.prepare(sampleRate);
        decimA.prepare();
        decimB.prepare();
        noiseRng.seed(0xdeadbeefu); keyRng.seed(0x13579bdu);
        smoothingCoefficient = 1.0 - std::exp(-1.0 / (0.003 * sampleRate));
        parametersInitialised = false;
        reset();
    }

    void reset()
    {
        osc1.reset(); osc2.reset(); sub.reset(); lfo.reset(); filter.reset(); ampEnv.reset(); filterEnv.reset(); outputDC.reset(); mixerDC.reset();
        decimA.reset(); decimB.reset();
        currentNote = -1; targetMidi = currentMidi = 60.0; heldNotes.clear();
        heldNotes.ensureStorageAllocated(128);
        multimode.reset();
        filterWeights.fill(0.0); filterWeights[static_cast<size_t>(params.filterMode)] = 1.0;
        lastOutput = 0.0; lfoFadeValue = 1.0;
        paraMode=false;paraNotes.fill(-1);paraGain.fill(0.0);externalSample=0.0;externalVcaGain=0.0;
    }

    void setParameters(const MonoParameters& p)
    {
        params = p;
        if (!parametersInitialised) { smoothed = p; parametersInitialised = true; filterWeights.fill(0.0); filterWeights[static_cast<size_t>(p.filterMode)] = 1.0; }
        ampEnv.set(p.ampAttack, p.ampDecay, p.ampSustain, p.ampRelease);
        filterEnv.set(p.filterAttack, p.filterDecay, p.filterSustain, p.filterRelease);
    }

    void noteOn(int note, float velocity)
    {
        const bool hadHeldNotes = !heldNotes.isEmpty();
        heldNotes.removeAllInstancesOf(note);
        heldNotes.add(note);
        velocityGain = juce::jlimit(0.0, 1.0, (double)velocity);
        selectPriorityNote();

        // Per-keypress VCO error: subtle and independent for each oscillator.
        std::uniform_real_distribution<double> keyErr(-1.0, 1.0);
        const double errAmount = juce::jlimit(0.0, 4.0, params.analogDriftCents * 0.55);
        osc1.setKeyErrorCents(keyErr(keyRng) * errAmount);
        osc2.setKeyErrorCents(keyErr(keyRng) * errAmount * 1.12);
        sub.setKeyErrorCents(0.0);

        const bool retrig = !hadHeldNotes || !params.legato || params.retrigger;
        if (retrig) { ampEnv.noteOn(); filterEnv.noteOn(); }
        lfoFadeValue = params.lfoFadeSeconds > 0.0 ? 0.0 : 1.0;

        const bool shouldGlide = params.glideSeconds > 0.0
                              && (params.glideMode == GlideMode::always || hadHeldNotes);
        if (!shouldGlide) currentMidi = targetMidi;
    }

    void noteOff(int note)
    {
        if (!heldNotes.contains(note)) return;
        const bool wasSelected = currentNote == note;
        heldNotes.removeAllInstancesOf(note);
        if (heldNotes.isEmpty())
        {
            currentNote = -1;
            ampEnv.noteOff();
            filterEnv.noteOff();
            return;
        }

        selectPriorityNote();
        if (wasSelected && (!params.legato || params.retrigger))
        {
            ampEnv.noteOn();
            filterEnv.noteOn();
        }
    }

    bool isActive() const noexcept { return ampEnv.isActive() || std::abs(lastOutput) > 1.0e-8; }
    void oscillatorNoteOn(int track, int note, float velocity)
    {
        track = juce::jlimit(0,1,track);
        const bool held = paraNotes[0] >= 0 || paraNotes[1] >= 0;
        paraMode = true; paraNotes[(size_t)track] = note;
        paraTarget[(size_t)track] = note;
        if (!held || params.glideSeconds <= 0.0) paraPitch[(size_t)track] = note;
        if (!held || params.retrigger || !params.legato) { ampEnv.noteOn(); filterEnv.noteOn(); }
        velocityGain = velocity; currentMidi = paraPitch[0];
    }
    void oscillatorNoteOff(int track)
    {
        paraNotes[(size_t)juce::jlimit(0,1,track)] = -1;
        if (paraNotes[0] < 0 && paraNotes[1] < 0) { ampEnv.noteOff(); filterEnv.noteOff(); }
    }

    float processSample(float external = 0.0f)
    {
        externalSample = external;
        double aOut = 0.0, bOut = lastOutput;
        for (int i = 0; i < osFactor; ++i)
        {
            const double hi = processOversampled();
            if (decimA.process(hi, aOut))
            {
                double candidate = 0.0;
                if (decimB.process(aOut, candidate))
                {
                    bOut = candidate;
                    lastOutput = candidate;
                }
            }
        }
        // Keep floating-point headroom for the output drive and post-synth FX.
        return (float)bOut;
    }

private:
    void selectPriorityNote()
    {
        if (heldNotes.isEmpty()) return;
        int chosen = heldNotes.getLast();
        if (params.priority == NotePriority::low)
            chosen = *std::min_element(heldNotes.begin(), heldNotes.end());
        else if (params.priority == NotePriority::high)
            chosen = *std::max_element(heldNotes.begin(), heldNotes.end());
        currentNote = chosen;
        targetMidi = (double)chosen;
    }

    double processOversampled()
    {
        MonoParameters p = params;
        p.osc1Level = smoothed.osc1Level += smoothingCoefficient * (params.osc1Level - smoothed.osc1Level);
        p.osc2Level = smoothed.osc2Level += smoothingCoefficient * (params.osc2Level - smoothed.osc2Level);
        p.subLevel = smoothed.subLevel += smoothingCoefficient * (params.subLevel - smoothed.subLevel);
        p.noiseLevel = smoothed.noiseLevel += smoothingCoefficient * (params.noiseLevel - smoothed.noiseLevel);
        p.osc2DetuneCents = smoothed.osc2DetuneCents += smoothingCoefficient * (params.osc2DetuneCents - smoothed.osc2DetuneCents);
        p.pulseWidth = smoothed.pulseWidth += smoothingCoefficient * (params.pulseWidth - smoothed.pulseWidth);
        p.mixerDrive = smoothed.mixerDrive += smoothingCoefficient * (params.mixerDrive - smoothed.mixerDrive);
        p.cutoffHz = smoothed.cutoffHz += smoothingCoefficient * (params.cutoffHz - smoothed.cutoffHz);
        p.resonance = smoothed.resonance += smoothingCoefficient * (params.resonance - smoothed.resonance);
        p.filterDrive = smoothed.filterDrive += smoothingCoefficient * (params.filterDrive - smoothed.filterDrive);
        p.filterEnvOct = smoothed.filterEnvOct += smoothingCoefficient * (params.filterEnvOct - smoothed.filterEnvOct);
        p.keyTrack = smoothed.keyTrack += smoothingCoefficient * (params.keyTrack - smoothed.keyTrack);
        p.lfoPitchCents = smoothed.lfoPitchCents += smoothingCoefficient * (params.lfoPitchCents - smoothed.lfoPitchCents);
        p.lfoFilterOct = smoothed.lfoFilterOct += smoothingCoefficient * (params.lfoFilterOct - smoothed.lfoFilterOct);
        p.lfoPWM = smoothed.lfoPWM += smoothingCoefficient * (params.lfoPWM - smoothed.lfoPWM);
        p.lfoAmp = smoothed.lfoAmp += smoothingCoefficient * (params.lfoAmp - smoothed.lfoAmp);
        p.outputDrive = smoothed.outputDrive += smoothingCoefficient * (params.outputDrive - smoothed.outputDrive);
        p.master = smoothed.master += smoothingCoefficient * (params.master - smoothed.master);
        p.analogDriftCents = smoothed.analogDriftCents += smoothingCoefficient * (params.analogDriftCents - smoothed.analogDriftCents);
        p.modEnvPitch = smoothed.modEnvPitch += smoothingCoefficient * (params.modEnvPitch - smoothed.modEnvPitch);
        p.modEnvPWM = smoothed.modEnvPWM += smoothingCoefficient * (params.modEnvPWM - smoothed.modEnvPWM);
        p.modEnvOsc2Pitch = smoothed.modEnvOsc2Pitch += smoothingCoefficient * (params.modEnvOsc2Pitch - smoothed.modEnvOsc2Pitch);
        p.modEnvResonance = smoothed.modEnvResonance += smoothingCoefficient * (params.modEnvResonance - smoothed.modEnvResonance);
        p.modEnvMixDrive = smoothed.modEnvMixDrive += smoothingCoefficient * (params.modEnvMixDrive - smoothed.modEnvMixDrive);
        p.modEnvAmp = smoothed.modEnvAmp += smoothingCoefficient * (params.modEnvAmp - smoothed.modEnvAmp);
        if (p.glideSeconds > 0.0)
        {
            const double a = 1.0 - std::exp(-1.0 / (p.glideSeconds * sampleRate));
            currentMidi += (targetMidi - currentMidi) * a;
        }
        else currentMidi = targetMidi;
        if (paraMode) {
            for (size_t i=0;i<2;++i) {
                const double glide = p.glideSeconds > 0.0 ? 1.0-std::exp(-1.0/(p.glideSeconds*sampleRate)) : 1.0;
                paraPitch[i] += (paraTarget[i]-paraPitch[i])*glide;
                paraGain[i] += smoothingCoefficient*((paraNotes[i]>=0?1.0:0.0)-paraGain[i]);
            }
            currentMidi = paraPitch[0];
        }

        lfo.set(p.lfoRate, p.lfoWave);
        const double l = lfo.process();
        if (p.lfoFadeSeconds <= 0.0)
            lfoFadeValue = 1.0;
        else
            lfoFadeValue = juce::jmin(1.0, lfoFadeValue + 1.0 / (p.lfoFadeSeconds * sampleRate));
        const double lf = l * lfoFadeValue;
        const double fe = filterEnv.process();
        const double ae = ampEnv.process();
        const double pitchMod = lf * p.lfoPitchCents / 100.0 + fe * p.modEnvPitch;
        const double baseHz = 440.0 * std::pow(2.0, (currentMidi + pitchMod - 69.0) / 12.0);

        osc1.setWave(p.osc1Wave); osc2.setWave(p.osc2Wave); sub.setWave(p.subWave);
        const double modPW = juce::jlimit(0.05, 0.95, p.pulseWidth + (lf * p.lfoPWM + fe * p.modEnvPWM) * 0.45);
        osc1.setPulseWidth(modPW); osc2.setPulseWidth(modPW);
        osc1.setDriftCents(p.analogDriftCents);
        osc2.setDriftCents(p.analogDriftCents * 1.13);
        sub.setDriftCents(0.0);
        osc1.setFrequency(baseHz * std::pow(2.0, p.osc1Octave));
        osc2.setFrequency(baseHz * std::pow(2.0, p.osc2Octave + p.osc2DetuneCents / 1200.0 + fe * p.modEnvOsc2Pitch / 12.0 + (paraMode ? (paraPitch[1]-paraPitch[0])/12.0 : 0.0)));
        sub.setFrequency(baseHz * std::pow(2.0, p.osc1Octave - 1));

        const double n = noiseDist(noiseRng);
        const double vco1 = osc1.process();
        sub.setFrequency(osc1.getLastFrequency() * 0.5);
        double mix = p.osc1Level * vco1 * (paraMode?paraGain[0]:1.0)
                   + p.osc2Level * osc2.process() * (paraMode?paraGain[1]:1.0)
                   + p.subLevel  * sub.process() * (paraMode?paraGain[0]:1.0)
                   + p.noiseLevel * n + externalSample;

        // Mixer drive is a real pre-filter gain stage. Blend from clean at zero
        // to progressively harder transistor clipping without compensating away
        // the added harmonics and level.
        const double driveAmount = juce::jlimit(0.0, 1.0, p.mixerDrive + fe * p.modEnvMixDrive);
        const double mixDrive = 1.0 + 7.0 * driveAmount;
        const double drivenMix = saturateAsymmetric(mix * mixDrive);
        mix += driveAmount * (drivenMix - mix);
        mix = mixerDC.process(mix);

        const double resonance = juce::jlimit(0.0, 1.15, p.resonance + fe * p.modEnvResonance);
        filter.setParams(p.cutoffHz, resonance, p.filterDrive, p.keyTrack, currentMidi);
        const double filterMod = fe * p.filterEnvOct + lf * p.lfoFilterOct;
        const double ladder = filter.process(mix, filterMod);
        const double keyOct = (currentMidi - 60.0) / 12.0 * p.keyTrack;
        const auto classic = multimode.process(mix, p.cutoffHz * std::pow(2.0, keyOct + filterMod), resonance, p.filterDrive);
        double y = 0.0;
        const size_t selected = static_cast<size_t>(p.filterMode);
        // Keep all filter states running and crossfade; changing type never clears the tail.
        for (size_t mode = 0; mode < filterWeights.size(); ++mode)
        {
            filterWeights[mode] += smoothingCoefficient * ((mode == selected ? 1.0 : 0.0) - filterWeights[mode]);
            y += filterWeights[mode] * (mode == 0 ? ladder : classic[mode - 1]);
        }

        const double tremolo = 1.0 - p.lfoAmp * 0.5 * (lf + 1.0);
        externalVcaGain += smoothingCoefficient*((p.externalOpen?1.0:ae*velocityGain)-externalVcaGain);
        const double vca = y * (p.externalProcessing?externalVcaGain:ae*velocityGain) * tremolo * juce::jlimit(0.0, 2.0, 1.0 + fe * p.modEnvAmp);
        const double vcaBiased = vca + 0.012 * vca * vca;
        y = saturateAsymmetric(vcaBiased * 1.28) / 1.12;

        // Output drive has a true neutral setting, then blends toward a clipped
        // amplifier stage. Avoid square-root level compensation: that made the
        // old drive sound quieter as it saturated.
        const double outAmount=juce::jlimit(0.0,1.0,p.outputDrive);
        const double outGain=std::pow(10.0,(18.0*outAmount)/20.0);
        const double drivenOut=saturateAsymmetric(y*outGain);
        y += outAmount*(drivenOut-y);
        y = outputDC.process(y) * p.master;
        return y;
    }

    double hostRate = 44100.0, sampleRate = 176400.0;
    int osFactor = 4;
    MonoParameters params, smoothed;
    double smoothingCoefficient = 0.001;
    bool parametersInitialised = false;
    BandLimitedOscillator osc1, osc2, sub;
    AnalogLFO lfo;
    NonlinearLadder filter;
    ClassicMultimode multimode;
    std::array<double, 7> filterWeights {1.0,0.0,0.0,0.0,0.0,0.0,0.0};
    AnalogADSR ampEnv, filterEnv;
    DCBlocker outputDC, mixerDC;
    HalfBandDecimator2x decimA, decimB;
    juce::Array<int> heldNotes;
    int currentNote = -1;
    double targetMidi = 60.0, currentMidi = 60.0, velocityGain = 1.0, lastOutput = 0.0;
    double lfoFadeValue = 1.0;
    bool paraMode = false;
    std::array<int,2> paraNotes {-1,-1};
    std::array<double,2> paraTarget {60,60}, paraPitch {60,60}, paraGain {0,0};
    double externalSample = 0.0, externalVcaGain = 0.0;
    std::mt19937 noiseRng;
    std::mt19937 keyRng;
    std::uniform_real_distribution<double> noiseDist {-1.0, 1.0};
};

} // namespace jerzy

