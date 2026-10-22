// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Gain.h"

#include <cmath>

namespace audioz
{

double dbToGain (double db)
{
    return std::pow (10.0, db / 20.0);
}

double gainToDb (double gain)
{
    return gain > 0.0 ? 20.0 * std::log10 (gain) : -HUGE_VAL;
}

void applyGain (AudioData& audio, size_t start, size_t length, double gain)
{
    audio.clampRange (start, length);
    const float g = (float) gain;

    for (auto& ch : audio.channels)
        for (size_t i = start; i < start + length; ++i)
            ch[i] *= g;
}

PeakNormalizeResult normalizePeak (AudioData& audio, size_t start, size_t length, double targetPeakDb)
{
    PeakNormalizeResult result;
    audio.clampRange (start, length);

    const float peak = audio.samplePeak (start, length);
    if (peak <= 1.0e-9f)
        return result;

    result.peakDbBefore = gainToDb (peak);
    result.gainDb = targetPeakDb - result.peakDbBefore;
    applyGain (audio, start, length, dbToGain (result.gainDb));
    result.ok = true;
    return result;
}

}  // namespace audioz
