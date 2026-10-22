// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Theme.h"

namespace audioz::ui
{

const std::vector<AccentPreset>& accentPresets()
{
    static const std::vector<AccentPreset> presets = {
        { "Blau",    0xff4c7dff },
        { "Violett", 0xff8b5cf6 },
        { "Türkis", 0xff14b8a6 },
        { "Grün",   0xff22b55e },
        { "Orange",  0xfff59e0b },
        { "Rosa",    0xffec4899 },
    };

    return presets;
}

AudioZLookAndFeel::AudioZLookAndFeel (bool isDark, juce::Colour accentColour)
    : dark (isDark), accent (accentColour)
{
    // Grundpalette
    const auto background  = dark ? juce::Colour (0xff15171c) : juce::Colour (0xfff2f3f7);
    const auto panel       = dark ? juce::Colour (0xff1d2027) : juce::Colour (0xffffffff);
    const auto raised      = dark ? juce::Colour (0xff292d37) : juce::Colour (0xffe7e9ef);
    const auto border      = dark ? juce::Colour (0xff343947) : juce::Colour (0xffd3d7e1);
    const auto text        = dark ? juce::Colour (0xffe9ebf1) : juce::Colour (0xff1a1d26);
    const auto textDim     = dark ? juce::Colour (0xff8d95a8) : juce::Colour (0xff687086);
    const auto onAccent    = accent.getPerceivedBrightness() > 0.68f ? juce::Colour (0xff101218) : juce::Colours::white;
    const auto waveColour  = dark ? accent.brighter (0.15f) : accent.darker (0.1f);

    setColourScheme (juce::LookAndFeel_V4::ColourScheme (background, raised, panel, border, text,
                                                         accent, onAccent, accent, text));

    // Standard-Komponenten
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::DocumentWindow::textColourId, text);

    setColour (juce::TextButton::buttonColourId, raised);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, onAccent);

    setColour (juce::ComboBox::backgroundColourId, raised);
    setColour (juce::ComboBox::outlineColourId, border);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, textDim);

    setColour (juce::Label::textColourId, text);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::ToggleButton::tickDisabledColourId, textDim);

    setColour (juce::Slider::backgroundColourId, border);
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::thumbColourId, dark ? juce::Colour (0xfff2f4f8) : juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, raised);
    setColour (juce::Slider::textBoxOutlineColourId, border);

    setColour (juce::TextEditor::backgroundColourId, raised);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, border);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);

    setColour (juce::ScrollBar::thumbColourId, textDim.withAlpha (0.55f));
    setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
    setColour (juce::PopupMenu::highlightedTextColourId, onAccent);
    setColour (juce::PopupMenu::headerTextColourId, textDim);

    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, border);

    setColour (juce::TooltipWindow::backgroundColourId, raised);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, border);

    setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedButtonBar::frontOutlineColourId, accent);
    setColour (juce::TabbedComponent::backgroundColourId, panel);
    setColour (juce::TabbedComponent::outlineColourId, border);

    setColour (juce::ListBox::backgroundColourId, panel);
    setColour (juce::ListBox::textColourId, text);

    // Eigene Farben
    setColour (ids::waveformBackground, dark ? juce::Colour (0xff0f1115) : juce::Colour (0xffffffff));
    setColour (ids::waveformColour, waveColour);
    setColour (ids::waveformCentreLine, border);
    setColour (ids::selectionFill, accent.withAlpha (dark ? 0.26f : 0.20f));
    setColour (ids::selectionEdge, accent);
    setColour (ids::playhead, dark ? juce::Colour (0xfff4f5f8) : juce::Colour (0xff1a1d26));
    setColour (ids::rulerBackground, panel);
    setColour (ids::rulerText, textDim);
    setColour (ids::rulerTick, border.brighter (dark ? 0.35f : -0.1f));
    setColour (ids::panelBackground, panel);
    setColour (ids::panelBorder, border);
    setColour (ids::accent, accent);
    setColour (ids::textDim, textDim);
    setColour (ids::meterBackground, raised);
    setColour (ids::meterLow, juce::Colour (0xff35c46a));
    setColour (ids::meterHigh, juce::Colour (0xfff2c230));
    setColour (ids::meterClip, juce::Colour (0xffed4a4a));

#if JUCE_WINDOWS
    setDefaultSansSerifTypefaceName ("Segoe UI");
#endif
}

juce::Font AudioZLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jmin (15.0f, (float) buttonHeight * 0.5f)));
}

void AudioZLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& backgroundColour,
                                              bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto colour = backgroundColour.withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f);

    if (isDown)
        colour = colour.contrasting (0.08f);
    else if (isHighlighted)
        colour = colour.contrasting (0.045f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, 6.0f);

    if (! dark)
    {
        g.setColour (findColour (juce::ComboBox::outlineColourId).withAlpha (0.8f));
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
    }
}

void AudioZLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float minSliderPos, float maxSliderPos,
                                          juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const float trackHeight = 4.0f;
    const float centreY = (float) y + (float) height * 0.5f;
    const float left = (float) x + 2.0f;
    const float right = (float) (x + width) - 2.0f;

    g.setColour (slider.findColour (juce::Slider::backgroundColourId));
    g.fillRoundedRectangle (left, centreY - trackHeight * 0.5f, right - left, trackHeight, trackHeight * 0.5f);

    g.setColour (slider.findColour (juce::Slider::trackColourId).withMultipliedAlpha (slider.isEnabled() ? 1.0f : 0.4f));
    g.fillRoundedRectangle (left, centreY - trackHeight * 0.5f, sliderPos - left, trackHeight, trackHeight * 0.5f);

    const float radius = 7.0f;
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillEllipse (sliderPos - radius, centreY - radius + 1.0f, radius * 2.0f, radius * 2.0f);
    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.fillEllipse (sliderPos - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
}

void AudioZLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar& bar, int x, int y, int width, int height,
                                       bool isVertical, int thumbStart, int thumbSize,
                                       bool isMouseOver, bool isMouseDown)
{
    juce::ignoreUnused (x, y, width, height);

    juce::Rectangle<float> thumb = isVertical
        ? juce::Rectangle<float> ((float) x + 3.0f, (float) thumbStart, (float) width - 6.0f, (float) thumbSize)
        : juce::Rectangle<float> ((float) thumbStart, (float) y + 3.0f, (float) thumbSize, (float) height - 6.0f);

    auto colour = bar.findColour (juce::ScrollBar::thumbColourId);
    if (isMouseDown)
        colour = colour.withAlpha (0.95f);
    else if (isMouseOver)
        colour = colour.withAlpha (0.8f);

    g.setColour (colour);
    g.fillRoundedRectangle (thumb, juce::jmin (thumb.getWidth(), thumb.getHeight()) * 0.5f);
}

void AudioZLookAndFeel::drawMenuBarBackground (juce::Graphics& g, int width, int height, bool,
                                               juce::MenuBarComponent&)
{
    g.fillAll (findColour (ids::panelBackground));
    g.setColour (findColour (ids::panelBorder));
    g.fillRect (0, height - 1, width, 1);
}

}  // namespace audioz::ui
