// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AudioData.h"

#include <cstdint>

namespace audioz
{

/** Addiert TPDF-Dither (Dreiecksrauschen, +-1 LSB) fuer die Reduktion auf
    `targetBits` Bit (z. B. 16). Sollte direkt vor dem Schreiben angewendet werden. */
void addTpdfDither (AudioData& audio, int targetBits, uint32_t seed = 0x9E3779B9u);

}  // namespace audioz
