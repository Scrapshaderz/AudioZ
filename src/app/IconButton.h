// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Theme.h"

namespace audioz::ui
{

/** Schaltflaeche mit einem selbst gezeichneten Vektor-Symbol (keine Bilddateien noetig). */
class IconButton : public juce::Button
{
public:
    enum class Icon
    {
        Open, Save, Undo, Redo, SkipToStart, Play, Pause, Stop, Loop,
        ZoomIn, ZoomOut, ZoomFit, Settings
    };

    explicit IconButton (Icon iconToShow, const juce::String& tooltip = {})
        : juce::Button (tooltip), icon (iconToShow)
    {
        setTooltip (tooltip);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setIcon (Icon newIcon)
    {
        if (icon != newIcon)
        {
            icon = newIcon;
            repaint();
        }
    }

    /** Toggle-Zustand (z. B. Schleife) wird mit der Akzentfarbe hervorgehoben. */
    void setHighlightWhenOn (bool shouldHighlight) { highlightWhenOn = shouldHighlight; }

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = highlightWhenOn && getToggleState();
        const auto accent = findColour (ids::accent);

        if (on)
        {
            g.setColour (accent.withAlpha (isDown ? 0.35f : 0.24f));
            g.fillRoundedRectangle (bounds, 8.0f);
        }
        else if (isDown || isHighlighted)
        {
            g.setColour (findColour (juce::Label::textColourId).withAlpha (isDown ? 0.16f : 0.09f));
            g.fillRoundedRectangle (bounds, 8.0f);
        }

        auto colour = on ? accent : findColour (juce::Label::textColourId);
        if (! isEnabled())
            colour = colour.withAlpha (0.28f);

        const float size = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.56f;
        const auto area = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());

        g.setColour (colour);
        drawIcon (g, icon, area);
    }

