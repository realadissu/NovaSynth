#include <gtest/gtest.h>
#include "NovaSynthProcessor.h"
#include "nova_dsp/synth/WorshipSynthEngine.h"
#include "nova_dsp/oscillators/MultiEngineOscillator.h"
#include "nova_dsp/effects/StereoBbdChorus.h"
#include "../libs/nova_core_dsp/src/OscillatorSharedTables.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace {

[[nodiscard]] float measureMag(const float* buf, int n, double sr, float freqHz) noexcept {
    double real = 0.0, imag = 0.0;
    const double omega = 2.0 * std::numbers::pi_v<double> * static_cast<double>(freqHz) / sr;
    for (int i = 0; i < n; ++i) {
        const double angle = omega * static_cast<double>(i);
        real += static_cast<double>(buf[i]) * std::cos(angle);
        imag -= static_cast<double>(buf[i]) * std::sin(angle);
    }
    return static_cast<float>(2.0 * std::hypot(real, imag) / static_cast<double>(n));
}

[[nodiscard]] float computeMean(const std::vector<float>& buf) noexcept {
    double sum = 0.0;
    for (float s : buf) sum += static_cast<double>(s);
    return static_cast<float>(sum / static_cast<double>(std::max<std::size_t>(1, buf.size())));
}

[[nodiscard]] float computeRms(const std::vector<float>& buf) noexcept {
    double sumSq = 0.0;
    for (float s : buf) sumSq += static_cast<double>(s) * static_cast<double>(s);
    return static_cast<float>(std::sqrt(sumSq / static_cast<double>(std::max<std::size_t>(1, buf.size()))));
}

[[nodiscard]] float computeL2Diff(const std::vector<float>& a, const std::vector<float>& b) noexcept {
    const std::size_t n = std::min(a.size(), b.size());
    double sumSq = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double d = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        sumSq += d * d;
    }
    return static_cast<float>(sumSq);
}

void assertFiniteBounded(const std::vector<float>& bufL, const std::vector<float>& bufR) {
    for (std::size_t i = 0; i < bufL.size(); ++i) {
        ASSERT_TRUE(std::isfinite(bufL[i]) && std::isfinite(bufR[i]));
        ASSERT_LE(std::abs(bufL[i]), 4.0f);
        ASSERT_LE(std::abs(bufR[i]), 4.0f);
    }
}

std::pair<std::vector<float>, std::vector<float>> renderOsc(
    const nova::dsp::MultiEngineOscillator::Parameters& p, float f0, double sr, int nSamples, uint32_t seed) {
    nova::dsp::MultiEngineOscillator osc;
    osc.prepare(sr);
    osc.setParameters(p);
    osc.triggerNote(f0, 0.95f, seed);
    std::vector<float> bufL(static_cast<std::size_t>(nSamples), 0.0f);
    std::vector<float> bufR(static_cast<std::size_t>(nSamples), 0.0f);
    for (int i = 0; i < nSamples; ++i) {
        osc.processSample(f0, 0.0f, 0.0f, bufL[static_cast<std::size_t>(i)], bufR[static_cast<std::size_t>(i)]);
    }
    assertFiniteBounded(bufL, bufR);
    return { std::move(bufL), std::move(bufR) };
}

