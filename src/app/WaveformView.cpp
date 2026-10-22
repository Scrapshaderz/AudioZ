// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "WaveformView.h"

#include "Formatting.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace audioz::ui
{

namespace
{
    constexpr double kMinSamplesPerPixel = 1.0 / 64.0;
    constexpr float kEdgeTolerance = 6.0f;
}

WaveformView::WaveformView (AudioEngine& engineToUse)
    : engine (engineToUse)
{
    addAndMakeVisible (scrollBar);
    scrollBar.addListener (this);
    scrollBar.setAutoHide (false);

    setMouseCursor (juce::MouseCursor::IBeamCursor);
    startTimerHz (30);
}

WaveformView::~WaveformView()
{
    scrollBar.removeListener (this);
}

// ---------------------------------------------------------------------------
// Daten und Ansicht
// ---------------------------------------------------------------------------

void WaveformView::setAudio (std::shared_ptr<const audioz::AudioData> newAudio, ViewMode mode)
{
    audio = std::move (newAudio);
    peaks.build (audio);

    if (! hasAudio())
    {
        viewStart = 0.0;
        selectionStart = selectionEnd = 0;
        cursor = 0;
        updateScrollBar();
        repaint();
        return;
    }

    const juce::int64 total = (juce::int64) audio->numFrames();

    if (mode == ViewMode::FitAll)
    {
        selectionStart = selectionEnd = 0;
        cursor = 0;
        zoomToFit();
    }
    else
    {
        selectionStart = std::min (selectionStart, total);
        selectionEnd = std::min (selectionEnd, total);
        if (selectionEnd <= selectionStart)
            selectionStart = selectionEnd = 0;

        cursor = std::min (cursor, total);
        samplesPerPixel = std::clamp (samplesPerPixel, kMinSamplesPerPixel,
                                      std::max (kMinSamplesPerPixel, (double) total / (double) std::max (1, getWidth())));
        setViewStart (viewStart);
    }

    updateScrollBar();
    repaint();
}

juce::Rectangle<int> WaveformView::getWaveArea() const
{
    return getLocalBounds().withTrimmedTop (kRulerHeight).withTrimmedBottom (kScrollBarHeight);
}

double WaveformView::visibleSamples() const
{
    return (double) std::max (1, getWidth()) * samplesPerPixel;
}

juce::int64 WaveformView::clampFrame (double frame) const
{
    if (! hasAudio())
        return 0;

    return (juce::int64) std::clamp (std::llround (frame), 0ll, (long long) audio->numFrames());
}

void WaveformView::setViewStart (double newStart)
{
    if (! hasAudio())
        return;

    const double maxStart = std::max (0.0, (double) audio->numFrames() - visibleSamples());
    newStart = std::clamp (newStart, 0.0, maxStart);

    if (std::abs (newStart - viewStart) > 1.0e-9)
    {
        viewStart = newStart;
        repaint();
        updateScrollBar();
        notifyView();
    }
    else
    {
        updateScrollBar();
    }
}

void WaveformView::zoomBy (double factor, float anchorX)
{
    if (! hasAudio() || factor <= 0.0)
        return;

    const double total = (double) audio->numFrames();
    const double maxSpp = std::max (kMinSamplesPerPixel, total / (double) std::max (1, getWidth()));
    const double anchorSample = xToSample (anchorX);
    const double newSpp = std::clamp (samplesPerPixel / factor, kMinSamplesPerPixel, maxSpp);

    samplesPerPixel = newSpp;
    viewStart = anchorSample - (double) anchorX * newSpp;
    setViewStart (viewStart);
    repaint();
    notifyView();
}

void WaveformView::zoomIn()  { zoomBy (2.0, (float) getWidth() * 0.5f); }
void WaveformView::zoomOut() { zoomBy (0.5, (float) getWidth() * 0.5f); }

void WaveformView::zoomToFit()
{
    if (! hasAudio())
        return;

    samplesPerPixel = std::max (kMinSamplesPerPixel, (double) audio->numFrames() / (double) std::max (1, getWidth()));
    viewStart = 0.0;
    updateScrollBar();
    repaint();
    notifyView();
}

void WaveformView::zoomToSelection()
{
    if (! hasAudio())
        return;

    if (! hasSelection())
    {
        zoomToFit();
        return;
    }

    const double length = (double) (selectionEnd - selectionStart);
    samplesPerPixel = std::max (kMinSamplesPerPixel, length * 1.08 / (double) std::max (1, getWidth()));
    viewStart = (double) selectionStart - length * 0.04;
    setViewStart (viewStart);
    repaint();
    notifyView();
}

void WaveformView::ensureVisible (juce::int64 frame)
{
    if (! hasAudio())
        return;

    const double visible = visibleSamples();

    if ((double) frame < viewStart)
        setViewStart ((double) frame - visible * 0.1);
    else if ((double) frame > viewStart + visible * 0.92)
        setViewStart ((double) frame - visible * 0.1);
}

void WaveformView::updateScrollBar()
{
    if (! hasAudio())
    {
        scrollBar.setRangeLimits (0.0, 1.0);
        scrollBar.setCurrentRange (0.0, 1.0, juce::dontSendNotification);
        return;
    }

    scrollBar.setRangeLimits (0.0, (double) audio->numFrames());
    scrollBar.setCurrentRange (viewStart, visibleSamples(), juce::dontSendNotification);
}

void WaveformView::notifyView()
{
    if (onViewChanged)
        onViewChanged();
}

void WaveformView::scrollBarMoved (juce::ScrollBar*, double newRangeStart)
{
    setViewStart (newRangeStart);
}

void WaveformView::resized()
{
    scrollBar.setBounds (getLocalBounds().removeFromBottom (kScrollBarHeight));

    if (hasAudio())
    {
        const double maxSpp = std::max (kMinSamplesPerPixel, (double) audio->numFrames() / (double) std::max (1, getWidth()));
        samplesPerPixel = std::min (samplesPerPixel, maxSpp);
        setViewStart (viewStart);
    }

    updateScrollBar();
}

void WaveformView::lookAndFeelChanged()
{
    repaint();
}

// ---------------------------------------------------------------------------
// Auswahl und Cursor
// ---------------------------------------------------------------------------

void WaveformView::setSelection (juce::int64 start, juce::int64 end)
{
    if (! hasAudio())
        return;

    start = clampFrame ((double) start);
    end = clampFrame ((double) end);
    if (end < start)
        std::swap (start, end);

    if (start == selectionStart && end == selectionEnd)
        return;

    selectionStart = start;
    selectionEnd = end;
    repaint();

    if (onSelectionChanged)
        onSelectionChanged();
}

void WaveformView::clearSelection()
{
    if (selectionEnd == selectionStart)
        return;

    selectionStart = selectionEnd = 0;
    repaint();

    if (onSelectionChanged)
        onSelectionChanged();
}

void WaveformView::selectAll()
{
    if (hasAudio())
        setSelection (0, (juce::int64) audio->numFrames());
}

void WaveformView::setCursor (juce::int64 frame)
{
    frame = clampFrame ((double) frame);
    if (frame == cursor)
        return;

    cursor = frame;
    repaint();

    if (onCursorChanged)
        onCursorChanged();
}

juce::int64 WaveformView::getPlayheadFrame() const
{
    return engine.isPlaying() ? engine.getPosition() : cursor;
}

// ---------------------------------------------------------------------------
// Zeichnen
// ---------------------------------------------------------------------------

void WaveformView::paint (juce::Graphics& g)
{
    g.fillAll (findColour (ids::waveformBackground));

    if (! hasAudio())
    {
        paintPlaceholder (g);
        return;
    }

    const auto waveArea = getWaveArea();
    const auto rulerArea = getLocalBounds().removeFromTop (kRulerHeight);

    paintWaveform (g, waveArea);

    if (hasSelection())
    {
        const float x1 = std::max (0.0f, sampleToX ((double) selectionStart));
        const float x2 = std::min ((float) getWidth(), sampleToX ((double) selectionEnd));

        if (x2 > x1)
        {
            g.setColour (findColour (ids::selectionFill));
            g.fillRect (x1, (float) waveArea.getY(), x2 - x1, (float) waveArea.getHeight());
            g.setColour (findColour (ids::selectionEdge));

            if (sampleToX ((double) selectionStart) >= 0.0f)
                g.fillRect (x1, (float) waveArea.getY(), 1.5f, (float) waveArea.getHeight());
            if (sampleToX ((double) selectionEnd) <= (float) getWidth())
                g.fillRect (x2 - 1.5f, (float) waveArea.getY(), 1.5f, (float) waveArea.getHeight());
        }
    }

    paintRuler (g, rulerArea);
    paintPlayhead (g, rulerArea, waveArea);
}

void WaveformView::paintPlaceholder (juce::Graphics& g)
{
    auto area = getLocalBounds().withTrimmedBottom (kScrollBarHeight).reduced (24);
    g.setColour (findColour (ids::textDim));

    g.setFont (juce::Font (juce::FontOptions (22.0f)));
    g.drawText (juce::String::fromUTF8 ("Audiodatei hier ablegen"), area.removeFromTop (area.getHeight() / 2).withTrimmedTop (area.getHeight() / 2 - 40),
                juce::Justification::centredBottom);

    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    g.drawText (juce::String::fromUTF8 ("oder mit Strg+O öffnen (WAV, FLAC, MP3, OGG, AIFF)"), area.withTrimmedTop (4),
                juce::Justification::centredTop);
}

void WaveformView::paintWaveform (juce::Graphics& g, juce::Rectangle<int> area)
{
    const int numChannels = audio->numChannels();
    const juce::int64 total = (juce::int64) audio->numFrames();
    const float laneHeight = (float) area.getHeight() / (float) std::max (1, numChannels);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float laneTop = (float) area.getY() + laneHeight * (float) ch;
        const float centreY = laneTop + laneHeight * 0.5f;
        const float halfHeight = std::max (2.0f, laneHeight * 0.5f - 4.0f);

        // Mittellinie und Trennung zwischen den Kanaelen
        g.setColour (findColour (ids::waveformCentreLine));
        g.fillRect ((float) area.getX(), centreY, (float) area.getWidth(), 1.0f);
        if (ch > 0)
            g.fillRect ((float) area.getX(), laneTop, (float) area.getWidth(), 1.0f);

        g.setColour (findColour (ids::waveformColour));

        if (samplesPerPixel >= 1.0)
        {
            for (int x = 0; x < area.getWidth(); ++x)
            {
                const double s0 = viewStart + (double) x * samplesPerPixel;
                const juce::int64 from = (juce::int64) std::floor (s0);
                if (from >= total)
                    break;

                const juce::int64 to = std::max (from + 1, (juce::int64) std::ceil (s0 + samplesPerPixel));

                float lo, hi;
                peaks.getMinMax (ch, from, to, lo, hi);

                const float top = centreY - std::min (1.0f, hi) * halfHeight;
                const float bottom = centreY - std::max (-1.0f, lo) * halfHeight;
                g.fillRect ((float) (area.getX() + x), top, 1.0f, std::max (1.0f, bottom - top));
            }
        }
        else
        {
            // Staerker eingezoomt als ein Sample je Pixel: Linie durch die einzelnen Samples
            const auto& samples = audio->channels[(size_t) ch];
            const juce::int64 first = std::max<juce::int64> (0, (juce::int64) std::floor (viewStart) - 1);
            const juce::int64 last = std::min<juce::int64> (total - 1, (juce::int64) std::ceil (viewStart + visibleSamples()) + 1);

            juce::Path path;
            for (juce::int64 i = first; i <= last; ++i)
            {
                const float x = (float) area.getX() + sampleToX ((double) i);
                const float y = centreY - std::clamp (samples[(size_t) i], -1.0f, 1.0f) * halfHeight;
                if (i == first)
                    path.startNewSubPath (x, y);
                else
                    path.lineTo (x, y);
            }

            g.strokePath (path, juce::PathStrokeType (1.5f));

            if (1.0 / samplesPerPixel >= 8.0)  // einzelne Samples als Punkte
            {
                for (juce::int64 i = first; i <= last; ++i)
                {
                    const float x = (float) area.getX() + sampleToX ((double) i);
                    const float y = centreY - std::clamp (samples[(size_t) i], -1.0f, 1.0f) * halfHeight;
                    g.fillEllipse (x - 2.5f, y - 2.5f, 5.0f, 5.0f);
                }
            }
        }
    }
}

