// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Fades.h"

#include <algorithm>
#include <cmath>

namespace audioz
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;
}

float fadeShape (FadeCurve curve, float x)
{
    x = std::min (1.0f, std::max (0.0f, x));

    switch (curve)
    {
        case FadeCurve::EqualPower: return (float) std::sin ((double) x * kPi * 0.5);
        case FadeCurve::SCurve:     return x * x * (3.0f - 2.0f * x);
        case FadeCurve::Linear:     break;
    }

    return x;
}

namespace
{
    void applyFade (AudioData& audio, size_t start, size_t length, FadeCurve curve, bool fadeIn)
    {
        audio.clampRange (start, length);
        if (length == 0)
            return;

        const double denom = length > 1 ? (double) (length - 1) : 1.0;

        for (size_t i = 0; i < length; ++i)
        {
            const float x = length > 1 ? (float) ((double) i / denom) : (fadeIn ? 1.0f : 0.0f);
            const float gain = fadeShape (curve, fadeIn ? x : 1.0f - x);

            for (auto& ch : audio.channels)
                ch[start + i] *= gain;
        }
    }
}

void applyFadeIn (AudioData& audio, size_t start, size_t length, FadeCurve curve)
{
    applyFade (audio, start, length, curve, true);
}

void applyFadeOut (AudioData& audio, size_t start, size_t length, FadeCurve curve)
{
    applyFade (audio, start, length, curve, false);
}

}  // namespace audioz
