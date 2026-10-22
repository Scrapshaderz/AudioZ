// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Theme.h"

#include <algorithm>
#include <cmath>

namespace audioz::ui
{

/** Einfache Stereo-Pegelanzeige (Spitzenpegel mit weichem Abfall). */
class LevelMeter : public juce::Component
{
public:
    /** Spitzenwerte (linear) seit dem letzten Aufruf; `seconds` ist die vergangene Zeit. */
    void update (float peakLeft, float peakRight, double seconds)
    {
        const float peaks[2] = { peakLeft, peakRight };
        bool changed = false;

        for (int c = 0; c < 2; ++c)
        {
            const float db = peaks[c] > 1.0e-4f ? 20.0f * std::log10 (peaks[c]) : -80.0f;
            const float fallen = levelDb[c] - (float) (seconds * 28.0);
            const float next = std::max (db, fallen);

            if (std::abs (next - levelDb[c]) > 0.05f)
                changed = true;

            levelDb[c] = next;

            if (db >= -0.1f)
            {
                clipHold[c] = 1.5;
                changed = true;
            }
            else if (clipHold[c] > 0.0)
            {
                clipHold[c] = std::max (0.0, clipHold[c] - seconds);
                changed = true;
            }
        }

        if (changed)
            repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const float barHeight = (bounds.getHeight() - 4.0f) / 2.0f;

        for (int c = 0; c < 2; ++c)
        {
            auto bar = juce::Rectangle<float> (bounds.getX(), bounds.getY() + (barHeight + 4.0f) * (float) c,
                                               bounds.getWidth(), barHeight);

            g.setColour (findColour (ids::meterBackground));
            g.fillRoundedRectangle (bar, barHeight * 0.5f);

            const float fraction = std::clamp ((levelDb[c] + 60.0f) / 60.0f, 0.0f, 1.0f);
            if (fraction > 0.0f)
            {
                auto filled = bar.withWidth (std::max (barHeight, bar.getWidth() * fraction));
                const auto low = findColour (ids::meterLow);
                const auto high = findColour (ids::meterHigh);
                g.setGradientFill (juce::ColourGradient (low, bar.getX(), 0.0f, high, bar.getRight(), 0.0f, false));
                g.fillRoundedRectangle (filled, barHeight * 0.5f);
            }

            if (clipHold[c] > 0.0)
            {
                g.setColour (findColour (ids::meterClip));
                g.fillEllipse (bar.getRight() - barHeight, bar.getY(), barHeight, barHeight);
            }
        }
    }

private:
    float levelDb[2] = { -80.0f, -80.0f };
    double clipHold[2] = { 0.0, 0.0 };
};

}  // namespace audioz::ui
