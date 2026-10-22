// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AudioData.h"

namespace audioz
{

double dbToGain (double db);
double gainToDb (double gain);  // liefert -HUGE_VAL fuer gain <= 0

/** Multipliziert einen Abschnitt mit einem linearen Faktor. */
void applyGain (AudioData& audio, size_t start, size_t length, double gain);

struct PeakNormalizeResult
{
    bool ok = false;           // false: Abschnitt ist komplett still
    double peakDbBefore = 0.0; // Sample-Peak vor der Bearbeitung in dBFS
    double gainDb = 0.0;       // angewendete Verstaerkung in dB
};

/** Normalisiert den Sample-Peak eines Abschnitts auf einen Zielwert in dBFS. */
PeakNormalizeResult normalizePeak (AudioData& audio, size_t start, size_t length, double targetPeakDb);

}  // namespace audioz
