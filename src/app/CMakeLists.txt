# AudioZ - Audio-Editor und Mastering-Werkzeug
# Copyright (C) 2026 AudioZ-Mitwirkende
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Die Oberflaeche basiert auf JUCE (AGPL-3.0, siehe LICENSE und README).

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(AudioZ
    PRODUCT_NAME "AudioZ"
    COMPANY_NAME "AudioZ"
    VERSION "${PROJECT_VERSION}"
    BUNDLE_ID "org.audioz.AudioZ"
    DOCUMENT_EXTENSIONS wav flac mp3 ogg aif aiff
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/icon.png")

target_sources(AudioZ PRIVATE
    AppSettings.cpp
    AudioEngine.cpp
    AudioIO.cpp
    Document.cpp
    FormDialog.cpp
    Main.cpp
    MainComponent.cpp
    MainEdits.cpp
    MainFiles.cpp
    SettingsDialog.cpp
    Theme.cpp
    WaveformView.cpp)

target_compile_definitions(AudioZ PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_OGGVORBIS=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_APPLICATION_NAME_STRING="AudioZ"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    AUDIOZ_VERSION_STRING="${PROJECT_VERSION}")

target_include_directories(AudioZ PRIVATE .)

target_link_libraries(AudioZ
    PRIVATE
        audioz_core
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    # JUCE-Warnungen nicht als Fehler behandeln, Konsolenfenster vermeiden
    target_compile_options(AudioZ PRIVATE /wd4100 /wd4505)
endif()
