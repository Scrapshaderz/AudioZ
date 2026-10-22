// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace audioz::ui
{

enum class ThemeMode
{
    System,
    Dark,
    Light
};

/** Farb-IDs fuer die eigenen Komponenten (Wellenform, Lineal, Pegelanzeige ...).
    Sie werden im LookAndFeel gesetzt und mit findColour() abgefragt, wodurch ein
    Wechsel zwischen hellem und dunklem Design automatisch ueberall greift. */
namespace ids
{
    enum ColourId
    {
        waveformBackground = 0x5a5a0001,
        waveformColour,
        waveformCentreLine,
        selectionFill,
        selectionEdge,
        playhead,
        rulerBackground,
        rulerText,
        rulerTick,
        panelBackground,   // Werkzeugleiste und Statuszeile
        panelBorder,
        accent,
        textDim,
        meterBackground,
        meterLow,
        meterHigh,
        meterClip
    };
}

struct AccentPreset
{
    const char* name;
    juce::uint32 argb;
};

/** Auswahl an Akzentfarben fuer die Einstellungen. */
const std::vector<AccentPreset>& accentPresets();

/** Moderne, flache Oberflaeche mit abgerundeten Elementen. */
class AudioZLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AudioZLookAndFeel (bool dark, juce::Colour accentColour);

    bool isDark() const noexcept { return dark; }
    juce::Colour getAccent() const noexcept { return accent; }

    // LookAndFeel
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isHighlighted, bool isDown) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    int getSliderThumbRadius (juce::Slider&) override { return 8; }

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                        bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override;

    void drawMenuBarBackground (juce::Graphics&, int width, int height, bool isMouseOverBar,
                                juce::MenuBarComponent&) override;

private:
    bool dark;
    juce::Colour accent;
};

}  // namespace audioz::ui
