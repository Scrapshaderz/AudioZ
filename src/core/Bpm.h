// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AudioData.h"
#include "Loudness.h"  // kWholeFile

namespace audioz
{

struct BpmResult
{
    bool ok = false;            // false: zu kurz oder kein erkennbarer Takt
    double bpm = 0.0;           // erkanntes Tempo in Schlaegen pro Minute
    double confidence = 0.0;    // 0..1: wie eindeutig sich ein Tempo aus den Daten abhebt
    int onsetCount = 0;         // Anzahl erkannter Einsaetze (Anschlaege) im Analysefenster
    double analysedSeconds = 0.0;
};

/** Misst das Tempo (55 bis 200 BPM).

    Einsatz-Erkennung: aubio (Spectral Flux). Daraus wird die Einsatz-Staerke ueber die Zeit
    per Autokorrelation ausgewertet; das erlaubt eine Genauigkeit von deutlich unter 1 BPM.
    Analysiert hoechstens 150 s aus der Mitte des Abschnitts.

    Hinweis: Bei der Tempo-Erkennung sind Halbierung und Verdopplung typische Fehler;
    die Oberflaeche zeigt deshalb auch 1/2 und 2x an. */
BpmResult detectBpm (const AudioData& audio, size_t start = 0, size_t length = kWholeFile);

}  // namespace audioz
