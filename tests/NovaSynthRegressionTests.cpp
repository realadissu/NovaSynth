#include <gtest/gtest.h>
#include "NovaSynthProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <vector>
#include <functional>
#include <algorithm>

namespace {

void assertBufferAcousticInvariants(const juce::AudioBuffer<float>& buffer, float maxPeakLimit = 4.0f) {
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        const float* readPtr = buffer.getReadPointer(ch);
        for (int s = 0; s < buffer.getNumSamples(); ++s) {
            const float sample = readPtr[s];
            ASSERT_FALSE(std::isnan(sample)) << "NaN detected at ch=" << ch << ", sample=" << s;
            ASSERT_FALSE(std::isinf(sample)) << "Inf detected at ch=" << ch << ", sample=" << s;
            ASSERT_LE(std::abs(sample), maxPeakLimit) << "Sample blew up past limit " << maxPeakLimit
                                                      << " at ch=" << ch << ", sample=" << s;
        }
    }
}

} // namespace

// ==============================================================================
// 1. Preset Torture & Acoustic Invariants Test
// ==============================================================================
TEST(NovaSynthRegressionTests, PresetTortureAndAcousticInvariants) {
    nova::synth::NovaSynthProcessor processor;
    const double sampleRate = 48000.0;
    const int blockSize = 256;

    auto& presetMgr = processor.getPresetManager();
    const auto& presets = presetMgr.getAllPresets();
    ASSERT_GE(presets.size(), 15u);

    for (int pIdx = 0; pIdx < 15; ++pIdx) {
        presetMgr.loadPresetByIndex(pIdx);
        processor.prepareToPlay(sampleRate, blockSize);

        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;

        // Play chord (MIDI 60, 64, 67, velocity 100) on block 0
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 64, static_cast<juce::uint8>(100)), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 67, static_cast<juce::uint8>(100)), 0);

        float maxPeak = 0.0f;
        float maxRms = 0.0f;

        for (int b = 0; b < 10; ++b) {
            buffer.clear();
            processor.processBlock(buffer, midi);
            midi.clear();

            assertBufferAcousticInvariants(buffer, 4.0f);

            const float bPeak = std::max(buffer.getMagnitude(0, 0, blockSize),
                                         buffer.getMagnitude(1, 0, blockSize));
            const float bRms = 0.5f * (buffer.getRMSLevel(0, 0, blockSize) +
                                       buffer.getRMSLevel(1, 0, blockSize));
            maxPeak = std::max(maxPeak, bPeak);
            maxRms = std::max(maxRms, bRms);
        }

        EXPECT_LT(maxPeak, 4.0f) << "Preset '" << presets[static_cast<std::size_t>(pIdx)].name
                                 << "' exceeded peak limit of 4.0f";
        EXPECT_GT(maxRms, 0.0001f) << "Preset '" << presets[static_cast<std::size_t>(pIdx)].name
                                   << "' produced silent RMS (< 0.0001f)";

        // Panic test: call triggerPanic and verify audio decay
        processor.triggerPanic();
        float postPanicRms = 0.0f;
        for (int b = 0; b < 6; ++b) {
            buffer.clear();
            midi.clear();
            processor.processBlock(buffer, midi);
            assertBufferAcousticInvariants(buffer, 4.0f);
            postPanicRms = 0.5f * (buffer.getRMSLevel(0, 0, blockSize) +
                                  buffer.getRMSLevel(1, 0, blockSize));
        }

        EXPECT_LT(postPanicRms, maxRms) << "Preset '" << presets[static_cast<std::size_t>(pIdx)].name
                                        << "' level did not decay after triggerPanic()";
    }
}

