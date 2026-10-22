// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Resample.h"

#include <algorithm>
#include <cmath>

namespace audioz
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr int kZeroCrossings = 16;   // Nulldurchgaenge je Seite des Filters
    constexpr int kTableResolution = 256; // Stuetzstellen je Eingabe-Sample
    constexpr double kKaiserBeta = 9.0;

    double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        const double q = x * x * 0.25;

        for (int k = 1; k < 60; ++k)
        {
            term *= q / ((double) k * (double) k);
            sum += term;
            if (term < 1.0e-14 * sum)
                break;
        }

        return sum;
    }

    double sinc (double x)
    {
        if (std::fabs (x) < 1.0e-12)
            return 1.0;
        const double px = kPi * x;
        return std::sin (px) / px;
    }

    /** Tabelle der halben (symmetrischen) Filterantwort h(u) fuer u in [0, halfWidth]. */
    struct KernelTable
    {
        double halfWidth = 0.0;
        std::vector<float> values;

        KernelTable (double cutoff)
        {
            halfWidth = (double) kZeroCrossings / cutoff;
            const size_t n = (size_t) std::ceil (halfWidth * kTableResolution) + 2;
            values.assign (n, 0.0f);
            const double i0Beta = besselI0 (kKaiserBeta);

            for (size_t i = 0; i < n; ++i)
            {
                const double u = (double) i / (double) kTableResolution;
                const double r = u / halfWidth;

                if (r >= 1.0)
                    continue;

                const double window = besselI0 (kKaiserBeta * std::sqrt (1.0 - r * r)) / i0Beta;
                values[i] = (float) (cutoff * sinc (cutoff * u) * window);
            }
        }

        inline float at (double u) const
        {
            const double pos = std::fabs (u) * kTableResolution;
            const size_t i = (size_t) pos;
            const float frac = (float) (pos - (double) i);
            return values[i] + (values[i + 1] - values[i]) * frac;
        }
    };
}

std::vector<float> resampleChannel (const float* in, size_t numSamples, double step)
{
    if (in == nullptr || numSamples == 0 || ! (step > 0.0))
        return {};

    const size_t outLength = (size_t) std::floor ((double) numSamples / step);
    std::vector<float> out (outLength, 0.0f);

    if (std::fabs (step - 1.0) < 1.0e-12)
    {
        std::copy (in, in + std::min (numSamples, outLength), out.begin());
        return out;
    }

    const double cutoff = std::min (1.0, 1.0 / step) * 0.98;
    const KernelTable table (cutoff);
    const long long reach = (long long) std::ceil (table.halfWidth);
    const long long total = (long long) numSamples;

    for (size_t j = 0; j < outLength; ++j)
    {
        const double t = (double) j * step;
        const long long centre = (long long) std::floor (t);
        const long long first = std::max<long long> (0, centre - reach + 1);
        const long long last = std::min<long long> (total - 1, centre + reach);

        double acc = 0.0;

        for (long long k = first; k <= last; ++k)
        {
            const double u = t - (double) k;
            if (std::fabs (u) >= table.halfWidth)
                continue;

            acc += (double) in[k] * (double) table.at (u);
        }

        out[j] = (float) acc;
    }

    return out;
}

AudioData varispeed (const AudioData& in, double rate)
{
    AudioData out (0, 0, in.sampleRate);

    if (! (rate > 0.0))
        return in;

    out.channels.reserve (in.channels.size());

    for (const auto& ch : in.channels)
        out.channels.push_back (resampleChannel (ch.data(), ch.size(), rate));

    return out;
}

AudioData convertSampleRate (const AudioData& in, int newSampleRate)
{
    if (newSampleRate <= 0 || newSampleRate == in.sampleRate)
        return in;

    AudioData out = varispeed (in, (double) in.sampleRate / (double) newSampleRate);
    out.sampleRate = newSampleRate;
    return out;
}

}  // namespace audioz
