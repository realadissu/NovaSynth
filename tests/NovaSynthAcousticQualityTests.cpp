#include <gtest/gtest.h>
#include "NovaSynthProcessor.h"
#include "nova_dsp/filters/StateVariableFilter.h"
#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>

namespace {

struct BandpassFilter {
    nova::dsp::StateVariableFilter hp;
    nova::dsp::StateVariableFilter lp;

    void prepare(double sr, float lowHz, float highHz) noexcept {
        hp.prepare(sr);
        lp.prepare(sr);
        hp.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, lowHz, 0.7071f);
        lp.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, highHz, 0.7071f);
    }

    void reset() noexcept {
        hp.reset();
        lp.reset();
    }

    [[nodiscard]] float processSample(float in) noexcept {
        return lp.processSample(hp.processSample(in));
    }
};

struct LowpassFilter4thOrder {
    nova::dsp::StateVariableFilter lp1;
    nova::dsp::StateVariableFilter lp2;

    void prepare(double sr, float cutHz) noexcept {
        lp1.prepare(sr);
        lp2.prepare(sr);
        lp1.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, cutHz, 0.7071f);
        lp2.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, cutHz, 0.7071f);
    }

    void reset() noexcept {
        lp1.reset();
        lp2.reset();
    }

    [[nodiscard]] float processSample(float in) noexcept {
        return lp2.processSample(lp1.processSample(in));
    }
};

struct SpectralMeasurements {
    float rmsFundament = 0.0f;    // 80–250 Hz
    float rmsHonk = 0.0f;         // 500–1300 Hz
    float rmsSibilance = 0.0f;    // 2500–5000 Hz
    float rmsTotal = 0.0f;
    float peakValue = 0.0f;
    double energyBelow450 = 0.0;
    double energyTotal = 0.0;
    float honkVsFundDb = 0.0f;
    float lowEnergyRatio = 0.0f;
};

SpectralMeasurements analyzeChordSteadyState(nova::synth::NovaSynthProcessor& processor,
                                            float swellAmount,
                                            double sampleRate = 48000.0,
                                            int blockSize = 512) {
    // Set swell via both modWheel (CC 1) and processor UI method
    processor.setModWheelFromUI(swellAmount);

    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;

    // Send MIDI CC 1 (ModWheel / Swell)
    const int ccVal = std::clamp(static_cast<int>(std::round(swellAmount * 127.0f)), 0, 127);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, ccVal), 0);

    // 5-voice worship chord: C2 (36), G2 (43), D3 (50), E3 (52), G3 (55)
    const int chordNotes[5] = { 36, 43, 50, 52, 55 };
    for (int note : chordNotes) {
        midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(100)), 0);
    }

    // Filters for analysis
    BandpassFilter bandFundament;
    bandFundament.prepare(sampleRate, 80.0f, 250.0f);

    BandpassFilter bandHonk;
    bandHonk.prepare(sampleRate, 500.0f, 1300.0f);

    BandpassFilter bandSibilance;
    bandSibilance.prepare(sampleRate, 2500.0f, 5000.0f);

    LowpassFilter4thOrder lp450;
    lp450.prepare(sampleRate, 450.0f);

    // Warm-up / Attack phase: 1.0 second (~94 blocks at 48k/512) for pad attack to stabilize
    for (int b = 0; b < 94; ++b) {
        processor.processBlock(buffer, midi);
        midi.clear(); // only note-on in first block
    }

    // Steady-state measurement phase: 1.0 second (~94 blocks)
    double sumSqFund = 0.0, sumSqHonk = 0.0, sumSqSib = 0.0;
    double sumSqTotal = 0.0, sumSqBelow450 = 0.0;
    float maxPeak = 0.0f;
    std::size_t sampleCount = 0;

    for (int b = 0; b < 94; ++b) {
        processor.processBlock(buffer, midi);
        const float* left = buffer.getReadPointer(0);
        const float* right = buffer.getReadPointer(1);

        for (int i = 0; i < blockSize; ++i) {
            // Mono mix of left & right
            const float s = 0.5f * (left[i] + right[i]);
            maxPeak = std::max(maxPeak, std::abs(s));
            sumSqTotal += static_cast<double>(s) * static_cast<double>(s);

            const float f = bandFundament.processSample(s);
            sumSqFund += static_cast<double>(f) * static_cast<double>(f);

            const float h = bandHonk.processSample(s);
            sumSqHonk += static_cast<double>(h) * static_cast<double>(h);

            const float sib = bandSibilance.processSample(s);
            sumSqSib += static_cast<double>(sib) * static_cast<double>(sib);

            const float l = lp450.processSample(s);
            sumSqBelow450 += static_cast<double>(l) * static_cast<double>(l);
            sampleCount++;
        }
    }

    // Release chord notes
    for (int note : chordNotes) {
        midi.addEvent(juce::MidiMessage::noteOff(1, note), 0);
    }
    for (int b = 0; b < 30; ++b) {
        processor.processBlock(buffer, midi);
        midi.clear();
    }

    SpectralMeasurements result;
    result.peakValue = maxPeak;
    result.energyTotal = sumSqTotal;
    result.energyBelow450 = sumSqBelow450;
    result.rmsTotal = static_cast<float>(std::sqrt(sumSqTotal / std::max<std::size_t>(1, sampleCount)));
    result.rmsFundament = static_cast<float>(std::sqrt(sumSqFund / std::max<std::size_t>(1, sampleCount)));
    result.rmsHonk = static_cast<float>(std::sqrt(sumSqHonk / std::max<std::size_t>(1, sampleCount)));
    result.rmsSibilance = static_cast<float>(std::sqrt(sumSqSib / std::max<std::size_t>(1, sampleCount)));
    result.lowEnergyRatio = static_cast<float>(sumSqBelow450 / (sumSqTotal + 1e-12));
    result.honkVsFundDb = 20.0f * std::log10((result.rmsHonk + 1e-9f) / (result.rmsFundament + 1e-9f));

    return result;
}

} // namespace

