#include <gtest/gtest.h>
#include "NovaSynthProcessor.h"
#include <algorithm>
#include <cmath>
#include <tuple>
#include <vector>

TEST(NovaSynthTuningAndSampleTests, SwellPedalPresetsAndSampleAuditVerification) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);

    auto& pm = proc.getPresetManager();
    EXPECT_GE(pm.getAllPresets().size(), 9u);

    // 1. Verify acoustic sample loop modes sync cleanly through APVTS (One-Shot vs Ping-Pong + crossfade)
    ASSERT_TRUE(pm.loadPresetByName("Init Patch"));
    if (auto* pSt = proc.getAPVTS().getParameter("osc1_sourceType")) {
        pSt->setValueNotifyingHost(pSt->convertTo0to1(4.0f));
    }
    if (auto* pLm = proc.getAPVTS().getParameter("osc1_loopMode")) {
        pLm->setValueNotifyingHost(pLm->convertTo0to1(0.0f));
    }
    if (auto* pSc = proc.getAPVTS().getParameter("osc1_scatter")) {
        pSc->setValueNotifyingHost(pSc->convertTo0to1(0.0f));
    }
    proc.syncParametersFromAPVTS();
    EXPECT_EQ(proc.getCurrentParameters().oscSlots[0].sampleLoopMode, 0);
    EXPECT_NEAR(proc.getCurrentParameters().oscSlots[0].scatter, 0.0f, 1e-4f);

    if (auto* pLm = proc.getAPVTS().getParameter("osc1_loopMode")) {
        pLm->setValueNotifyingHost(pLm->convertTo0to1(2.0f));
    }
    if (auto* pXf = proc.getAPVTS().getParameter("osc1_loopXfade")) {
        pXf->setValueNotifyingHost(pXf->convertTo0to1(0.15f));
    }
    proc.syncParametersFromAPVTS();
    EXPECT_EQ(proc.getCurrentParameters().oscSlots[0].sampleLoopMode, 2);
    EXPECT_GT(proc.getCurrentParameters().oscSlots[0].sampleCrossfade, 0.10f);

    // 2. Collect all 8 factory pad presets dynamically and verify swell pedal response (CC 11)
    std::vector<juce::String> padPresets;
    for (const auto& preset : pm.getAllPresets()) {
        if (preset.isFactory && preset.category != "Utility") {
            padPresets.push_back(preset.name);
        }
    }
    ASSERT_EQ(padPresets.size(), 8u);

    for (const auto& name : padPresets) {
        ASSERT_TRUE(pm.loadPresetByName(name)) << "Missing preset: " << name.toStdString();
        proc.syncParametersFromAPVTS();
        const auto p = proc.getCurrentParameters();
        EXPECT_GE(p.ampEnv.releaseSec, 2.2f) << "Release out of range on " << name.toStdString();
        EXPECT_LE(p.ampEnv.releaseSec, 4.0f) << "Release out of range on " << name.toStdString();
        EXPECT_LE(p.reverbDucking, 0.15f) << "Reverb ducking too high for pad carpet on " << name.toStdString();
    }

    // 3. Measure low-swell vs high-swell via MIDI CC 11 (Expression Pedal) on Intimate Tape Choir & Soft Velvet Cushion
    juce::AudioBuffer<float> buf(2, 512);
    auto measureSwellRms = [&](int cc11Value) {
        proc.triggerPanic();
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 11, cc11Value), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 64, static_cast<juce::uint8>(100)), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 67, static_cast<juce::uint8>(100)), 0);
        float sumSq = 0.0f;
        int totalSamples = 0;
        for (int b = 0; b < 140; ++b) {
            proc.processBlock(buf, midi);
            midi.clear();
            if (b >= 90) {
                const float* l = buf.getReadPointer(0);
                const float* r = buf.getReadPointer(1);
                for (int i = 0; i < 512; ++i) {
                    sumSq += 0.5f * (l[i] * l[i] + r[i] * r[i]);
                }
                totalSamples += 512;
            }
        }
        return std::sqrt(sumSq / static_cast<float>(std::max(1, totalSamples)));
    };

    for (const auto& swellPresetName : { juce::String("Intimate Tape Choir"), juce::String("Soft Velvet Cushion") }) {
        ASSERT_TRUE(pm.loadPresetByName(swellPresetName));
        const float rmsLowPedal  = measureSwellRms(0);
        const float rmsHighPedal = measureSwellRms(127);

        EXPECT_GT(rmsLowPedal, 0.003f) << swellPresetName.toStdString();
        EXPECT_GT(rmsHighPedal, rmsLowPedal * 2.5f) << swellPresetName.toStdString();
        EXPECT_LT(rmsHighPedal, 0.85f) << swellPresetName.toStdString();
    }

    // 4. Verify all 8 pad presets have distinct oscillator/filter/chorus signatures
    std::vector<std::tuple<int, int, int, int, float, int, int>> signatures;
    for (const auto& name : padPresets) {
        ASSERT_TRUE(pm.loadPresetByName(name));
        proc.syncParametersFromAPVTS();
        const auto p = proc.getCurrentParameters();
        signatures.emplace_back(
            static_cast<int>(p.oscSlots[0].engine) * 10 + p.oscSlots[0].modelIndex,
            static_cast<int>(p.oscSlots[1].engine) * 10 + p.oscSlots[1].modelIndex,
            static_cast<int>(p.oscSlots[2].engine) * 10 + p.oscSlots[2].modelIndex,
            static_cast<int>(p.oscSlots[3].engine) * 10 + p.oscSlots[3].modelIndex,
            std::round(p.filterCutoffHz),
            p.filter2Type,
            p.chorusMode
        );
    }
    for (std::size_t i = 0; i < signatures.size(); ++i) {
        for (std::size_t j = i + 1; j < signatures.size(); ++j) {
            EXPECT_NE(signatures[i], signatures[j])
                << "Pad presets " << padPresets[i].toStdString()
                << " and " << padPresets[j].toStdString() << " have identical engine/filter/chorus signatures!";
        }
    }
}
