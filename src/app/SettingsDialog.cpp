// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "SettingsDialog.h"

#include "Formatting.h"

namespace audioz::ui
{

namespace
{
    constexpr int kRow = 40;

    /** Seite mit Zeilen aus "Beschriftung | Steuerelement". */
    class FormPage : public juce::Component
    {
    public:
        void addRow (const juce::String& labelText, juce::Component& control, int height = 30)
        {
            auto label = std::make_unique<juce::Label> ("", labelText);
            addAndMakeVisible (*label);
            addAndMakeVisible (control);
            rows.push_back ({ std::move (label), &control, height });
        }

        void addNote (const juce::String& text, int height)
        {
            auto note = std::make_unique<juce::Label> ("", text);
            note->setJustificationType (juce::Justification::topLeft);
            note->setColour (juce::Label::textColourId, juce::Colours::grey);
            addAndMakeVisible (*note);
            rows.push_back ({ std::move (note), nullptr, height });
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (20, 16);

            for (auto& r : rows)
            {
                auto line = area.removeFromTop (std::max (kRow, r.height + 8));
                line = line.withHeight (r.height);

                if (r.control == nullptr)
                {
                    r.label->setBounds (line);
                    area.removeFromTop (0);
                    continue;
                }

                r.label->setBounds (line.removeFromLeft (200));
                r.control->setBounds (line);
            }
        }

    private:
        struct Row
        {
            std::unique_ptr<juce::Label> label;
            juce::Component* control;
            int height;
        };

        std::vector<Row> rows;
    };

