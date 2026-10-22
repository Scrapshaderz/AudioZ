// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioIO.h"

#include "Dither.h"
#include "Loudness.h"  // kWholeFile

#include <algorithm>

namespace audioz::ui
{

namespace
{
    constexpr juce::int64 kMaxBytesInMemory = 3ll * 1024 * 1024 * 1024;
    constexpr int kChunkFrames = 1 << 16;
}

LoadResult loadAudioFile (juce::AudioFormatManager& formats, const juce::File& file)
{
    LoadResult result;

    if (! file.existsAsFile())
    {
        result.error = juce::String::fromUTF8 ("Die Datei existiert nicht mehr.");
        return result;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        result.error = juce::String::fromUTF8 ("Das Dateiformat wird nicht unterstützt oder die Datei ist beschädigt.");
        return result;
    }

    const int numChannels = (int) reader->numChannels;
    const juce::int64 length = reader->lengthInSamples;
    const double rate = reader->sampleRate;

    if (numChannels < 1 || length <= 0 || rate < 1000.0)
    {
        result.error = juce::String::fromUTF8 ("Die Datei enthält keine Audiodaten.");
        return result;
    }

    if (length * numChannels * (juce::int64) sizeof (float) > kMaxBytesInMemory)
    {
        result.error = juce::String::fromUTF8 ("Die Datei ist zu groß, um sie vollständig im Arbeitsspeicher zu bearbeiten (Grenze: 3 GB).");
        return result;
    }

    audioz::AudioData data (numChannels, (size_t) length, (int) std::llround (rate));
    std::vector<float*> pointers ((size_t) numChannels);

    juce::int64 position = 0;
    while (position < length)
    {
        const int count = (int) std::min<juce::int64> (kChunkFrames, length - position);

        for (int c = 0; c < numChannels; ++c)
            pointers[(size_t) c] = data.channels[(size_t) c].data() + position;

        if (! reader->read (pointers.data(), numChannels, position, count))
            break;

        position += count;
    }

    if (position <= 0)
    {
        result.error = juce::String::fromUTF8 ("Die Audiodaten konnten nicht gelesen werden.");
        return result;
    }

    if (position < length)  // Datei war kuerzer als angekuendigt oder teilweise defekt
        for (auto& ch : data.channels)
            ch.resize ((size_t) position);

    result.data = std::move (data);
    result.bitsPerSample = (int) reader->bitsPerSample;
    result.isFloat = reader->usesFloatingPointData;
    result.formatName = file.getFileExtension().trimCharactersAtStart (".").toUpperCase();
    result.ok = true;
    return result;
}

juce::String exportExtension (ExportFormat format)
{
    switch (format)
    {
        case ExportFormat::Flac: return ".flac";
        case ExportFormat::Mp3:  return ".mp3";
        case ExportFormat::Wav:  break;
    }

    return ".wav";
}

ExportFormat exportFormatForFile (const juce::File& file)
{
    const auto ext = file.getFileExtension().toLowerCase();
    if (ext == ".flac") return ExportFormat::Flac;
    if (ext == ".mp3")  return ExportFormat::Mp3;
    return ExportFormat::Wav;
}

namespace
{
    bool writeEncodedFile (const juce::File& target, const std::vector<unsigned char>& bytes, juce::String& error)
    {
        juce::TemporaryFile temp (target);

        if (! temp.getFile().replaceWithData (bytes.data(), bytes.size()) || ! temp.overwriteTargetFileWithTemporary())
        {
            error = juce::String::fromUTF8 ("Die Datei konnte nicht geschrieben werden. Prüfen Sie Schreibrechte und freien Speicherplatz.");
            return false;
        }

        return true;
    }
}

bool saveAudioFile (const juce::File& file, const audioz::AudioData& audio, size_t start, size_t length,
                    const ExportOptions& options, juce::String& error)
{
    audio.clampRange (start, length);

    if (audio.numChannels() < 1 || length == 0)
    {
        error = juce::String::fromUTF8 ("Es gibt keine Audiodaten zum Speichern.");
        return false;
    }

    if (options.format == ExportFormat::Mp3)
    {
        std::string message;
        const auto bytes = audioz::encodeMp3 (audio, start, length, options.mp3, &message);

        if (bytes.empty())
        {
            error = juce::String::fromUTF8 (message.c_str());
            return false;
        }

        return writeEncodedFile (file, bytes, error);
    }

    // WAV und FLAC ueber JUCE
    const bool wantsDither = options.dither && options.bitsPerSample == 16;
    audioz::AudioData dithered;
    const audioz::AudioData* source = &audio;
    size_t sourceStart = start;

    if (wantsDither)
    {
        dithered = audio.slice (start, length);
        audioz::addTpdfDither (dithered, 16);
        source = &dithered;
        sourceStart = 0;
    }

    juce::TemporaryFile temp (file);
    std::unique_ptr<juce::OutputStream> stream = temp.getFile().createOutputStream();

    if (stream == nullptr)
    {
        error = juce::String::fromUTF8 ("Die Datei konnte nicht zum Schreiben geöffnet werden. Prüfen Sie Schreibrechte und Pfad.");
        return false;
    }

    juce::WavAudioFormat wav;
    juce::FlacAudioFormat flac;
    juce::AudioFormat& format = options.format == ExportFormat::Flac ? static_cast<juce::AudioFormat&> (flac)
                                                                     : static_cast<juce::AudioFormat&> (wav);

    int bits = options.bitsPerSample;
    if (options.format == ExportFormat::Flac)
        bits = bits >= 24 ? 24 : 16;
    else if (bits != 16 && bits != 24 && bits != 32)
        bits = 24;

    auto writerOptions = juce::AudioFormatWriterOptions()
                             .withSampleRate ((double) audio.sampleRate)
                             .withNumChannels (audio.numChannels())
                             .withBitsPerSample (bits);

    if (options.format == ExportFormat::Wav && bits == 32)
        writerOptions = writerOptions.withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);

    std::unique_ptr<juce::AudioFormatWriter> writer = format.createWriterFor (stream, writerOptions);

    if (writer == nullptr)
    {
        error = juce::String::fromUTF8 ("Dieses Format unterstützt die gewählten Einstellungen nicht (z. B. zu viele Kanäle).");
        return false;
    }

    std::vector<const float*> pointers ((size_t) audio.numChannels());
    size_t position = 0;

    while (position < length)
    {
        const int count = (int) std::min<size_t> ((size_t) kChunkFrames, length - position);

        for (int c = 0; c < audio.numChannels(); ++c)
            pointers[(size_t) c] = source->channels[(size_t) c].data() + sourceStart + position;

        if (! writer->writeFromFloatArrays (pointers.data(), audio.numChannels(), count))
        {
            error = juce::String::fromUTF8 ("Beim Schreiben ist ein Fehler aufgetreten (Speicherplatz voll?).");
            return false;
        }

        position += (size_t) count;
    }

    writer.reset();  // schliesst auch den Stream und schreibt den Dateikopf fertig

    if (! temp.overwriteTargetFileWithTemporary())
    {
        error = juce::String::fromUTF8 ("Die Zieldatei konnte nicht ersetzt werden. Ist sie in einem anderen Programm geöffnet?");
        return false;
    }

    return true;
}

}  // namespace audioz::ui
