// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AudioData.h"

#include <cmath>

namespace audioz
{

enum class PitchMode
{
    KeepTempo,            // nur die Tonhoehe aendern, Dauer und Tempo bleiben gleich (Rubber Band)
    ChangeTempoWithPitch  // Tonhoehe und Tempo gemeinsam aendern, wie bei einer Schallplatte
};

enum class PitchQuality
{
    Fast,  // Rubber Band R2: schneller
    High   // Rubber Band R3: beste Qualitaet, deutlich rechenintensiver
};

struct PitchParams
{
    double semitones = 0.0;  // Halbtoene inkl. Bruchteil (Cents / 100); positiv = hoeher
    PitchMode mode = PitchMode::KeepTempo;
    PitchQuality quality = PitchQuality::High;
};

/** Frequenzverhaeltnis fuer eine Verschiebung in Halbtoenen. */
inline double pitchRatio (double semitones)
{
    return std::pow (2.0, semitones / 12.0);
}

/** Liefert das umgerechnete Audio. Bei KeepTempo bleibt die Laenge erhalten, bei
    ChangeTempoWithPitch aendert sie sich um den Faktor 1 / pitchRatio. */
AudioData changePitch (const AudioData& input, const PitchParams& params);

}  // namespace audioz
