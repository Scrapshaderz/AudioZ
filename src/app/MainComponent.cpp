// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainComponent.h"

#include "FormDialog.h"
#include "Gain.h"
#include "SettingsDialog.h"

namespace audioz::ui
{

namespace
{
    constexpr int kMenuHeight = 26;
    constexpr int kToolbarHeight = 58;
    constexpr int kStatusHeight = 28;
    constexpr int kButtonSize = 40;
}

MainComponent::MainComponent (AppSettings& appSettings)
    : settings (appSettings)
{
    formats.registerBasicFormats();
    doc.setHistoryBudgetBytes ((size_t) settings.getUndoMemoryMb() * 1024u * 1024u);

    for (auto* b : { &openButton, &saveButton, &undoButton, &redoButton, &toStartButton, &playButton, &stopButton,
                     &loopButton, &zoomInButton, &zoomOutButton, &zoomFitButton, &settingsButton })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }

    auto bind = [this] (IconButton& button, CommandIds id)
    {
        button.onClick = [this, id] { commandManager.invokeDirectly (id, true); };
    };

    bind (openButton, cmdOpen);
    bind (saveButton, cmdSave);
    bind (undoButton, cmdUndo);
    bind (redoButton, cmdRedo);
    bind (toStartButton, cmdToStart);
    bind (playButton, cmdPlayPause);
    bind (stopButton, cmdStop);
    bind (zoomInButton, cmdZoomIn);
    bind (zoomOutButton, cmdZoomOut);
    bind (zoomFitButton, cmdZoomFit);
    bind (settingsButton, cmdSettings);

    loopButton.setClickingTogglesState (true);
    loopButton.setHighlightWhenOn (true);
    loopButton.onClick = [this] { commandManager.invokeDirectly (cmdLoop, true); };

    timeLabel.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 24.0f, juce::Font::plain)));
    timeLabel.setJustificationType (juce::Justification::centred);
    timeLabel.setText ("0:00.000", juce::dontSendNotification);
    addAndMakeVisible (timeLabel);

    volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setRange (-40.0, 0.0, 0.1);
    volumeSlider.setValue (-1.0, juce::dontSendNotification);
    volumeSlider.setDoubleClickReturnValue (true, -1.0);
    volumeSlider.setWantsKeyboardFocus (false);
    volumeSlider.setTooltip (de ("Lautstärke der Wiedergabe"));
    volumeSlider.onValueChange = [this]
    {
        const double db = volumeSlider.getValue();
        engine.setVolume (db <= -39.9 ? 0.0f : (float) audioz::dbToGain (db));
    };
    engine.setVolume ((float) audioz::dbToGain (-1.0));
    addAndMakeVisible (volumeSlider);
    addAndMakeVisible (meter);

    addAndMakeVisible (waveform);
    waveform.setFollowPlayhead (settings.getFollowPlayhead());
    waveform.onSelectionChanged = [this] { updateLoopRegion(); refreshStatus(); commandManager.commandStatusChanged(); };
    waveform.onCursorChanged = [this] { refreshStatus(); refreshTime(); };
    waveform.onViewChanged = [] {};

    for (auto* l : { &statusLeft, &statusRight })
    {
        l->setFont (juce::Font (juce::FontOptions (13.0f)));
        l->setColour (juce::Label::textColourId, findColour (ids::textDim));
        addAndMakeVisible (*l);
    }
    statusRight.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (menuBar);

    commandManager.registerAllCommandsForTarget (this);
    commandManager.setFirstCommandTarget (this);
    setWantsKeyboardFocus (true);
    addKeyListener (commandManager.getKeyMappings());

    setApplicationCommandManagerToWatch (&commandManager);

    const auto deviceState = settings.getAudioDeviceState();
    const auto error = engine.initialise (deviceState.get());
    if (error.isNotEmpty())
        juce::MessageManager::callAsync ([this, error]
        {
            showError (error + de ("\n\nDie Wiedergabe ist deaktiviert. Unter Einstellungen > Audio lässt sich ein Gerät wählen."));
        });

    setSize (1180, 720);
    startTimerHz (30);
    refreshAll();
}

