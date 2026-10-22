// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AudioData.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace audioz::ui
{

/** Min/Max-Pyramide fuer die Wellenform-Darstellung.

    Beim Herauszoomen muessten sonst bei jedem Neuzeichnen Millionen Samples gelesen
    werden. Die Pyramide fasst 32, 128, 512 ... Samples zu je einem Min/Max-Paar zusammen. */
class PeakCache
{
public:
    void build (std::shared_ptr<const audioz::AudioData> audio)
    {
        source = std::move (audio);
        levels.clear();

        if (source == nullptr || source->empty())
            return;

        const size_t numChannels = source->channels.size();
        const size_t frames = source->numFrames();

        Level first;
        first.blockSize = kBaseBlock;
        first.mins.resize (numChannels);
        first.maxs.resize (numChannels);

        const size_t blocks = (frames + kBaseBlock - 1) / kBaseBlock;
        for (size_t c = 0; c < numChannels; ++c)
        {
            first.mins[c].resize (blocks);
            first.maxs[c].resize (blocks);
            const float* x = source->channels[c].data();

            for (size_t b = 0; b < blocks; ++b)
            {
                const size_t from = b * (size_t) kBaseBlock;
                const size_t to = std::min (frames, from + (size_t) kBaseBlock);
                float lo = x[from], hi = x[from];

                for (size_t i = from + 1; i < to; ++i)
                {
                    lo = std::min (lo, x[i]);
                    hi = std::max (hi, x[i]);
                }

                first.mins[c][b] = lo;
                first.maxs[c][b] = hi;
            }
        }

        levels.push_back (std::move (first));

        while (levels.back().mins[0].size() > 64)
        {
            const Level& prev = levels.back();
            Level next;
            next.blockSize = prev.blockSize * (int64_t) kFactor;
            next.mins.resize (numChannels);
            next.maxs.resize (numChannels);
            const size_t count = (prev.mins[0].size() + kFactor - 1) / kFactor;

            for (size_t c = 0; c < numChannels; ++c)
            {
                next.mins[c].resize (count);
                next.maxs[c].resize (count);

                for (size_t b = 0; b < count; ++b)
                {
                    const size_t from = b * kFactor;
                    const size_t to = std::min (prev.mins[c].size(), from + kFactor);
                    float lo = prev.mins[c][from], hi = prev.maxs[c][from];

                    for (size_t i = from + 1; i < to; ++i)
                    {
                        lo = std::min (lo, prev.mins[c][i]);
                        hi = std::max (hi, prev.maxs[c][i]);
                    }

                    next.mins[c][b] = lo;
                    next.maxs[c][b] = hi;
                }
            }

            levels.push_back (std::move (next));
        }
    }

    bool isEmpty() const { return source == nullptr || levels.empty(); }

    /** Kleinster und groesster Wert im Bereich [from, to) (Frames). */
    void getMinMax (int channel, int64_t from, int64_t to, float& minValue, float& maxValue) const
    {
        minValue = 0.0f;
        maxValue = 0.0f;

        if (isEmpty() || channel < 0 || (size_t) channel >= source->channels.size())
            return;

        const int64_t total = (int64_t) source->numFrames();
        from = std::clamp<int64_t> (from, 0, total - 1);
        to = std::clamp<int64_t> (to, from + 1, total);

        const auto& samples = source->channels[(size_t) channel];
        const int64_t span = to - from;

        if (span <= 2 * kBaseBlock)
        {
            float lo = samples[(size_t) from], hi = lo;
            for (int64_t i = from + 1; i < to; ++i)
            {
                lo = std::min (lo, samples[(size_t) i]);
                hi = std::max (hi, samples[(size_t) i]);
            }

            minValue = lo;
            maxValue = hi;
            return;
        }

        size_t level = 0;
        while (level + 1 < levels.size() && levels[level + 1].blockSize <= span)
            ++level;

        const Level& l = levels[level];
        const size_t b0 = (size_t) (from / l.blockSize);
        const size_t b1 = std::min (l.mins[(size_t) channel].size() - 1, (size_t) ((to - 1) / l.blockSize));

        float lo = l.mins[(size_t) channel][b0], hi = l.maxs[(size_t) channel][b0];
        for (size_t b = b0 + 1; b <= b1; ++b)
        {
            lo = std::min (lo, l.mins[(size_t) channel][b]);
            hi = std::max (hi, l.maxs[(size_t) channel][b]);
        }

        minValue = lo;
        maxValue = hi;
    }

private:
    static constexpr int64_t kBaseBlock = 32;
    static constexpr size_t kFactor = 4;

    struct Level
    {
        int64_t blockSize = kBaseBlock;
        std::vector<std::vector<float>> mins, maxs;  // je Kanal
    };

    std::shared_ptr<const audioz::AudioData> source;
    std::vector<Level> levels;
};

}  // namespace audioz::ui
