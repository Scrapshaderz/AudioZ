// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioData.h"

#include <algorithm>
#include <cmath>

namespace audioz
{

AudioData::AudioData (int numChannelsIn, size_t numFramesIn, int sampleRateHz)
    : sampleRate (sampleRateHz),
      channels ((size_t) std::max (0, numChannelsIn), std::vector<float> (numFramesIn, 0.0f))
{
}

double AudioData::durationSeconds() const
{
    return sampleRate > 0 ? (double) numFrames() / (double) sampleRate : 0.0;
}

size_t AudioData::memoryBytes() const
{
    return numFrames() * channels.size() * sizeof (float);
}

void AudioData::clampRange (size_t& start, size_t& length) const
{
    const size_t total = numFrames();
    start = std::min (start, total);
    length = std::min (length, total - start);
}

AudioData AudioData::slice (size_t start, size_t length) const
{
    clampRange (start, length);
    AudioData out ((int) channels.size(), 0, sampleRate);

    for (size_t c = 0; c < channels.size(); ++c)
        out.channels[c].assign (channels[c].begin() + (std::ptrdiff_t) start,
                                channels[c].begin() + (std::ptrdiff_t) (start + length));

    return out;
}

void AudioData::erase (size_t start, size_t length)
{
    clampRange (start, length);
    if (length == 0)
        return;

    for (auto& ch : channels)
        ch.erase (ch.begin() + (std::ptrdiff_t) start, ch.begin() + (std::ptrdiff_t) (start + length));
}

void AudioData::insert (size_t position, const AudioData& other)
{
    if (other.numFrames() == 0)
        return;

    if (channels.empty())
        channels.assign ((size_t) other.numChannels(), {});

    const AudioData adapted = adaptChannels (other, numChannels());
    position = std::min (position, numFrames());

    for (size_t c = 0; c < channels.size(); ++c)
        channels[c].insert (channels[c].begin() + (std::ptrdiff_t) position,
                            adapted.channels[c].begin(), adapted.channels[c].end());
}

void AudioData::replace (size_t start, size_t length, const AudioData& replacement)
{
    clampRange (start, length);
    erase (start, length);
    insert (start, replacement);
}

void AudioData::silence (size_t start, size_t length)
{
    clampRange (start, length);

    for (auto& ch : channels)
        std::fill (ch.begin() + (std::ptrdiff_t) start, ch.begin() + (std::ptrdiff_t) (start + length), 0.0f);
}

float AudioData::samplePeak (size_t start, size_t length) const
{
    clampRange (start, length);
    float peak = 0.0f;

    for (const auto& ch : channels)
        for (size_t i = start; i < start + length; ++i)
            peak = std::max (peak, std::fabs (ch[i]));

    return peak;
}

AudioData adaptChannels (const AudioData& in, int targetChannels)
{
    if (targetChannels <= 0 || in.numChannels() == targetChannels)
        return in;

    AudioData out (targetChannels, in.numFrames(), in.sampleRate);
    const int n = in.numChannels();

    if (n == 0)
        return out;

    if (targetChannels == 1)
    {
        const float scale = 1.0f / (float) n;

        for (size_t i = 0; i < in.numFrames(); ++i)
        {
            float sum = 0.0f;
            for (int c = 0; c < n; ++c)
                sum += in.channels[(size_t) c][i];
            out.channels[0][i] = sum * scale;
        }
    }
    else
    {
        for (int c = 0; c < targetChannels; ++c)
            out.channels[(size_t) c] = in.channels[(size_t) std::min (c, n - 1)];
    }

    return out;
}

}  // namespace audioz
