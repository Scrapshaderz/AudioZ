// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AudioData.h"
#include "Mp3Encoder.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace audioz::ui
{

struct LoadResult
{
    bool ok = false;
    juce::String error;
    audioz::AudioData data;
    int bitsPerSample = 0;
    bool isFloat = false;
    juce::String formatName;  // z. B. "WAV", "FLAC", "MP3"
};

/** Liest eine Audiodatei komplett in den Speicher (WAV, AIFF, FLAC, MP3, OGG). */
LoadResult loadAudioFile (juce::AudioFormatManager& formats, const juce::File& file);

enum class ExportFormat
{
    Wav,
    Flac,
    Mp3
};

struct ExportOptions
{
    ExportFormat format = ExportFormat::Wav;
    int bitsPerSample = 24;  // WAV: 16, 24 oder 32 (= Float); FLAC: 16 oder 24
    bool dither = true;      // TPDF-Dither bei 16 Bit
    audioz::Mp3Settings mp3;
};

juce::String exportExtension (ExportFormat format);

/** Format anhand der Dateiendung; Wav, wenn unbekannt. */
ExportFormat exportFormatForFile (const juce::File& file);

/** Schreibt einen Abschnitt in eine Datei. Die Zieldatei bleibt unveraendert, wenn
    das Schreiben fehlschlaegt (es wird erst in eine temporaere Datei geschrieben). */
bool saveAudioFile (const juce::File& file, const audioz::AudioData& audio, size_t start, size_t length,
                    const ExportOptions& options, juce::String& error);

}  // namespace audioz::ui
