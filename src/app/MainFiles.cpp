// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Teil von MainComponent: Dateien oeffnen, speichern und exportieren.
#include "MainComponent.h"

#include "BackgroundTask.h"
#include "FormDialog.h"

namespace audioz::ui
{

void MainComponent::confirmDiscardChanges (std::function<void()> proceed)
{
    if (! hasAudio() || ! doc.isDirty())
    {
        proceed();
        return;
    }

    juce::AlertWindow::showAsync (
        juce::MessageBoxOptions::makeOptionsYesNoCancel (
            juce::MessageBoxIconType::QuestionIcon,
            de ("Ungespeicherte Änderungen"),
            de ("Die Datei „") + doc.getDisplayName() + de ("“ wurde geändert.\nSollen die Änderungen gespeichert werden?"),
            "Speichern", "Nicht speichern", "Abbrechen", this),
        [this, proceed] (int result)
        {
            if (result == 1)
                saveDocument ([proceed] (bool ok) { if (ok) proceed(); });
            else if (result == 2)
                proceed();
        });
}

void MainComponent::openFile (const juce::File& file)
{
    confirmDiscardChanges ([this, file] { loadFile (file); });
}

void MainComponent::showOpenDialog()
{
    chooser = std::make_unique<juce::FileChooser> (de ("Audiodatei öffnen"), settings.getLastDirectory(),
                                                   formats.getWildcardForAllFormats());

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file != juce::File())
                                  openFile (file);
                          });
}

void MainComponent::loadFile (const juce::File& file)
{
    if (! file.existsAsFile())
    {
        showError (de ("Die Datei wurde nicht gefunden:\n") + file.getFullPathName());
        return;
    }

    engine.pause();
    stoppedByUser = true;

    auto result = std::make_shared<LoadResult>();

    BackgroundTask::run (de ("Datei wird geladen"), file.getFileName(),
        [this, result, file] { *result = loadAudioFile (formats, file); },
        [this, result, file] (const juce::String& error)
        {
            if (error.isNotEmpty())
            {
                showError (error);
                return;
            }

            if (! result->ok)
            {
                showError (de ("Die Datei konnte nicht geöffnet werden:\n") + file.getFullPathName() + "\n\n" + result->error);
                return;
            }

            doc.setHistoryBudgetBytes ((size_t) settings.getUndoMemoryMb() * 1024u * 1024u);
            doc.load (std::move (result->data), file);
            doc.sourceBitsPerSample = result->bitsPerSample;
            doc.sourceIsFloat = result->isFloat;
            doc.sourceFormatName = result->formatName;

            loudnessCache = {};
            bpmCache = {};
            playStartFrame = 0;

            settings.recentFiles().addFile (file);
            settings.setLastDirectory (file.getParentDirectory());

            updateAudio (WaveformView::ViewMode::FitAll);
            grabKeyboardFocus();
        },
        this);
}

// ---------------------------------------------------------------------------
// Speichern und Exportieren
// ---------------------------------------------------------------------------

ExportOptions MainComponent::defaultExportOptions (ExportFormat format) const
{
    ExportOptions o;
    o.format = format;
    o.dither = true;
    o.mp3.bitrateKbps = settings.getMp3Bitrate();

    if (format == ExportFormat::Wav)
        o.bitsPerSample = doc.sourceIsFloat ? 32 : (doc.sourceBitsPerSample == 16 ? 16 : 24);
    else
        o.bitsPerSample = doc.sourceBitsPerSample == 16 ? 16 : 24;

    return o;
}

void MainComponent::saveDocument (std::function<void (bool)> done)
{
    if (! hasAudio())
    {
        if (done)
            done (false);
        return;
    }

    const auto file = doc.getFile();
    const auto extension = file.getFileExtension().toLowerCase();

    // Direkt zurueckschreiben geht nur bei verlustfrei schreibbaren Formaten.
    // Bei MP3, OGG usw. wuerde ein stilles Neu-Kodieren die Qualitaet mindern.
    if (file == juce::File() || (extension != ".wav" && extension != ".flac"))
    {
        saveDocumentAs (false, std::move (done));
        return;
    }

    writeFile (file, defaultExportOptions (exportFormatForFile (file)), false, true, std::move (done));
}

void MainComponent::saveDocumentAs (bool selectionOnly, std::function<void (bool)> done)
{
    if (! hasAudio() || (selectionOnly && ! waveform.hasSelection()))
    {
        if (done)
            done (false);
        return;
    }

    juce::File suggestion = settings.getLastDirectory().getChildFile ("Unbenannt.wav");
    if (doc.getFile() != juce::File())
        suggestion = doc.getFile().getParentDirectory().getChildFile (
            doc.getFile().getFileNameWithoutExtension() + (selectionOnly ? "_Auswahl" : "") + ".wav");

    chooser = std::make_unique<juce::FileChooser> (selectionOnly ? de ("Auswahl exportieren") : "Speichern unter",
                                                   suggestion, "*.wav;*.flac;*.mp3");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, selectionOnly, done] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == juce::File())
                              {
                                  if (done)
                                      done (false);
                                  return;
                              }

                              const auto ext = file.getFileExtension().toLowerCase();
                              if (ext != ".wav" && ext != ".flac" && ext != ".mp3")
                                  file = file.withFileExtension ("wav");

                              const auto format = exportFormatForFile (file);

                              if (format == ExportFormat::Mp3 && ! audioz::mp3EncoderAvailable())
                              {
                                  showError (de ("Diese Version von AudioZ wurde ohne MP3-Encoder (LAME) erstellt.\n"
                                                 "Bitte WAV oder FLAC wählen."));
                                  if (done)
                                      done (false);
                                  return;
                              }

                              askExportOptions (format, [this, file, selectionOnly, done] (const ExportOptions& options)
                              {
                                  writeFile (file, options, selectionOnly, ! selectionOnly, done);
                              });
                          });
}

