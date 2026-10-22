// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <vector>

namespace audioz
{

/** Mehrkanalige Audiodaten im Speicher (32-Bit-Float, nicht interleaved).

    Die Kern-Bibliothek kennt weder JUCE noch Dateien oder Oberflaeche. Dadurch
    lassen sich alle Bearbeitungsfunktionen ohne GUI testen.
*/
struct AudioData
{
    int sampleRate = 44100;
    std::vector<std::vector<float>> channels;

    AudioData() = default;
    AudioData (int numChannels, size_t numFrames, int sampleRateHz);

    int numChannels() const { return (int) channels.size(); }
    size_t numFrames() const { return channels.empty() ? 0 : channels[0].size(); }
    bool empty() const { return numFrames() == 0; }
    double durationSeconds() const;
    size_t memoryBytes() const;

    /** Begrenzt (start, length) auf den gueltigen Bereich. */
    void clampRange (size_t& start, size_t& length) const;

    /** Kopie eines Abschnitts. */
    AudioData slice (size_t start, size_t length) const;

    /** Entfernt einen Abschnitt (Frames dahinter ruecken nach vorn). */
    void erase (size_t start, size_t length);

    /** Fuegt Audio an einer Position ein. Abweichende Kanalzahlen werden angepasst. */
    void insert (size_t position, const AudioData& other);

    /** Ersetzt einen Abschnitt durch anderes Audio (die Laenge darf sich aendern). */
    void replace (size_t start, size_t length, const AudioData& replacement);

    /** Setzt einen Abschnitt auf Stille. */
    void silence (size_t start, size_t length);

    /** Groesster Betrag aller Samples im Abschnitt (linear, 1.0 = Vollaussteuerung). */
    float samplePeak (size_t start, size_t length) const;
};

/** Passt die Kanalzahl an: Mono wird verdoppelt, mehrere Kanaele werden fuer Mono
    gemittelt, ueberzaehlige Kanaele fallen weg. */
AudioData adaptChannels (const AudioData& in, int targetChannels);

}  // namespace audioz