void WaveformView::paintRuler (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (findColour (ids::rulerBackground));
    g.fillRect (area);
    g.setColour (findColour (ids::panelBorder));
    g.fillRect (area.getX(), area.getBottom() - 1, area.getWidth(), 1);

    const double sampleRate = (double) audio->sampleRate;
    const double secondsPerPixel = samplesPerPixel / sampleRate;

    static const double steps[] = { 0.0005, 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0,
                                    10.0, 15.0, 30.0, 60.0, 120.0, 300.0, 600.0, 900.0, 1800.0, 3600.0 };
    double step = steps[std::size (steps) - 1];
    for (double s : steps)
        if (s / secondsPerPixel >= 90.0)
        {
            step = s;
            break;
        }

    const double startTime = viewStart / sampleRate;
    const double endTime = (viewStart + visibleSamples()) / sampleRate;
    const auto firstTick = (long long) std::floor (startTime / step);

    g.setFont (juce::Font (juce::FontOptions (11.5f)));

    for (long long n = firstTick; (double) n * step <= endTime + step; ++n)
    {
        const double t = (double) n * step;
        const float x = sampleToX (t * sampleRate);

        if (x > (float) getWidth() + 60.0f)
            break;

        g.setColour (findColour (ids::rulerTick));
        g.fillRect (x, (float) area.getBottom() - 11.0f, 1.0f, 10.0f);

        for (int k = 1; k < 5; ++k)
        {
            const float xm = sampleToX ((t + step * (double) k / 5.0) * sampleRate);
            g.fillRect (xm, (float) area.getBottom() - 5.0f, 1.0f, 4.0f);
        }

        if (x > -60.0f && t >= 0.0)
        {
            g.setColour (findColour (ids::rulerText));
            g.drawText (formatTime (t, step < 1.0), (int) x + 4, area.getY() + 2, 90, area.getHeight() - 12,
                        juce::Justification::topLeft, false);
        }
    }
}

