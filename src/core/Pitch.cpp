// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Pitch.h"

#include "Resample.h"

#include <rubberband/RubberBandStretcher.h>

#include <algorithm>
#include <vector>

namespace audioz
{

namespace
{
    AudioData pitchKeepingTempo (const AudioData& in, double ratio, PitchQuality quality)
    {
        using RB = RubberBand::RubberBandStretcher;

        const size_t numFrames = in.numFrames();
        const int numChannels = in.numChannels();

        int options = RB::OptionProcessOffline | RB::OptionThreadingNever | RB::OptionChannelsTogether;
        options |= (quality == PitchQuality::High) ? (int) RB::OptionEngineFiner
                                                   : (int) (RB::OptionEngineFaster | RB::OptionPitchHighQuality);

        RB stretcher ((size_t) in.sampleRate, (size_t) numChannels, options, 1.0, ratio);
        stretcher.setExpectedInputDuration (numFrames);

        constexpr size_t kBlock = 4096;
        std::vector<const float*> inputPtrs ((size_t) numChannels);

        // Offline-Betrieb: erst das ganze Material analysieren, dann verarbeiten
        for (size_t pos = 0; pos < numFrames; pos += kBlock)
        {
            const size_t count = std::min (kBlock, numFrames - pos);
            for (int c = 0; c < numChannels; ++c)
                inputPtrs[(size_t) c] = in.channels[(size_t) c].data() + pos;

            stretcher.study (inputPtrs.data(), count, pos + count >= numFrames);
        }

        AudioData out (numChannels, 0, in.sampleRate);
        for (auto& ch : out.channels)
            ch.reserve (numFrames + kBlock);

        std::vector<std::vector<float>> scratch ((size_t) numChannels, std::vector<float> (kBlock));
        std::vector<float*> scratchPtrs ((size_t) numChannels);
        for (int c = 0; c < numChannels; ++c)
            scratchPtrs[(size_t) c] = scratch[(size_t) c].data();

        auto drain = [&]
        {
            for (;;)
            {
                const int available = stretcher.available();
                if (available <= 0)
                    break;

                const size_t take = std::min ((size_t) available, kBlock);
                const size_t got = stretcher.retrieve (scratchPtrs.data(), take);
                if (got == 0)
                    break;

                for (int c = 0; c < numChannels; ++c)
                    out.channels[(size_t) c].insert (out.channels[(size_t) c].end(),
                                                     scratch[(size_t) c].begin(),
                                                     scratch[(size_t) c].begin() + (std::ptrdiff_t) got);
            }
        };

        for (size_t pos = 0; pos < numFrames; pos += kBlock)
        {
            const size_t count = std::min (kBlock, numFrames - pos);
            for (int c = 0; c < numChannels; ++c)
                inputPtrs[(size_t) c] = in.channels[(size_t) c].data() + pos;

            stretcher.process (inputPtrs.data(), count, pos + count >= numFrames);
            drain();
        }

        drain();

        // Die Laenge muss exakt erhalten bleiben
        for (auto& ch : out.channels)
            ch.resize (numFrames, 0.0f);

        return out;
    }
}

AudioData changePitch (const AudioData& input, const PitchParams& params)
{
    if (input.empty() || std::fabs (params.semitones) < 1.0e-9)
        return input;

    const double ratio = pitchRatio (params.semitones);

    if (params.mode == PitchMode::ChangeTempoWithPitch)
        return varispeed (input, ratio);

    return pitchKeepingTempo (input, ratio, params.quality);
}

}  // namespace audioz
