// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Loudness.h"

#include "Gain.h"

#include <ebur128.h>

#include <algorithm>
#include <vector>

namespace audioz
{

LoudnessStats measureLoudness (const AudioData& audio, size_t start, size_t length)
{
    LoudnessStats stats;
    audio.clampRange (start, length);

    const int numChannels = audio.numChannels();
    if (numChannels < 1 || length == 0 || audio.sampleRate < 8000)
        return stats;

    const int mode = EBUR128_MODE_I | EBUR128_MODE_LRA | EBUR128_MODE_SAMPLE_PEAK | EBUR128_MODE_TRUE_PEAK;
    ebur128_state* st = ebur128_init ((unsigned) numChannels, (unsigned long) audio.sampleRate, mode);
    if (st == nullptr)
        return stats;

    if (numChannels == 1)
        ebur128_set_channel (st, 0, EBUR128_DUAL_MONO);

    constexpr size_t kBlock = 8192;
    std::vector<float> interleaved (kBlock * (size_t) numChannels);

    for (size_t pos = 0; pos < length; pos += kBlock)
    {
        const size_t n = std::min (kBlock, length - pos);

        for (size_t i = 0; i < n; ++i)
            for (int c = 0; c < numChannels; ++c)
                interleaved[i * (size_t) numChannels + (size_t) c] = audio.channels[(size_t) c][start + pos + i];

        if (ebur128_add_frames_float (st, interleaved.data(), n) != EBUR128_SUCCESS)
        {
            ebur128_destroy (&st);
            return stats;
        }
    }

    double lufs = -HUGE_VAL;
    if (ebur128_loudness_global (st, &lufs) == EBUR128_SUCCESS && std::isfinite (lufs))
    {
        stats.valid = true;
        stats.integratedLufs = lufs;
    }

    double lra = 0.0;
    if (ebur128_loudness_range (st, &lra) == EBUR128_SUCCESS)
        stats.loudnessRangeLu = lra;

    double maxSample = 0.0, maxTrue = 0.0;
    for (int c = 0; c < numChannels; ++c)
    {
        double p = 0.0;
        if (ebur128_sample_peak (st, (unsigned) c, &p) == EBUR128_SUCCESS)
            maxSample = std::max (maxSample, p);
        if (ebur128_true_peak (st, (unsigned) c, &p) == EBUR128_SUCCESS)
            maxTrue = std::max (maxTrue, p);
    }

    stats.samplePeakDb = gainToDb (maxSample);
    stats.truePeakDb = gainToDb (std::max (maxTrue, maxSample));

    ebur128_destroy (&st);
    return stats;
}

double measureTruePeakDb (const AudioData& audio, size_t start, size_t length)
{
    audio.clampRange (start, length);

    const int numChannels = audio.numChannels();
    if (numChannels < 1 || length == 0 || audio.sampleRate < 8000)
        return -HUGE_VAL;

    ebur128_state* st = ebur128_init ((unsigned) numChannels, (unsigned long) audio.sampleRate, EBUR128_MODE_TRUE_PEAK);
    if (st == nullptr)
        return -HUGE_VAL;

    constexpr size_t kBlock = 8192;
    std::vector<float> interleaved (kBlock * (size_t) numChannels);

    for (size_t pos = 0; pos < length; pos += kBlock)
    {
        const size_t n = std::min (kBlock, length - pos);

        for (size_t i = 0; i < n; ++i)
            for (int c = 0; c < numChannels; ++c)
                interleaved[i * (size_t) numChannels + (size_t) c] = audio.channels[(size_t) c][start + pos + i];

        if (ebur128_add_frames_float (st, interleaved.data(), n) != EBUR128_SUCCESS)
        {
            ebur128_destroy (&st);
            return -HUGE_VAL;
        }
    }

    double maxPeak = 0.0;
    for (int c = 0; c < numChannels; ++c)
    {
        double p = 0.0;
        if (ebur128_sample_peak (st, (unsigned) c, &p) == EBUR128_SUCCESS)
            maxPeak = std::max (maxPeak, p);
        if (ebur128_true_peak (st, (unsigned) c, &p) == EBUR128_SUCCESS)
            maxPeak = std::max (maxPeak, p);
    }

    ebur128_destroy (&st);
    return gainToDb (maxPeak);
}

}  // namespace audioz
