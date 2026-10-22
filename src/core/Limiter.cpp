// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Limiter.h"

#include "Gain.h"
#include "Loudness.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <vector>

namespace audioz
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr int kTaps = 16;       // Filterlaenge je Phase
    constexpr int kTapsBefore = 7;  // Taps vor dem linken Nachbarsample
    constexpr int kPhases = 3;      // 4-faches Oversampling: Phasen 1/4, 2/4, 3/4

    struct InterpolationFilter
    {
        std::array<std::array<float, kTaps>, kPhases> taps {};
        float lebesgue = 1.0f;  // groesste Summe der Betraege einer Phase

        InterpolationFilter()
        {
            double worst = 0.0;

            for (int ph = 0; ph < kPhases; ++ph)
            {
                const double d = (double) (ph + 1) / 4.0;
                std::array<double, kTaps> h {};
                double sum = 0.0;

                for (int k = 0; k < kTaps; ++k)
                {
                    const double x = (double) (k - kTapsBefore) - d;  // Abstand zum Stuetzpunkt
                    const double sinc = std::fabs (x) < 1.0e-12 ? 1.0 : std::sin (kPi * x) / (kPi * x);
                    const double w = 0.42 + 0.5 * std::cos (kPi * x / 8.5) + 0.08 * std::cos (2.0 * kPi * x / 8.5);  // Blackman
                    h[(size_t) k] = sinc * w;
                    sum += h[(size_t) k];
                }

                double absSum = 0.0;
                for (int k = 0; k < kTaps; ++k)
                {
                    h[(size_t) k] /= sum;  // exakte Verstaerkung 1 bei Gleichanteil
                    taps[(size_t) ph][(size_t) k] = (float) h[(size_t) k];
                    absSum += std::fabs (h[(size_t) k]);
                }

                worst = std::max (worst, absSum);
            }

            lebesgue = (float) worst;
        }
    };

    const InterpolationFilter& interpolationFilter()
    {
        static const InterpolationFilter f;
        return f;
    }

    /** out[n] = Extremwert von v im Fenster [n - before, n + after] (am Rand abgeschnitten). */
    template <typename Better>
    std::vector<float> slidingExtreme (const std::vector<float>& v, size_t before, size_t after, Better better)
    {
        const size_t n = v.size();
        std::vector<float> out (n);
        std::deque<size_t> window;
        size_t next = 0;

        for (size_t i = 0; i < n; ++i)
        {
            const size_t right = std::min (n - 1, i + after);

            while (next <= right)
            {
                while (! window.empty() && ! better (v[window.back()], v[next]))
                    window.pop_back();
                window.push_back (next);
                ++next;
            }

            while (window.front() + before < i)
                window.pop_front();

            out[i] = v[window.front()];
        }

        return out;
    }
}

double applyLimiter (AudioData& audio, size_t start, size_t length, const LimiterParams& params)
{
    audio.clampRange (start, length);
    const int numChannels = audio.numChannels();
    if (numChannels == 0 || length == 0)
        return 0.0;

    const float ceilingLin = (float) dbToGain (params.ceilingDb);
    const size_t lookahead = std::max<size_t> (1, (size_t) std::llround (params.lookaheadMs * 0.001 * audio.sampleRate));
    const InterpolationFilter& filter = interpolationFilter();

    // 1) Betrag der Samples (groesster Wert ueber alle Kanaele)
    std::vector<float> peak (length, 0.0f);
    for (const auto& ch : audio.channels)
        for (size_t i = 0; i < length; ++i)
            peak[i] = std::max (peak[i], std::fabs (ch[start + i]));

    // 2) Zwischen den Samples koennen hoehere Spitzen liegen (Inter-Sample-Peaks).
    //    Nur dort rechnen, wo das theoretisch moeglich ist.
    const auto neighbourhood = slidingExtreme (peak, (size_t) kTapsBefore, (size_t) (kTaps - kTapsBefore),
                                               [] (float a, float b) { return a > b; });
    std::vector<float> truePeak = peak;

    for (size_t n = 0; n < length; ++n)
    {
        if (neighbourhood[n] * filter.lebesgue <= ceilingLin)
            continue;

        for (const auto& chData : audio.channels)
        {
            const float* x = chData.data() + start;
            const long long base = (long long) n - kTapsBefore;

            for (int ph = 0; ph < kPhases; ++ph)
            {
                float acc = 0.0f;

                for (int k = 0; k < kTaps; ++k)
                {
                    const long long idx = base + k;
                    if (idx >= 0 && idx < (long long) length)
                        acc += x[idx] * filter.taps[(size_t) ph][(size_t) k];
                }

                truePeak[n] = std::max (truePeak[n], std::fabs (acc));
            }
        }
    }

    // 3) Gewuenschte Verstaerkung je Sample
    std::vector<float> required (length, 1.0f);
    bool needed = false;

    for (size_t n = 0; n < length; ++n)
        if (truePeak[n] > ceilingLin)
        {
            required[n] = ceilingLin / truePeak[n];
            needed = true;
        }

    if (! needed)
        return 0.0;

    // 4) Minimum im Fenster +-lookahead, danach Mittelwert ueber [n - lookahead, n].
    //    Jeder Mittelwert-Eintrag enthaelt n in seinem Minimum-Fenster, daher bleibt
    //    die Verstaerkung garantiert <= der gewuenschten.
    const auto windowMin = slidingExtreme (required, lookahead, lookahead, [] (float a, float b) { return a < b; });

    std::vector<double> prefix (length + 1, 0.0);
    for (size_t n = 0; n < length; ++n)
        prefix[n + 1] = prefix[n] + (double) windowMin[n];

    std::vector<float> gain (length);
    for (size_t n = 0; n < length; ++n)
    {
        const size_t from = n >= lookahead ? n - lookahead : 0;
        gain[n] = (float) ((prefix[n + 1] - prefix[from]) / (double) (n - from + 1));
    }

    // 5) Langsames Zurueckgehen der Absenkung (Release); nie ueber der Zielverstaerkung
    const double releaseSamples = std::max (1.0, params.releaseMs * 0.001 * audio.sampleRate);
    const float releaseCoef = (float) (1.0 - std::exp (-1.0 / releaseSamples));
    float minGain = 1.0f;
    float state = gain[0];

    for (size_t n = 0; n < length; ++n)
    {
        state = std::min (gain[n], state + (1.0f - state) * releaseCoef);
        gain[n] = state;
        minGain = std::min (minGain, state);
    }

    // 6) Anwenden
    for (auto& ch : audio.channels)
        for (size_t n = 0; n < length; ++n)
            ch[start + n] *= gain[n];

    return -gainToDb ((double) minGain);
}

double limitToTruePeak (AudioData& audio, size_t start, size_t length, double ceilingDb)
{
    audio.clampRange (start, length);
    if (length == 0)
        return 0.0;

    if (measureTruePeakDb (audio, start, length) <= ceilingDb)
        return 0.0;

    const AudioData original = audio.slice (start, length);
    LimiterParams params;
    params.ceilingDb = ceilingDb;

    double reduction = 0.0;
    AudioData work = original;

    for (int pass = 0; pass < 5; ++pass)
    {
        work = original;
        reduction = applyLimiter (work, 0, work.numFrames(), params);

        const double tp = measureTruePeakDb (work, 0, work.numFrames());
        if (tp <= ceilingDb + 0.005)
            break;

        params.ceilingDb -= (tp - ceilingDb) + 0.01;  // Rest-Ueberschreitung ausgleichen
    }

    audio.replace (start, length, work);
    return reduction;
}

}  // namespace audioz
