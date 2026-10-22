// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AppSettings.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace audioz::ui
{

/** Oeffnet den Einstellungsdialog. Aenderungen wirken sofort und werden gespeichert. */
void showSettingsDialog (juce::Component* parent, AppSettings& settings, juce::AudioDeviceManager& deviceManager);

}  // namespace audioz::ui
