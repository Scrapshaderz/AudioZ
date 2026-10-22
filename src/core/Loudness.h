// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AudioData.h"

#include <cmath>
#include <cstddef>
#include <limits>

namespace audioz
{

constexpr size_t kWholeFile = std::numeric_limits<size_t>::max();

/** Messwerte nach ITU-R BS.1770 / EBU R128 (libebur128). */
struct LoudnessStats
{
    bool valid = false;                    // false: zu kurz (< 0,4 s) oder still
    double integratedLufs = -HUGE_VAL;     // integrierte Lautheit in LUFS
    double loudnessRangeLu = 0.0;          // Lautheitsumfang (LRA) in LU
    double samplePeakDb = -HUGE_VAL;       // Sample-Peak in dBFS
    double truePeakDb = -HUGE_VAL;         // True Peak in dBTP (4-fach oversampled)
};

/** Misst einen Abschnitt (Standard: ganze Datei). Mono wird wie Dual-Mono behandelt,
    also so, als lief es auf beiden Lautsprechern. */
LoudnessStats measureLoudness (const AudioData& audio, size_t start = 0, size_t length = kWholeFile);

/** Nur der True Peak in dBTP (deutlich schneller als die volle Messung). */
double measureTruePeakDb (const AudioData& audio, size_t start = 0, size_t length = kWholeFile);

}  // namespace audioz
