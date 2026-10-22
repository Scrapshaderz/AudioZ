# AudioZ

AudioZ ist ein freier Audio-Editor mit Mastering-Werkzeugen für Windows, inspiriert von Programmen wie WaveLab.
Er ist in C++ mit [JUCE](https://juce.com) geschrieben und steht unter der **GNU General Public License v3**.

> Status: Version 0.1 – der erste Wurf. Die Funktionen sind getestet, aber es ist ein junges Projekt.
> AudioZ steht in keiner Verbindung zu Steinberg oder WaveLab.

## Funktionen (Version 0.1)

**Bearbeiten**
- Wellenform mit Zeitlineal, stufenlosem Zoom (bis auf einzelne Samples) und Scrollleiste
- Öffnen und Speichern: WAV (16/24/32 Bit Float), FLAC (16/24 Bit), MP3 (CBR/VBR); zusätzlich Öffnen von AIFF und OGG
- Auswahl per Maus, Ausschneiden, Kopieren, Einfügen (mit automatischer Anpassung von Abtastrate und Kanalzahl), Löschen, Zuschneiden, Stummschalten
- Beliebig viele Schritte Rückgängig/Wiederholen (Speichergrenze einstellbar)
- Verstärkung, Ein-/Ausblenden (linear, Sinus, S-Kurve)
- Wiedergabe mit Schleife, Pegelanzeige, Drag & Drop zum Öffnen

**Mastering**
- Peak-Normalisierung
- Lautheitsmessung nach ITU-R BS.1770 / EBU R128 (integriert in LUFS, Lautheitsumfang LRA, Sample-Peak, True Peak)
- **Lautheits-Normalisierung** auf frei wählbare LUFS-Werte, mit Voreinstellungen für Streaming (−14 LUFS) und Rundfunk (−23 LUFS, EBU R128) und True-Peak-Limiter
- **Tonhöhe verschieben** (Halbtöne und Cent) – wahlweise *nur die Tonhöhe* bei gleichbleibendem Tempo (Rubber Band) *oder* Tonhöhe und Tempo gemeinsam
- **Tempo-Messung (BPM)** mit Hinweis auf halbes/doppeltes Tempo, da genau diese Verwechslung der typische Fehler jeder BPM-Erkennung ist

**Oberfläche**
- Modernes, flaches Design mit hellem und dunklem Modus (oder automatisch nach Windows-Einstellung)
- Wählbare Akzentfarbe, Einstellungsdialog (Darstellung, Audio-Gerät, Bearbeitung)
- Alle Texte auf Deutsch

## Installation (Windows)

1. Auf der Seite **Releases** dieses Repositorys die Datei `AudioZ-Setup-<Version>.exe` herunterladen.
2. Ausführen und dem Assistenten folgen.

Die Setup.exe ist nicht digital signiert. Windows SmartScreen zeigt deshalb beim ersten Start eventuell
„Der Computer wurde durch Windows geschützt“ an – über *Weitere Informationen → Trotzdem ausführen* geht es weiter.
Ein Code-Signing-Zertifikat kostet Geld und lässt sich später im Workflow ergänzen.

## Selbst bauen

Voraussetzungen: CMake ab 3.24, ein C++17-Compiler (unter Windows Visual Studio 2022 mit „Desktopentwicklung mit C++“), Git.
Alle Bibliotheken (JUCE, Rubber Band, libebur128, aubio) lädt CMake beim ersten Konfigurieren selbst herunter.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release     # Tests der Kern-Bibliothek
```

**MP3-Export** benötigt den Encoder LAME. Unter Windows besorgt ihn [vcpkg](https://vcpkg.io) (siehe `vcpkg.json`):

```powershell
cmake -S . -B build -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static
```

Ohne LAME baut AudioZ trotzdem, der MP3-Export ist dann deaktiviert (Öffnen von MP3 funktioniert immer).

Unter Linux werden zusätzlich die JUCE-Entwicklungspakete benötigt (z. B. `libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libfreetype-dev libfontconfig1-dev`).
Offiziell unterstützt wird zunächst Windows; unter Linux läuft die Entwicklung und der Test.

## Automatischer Build mit GitHub Actions

Die Datei `.github/workflows/build.yml` baut bei jedem Push unter Windows, führt die Tests aus und erzeugt die Setup.exe
(mit [Inno Setup](https://jrsoftware.org/isinfo.php), Skript: `installer/AudioZ.iss`).

- Normale Pushes: die Setup.exe erscheint unter **Actions → (Lauf anklicken) → Artifacts**.
- Veröffentlichung: einen Tag setzen, z. B. `git tag v0.1.0 && git push origin v0.1.0` – dann legt der Workflow automatisch ein **Release** mit der Setup.exe an.

Die Versionsnummer steht in der obersten `CMakeLists.txt` (`project(AudioZ VERSION ...)`).

## Aufbau des Quellcodes

| Ordner | Inhalt |
|---|---|
| `src/core` | Kern-Bibliothek ohne Oberfläche: Bearbeitung, Lautheit, Limiter, Pitch, BPM, MP3, Resampling. Vollständig durch Tests abgedeckt. |
| `src/app` | Oberfläche (JUCE): Hauptfenster, Wellenform, Dialoge, Einstellungen, Wiedergabe |
| `tests` | Testprogramm für die Kern-Bibliothek |
| `installer` | Inno-Setup-Skript |
| `cmake` | Einbindung der Fremdbibliotheken |

## Technische Hinweise

- **Lautheit:** libebur128 (EBU R128). Der Limiter arbeitet mit Vorausschau und vierfach überabgetasteter Spitzenerkennung, damit der True Peak die Obergrenze einhält. Dadurch kann die Lautheit nach dem Normalisieren leicht unter dem Ziel liegen – der Dialog zeigt das vorher an.
- **Tonhöhe:** Rubber Band (Qualität „Hoch“ = R3-Engine, „Schnell“ = R2). Bei „Tonhöhe und Tempo gemeinsam“ wird mit einem eigenen Sinc-Resampler wie bei einer Schallplatte umgerechnet.
- **Tempo:** Einsatzerkennung mit aubio (Spectral Flux), anschließend Autokorrelation für eine Genauigkeit unter 1 BPM. Analysiert werden höchstens 150 Sekunden aus der Mitte.
- **Speicher:** Die Datei wird komplett als 32-Bit-Float im Arbeitsspeicher gehalten; jeder Bearbeitungsschritt legt einen Rückgängig-Zustand an. Für sehr lange Aufnahmen (Stunden) die Speichergrenze in den Einstellungen anpassen.
- **Speichern:** WAV/FLAC werden zuerst in eine temporäre Datei geschrieben; schlägt das fehl, bleibt die Originaldatei unberührt. MP3 wird nie stillschweigend überschrieben – „Speichern“ fragt bei MP3 nach einem neuen Namen.

## Lizenz

AudioZ steht unter der **GNU General Public License, Version 3 oder neuer** (siehe `LICENSE`).

AudioZ verwendet:

| Bibliothek | Zweck | Lizenz |
|---|---|---|
| [JUCE](https://github.com/juce-framework/JUCE) 9 | Oberfläche, Audio-Ein-/Ausgabe, Dateiformate | AGPL-3.0 (oder kommerziell) |
| [Rubber Band](https://breakfastquay.com/rubberband/) | Tonhöhenverschiebung | GPL-2.0-oder-neuer |
| [libebur128](https://github.com/jiixyj/libebur128) | Lautheitsmessung | MIT |
| [aubio](https://aubio.org) | Einsatzerkennung für die BPM-Messung | GPL-3.0-oder-neuer |
| [LAME](https://lame.sourceforge.io) | MP3-Export | LGPL-2.0-oder-neuer |

Weil JUCE unter der AGPL genutzt wird, gilt für das fertige Programm auch deren Anforderung, den Quelltext bereitzustellen – bei einem öffentlichen GPL-Repository ist das erfüllt.
Details: `THIRD-PARTY-NOTICES.md`.
