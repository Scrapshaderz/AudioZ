// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Mp3Encoder.h"

#include <algorithm>
#include <cstring>

#if AUDIOZ_HAS_LAME
 #include <lame/lame.h>
#endif

namespace audioz
{

#if AUDIOZ_HAS_LAME

bool mp3EncoderAvailable()
{
    return true;
}

std::vector<unsigned char> encodeMp3 (const AudioData& audio, size_t start, size_t length,
                                      const Mp3Settings& settings, std::string* error)
{
    auto fail = [error] (const char* message) -> std::vector<unsigned char>
    {
        if (error != nullptr)
            *error = message;
        return {};
    };

    audio.clampRange (start, length);
    if (audio.numChannels() < 1 || length == 0)
        return fail ("Keine Audiodaten zum Kodieren.");

    const int numChannels = audio.numChannels() == 1 ? 1 : 2;

    lame_t lame = lame_init();
    if (lame == nullptr)
        return fail ("Der MP3-Encoder konnte nicht gestartet werden.");

    lame_set_in_samplerate (lame, audio.sampleRate);
    lame_set_num_channels (lame, numChannels);
    lame_set_mode (lame, numChannels == 1 ? MONO : JOINT_STEREO);
    lame_set_quality (lame, 2);

    if (settings.vbr)
    {
        lame_set_VBR (lame, vbr_mtrh);
        lame_set_VBR_quality (lame, (float) std::min (9, std::max (0, settings.vbrQuality)));
    }
    else
    {
        lame_set_VBR (lame, vbr_off);
        lame_set_brate (lame, settings.bitrateKbps);
    }

    if (lame_init_params (lame) < 0)
    {
        lame_close (lame);
        return fail ("Die MP3-Einstellungen werden vom Encoder nicht unterstuetzt.");
    }

    constexpr size_t kBlock = 8192;
    std::vector<float> left (kBlock), right (kBlock);
    std::vector<unsigned char> block (kBlock * 5 / 4 + 7200);
    std::vector<unsigned char> output;
    output.reserve (length / 4 + 16384);

    for (size_t pos = 0; pos < length; pos += kBlock)
    {
        const size_t n = std::min (kBlock, length - pos);

        for (size_t i = 0; i < n; ++i)
        {
            left[i] = std::min (1.0f, std::max (-1.0f, audio.channels[0][start + pos + i]));
            right[i] = numChannels == 2
                           ? std::min (1.0f, std::max (-1.0f, audio.channels[1][start + pos + i]))
                           : left[i];
        }

        const int bytes = lame_encode_buffer_ieee_float (lame, left.data(), right.data(), (int) n,
                                                         block.data(), (int) block.size());
        if (bytes < 0)
        {
            lame_close (lame);
            return fail ("Beim Kodieren des MP3 ist ein Fehler aufgetreten.");
        }

        output.insert (output.end(), block.begin(), block.begin() + bytes);
    }

    const int flushed = lame_encode_flush (lame, block.data(), (int) block.size());
    if (flushed < 0)
    {
        lame_close (lame);
        return fail ("Beim Abschliessen des MP3 ist ein Fehler aufgetreten.");
    }

    output.insert (output.end(), block.begin(), block.begin() + flushed);

    // LAME reserviert am Dateianfang einen Platzhalter-Frame fuer den Xing/Info-Header.
    // Weil die Datei hier komplett im Speicher liegt, koennen wir ihn direkt ueberschreiben.
    const size_t tagSize = lame_get_lametag_frame (lame, nullptr, 0);
    if (tagSize > 0 && tagSize <= output.size())
    {
        std::vector<unsigned char> tag (tagSize);
        if (lame_get_lametag_frame (lame, tag.data(), tag.size()) == tagSize)
            std::memcpy (output.data(), tag.data(), tagSize);
    }

    lame_close (lame);
    return output;
}

#else

bool mp3EncoderAvailable()
{
    return false;
}

std::vector<unsigned char> encodeMp3 (const AudioData&, size_t, size_t, const Mp3Settings&, std::string* error)
{
    if (error != nullptr)
        *error = "Diese AudioZ-Version wurde ohne MP3-Encoder (LAME) gebaut.";
    return {};
}

#endif

}  // namespace audioz