MainComponent::~MainComponent()
{
    stopTimer();
    menuBar.setModel (nullptr);
    removeKeyListener (commandManager.getKeyMappings());

    if (auto xml = engine.getDeviceManager().createStateXml())
        settings.setAudioDeviceState (xml.get());

    engine.pause();
}

juce::String MainComponent::getWindowTitle() const
{
    if (! hasAudio())
        return "AudioZ";

    return doc.getDisplayName() + (doc.isDirty() ? " *" : "") + juce::String::fromUTF8 (" – AudioZ");
}

// ---------------------------------------------------------------------------
// Darstellung
// ---------------------------------------------------------------------------

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (findColour (juce::ResizableWindow::backgroundColourId));

    auto toolbar = getLocalBounds().withTrimmedTop (kMenuHeight).removeFromTop (kToolbarHeight);
    g.setColour (findColour (ids::panelBackground));
    g.fillRect (toolbar);
    g.setColour (findColour (ids::panelBorder));
    g.fillRect (toolbar.removeFromBottom (1));

    auto status = getLocalBounds().removeFromBottom (kStatusHeight);
    g.setColour (findColour (ids::panelBackground));
    g.fillRect (status);
    g.setColour (findColour (ids::panelBorder));
    g.fillRect (status.removeFromTop (1));

    // feine Trennlinien zwischen den Werkzeug-Gruppen
    g.setColour (findColour (ids::panelBorder));
    for (auto* b : { &saveButton, &redoButton, &loopButton })
        g.fillRect (b->getRight() + 8, toolbar.getY() + 14, 1, toolbar.getHeight() - 28);
}

void MainComponent::lookAndFeelChanged()
{
    for (auto* l : { &statusLeft, &statusRight })
        l->setColour (juce::Label::textColourId, findColour (ids::textDim));

    repaint();
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    menuBar.setBounds (area.removeFromTop (kMenuHeight));

    auto toolbar = area.removeFromTop (kToolbarHeight).reduced (12, 0);
    auto status = area.removeFromBottom (kStatusHeight).reduced (14, 0);
    waveform.setBounds (area);

    // Werkzeugleiste: links die Bedienung, rechts Zoom, Lautstaerke, Pegel, Einstellungen
    auto place = [&toolbar] (juce::Component& c, int width, int height, bool fromRight)
    {
        auto slot = fromRight ? toolbar.removeFromRight (width) : toolbar.removeFromLeft (width);
        c.setBounds (slot.withSizeKeepingCentre (width, height));
    };

    const int gap = 4, groupGap = 16;

    place (openButton, kButtonSize, kButtonSize, false);  toolbar.removeFromLeft (gap);
    place (saveButton, kButtonSize, kButtonSize, false);  toolbar.removeFromLeft (groupGap);
    place (undoButton, kButtonSize, kButtonSize, false);  toolbar.removeFromLeft (gap);
    place (redoButton, kButtonSize, kButtonSize, false);  toolbar.removeFromLeft (groupGap);
    place (toStartButton, kButtonSize, kButtonSize, false); toolbar.removeFromLeft (gap);
    place (playButton, kButtonSize, kButtonSize, false);  toolbar.removeFromLeft (gap);
    place (stopButton, kButtonSize, kButtonSize, false);  toolbar.removeFromLeft (gap);
    place (loopButton, kButtonSize, kButtonSize, false);  toolbar.removeFromLeft (groupGap);

    place (settingsButton, kButtonSize, kButtonSize, true);  toolbar.removeFromRight (groupGap);
    place (meter, 110, 22, true);                            toolbar.removeFromRight (10);
    place (volumeSlider, 110, 28, true);                     toolbar.removeFromRight (groupGap);
    place (zoomFitButton, kButtonSize, kButtonSize, true);   toolbar.removeFromRight (gap);
    place (zoomOutButton, kButtonSize, kButtonSize, true);   toolbar.removeFromRight (gap);
    place (zoomInButton, kButtonSize, kButtonSize, true);    toolbar.removeFromRight (groupGap);

    timeLabel.setBounds (toolbar.withSizeKeepingCentre (std::min (toolbar.getWidth(), 190), 40));

    statusLeft.setBounds (status.removeFromLeft (status.getWidth() * 3 / 5));
    statusRight.setBounds (status);
}

