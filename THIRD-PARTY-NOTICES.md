# Hinweise zu Fremdbibliotheken

AudioZ ist freie Software unter der GNU General Public License v3 (oder neuer).
Es verwendet die folgenden Bibliotheken. Ihre Lizenztexte liegen den jeweiligen Quellpaketen bei
und werden beim Bauen von CMake heruntergeladen (Ordner `_deps` im Build-Verzeichnis).

- **JUCE** 9 – Copyright Raw Material Software Limited – AGPL-3.0 – https://github.com/juce-framework/JUCE
- **Rubber Band Library** – Copyright Particular Programs Ltd (Breakfast Quay) – GPL-2.0-or-later – https://breakfastquay.com/rubberband/
- **libebur128** – Copyright Jan Kokemüller – MIT – https://github.com/jiixyj/libebur128
- **aubio** – Copyright Paul Brossier – GPL-3.0-or-later – https://aubio.org
- **LAME** (nur wenn MP3-Export aktiviert ist) – LGPL-2.0-or-later – https://lame.sourceforge.io

Die Vorlagen für Dateiformate (WAV, AIFF, FLAC, Ogg Vorbis, MP3-Dekodierung) stammen aus JUCE und enthalten
dort die Bibliotheken der Xiph.Org Foundation (FLAC, Ogg, Vorbis; BSD-Lizenz) sowie einen MP3-Decoder
(Bestandteil von JUCE).
