// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "LoudnessNormalize.h"

#include "Gain.h"
#include "Limiter.h"

namespace audioz
{

LoudnessNormalizeResult normalizeLoudness (AudioData& audio, size_t start, size_t length,
                                           const LoudnessNormalizeParams& params)
{
    LoudnessNormalizeResult result;
    audio.clampRange (start, length);

    const LoudnessStats before = measureLoudness (audio, start, length);
    if (! before.valid)
        return result;

    result.measuredLufs = before.integratedLufs;
    result.appliedGainDb = params.targetLufs - before.integratedLufs;
    result.predictedTruePeakDb = before.truePeakDb + result.appliedGainDb;

    applyGain (audio, start, length, dbToGain (result.appliedGainDb));

    if (params.useLimiter && result.predictedTruePeakDb > params.truePeakCeilingDb)
    {
        result.limiterUsed = true;
        result.limiterReductionDb = limitToTruePeak (audio, start, length, params.truePeakCeilingDb);
    }

    result.after = measureLoudness (audio, start, length);
    result.status = NormalizeStatus::Ok;
    return result;
}

}  // namespace audioz