void MainComponent::parentHierarchyChanged()
{
    if (isShowing())
        grabKeyboardFocus();
}

void MainComponent::refreshAll()
{
    const bool audio = hasAudio();

    saveButton.setEnabled (audio);
    undoButton.setEnabled (doc.canUndo());
    redoButton.setEnabled (doc.canRedo());
    toStartButton.setEnabled (audio);
    playButton.setEnabled (audio);
    stopButton.setEnabled (audio);
    zoomInButton.setEnabled (audio);
    zoomOutButton.setEnabled (audio);
    zoomFitButton.setEnabled (audio);

    undoButton.setTooltip (doc.canUndo() ? de ("Rückgängig: ") + doc.getUndoName() + " (Strg+Z)" : de ("Rückgängig (Strg+Z)"));
    redoButton.setTooltip (doc.canRedo() ? "Wiederholen: " + doc.getRedoName() + " (Strg+Y)" : "Wiederholen (Strg+Y)");

    refreshStatus();
    refreshTime();
    updatePlayButton();
    commandManager.commandStatusChanged();
    menuItemsChanged();

    if (onTitleChanged)
        onTitleChanged();
}

void MainComponent::refreshStatus()
{
    if (flashTicks > 0)
        return;  // eine kurze Meldung ist gerade sichtbar

    const auto audio = doc.audio();
    if (audio == nullptr || audio->empty())
    {
        statusLeft.setText (de ("Keine Datei geöffnet"), juce::dontSendNotification);
        statusRight.setText ({}, juce::dontSendNotification);
        return;
    }

    const double rate = (double) audio->sampleRate;
    juce::String left = formatSampleRate (audio->sampleRate) + "  |  " + formatChannels (audio->numChannels());

    if (doc.sourceFormatName.isNotEmpty())
    {
        left << "  |  " << doc.sourceFormatName;
        if (doc.sourceBitsPerSample > 0)
            left << " " << doc.sourceBitsPerSample << (doc.sourceIsFloat ? " Bit Float" : " Bit");
    }

    left << "  |  " << formatTime ((double) audio->numFrames() / rate);
    statusLeft.setText (left, juce::dontSendNotification);

    juce::String right;
    if (waveform.hasSelection())
    {
        const double a = (double) waveform.getSelectionStart() / rate;
        const double b = (double) waveform.getSelectionEnd() / rate;
        right << "Auswahl " << formatTime (a) << " - " << formatTime (b) << "  (" << formatTime (b - a) << ")";
    }
    else
    {
        right << "Cursor " << formatTime ((double) waveform.getCursor() / rate);
    }

    statusRight.setText (right, juce::dontSendNotification);
}

void MainComponent::refreshTime()
{
    const auto audio = doc.audio();
    const double rate = audio != nullptr ? (double) audio->sampleRate : 44100.0;
    timeLabel.setText (formatTime ((double) waveform.getPlayheadFrame() / rate), juce::dontSendNotification);
}

void MainComponent::flashMessage (const juce::String& text)
{
    statusLeft.setText (text, juce::dontSendNotification);
    flashTicks = 30 * 4;
}

void MainComponent::updatePlayButton()
{
    playButton.setIcon (engine.isPlaying() ? IconButton::Icon::Pause : IconButton::Icon::Play);
}

void MainComponent::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const double dt = lastTimerSeconds > 0.0 ? now - lastTimerSeconds : 1.0 / 30.0;
    lastTimerSeconds = now;

    meter.update (engine.takePeak (0), engine.takePeak (1), dt);

    const bool playing = engine.isPlaying();
    if (playing != wasPlaying)
    {
        if (! playing && ! stoppedByUser)
            waveform.setCursor (playStartFrame);  // Wiedergabe ist von selbst zu Ende gegangen

        stoppedByUser = false;
        wasPlaying = playing;
        updatePlayButton();
    }

    if (playing)
        refreshTime();

    if (flashTicks > 0 && --flashTicks == 0)
        refreshStatus();

    if (focusTicks > 0 && --focusTicks == 0 && isShowing() && ! hasKeyboardFocus (true))
        grabKeyboardFocus();
}

