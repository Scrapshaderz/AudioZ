// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AudioEngine.h"
#include "PeakCache.h"
#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace audioz::ui
{

/** Wellenform mit Zeitlineal, Auswahl, Zoom, Scrollleiste und Abspielposition. */
class WaveformView : public juce::Component,
                     private juce::Timer,
                     private juce::ScrollBar::Listener
{
public:
    explicit WaveformView (AudioEngine& engineToUse);
    ~WaveformView() override;

    enum class ViewMode
    {
        FitAll,  // ganze Datei anzeigen
        Keep     // Ausschnitt und Zoom moeglichst beibehalten
    };

    void setAudio (std::shared_ptr<const audioz::AudioData> newAudio, ViewMode mode);
    bool hasAudio() const { return audio != nullptr && ! audio->empty(); }

    // Auswahl und Cursor (Frames)
    bool hasSelection() const { return selectionEnd > selectionStart; }
    juce::int64 getSelectionStart() const { return selectionStart; }
    juce::int64 getSelectionEnd() const { return selectionEnd; }
    juce::int64 getCursor() const { return cursor; }

    void setSelection (juce::int64 start, juce::int64 end);
    void clearSelection();
    void selectAll();
    void setCursor (juce::int64 frame);

    /** Position der Abspielmarke: laufende Wiedergabe oder Cursor. */
    juce::int64 getPlayheadFrame() const;

    // Ansicht
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void zoomToSelection();
    void ensureVisible (juce::int64 frame);
    void setFollowPlayhead (bool shouldFollow) { followPlayhead = shouldFollow; }

    std::function<void()> onSelectionChanged;
    std::function<void()> onCursorChanged;
    std::function<void()> onViewChanged;

    // Component
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void lookAndFeelChanged() override;

private:
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;

    // Geometrie
    static constexpr int kRulerHeight = 26;
    static constexpr int kScrollBarHeight = 12;
    juce::Rectangle<int> getWaveArea() const;
    double xToSample (float x) const { return viewStart + (double) x * samplesPerPixel; }
    float sampleToX (double sample) const { return (float) ((sample - viewStart) / samplesPerPixel); }
    juce::int64 clampFrame (double frame) const;
    double visibleSamples() const;

    void setViewStart (double newStart);
    void zoomBy (double factor, float anchorX);
    void updateScrollBar();
    void notifyView();

    void paintWaveform (juce::Graphics&, juce::Rectangle<int> area);
    void paintRuler (juce::Graphics&, juce::Rectangle<int> area);
    void paintPlayhead (juce::Graphics&, juce::Rectangle<int> rulerArea, juce::Rectangle<int> waveArea);
    void paintPlaceholder (juce::Graphics&);

    enum class DragMode { None, Ruler, NewSelection, MoveStart, MoveEnd };

    AudioEngine& engine;
    std::shared_ptr<const audioz::AudioData> audio;
    PeakCache peaks;
    juce::ScrollBar scrollBar { false };

    double viewStart = 0.0;
    double samplesPerPixel = 1.0;
    juce::int64 selectionStart = 0, selectionEnd = 0;
    juce::int64 cursor = 0;

    DragMode dragMode = DragMode::None;
    juce::int64 dragAnchor = 0;
    float dragStartX = 0.0f;
    bool dragMoved = false;
    bool followPlayhead = true;
    bool wasPlaying = false;
    float lastPlayheadX = -1.0f;
};

}  // namespace audioz::ui
