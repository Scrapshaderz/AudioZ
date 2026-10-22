// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AudioData.h"

#include <juce_core/juce_core.h>

#include <memory>
#include <vector>

namespace audioz::ui
{

/** Das geoeffnete Audio samt Rueckgaengig-Verlauf.

    Jeder Bearbeitungsschritt erzeugt einen neuen, unveraenderlichen Zustand. Die
    Oberflaeche und die Wiedergabe halten nur shared_ptr auf diese Zustaende, dadurch
    kann nie ein Zugriff auf Daten ins Leere laufen. */
class Document
{
public:
    using AudioPtr = std::shared_ptr<const audioz::AudioData>;

    bool hasAudio() const { return current != nullptr && ! current->empty(); }
    AudioPtr audio() const { return current; }

    const juce::File& getFile() const { return file; }
    bool isDirty() const { return dirty; }
    juce::String getDisplayName() const;

    /** Zaehlt bei jeder Aenderung hoch; erlaubt zu erkennen, ob Messwerte noch gelten. */
    int getVersion() const { return version; }

    /** Format der Quelldatei, fuer "Speichern" im selben Format. */
    int sourceBitsPerSample = 0;
    bool sourceIsFloat = false;
    juce::String sourceFormatName;

    /** Ersetzt das Dokument (neue Datei geoeffnet). Loescht den Verlauf. */
    void load (audioz::AudioData&& data, const juce::File& sourceFile);

    /** Uebernimmt das Ergebnis eines Bearbeitungsschritts und merkt den alten Zustand. */
    void commit (audioz::AudioData&& newData, const juce::String& actionName);

    void markSaved (const juce::File& newFile);

    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    juce::String getUndoName() const { return undoStack.empty() ? juce::String() : undoStack.back().name; }
    juce::String getRedoName() const { return redoStack.empty() ? juce::String() : redoStack.back().name; }

    void undo();
    void redo();

    void setHistoryBudgetBytes (size_t bytes);
    size_t getHistoryBytes() const;

private:
    struct Step
    {
        AudioPtr state;      // Zustand VOR der Aktion (Undo) bzw. NACH der Aktion (Redo)
        juce::String name;
    };

    void trimHistory();

    AudioPtr current;
    juce::File file;
    bool dirty = false;
    int version = 0;
    size_t historyBudget = (size_t) 1024 * 1024 * 1024;

    std::vector<Step> undoStack;
    std::vector<Step> redoStack;
};

}  // namespace audioz::ui