// ---------------------------------------------------------------------------
// Dokument -> Anzeige und Wiedergabe
// ---------------------------------------------------------------------------

void MainComponent::updateAudio (WaveformView::ViewMode mode)
{
    const auto audio = doc.audio();
    engine.setAudio (audio);
    waveform.setAudio (audio, mode);
    updateLoopRegion();
    refreshAll();
}

void MainComponent::updateLoopRegion()
{
    if (waveform.hasSelection())
        engine.setLoopRegion (waveform.getSelectionStart(), waveform.getSelectionEnd());
    else
        engine.setLoopRegion (0, -1);
}

void MainComponent::startPlayback()
{
    if (! hasAudio())
        return;

    const auto total = (juce::int64) doc.audio()->numFrames();
    juce::int64 start = waveform.getCursor();
    juce::int64 end = -1;

    if (waveform.hasSelection())
    {
        const auto s = waveform.getSelectionStart(), e = waveform.getSelectionEnd();
        if (start < s || start >= e)
            start = s;
        end = e;
    }
    else if (start >= total - 1)
    {
        start = 0;
    }

    if (! engine.hasDevice())
    {
        showError (de ("Es ist kein Audio-Ausgabegerät verfügbar. Unter Einstellungen > Audio lässt sich eines wählen."));
        return;
    }

    updateLoopRegion();
    playStartFrame = start;
    stoppedByUser = false;
    engine.play (start, end);
}

void MainComponent::togglePlay()
{
    if (! hasAudio())
        return;

    if (engine.isPlaying())
    {
        stoppedByUser = true;
        engine.pause();
        waveform.setCursor (engine.getPosition());  // Pause: Cursor bleibt stehen
        refreshTime();
    }
    else
    {
        startPlayback();
    }

    updatePlayButton();
}

void MainComponent::stopPlayback()
{
    if (! hasAudio())
        return;

    stoppedByUser = true;
    engine.pause();
    waveform.setCursor (playStartFrame);
    refreshTime();
    updatePlayButton();
}

void MainComponent::toStart()
{
    if (! hasAudio())
        return;

    waveform.setCursor (0);
    waveform.ensureVisible (0);

    if (engine.isPlaying())
        engine.setPosition (0);

    refreshTime();
}

// ---------------------------------------------------------------------------
// Drag & Drop
// ---------------------------------------------------------------------------

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (formats.findFormatForFileExtension (juce::File (f).getFileExtension()) != nullptr)
            return true;

    return false;
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
        if (formats.findFormatForFileExtension (juce::File (f).getFileExtension()) != nullptr)
        {
            openFile (juce::File (f));
            return;
        }
}

// ---------------------------------------------------------------------------
// Menues und Befehle
// ---------------------------------------------------------------------------

