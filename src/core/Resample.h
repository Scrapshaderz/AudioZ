// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AudioData.h"

#include <vector>

namespace audioz
{

/** Resampling mit Kaiser-gefenstertem Sinc-Filter (hohe Qualitaet).

    `step` ist der Abstand zweier Ausgabe-Samples gemessen in Eingabe-Samples.
    Die Ausgabe hat floor(n / step) Samples. Bei step > 1 wird vorher tiefpass-
    gefiltert, damit keine Alias-Frequenzen entstehen.
*/
std::vector<float> resampleChannel (const float* in, size_t numSamples, double step);

/** Spielt das Audio schneller oder langsamer ab (Tempo und Tonhoehe aendern sich
    gemeinsam, wie bei einer Schallplatte). Die Abtastrate bleibt unveraendert.
    rate = 2.0: doppelte Geschwindigkeit, eine Oktave hoeher, halbe Dauer. */
AudioData varispeed (const AudioData& in, double rate);

/** Rechnet auf eine andere Abtastrate um; Dauer und Tonhoehe bleiben gleich. */
AudioData convertSampleRate (const AudioData& in, int newSampleRate);

}  // namespace audioz
