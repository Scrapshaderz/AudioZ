// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AudioData.h"
#include "Loudness.h"

namespace audioz
{

struct LoudnessNormalizeParams
{
    double targetLufs = -14.0;        // Ziel-Lautheit (integriert) in LUFS
    double truePeakCeilingDb = -1.0;  // Obergrenze fuer den True Peak in dBTP
    bool useLimiter = true;           // Spitzen mit dem Limiter begrenzen, falls noetig
};

enum class NormalizeStatus
{
    Ok,
    TooShortOrSilent  // Messung nicht moeglich: unter 0,4 s oder digitale Stille
};

struct LoudnessNormalizeResult
{
    NormalizeStatus status = NormalizeStatus::TooShortOrSilent;
    double measuredLufs = 0.0;          // Lautheit vor der Bearbeitung
    double appliedGainDb = 0.0;         // Verstaerkung, um das Ziel zu erreichen
    double predictedTruePeakDb = 0.0;   // True Peak nach der Verstaerkung, ohne Limiter
    bool limiterUsed = false;
    double limiterReductionDb = 0.0;    // groesste Absenkung durch den Limiter
    LoudnessStats after;                // Messung des Ergebnisses
};

/** Misst die Lautheit des Abschnitts und verstaerkt (oder daempft) ihn auf das Ziel.
    Mit useLimiter bleibt der True Peak unter der Obergrenze; die Lautheit kann dann
    leicht unter dem Ziel liegen. Ohne Limiter kann das Ergebnis uebersteuern
    (siehe predictedTruePeakDb). */
LoudnessNormalizeResult normalizeLoudness (AudioData& audio, size_t start, size_t length,
                                           const LoudnessNormalizeParams& params);

}  // namespace audioz