juce::StringArray MainComponent::getMenuBarNames()
{
    return { "Datei", "Bearbeiten", "Effekte", "Analyse", "Ansicht", "Wiedergabe", "Hilfe" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu m;
    auto add = [&] (CommandIds id) { m.addCommandItem (&commandManager, id); };

    switch (index)
    {
        case 0:
        {
            add (cmdOpen);

            juce::PopupMenu recent;
            settings.recentFiles().createPopupMenuItems (recent, 1000, false, true);
            m.addSubMenu (de ("Zuletzt geöffnet"), recent, recent.getNumItems() > 0);

            m.addSeparator();
            add (cmdSave);
            add (cmdSaveAs);
            add (cmdExportSelection);
            m.addSeparator();
            add (cmdSettings);
            m.addSeparator();
            add (cmdQuit);
            break;
        }

        case 1:
            add (cmdUndo); add (cmdRedo);
            m.addSeparator();
            add (cmdCut); add (cmdCopy); add (cmdPaste); add (cmdDelete);
            m.addSeparator();
            add (cmdCrop); add (cmdSilence);
            m.addSeparator();
            add (cmdSelectAll); add (cmdDeselect);
            break;

        case 2:
            add (cmdGain); add (cmdFadeIn); add (cmdFadeOut);
            m.addSeparator();
            add (cmdNormalizePeak); add (cmdNormalizeLoudness);
            m.addSeparator();
            add (cmdPitch);
            break;

        case 3:
            add (cmdMeasureLoudness); add (cmdMeasureBpm);
            break;

        case 4:
            add (cmdZoomIn); add (cmdZoomOut); add (cmdZoomFit); add (cmdZoomSelection);
            break;

        case 5:
            add (cmdPlayPause); add (cmdStop); add (cmdLoop); add (cmdToStart);
            break;

        case 6:
            add (cmdAbout);
            break;

        default:
            break;
    }

    return m;
}

void MainComponent::menuItemSelected (int menuItemID, int)
{
    if (menuItemID >= 1000 && menuItemID < 1100)
    {
        const auto file = settings.recentFiles().getFile (menuItemID - 1000);
        if (file.existsAsFile())
            openFile (file);
    }
}

void MainComponent::getAllCommands (juce::Array<juce::CommandID>& commands)
{
    commands.addArray ({ cmdOpen, cmdSave, cmdSaveAs, cmdExportSelection, cmdSettings, cmdQuit,
                         cmdUndo, cmdRedo, cmdCut, cmdCopy, cmdPaste, cmdDelete, cmdCrop, cmdSilence,
                         cmdSelectAll, cmdDeselect,
                         cmdGain, cmdFadeIn, cmdFadeOut, cmdNormalizePeak, cmdNormalizeLoudness, cmdPitch,
                         cmdMeasureLoudness, cmdMeasureBpm,
                         cmdZoomIn, cmdZoomOut, cmdZoomFit, cmdZoomSelection,
                         cmdPlayPause, cmdStop, cmdLoop, cmdToStart, cmdAbout });
}

void MainComponent::getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    const auto ctrl = juce::ModifierKeys::commandModifier;
    const auto shift = juce::ModifierKeys::shiftModifier;
    const bool audio = hasAudio();
    const bool selection = audio && waveform.hasSelection();

    auto set = [&] (const char* name, const char* category, bool enabled)
    {
        info.setInfo (de (name), de (name), de (category), 0);
        info.setActive (enabled);
    };

    switch (id)
    {
        case cmdOpen:         set ("Öffnen ...", "Datei", true);                     info.addDefaultKeypress ('o', ctrl); break;
        case cmdSave:         set ("Speichern", "Datei", audio);                     info.addDefaultKeypress ('s', ctrl); break;
        case cmdSaveAs:       set ("Speichern unter ...", "Datei", audio);           info.addDefaultKeypress ('s', ctrl | shift); break;
        case cmdExportSelection: set ("Auswahl exportieren ...", "Datei", selection); info.addDefaultKeypress ('e', ctrl); break;
        case cmdSettings:     set ("Einstellungen ...", "Datei", true);              info.addDefaultKeypress (',', ctrl); break;
        case cmdQuit:         set ("Beenden", "Datei", true);                        info.addDefaultKeypress ('q', ctrl); break;

        case cmdUndo:
            set ("Rückgängig", "Bearbeiten", doc.canUndo());
            if (doc.canUndo()) info.shortName = de ("Rückgängig: ") + doc.getUndoName();
            info.addDefaultKeypress ('z', ctrl);
            break;
        case cmdRedo:
            set ("Wiederholen", "Bearbeiten", doc.canRedo());
            if (doc.canRedo()) info.shortName = "Wiederholen: " + doc.getRedoName();
            info.addDefaultKeypress ('y', ctrl);
            info.addDefaultKeypress ('z', ctrl | shift);
            break;
        case cmdCut:          set ("Ausschneiden", "Bearbeiten", selection);         info.addDefaultKeypress ('x', ctrl); break;
        case cmdCopy:         set ("Kopieren", "Bearbeiten", selection);             info.addDefaultKeypress ('c', ctrl); break;
        case cmdPaste:        set ("Einfügen", "Bearbeiten", audio && clipboard != nullptr); info.addDefaultKeypress ('v', ctrl); break;
        case cmdDelete:       set ("Auswahl löschen", "Bearbeiten", selection);      info.addDefaultKeypress (juce::KeyPress::deleteKey, 0); break;
        case cmdCrop:         set ("Auf Auswahl zuschneiden", "Bearbeiten", selection); info.addDefaultKeypress ('t', ctrl); break;
        case cmdSilence:      set ("Auswahl stummschalten", "Bearbeiten", selection); info.addDefaultKeypress ('l', ctrl); break;
        case cmdSelectAll:    set ("Alles auswählen", "Bearbeiten", audio);          info.addDefaultKeypress ('a', ctrl); break;
        case cmdDeselect:     set ("Auswahl aufheben", "Bearbeiten", selection);     info.addDefaultKeypress (juce::KeyPress::escapeKey, 0); break;

        case cmdGain:         set ("Verstärkung ...", "Effekte", audio); break;
        case cmdFadeIn:       set ("Einblenden ...", "Effekte", audio); break;
        case cmdFadeOut:      set ("Ausblenden ...", "Effekte", audio); break;
        case cmdNormalizePeak: set ("Peak normalisieren ...", "Effekte", audio); break;
        case cmdNormalizeLoudness: set ("Lautheit normalisieren (LUFS) ...", "Effekte", audio); info.addDefaultKeypress ('n', ctrl | shift); break;
        case cmdPitch:        set ("Tonhöhe verschieben ...", "Effekte", audio); info.addDefaultKeypress ('p', ctrl | shift); break;

        case cmdMeasureLoudness: set ("Lautheit messen ...", "Analyse", audio); info.addDefaultKeypress ('l', ctrl | shift); break;
        case cmdMeasureBpm:   set ("Tempo (BPM) messen ...", "Analyse", audio); info.addDefaultKeypress ('b', ctrl | shift); break;

        case cmdZoomIn:       set ("Hineinzoomen", "Ansicht", audio); info.addDefaultKeypress ('+', 0); info.addDefaultKeypress ('=', ctrl); break;
        case cmdZoomOut:      set ("Herauszoomen", "Ansicht", audio); info.addDefaultKeypress ('-', 0); info.addDefaultKeypress ('-', ctrl); break;
        case cmdZoomFit:      set ("Alles anzeigen", "Ansicht", audio); info.addDefaultKeypress ('0', ctrl); break;
        case cmdZoomSelection: set ("Auf Auswahl zoomen", "Ansicht", selection); info.addDefaultKeypress ('0', ctrl | shift); break;

        case cmdPlayPause:    set ("Wiedergabe / Pause", "Wiedergabe", audio); info.addDefaultKeypress (juce::KeyPress::spaceKey, 0); break;
        case cmdStop:         set ("Stopp", "Wiedergabe", audio); info.addDefaultKeypress (juce::KeyPress::returnKey, 0); break;
        case cmdLoop:
            set ("Schleife", "Wiedergabe", true);
            info.setTicked (engine.isLooping());
            info.addDefaultKeypress ('r', ctrl);
            break;
        case cmdToStart:      set ("Zum Anfang", "Wiedergabe", audio); info.addDefaultKeypress (juce::KeyPress::homeKey, 0); break;

        case cmdAbout:        set ("Über AudioZ", "Hilfe", true); info.addDefaultKeypress (juce::KeyPress::F1Key, 0); break;

        default: break;
    }
}

