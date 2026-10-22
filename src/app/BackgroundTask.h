// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <exception>
#include <new>
#include <functional>

namespace audioz::ui
{

/** Fuehrt eine laengere Berechnung in einem eigenen Thread aus und zeigt dabei einen
    Fortschrittsdialog ("bitte warten"), damit die Oberflaeche nicht einfriert.

    Das Objekt loescht sich selbst. `work` laeuft im Hintergrund und darf nur eigene
    Daten anfassen; `done` laeuft danach im Hauptthread. `error` ist leer, wenn alles
    gut ging (z. B. nicht genug Speicher fuehrt sonst zu einem Fehlertext). */
class BackgroundTask : public juce::ThreadWithProgressWindow
{
public:
    static void run (const juce::String& title, const juce::String& message, std::function<void()> work,
                     std::function<void (const juce::String& error)> done, juce::Component* centreAround = nullptr)
    {
        auto* task = new BackgroundTask (title, message, std::move (work), std::move (done), centreAround);
        task->launchThread();
    }

private:
    BackgroundTask (const juce::String& title, const juce::String& statusText, std::function<void()> w,
                    std::function<void (const juce::String&)> d, juce::Component* centreAround)
        : juce::ThreadWithProgressWindow (title, true, false, 10000, {}, centreAround),
          work (std::move (w)), done (std::move (d))
    {
        setStatusMessage (statusText);
    }

    void run() override
    {
        setProgress (-1.0);  // unbestimmter Fortschrittsbalken

        try
        {
            work();
        }
        catch (const std::bad_alloc&)
        {
            error = juce::String::fromUTF8 ("Nicht genug Arbeitsspeicher für diese Bearbeitung.");
        }
        catch (const std::exception& e)
        {
            error = juce::String::fromUTF8 (e.what());
        }
        catch (...)
        {
            error = "Unbekannter Fehler.";
        }
    }

    void threadComplete (bool) override
    {
        auto callback = std::move (done);
        auto errorText = error;
        auto* self = this;

        // Das Objekt wird erst nach Rueckkehr aus dieser Methode freigegeben.
        juce::MessageManager::callAsync ([self, callback = std::move (callback), errorText]
        {
            delete self;
            if (callback)
                callback (errorText);
        });
    }

    std::function<void()> work;
    std::function<void (const juce::String&)> done;
    juce::String error;
};

}  // namespace audioz::ui
