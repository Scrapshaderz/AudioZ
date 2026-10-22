// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Dither.h"

#include <cmath>

namespace audioz
{

namespace
{
    // xorshift32: schnell und fuer Dither voellig ausreichend
    inline uint32_t nextRandom (uint32_t& state)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    inline float uniform01 (uint32_t& state)
    {
        return (float) (nextRandom (state) >> 8) * (1.0f / 16777216.0f);
    }
}

void addTpdfDither (AudioData& audio, int targetBits, uint32_t seed)
{
    if (targetBits < 2 || targetBits > 24)
        return;

    const float lsb = 1.0f / (float) (1u << (targetBits - 1));
    uint32_t state = seed != 0 ? seed : 1u;

    for (auto& ch : audio.channels)
    {
        // eigener Zustand je Kanal, damit die Kanaele nicht korreliert rauschen
        state = state * 1664525u + 1013904223u;
        if (state == 0)
            state = 1;

        for (auto& s : ch)
            s += (uniform01 (state) - uniform01 (state)) * lsb;
    }
}

}  // namespace audioz