void MainComponent::askExportOptions (ExportFormat format, std::function<void (const ExportOptions&)> callback)
{
    const auto defaults = defaultExportOptions (format);
    std::vector<FormField> fields;

    if (format == ExportFormat::Mp3)
    {
        int bitrateIndex = 2;
        const int rates[] = { 128, 160, 192, 224, 256, 320 };
        for (int i = 0; i < 6; ++i)
            if (rates[i] == defaults.mp3.bitrateKbps)
                bitrateIndex = i;

        fields.push_back (FormField::choice ("mode", "Modus", { de ("Konstante Bitrate (CBR)"), de ("Variable Bitrate (VBR)") }, 0));
        fields.push_back (FormField::choice ("bitrate", "Bitrate", { "128 kbit/s", "160 kbit/s", "192 kbit/s", "224 kbit/s", "256 kbit/s", "320 kbit/s" }, bitrateIndex));
        fields.push_back (FormField::choice ("vbr", de ("VBR-Qualität"),
                                             { "V0 (beste, ca. 245 kbit/s)", "V2 (hoch, ca. 190 kbit/s)", "V4 (mittel, ca. 165 kbit/s)", "V6 (klein, ca. 115 kbit/s)" }, 1));
        fields.push_back (FormField::info ("hint", de ("MP3 ist ein verlustbehaftetes Format. Für die Weiterbearbeitung ist WAV oder FLAC besser."), 44));
    }
    else
    {
        if (format == ExportFormat::Wav)
            fields.push_back (FormField::choice ("bits", "Bittiefe", { "16 Bit", "24 Bit", "32 Bit (Float)" },
                                                 defaults.bitsPerSample == 16 ? 0 : (defaults.bitsPerSample == 32 ? 2 : 1)));
        else
            fields.push_back (FormField::choice ("bits", "Bittiefe", { "16 Bit", "24 Bit" }, defaults.bitsPerSample == 16 ? 0 : 1));

        fields.push_back (FormField::toggle ("dither", de ("Dithering (TPDF) bei 16 Bit verwenden"), defaults.dither));
    }

    const juce::String title = format == ExportFormat::Mp3 ? "Exportoptionen: MP3"
                               : format == ExportFormat::Flac ? "Exportoptionen: FLAC" : "Exportoptionen: WAV";

    showFormDialog (this, title, std::move (fields), "Speichern", "Abbrechen",
        [format, callback] (FormContent& form)
        {
            ExportOptions o;
            o.format = format;

            if (format == ExportFormat::Mp3)
            {
                const int rates[] = { 128, 160, 192, 224, 256, 320 };
                const int quality[] = { 0, 2, 4, 6 };
                o.mp3.vbr = form.getChoice ("mode") == 1;
                o.mp3.bitrateKbps = rates[juce::jlimit (0, 5, form.getChoice ("bitrate"))];
                o.mp3.vbrQuality = quality[juce::jlimit (0, 3, form.getChoice ("vbr"))];
                o.bitsPerSample = 0;
            }
            else
            {
                const int index = form.getChoice ("bits");
                o.bitsPerSample = format == ExportFormat::Wav ? (index == 0 ? 16 : (index == 2 ? 32 : 24))
                                                              : (index == 0 ? 16 : 24);
                o.dither = form.getToggle ("dither");
            }

            callback (o);
        },
        [format] (FormContent& form, const juce::String&)
        {
            if (format == ExportFormat::Mp3)
            {
                const bool vbr = form.getChoice ("mode") == 1;
                form.setFieldEnabled ("bitrate", ! vbr);
                form.setFieldEnabled ("vbr", vbr);
            }
        });
}

void MainComponent::writeFile (const juce::File& file, const ExportOptions& options, bool selectionOnly,
                               bool updateDocument, std::function<void (bool)> done)
{
    const auto audio = doc.audio();
    if (audio == nullptr)
    {
        if (done)
            done (false);
        return;
    }

    const Range range = selectionOnly ? getRange() : Range { 0, audio->numFrames(), false };
    auto success = std::make_shared<bool> (false);
    auto errorText = std::make_shared<juce::String>();

    BackgroundTask::run (selectionOnly ? "Exportieren" : "Speichern", file.getFileName(),
        [audio, file, options, range, success, errorText]
        {
            *success = saveAudioFile (file, *audio, range.start, range.length, options, *errorText);
        },
        [this, file, options, updateDocument, success, errorText, done] (const juce::String& taskError)
        {
            if (taskError.isNotEmpty() || ! *success)
            {
                showError (de ("Die Datei konnte nicht gespeichert werden:\n") + file.getFullPathName() + "\n\n"
                           + (taskError.isNotEmpty() ? taskError : *errorText));
                if (done)
                    done (false);
                return;
            }

            settings.setLastDirectory (file.getParentDirectory());
            settings.recentFiles().addFile (file);

            if (updateDocument)
            {
                doc.markSaved (file);
                doc.sourceFormatName = options.format == ExportFormat::Mp3 ? "MP3" : (options.format == ExportFormat::Flac ? "FLAC" : "WAV");
                doc.sourceBitsPerSample = options.bitsPerSample;
                doc.sourceIsFloat = options.format == ExportFormat::Wav && options.bitsPerSample == 32;
            }

            refreshAll();
            flashMessage (de ("Gespeichert: ") + file.getFileName());

            if (done)
                done (true);
        },
        this);
}

}  // namespace audioz::ui