bool MainComponent::perform (const InvocationInfo& info)
{
    switch (info.commandID)
    {
        case cmdOpen:         showOpenDialog(); break;
        case cmdSave:         saveDocument ({}); break;
        case cmdSaveAs:       saveDocumentAs (false, {}); break;
        case cmdExportSelection: saveDocumentAs (true, {}); break;
        case cmdSettings:
            showSettingsDialog (this, settings, engine.getDeviceManager());
            break;
        case cmdQuit:
            if (auto* app = juce::JUCEApplication::getInstance())
                app->systemRequestedQuit();
            break;

        case cmdUndo:         doUndo(); break;
        case cmdRedo:         doRedo(); break;
        case cmdCut:          doCut(); break;
        case cmdCopy:         doCopy(); break;
        case cmdPaste:        doPaste(); break;
        case cmdDelete:       doDelete(); break;
        case cmdCrop:         doCrop(); break;
        case cmdSilence:      doSilence(); break;
        case cmdSelectAll:    waveform.selectAll(); break;
        case cmdDeselect:     waveform.clearSelection(); break;

        case cmdGain:         doGain(); break;
        case cmdFadeIn:       doFade (true); break;
        case cmdFadeOut:      doFade (false); break;
        case cmdNormalizePeak: doNormalizePeak(); break;
        case cmdNormalizeLoudness: doNormalizeLoudness(); break;
        case cmdPitch:        doPitch(); break;

        case cmdMeasureLoudness: doMeasureLoudness(); break;
        case cmdMeasureBpm:   doMeasureBpm(); break;

        case cmdZoomIn:       waveform.zoomIn(); break;
        case cmdZoomOut:      waveform.zoomOut(); break;
        case cmdZoomFit:      waveform.zoomToFit(); break;
        case cmdZoomSelection: waveform.zoomToSelection(); break;

        case cmdPlayPause:    togglePlay(); break;
        case cmdStop:         stopPlayback(); break;
        case cmdLoop:
            engine.setLooping (! engine.isLooping());
            loopButton.setToggleState (engine.isLooping(), juce::dontSendNotification);
            updateLoopRegion();
            commandManager.commandStatusChanged();
            break;
        case cmdToStart:      toStart(); break;

        case cmdAbout:
        {
            const auto text = de ("AudioZ ") + AUDIOZ_VERSION_STRING
                              + de ("\nAudio-Editor und Mastering-Werkzeug\n\n"
                                    "Freie Software unter der GNU General Public License v3.\n"
                                    "Quellcode: Projekt \"AudioZ\" auf GitHub\n\n"
                                    "Verwendet: JUCE (AGPLv3), Rubber Band (GPL), libebur128 (MIT),\n"
                                    "aubio (GPL) und LAME (LGPL).");
            showFormDialog (this, de ("Über AudioZ"), { FormField::info ("t", text, 170) }, "OK", {}, nullptr);
            break;
        }

        default: return false;
    }

    grabKeyboardFocus();
    return true;
}

