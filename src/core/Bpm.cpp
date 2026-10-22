// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Bpm.h"

extern "C"
{
#include <aubio.h>
}

#include <algorithm>
#include <cmath>
#include <vector>

namespace audioz
{

namespace
{
    constexpr double kMinBpm = 55.0;
    constexpr double kMaxBpm = 200.0;
    constexpr double kBpmStep = 0.05;
    constexpr double kPreferredBpm = 125.0;  // Schwerpunkt der Gewichtung gegen Oktavfehler
    constexpr double kPreferenceWidthOctaves = 1.0;
    constexpr int kMinOnsets = 8;         // weniger Einsaetze: kein erkennbarer Takt
    constexpr double kMinOnsetCrest = 5.0; // Spitzenwert / Effektivwert der Einsatz-Kurve

    /** Einsatz-Staerke ueber die Zeit (ein Wert je Hop) aus aubios Spectral-Flux-Detektor. */
    std::vector<float> onsetStrength (const AudioData& audio, size_t start, size_t length,
                                      unsigned hop, int& onsetCount)
    {
        const int numChannels = audio.numChannels();
        const unsigned window = hop * 4;

        std::vector<float> odf;
        onsetCount = 0;

        aubio_onset_t* onset = new_aubio_onset ("specflux", window, hop, (uint_t) audio.sampleRate);
        fvec_t* input = new_fvec (hop);
        fvec_t* output = new_fvec (1);

        if (onset == nullptr || input == nullptr || output == nullptr)
        {
            if (onset != nullptr) del_aubio_onset (onset);
            if (input != nullptr) del_fvec (input);
            if (output != nullptr) del_fvec (output);
            return odf;
        }

        const float scale = 1.0f / (float) numChannels;
        odf.reserve (length / hop + 1);

        for (size_t pos = 0; pos + hop <= length; pos += hop)
        {
            for (unsigned i = 0; i < hop; ++i)
            {
                float sum = 0.0f;
                for (int c = 0; c < numChannels; ++c)
                    sum += audio.channels[(size_t) c][start + pos + i];
                input->data[i] = sum * scale;
            }

            aubio_onset_do (onset, input, output);

            if (output->data[0] != 0.0f)
                ++onsetCount;

            odf.push_back ((float) aubio_onset_get_descriptor (onset));
        }

        del_aubio_onset (onset);
        del_fvec (input);
        del_fvec (output);
        return odf;
    }

