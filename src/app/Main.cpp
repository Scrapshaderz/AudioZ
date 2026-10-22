// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "AppSettings.h"
#include "MainComponent.h"
#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace audioz::ui
{

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (AppSettings& appSettings)
        : juce::DocumentWindow ("AudioZ",
                                juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
                                juce::DocumentWindow::allButtons),
          settings (appSettings)
    {
        setUsingNativeTitleBar (true);

        main = new MainComponent (appSettings);
        setContentOwned (main, true);
        main->onTitleChanged = [this] { setName (main->getWindowTitle()); };
        setName (main->getWindowTitle());

        setResizable (true, true);
        setResizeLimits (980, 560, 8000, 6000);

        const auto state = settings.getWindowState();
        if (state.isNotEmpty())
            restoreWindowStateFromString (state);
        else
            centreWithSize (1180, 720);

        setVisible (true);
    }

    ~MainWindow() override
    {
        settings.setWindowState (getWindowStateAsString());
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

    void lookAndFeelChanged() override
    {
        setBackgroundColour (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    }

    MainComponent* main = nullptr;

private:
    AppSettings& settings;
};

class AudioZApplication : public juce::JUCEApplication,
                          private juce::ChangeListener,
                          private juce::DarkModeSettingListener
{
public:
    const juce::String getApplicationName() override { return "AudioZ"; }
    const juce::String getApplicationVersion() override { return AUDIOZ_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& commandLine) override
    {
        settings = std::make_unique<AppSettings>();
        settings->addChangeListener (this);
        juce::Desktop::getInstance().addDarkModeSettingListener (this);
        applyTheme (true);

        mainWindow = std::make_unique<MainWindow> (*settings);
        openFromCommandLine (commandLine);
    }

    void shutdown() override
    {
        if (settings != nullptr)
            settings->removeChangeListener (this);

        juce::Desktop::getInstance().removeDarkModeSettingListener (this);

        mainWindow.reset();

        if (settings != nullptr)
            settings->save();

        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        lookAndFeel.reset();
        settings.reset();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow == nullptr || mainWindow->main == nullptr)
        {
            quit();
            return;
        }

        mainWindow->main->confirmDiscardChanges ([] { juce::JUCEApplication::getInstance()->quit(); });
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        if (mainWindow != nullptr)
        {
            mainWindow->toFront (true);
            openFromCommandLine (commandLine);
        }
    }

private:
    void openFromCommandLine (const juce::String& commandLine)
    {
        const juce::File file (commandLine.unquoted().trim());

        if (mainWindow != nullptr && mainWindow->main != nullptr && file.existsAsFile())
            mainWindow->main->openFile (file);
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        applyTheme (false);

        if (mainWindow != nullptr && mainWindow->main != nullptr)
            mainWindow->main->settingsChanged();
    }

    void darkModeSettingChanged() override
    {
        applyTheme (false);
    }

    void applyTheme (bool force)
    {
        const auto mode = settings->getThemeMode();
        const bool dark = mode == ThemeMode::Dark
                          || (mode == ThemeMode::System && juce::Desktop::getInstance().isDarkModeActive());
        const auto accent = settings->getAccentColour();

        if (! force && lookAndFeel != nullptr && lookAndFeel->isDark() == dark && lookAndFeel->getAccent() == accent)
            return;

        auto newLookAndFeel = std::make_unique<AudioZLookAndFeel> (dark, accent);
        juce::LookAndFeel::setDefaultLookAndFeel (newLookAndFeel.get());
        lookAndFeel = std::move (newLookAndFeel);  // das alte Aussehen wird hier freigegeben

        for (int i = 0; i < juce::TopLevelWindow::getNumTopLevelWindows(); ++i)
            if (auto* window = juce::TopLevelWindow::getTopLevelWindow (i))
                window->sendLookAndFeelChange();
    }

    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<AudioZLookAndFeel> lookAndFeel;
    std::unique_ptr<MainWindow> mainWindow;
};

}  // namespace audioz::ui

START_JUCE_APPLICATION (audioz::ui::AudioZApplication)