TEST(NovaSynthAcousticQualityTests, SundayPianoFoundationSpectralQuality) {
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    nova::synth::NovaSynthProcessor processor;
    processor.prepareToPlay(kSampleRate, kBlockSize);

    auto& pm = processor.getPresetManager();
    // 1. Verify Sunday Piano Foundation is registered and loaded as Preset 0
    ASSERT_TRUE(pm.loadPresetByName("Sunday Piano Foundation"))
        << "Preset 'Sunday Piano Foundation' must exist!";
    EXPECT_EQ(pm.getCurrentPresetIndex(), 0)
        << "Sunday Piano Foundation should be the very first factory pad preset (index 0)!";

    // --- Requirement 1: 20% Swell (zurueckhaltendes, warmes Fundament) ---
    const auto m20 = analyzeChordSteadyState(processor, 0.20f, kSampleRate, kBlockSize);
    std::cout << "[INFO] 20% Swell: lowEnergyRatio=" << (m20.lowEnergyRatio * 100.0f)
              << "%, rmsFund=" << m20.rmsFundament << ", rmsHonk=" << m20.rmsHonk
              << ", rmsSib=" << m20.rmsSibilance << ", honkVsFundDb=" << m20.honkVsFundDb << " dB\n";
    EXPECT_GT(m20.lowEnergyRatio, 0.80f)
        << "At 20% swell, > 80% of energy must lie below 450 Hz! Actual: "
        << (m20.lowEnergyRatio * 100.0f) << "%";
    EXPECT_GT(m20.rmsFundament, 0.01f);
    EXPECT_LT(m20.peakValue, 3.0f);

    // --- Requirement 2: 60% Swell (mittlerer Worship-Teppich) ---
    const auto m60 = analyzeChordSteadyState(processor, 0.60f, kSampleRate, kBlockSize);
    std::cout << "[INFO] 60% Swell: lowEnergyRatio=" << (m60.lowEnergyRatio * 100.0f)
              << "%, rmsFund=" << m60.rmsFundament << ", rmsHonk=" << m60.rmsHonk
              << ", rmsSib=" << m60.rmsSibilance << ", honkVsFundDb=" << m60.honkVsFundDb << " dB\n";
    EXPECT_GT(m60.rmsFundament, 0.01f);
    EXPECT_LE(m60.honkVsFundDb, -4.0f);

    // --- Requirement 3: 100% Swell (voller Teppich, Honk-Zone mind. 5 dB unter Fundament) ---
    const auto m100 = analyzeChordSteadyState(processor, 1.00f, kSampleRate, kBlockSize);
    std::cout << "[INFO] 100% Swell: lowEnergyRatio=" << (m100.lowEnergyRatio * 100.0f)
              << "%, rmsFund=" << m100.rmsFundament << ", rmsHonk=" << m100.rmsHonk
              << ", rmsSib=" << m100.rmsSibilance << ", honkVsFundDb=" << m100.honkVsFundDb << " dB\n";
    EXPECT_LE(m100.honkVsFundDb, -5.0f)
        << "At 100% swell, 500-1300 Hz honk zone must be at least 5 dB below 80-250 Hz fundament! Actual: "
        << m100.honkVsFundDb << " dB";
    EXPECT_LT(m100.peakValue, 3.5f)
        << "100% swell must remain bounded and distortion-free!";

    // --- Requirement 4: Kein schmalbandiger Resonanz-Peak / kein Heulen ueber den gesamten Swell ---
    const float swellSteps[] = { 0.0f, 0.15f, 0.30f, 0.45f, 0.60f, 0.75f, 0.90f, 1.0f };
    float prevPeak = 0.0f;
    for (float swell : swellSteps) {
        const auto m = analyzeChordSteadyState(processor, swell, kSampleRate, kBlockSize);
        ASSERT_TRUE(std::isfinite(m.rmsTotal));
        ASSERT_TRUE(std::isfinite(m.peakValue));
        EXPECT_LT(m.peakValue, 3.5f)
            << "Peak exploded at swell " << swell << " (howling/unstable filter)";
        // Crest factor (peak / RMS) must remain bounded (< 5.0 for dense 5-note worship pad)
        const float crestFactor = m.peakValue / (m.rmsTotal + 1e-9f);
        EXPECT_LT(crestFactor, 5.5f)
            << "Excessive peak-to-RMS ratio indicates whistling/howling resonance peak at swell " << swell;

        // Peak must not jump abruptly (indicative of narrow resonance spikes passing harmonics)
        if (prevPeak > 0.05f) {
            EXPECT_LT(m.peakValue / prevPeak, 2.8f)
                << "Sudden resonance peak jump between swell steps!";
        }
        prevPeak = m.peakValue;
    }
}