[[nodiscard]] float renderFilterRms(bool flt1Enabled, float cutoffHz, float reso, float warmth = 0.50f) {
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;
    nova::dsp::WorshipSynthEngine engine;
    engine.prepare(kSr, kBlock);
    auto p = engine.getParameters();
    p.activeOscCount = 1;
    p.oscSlots[0] = { true, 0, nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog, 3, 0, 0, 0, 0.0f, 0.0f, 0.0f, 1.0f };
    p.filter1Enabled = flt1Enabled; p.filterType = 0; p.filterCutoffHz = cutoffHz;
    p.filterResonance = reso; p.filterEnvAmount = 0.0f; p.filterDrive = 0.0f;
    p.filter2Enabled = false; p.lfo1Depth = 0.0f; p.chorusEnabled = false;
    p.delayEnabled = false; p.reverbMix = 0.0f; p.macroWarmth = warmth; p.macroAir = 0.30f;
    p.ampEnv.attackSec = 0.001f; p.ampEnv.decaySec = 0.05f; p.ampEnv.sustainLvl = 1.0f;
    engine.setParameters(p);
    engine.snapSmoothersToTarget();
    engine.noteOn(69, 1.0f); // A4 = 440 Hz

    std::vector<float> bL(kBlock, 0.0f), bR(kBlock, 0.0f);
    double sumSq = 0.0;
    int count = 0;
    for (int b = 0; b < 12; ++b) {
        std::fill(bL.begin(), bL.end(), 0.0f);
        std::fill(bR.begin(), bR.end(), 0.0f);
        engine.processBlock(bL.data(), bR.data(), kBlock);
        if (b >= 8) {
            for (int i = 0; i < kBlock; ++i) {
                const float s = 0.5f * (bL[static_cast<std::size_t>(i)] + bR[static_cast<std::size_t>(i)]);
                sumSq += static_cast<double>(s) * static_cast<double>(s);
                ++count;
            }
        }
    }
    return static_cast<float>(std::sqrt(sumSq / static_cast<double>(count)));
}

} // namespace

TEST(NovaSynthBugfixRegressionTests, WarmAnalogSubOctaveDcFreeAndTrueHalfFrequency) {
    using ET = nova::dsp::MultiEngineOscillator::EngineType;
    constexpr double kSr = 48000.0;
    constexpr int kN = 48000;
    nova::dsp::MultiEngineOscillator::Parameters p{};
    p.enabled = true; p.engine = ET::WarmAnalog; p.detune = 0.0f; p.scatter = 0.0f; p.level = 1.0f;

    p.modelIndex = 0; p.shapeMorph = 0.80f;
    const auto [sawL, sawR] = renderOsc(p, 440.0f, kSr, kN, 1u);
    (void)sawR;
    EXPECT_LT(std::abs(computeMean(sawL)), 0.01f) << "Model 0 sub-octave produced DC offset";
    EXPECT_GT(measureMag(sawL.data(), kN, kSr, 220.0f), 0.08f) << "Model 0 missing f0/2 sub-octave";

    p.modelIndex = 6; p.shapeMorph = 0.35f;
    const auto [velL, velR] = renderOsc(p, 440.0f, kSr, kN, 1u);
    (void)velR;
    EXPECT_LT(std::abs(computeMean(velL)), 0.01f) << "Model 6 sub-triangle produced DC offset";
    const float sub220 = measureMag(velL.data(), kN, kSr, 220.0f);
    const float sub660 = measureMag(velL.data(), kN, kSr, 660.0f);
    EXPECT_GT(sub220, 0.08f) << "Model 6 missing f0/2 sub-triangle";
    EXPECT_GT(sub220, sub660 * 5.0f) << "Model 6 sub-triangle rolloff degraded";
}

TEST(NovaSynthBugfixRegressionTests, SuperSawMultiHasIndependentDetunedPhasesNotStaticComb) {
    using ET = nova::dsp::MultiEngineOscillator::EngineType;
    constexpr int kWin = 1091, kNumWins = 20;
    nova::dsp::MultiEngineOscillator::Parameters p{};
    p.enabled = true; p.engine = ET::WarmAnalog; p.modelIndex = 4;
    p.shapeMorph = 0.50f; p.detune = 0.0f; p.scatter = 0.0f; p.level = 1.0f;
    const auto [bufL, bufR] = renderOsc(p, 440.0f, 48000.0, 24000, 7u);
    (void)bufR;

    float minRms = 1e9f, maxRms = 0.0f, sumRms = 0.0f;
    for (int w = 0; w < kNumWins; ++w) {
        double sq = 0.0;
        for (int i = 0; i < kWin; ++i) {
            const float s = bufL[static_cast<std::size_t>(w * kWin + i)];
            sq += static_cast<double>(s) * static_cast<double>(s);
        }
        const float rms = static_cast<float>(std::sqrt(sq / static_cast<double>(kWin)));
        minRms = std::min(minRms, rms); maxRms = std::max(maxRms, rms); sumRms += rms;
    }
    EXPECT_GT((maxRms - minRms) / std::max(1e-6f, sumRms / static_cast<float>(kNumWins)), 0.05f)
        << "Super Saw Multi layers are phase-locked into a static comb filter";
}

