#include <gtest/gtest.h>
#include "NovaSynthProcessor.h"
#include "nova_dsp/filters/StateVariableFilter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

struct SwellMeasure {
    float rmsTotal = 0.0f;
    float rmsMid = 0.0f;
    float rmsSide = 0.0f;
    float rmsFund = 0.0f;   // 80 - 350 Hz
    float rmsHigh2k = 0.0f; // > 2000 Hz
    float peak = 0.0f;
    float preClipPeak = 0.0f;
    float maxStep = 0.0f;
};

[[nodiscard]] float computeRms(const std::vector<float>& buf) noexcept {
    double sq = 0.0;
    for (float s : buf) sq += static_cast<double>(s) * static_cast<double>(s);
    return static_cast<float>(std::sqrt(sq / static_cast<double>(std::max<std::size_t>(1, buf.size()))));
}

void setupCleanTestEngine(nova::dsp::WorshipSynthEngine& engine, double sr, int blockSize) {
    engine.prepare(sr, blockSize);
    auto p = engine.getParameters();
    p.activeOscCount = 1;
    p.oscSlots[0] = {};
    p.oscSlots[0].enabled = true;
    p.oscSlots[0].engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
    p.oscSlots[0].modelIndex = 0;
    p.oscSlots[0].detune = 0.0f;
    p.oscSlots[0].scatter = 0.0f;
    p.oscSlots[0].level = 0.85f;
    p.filter1Enabled = false; p.filter2Enabled = false;
    p.lfo1Depth = 0.0f; p.lfo2Depth = 0.0f; p.lfo3Depth = 0.0f; p.lfo4Depth = 0.0f;
    p.analogDrift = 0.0f; p.chorusEnabled = false; p.delayEnabled = false; p.reverbMix = 0.0f;
    p.ampEnv = { 0.002f, 0.2f, 1.0f, 0.2f };
    engine.setParameters(p);
    engine.getModMatrix().clear();
    engine.snapSmoothersToTarget();
}

SwellMeasure measurePresetSwell(nova::synth::NovaSynthProcessor& proc, int presetIdx, int cc11) {
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;
    proc.getPresetManager().loadPresetByIndex(presetIdx);
    proc.triggerPanic();
    const float wheel = static_cast<float>(cc11) / 127.0f;
    proc.setModWheelFromUI(wheel);

    juce::AudioBuffer<float> buf(2, kBlock);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 11, cc11), 0);
    for (int note : { 48, 55, 60, 64, 67 }) {
        midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(100)), 0);
    }

    for (int b = 0; b < 128; ++b) {
        buf.clear();
        proc.processBlock(buf, midi);
        midi.clear();
    }

    nova::dsp::StateVariableFilter hpFund, lpFund, hp2k;
    hpFund.prepare(kSr); lpFund.prepare(kSr); hp2k.prepare(kSr);
    hpFund.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, 80.0f, 0.7071f);
    lpFund.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, 350.0f, 0.7071f);
    hp2k.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, 2000.0f, 0.7071f);

    double sqTot = 0.0, sqMid = 0.0, sqSide = 0.0, sqFund = 0.0, sqHigh = 0.0;
    float peak = 0.0f, preClipPeak = 0.0f, maxStep = 0.0f, prevL = 0.0f;
    std::size_t count = 0;

    for (int b = 0; b < 64; ++b) {
        buf.clear();
        proc.processBlock(buf, midi);
        preClipPeak = std::max(preClipPeak, proc.getEngine().getTelemetry().preClipPeak);
        const float* l = buf.getReadPointer(0);
        const float* r = buf.getReadPointer(1);
        for (int i = 0; i < kBlock; ++i) {
            EXPECT_TRUE(std::isfinite(l[i]) && std::isfinite(r[i]));
            peak = std::max(peak, std::max(std::abs(l[i]), std::abs(r[i])));
            if (count > 0) maxStep = std::max(maxStep, std::abs(l[i] - prevL));
            prevL = l[i];
            const float mid = 0.5f * (l[i] + r[i]);
            const float side = 0.5f * (l[i] - r[i]);
            sqTot += 0.5 * (static_cast<double>(l[i]) * l[i] + static_cast<double>(r[i]) * r[i]);
            sqMid += static_cast<double>(mid) * mid;
            sqSide += static_cast<double>(side) * side;
            const float f = lpFund.processSample(hpFund.processSample(mid));
            const float h = hp2k.processSample(mid);
            sqFund += static_cast<double>(f) * f;
            sqHigh += static_cast<double>(h) * h;
            ++count;
        }
    }

    const double invN = 1.0 / static_cast<double>(std::max<std::size_t>(1, count));
    return {
        static_cast<float>(std::sqrt(sqTot * invN)),
        static_cast<float>(std::sqrt(sqMid * invN)),
        static_cast<float>(std::sqrt(sqSide * invN)),
        static_cast<float>(std::sqrt(sqFund * invN)),
        static_cast<float>(std::sqrt(sqHigh * invN)),
        peak, preClipPeak, maxStep
    };
}

} // namespace