    /** Zieht den lokalen Mittelwert ab und schneidet Negatives ab (hebt Einsaetze hervor). */
    void emphasisePeaks (std::vector<float>& x, size_t halfWindow)
    {
        const size_t n = x.size();
        std::vector<double> prefix (n + 1, 0.0);
        for (size_t i = 0; i < n; ++i)
            prefix[i + 1] = prefix[i] + (double) x[i];

        std::vector<float> out (n);
        for (size_t i = 0; i < n; ++i)
        {
            const size_t from = i > halfWindow ? i - halfWindow : 0;
            const size_t to = std::min (n, i + halfWindow + 1);
            const double mean = (prefix[to] - prefix[from]) / (double) (to - from);
            out[i] = (float) std::max (0.0, (double) x[i] - mean);
        }

        x = std::move (out);
    }
}

BpmResult detectBpm (const AudioData& audio, size_t start, size_t length)
{
    BpmResult result;
    audio.clampRange (start, length);

    const int sampleRate = audio.sampleRate;
    if (audio.numChannels() < 1 || sampleRate < 8000)
        return result;

    // Hoechstens 150 s aus der Mitte analysieren (Intros sind selten repraesentativ)
    const size_t maxFrames = (size_t) sampleRate * 150;
    if (length > maxFrames)
    {
        start += (length - maxFrames) / 2;
        length = maxFrames;
    }

    if (length < (size_t) sampleRate * 6)
        return result;  // unter 6 s laesst sich kein Tempo sinnvoll bestimmen

    result.analysedSeconds = (double) length / (double) sampleRate;

    const unsigned hop = sampleRate <= 50000 ? 128u : (sampleRate <= 100000 ? 256u : 512u);
    const double framesPerSecond = (double) sampleRate / (double) hop;

    std::vector<float> odf = onsetStrength (audio, start, length, hop, result.onsetCount);
    if (odf.size() < (size_t) (framesPerSecond * 5.0))
        return result;

    // Anfang und Ende wegschneiden: Dort erzeugt der abrupte Beginn des Abschnitts
    // einen scheinbaren Einsatz, der die Auswertung verzerren wuerde.
    const size_t trim = (size_t) (framesPerSecond * 0.3);
    odf.erase (odf.end() - (std::ptrdiff_t) trim, odf.end());
    odf.erase (odf.begin(), odf.begin() + (std::ptrdiff_t) trim);

    emphasisePeaks (odf, (size_t) (framesPerSecond * 0.4));

    const size_t n = odf.size();
    double energy = 0.0;
    for (float v : odf)
        energy += (double) v * (double) v;

    if (energy < 1.0e-9 || result.onsetCount < kMinOnsets)
        return result;  // keine oder zu wenige Einsaetze (Stille, Dauerton)

    // Rhythmische Musik hat spitze, selten auftretende Einsaetze. Bleibt die Kurve glatt
    // (niedriger Crest-Faktor), liegt kein Takt vor, sondern z. B. ein Dauerton.
    {
        float peak = 0.0f;
        for (float v : odf)
            peak = std::max (peak, v);

        const double crest = (double) peak / std::sqrt (energy / (double) odf.size());
        if (crest < kMinOnsetCrest)
            return result;
    }

    // Autokorrelation (ohne Verzerrung durch die endliche Laenge) bis zum vierfachen
    // der laengsten Periode, damit Vielfache der Periode mitbewertet werden koennen.
    const size_t maxLag = std::min (n / 2, (size_t) std::ceil (framesPerSecond * 60.0 / kMinBpm * 4.0) + 2);
    std::vector<double> acf (maxLag + 2, 0.0);

    for (size_t lag = 0; lag <= maxLag; ++lag)
    {
        double sum = 0.0;
        for (size_t i = 0; i + lag < n; ++i)
            sum += (double) odf[i] * (double) odf[i + lag];
        acf[lag] = sum / (double) (n - lag);
    }

    auto acfAt = [&] (double lag) -> double
    {
        const size_t i = (size_t) lag;
        if (i + 1 > maxLag)
            return 0.0;
        const double frac = lag - (double) i;
        return acf[i] + (acf[i + 1] - acf[i]) * frac;
    };

    auto score = [&] (double bpm) -> double
    {
        const double lag = framesPerSecond * 60.0 / bpm;
        double sum = 0.0, weights = 0.0;

        for (int multiple : { 1, 2, 4 })
        {
            if (lag * multiple > (double) maxLag)
                break;

            const double w = 1.0 / (double) multiple;
            sum += w * acfAt (lag * multiple);
            weights += w;
        }

        const double octaves = std::log2 (bpm / kPreferredBpm) / kPreferenceWidthOctaves;
        return (weights > 0.0 ? sum / weights : 0.0) * std::exp (-0.5 * octaves * octaves);
    };

    const int steps = (int) std::floor ((kMaxBpm - kMinBpm) / kBpmStep);
    std::vector<double> scores ((size_t) steps + 1);
    double best = -1.0, total = 0.0;
    int bestIndex = 0;

    for (int i = 0; i <= steps; ++i)
    {
        scores[(size_t) i] = score (kMinBpm + kBpmStep * i);
        total += scores[(size_t) i];

        if (scores[(size_t) i] > best)
        {
            best = scores[(size_t) i];
            bestIndex = i;
        }
    }

    if (best <= 0.0)
        return result;

    // Parabolische Verfeinerung um das Maximum
    double bpm = kMinBpm + kBpmStep * bestIndex;
    if (bestIndex > 0 && bestIndex < steps)
    {
        const double a = scores[(size_t) bestIndex - 1], b = scores[(size_t) bestIndex], c = scores[(size_t) bestIndex + 1];
        const double denom = a - 2.0 * b + c;
        if (denom < -1.0e-18)
            bpm += kBpmStep * 0.5 * (a - c) / denom;
    }

    const double mean = total / (double) (steps + 1);
    result.bpm = bpm;
    result.confidence = std::min (1.0, std::max (0.0, 1.0 - mean / best));
    result.ok = true;
    return result;
}

}  // namespace audioz