TEST(NovaSynthBugfixRegressionTests, OrganicTextureVelvetTableDcFreeAndNoForcedOrganFifths) {
    const auto& velvetTable = nova::dsp::getTextureSourceBank().tables[5];
    constexpr int kTexN = nova::dsp::SharedTextureSourceBank::kTexSize;
    double dcSum = 0.0, real = 0.0, imag = 0.0;
    for (int i = 0; i < kTexN; ++i) {
        const float s = velvetTable[static_cast<std::size_t>(i)];
        dcSum += static_cast<double>(s);
        const double ang = 2.0 * std::numbers::pi_v<double> * 4.0 * static_cast<double>(i) / static_cast<double>(kTexN);
        real += static_cast<double>(s) * std::cos(ang);
        imag -= static_cast<double>(s) * std::sin(ang);
    }
    EXPECT_LT(std::abs(dcSum / static_cast<double>(kTexN)), 0.005);
    EXPECT_GT(2.0 * std::hypot(real, imag) / static_cast<double>(kTexN), 0.05);

    constexpr double kSr = 48000.0;
    constexpr int kN = 24000;
    constexpr float kF0 = 261.63f;
    auto renderTex = [&](int model) {
        nova::dsp::MultiEngineOscillator::Parameters p{};
        p.enabled = true; p.engine = nova::dsp::MultiEngineOscillator::EngineType::OrganicTexture;
        p.modelIndex = model; p.detune = 0.20f; p.scatter = 0.10f; p.duration = 0.50f;
        p.shapeMorph = 0.50f; p.level = 1.0f;
        const auto [l, r] = renderOsc(p, kF0, kSr, kN, 42u);
        std::vector<float> mono(kN);
        for (int i = 0; i < kN; ++i) mono[static_cast<std::size_t>(i)] = 0.5f * (l[static_cast<std::size_t>(i)] + r[static_cast<std::size_t>(i)]);
        return mono;
    };

    for (int model : { 2, 4, 5 }) {
        const auto mono = renderTex(model);
        const float magFund = measureMag(mono.data(), kN, kSr, kF0);
        const float magFifth = measureMag(mono.data(), kN, kSr, kF0 * 1.5f);
        EXPECT_GT(magFund, 0.02f);
        EXPECT_LT(magFifth, magFund * 0.15f) << "OrganicTexture model " << model << " emits forced 1.5x fifth";
    }
    const auto rain = renderTex(3);
    EXPECT_LT(measureMag(rain.data(), kN, kSr, kF0 * 1.5f), measureMag(rain.data(), kN, kSr, kF0) * 0.15f);
    EXPECT_GT(measureMag(rain.data(), kN, kSr, kF0 * 2.0f), 0.015f);
}