TEST(NovaSynthSwellDynamicsTests, SwellGainPreFxAttenuationNeutralityAndReverbTailPreservation) {
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;
    nova::dsp::WorshipSynthEngine engine;
    setupCleanTestEngine(engine, kSr, kBlock);

    auto measureEngineRms = [&](float modWheel, bool jumpTest) {
        engine.reset();
        engine.setModWheel(modWheel);
        engine.snapSmoothersToTarget();
        engine.noteOn(60, 0.9f);
        std::vector<float> bL(kBlock), bR(kBlock), tail;
        float maxJumpStep = 0.0f, prev = 0.0f;
        for (int b = 0; b < 24; ++b) {
            if (jumpTest && b == 8) engine.setModWheel(1.0f);
            std::fill(bL.begin(), bL.end(), 0.0f);
            std::fill(bR.begin(), bR.end(), 0.0f);
            engine.processBlock(bL.data(), bR.data(), kBlock);
            for (int i = 0; i < kBlock; ++i) {
                if (b >= 8 && i > 0) maxJumpStep = std::max(maxJumpStep, std::abs(bL[static_cast<std::size_t>(i)] - prev));
                prev = bL[static_cast<std::size_t>(i)];
            }
            if (b >= 12) tail.insert(tail.end(), bL.begin(), bL.end());
        }
        return std::make_pair(computeRms(tail), maxJumpStep);
    };

    const float neutral0 = measureEngineRms(0.0f, false).first;
    const float neutral1 = measureEngineRms(1.0f, false).first;
    EXPECT_LT(std::abs(20.0f * std::log10((neutral1 + 1e-9f) / (neutral0 + 1e-9f))), 0.15f);

    engine.getModMatrix().setSlot(0, nova::dsp::ModMatrix::Source::ModWheel, nova::dsp::ModMatrix::Destination::SwellGain, 0.80f, true);
    const float swell0 = measureEngineRms(0.0f, false).first;
    const auto [swell1, jumpStep] = measureEngineRms(1.0f, false);
    const float jumpRampStep = measureEngineRms(0.0f, true).second;
    const float attenDb = 20.0f * std::log10((swell1 + 1e-9f) / (swell0 + 1e-9f));
    EXPECT_GE(attenDb, 10.0f);
    EXPECT_LE(attenDb, 18.0f);
    EXPECT_LT(jumpRampStep, std::max(0.25f, jumpStep * 1.35f));

    auto p = engine.getParameters();
    p.reverbDry = 0.0f; p.reverbMix = 0.85f; p.reverbDecay = 6.0f;
    p.reverbShimmer = 0.0f; p.reverbDucking = 0.0f;
    engine.setParameters(p);

    const float wetSteady0 = measureEngineRms(0.0f, false).first;
    engine.reset();
    engine.setModWheel(1.0f);
    engine.snapSmoothersToTarget();
    engine.noteOn(60, 0.9f);
    std::vector<float> bL(kBlock), bR(kBlock);
    for (int b = 0; b < 48; ++b) {
        std::fill(bL.begin(), bL.end(), 0.0f); std::fill(bR.begin(), bR.end(), 0.0f);
        engine.processBlock(bL.data(), bR.data(), kBlock);
    }
    engine.setModWheel(0.0f);
    std::vector<float> postPullback;
    for (int b = 0; b < 24; ++b) {
        std::fill(bL.begin(), bL.end(), 0.0f); std::fill(bR.begin(), bR.end(), 0.0f);
        engine.processBlock(bL.data(), bR.data(), kBlock);
        if (b >= 5 && b < 24) postPullback.insert(postPullback.end(), bL.begin(), bL.end());
    }
    EXPECT_GT(computeRms(postPullback), wetSteady0 * 3.0f);
}

