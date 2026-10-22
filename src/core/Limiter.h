// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AudioData.h"

namespace audioz
{

struct LimiterParams
{
    double ceilingDb = -1.0;   // Obergrenze in dBFS (gemessen auf 4-fach oversampelten Spitzen)
    double lookaheadMs = 2.0;  // Vorausschau: so frueh beginnt die Absenkung vor einer Spitze
    double releaseMs = 80.0;   // Zeit, in der die Absenkung wieder zurueckgeht
};

/** Brickwall-Limiter mit Vorausschau und 4-fach oversampelter Spitzenerkennung.
    Alle Kanaele werden mit derselben Verstaerkung bearbeitet (Stereobild bleibt erhalten).
    Rueckgabe: groesste Pegelabsenkung in dB (0 = nichts begrenzt). */
double applyLimiter (AudioData& audio, size_t start, size_t length, const LimiterParams& params);

/** Begrenzt so, dass der True Peak (gemessen nach EBU R128) die Obergrenze nicht
    ueberschreitet. Bei Bedarf wird der Limiter mit etwas niedrigerer Obergrenze
    wiederholt. Rueckgabe: groesste Pegelabsenkung in dB. */
double limitToTruePeak (AudioData& audio, size_t start, size_t length, double ceilingDb);

}  // namespace audioz