    static void drawIcon (juce::Graphics& g, Icon which, juce::Rectangle<float> area)
    {
        // Alle Symbole sind auf einem 24x24-Raster entworfen
        const auto fit = juce::AffineTransform::scale (area.getWidth() / 24.0f).translated (area.getX(), area.getY());
        const juce::PathStrokeType stroke (2.0f * area.getWidth() / 24.0f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded);

        auto fillRounded = [&] (juce::Path p, float radius)
        {
            g.fillPath (p.createPathWithRoundedCorners (radius), fit);
        };

        switch (which)
        {
            case Icon::Open:
            {
                juce::Path p;
                p.startNewSubPath (2.5f, 5.5f);
                p.lineTo (9.5f, 5.5f);
                p.lineTo (11.8f, 8.2f);
                p.lineTo (21.5f, 8.2f);
                p.lineTo (21.5f, 19.5f);
                p.lineTo (2.5f, 19.5f);
                p.closeSubPath();
                fillRounded (p, 1.8f);
                break;
            }

            case Icon::Save:
            {
                juce::Path p;
                p.setUsingNonZeroWinding (false);
                p.addRoundedRectangle (3.5f, 3.5f, 17.0f, 17.0f, 2.0f);
                p.addRectangle (7.5f, 3.5f, 8.0f, 5.5f);    // Schieber oben
                p.addRectangle (6.5f, 13.0f, 11.0f, 7.5f);  // Etikett unten
                g.fillPath (p, fit);
                break;
            }

            case Icon::Undo:
            case Icon::Redo:
            {
                juce::Path arc;
                arc.addCentredArc (12.5f, 13.0f, 6.5f, 6.5f, 0.0f, juce::MathConstants<float>::halfPi * 0.15f,
                                   juce::MathConstants<float>::pi * 1.55f, true);
                // Pfeilspitze am oberen Anfang des Bogens
                juce::Path head;
                head.addTriangle (4.0f, 4.5f, 4.0f, 13.0f, 11.5f, 8.5f);
                auto transform = fit;
                if (which == Icon::Redo)
                    transform = juce::AffineTransform::scale (-1.0f, 1.0f, 12.0f, 12.0f).followedBy (fit);

                g.strokePath (arc, stroke, transform);
                g.fillPath (head.createPathWithRoundedCorners (1.0f), transform);
                break;
            }

            case Icon::SkipToStart:
            {
                juce::Path p;
                p.addRoundedRectangle (4.5f, 5.0f, 2.8f, 14.0f, 1.0f);
                p.addTriangle (19.5f, 5.0f, 19.5f, 19.0f, 9.0f, 12.0f);
                fillRounded (p, 1.0f);
                break;
            }

            case Icon::Play:
            {
                juce::Path p;
                p.addTriangle (7.0f, 4.5f, 7.0f, 19.5f, 19.5f, 12.0f);
                fillRounded (p, 1.6f);
                break;
            }

            case Icon::Pause:
            {
                juce::Path p;
                p.addRoundedRectangle (6.0f, 5.0f, 4.2f, 14.0f, 1.2f);
                p.addRoundedRectangle (13.8f, 5.0f, 4.2f, 14.0f, 1.2f);
                g.fillPath (p, fit);
                break;
            }

            case Icon::Stop:
            {
                juce::Path p;
                p.addRoundedRectangle (6.0f, 6.0f, 12.0f, 12.0f, 1.8f);
                g.fillPath (p, fit);
                break;
            }

            case Icon::Loop:
            {
                // Zwei Pfeile, die im Kreis laufen: oben nach rechts, unten nach links
                juce::Path p;
                p.startNewSubPath (13.0f, 7.0f);
                p.lineTo (8.0f, 7.0f);
                p.quadraticTo (4.0f, 7.0f, 4.0f, 12.0f);
                p.quadraticTo (4.0f, 17.0f, 8.0f, 17.0f);
                p.lineTo (10.0f, 17.0f);
                p.startNewSubPath (11.0f, 17.0f);
                p.lineTo (16.0f, 17.0f);
                p.quadraticTo (20.0f, 17.0f, 20.0f, 12.0f);
                p.quadraticTo (20.0f, 7.0f, 16.0f, 7.0f);
                p.lineTo (15.0f, 7.0f);
                g.strokePath (p, stroke, fit);

                juce::Path headTop, headBottom;
                headTop.addTriangle (12.5f, 3.8f, 12.5f, 10.2f, 17.0f, 7.0f);
                headBottom.addTriangle (11.5f, 13.8f, 11.5f, 20.2f, 7.0f, 17.0f);
                g.fillPath (headTop, fit);
                g.fillPath (headBottom, fit);
                break;
            }

            case Icon::ZoomIn:
            case Icon::ZoomOut:
            {
                juce::Path lens;
                lens.addEllipse (3.5f, 3.5f, 13.0f, 13.0f);
                juce::Path handle;
                handle.startNewSubPath (14.5f, 14.5f);
                handle.lineTo (20.5f, 20.5f);
                g.strokePath (lens, stroke, fit);
                g.strokePath (handle, juce::PathStrokeType (2.6f * area.getWidth() / 24.0f, juce::PathStrokeType::curved,
                                                             juce::PathStrokeType::rounded), fit);

                juce::Path sign;
                sign.startNewSubPath (7.0f, 10.0f);
                sign.lineTo (13.0f, 10.0f);
                if (which == Icon::ZoomIn)
                {
                    sign.startNewSubPath (10.0f, 7.0f);
                    sign.lineTo (10.0f, 13.0f);
                }
                g.strokePath (sign, juce::PathStrokeType (1.6f * area.getWidth() / 24.0f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded), fit);
                break;
            }

            case Icon::ZoomFit:
            {
                juce::Path bars;
                bars.addRoundedRectangle (2.5f, 5.5f, 2.2f, 13.0f, 1.0f);
                bars.addRoundedRectangle (19.3f, 5.5f, 2.2f, 13.0f, 1.0f);
                g.fillPath (bars, fit);

                juce::Path arrows;
                arrows.addArrow (juce::Line<float> (11.2f, 12.0f, 6.8f, 12.0f), 2.0f, 6.0f, 4.5f);
                arrows.addArrow (juce::Line<float> (12.8f, 12.0f, 17.2f, 12.0f), 2.0f, 6.0f, 4.5f);
                g.fillPath (arrows, fit);
                break;
            }

            case Icon::Settings:
            {
                juce::Path gear;
                gear.setUsingNonZeroWinding (false);
                gear.addStar ({ 12.0f, 12.0f }, 8, 7.6f, 10.2f, 0.0f);
                gear.addEllipse (8.5f, 8.5f, 7.0f, 7.0f);
                g.fillPath (gear, fit);
                break;
            }
        }
    }

private:
    Icon icon;
    bool highlightWhenOn = false;
};

}  // namespace audioz::ui