TEST(NovaSynthSwellDynamicsTests, AirAmountModulationBoostsHighShelfAndReverbAir) {
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;
    nova::dsp::WorshipSynthEngine engine;
    setupCleanTestEngine(engine, kSr, kBlock);

    auto p = engine.getParameters();
    p.filter1Enabled = true; p.filterType = 3; p.filterCutoffHz = 6000.0f; p.filterResonance = 0.7071f;
    p.macroAir = 0.25f;
    engine.setParameters(p);
    engine.getModMatrix().setSlot(0, nova::dsp::ModMatrix::Source::ModWheel, nova::dsp::ModMatrix::Destination::AirAmount, 0.65f, true);

    auto measureBands = [&](float modWheel) {
        engine.reset(); engine.setModWheel(modWheel); engine.snapSmoothersToTarget(); engine.noteOn(60, 0.9f);
        nova::dsp::StateVariableFilter hpBody, lpBody, hpAir1, hpAir2, lpAir;
        hpBody.prepare(kSr); lpBody.prepare(kSr); hpAir1.prepare(kSr); hpAir2.prepare(kSr); lpAir.prepare(kSr);
        hpBody.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, 250.0f, 0.7071f);
        lpBody.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, 1000.0f, 0.7071f);
        hpAir1.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, 8000.0f, 0.7071f);
        hpAir2.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, 8000.0f, 0.7071f);
        lpAir.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, 14000.0f, 0.7071f);
        std::vector<float> bL(kBlock), bR(kBlock), bodyBuf, airBuf;
        for (int b = 0; b < 20; ++b) {
            std::fill(bL.begin(), bL.end(), 0.0f); std::fill(bR.begin(), bR.end(), 0.0f);
            engine.processBlock(bL.data(), bR.data(), kBlock);
            for (int i = 0; i < kBlock; ++i) {
                const float bd = lpBody.processSample(hpBody.processSample(bL[static_cast<std::size_t>(i)]));
                const float ar = lpAir.processSample(hpAir2.processSample(hpAir1.processSample(bL[static_cast<std::size_t>(i)])));
                if (b >= 8) { bodyBuf.push_back(bd); airBuf.push_back(ar); }
            }
        }
        return std::make_pair(computeRms(bodyBuf), computeRms(airBuf));
    };

    const auto [bodyLow, airLow] = measureBands(0.0f);
    const auto [bodyHi, airHi]   = measureBands(1.0f);
    EXPECT_GT(airHi, airLow * 1.45f);
    EXPECT_NEAR(bodyHi, bodyLow, bodyLow * 0.15f);

    p.filter1Enabled = false; p.reverbDry = 0.0f; p.reverbMix = 0.85f; p.reverbDecay = 4.0f;
    p.macroAir = 0.10f; p.macroWarmth = 0.50f;
    engine.setParameters(p);
    engine.getModMatrix().setSlot(0, nova::dsp::ModMatrix::Source::ModWheel, nova::dsp::ModMatrix::Destination::AirAmount, 0.85f, true);

    auto measureRevTailAir = [&](float modWheel, bool callSnap) {
        engine.reset();
        engine.setModWheel(modWheel);
        if (callSnap) engine.snapSmoothersToTarget();
        engine.noteOn(60, 0.95f);

        nova::dsp::StateVariableFilter hp1, hp2;
        hp1.prepare(kSr); hp2.prepare(kSr);
        hp1.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, 7000.0f, 0.7071f);
        hp2.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, 7000.0f, 0.7071f);
        std::vector<float> bL(kBlock), bR(kBlock), hi;
        for (int b = 0; b < 45; ++b) {
            if (b == 8) engine.noteOff(60);
            std::fill(bL.begin(), bL.end(), 0.0f); std::fill(bR.begin(), bR.end(), 0.0f);
            engine.processBlock(bL.data(), bR.data(), kBlock);
            for (int i = 0; i < kBlock; ++i) {
                const float y = hp2.processSample(hp1.processSample(bL[static_cast<std::size_t>(i)]));
                if (b >= 15) hi.push_back(y);
            }
        }
        return computeRms(hi);
    };

    const float revAirLow = measureRevTailAir(0.0f, true);
    const float revAirHi = measureRevTailAir(1.0f, true);
    const float revAirRestored = measureRevTailAir(0.0f, false);
    EXPECT_GT(revAirHi, revAirLow * 1.35f);
    EXPECT_LT(revAirRestored, revAirHi * 0.80f);
}