TEST(NovaSynthBugfixRegressionTests, DeadParametersNowActiveInOscillatorsChorusAndPreset47) {
    using ET = nova::dsp::MultiEngineOscillator::EngineType;
    constexpr double kSr = 48000.0;

    for (auto eng : { ET::WarmAnalog, ET::EvolvingWavetable }) {
        nova::dsp::MultiEngineOscillator::Parameters p{};
        p.enabled = true; p.engine = eng; p.modelIndex = 0; p.shapeMorph = 0.35f; p.detune = 0.0f; p.spread = 0.85f; p.level = 1.0f;
        p.scatter = 0.0f;
        const auto [l0, r0] = renderOsc(p, 440.0f, kSr, 4096, 99u);
        p.scatter = 0.85f;
        const auto [l1, r1] = renderOsc(p, 440.0f, kSr, 4096, 99u);
        std::vector<float> side0(4096), side1(4096);
        for (std::size_t i = 0; i < 4096; ++i) { side0[i] = 0.5f * (l0[i] - r0[i]); side1[i] = 0.5f * (l1[i] - r1[i]); }
        EXPECT_LT(computeRms(side0), 1e-4f);
        EXPECT_GT(computeRms(side1), 0.05f);
        EXPECT_GT(computeL2Diff(l0, l1), 1.0f);
    }

    nova::dsp::MultiEngineOscillator::Parameters waP{};
    waP.enabled = true; waP.engine = ET::WarmAnalog; waP.modelIndex = 0; waP.shapeMorph = 0.0f; waP.detune = 1.0f; waP.level = 1.0f;
    waP.scatter = 0.0f; const auto [wa0L, wa0R] = renderOsc(waP, 440.0f, kSr, 24000, 99u);
    waP.scatter = 1.0f; const auto [wa1L, wa1R] = renderOsc(waP, 440.0f, kSr, 24000, 99u);
    (void)wa0R; (void)wa1R;
    EXPECT_GT(computeL2Diff(wa0L, wa1L), 1.0f);
    const float ext0 = measureMag(wa0L.data(), 24000, kSr, 450.3f) + measureMag(wa0L.data(), 24000, kSr, 429.9f);
    const float ext1 = measureMag(wa1L.data(), 24000, kSr, 450.3f) + measureMag(wa1L.data(), 24000, kSr, 429.9f);
    EXPECT_GT(ext1, ext0 * 2.5f);

    nova::dsp::MultiEngineOscillator::Parameters texP{};
    texP.enabled = true; texP.engine = ET::OrganicTexture; texP.modelIndex = 5; texP.scatter = 0.25f; texP.duration = 0.40f; texP.level = 1.0f;
    auto texBuf = [&](float det, float shp, float rt) {
        texP.detune = det; texP.shapeMorph = shp; texP.rate = rt;
        return renderOsc(texP, 261.63f, kSr, 24000, 55u).first;
    };
    auto highRatio = [&](const std::vector<float>& b) {
        float sum = 0.0f;
        for (int h = 6; h <= 12; ++h) sum += measureMag(b.data(), 24000, kSr, 261.63f * static_cast<float>(h));
        return sum / std::max(1e-5f, computeRms(b));
    };
    EXPECT_GT(highRatio(texBuf(0.20f, 0.95f, 0.50f)), highRatio(texBuf(0.20f, 0.05f, 0.50f)) * 1.4f);
    EXPECT_GT(computeL2Diff(texBuf(0.0f, 0.50f, 0.50f), texBuf(1.0f, 0.50f, 0.50f)), 0.5f);
    EXPECT_GT(computeL2Diff(texBuf(0.25f, 0.50f, 0.05f), texBuf(0.25f, 0.50f, 0.95f)), 0.5f);

    nova::dsp::MultiEngineOscillator::Parameters sp{};
    sp.enabled = true; sp.sourceType = 4; sp.engine = ET::SamplePlayback; sp.modelIndex = 0;
    sp.detune = 0.0f; sp.scatter = 1.0f; sp.level = 1.0f; sp.sampleLoopMode = 1;
    const auto [sL, sR] = renderOsc(sp, 261.6256f, kSr, 48000, 123u);
    EXPECT_GT(measureMag(sL.data() + 4800, 43200, kSr, 257.875f), measureMag(sL.data() + 4800, 43200, kSr, 269.292f) * 3.0f);
    EXPECT_GT(measureMag(sR.data() + 4800, 43200, kSr, 265.431f), measureMag(sR.data() + 4800, 43200, kSr, 269.292f) * 3.0f);

    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(kSr, 512);
    ASSERT_NE(proc.getAPVTS().getParameter("chorusWarmth"), nullptr);
    ASSERT_TRUE(proc.getPresetManager().loadPresetByName("Sunday Piano Foundation"));
    proc.syncParametersFromAPVTS();
    EXPECT_NEAR(proc.getCurrentParameters().chorusWarmth, 0.80f, 1e-3f);

    if (auto* cw = proc.getAPVTS().getParameter("chorusWarmth")) cw->setValueNotifyingHost(cw->convertTo0to1(0.25f));
    juce::MemoryBlock state;
    proc.getStateInformation(state);
    nova::synth::NovaSynthProcessor proc2;
    proc2.prepareToPlay(kSr, 512);
    proc2.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    proc2.syncParametersFromAPVTS();
    EXPECT_NEAR(proc2.getCurrentParameters().chorusWarmth, 0.25f, 1e-3f);

    bool foundAnthem = false;
    for (const auto& preset : proc.getPresetManager().getAllPresets()) {
        if (preset.name == "Holy Forever Anthem Pad") {
            foundAnthem = true;
            EXPECT_EQ(preset.parameterValues.count("lfo4Waveform"), 0u);
            EXPECT_EQ(preset.parameterValues.count("lfo4Shape"), 1u);
        }
    }
    ASSERT_TRUE(foundAnthem && proc.getPresetManager().loadPresetByName("Holy Forever Anthem Pad"));
    proc.syncParametersFromAPVTS();
    EXPECT_EQ(proc.getCurrentParameters().lfo4Shape, nova::dsp::LfoGenerator::Shape::PulseGate);
}

