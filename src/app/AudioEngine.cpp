// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioEngine.h"

#include <algorithm>

namespace audioz::ui
{

class AudioEngine::Reader : public juce::AudioSource
{
public:
    explicit Reader (AudioEngine& o) : owner (o) {}

    void prepareToPlay (int, double) override {}
    void releaseResources() override {}
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override { owner.readBlock (info); }

private:
    AudioEngine& owner;
};

AudioEngine::AudioEngine()
    : reader (std::make_unique<Reader> (*this)),
      resampler (reader.get(), false, 2)
{
    peaks[0] = 0.0f;
    peaks[1] = 0.0f;
}

AudioEngine::~AudioEngine()
{
    deviceManager.removeAudioCallback (&player);
    player.setSource (nullptr);
    deviceManager.closeAudioDevice();
}

juce::String AudioEngine::initialise (const juce::XmlElement* savedState)
{
    auto error = deviceManager.initialise (0, 2, savedState, true);

    if (error.isEmpty() && deviceManager.getCurrentAudioDevice() == nullptr)
        error = juce::String::fromUTF8 ("Es wurde kein Audio-Ausgabegerät gefunden.");

    player.setSource (this);
    deviceManager.addAudioCallback (&player);
    return error;
}

bool AudioEngine::hasDevice() const
{
    return deviceManager.getCurrentAudioDevice() != nullptr;
}

void AudioEngine::setAudio (std::shared_ptr<const audioz::AudioData> newData)
{
    {
        const juce::ScopedLock lock (dataLock);
        std::swap (data, newData);
        dataPtr = data.get();
        fileRate = data != nullptr ? data->sampleRate : 44100;
    }

    // `newData` haelt jetzt die alten Daten und gibt sie hier, ausserhalb der Sperre und
    // ausserhalb des Audio-Threads, frei.
    newData.reset();
    updateResamplingRatio();
}

void AudioEngine::setLoopRegion (juce::int64 start, juce::int64 end)
{
    loopStart = start;
    loopEnd = end;
}

void AudioEngine::setLooping (bool shouldLoop)
{
    looping = shouldLoop;
}

void AudioEngine::play (juce::int64 startFrame, juce::int64 endFrame)
{
    position = std::max<juce::int64> (0, startFrame);
    playEnd = endFrame;
    resampler.flushBuffers();
    playing = true;
}

void AudioEngine::pause()
{
    playing = false;
}

void AudioEngine::setPosition (juce::int64 frame)
{
    position = std::max<juce::int64> (0, frame);

    if (playing.load())
        resampler.flushBuffers();
}

float AudioEngine::takePeak (int channel)
{
    return peaks[juce::jlimit (0, 1, channel)].exchange (0.0f);
}

void AudioEngine::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    deviceRate = sampleRate;
    resampler.prepareToPlay (samplesPerBlockExpected, sampleRate);
    updateResamplingRatio();
}

void AudioEngine::releaseResources()
{
    resampler.releaseResources();
}

void AudioEngine::updateResamplingRatio()
{
    const double out = deviceRate.load();
    resampler.setResamplingRatio (out > 0.0 ? (double) fileRate.load() / out : 1.0);
}

void AudioEngine::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    resampler.getNextAudioBlock (info);

    const float gain = volume.load();
    const int channels = std::min (2, info.buffer->getNumChannels());

    for (int c = 0; c < channels; ++c)
    {
        info.buffer->applyGain (c, info.startSample, info.numSamples, gain);

        const float peak = info.buffer->getMagnitude (c, info.startSample, info.numSamples);
        if (peak > peaks[c].load())
            peaks[c].store (peak);
    }
}

void AudioEngine::readBlock (const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();

    const juce::ScopedTryLock lock (dataLock);
    if (! lock.isLocked() || dataPtr == nullptr || ! playing.load())
        return;

    const auto& audio = *dataPtr;
    const juce::int64 total = (juce::int64) audio.numFrames();
    const int sourceChannels = audio.numChannels();
    if (total == 0 || sourceChannels == 0)
    {
        playing = false;
        return;
    }

    const bool loop = looping.load();
    const juce::int64 regionStart = std::clamp<juce::int64> (loopStart.load(), 0, total);
    const juce::int64 regionEnd = std::clamp<juce::int64> (loopEnd.load(), 0, total);
    const bool hasRegion = loop && regionEnd > regionStart;

    const juce::int64 limit = playEnd.load();
    juce::int64 end = hasRegion ? regionEnd : (limit > 0 ? std::min (limit, total) : total);
    const juce::int64 restart = hasRegion ? regionStart : 0;

    juce::int64 pos = position.load();
    int written = 0;

    while (written < info.numSamples)
    {
        if (pos >= end)
        {
            if (! loop)
            {
                playing = false;
                pos = end;
                break;
            }

            pos = restart;
            end = hasRegion ? regionEnd : total;

            if (pos >= end)  // leere Schleife: nichts abzuspielen
            {
                playing = false;
                break;
            }
        }

        const int count = (int) std::min<juce::int64> (info.numSamples - written, end - pos);

        for (int c = 0; c < info.buffer->getNumChannels(); ++c)
        {
            const auto& source = audio.channels[(size_t) std::min (c, sourceChannels - 1)];
            info.buffer->copyFrom (c, info.startSample + written, source.data() + pos, count);
        }

        pos += count;
        written += count;
    }

    position = pos;
}

}  // namespace audioz::ui
