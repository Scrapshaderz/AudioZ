// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AppSettings.h"
#include "AudioEngine.h"
#include "AudioIO.h"
#include "Bpm.h"
#include "Document.h"
#include "IconButton.h"
#include "Formatting.h"
#include "LevelMeter.h"
#include "Loudness.h"
#include "WaveformView.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace audioz::ui
{

/** Hauptansicht: Menue, Werkzeugleiste, Wellenform und Statuszeile samt aller Funktionen. */
class MainComponent : public juce::Component,
                      public juce::MenuBarModel,
                      public juce::ApplicationCommandTarget,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer
{
public:
    explicit MainComponent (AppSettings& appSettings);
    ~MainComponent() override;

    /** Oeffnet eine Datei (fragt vorher nach, falls ungespeicherte Aenderungen da sind). */
    void openFile (const juce::File& file);

    /** Fragt bei ungespeicherten Aenderungen nach und ruft dann `proceed` auf. */
    void confirmDiscardChanges (std::function<void()> proceed);

    juce::String getWindowTitle() const;
    std::function<void()> onTitleChanged;

    juce::ApplicationCommandManager& getCommandManager() { return commandManager; }
    AudioEngine& getEngine() { return engine; }
    /** Uebernimmt geaenderte Programmeinstellungen (Mitscrollen, Undo-Speicher). */
    void settingsChanged()
    {
        waveform.setFollowPlayhead (settings.getFollowPlayhead());
        doc.setHistoryBudgetBytes ((size_t) settings.getUndoMemoryMb() * 1024u * 1024u);
    }

    // Component
    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void lookAndFeelChanged() override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int menuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

    // ApplicationCommandTarget
    ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands (juce::Array<juce::CommandID>& commands) override;
    void getCommandInfo (juce::CommandID, juce::ApplicationCommandInfo&) override;
    bool perform (const InvocationInfo&) override;

    enum CommandIds
    {
        cmdOpen = 0x3001, cmdSave, cmdSaveAs, cmdExportSelection, cmdSettings, cmdQuit,
        cmdUndo, cmdRedo, cmdCut, cmdCopy, cmdPaste, cmdDelete, cmdCrop, cmdSilence,
        cmdSelectAll, cmdDeselect,
        cmdGain, cmdFadeIn, cmdFadeOut, cmdNormalizePeak, cmdNormalizeLoudness, cmdPitch,
        cmdMeasureLoudness, cmdMeasureBpm,
        cmdZoomIn, cmdZoomOut, cmdZoomFit, cmdZoomSelection,
        cmdPlayPause, cmdStop, cmdLoop, cmdToStart,
        cmdAbout
    };

private:
    // Bearbeitungen -----------------------------------------------------
    struct EditOutcome
    {
        enum class Selection { Keep, Clear, Set };

        Selection selection = Selection::Keep;
        juce::int64 selStart = 0, selEnd = 0;
        juce::int64 cursor = -1;   // >= 0: Cursor setzen
        bool fitView = false;
        bool discard = false;      // true: Ergebnis verwerfen (z. B. Messung nicht moeglich)
        juce::String info;         // wird nach der Bearbeitung in einem Dialog gezeigt
    };

    using EditFunction = std::function<void (audioz::AudioData&, EditOutcome&)>;

    struct Range
    {
        size_t start = 0, length = 0;
        bool isSelection = false;
    };

    Range getRange() const;
    bool hasAudio() const { return doc.hasAudio(); }
    void runEdit (const juce::String& name, EditFunction function, bool inBackground);

    void updateAudio (WaveformView::ViewMode mode);
    void refreshAll();
    void refreshStatus();
    void refreshTime();
    void flashMessage (const juce::String& text);

    // Dateien -------------------------------------------------------------
    void showOpenDialog();
    void loadFile (const juce::File& file);
    void saveDocument (std::function<void (bool)> done);
    void saveDocumentAs (bool selectionOnly, std::function<void (bool)> done);
    void writeFile (const juce::File& file, const ExportOptions& options, bool selectionOnly, bool updateDocument,
                    std::function<void (bool)> done);
    void askExportOptions (ExportFormat format, std::function<void (const ExportOptions&)> callback);
    ExportOptions defaultExportOptions (ExportFormat format) const;

    // Befehle -------------------------------------------------------------
    void doCut();
    void doCopy();
    void doPaste();
    void doDelete();
    void doCrop();
    void doSilence();
    void doGain();
    void doFade (bool fadeIn);
    void doNormalizePeak();
    void doNormalizeLoudness();
    void doPitch();
    void doMeasureLoudness();
    void doMeasureBpm();
    void doUndo();
    void doRedo();

    /** Misst (falls noetig im Hintergrund) und ruft danach `callback` im Hauptthread auf. */
    void withLoudness (std::function<void (const audioz::LoudnessStats&, const Range&)> callback);
    void withBpm (std::function<void (const audioz::BpmResult&, const Range&)> callback);

    void togglePlay();
    void stopPlayback();
    void startPlayback();
    void toStart();
    void updateLoopRegion();
    void updatePlayButton();

    // Hilfen ----------------------------------------------------------------
    void showInfo (const juce::String& title, const juce::String& text);
    void showError (const juce::String& text);
    juce::String scopeText (const Range& range) const;

    void timerCallback() override;

    AppSettings& settings;
    Document doc;
    AudioEngine engine;
    juce::AudioFormatManager formats;
    juce::ApplicationCommandManager commandManager;

    std::shared_ptr<const audioz::AudioData> clipboard;

    // Messwerte, solange das Dokument unveraendert ist
    struct LoudnessCache
    {
        int version = -1;
        size_t start = 0, length = 0;
        audioz::LoudnessStats stats;
    } loudnessCache;

    struct BpmCache
    {
        int version = -1;
        size_t start = 0, length = 0;
        audioz::BpmResult result;
    } bpmCache;

    juce::TooltipWindow tooltipWindow { this, 600 };

    IconButton openButton { IconButton::Icon::Open, de ("Öffnen (Strg+O)") };
    IconButton saveButton { IconButton::Icon::Save, "Speichern (Strg+S)" };
    IconButton undoButton { IconButton::Icon::Undo, de ("Rückgängig (Strg+Z)") };
    IconButton redoButton { IconButton::Icon::Redo, de ("Wiederholen (Strg+Y)") };
    IconButton toStartButton { IconButton::Icon::SkipToStart, "Zum Anfang (Pos1)" };
    IconButton playButton { IconButton::Icon::Play, "Wiedergabe / Pause (Leertaste)" };
    IconButton stopButton { IconButton::Icon::Stop, "Stopp (Eingabetaste)" };
    IconButton loopButton { IconButton::Icon::Loop, "Schleife" };
    IconButton zoomInButton { IconButton::Icon::ZoomIn, "Hineinzoomen (+)" };
    IconButton zoomOutButton { IconButton::Icon::ZoomOut, "Herauszoomen (-)" };
    IconButton zoomFitButton { IconButton::Icon::ZoomFit, "Alles anzeigen (Strg+0)" };
    IconButton settingsButton { IconButton::Icon::Settings, "Einstellungen (Strg+,)" };

    juce::Label timeLabel;
    juce::Slider volumeSlider;
    LevelMeter meter;
    WaveformView waveform { engine };
    juce::Label statusLeft, statusRight;
    juce::MenuBarComponent menuBar { this };

    std::unique_ptr<juce::FileChooser> chooser;

    juce::int64 playStartFrame = 0;
    bool wasPlaying = false;
    bool stoppedByUser = false;
    double lastTimerSeconds = 0.0;
    int flashTicks = 0;
    int focusTicks = 3;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

}  // namespace audioz::ui
