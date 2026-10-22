// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <juce_core/juce_core.h>

#include <cmath>

namespace audioz::ui
{

/** Deutscher Text (UTF-8) als juce::String; Umlaute und Sonderzeichen sind sonst nicht
    plattformunabhaengig in Zeichenketten-Literalen. */
inline juce::String de (const char* utf8)
{
    return juce::String::fromUTF8 (utf8);
}

/** Zeit als h:mm:ss.mmm bzw. m:ss.mmm (Stunden nur bei Bedarf). */
inline juce::String formatTime (double seconds, bool withMilliseconds = true)
{
    if (! std::isfinite (seconds) || seconds < 0.0)
        seconds = 0.0;

    auto totalMs = (juce::int64) std::llround (seconds * 1000.0);
    const auto ms = (int) (totalMs % 1000);
    totalMs /= 1000;
    const auto s = (int) (totalMs % 60);
    totalMs /= 60;
    const auto m = (int) (totalMs % 60);
    const auto h = (int) (totalMs / 60);

    juce::String text;
    if (h > 0)
        text << h << ":" << juce::String (m).paddedLeft ('0', 2) << ":";
    else
        text << m << ":";

    text << juce::String (s).paddedLeft ('0', 2);
    if (withMilliseconds)
        text << "." << juce::String (ms).paddedLeft ('0', 3);

    return text;
}

/** Zahl mit Komma als Dezimaltrenner, wie im Deutschen ueblich. */
inline juce::String formatNumber (double value, int decimals)
{
    return juce::String (value, decimals).replaceCharacter ('.', ',');
}

inline juce::String formatDb (double db, int decimals = 1)
{
    if (! std::isfinite (db))
        return juce::String::fromUTF8 ("−∞ dB");  // -inf

    return formatNumber (db, decimals) + " dB";
}

inline juce::String formatLufs (double lufs, int decimals = 1)
{
    if (! std::isfinite (lufs))
        return juce::String::fromUTF8 ("−∞ LUFS");

    return formatNumber (lufs, decimals) + " LUFS";
}

inline juce::String formatDbtp (double db, int decimals = 1)
{
    if (! std::isfinite (db))
        return juce::String::fromUTF8 ("−∞ dBTP");

    return formatNumber (db, decimals) + " dBTP";
}

inline juce::String formatSampleRate (int hz)
{
    if (hz % 1000 == 0)
        return juce::String (hz / 1000) + " kHz";

    return formatNumber ((double) hz / 1000.0, 1) + " kHz";
}

inline juce::String formatChannels (int channels)
{
    switch (channels)
    {
        case 1:  return "Mono";
        case 2:  return "Stereo";
        default: return juce::String (channels) + juce::String::fromUTF8 (" Kanäle");
    }
}

inline juce::String formatBytes (juce::int64 bytes)
{
    if (bytes >= (1ll << 30))
        return formatNumber ((double) bytes / (double) (1ll << 30), 1) + " GB";
    if (bytes >= (1ll << 20))
        return formatNumber ((double) bytes / (double) (1ll << 20), 1) + " MB";

    return formatNumber ((double) bytes / 1024.0, 0) + " KB";
}

}  // namespace audioz::ui
