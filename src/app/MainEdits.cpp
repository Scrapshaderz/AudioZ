// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Teil von MainComponent: Bearbeitung, Effekte und Analyse.
#include "MainComponent.h"

#include "BackgroundTask.h"
#include "Fades.h"
#include "FormDialog.h"
#include "Gain.h"
#include "LoudnessNormalize.h"
#include "Pitch.h"
#include "Resample.h"

#include <cmath>

namespace audioz::ui
{

namespace
{
    juce::String signedDb (double db, int decimals = 1)
    {
        return (db > 0.0 ? "+" : "") + formatNumber (db, decimals) + " dB";
    }
}

// ---------------------------------------------------------------------------
// Grundlage: Bearbeitungsschritt ausfuehren und als Undo-Schritt uebernehmen
// ---------------------------------------------------------------------------

void MainComponent::runEdit (const juce::String& name, EditFunction function, bool inBackground)
{
    const auto base = doc.audio();
    if (base == nullptr)
        return;

    const int version = doc.getVersion();
    auto result = std::make_shared<audioz::AudioData>();
    auto outcome = std::make_shared<EditOutcome>();

    auto work = [base, function, result, outcome]
    {
        *result = *base;  // Kopie: das Original bleibt fuer "Rueckgaengig" unveraendert
        function (*result, *outcome);
    };

    auto finish = [this, name, version, result, outcome] (const juce::String& error)
    {
        if (error.isNotEmpty())
        {
            showError (error);
            return;
        }

        if (outcome->discard)
        {
            if (outcome->info.isNotEmpty())
                showInfo (name, outcome->info);
            return;
        }

        if (doc.getVersion() != version)
            return;  // Dokument wurde inzwischen ersetzt

        if (result->empty())
        {
            showError (de ("Nach dieser Bearbeitung wäre keine Audiodaten mehr übrig. Der Schritt wurde nicht ausgeführt."));
            return;
        }

        doc.commit (std::move (*result), name);
        updateAudio (WaveformView::ViewMode::Keep);

        switch (outcome->selection)
        {
            case EditOutcome::Selection::Clear: waveform.clearSelection(); break;
            case EditOutcome::Selection::Set:   waveform.setSelection (outcome->selStart, outcome->selEnd); break;
            case EditOutcome::Selection::Keep:  break;
        }

        if (outcome->cursor >= 0)
            waveform.setCursor (outcome->cursor);
        if (outcome->fitView)
            waveform.zoomToFit();

        refreshAll();
        flashMessage (name);

        if (outcome->info.isNotEmpty())
            showInfo (name, outcome->info);
    };

    if (inBackground)
    {
        BackgroundTask::run (name, de ("Bitte warten ..."), work, finish, this);
        return;
    }

    juce::String error;
    try
    {
        work();
    }
    catch (const std::bad_alloc&)
    {
        error = de ("Nicht genug Arbeitsspeicher für diese Bearbeitung.");
    }
    catch (const std::exception& e)
    {
        error = juce::String::fromUTF8 (e.what());
    }

    finish (error);
}

// ---------------------------------------------------------------------------
// Schneiden, Kopieren, Einfuegen
// ---------------------------------------------------------------------------

void MainComponent::doCut()
{
    if (! hasAudio() || ! waveform.hasSelection())
        return;

    const auto r = getRange();
    clipboard = std::make_shared<const audioz::AudioData> (doc.audio()->slice (r.start, r.length));

    runEdit ("Ausschneiden", [r] (audioz::AudioData& a, EditOutcome& o)
    {
        a.erase (r.start, r.length);
        o.selection = EditOutcome::Selection::Clear;
        o.cursor = (juce::int64) r.start;
    }, false);
}

void MainComponent::doCopy()
{
    if (! hasAudio() || ! waveform.hasSelection())
        return;

    const auto r = getRange();
    clipboard = std::make_shared<const audioz::AudioData> (doc.audio()->slice (r.start, r.length));
    flashMessage (de ("Kopiert: ") + formatTime ((double) r.length / (double) doc.audio()->sampleRate));
    commandManager.commandStatusChanged();
}

void MainComponent::doPaste()
{
    if (! hasAudio() || clipboard == nullptr)
        return;

    const auto clip = clipboard;
    const auto rate = doc.audio()->sampleRate;
    const auto total = (juce::int64) doc.audio()->numFrames();

    juce::int64 position = waveform.hasSelection() ? waveform.getSelectionStart() : waveform.getCursor();
    juce::int64 replaceLength = waveform.hasSelection() ? waveform.getSelectionEnd() - waveform.getSelectionStart() : 0;
    position = std::clamp<juce::int64> (position, 0, total);
    replaceLength = std::clamp<juce::int64> (replaceLength, 0, total - position);

    runEdit (de ("Einfügen"), [clip, rate, position, replaceLength] (audioz::AudioData& a, EditOutcome& o)
    {
        // Abtastrate und Kanalzahl an die Datei anpassen
        audioz::AudioData piece = clip->sampleRate == rate ? *clip : audioz::convertSampleRate (*clip, rate);
        piece = audioz::adaptChannels (piece, a.numChannels());

        if (replaceLength > 0)
            a.replace ((size_t) position, (size_t) replaceLength, piece);
        else
            a.insert ((size_t) position, piece);

        o.selection = EditOutcome::Selection::Set;
        o.selStart = position;
        o.selEnd = position + (juce::int64) piece.numFrames();
        o.cursor = position;
    }, false);
}

void MainComponent::doDelete()
{
    if (! hasAudio() || ! waveform.hasSelection())
        return;

    const auto r = getRange();

    runEdit (de ("Auswahl löschen"), [r] (audioz::AudioData& a, EditOutcome& o)
    {
        a.erase (r.start, r.length);
        o.selection = EditOutcome::Selection::Clear;
        o.cursor = (juce::int64) r.start;
    }, false);
}

void MainComponent::doCrop()
{
    if (! hasAudio() || ! waveform.hasSelection())
        return;

    const auto r = getRange();

    runEdit ("Auf Auswahl zuschneiden", [r] (audioz::AudioData& a, EditOutcome& o)
    {
        a = a.slice (r.start, r.length);
        o.selection = EditOutcome::Selection::Clear;
        o.cursor = 0;
        o.fitView = true;
    }, false);
}

void MainComponent::doSilence()
{
    if (! hasAudio() || ! waveform.hasSelection())
        return;

    const auto r = getRange();

    runEdit ("Auswahl stummschalten", [r] (audioz::AudioData& a, EditOutcome&)
    {
        a.silence (r.start, r.length);
    }, false);
}

void MainComponent::doUndo()
{
    if (! doc.canUndo())
        return;

    const auto name = doc.getUndoName();
    doc.undo();
    updateAudio (WaveformView::ViewMode::Keep);
    flashMessage (de ("Rückgängig: ") + name);
}

void MainComponent::doRedo()
{
    if (! doc.canRedo())
        return;

    const auto name = doc.getRedoName();
    doc.redo();
    updateAudio (WaveformView::ViewMode::Keep);
    flashMessage ("Wiederholt: " + name);
}

// ---------------------------------------------------------------------------
// Verstaerkung, Fades, Peak-Normalisierung
// ---------------------------------------------------------------------------

void MainComponent::doGain()
{
    if (! hasAudio())
        return;

    const auto r = getRange();
    const double peakDb = audioz::gainToDb (doc.audio()->samplePeak (r.start, r.length));

    showFormDialog (this, de ("Verstärkung"),
        {
            FormField::info ("scope", scopeText (r) + "\nAktueller Spitzenpegel: " + formatDb (peakDb), 44),
            FormField::number ("gain", de ("Verstärkung"), 0.0, -60.0, 40.0, 1, "dB"),
            FormField::info ("after", {}, 44)
        },
        "Anwenden", "Abbrechen",
        [this, r] (FormContent& form)
        {
            const double gainDb = form.getNumber ("gain");
            if (std::abs (gainDb) < 1.0e-6)
                return;

            runEdit (de ("Verstärkung ") + signedDb (gainDb), [r, gainDb] (audioz::AudioData& a, EditOutcome&)
            {
                audioz::applyGain (a, r.start, r.length, audioz::dbToGain (gainDb));
            }, false);
        },
        [peakDb] (FormContent& form, const juce::String&)
        {
            const double after = peakDb + form.getNumber ("gain");
            juce::String text = "Spitzenpegel danach: " + formatDb (after);
            if (after > 0.0)
                text << de ("\nAchtung: Das Signal übersteuert (über 0 dBFS) und wird beim Export begrenzt.");
            form.setInfo ("after", text);
        });
}

void MainComponent::doFade (bool fadeIn)
{
    if (! hasAudio())
        return;

    const auto r = getRange();
    const double rate = (double) doc.audio()->sampleRate;
    const double totalSeconds = (double) doc.audio()->numFrames() / rate;
    const size_t total = doc.audio()->numFrames();

    std::vector<FormField> fields;
    if (r.isSelection)
        fields.push_back (FormField::info ("scope", de ("Der Fade wird über die gesamte Auswahl gelegt (") + formatTime ((double) r.length / rate) + ").", 44));
    else
        fields.push_back (FormField::number ("duration", "Dauer", std::min (settings.getDefaultFadeSeconds(), totalSeconds),
                                             0.001, totalSeconds, 3, "s"));

    fields.push_back (FormField::choice ("curve", "Kurve", { "Linear", de ("Sinus (gleichbleibende Lautheit)"), de ("S-Kurve (weich)") }, 0));

    showFormDialog (this, fadeIn ? "Einblenden" : "Ausblenden", std::move (fields), "Anwenden", "Abbrechen",
        [this, r, rate, total, fadeIn] (FormContent& form)
        {
            const int index = form.getChoice ("curve");
            const auto curve = index == 1 ? audioz::FadeCurve::EqualPower
                               : index == 2 ? audioz::FadeCurve::SCurve : audioz::FadeCurve::Linear;

            size_t start = r.start, length = r.length;
            if (! r.isSelection)
            {
                length = std::clamp<size_t> ((size_t) std::llround (form.getNumber ("duration") * rate), 1, total);
                start = fadeIn ? 0 : total - length;
            }

            runEdit (fadeIn ? "Einblenden" : "Ausblenden", [start, length, curve, fadeIn] (audioz::AudioData& a, EditOutcome&)
            {
                if (fadeIn)
                    audioz::applyFadeIn (a, start, length, curve);
                else
                    audioz::applyFadeOut (a, start, length, curve);
            }, false);
        });
}

void MainComponent::doNormalizePeak()
{
    if (! hasAudio())
        return;

    const auto r = getRange();
    const double peakDb = audioz::gainToDb (doc.audio()->samplePeak (r.start, r.length));

    if (! std::isfinite (peakDb))
    {
        showInfo ("Peak normalisieren", de ("Der Abschnitt ist komplett still und lässt sich nicht normalisieren."));
        return;
    }

    showFormDialog (this, "Peak normalisieren",
        {
            FormField::info ("scope", scopeText (r) + "\nAktueller Spitzenpegel: " + formatDb (peakDb), 44),
            FormField::number ("target", "Ziel-Spitzenpegel", -1.0, -40.0, 0.0, 1, "dBFS"),
            FormField::info ("after", {}, 24)
        },
        "Anwenden", "Abbrechen",
        [this, r] (FormContent& form)
        {
            const double target = form.getNumber ("target");

            runEdit ("Peak normalisieren", [r, target] (audioz::AudioData& a, EditOutcome& o)
            {
                if (! audioz::normalizePeak (a, r.start, r.length, target).ok)
                {
                    o.discard = true;
                    o.info = de ("Der Abschnitt ist komplett still.");
                }
            }, false);
        },
        [peakDb] (FormContent& form, const juce::String&)
        {
            form.setInfo ("after", de ("Verstärkung: ") + signedDb (form.getNumber ("target") - peakDb));
        });
}

// ---------------------------------------------------------------------------
// Messungen mit Zwischenspeicher
// ---------------------------------------------------------------------------

void MainComponent::withLoudness (std::function<void (const audioz::LoudnessStats&, const Range&)> callback)
{
    if (! hasAudio())
        return;

    const Range r = getRange();
    const int version = doc.getVersion();

    if (loudnessCache.version == version && loudnessCache.start == r.start && loudnessCache.length == r.length)
    {
        callback (loudnessCache.stats, r);
        return;
    }

    const auto audio = doc.audio();
    auto stats = std::make_shared<audioz::LoudnessStats>();

    BackgroundTask::run (de ("Lautheit wird gemessen"), scopeText (r),
        [audio, r, stats] { *stats = audioz::measureLoudness (*audio, r.start, r.length); },
        [this, r, version, stats, callback] (const juce::String& error)
        {
            if (error.isNotEmpty())
            {
                showError (error);
                return;
            }

            if (doc.getVersion() != version)
                return;

            loudnessCache = { version, r.start, r.length, *stats };
            callback (*stats, r);
        },
        this);
}

void MainComponent::withBpm (std::function<void (const audioz::BpmResult&, const Range&)> callback)
{
    if (! hasAudio())
        return;

    const Range r = getRange();
    const int version = doc.getVersion();

    if (bpmCache.version == version && bpmCache.start == r.start && bpmCache.length == r.length)
    {
        callback (bpmCache.result, r);
        return;
    }

    const auto audio = doc.audio();
    auto result = std::make_shared<audioz::BpmResult>();

    BackgroundTask::run (de ("Tempo wird gemessen"), scopeText (r),
        [audio, r, result] { *result = audioz::detectBpm (*audio, r.start, r.length); },
        [this, r, version, result, callback] (const juce::String& error)
        {
            if (error.isNotEmpty())
            {
                showError (error);
                return;
            }

            if (doc.getVersion() != version)
                return;

            bpmCache = { version, r.start, r.length, *result };
            callback (*result, r);
        },
        this);
}

void MainComponent::doMeasureLoudness()
{
    withLoudness ([this] (const audioz::LoudnessStats& s, const Range& r)
    {
        juce::String text = scopeText (r) + "\n\n";

        if (! s.valid)
        {
            text << de ("Die Lautheit lässt sich nicht bestimmen: Der Abschnitt ist kürzer als 0,4 Sekunden oder still.\n\n")
                 << "Sample-Peak: " << formatDb (s.samplePeakDb) << "\nTrue Peak: " << formatDbtp (s.truePeakDb);
        }
        else
        {
            text << "Integrierte Lautheit: " << formatLufs (s.integratedLufs, 1) << "\n"
                 << "Lautheitsumfang (LRA): " << formatNumber (s.loudnessRangeLu, 1) << " LU\n"
                 << "Sample-Peak: " << formatDb (s.samplePeakDb) << "\n"
                 << "True Peak: " << formatDbtp (s.truePeakDb) << "\n\n"
                 << de ("Messung nach ITU-R BS.1770 / EBU R128.");
        }

        showInfo ("Lautheit messen", text);
    });
}

void MainComponent::doMeasureBpm()
{
    withBpm ([this] (const audioz::BpmResult& b, const Range& r)
    {
        juce::String text = scopeText (r) + "\n\n";

        if (! b.ok)
        {
            text << de ("Es wurde kein eindeutiges Tempo erkannt.\n\n"
                        "Das passiert bei sehr kurzen Abschnitten, Musik ohne deutliche Schläge (z. B. Flächen oder "
                        "Sprache) oder stark schwankendem Tempo.");
        }
        else
        {
            text << "Tempo: " << formatNumber (b.bpm, 1) << " BPM\n"
                 << "Sicherheit: " << juce::String (juce::roundToInt (b.confidence * 100.0)) << " %\n\n"
                 << de ("Falls das Ergebnis nicht passt, ist häufig das halbe oder doppelte Tempo gemeint:\n")
                 << de ("Halbes Tempo: ") << formatNumber (b.bpm / 2.0, 1) << " BPM    "
                 << "Doppeltes Tempo: " << formatNumber (b.bpm * 2.0, 1) << " BPM";
        }

        showInfo (de ("Tempo (BPM) messen"), text);
    });
}

// ---------------------------------------------------------------------------
// Lautheits-Normalisierung
// ---------------------------------------------------------------------------

void MainComponent::doNormalizeLoudness()
{
    withLoudness ([this] (const audioz::LoudnessStats& stats, const Range& r)
    {
        if (! stats.valid)
        {
            showInfo (de ("Lautheit normalisieren"),
                      de ("Die Lautheit lässt sich nicht bestimmen: Der Abschnitt ist kürzer als 0,4 Sekunden oder still."));
            return;
        }

        const double target = settings.getTargetLufs();
        const int preset = std::abs (target + 14.0) < 0.05 ? 1 : (std::abs (target + 23.0) < 0.05 ? 2 : 0);

        const juce::String measured = scopeText (r) + "\nGemessene Lautheit: " + formatLufs (stats.integratedLufs)
                                      + "    True Peak: " + formatDbtp (stats.truePeakDb);

        showFormDialog (this, de ("Lautheit normalisieren"),
            {
                FormField::info ("measured", measured, 44),
                FormField::choice ("preset", "Voreinstellung",
                                   { "Eigener Wert", juce::String::fromUTF8 ("Streaming (−14 LUFS)"),
                                     juce::String::fromUTF8 ("Rundfunk, EBU R128 (−23 LUFS)") }, preset),
                FormField::number ("target", "Ziel-Lautheit", target, -60.0, 0.0, 1, "LUFS"),
                FormField::toggle ("limit", de ("True-Peak-Limiter verwenden (verhindert Übersteuern)"), true),
                FormField::number ("ceiling", "Obergrenze True Peak", settings.getTruePeakCeilingDb(), -12.0, 0.0, 1, "dBTP"),
                FormField::info ("result", {}, 88)
            },
            "Anwenden", "Abbrechen",
            [this, r, stats] (FormContent& form)
            {
                audioz::LoudnessNormalizeParams params;
                params.targetLufs = form.getNumber ("target");
                params.truePeakCeilingDb = form.getNumber ("ceiling");
                params.useLimiter = form.getToggle ("limit");

                settings.setTargetLufs (params.targetLufs);
                settings.setTruePeakCeilingDb (params.truePeakCeilingDb);

                runEdit ("Lautheit normalisieren", [r, params] (audioz::AudioData& a, EditOutcome& o)
                {
                    const auto result = audioz::normalizeLoudness (a, r.start, r.length, params);

                    if (result.status != audioz::NormalizeStatus::Ok)
                    {
                        o.discard = true;
                        o.info = de ("Die Lautheit lässt sich nicht bestimmen (zu kurz oder still).");
                        return;
                    }

                    juce::String text;
                    text << "Lautheit vorher: " << formatLufs (result.measuredLufs) << "\n"
                         << de ("Angewendete Verstärkung: ") << signedDb (result.appliedGainDb, 2) << "\n"
                         << "Lautheit nachher: " << formatLufs (result.after.integratedLufs) << "\n"
                         << "True Peak nachher: " << formatDbtp (result.after.truePeakDb);

                    if (result.limiterUsed)
                        text << de ("\n\nDer Limiter hat Spitzen um bis zu ") << formatNumber (result.limiterReductionDb, 1)
                             << de (" dB abgesenkt, daher liegt die Lautheit leicht unter dem Ziel.");

                    o.info = text;
                }, true);
            },
            [stats] (FormContent& form, const juce::String& changed)
            {
                if (changed == "preset")
                {
                    const int p = form.getChoice ("preset");
                    if (p == 1) form.setNumber ("target", -14.0);
                    if (p == 2) form.setNumber ("target", -23.0);
                }
                else if (changed == "target")
                {
                    const double t = form.getNumber ("target");
                    form.setChoice ("preset", std::abs (t + 14.0) < 0.05 ? 1 : (std::abs (t + 23.0) < 0.05 ? 2 : 0));
                }

                const bool limit = form.getToggle ("limit");
                form.setFieldEnabled ("ceiling", limit);

                const double gain = form.getNumber ("target") - stats.integratedLufs;
                const double predictedPeak = stats.truePeakDb + gain;
                const double ceiling = form.getNumber ("ceiling");

                juce::String text = de ("Verstärkung: ") + signedDb (gain, 2) + "\nTrue Peak danach (ohne Limiter): " + formatDbtp (predictedPeak);

                if (limit && predictedPeak > ceiling)
                    text << de ("\nDer Limiter begrenzt die Spitzen; die Lautheit kann dadurch leicht unter dem Ziel liegen.");
                else if (! limit && predictedPeak > 0.0)
                    text << de ("\nAchtung: Ohne Limiter übersteuert das Ergebnis.");

                form.setInfo ("result", text);
            });
    });
}

// ---------------------------------------------------------------------------
// Tonhoehe
// ---------------------------------------------------------------------------

void MainComponent::doPitch()
{
    if (! hasAudio())
        return;

    const auto r = getRange();
    const double rate = (double) doc.audio()->sampleRate;
    const double seconds = (double) r.length / rate;

    const bool bpmKnown = bpmCache.version == doc.getVersion() && bpmCache.start == r.start
                          && bpmCache.length == r.length && bpmCache.result.ok;
    const double bpm = bpmKnown ? bpmCache.result.bpm : 0.0;

    showFormDialog (this, de ("Tonhöhe verschieben"),
        {
            FormField::info ("scope", scopeText (r), 24),
            FormField::choice ("mode", "Modus",
                               { de ("Nur Tonhöhe (Tempo bleibt gleich)"), de ("Tonhöhe und Tempo gemeinsam") }, 0),
            FormField::number ("semitones", de ("Halbtöne"), 0.0, -24.0, 24.0, 0, de ("Halbtöne")),
            FormField::number ("cents", "Feinstimmung", 0.0, -100.0, 100.0, 0, "Cent"),
            FormField::choice ("quality", de ("Qualität"),
                               { "Hoch (Rubber Band R3, langsamer)", "Schnell (Rubber Band R2)" }, 0),
            FormField::info ("result", {}, 88)
        },
        "Anwenden", "Abbrechen",
        [this, r] (FormContent& form)
        {
            audioz::PitchParams params;
            params.semitones = form.getNumber ("semitones") + form.getNumber ("cents") / 100.0;
            params.mode = form.getChoice ("mode") == 0 ? audioz::PitchMode::KeepTempo : audioz::PitchMode::ChangeTempoWithPitch;
            params.quality = form.getChoice ("quality") == 0 ? audioz::PitchQuality::High : audioz::PitchQuality::Fast;

            if (std::abs (params.semitones) < 1.0e-6)
                return;

            runEdit (de ("Tonhöhe ") + (params.semitones > 0 ? "+" : "") + formatNumber (params.semitones, 2) + " HT",
                     [r, params] (audioz::AudioData& a, EditOutcome& o)
            {
                const audioz::AudioData processed = audioz::changePitch (a.slice (r.start, r.length), params);

                if (r.isSelection)
                {
                    a.replace (r.start, r.length, processed);
                    o.selection = EditOutcome::Selection::Set;
                    o.selStart = (juce::int64) r.start;
                    o.selEnd = (juce::int64) (r.start + processed.numFrames());
                }
                else
                {
                    a = processed;
                }
            }, true);
        },
        [seconds, bpm] (FormContent& form, const juce::String&)
        {
            const double st = form.getNumber ("semitones") + form.getNumber ("cents") / 100.0;
            const double ratio = audioz::pitchRatio (st);
            const bool keepTempo = form.getChoice ("mode") == 0;

            juce::String text = "Verschiebung: " + (st > 0 ? juce::String ("+") : juce::String()) + formatNumber (st, 2)
                                + de (" Halbtöne  (Frequenz ×") + formatNumber (ratio, 4) + ")\n";

            if (keepTempo)
            {
                text << de ("Dauer und Tempo bleiben unverändert.");
                if (bpm > 0.0)
                    text << "\nTempo: " << formatNumber (bpm, 1) << " BPM";
            }
            else
            {
                text << "Dauer: " << formatTime (seconds) << " > " << formatTime (seconds / ratio)
                     << de (" (Tempo ×") << formatNumber (ratio, 4) << ")";
                if (bpm > 0.0)
                    text << "\nTempo: " << formatNumber (bpm, 1) << " > " << formatNumber (bpm * ratio, 1) << " BPM";
            }

            if (bpm <= 0.0)
                text << de ("\n(Das aktuelle Tempo zeigt Analyse > Tempo messen.)");

            form.setInfo ("result", text);
        });
}

}  // namespace audioz::ui
