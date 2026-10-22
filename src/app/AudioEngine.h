// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AudioData.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>
#include <memory>

namespace audioz::ui
{

/** Wiedergabe der Daten aus dem Speicher ueber das Audio-Geraet.

    Die Audio-Schnittstelle laeuft in einem eigenen Thread. Alle Zugriffe auf die
    Daten sind durch eine kurze Sperre geschuetzt, die der Audio-Thread nur
    "versucht" (try-lock): im Zweifel gibt er einen Block Stille aus, statt zu warten. */
class AudioEngine : private juce::AudioSource
{
public:
    AudioEngine();
    ~AudioEngine() override;

    /** Oeffnet das Audio-Geraet (optional mit gespeichertem Zustand).
        Rueckgabe: Fehlertext, leer bei Erfolg. */
    juce::String initialise (const juce::XmlElement* savedState);

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }

    void setAudio (std::shared_ptr<const audioz::AudioData> data);

    /** Schleife: Bereich [start, end) in Frames; end <= start bedeutet ganze Datei. */
    void setLoopRegion (juce::int64 start, juce::int64 end);
    void setLooping (bool shouldLoop);
    bool isLooping() const { return looping.load(); }

    /** Startet die Wiedergabe ab `startFrame` bis `endFrame` (-1: bis zum Ende). */
    void play (juce::int64 startFrame, juce::int64 endFrame);
    void pause();
    bool isPlaying() const { return playing.load(); }

    juce::int64 getPosition() const { return position.load(); }
    void setPosition (juce::int64 frame);

    void setVolume (float linearGain) { volume.store (linearGain); }
    float getVolume() const { return volume.load(); }

    /** Spitzenpegel seit dem letzten Aufruf (linear, 0..1+), fuer die Pegelanzeige. */
    float takePeak (int channel);

    bool hasDevice() const;

private:
    // juce::AudioSource (Ausgabe in Geraete-Abtastrate)
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo&) override;

    /** Liefert Audio in der Abtastrate der Datei. */
    class Reader;
    void readBlock (const juce::AudioSourceChannelInfo&);
    void updateResamplingRatio();

    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer player;
    std::unique_ptr<Reader> reader;
    juce::ResamplingAudioSource resampler;

    juce::CriticalSection dataLock;
    std::shared_ptr<const audioz::AudioData> data;  // geschuetzt durch dataLock
    const audioz::AudioData* dataPtr = nullptr;     // geschuetzt durch dataLock

    std::atomic<bool> playing { false };
    std::atomic<bool> looping { false };
    std::atomic<juce::int64> position { 0 };
    std::atomic<juce::int64> playEnd { -1 };
    std::atomic<juce::int64> loopStart { 0 };
    std::atomic<juce::int64> loopEnd { -1 };
    std::atomic<float> volume { 0.9f };
    std::atomic<float> peaks[2];
    std::atomic<double> deviceRate { 44100.0 };
    std::atomic<int> fileRate { 44100 };
};

}  // namespace audioz::ui