TEST(NovaSynthAcousticQualityTests, HostileSampleRatesAndOddBufferSizes) {
    const double testSampleRates[] = { 22050.0, 44100.0, 96000.0, 192000.0 };
    const int testBufferSizes[] = { 13, 64, 512, 2048 };

    for (double sr : testSampleRates) {
        for (int bufSize : testBufferSizes) {
            nova::synth::NovaSynthProcessor processor;
            processor.prepareToPlay(sr, bufSize);

            auto& pm = processor.getPresetManager();
            ASSERT_TRUE(pm.loadPresetByName("Sunday Piano Foundation"));

            juce::AudioBuffer<float> buffer(2, bufSize);
            juce::MidiBuffer midi;

            // Trigger chord
            processor.triggerAuditionNoteOn(36, 0.85f);
            processor.triggerAuditionNoteOn(43, 0.85f);
            processor.triggerAuditionNoteOn(50, 0.85f);

            float peak = 0.0f;
            double sumSq = 0.0;
            const int blocksToRun = std::max(20, static_cast<int>((sr * 0.45) / bufSize));

            for (int b = 0; b < blocksToRun; ++b) {
                processor.processBlock(buffer, midi);
                midi.clear();
                for (int ch = 0; ch < 2; ++ch) {
                    const float* r = buffer.getReadPointer(ch);
                    for (int i = 0; i < bufSize; ++i) {
                        ASSERT_TRUE(std::isfinite(r[i])) << "Non-finite sample at sr=" << sr << ", buf=" << bufSize;
                        peak = std::max(peak, std::abs(r[i]));
                        sumSq += static_cast<double>(r[i]) * static_cast<double>(r[i]);
                    }
                }
            }

            EXPECT_LT(peak, 4.0f) << "Peak overflow at sr=" << sr << ", buf=" << bufSize;
            EXPECT_GT(sumSq, 1e-4) << "Silent output at sr=" << sr << ", buf=" << bufSize;

            // Test Panic decay to silence
            processor.triggerPanic();
            const int panicBlocks = std::max(40, static_cast<int>((sr * 0.1) / bufSize));
            for (int b = 0; b < panicBlocks; ++b) {
                processor.processBlock(buffer, midi);
            }
            float postPanicPeak = 0.0f;
            for (int ch = 0; ch < 2; ++ch) {
                const float* r = buffer.getReadPointer(ch);
                for (int i = 0; i < bufSize; ++i) {
                    postPanicPeak = std::max(postPanicPeak, std::abs(r[i]));
                }
            }
            EXPECT_LT(postPanicPeak, 1e-3f) << "Audio failed to decay after panic at sr=" << sr;
        }
    }
}

TEST(NovaSynthAcousticQualityTests, StateSerializationPreservesPreset) {
    nova::synth::NovaSynthProcessor proc1;
    proc1.prepareToPlay(48000.0, 512);
    auto& pm1 = proc1.getPresetManager();
    ASSERT_TRUE(pm1.loadPresetByName("Sunday Piano Foundation"));

    juce::MemoryBlock stateData;
    proc1.getStateInformation(stateData);
    ASSERT_GT(stateData.getSize(), 0u);

    nova::synth::NovaSynthProcessor proc2;
    proc2.prepareToPlay(48000.0, 512);
    proc2.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));

    EXPECT_FLOAT_EQ(*proc2.getAPVTS().getRawParameterValue("filterResonance"), 0.30f);
    EXPECT_FLOAT_EQ(*proc2.getAPVTS().getRawParameterValue("vocalCarve"), 0.65f);
}
