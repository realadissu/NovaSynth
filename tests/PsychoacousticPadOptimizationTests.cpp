#include <gtest/gtest.h>
#include "NovaSynthProcessor.h"
#include "nova_dsp/synth/WorshipSynthEngine.h"
#include "../libs/nova_core_dsp/src/OscillatorSharedTables.h"
#include <cmath>
#include <numbers>
#include <vector>
#include <algorithm>

namespace {

[[nodiscard]] float measureBinMagnitude(const float* buf, int n, double sr, float freq) noexcept {
    double real = 0.0, imag = 0.0;
    const double omega = 2.0 * std::numbers::pi_v<double> * static_cast<double>(freq) / sr;
    for (int i = 0; i < n; ++i) {
        const double angle = omega * static_cast<double>(i);
        real += static_cast<double>(buf[i]) * std::cos(angle);
        imag -= static_cast<double>(buf[i]) * std::sin(angle);
    }
    return static_cast<float>(std::sqrt(real * real + imag * imag) / static_cast<double>(n));
}

TEST(PsychoacousticPadOptimizationTests, AntiHowlFilterKneePreventsResonancePeakingBelowQuarterReso) {
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;
    nova::dsp::WorshipSynthEngine engine;
    engine.prepare(kSr, kBlock);

    auto renderSweptPeak = [&](float reso) {
        nova::dsp::WorshipSynthEngine::Parameters p;
        p.activeOscCount = 1;
        p.oscSlots[0].enabled = true;
        p.oscSlots[0].engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
        p.oscSlots[0].modelIndex = 0; // Saw wave (flat harmonic series)
        p.oscSlots[0].filterRouting = 0;
        p.oscSlots[0].level = 1.0f;
        p.filter1Enabled = true;
        p.filterType = 0; // LowPass 24
        p.filterCutoffHz = 1200.0f;
        p.filterResonance = reso;
        p.filterEnvAmount = 0.0f;
        p.filter2Enabled = false;
        p.delayEnabled = false;
        p.reverbMix = 0.0f;
        p.chorusEnabled = false;
        p.ampEnv.attackSec = 0.001f;
        p.ampEnv.sustainLvl = 1.0f;

        engine.setParameters(p);
        engine.snapSmoothersToTarget();
        engine.noteOn(60, 0.85f); // 261.63 Hz fundamental

        std::vector<float> bL(kBlock, 0.0f), bR(kBlock, 0.0f);
        // Stabilize engine
        for (int b = 0; b < 8; ++b) {
            std::fill(bL.begin(), bL.end(), 0.0f);
            std::fill(bR.begin(), bR.end(), 0.0f);
            engine.processBlock(bL.data(), bR.data(), kBlock);
        }

        // Measure H1 (261.63 Hz, passband) vs H4 (1046.5 Hz, cutoff knee)
        const float magPassband = measureBinMagnitude(bL.data(), kBlock, kSr, 261.63f);
        const float magKnee = measureBinMagnitude(bL.data(), kBlock, kSr, 1046.5f);

        engine.allNotesOff();
        engine.reset();
        return std::make_pair(magPassband, magKnee);
    };

    // Smooth pad / Swell setting (reso <= 0.25f): must have NO resonance peaking
    const auto [passPad, kneePad] = renderSweptPeak(0.12f);
    EXPECT_GT(passPad, 0.05f);
    // At Butterworth/Bessel transition (Q <= 0.60f), knee magnitude must never exceed passband!
    EXPECT_LE(kneePad, passPad * 0.95f)
        << "Anti-howl knee must prevent resonance peak at or below resonance 0.25f!";

    // High resonance setting (reso = 2.5f): resonance peak must be clearly present and boosted over pad
    const auto [passLead, kneeLead] = renderSweptPeak(2.5f);
    EXPECT_GT(kneeLead, kneePad * 1.25f)
        << "High resonance must retain resonant character!";
}

TEST(PsychoacousticPadOptimizationTests, DynamicPianoPocketCouplesWithCutoffAndSwell) {
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;
    nova::dsp::WorshipSynthEngine engine;
    engine.prepare(kSr, kBlock);

    auto renderMids = [&](float cutoffHz, float warmth, float modWheel) {
        nova::dsp::WorshipSynthEngine::Parameters p;
        p.activeOscCount = 1;
        p.oscSlots[0].enabled = true;
        p.oscSlots[0].engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
        p.oscSlots[0].modelIndex = 0; // Saw wave
        p.oscSlots[0].filterRouting = 2; // Both filters in serial
        p.oscSlots[0].level = 1.0f;
        p.filter1Enabled = true;
        p.filterType = 0; // LowPass 24
        p.filterCutoffHz = cutoffHz;
        p.filterResonance = 0.20f;
        p.filterEnvAmount = 0.0f;
        p.macroWarmth = warmth;

        p.filter2Enabled = true;
        p.filter2Type = 3; // Vocal Carve / Piano Pocket Bell Notch
        p.filter2CutoffHz = 720.0f; // Center of piano pocket
        p.vocalCarve = 0.30f;
        p.filterRoutingMode = 0; // Serial

        p.delayEnabled = false;
        p.reverbMix = 0.0f;
        p.chorusEnabled = false;
        p.ampEnv.attackSec = 0.001f;
        p.ampEnv.sustainLvl = 1.0f;

        engine.setParameters(p);
        engine.setModWheel(modWheel);
        engine.snapSmoothersToTarget();
        engine.noteOn(57, 0.85f); // A3 = 220 Hz; 3rd harmonic = 660 Hz, 4th = 880 Hz (in pocket)

        std::vector<float> bL(kBlock, 0.0f), bR(kBlock, 0.0f);
        for (int b = 0; b < 10; ++b) {
            std::fill(bL.begin(), bL.end(), 0.0f);
            std::fill(bR.begin(), bR.end(), 0.0f);
            engine.processBlock(bL.data(), bR.data(), kBlock);
        }

        // Measure harmonic 3 (660 Hz, in the 650-800 Hz piano pocket)
        const float midPocketEnergy = measureBinMagnitude(bL.data(), kBlock, kSr, 660.0f);
        const float lowFundamental = measureBinMagnitude(bL.data(), kBlock, kSr, 220.0f);

        engine.allNotesOff();
        engine.reset();
        return midPocketEnergy / std::max(1e-4f, lowFundamental);
    };

    // When cutoff is low (600 Hz <= 800 Hz, dark warmth), mid scoop is quiescent
    const float midRatioClosed = renderMids(600.0f, 0.80f, 0.0f);

    // When cutoff opens (3500 Hz > 800 Hz) and player pushes Swell pedal (modWheel = 0.90f)
    const float midRatioOpenSwell = renderMids(3500.0f, 0.20f, 0.90f);

    // With dynamic piano-pocket coupling, relative mid energy around 660-720 Hz is scoped
    EXPECT_LT(midRatioOpenSwell, 0.85f);
    EXPECT_GT(midRatioClosed, 0.001f);
}

TEST(PsychoacousticPadOptimizationTests, WavetableAndTexturePadModelsExhibitNaturalHarmonicRolloff) {
    const auto& wtBank = nova::dsp::getWavetableBank();
    const auto& texBank = nova::dsp::getTextureSourceBank();

    // Check Pad wavetables: 0 (Worship Harmonics), 2 (Strings), 3 (Choir), 6 (Brass)
    const int padTables[] = { 0, 2, 3, 6 };
    for (int t : padTables) {
        for (int f = 0; f < nova::dsp::SharedWavetableBank::kWtFrames; ++f) {
            float peak = 0.0f, sumSq = 0.0f;
            for (int i = 0; i < nova::dsp::SharedWavetableBank::kWtSize; ++i) {
                const float s = wtBank.tables[static_cast<std::size_t>(t)][static_cast<std::size_t>(f)][static_cast<std::size_t>(i)];
                EXPECT_FALSE(std::isnan(s));
                EXPECT_FALSE(std::isinf(s));
                peak = std::max(peak, std::abs(s));
                sumSq += s * s;
            }
            const float rms = std::sqrt(sumSq / static_cast<float>(nova::dsp::SharedWavetableBank::kWtSize));
            EXPECT_NEAR(rms, 0.52f, 0.03f) << "Wavetable " << t << " frame " << f << " RMS should be ~0.52";
            EXPECT_LE(peak, 2.5f);
        }
    }

    // Check Texture pad tables: 0 (Breath), 1 (Cello), 2 (Tape), 5 (Velvet Pad)
    const int padTex[] = { 0, 1, 2, 5 };
    for (int t : padTex) {
        float peak = 0.0f, sumSq = 0.0f;
        for (int i = 0; i < nova::dsp::SharedTextureSourceBank::kTexSize; ++i) {
            const float s = texBank.tables[static_cast<std::size_t>(t)][static_cast<std::size_t>(i)];
            EXPECT_FALSE(std::isnan(s));
            EXPECT_FALSE(std::isinf(s));
            peak = std::max(peak, std::abs(s));
            sumSq += s * s;
        }
        const float rms = std::sqrt(sumSq / static_cast<float>(nova::dsp::SharedTextureSourceBank::kTexSize));
        EXPECT_NEAR(rms, 0.50f, 0.03f) << "Texture table " << t << " RMS should be ~0.50";
        EXPECT_LE(peak, 2.5f);
    }
}

TEST(PsychoacousticPadOptimizationTests, HostileSampleRatesAndOddBufferSizesNeverProduceNaNOrInf) {
    nova::dsp::WorshipSynthEngine engine;
    const double testSampleRates[] = { 22050.0, 44100.0, 96000.0, 192000.0 };
    const int testBufferSizes[] = { 1, 13, 64, 512, 2048 };

    for (double sr : testSampleRates) {
        for (int bSize : testBufferSizes) {
            engine.prepare(sr, bSize);

            nova::dsp::WorshipSynthEngine::Parameters p;
            p.filter1Enabled = true;
            p.filterCutoffHz = 900.0f;
            p.filterResonance = 0.15f; // Anti-howl knee
            p.macroWarmth = 0.30f;
            p.filter2Enabled = true;
            p.filter2Type = 3; // Vocal Carve piano pocket
            p.vocalCarve = 0.40f;
            engine.setParameters(p);
            engine.setModWheel(0.75f);
            engine.noteOn(60, 0.9f);

            std::vector<float> left(static_cast<std::size_t>(bSize), 0.0f);
            std::vector<float> right(static_cast<std::size_t>(bSize), 0.0f);

            for (int b = 0; b < 6; ++b) {
                engine.processBlock(left.data(), right.data(), bSize);
                for (int i = 0; i < bSize; ++i) {
                    ASSERT_FALSE(std::isnan(left[static_cast<std::size_t>(i)]));
                    ASSERT_FALSE(std::isinf(left[static_cast<std::size_t>(i)]));
                    ASSERT_LE(std::abs(left[static_cast<std::size_t>(i)]), 4.0f);
                    ASSERT_FALSE(std::isnan(right[static_cast<std::size_t>(i)]));
                    ASSERT_FALSE(std::isinf(right[static_cast<std::size_t>(i)]));
                    ASSERT_LE(std::abs(right[static_cast<std::size_t>(i)]), 4.0f);
                }
            }

            engine.allNotesOff();
            engine.reset();
        }
    }
}

TEST(PsychoacousticPadOptimizationTests, StereoSpreadDecoupledFromDetune) {
    nova::dsp::MultiEngineOscillator osc;
    osc.prepare(48000.0);

    auto renderSideToMidRatio = [&](float spreadVal, float detuneVal) {
        nova::dsp::MultiEngineOscillator::Parameters p;
        p.engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
        p.modelIndex = 0; // Saw
        p.spread = spreadVal;
        p.detune = detuneVal;
        osc.setParameters(p);
        osc.triggerNote(220.0f, 0.8f, 42);

        double sumMidSq = 0.0, sumSideSq = 0.0;
        for (int i = 0; i < 2048; ++i) {
            float outL = 0.0f, outR = 0.0f;
            osc.processSample(220.0f, 0.0f, 0.0f, outL, outR);
            const float M = 0.5f * (outL + outR);
            const float S = 0.5f * (outL - outR);
            sumMidSq += static_cast<double>(M * M);
            sumSideSq += static_cast<double>(S * S);
        }
        return static_cast<float>(std::sqrt(sumSideSq / (sumMidSq + 1e-9)));
    };

    const float wideRatio = renderSideToMidRatio(0.85f, 0.12f);
    EXPECT_GT(wideRatio, 0.35f) << "High spread must generate wide stereo side energy even at low detune!";

    const float monoRatio = renderSideToMidRatio(0.0f, 0.12f);
    EXPECT_LT(monoRatio, 0.15f) << "Zero spread must collapse stereo side energy!";
    EXPECT_GT(wideRatio, monoRatio * 2.5f);
}

TEST(PsychoacousticPadOptimizationTests, FreeRunningUncorrelatedRandomPhasesAtNoteOn) {
    nova::dsp::MultiEngineOscillator osc;
    osc.prepare(48000.0);
    nova::dsp::MultiEngineOscillator::Parameters p;
    p.engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
    p.modelIndex = 6; // Velvet Pad
    p.detune = 0.12f;
    p.spread = 0.85f;
    osc.setParameters(p);

    osc.triggerNote(261.63f, 0.8f, 101);
    for (int u = 0; u < nova::dsp::MultiEngineOscillator::kMaxUnison; ++u) {
        const float ph = osc.getUnisonPhase(u);
        EXPECT_GE(ph, 0.0f);
        EXPECT_LT(ph, 1.0f);
    }
    EXPECT_NE(osc.getUnisonPhase(0), osc.getUnisonPhase(1));
    EXPECT_NE(osc.getUnisonPhase(1), osc.getUnisonPhase(2));
    EXPECT_NE(osc.getUnisonPhase(2), osc.getUnisonPhase(3));
    EXPECT_NE(osc.getUnisonPhase(3), osc.getUnisonPhase(4));

    const float firstPh0 = osc.getUnisonPhase(0);
    osc.triggerNote(261.63f, 0.8f, 202);
    EXPECT_NE(osc.getUnisonPhase(0), firstPh0);
}

TEST(PsychoacousticPadOptimizationTests, LinearHeadroomScalingNoTanhFlutterInWarmAnalogAndTexture) {
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;
    nova::dsp::WorshipSynthEngine engine;
    engine.prepare(kSr, kBlock);

    auto renderPolyChord = [&](nova::dsp::MultiEngineOscillator::EngineType eng, int model) {
        nova::dsp::WorshipSynthEngine::Parameters p;
        p.activeOscCount = 1;
        p.oscSlots[0].enabled = true;
        p.oscSlots[0].engine = eng;
        p.oscSlots[0].modelIndex = model;
        p.oscSlots[0].level = 1.0f;
        p.filter1Enabled = false;
        p.filter2Enabled = false;
        p.reverbMix = 0.0f;
        p.chorusEnabled = false;
        engine.setParameters(p);
        engine.snapSmoothersToTarget();

        const int notes[5] = { 36, 43, 48, 55, 60 };
        for (int n : notes) engine.noteOn(n, 0.85f);

        std::vector<float> bL(kBlock, 0.0f), bR(kBlock, 0.0f);
        float peak = 0.0f;
        for (int b = 0; b < 10; ++b) {
            std::fill(bL.begin(), bL.end(), 0.0f);
            std::fill(bR.begin(), bR.end(), 0.0f);
            engine.processBlock(bL.data(), bR.data(), kBlock);
            for (int i = 0; i < kBlock; ++i) {
                EXPECT_FALSE(std::isnan(bL[static_cast<std::size_t>(i)]));
                EXPECT_FALSE(std::isinf(bL[static_cast<std::size_t>(i)]));
                peak = std::max(peak, std::abs(bL[static_cast<std::size_t>(i)]));
            }
        }
        engine.allNotesOff();
        engine.reset();
        return peak;
    };

    const float peakWarm = renderPolyChord(nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog, 6);
    EXPECT_GT(peakWarm, 0.10f);
    EXPECT_LT(peakWarm, 4.0f);

    const float peakTex = renderPolyChord(nova::dsp::MultiEngineOscillator::EngineType::OrganicTexture, 2);
    EXPECT_GT(peakTex, 0.10f);
    EXPECT_LT(peakTex, 4.0f);
}

TEST(PsychoacousticPadOptimizationTests, VelvetPianoCushionPresetSettings) {
    nova::synth::NovaSynthProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    auto& pm = processor.getPresetManager();
    ASSERT_TRUE(pm.loadPresetByName("Velvet Piano Cushion"));

    auto& apvts = processor.getAPVTS();
    EXPECT_FLOAT_EQ(apvts.getRawParameterValue("filterType")->load(), 3.0f);
    EXPECT_FLOAT_EQ(apvts.getRawParameterValue("filterCutoffHz")->load(), 1150.0f);
    EXPECT_FLOAT_EQ(apvts.getRawParameterValue("filterKeyTrack")->load(), 0.45f);
    EXPECT_FLOAT_EQ(apvts.getRawParameterValue("filter2Type")->load(), 3.0f);
    EXPECT_FLOAT_EQ(apvts.getRawParameterValue("vocalCarve")->load(), 0.45f);
    EXPECT_FLOAT_EQ(apvts.getRawParameterValue("analogDrift")->load(), 0.48f);
    EXPECT_FLOAT_EQ(apvts.getRawParameterValue("osc1_fine")->load(), -5.0f);
    EXPECT_FLOAT_EQ(apvts.getRawParameterValue("osc2_fine")->load(), 6.0f);
    EXPECT_GE(apvts.getRawParameterValue("osc1_spread")->load(), 0.80f);
    EXPECT_GE(apvts.getRawParameterValue("osc2_spread")->load(), 0.80f);
    EXPECT_FLOAT_EQ(apvts.getRawParameterValue("chorusEnabled")->load(), 1.0f);
    EXPECT_FLOAT_EQ(apvts.getRawParameterValue("chorusMode")->load(), 0.0f);
    EXPECT_FLOAT_EQ(apvts.getRawParameterValue("chorusMix")->load(), 0.40f);
    EXPECT_FLOAT_EQ(apvts.getRawParameterValue("reverbDecay")->load(), 4.2f);
    EXPECT_FLOAT_EQ(apvts.getRawParameterValue("reverbShimmer")->load(), 0.0f);
}

} // namespace
