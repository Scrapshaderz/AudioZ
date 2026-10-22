// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Theme.h"

#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace audioz::ui
{

/** Programmeinstellungen, gespeichert als XML im Benutzerprofil
    (Windows: %APPDATA%\AudioZ\AudioZ.settings). */
class AppSettings : public juce::ChangeBroadcaster
{
public:
    AppSettings();

    // Darstellung
    ThemeMode getThemeMode() const;
    void setThemeMode (ThemeMode mode);

    int getAccentIndex() const;
    void setAccentIndex (int index);
    juce::Colour getAccentColour() const;

    bool getFollowPlayhead() const;
    void setFollowPlayhead (bool shouldFollow);

    // Bearbeitung
    int getUndoMemoryMb() const;
    void setUndoMemoryMb (int megabytes);

    double getDefaultFadeSeconds() const;
    void setDefaultFadeSeconds (double seconds);

    // Loudness
    double getTargetLufs() const;
    void setTargetLufs (double lufs);

    double getTruePeakCeilingDb() const;
    void setTruePeakCeilingDb (double db);

    // Export
    int getMp3Bitrate() const;
    void setMp3Bitrate (int kbps);

    // Dateien
    juce::File getLastDirectory() const;
    void setLastDirectory (const juce::File& directory);
    juce::RecentlyOpenedFilesList& recentFiles() { return recent; }

    // Audio-Geraet und Fenster
    std::unique_ptr<juce::XmlElement> getAudioDeviceState() const;
    void setAudioDeviceState (const juce::XmlElement* state);

    juce::String getWindowState() const;
    void setWindowState (const juce::String& state);

    void save();

private:
    juce::PropertiesFile& file() const;

    mutable juce::ApplicationProperties properties;
    juce::RecentlyOpenedFilesList recent;
};

}  // namespace audioz::ui
