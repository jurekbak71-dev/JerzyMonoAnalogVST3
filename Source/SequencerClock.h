#pragma once
#include <algorithm>
#include <cmath>

namespace jerzy
{
// A pair of steps always occupies two unswung steps. Odd steps start late;
// the preceding even step grows by the same amount that the odd step shrinks.
inline int swungStepAtPpq(double ppq, double quarterNotesPerStep, double swing)
{
    if (!std::isfinite(ppq) || !std::isfinite(quarterNotesPerStep) || quarterNotesPerStep <= 0.0)
        return 0;
    const double phase = ppq / quarterNotesPerStep;
    int step = static_cast<int>(std::floor(phase + 1.0e-9));
    swing = std::clamp(swing, 0.0, 0.49);
    if (step > 0 && (step & 1) && phase + 1.0e-9 < step + swing)
        --step;
    return std::max(0, step);
}
}
