// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AudioData.h"

namespace audioz
{

enum class FadeCurve
{
    Linear,      // gleichmaessiger Pegelverlauf
    EqualPower,  // Sinus-Kurve, klingt bei Ueberblendungen gleichmaessig laut
    SCurve       // weicher Anfang und weiches Ende
};

/** Gewicht der Kurve fuer x in [0, 1]; 0 -> 0, 1 -> 1. */
float fadeShape (FadeCurve curve, float x);

/** Blendet den Abschnitt von Stille auf volle Lautstaerke ein. */
void applyFadeIn (AudioData& audio, size_t start, size_t length, FadeCurve curve);

/** Blendet den Abschnitt von voller Lautstaerke auf Stille aus. */
void applyFadeOut (AudioData& audio, size_t start, size_t length, FadeCurve curve);

}  // namespace audioz