TEST(NovaSynthBugfixRegressionTests, FourPoleFilter1ButterworthMinus3dBAndNoDoubleMacroWarmth) {
    const float rmsBypass  = renderFilterRms(false, 440.0f, 0.30f);
    const float rmsAtCut   = renderFilterRms(true,  440.0f, 0.30f);
    const float rmsPass    = renderFilterRms(true, 1760.0f, 0.30f);
    const float rmsStop    = renderFilterRms(true,  220.0f, 0.30f);
    const float rmsResMid  = renderFilterRms(true,  440.0f, 0.75f);
    const float rmsResHigh = renderFilterRms(true,  440.0f, 2.00f);

    const float dbAtCut = 20.0f * std::log10(rmsAtCut / std::max(1e-6f, rmsBypass));
    const float dbPass  = 20.0f * std::log10(rmsPass  / std::max(1e-6f, rmsBypass));
    const float dbStop  = 20.0f * std::log10(rmsStop  / std::max(1e-6f, rmsBypass));
    EXPECT_GE(dbAtCut, -3.6f); EXPECT_LE(dbAtCut, -2.4f);
    EXPECT_GE(dbPass, -0.5f);  EXPECT_LE(dbPass, 0.3f);
    EXPECT_LE(dbStop, -20.0f);
    EXPECT_GT(rmsResHigh, rmsResMid);
    EXPECT_GT(rmsResMid, rmsAtCut);

    auto checkNoDupSlot = [](const nova::dsp::ModMatrix& mm) {
        for (std::size_t i = 0; i < nova::dsp::ModMatrix::kMaxSlots; ++i) {
            const auto& slot = mm.getSlot(i);
            EXPECT_FALSE(slot.enabled && slot.source == nova::dsp::ModMatrix::Source::MacroWarmth &&
                         slot.destination == nova::dsp::ModMatrix::Destination::FilterCutoff);
        }
    };
    nova::dsp::WorshipSynthEngine defaultEngine;
    checkNoDupSlot(defaultEngine.getModMatrix());
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    ASSERT_TRUE(proc.getPresetManager().loadPresetByName("Bethel Shimmer Cloud"));
    checkNoDupSlot(proc.getEngine().getModMatrix());

    const float dbWarmth1 = 20.0f * std::log10(renderFilterRms(true, 702.6f, 0.30f, 1.0f) / std::max(1e-6f, rmsBypass));
    EXPECT_GE(dbWarmth1, -3.8f); EXPECT_LE(dbWarmth1, -2.2f);
    auto* flt2Param = dynamic_cast<juce::AudioParameterChoice*>(proc.getAPVTS().getParameter("filter2Type"));
    ASSERT_NE(flt2Param, nullptr);
    ASSERT_GE(flt2Param->choices.size(), 4);
    EXPECT_TRUE(flt2Param->choices[0].contains("LOW 12") && !flt2Param->choices[0].contains("LOW 24"));
    EXPECT_TRUE(flt2Param->choices[2].contains("HIGH 12") && !flt2Param->choices[2].contains("HIGH 24"));
}