void WaveformView::paintPlayhead (juce::Graphics& g, juce::Rectangle<int> rulerArea, juce::Rectangle<int> waveArea)
{
    const float x = sampleToX ((double) getPlayheadFrame());
    lastPlayheadX = x;

    if (x < 0.0f || x > (float) getWidth())
        return;

    const auto colour = findColour (ids::playhead);
    g.setColour (colour);
    g.fillRect (x - 0.5f, (float) waveArea.getY(), 1.5f, (float) waveArea.getHeight());

    juce::Path marker;
    marker.addTriangle (x - 5.5f, (float) rulerArea.getBottom() - 10.0f,
                        x + 5.5f, (float) rulerArea.getBottom() - 10.0f,
                        x, (float) rulerArea.getBottom() - 1.0f);
    g.fillPath (marker);
}

// ---------------------------------------------------------------------------
// Maus und Zeitgeber
// ---------------------------------------------------------------------------

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    if (! hasAudio())
        return;

    juce::Component::beginDragAutoRepeat (40);
    dragMoved = false;
    dragStartX = (float) e.x;

    auto setCursorFromX = [this] (float x)
    {
        const auto frame = clampFrame (xToSample (x));
        setCursor (frame);

        if (engine.isPlaying())
            engine.setPosition (frame);
    };

    if (e.y < kRulerHeight)
    {
        dragMode = DragMode::Ruler;
        setCursorFromX ((float) e.x);
        return;
    }

    if (hasSelection())
    {
        const float xStart = sampleToX ((double) selectionStart);
        const float xEnd = sampleToX ((double) selectionEnd);

        if (std::abs ((float) e.x - xStart) <= kEdgeTolerance)
        {
            dragMode = DragMode::MoveStart;
            dragAnchor = selectionEnd;
            return;
        }

        if (std::abs ((float) e.x - xEnd) <= kEdgeTolerance)
        {
            dragMode = DragMode::MoveEnd;
            dragAnchor = selectionStart;
            return;
        }
    }

    dragMode = DragMode::NewSelection;
    const auto clicked = clampFrame (xToSample ((float) e.x));

    if (e.mods.isShiftDown())
    {
        // Umschalt + Klick erweitert die Auswahl bzw. waehlt vom Cursor bis zum Klick
        if (hasSelection())
            dragAnchor = clicked < (selectionStart + selectionEnd) / 2 ? selectionEnd : selectionStart;
        else
            dragAnchor = cursor;

        dragMoved = true;
        setSelection (std::min (dragAnchor, clicked), std::max (dragAnchor, clicked));
    }
    else
    {
        dragAnchor = clicked;
    }
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::None || ! hasAudio())
        return;

    const float x = (float) e.x;

    // Am Rand scrollen, solange die Maus gedrueckt ausserhalb gehalten wird
    if (x < 0.0f || x > (float) getWidth())
    {
        const float overshoot = x < 0.0f ? x : x - (float) getWidth();
        setViewStart (viewStart + (double) overshoot * samplesPerPixel * 0.3);
    }

    const auto position = clampFrame (xToSample (x));

    if (dragMode == DragMode::Ruler)
    {
        setCursor (position);

        if (engine.isPlaying())
            engine.setPosition (position);

        return;
    }

    if (! dragMoved && std::abs (x - dragStartX) < 3.0f)
        return;

    dragMoved = true;
    setSelection (std::min (dragAnchor, position), std::max (dragAnchor, position));
}