// ==============================================================================
// 2. State Serialization Roundtrip Test
// ==============================================================================
TEST(NovaSynthRegressionTests, StateSerializationRoundtrip) {
    nova::synth::NovaSynthProcessor proc1;
    auto& apvts1 = proc1.getAPVTS();

    // 10 distinct continuous/discrete APVTS parameters modified to non-defaults
    const struct ParamTarget {
        const char* id;
        float value;
    } modifiedParams[] = {
        { "macroWarmth",       0.82f },
        { "macroShimmer",      0.74f },
        { "macroAir",          0.66f },
        { "macroMotion",       0.58f },
        { "filterCutoffHz",  1750.0f },
        { "filterResonance",   0.88f },
        { "ampAttack",         0.42f },
        { "ampRelease",        2.35f },
        { "delayMix",          0.38f },
        { "reverbMix",         0.54f }
    };

    for (const auto& pt : modifiedParams) {
        auto* p = apvts1.getParameter(pt.id);
        ASSERT_NE(p, nullptr) << "Missing parameter: " << pt.id;
        p->setValueNotifyingHost(p->convertTo0to1(pt.value));
    }

    // Set non-default Arp gates, velocities, and length
    proc1.setArpPatternLength(32);
    proc1.setArpStepGate(2, false);
    proc1.setArpStepGate(5, false);
    proc1.setArpStepGate(7, false);
    proc1.setArpStepVelocity(0, 0.44f);
    proc1.setArpStepVelocity(3, 0.92f);
    proc1.setArpStepVelocity(6, 0.65f);

    // Serialize state
    juce::MemoryBlock stateBlock;
    proc1.getStateInformation(stateBlock);
    ASSERT_GT(stateBlock.getSize(), 0u);

    // Deserialize into fresh instance
    nova::synth::NovaSynthProcessor proc2;
    proc2.setStateInformation(stateBlock.getData(), static_cast<int>(stateBlock.getSize()));
    auto& apvts2 = proc2.getAPVTS();

    // Verify 10 APVTS parameters match exactly
    for (const auto& pt : modifiedParams) {
        auto* p1 = apvts1.getParameter(pt.id);
        auto* p2 = apvts2.getParameter(pt.id);
        ASSERT_NE(p2, nullptr) << "Restored parameter missing: " << pt.id;
        EXPECT_NEAR(p2->convertFrom0to1(p2->getValue()), pt.value, 0.05f)
            << "Parameter mismatch for: " << pt.id;
        EXPECT_NEAR(p1->getValue(), p2->getValue(), 1e-4f)
            << "Normalized mismatch for: " << pt.id;
    }

    // Verify Arp configuration restored exactly
    EXPECT_EQ(proc2.getArpPatternLength(), 32);
    EXPECT_FALSE(proc2.isArpStepGateActive(2));
    EXPECT_FALSE(proc2.isArpStepGateActive(5));
    EXPECT_FALSE(proc2.isArpStepGateActive(7));
    EXPECT_TRUE(proc2.isArpStepGateActive(0));
    EXPECT_TRUE(proc2.isArpStepGateActive(1));
    EXPECT_NEAR(proc2.getArpStepVelocity(0), 0.44f, 1e-3f);
    EXPECT_NEAR(proc2.getArpStepVelocity(3), 0.92f, 1e-3f);
    EXPECT_NEAR(proc2.getArpStepVelocity(6), 0.65f, 1e-3f);
}

// ==============================================================================
// 3. Extreme SampleRate & BufferSize Stress Test
// ==============================================================================
TEST(NovaSynthRegressionTests, ExtremeSampleRateAndBufferSizeStress) {
    nova::synth::NovaSynthProcessor processor;
    auto& apvts = processor.getAPVTS();

    // Extreme filter resonance
    if (auto* r1 = apvts.getParameter("filterResonance"))
        r1->setValueNotifyingHost(r1->convertTo0to1(1.0f));
    if (auto* r2 = apvts.getParameter("filter2Resonance"))
        r2->setValueNotifyingHost(r2->convertTo0to1(1.0f));

    const double sampleRates[] = { 44100.0, 96000.0 };
    const int bufferSizes[] = { 1, 17, 1024 };

    for (double sr : sampleRates) {
        for (int bSize : bufferSizes) {
            processor.prepareToPlay(sr, bSize);

            juce::AudioBuffer<float> buffer(2, bSize);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(127)), 0);

            // Process multiple blocks to verify stability across boundaries
            const int blocksToRun = (bSize == 1) ? 64 : ((bSize == 17) ? 20 : 6);
            for (int b = 0; b < blocksToRun; ++b) {
                buffer.clear();
                processor.processBlock(buffer, midi);
                midi.clear();

                assertBufferAcousticInvariants(buffer, 4.0f);
            }
        }
    }
}