    juce::Slider* makeSlider (juce::Slider& s, double min, double max, double step, const juce::String& suffix, int decimals)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 100, 24);
        s.setRange (min, max, step);
        s.setNumDecimalPlacesToDisplay (decimals);
        s.setTextValueSuffix (" " + suffix);
        s.setWantsKeyboardFocus (false);
        return &s;
    }

    class AppearancePage : public FormPage
    {
    public:
        explicit AppearancePage (AppSettings& s) : settings (s)
        {
            theme.addItem ("System (automatisch)", 1);
            theme.addItem ("Hell", 2);
            theme.addItem ("Dunkel", 3);
            theme.setSelectedId (settings.getThemeMode() == ThemeMode::System ? 1
                                 : settings.getThemeMode() == ThemeMode::Light ? 2 : 3, juce::dontSendNotification);
            theme.onChange = [this]
            {
                const int id = theme.getSelectedId();
                settings.setThemeMode (id == 1 ? ThemeMode::System : (id == 2 ? ThemeMode::Light : ThemeMode::Dark));
            };

            int index = 1;
            for (const auto& preset : accentPresets())
                accent.addItem (juce::String::fromUTF8 (preset.name), index++);
            accent.setSelectedId (settings.getAccentIndex() + 1, juce::dontSendNotification);
            accent.onChange = [this] { settings.setAccentIndex (accent.getSelectedId() - 1); };

            follow.setButtonText (juce::String::fromUTF8 ("Ansicht beim Abspielen mitscrollen"));
            follow.setToggleState (settings.getFollowPlayhead(), juce::dontSendNotification);
            follow.onClick = [this] { settings.setFollowPlayhead (follow.getToggleState()); };

            addRow ("Design", theme);
            addRow ("Akzentfarbe", accent);
            addRow ("Wiedergabe", follow);
            addNote (juce::String::fromUTF8 ("Bei \"System\" folgt AudioZ der Einstellung von Windows für helle bzw. dunkle Apps."), 40);
        }

    private:
        AppSettings& settings;
        juce::ComboBox theme, accent;
        juce::ToggleButton follow;
    };

    class EditingPage : public FormPage
    {
    public:
        explicit EditingPage (AppSettings& s) : settings (s)
        {
            makeSlider (undoMemory, 128, 8192, 64, "MB", 0);
            undoMemory.setValue (settings.getUndoMemoryMb(), juce::dontSendNotification);
            undoMemory.onValueChange = [this] { settings.setUndoMemoryMb ((int) undoMemory.getValue()); };

            makeSlider (fade, 0.01, 10.0, 0.01, "s", 2);
            fade.setSkewFactorFromMidPoint (1.0);
            fade.setValue (settings.getDefaultFadeSeconds(), juce::dontSendNotification);
            fade.onValueChange = [this] { settings.setDefaultFadeSeconds (fade.getValue()); };

            makeSlider (lufs, -30.0, -5.0, 0.5, "LUFS", 1);
            lufs.setValue (settings.getTargetLufs(), juce::dontSendNotification);
            lufs.onValueChange = [this] { settings.setTargetLufs (lufs.getValue()); };

            makeSlider (ceiling, -6.0, 0.0, 0.1, "dBTP", 1);
            ceiling.setValue (settings.getTruePeakCeilingDb(), juce::dontSendNotification);
            ceiling.onValueChange = [this] { settings.setTruePeakCeilingDb (ceiling.getValue()); };

            for (int kbps : { 128, 160, 192, 224, 256, 320 })
                mp3.addItem (juce::String (kbps) + " kbit/s", kbps);
            mp3.setSelectedId (settings.getMp3Bitrate(), juce::dontSendNotification);
            if (mp3.getSelectedId() == 0)
                mp3.setSelectedId (192, juce::dontSendNotification);
            mp3.onChange = [this] { settings.setMp3Bitrate (mp3.getSelectedId()); };

            addRow (juce::String::fromUTF8 ("Speicher für Rückgängig"), undoMemory);
            addRow (juce::String::fromUTF8 ("Standard-Fadelänge"), fade);
            addRow ("Standard-Lautheitsziel", lufs);
            addRow ("Obergrenze True Peak", ceiling);
            addRow ("Standard-MP3-Bitrate", mp3);
            addNote (juce::String::fromUTF8 ("Diese Werte sind die Vorgaben in den jeweiligen Dialogen. "
                                             "Typisch: -14 LUFS für Streaming, -23 LUFS für Rundfunk (EBU R128)."), 40);
        }

    private:
        AppSettings& settings;
        juce::Slider undoMemory, fade, lufs, ceiling;
        juce::ComboBox mp3;
    };

    class SettingsContent : public juce::Component
    {
    public:
        SettingsContent (AppSettings& settings, juce::AudioDeviceManager& deviceManager)
            : audioSelector (deviceManager, 0, 0, 0, 2, false, false, true, false),
              tabs (juce::TabbedButtonBar::TabsAtTop)
        {
            const auto bg = findColour (juce::ResizableWindow::backgroundColourId);
            tabs.setTabBarDepth (38);
            tabs.setOutline (0);
            tabs.addTab ("Darstellung", bg, new AppearancePage (settings), true);
            tabs.addTab ("Audio", bg, &audioSelector, false);
            tabs.addTab ("Bearbeitung", bg, new EditingPage (settings), true);
            addAndMakeVisible (tabs);

            close.setButtonText (juce::String::fromUTF8 ("Schließen"));
            close.onClick = [this]
            {
                if (auto* w = findParentComponentOfClass<juce::DialogWindow>())
                    w->exitModalState (0);
            };
            addAndMakeVisible (close);

            setSize (580, 520);
        }

        void resized() override
        {
            auto area = getLocalBounds();
            auto bottom = area.removeFromBottom (56).reduced (16, 10);
            close.setBounds (bottom.removeFromRight (120));
            tabs.setBounds (area);
        }

    private:
        juce::AudioDeviceSelectorComponent audioSelector;  // vor `tabs`, damit es danach freigegeben wird
        juce::TabbedComponent tabs;
        juce::TextButton close;
    };
}

void showSettingsDialog (juce::Component* parent, AppSettings& settings, juce::AudioDeviceManager& deviceManager)
{
    auto content = std::make_unique<SettingsContent> (settings, deviceManager);

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Einstellungen";
    options.dialogBackgroundColour = content->findColour (juce::ResizableWindow::backgroundColourId);
    options.content.setOwned (content.release());
    options.componentToCentreAround = parent;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

}  // namespace audioz::ui
