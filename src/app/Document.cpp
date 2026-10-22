// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "Document.h"

namespace audioz::ui
{

juce::String Document::getDisplayName() const
{
    return file == juce::File() ? juce::String ("Unbenannt") : file.getFileName();
}

void Document::load (audioz::AudioData&& data, const juce::File& sourceFile)
{
    current = std::make_shared<const audioz::AudioData> (std::move (data));
    file = sourceFile;
    dirty = false;
    ++version;
    undoStack.clear();
    redoStack.clear();
}

void Document::commit (audioz::AudioData&& newData, const juce::String& actionName)
{
    if (current != nullptr)
        undoStack.push_back ({ current, actionName });

    redoStack.clear();
    current = std::make_shared<const audioz::AudioData> (std::move (newData));
    dirty = true;
    ++version;
    trimHistory();
}

void Document::markSaved (const juce::File& newFile)
{
    file = newFile;
    dirty = false;
}

void Document::undo()
{
    if (undoStack.empty())
        return;

    Step step = std::move (undoStack.back());
    undoStack.pop_back();

    redoStack.push_back ({ current, step.name });
    current = std::move (step.state);
    dirty = true;
    ++version;
}

void Document::redo()
{
    if (redoStack.empty())
        return;

    Step step = std::move (redoStack.back());
    redoStack.pop_back();

    undoStack.push_back ({ current, step.name });
    current = std::move (step.state);
    dirty = true;
    ++version;
}

void Document::setHistoryBudgetBytes (size_t bytes)
{
    historyBudget = bytes;
    trimHistory();
}

size_t Document::getHistoryBytes() const
{
    size_t total = 0;

    for (const auto& s : undoStack)
        total += s.state != nullptr ? s.state->memoryBytes() : 0;
    for (const auto& s : redoStack)
        total += s.state != nullptr ? s.state->memoryBytes() : 0;

    return total;
}

void Document::trimHistory()
{
    // Aelteste Schritte zuerst verwerfen, mindestens ein Schritt bleibt erhalten
    while (undoStack.size() > 1 && getHistoryBytes() > historyBudget)
        undoStack.erase (undoStack.begin());
}

}  // namespace audioz::ui
