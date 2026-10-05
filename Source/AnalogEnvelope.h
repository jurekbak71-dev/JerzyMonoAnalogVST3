#pragma once
#include <algorithm>
#include <cmath>

namespace jerzy
{
// Capacitor charging/discharging curve shared by DSP and the editor.
inline double envelopeCurve(double t, bool attack)
{
    t = std::clamp(t, 0.0, 1.0);
    const double k = attack ? std::log(6.0) : std::log(1001.0);
    return (1.0 - std::exp(-k * t)) / (1.0 - std::exp(-k));
}

class AnalogADSR
{
public:
    void prepare(double fs) { sampleRate = std::max(1.0, fs); reset(); }
    void reset() { stage = Stage::idle; value = 0.0; remaining = 0; }
    void set(double a, double d, double s, double r)
    {
        const double oldTime = stage == Stage::attack ? attack : stage == Stage::decay ? decay : release;
        const double oldSustain = sustain;
        attack = std::max(0.0005, a); decay = std::max(0.0005, d);
        sustain = std::clamp(s, 0.0, 1.0); release = std::max(0.0005, r);
        const double newTime = stage == Stage::attack ? attack : stage == Stage::decay ? decay : release;
        if (remaining > 0 && (oldTime != newTime || (stage == Stage::decay && oldSustain != sustain)))
        {
            const double secondsLeft = static_cast<double>(remaining) / sampleRate * newTime / oldTime;
            begin(stage, secondsLeft, stage == Stage::attack ? 1.0 : stage == Stage::decay ? sustain : 0.0);
        }
    }
    void noteOn() { begin(Stage::attack, attack, 1.0); }
    void noteOff() { if (stage != Stage::idle && stage != Stage::release) begin(Stage::release, release, 0.0); }
    double process()
    {
        if (stage == Stage::idle) return 0.0;
        if (stage == Stage::sustain)
        {
            value += (sustain - value) * (1.0 - std::exp(-1.0 / (0.002 * sampleRate)));
            return value;
        }
        value = target + (value - target) * multiplier;
        if (--remaining <= 0)
        {
            value = endpoint;
            if (stage == Stage::attack) begin(Stage::decay, decay, sustain);
            else if (stage == Stage::decay) stage = Stage::sustain;
            else stage = Stage::idle;
        }
        return std::clamp(value, 0.0, 1.0);
    }
private:
    enum class Stage { idle, attack, decay, sustain, release } stage = Stage::idle;
    void begin(Stage next, double seconds, double end)
    {
        stage = next; endpoint = end;
        remaining = std::max(1LL, std::llround(seconds * sampleRate));
        const double overshoot = next == Stage::attack ? 0.2 : 0.001;
        target = end + (end - value) * overshoot;
        multiplier = std::exp(-std::log((1.0 + overshoot) / overshoot) / static_cast<double>(remaining));
    }
    long long remaining = 0;
    double sampleRate = 44100.0, attack = 0.01, decay = 0.2, sustain = 0.7, release = 0.3;
    double value = 0.0, target = 0.0, endpoint = 0.0, multiplier = 0.0;
};
}