// ---------------------------------------------------------------------------
// Hilfen
// ---------------------------------------------------------------------------

void MainComponent::showInfo (const juce::String& title, const juce::String& text)
{
    const int lines = text.isEmpty() ? 1 : text.retainCharacters ("\n").length() + 1;
    showFormDialog (this, title, { FormField::info ("t", text, std::max (24, lines * 21 + 6)) }, "OK", {}, nullptr);
}

void MainComponent::showError (const juce::String& text)
{
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::WarningIcon)
                                      .withTitle ("AudioZ")
                                      .withMessage (text)
                                      .withButton ("OK")
                                      .withAssociatedComponent (this),
                                  nullptr);
}

juce::String MainComponent::scopeText (const Range& range) const
{
    const double rate = hasAudio() ? (double) doc.audio()->sampleRate : 44100.0;
    return range.isSelection ? de ("Auswahl (") + formatTime ((double) range.length / rate) + ")"
                             : de ("Ganze Datei (") + formatTime ((double) range.length / rate) + ")";
}

MainComponent::Range MainComponent::getRange() const
{
    Range r;
    const auto audio = doc.audio();
    if (audio == nullptr)
        return r;

    const auto total = (juce::int64) audio->numFrames();

    if (waveform.hasSelection())
    {
        const auto s = std::clamp<juce::int64> (waveform.getSelectionStart(), 0, total);
        const auto e = std::clamp<juce::int64> (waveform.getSelectionEnd(), s, total);
        r.start = (size_t) s;
        r.length = (size_t) (e - s);
        r.isSelection = r.length > 0;
        if (r.isSelection)
            return r;
    }

    r.start = 0;
    r.length = (size_t) total;
    r.isSelection = false;
    return r;
}

}  // namespace audioz::ui
