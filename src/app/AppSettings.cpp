// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "AppSettings.h"

namespace audioz::ui
{

namespace
{
    constexpr const char* kTheme = "theme";
    constexpr const char* kAccent = "accent";
    constexpr const char* kFollow = "followPlayhead";
    constexpr const char* kUndoMb = "undoMemoryMb";
    constexpr const char* kFade = "defaultFadeSeconds";
    constexpr const char* kLufs = "targetLufs";
    constexpr const char* kCeiling = "truePeakCeilingDb";
    constexpr const char* kMp3 = "mp3Bitrate";
    constexpr const char* kLastDir = "lastDirectory";
    constexpr const char* kRecent = "recentFiles";
    constexpr const char* kAudio = "audioDeviceState";
    constexpr const char* kWindow = "windowState";
}

AppSettings::AppSettings()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "AudioZ";
    options.filenameSuffix = ".settings";
    options.folderName = "AudioZ";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    options.ignoreCaseOfKeyNames = true;
    properties.setStorageParameters (options);

    recent.setMaxNumberOfItems (10);
    recent.restoreFromString (file().getValue (kRecent));
    recent.removeNonExistentFiles();
}

juce::PropertiesFile& AppSettings::file() const
{
    return *properties.getUserSettings();
}

ThemeMode AppSettings::getThemeMode() const
{
    const auto value = file().getValue (kTheme, "system");
    if (value == "dark")  return ThemeMode::Dark;
    if (value == "light") return ThemeMode::Light;
    return ThemeMode::System;
}

void AppSettings::setThemeMode (ThemeMode mode)
{
    file().setValue (kTheme, mode == ThemeMode::Dark ? "dark" : (mode == ThemeMode::Light ? "light" : "system"));
    sendChangeMessage();
}

int AppSettings::getAccentIndex() const
{
    return juce::jlimit (0, (int) accentPresets().size() - 1, file().getIntValue (kAccent, 0));
}

void AppSettings::setAccentIndex (int index)
{
    file().setValue (kAccent, juce::jlimit (0, (int) accentPresets().size() - 1, index));
    sendChangeMessage();
}

juce::Colour AppSettings::getAccentColour() const
{
    return juce::Colour (accentPresets()[(size_t) getAccentIndex()].argb);
}

bool AppSettings::getFollowPlayhead() const        { return file().getBoolValue (kFollow, true); }
void AppSettings::setFollowPlayhead (bool follow)  { file().setValue (kFollow, follow); sendChangeMessage(); }

int AppSettings::getUndoMemoryMb() const           { return juce::jlimit (128, 8192, file().getIntValue (kUndoMb, 1024)); }
void AppSettings::setUndoMemoryMb (int megabytes)  { file().setValue (kUndoMb, juce::jlimit (128, 8192, megabytes)); sendChangeMessage(); }

double AppSettings::getDefaultFadeSeconds() const  { return juce::jlimit (0.001, 60.0, file().getDoubleValue (kFade, 1.0)); }
void AppSettings::setDefaultFadeSeconds (double s) { file().setValue (kFade, juce::jlimit (0.001, 60.0, s)); sendChangeMessage(); }

double AppSettings::getTargetLufs() const          { return juce::jlimit (-60.0, 0.0, file().getDoubleValue (kLufs, -14.0)); }
void AppSettings::setTargetLufs (double lufs)      { file().setValue (kLufs, juce::jlimit (-60.0, 0.0, lufs)); sendChangeMessage(); }

double AppSettings::getTruePeakCeilingDb() const   { return juce::jlimit (-12.0, 0.0, file().getDoubleValue (kCeiling, -1.0)); }
void AppSettings::setTruePeakCeilingDb (double db) { file().setValue (kCeiling, juce::jlimit (-12.0, 0.0, db)); sendChangeMessage(); }

int AppSettings::getMp3Bitrate() const             { return juce::jlimit (32, 320, file().getIntValue (kMp3, 192)); }
void AppSettings::setMp3Bitrate (int kbps)         { file().setValue (kMp3, juce::jlimit (32, 320, kbps)); sendChangeMessage(); }

juce::File AppSettings::getLastDirectory() const
{
    const juce::File dir (file().getValue (kLastDir));
    return dir.isDirectory() ? dir : juce::File::getSpecialLocation (juce::File::userMusicDirectory);
}

void AppSettings::setLastDirectory (const juce::File& directory)
{
    if (directory.isDirectory())
        file().setValue (kLastDir, directory.getFullPathName());
}

std::unique_ptr<juce::XmlElement> AppSettings::getAudioDeviceState() const
{
    return file().getXmlValue (kAudio);
}

void AppSettings::setAudioDeviceState (const juce::XmlElement* state)
{
    if (state != nullptr)
        file().setValue (kAudio, state);
}

juce::String AppSettings::getWindowState() const
{
    return file().getValue (kWindow);
}

void AppSettings::setWindowState (const juce::String& state)
{
    file().setValue (kWindow, state);
}

void AppSettings::save()
{
    file().setValue (kRecent, recent.toString());
    file().saveIfNeeded();
}

}  // namespace audioz::ui