// ==============================================================================
// 4. APVTS-to-DSP Wiring & Smoothing Test
// ==============================================================================
TEST(NovaSynthRegressionTests, ApvtsToDspWiringAndSmoothing) {
    nova::synth::NovaSynthProcessor processor;
    const double sampleRate = 48000.0;
    const int blockSize = 128;
    processor.prepareToPlay(sampleRate, blockSize);

    auto& apvts = processor.getAPVTS();

    struct ParameterWiringCheck {
        const char* paramId;
        std::function<float(const nova::dsp::WorshipSynthEngine::Parameters&)> getEngineValue;
    };

    const std::vector<ParameterWiringCheck> wiringChecks = {
        // Oscillator Mix / Detune
        { "osc1_level",       [](const auto& p) { return p.oscSlots[0].level; } },
        { "osc1_detune",      [](const auto& p) { return p.oscSlots[0].detune; } },
        { "osc2_level",       [](const auto& p) { return p.oscSlots[1].level; } },
        { "osc2_detune",      [](const auto& p) { return p.oscSlots[1].detune; } },

        // Filter Cutoff / Resonance
        { "filterCutoffHz",   [](const auto& p) { return p.filterCutoffHz; } },
        { "filterResonance",  [](const auto& p) { return p.filterResonance; } },
        { "filter2CutoffHz",  [](const auto& p) { return p.filter2CutoffHz; } },
        { "filter2Resonance", [](const auto& p) { return p.filter2Resonance; } },

        // Envelope Attack / Release
        { "ampAttack",        [](const auto& p) { return p.ampEnv.attackSec; } },
        { "ampRelease",       [](const auto& p) { return p.ampEnv.releaseSec; } },
        { "fltAttack",        [](const auto& p) { return p.filterEnv.attackSec; } },
        { "fltRelease",       [](const auto& p) { return p.filterEnv.releaseSec; } },

        // Reverb Mix / Decay
        { "reverbMix",        [](const auto& p) { return p.reverbMix; } },
        { "reverbDecay",      [](const auto& p) { return p.reverbDecay; } },

        // Worship Macros
        { "macroWarmth",      [](const auto& p) { return p.macroWarmth; } },
        { "macroMotion",      [](const auto& p) { return p.macroMotion; } },
        { "macroShimmer",     [](const auto& p) { return p.macroShimmer; } },
        { "macroAir",         [](const auto& p) { return p.macroAir; } }
    };

    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;

    // Start playback with a note so audio thread DSP and smoothers are active
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    midi.clear();
    buffer.clear();
    processor.processBlock(buffer, midi);

    for (const auto& item : wiringChecks) {
        auto* param = apvts.getParameter(item.paramId);
        ASSERT_NE(param, nullptr) << "Missing parameter: " << item.paramId;

        // Step 1: Set APVTS parameter to 0.0f
        param->setValueNotifyingHost(0.0f);
        processor.syncParametersFromAPVTS();
        buffer.clear();
        processor.processBlock(buffer, midi);

        const float valAtZero = item.getEngineValue(processor.getEngine().getParameters());
        assertBufferAcousticInvariants(buffer, 4.0f);

        // Step 2: Set APVTS parameter to 1.0f
        param->setValueNotifyingHost(1.0f);
        processor.syncParametersFromAPVTS();
        buffer.clear();
        processor.processBlock(buffer, midi);

        const float valAtOne = item.getEngineValue(processor.getEngine().getParameters());
        assertBufferAcousticInvariants(buffer, 4.0f);

        // Verification: Parameter actually changed in engine
        EXPECT_NE(valAtZero, valAtOne)
            << "Parameter " << item.paramId << " did not arrive in internal DSP engine!";
        EXPECT_GT(std::abs(valAtOne - valAtZero), 0.001f)
            << "Parameter difference too small for " << item.paramId;

        // Verification: Smoothing & discontinuity check (no harsh sample-to-sample clicks)
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            const float* readPtr = buffer.getReadPointer(ch);
            for (int s = 1; s < buffer.getNumSamples(); ++s) {
                const float delta = std::abs(readPtr[s] - readPtr[s - 1]);
                EXPECT_LT(delta, 2.5f)
                    << "Discontinuity / click detected on " << item.paramId
                    << " at ch=" << ch << ", sample=" << s << " (delta=" << delta << ")";
            }
        }

        // Restore default
        param->setValueNotifyingHost(param->getDefaultValue());
        processor.syncParametersFromAPVTS();
    }
}
