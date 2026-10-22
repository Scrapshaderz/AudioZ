// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests der Kern-Bibliothek. Bewusst ohne Test-Framework: ein einfacher
// Runner, der bei Fehlern einen Exit-Code ungleich 0 liefert.

#include "AudioData.h"
#include "Bpm.h"
#include "Dither.h"
#include "Fades.h"
#include "Gain.h"
#include "Limiter.h"
#include "Loudness.h"
#include "LoudnessNormalize.h"
#include "Mp3Encoder.h"
#include "Pitch.h"
#include "Resample.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>

using namespace audioz;

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    int g_checks = 0;
    int g_failures = 0;
    const char* g_currentTest = "";

    void report (bool ok, const char* expr, const char* file, int line)
    {
        ++g_checks;
        if (! ok)
        {
            ++g_failures;
            std::printf ("    FEHLER [%s] %s:%d: %s\n", g_currentTest, file, line, expr);
        }
    }

#define CHECK(cond) report ((cond), #cond, __FILE__, __LINE__)

#define CHECK_NEAR(actual, expected, tolerance)                                                       \
    do                                                                                                \
    {                                                                                                 \
        const double a_ = (actual), e_ = (expected), t_ = (tolerance);                                \
        const bool ok_ = std::fabs (a_ - e_) <= t_;                                                   \
        if (! ok_)                                                                                    \
            std::printf ("    Wert %s = %.5f, erwartet %.5f +- %.5f\n", #actual, a_, e_, t_);         \
        report (ok_, #actual " ~= " #expected, __FILE__, __LINE__);                                   \
    } while (false)

    // ---- Signalerzeugung ---------------------------------------------------

    AudioData makeSine (int channels, double seconds, double freq, double amplitude, int sampleRate = 44100)
    {
        const size_t n = (size_t) std::llround (seconds * sampleRate);
        AudioData a (channels, n, sampleRate);

        for (auto& ch : a.channels)
            for (size_t i = 0; i < n; ++i)
                ch[i] = (float) (amplitude * std::sin (2.0 * kPi * freq * (double) i / sampleRate));

        return a;
    }

    /** Frequenz ueber positive Nulldurchgaenge (reicht fuer reine Sinustoene). */
    double measureFrequency (const AudioData& a, double fromSeconds, double toSeconds)
    {
        const auto& x = a.channels[0];
        const size_t from = (size_t) (fromSeconds * a.sampleRate);
        const size_t to = std::min (x.size(), (size_t) (toSeconds * a.sampleRate));
        int crossings = 0;
        double firstCrossing = -1.0, lastCrossing = -1.0;

        for (size_t i = from + 1; i < to; ++i)
            if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            {
                // lineare Interpolation fuer eine genauere Position
                const double pos = (double) (i - 1) + (double) (-x[i - 1]) / (double) (x[i] - x[i - 1]);
                if (firstCrossing < 0.0)
                    firstCrossing = pos;
                lastCrossing = pos;
                ++crossings;
            }

        if (crossings < 3)
            return 0.0;

        return (double) (crossings - 1) * a.sampleRate / (lastCrossing - firstCrossing);
    }

    double rms (const AudioData& a, size_t from, size_t to)
    {
        double sum = 0.0;
        size_t count = 0;
        for (size_t i = from; i < to && i < a.numFrames(); ++i)
        {
            sum += (double) a.channels[0][i] * (double) a.channels[0][i];
            ++count;
        }
        return count > 0 ? std::sqrt (sum / (double) count) : 0.0;
    }

    /** Schlagzeug-aehnliches Muster: Bassdrum auf jedem Schlag, Hi-Hat dazwischen. */
    AudioData makeDrumPattern (double bpm, double seconds, int sampleRate = 44100)
    {
        const size_t n = (size_t) (seconds * sampleRate);
        AudioData a (2, n, sampleRate);
        std::mt19937 rng (1234);
        std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
        const double beat = 60.0 / bpm * sampleRate;

        for (double t = 0.0; t < (double) n; t += beat)
        {
            const size_t kickStart = (size_t) t;
            for (size_t i = 0; i < (size_t) (0.15 * sampleRate) && kickStart + i < n; ++i)
            {
                const double time = (double) i / sampleRate;
                const double freq = 55.0 + 90.0 * std::exp (-time * 40.0);
                const float s = (float) (0.9 * std::exp (-time * 22.0) * std::sin (2.0 * kPi * freq * time));
                a.channels[0][kickStart + i] += s;
                a.channels[1][kickStart + i] += s;
            }

            const size_t hatStart = (size_t) (t + beat * 0.5);
            for (size_t i = 0; i < (size_t) (0.04 * sampleRate) && hatStart + i < n; ++i)
            {
                const double time = (double) i / sampleRate;
                const float s = (float) (0.25 * std::exp (-time * 120.0)) * noise (rng);
                a.channels[0][hatStart + i] += s;
                a.channels[1][hatStart + i] += s;
            }
        }

        return a;
    }

    // ---- Tests ---------------------------------------------------------------

    void testAudioDataEditing()
    {
        AudioData a (2, 100, 44100);
        for (size_t i = 0; i < 100; ++i)
        {
            a.channels[0][i] = (float) i;
            a.channels[1][i] = (float) -(int) i;
        }

        const AudioData s = a.slice (10, 5);
        CHECK (s.numFrames() == 5 && s.channels[0][0] == 10.0f && s.channels[1][4] == -14.0f);

        AudioData b = a;
        b.erase (20, 30);
        CHECK (b.numFrames() == 70);
        CHECK (b.channels[0][19] == 19.0f && b.channels[0][20] == 50.0f);

        b.insert (20, a.slice (20, 30));
        CHECK (b.numFrames() == 100 && b.channels[0][49] == 49.0f && b.channels[1][99] == -99.0f);

        AudioData c = a;
        c.replace (10, 10, a.slice (0, 3));
        CHECK (c.numFrames() == 93 && c.channels[0][9] == 9.0f && c.channels[0][10] == 0.0f
               && c.channels[0][12] == 2.0f && c.channels[0][13] == 20.0f);

        AudioData d = a;
        d.silence (40, 10);
        CHECK (d.channels[0][45] == 0.0f && d.channels[0][50] == 50.0f);

        // Bereiche ausserhalb werden begrenzt, nichts stuerzt ab
        AudioData e = a;
        e.erase (90, 1000);
        CHECK (e.numFrames() == 90);
        CHECK (a.slice (500, 10).numFrames() == 0);

        // Kanalanpassung
        const AudioData mono = adaptChannels (a, 1);
        CHECK (mono.numChannels() == 1 && mono.channels[0][10] == 0.0f);
        AudioData monoSrc (1, 4, 44100);
        monoSrc.channels[0] = { 1, 2, 3, 4 };
        const AudioData stereo = adaptChannels (monoSrc, 2);
        CHECK (stereo.numChannels() == 2 && stereo.channels[1][2] == 3.0f);

        // Einfuegen in leeres Dokument uebernimmt das Kanalformat
        AudioData empty;
        empty.insert (0, a);
        CHECK (empty.numChannels() == 2 && empty.numFrames() == 100);
    }

    void testFades()
    {
        AudioData a (1, 1000, 44100);
        std::fill (a.channels[0].begin(), a.channels[0].end(), 1.0f);

        AudioData in = a;
        applyFadeIn (in, 0, 1000, FadeCurve::Linear);
        CHECK_NEAR (in.channels[0][0], 0.0, 1e-6);
        CHECK_NEAR (in.channels[0][999], 1.0, 1e-6);
        CHECK_NEAR (in.channels[0][500], 500.0 / 999.0, 1e-4);

        AudioData out = a;
        applyFadeOut (out, 0, 1000, FadeCurve::Linear);
        CHECK_NEAR (out.channels[0][0], 1.0, 1e-6);
        CHECK_NEAR (out.channels[0][999], 0.0, 1e-6);

        AudioData part = a;
        applyFadeIn (part, 200, 100, FadeCurve::EqualPower);
        CHECK (part.channels[0][199] == 1.0f && part.channels[0][200] == 0.0f && part.channels[0][400] == 1.0f);
        CHECK_NEAR (fadeShape (FadeCurve::EqualPower, 0.5f), std::sin (kPi / 4.0), 1e-5);
        CHECK_NEAR (fadeShape (FadeCurve::SCurve, 0.5f), 0.5, 1e-6);
    }

    void testGainAndPeakNormalize()
    {
        AudioData a = makeSine (2, 1.0, 440.0, 0.25);
        const auto r = normalizePeak (a, 0, kWholeFile, -1.0);
        CHECK (r.ok);
        CHECK_NEAR (r.peakDbBefore, -12.04, 0.05);
        CHECK_NEAR (r.gainDb, 11.04, 0.05);
        CHECK_NEAR (gainToDb (a.samplePeak (0, a.numFrames())), -1.0, 0.01);

        AudioData silence (1, 1000, 44100);
        CHECK (! normalizePeak (silence, 0, kWholeFile, 0.0).ok);
    }

    void testResampling()
    {
        const AudioData in = makeSine (1, 2.0, 1000.0, 0.5);

        // Varispeed: schneller = hoeher + kuerzer
        const AudioData fast = varispeed (in, 1.5);
        CHECK (std::llabs ((long long) fast.numFrames() - (long long) (in.numFrames() / 1.5)) <= 1);
        CHECK_NEAR (measureFrequency (fast, 0.2, 1.0), 1500.0, 5.0);
        CHECK_NEAR (rms (fast, 10000, 50000), 0.5 / std::sqrt (2.0), 0.01);

        const AudioData slow = varispeed (in, 0.5);
        CHECK (std::llabs ((long long) slow.numFrames() - (long long) (in.numFrames() * 2)) <= 1);
        CHECK_NEAR (measureFrequency (slow, 0.2, 3.0), 500.0, 3.0);

        // Alias-Schutz: 15 kHz bei Faktor 2 liegt ueber der neuen Nyquist-Frequenz und muss verschwinden
        const AudioData high = makeSine (1, 1.0, 15000.0, 0.5);
        const AudioData aliased = varispeed (high, 2.0);
        CHECK (rms (aliased, 5000, 20000) < 0.01);

        // Abtastraten-Umrechnung: gleiche Dauer und Tonhoehe
        const AudioData c = convertSampleRate (in, 48000);
        CHECK (c.sampleRate == 48000);
        CHECK (std::llabs ((long long) c.numFrames() - 96000) <= 1);
        CHECK_NEAR (measureFrequency (c, 0.2, 1.8), 1000.0, 1.0);
    }

    void testLoudnessMeasurement()
    {
        // Stereo-Sinus mit 1 kHz und Amplitude 0,1: 20*log10(0,1) = -20 LUFS
        const AudioData stereo = makeSine (2, 10.0, 1000.0, 0.1);
        const auto s = measureLoudness (stereo);
        CHECK (s.valid);
        CHECK_NEAR (s.integratedLufs, -20.0, 0.15);
        CHECK_NEAR (s.samplePeakDb, -20.0, 0.05);
        CHECK_NEAR (s.truePeakDb, -20.0, 0.1);

        // Mono wird wie Dual-Mono gemessen -> gleicher Wert
        const AudioData mono = makeSine (1, 10.0, 1000.0, 0.1);
        CHECK_NEAR (measureLoudness (mono).integratedLufs, -20.0, 0.15);

        // Zu kurz oder still
        CHECK (! measureLoudness (makeSine (2, 0.2, 1000.0, 0.1)).valid);
        CHECK (! measureLoudness (AudioData (2, 44100 * 5, 44100)).valid);

        // Teilbereich
        AudioData mixed = makeSine (2, 6.0, 1000.0, 0.1);
        for (size_t i = 0; i < 3 * 44100; ++i)
            for (auto& ch : mixed.channels)
                ch[i] *= 0.1f;  // erste Haelfte 20 dB leiser
        CHECK_NEAR (measureLoudness (mixed, 3 * 44100, 3 * 44100).integratedLufs, -20.0, 0.2);
        CHECK_NEAR (measureLoudness (mixed, 0, 3 * 44100).integratedLufs, -40.0, 0.2);
    }

    void testLoudnessNormalizeAndLimiter()
    {
        // 1) Ohne Begrenzung noetig
        AudioData a = makeSine (2, 10.0, 1000.0, 0.05);  // -26 LUFS
        LoudnessNormalizeParams params;
        params.targetLufs = -14.0;
        params.truePeakCeilingDb = -1.0;
        auto r = normalizeLoudness (a, 0, kWholeFile, params);
        CHECK (r.status == NormalizeStatus::Ok);
        CHECK_NEAR (r.measuredLufs, -26.0, 0.2);
        CHECK_NEAR (r.appliedGainDb, 12.0, 0.2);
        CHECK (! r.limiterUsed);
        CHECK_NEAR (r.after.integratedLufs, -14.0, 0.05);

        // 2) Stille Datei mit seltenen Spitzen: Limiter muss eingreifen
        std::mt19937 rng (42);
        std::normal_distribution<float> gauss (0.0f, 0.02f);
        AudioData spiky (2, 44100 * 12, 44100);
        for (auto& ch : spiky.channels)
            for (auto& s : ch)
                s = gauss (rng);
        for (size_t pos = 22050; pos + 4 < spiky.numFrames(); pos += 22050)
            for (size_t k = 0; k < 3; ++k)
                for (auto& ch : spiky.channels)
                    ch[pos + k] = 0.9f;  // kurze Impulse

        r = normalizeLoudness (spiky, 0, kWholeFile, params);
        CHECK (r.status == NormalizeStatus::Ok);
        CHECK (r.limiterUsed);
        CHECK (r.predictedTruePeakDb > params.truePeakCeilingDb);
        CHECK (r.after.truePeakDb <= params.truePeakCeilingDb + 0.05);
        std::printf ("    Spitzen-Test: gemessen %.1f LUFS, Gain %+.1f dB, Limiter-Absenkung %.1f dB, Ergebnis %.2f LUFS / %.2f dBTP\n",
                     r.measuredLufs, r.appliedGainDb, r.limiterReductionDb, r.after.integratedLufs, r.after.truePeakDb);

        // 3) Ohne Limiter wird nur verstaerkt und die Uebersteuerung gemeldet
        AudioData b = spiky;  // bereits bearbeitet, daher zuerst leiser machen
        applyGain (b, 0, kWholeFile, dbToGain (-30.0));
        params.useLimiter = false;
        params.targetLufs = -14.0;
        r = normalizeLoudness (b, 0, kWholeFile, params);
        CHECK (r.status == NormalizeStatus::Ok);
        CHECK (! r.limiterUsed);
        CHECK (r.predictedTruePeakDb > 0.0);
        CHECK_NEAR (r.after.integratedLufs, -14.0, 0.05);

        // 4) Stille laesst sich nicht normalisieren
        AudioData silent (2, 44100 * 5, 44100);
        CHECK (normalizeLoudness (silent, 0, kWholeFile, LoudnessNormalizeParams()).status == NormalizeStatus::TooShortOrSilent);

        // 5) Limiter direkt: stark uebersteuerter Sinus
        AudioData loud = makeSine (2, 3.0, 997.0, 1.5);
        const double reduction = limitToTruePeak (loud, 0, kWholeFile, -1.0);
        CHECK (reduction > 3.0);
        CHECK (measureTruePeakDb (loud) <= -1.0 + 0.05);
        // Das Signal bleibt ein Sinus (keine harte Zerstoerung): RMS im erwarteten Bereich
        CHECK (rms (loud, 20000, 100000) > 0.3);

        // 6) Nur ein Abschnitt wird begrenzt, der Rest bleibt unveraendert
        AudioData partial = makeSine (2, 4.0, 440.0, 0.4);
        for (size_t i = 44100; i < 2 * 44100; ++i)
            for (auto& ch : partial.channels)
                ch[i] *= 3.0f;
        const AudioData reference = partial;
        limitToTruePeak (partial, 44100, 44100, -1.0);
        CHECK (partial.channels[0][1000] == reference.channels[0][1000]);
        CHECK (partial.channels[0][3 * 44100] == reference.channels[0][3 * 44100]);
        CHECK (measureTruePeakDb (partial, 44100, 44100) <= -1.0 + 0.05);
    }

    void testPitch()
    {
        const AudioData in = makeSine (2, 3.0, 440.0, 0.4);

        // nur Tonhoehe: Dauer bleibt, Frequenz verdoppelt sich
        PitchParams p;
        p.semitones = 12.0;
        p.mode = PitchMode::KeepTempo;
        p.quality = PitchQuality::High;
        AudioData out = changePitch (in, p);
        CHECK (out.numFrames() == in.numFrames() && out.numChannels() == 2);
        CHECK_NEAR (measureFrequency (out, 0.6, 2.4), 880.0, 6.0);

        // ... auch in der schnellen Variante und nach unten
        p.quality = PitchQuality::Fast;
        p.semitones = -5.0;
        out = changePitch (in, p);
        CHECK (out.numFrames() == in.numFrames());
        CHECK_NEAR (measureFrequency (out, 0.6, 2.4), 440.0 * pitchRatio (-5.0), 5.0);

        // Cents (Bruchteile von Halbtoenen)
        p.quality = PitchQuality::High;
        p.semitones = 0.5;
        out = changePitch (in, p);
        CHECK_NEAR (measureFrequency (out, 0.6, 2.4), 440.0 * pitchRatio (0.5), 4.0);

        // Tonhoehe und Tempo gemeinsam: Dauer halbiert sich
        p.semitones = 12.0;
        p.mode = PitchMode::ChangeTempoWithPitch;
        out = changePitch (in, p);
        CHECK (std::llabs ((long long) out.numFrames() - (long long) (in.numFrames() / 2)) <= 1);
        CHECK_NEAR (measureFrequency (out, 0.2, 1.3), 880.0, 3.0);

        // Keine Aenderung -> identisch
        p.semitones = 0.0;
        out = changePitch (in, p);
        CHECK (out.numFrames() == in.numFrames() && out.channels[0][1000] == in.channels[0][1000]);
    }

    void testBpm()
    {
        for (double bpm : { 75.0, 90.0, 100.0, 120.0, 128.0, 140.0, 170.0 })
        {
            const AudioData drums = makeDrumPattern (bpm, 40.0);
            const BpmResult r = detectBpm (drums);
            std::printf ("    BPM-Test: erwartet %.0f, erkannt %.2f (Konfidenz %.2f, %d Einsaetze)\n",
                         bpm, r.bpm, r.confidence, r.onsetCount);
            CHECK (r.ok);

            // Halbierung und Verdopplung sind bei Tempo-Erkennung bekannte Abweichungen,
            // das Tempo selbst muss aber auf unter 1 BPM genau stimmen.
            const bool exact = std::fabs (r.bpm - bpm) < 1.0;
            const bool half = std::fabs (r.bpm * 2.0 - bpm) < 1.5;
            const bool twice = std::fabs (r.bpm - bpm * 2.0) < 2.0;
            CHECK (exact || half || twice);
            if (bpm <= 150.0)
                CHECK (exact);  // im ueblichen Bereich muss auch die richtige Oktave getroffen werden
        }

        // Anderes Format: Mono, 48 kHz, gekuerzt durch Auswahl eines Abschnitts
        AudioData drums48 = makeDrumPattern (110.0, 30.0, 48000);
        drums48 = adaptChannels (drums48, 1);
        const BpmResult r48 = detectBpm (drums48, 48000 * 4, 48000 * 20);
        CHECK (r48.ok);
        CHECK_NEAR (r48.bpm, 110.0, 1.0);

        // Zu kurz oder still
        CHECK (! detectBpm (makeSine (2, 2.0, 440.0, 0.5)).ok);
        CHECK (! detectBpm (AudioData (2, 44100 * 10, 44100)).ok);
        CHECK (! detectBpm (makeSine (2, 20.0, 440.0, 0.5)).ok);  // Dauerton ohne Einsaetze
    }

    void testMp3()
    {
        if (! mp3EncoderAvailable())
        {
            std::printf ("    (MP3-Encoder nicht verfuegbar, Test uebersprungen)\n");
            return;
        }

        const AudioData in = makeSine (2, 3.0, 440.0, 0.5);
        Mp3Settings settings;
        settings.bitrateKbps = 192;
        std::string error;
        auto bytes = encodeMp3 (in, 0, kWholeFile, settings, &error);
        CHECK (! bytes.empty());

        if (! bytes.empty())
        {
            // 3 s bei 192 kbit/s = ca. 72 kB
            CHECK (bytes.size() > 60000 && bytes.size() < 85000);
            CHECK (bytes[0] == 0xFF && (bytes[1] & 0xE0) == 0xE0);  // MPEG-Frame-Synchronisation

            // Der Xing/Info-Header darf nicht fehlen und muss im ersten Frame stehen
            const std::string head (bytes.begin(), bytes.begin() + 200);
            CHECK (head.find ("Info") != std::string::npos || head.find ("Xing") != std::string::npos);

            // Zweiter Frame beginnt wieder mit einer Synchronisation (kein beschaedigter Audio-Frame)
            // CBR 192 kbit/s, 44,1 kHz: Frame-Laenge 626 oder 627 Bytes
            bool secondFrame = false;
            for (size_t off : { (size_t) 626, (size_t) 627 })
                if (bytes.size() > off + 2 && bytes[off] == 0xFF && (bytes[off + 1] & 0xE0) == 0xE0)
                    secondFrame = true;
            CHECK (secondFrame);
        }

        // Mono, VBR und Teilbereich
        const AudioData mono = makeSine (1, 2.0, 440.0, 0.5);
        settings.vbr = true;
        settings.vbrQuality = 4;
        bytes = encodeMp3 (mono, 22050, 44100, settings, &error);
        CHECK (! bytes.empty() && bytes.size() < 40000);

        // Ungueltige Eingabe
        CHECK (encodeMp3 (AudioData(), 0, kWholeFile, Mp3Settings(), &error).empty());
    }

    void testDither()
    {
        AudioData a (1, 100000, 44100);
        addTpdfDither (a, 16);
        const float lsb = 1.0f / 32768.0f;
        float maxAbs = 0.0f;
        double sum = 0.0;
        for (float s : a.channels[0])
        {
            maxAbs = std::max (maxAbs, std::fabs (s));
            sum += s;
        }
        CHECK (maxAbs <= lsb * 1.001f);
        CHECK (maxAbs > lsb * 0.9f);
        CHECK (std::fabs (sum / 100000.0) < lsb * 0.02);

        AudioData untouched (1, 10, 44100);
        addTpdfDither (untouched, 1);
        CHECK (untouched.channels[0][3] == 0.0f);
    }

    struct TestCase
    {
        const char* name;
        std::function<void()> run;
    };
}

int main()
{
    const std::vector<TestCase> tests = {
        { "Bearbeitung (Schneiden/Einfuegen)", testAudioDataEditing },
        { "Fades", testFades },
        { "Gain und Peak-Normalisierung", testGainAndPeakNormalize },
        { "Resampling und Varispeed", testResampling },
        { "Lautheitsmessung (LUFS)", testLoudnessMeasurement },
        { "Loudness-Normalisierung und Limiter", testLoudnessNormalizeAndLimiter },
        { "Pitching", testPitch },
        { "BPM-Erkennung", testBpm },
        { "MP3-Export", testMp3 },
        { "Dither", testDither },
    };

    for (const auto& t : tests)
    {
        g_currentTest = t.name;
        const int before = g_failures;
        std::printf ("[ LAEUFT ] %s\n", t.name);
        t.run();
        std::printf ("[ %s ] %s\n", g_failures == before ? "  OK   " : "FEHLER ", t.name);
    }

    std::printf ("\n%d Pruefungen, %d Fehler\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