void WaveformView::mouseUp (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::NewSelection && ! dragMoved)
    {
        // Einfacher Klick: Auswahl aufheben und Cursor setzen
        clearSelection();
        const auto frame = clampFrame (xToSample ((float) e.x));
        setCursor (frame);

        if (engine.isPlaying())
            engine.setPosition (frame);
    }
    else if (dragMode != DragMode::None && dragMode != DragMode::Ruler && hasSelection())
    {
        setCursor (selectionStart);
    }

    dragMode = DragMode::None;
}

void WaveformView::mouseMove (const juce::MouseEvent& e)
{
    if (hasAudio() && hasSelection() && e.y >= kRulerHeight)
    {
        const bool nearEdge = std::abs ((float) e.x - sampleToX ((double) selectionStart)) <= kEdgeTolerance
                              || std::abs ((float) e.x - sampleToX ((double) selectionEnd)) <= kEdgeTolerance;

        setMouseCursor (nearEdge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::IBeamCursor);
        return;
    }

    setMouseCursor (e.y < kRulerHeight ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::IBeamCursor);
}

void WaveformView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.y >= kRulerHeight)
        selectAll();
}

void WaveformView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (! hasAudio())
        return;

    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
    {
        zoomBy (std::exp ((double) wheel.deltaY * 0.8), (float) e.x);
        return;
    }

    const float delta = std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? wheel.deltaX : wheel.deltaY;
    setViewStart (viewStart - (double) delta * visibleSamples() * 0.3);
}

void WaveformView::timerCallback()
{
    const bool playing = engine.isPlaying();

    if (playing)
    {
        if (followPlayhead && dragMode == DragMode::None)
            ensureVisible (engine.getPosition());

        const float x = sampleToX ((double) engine.getPosition());
        const int oldX = (int) lastPlayheadX;
        const int newX = (int) x;

        if (oldX != newX)
        {
            // Nur die schmalen Streifen alter und neuer Position neu zeichnen
            repaint (oldX - 8, 0, 16, getHeight() - kScrollBarHeight);
            repaint (newX - 8, 0, 16, getHeight() - kScrollBarHeight);
        }
    }
    else if (wasPlaying)
    {
        repaint();
    }

    wasPlaying = playing;
}

}  // namespace audioz::ui