TEST(NovaSynthSwellDynamicsTests, AllEightPadsPreserveCleanHeadroomBelowSoftClipAtFullSwell) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    const auto& presets = proc.getPresetManager().getAllPresets();

    int padCount = 0;
    for (int idx = 0; idx < static_cast<int>(presets.size()); ++idx) {
        const auto& preset = presets[static_cast<std::size_t>(idx)];
        if (!preset.isFactory || preset.category == "Utility") continue;
        ++padCount;
        const auto m = measurePresetSwell(proc, idx, 127);
        EXPECT_LT(m.peak, 1.5f) << preset.name.toStdString();
        EXPECT_LE(m.preClipPeak, 0.78f) << preset.name.toStdString();
        EXPECT_GT(m.rmsTotal, 0.03f) << preset.name.toStdString();
        EXPECT_LT(m.maxStep, 0.25f) << preset.name.toStdString();
    }
    EXPECT_EQ(padCount, 8);
}

TEST(NovaSynthSwellDynamicsTests, AllEightPadsSwellVolumeWidthAndBrightnessFromZeroToFullPedal) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    const auto& presets = proc.getPresetManager().getAllPresets();

    int padCount = 0;
    for (int idx = 0; idx < static_cast<int>(presets.size()); ++idx) {
        const auto& preset = presets[static_cast<std::size_t>(idx)];
        if (!preset.isFactory || preset.category == "Utility") continue;
        ++padCount;

        const auto low = measurePresetSwell(proc, idx, 0);
        const auto high = measurePresetSwell(proc, idx, 127);

        const float swellDb = 20.0f * std::log10((high.rmsTotal + 1e-9f) / (low.rmsTotal + 1e-9f));
        const float sideLow = low.rmsSide / (low.rmsMid + 1e-9f);
        const float sideHigh = high.rmsSide / (high.rmsMid + 1e-9f);
        const float brightLow = low.rmsHigh2k / (low.rmsFund + 1e-9f);
        const float brightHigh = high.rmsHigh2k / (high.rmsFund + 1e-9f);

        std::cout << "[SWELL-METRICS] " << preset.name.toStdString()
                  << ": swellDb=" << swellDb
                  << " dB, side/mid=" << sideLow << "->" << sideHigh
                  << ", >2k/fund=" << brightLow << "->" << brightHigh
                  << ", preClipPeak=" << high.preClipPeak << "\n";

        EXPECT_GE(swellDb, 8.0f) << preset.name.toStdString();
        EXPECT_LE(swellDb, 22.0f) << preset.name.toStdString();
        EXPECT_GT(sideHigh, sideLow * 1.25f) << preset.name.toStdString();
        EXPECT_GT(brightHigh, brightLow * 1.60f) << preset.name.toStdString();
    }
    EXPECT_EQ(padCount, 8);
}