TEST(NovaSynthBugfixRegressionTests, SolinaEnsembleUsesAllThreePhaseTapsWithBalancedHeadroom) {
    using nova::dsp::StereoBbdChorus;
    StereoBbdChorus chorus;
    chorus.prepare(48000.0);
    StereoBbdChorus::Parameters p{ true, StereoBbdChorus::Mode::SolinaEnsemble, 1.0f, 0.50f, 0.85f, 0.0f };
    chorus.setParameters(p);
    for (int i = 0; i < 20000; ++i) { float l = 0.0f, r = 0.0f; chorus.process(0.0f, 0.0f, l, r); }
    std::vector<float> irL(600, 0.0f), irR(600, 0.0f);
    for (int i = 0; i < 600; ++i) {
        const float in = (i == 0) ? 1.0f : 0.0f;
        chorus.process(in, in, irL[static_cast<std::size_t>(i)], irR[static_cast<std::size_t>(i)]);
    }
    auto countPeaks = [](const std::vector<float>& ir) {
        int count = 0, lastIdx = -100;
        for (int i = 50; i < 500; ++i) {
            const float y = ir[static_cast<std::size_t>(i)];
            if (y > 0.015f && y >= ir[static_cast<std::size_t>(i - 1)] && y > ir[static_cast<std::size_t>(i + 1)] && (i - lastIdx) >= 10) { ++count; lastIdx = i; }
        }
        return count;
    };
    EXPECT_EQ(countPeaks(irL), 3);
    EXPECT_EQ(countPeaks(irR), 3);
    for (double sr : { 22050.0, 44100.0, 96000.0, 192000.0 }) {
        chorus.prepare(sr); p.rateHz = 8.0f; p.depth = 1.0f; chorus.setParameters(p);
        float peak = 0.0f;
        for (int i = 0; i < 2048; ++i) {
            const float in = std::sin(2.0f * std::numbers::pi_v<float> * 261.63f * static_cast<float>(i) / static_cast<float>(sr));
            float l = 0.0f, r = 0.0f; chorus.process(in, in, l, r);
            ASSERT_TRUE(std::isfinite(l) && std::isfinite(r));
            peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
        }
        EXPECT_LE(peak, 1.25f);
    }
}

TEST(NovaSynthBugfixRegressionTests, HostileInputsAndAcousticInvariantsAcrossFixedEngines) {
    struct EngineCase { int sourceType; int engine; int model; };
    const std::array<EngineCase, 3> cases = { { { 0, 0, 6 }, { 0, 2, 5 }, { 4, 3, 0 } } };
    for (double sr : { 22050.0, 44100.0, 96000.0, 192000.0 }) {
        for (int bSize : { 1, 13, 64, 512 }) {
            for (const auto& c : cases) {
                nova::synth::NovaSynthProcessor proc;
                proc.prepareToPlay(sr, bSize);
                auto& apvts = proc.getAPVTS();
                auto setRaw = [&](const char* id, float val) {
                    if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(val));
                };
                setRaw("activeOscCount", 1.0f); setRaw("osc1_enabled", 1.0f);
                setRaw("osc1_sourceType", static_cast<float>(c.sourceType));
                setRaw("osc1_engine", static_cast<float>(c.engine)); setRaw("osc1_model", static_cast<float>(c.model));
                setRaw("osc1_detune", 1.0f); setRaw("osc1_scatter", 1.0f); setRaw("osc1_level", 1.0f);
                setRaw("ampAttack", 0.002f); setRaw("filterResonance", 6.0f);
                setRaw("chorusEnabled", 1.0f); setRaw("chorusMode", 3.0f); setRaw("chorusMix", 0.7f); setRaw("chorusWarmth", 0.9f);
                proc.syncParametersFromAPVTS();
                juce::AudioBuffer<float> buf(2, bSize);
                juce::MidiBuffer midi;
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(115)), 0);
                double sumSq = 0.0;
                const int blocks = std::max(12, static_cast<int>((sr * 0.06) / bSize));
                for (int b = 0; b < blocks; ++b) {
                    proc.processBlock(buf, midi); midi.clear();
                    for (int ch = 0; ch < 2; ++ch) {
                        const float* ptr = buf.getReadPointer(ch);
                        for (int i = 0; i < bSize; ++i) {
                            ASSERT_TRUE(std::isfinite(ptr[i])); ASSERT_LE(std::abs(ptr[i]), 4.0f);
                            sumSq += static_cast<double>(ptr[i]) * static_cast<double>(ptr[i]);
                        }
                    }
                }
                EXPECT_GT(sumSq, 1e-4);
                proc.triggerPanic();
                for (int b = 0; b < std::max(8, static_cast<int>((sr * 0.02) / bSize)); ++b) proc.processBlock(buf, midi);
                EXPECT_LT(std::max(buf.getMagnitude(0, 0, bSize), buf.getMagnitude(1, 0, bSize)), 1e-3f);
            }
        }
    }
}
