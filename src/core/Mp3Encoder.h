// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AudioData.h"
#include "Loudness.h"  // kWholeFile

#include <string>
#include <vector>

namespace audioz
{

struct Mp3Settings
{
    int bitrateKbps = 192;  // konstante Bitrate (CBR), wenn vbr == false
    bool vbr = false;       // variable Bitrate
    int vbrQuality = 2;     // 0 (beste) bis 9 (kleinste Datei), nur bei vbr
};

/** true, wenn AudioZ mit dem LAME-MP3-Encoder gebaut wurde. */
bool mp3EncoderAvailable();

/** Kodiert einen Abschnitt als MP3 (komplette Datei im Speicher, inkl. Xing/LAME-Header).
    Bei einem Fehler ist das Ergebnis leer und `error` (falls gesetzt) enthaelt den Grund. */
std::vector<unsigned char> encodeMp3 (const AudioData& audio, size_t start, size_t length,
                                      const Mp3Settings& settings, std::string* error = nullptr);

}  // namespace audioz