TEST(NovaSynthSwellDynamicsTests, NeutralNamingCategoryOrderDestinationNamesHostileInputsAndRoundtrip) {
    EXPECT_STREQ(nova::dsp::ModMatrix::getDestinationName(nova::dsp::ModMatrix::Destination::DelayFeedback), "Delay Feedback");
    EXPECT_STREQ(nova::dsp::ModMatrix::getDestinationName(nova::dsp::ModMatrix::Destination::SwellGain), "Swell Volume");
    EXPECT_STREQ(nova::dsp::ModMatrix::getDestinationName(nova::dsp::ModMatrix::Destination::AirAmount), "Air Amount");

    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    const auto& presets = proc.getPresetManager().getAllPresets();

    const std::array<const char*, 20> forbiddenTerms = {
        "worship", "bethel", "hillsong", "elevation", "jesus", "god",
        "prophet", "ob-6", "ob6", "oberheim", "mellotron", "juno",
        "roland", "solina", "dimension d", "valhalla", "jorna", "moog", "strymon", "nord"
    };

    std::vector<juce::String> catOrder;
    for (const auto& preset : presets) {
        if (!preset.isFactory) continue;
        if (std::find(catOrder.begin(), catOrder.end(), preset.category) == catOrder.end()) {
            catOrder.push_back(preset.category);
        }
        if (preset.category == "Utility") continue;

        juce::String combined = preset.name + " " + preset.description;
        for (const auto& t : preset.tags) combined += " " + t;
        const juce::String lower = combined.toLowerCase();
        for (const char* term : forbiddenTerms) {
            EXPECT_FALSE(lower.contains(term)) << "Forbidden term '" << term << "' in preset '" << preset.name.toStdString() << "'";
        }
    }
    ASSERT_EQ(catOrder.size(), 3u);
    EXPECT_EQ(catOrder[0], "Warm Pads");
    EXPECT_EQ(catOrder[1], "Airy & Shimmer");
    EXPECT_EQ(catOrder[2], "Utility");

    for (double sr : { 22050.0, 44100.0, 96000.0, 192000.0 }) {
        for (int bSize : { 1, 13, 64, 2048 }) {
            nova::synth::NovaSynthProcessor pHostile;
            pHostile.prepareToPlay(sr, bSize);
            ASSERT_TRUE(pHostile.getPresetManager().loadPresetByName("Soft Velvet Cushion"));
            juce::AudioBuffer<float> buf(2, bSize);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
            double sumSq = 0.0;
            float peak = 0.0f;
            const int blocks = std::max(12, static_cast<int>((sr * 0.10) / bSize));
            for (int b = 0; b < blocks; ++b) {
                const int cc = (b % 3 == 0) ? 0 : ((b % 3 == 1) ? 127 : 32);
                midi.addEvent(juce::MidiMessage::controllerEvent(1, 11, cc), 0);
                pHostile.processBlock(buf, midi);
                midi.clear();
                for (int ch = 0; ch < 2; ++ch) {
                    const float* r = buf.getReadPointer(ch);
                    for (int i = 0; i < bSize; ++i) {
                        ASSERT_TRUE(std::isfinite(r[i]));
                        peak = std::max(peak, std::abs(r[i]));
                        sumSq += static_cast<double>(r[i]) * r[i];
                    }
                }
            }
            EXPECT_LT(peak, 1.5f);
            EXPECT_GT(sumSq, 1e-6);
            pHostile.triggerPanic();
            for (int b = 0; b < std::max(8, static_cast<int>((sr * 0.04) / bSize)); ++b) {
                pHostile.processBlock(buf, midi);
            }
            EXPECT_LT(std::max(buf.getMagnitude(0, 0, bSize), buf.getMagnitude(1, 0, bSize)), 1e-3f);
        }
    }

    ASSERT_TRUE(proc.getPresetManager().loadPresetByName("Soft Velvet Cushion"));
    juce::MemoryBlock state;
    proc.getStateInformation(state);
    nova::synth::NovaSynthProcessor procRestored;
    procRestored.prepareToPlay(48000.0, 512);
    procRestored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    bool foundSwell = false, foundAir = false;
    for (std::size_t m = 0; m < nova::dsp::ModMatrix::kMaxSlots; ++m) {
        const auto& sl = procRestored.getModMatrix().getSlot(m);
        if (sl.enabled && sl.source == nova::dsp::ModMatrix::Source::ModWheel) {
            if (sl.destination == nova::dsp::ModMatrix::Destination::SwellGain && std::abs(sl.amount - 0.74f) < 1e-3f) foundSwell = true;
            if (sl.destination == nova::dsp::ModMatrix::Destination::AirAmount && std::abs(sl.amount - 0.40f) < 1e-3f) foundAir = true;
        }
    }
    EXPECT_TRUE(foundSwell);
    EXPECT_TRUE(foundAir);
}
