#include <gtest/gtest.h>
#include "NovaSynthProcessor.h"
#include "nova_dsp/synth/WorshipSynthEngine.h"
#include "nova_dsp/oscillators/MultiEngineOscillator.h"
#include "nova_dsp/filters/StateVariableFilter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace {

[[nodiscard]] float measureDftMag(const float* buf, int n, double sr, float freqHz) noexcept {
    double real = 0.0, imag = 0.0, sumW = 0.0;
    const double omega = 2.0 * std::numbers::pi_v<double> * static_cast<double>(freqHz) / sr;
    const double denom = static_cast<double>(std::max(1, n - 1));
    for (int i = 0; i < n; ++i) {
        const double w = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi_v<double> * static_cast<double>(i) / denom));
        const double ang = omega * static_cast<double>(i);
        const double s = static_cast<double>(buf[i]) * w;
        real += s * std::cos(ang);
        imag -= s * std::sin(ang);
        sumW += w;
    }
    return static_cast<float>(2.0 * std::hypot(real, imag) / std::max(1.0, sumW));
}

[[nodiscard]] float computeRms(const float* buf, int n) noexcept {
    double sq = 0.0;
    for (int i = 0; i < n; ++i) sq += static_cast<double>(buf[i]) * static_cast<double>(buf[i]);
    return static_cast<float>(std::sqrt(sq / static_cast<double>(std::max(1, n))));
}

[[nodiscard]] float computePearsonCorr(const float* a, const float* b, int n) noexcept {
    double meanA = 0.0, meanB = 0.0;
    for (int i = 0; i < n; ++i) { meanA += a[i]; meanB += b[i]; }
    meanA /= static_cast<double>(n); meanB /= static_cast<double>(n);
    double num = 0.0, denA = 0.0, denB = 0.0;
    for (int i = 0; i < n; ++i) {
        const double da = static_cast<double>(a[i]) - meanA;
        const double db = static_cast<double>(b[i]) - meanB;
        num += da * db; denA += da * da; denB += db * db;
    }
    const double denom = std::sqrt(denA * denB);
    return (denom > 1e-12) ? static_cast<float>(num / denom) : 0.0f;
}

void configureCleanEngine(nova::dsp::WorshipSynthEngine::Parameters& p) noexcept {
    p.activeOscCount = 1;
    p.oscSlots[0] = {};
    p.oscSlots[0].enabled = true;
    p.oscSlots[0].engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
    p.oscSlots[0].modelIndex = 0;
    p.oscSlots[0].detune = 0.0f;
    p.oscSlots[0].scatter = 0.0f;
    p.oscSlots[0].level = 0.9f;
    p.filter1Enabled = true;
    p.filterType = 3;
    p.filterCutoffHz = 2000.0f;
    p.filterResonance = 0.7071f;
    p.filterEnvAmount = 0.0f;
    p.filterKeyTrack = 0.0f;
    p.filterDrive = 0.0f;
    p.filter2Enabled = false;
    p.lfo1Depth = 0.0f; p.lfo2Depth = 0.0f;
    p.macroWarmth = 0.50f; p.macroMotion = 0.0f; p.macroShimmer = 0.0f; p.macroAir = 0.30f;
    p.analogDrift = 0.0f;
    p.chorusEnabled = false;
    p.delayEnabled = false;
    p.reverbMix = 0.0f;
    p.ampEnv = { 0.002f, 0.2f, 1.0f, 0.2f };
}

void setProcParam(nova::synth::NovaSynthProcessor& proc, const char* id, float val) {
    if (auto* p = proc.getAPVTS().getParameter(id)) {
        p->setValueNotifyingHost(p->convertTo0to1(val));
    }
}

} // namespace

TEST(NovaSynthAirAndNoiseTests, MacroAirHighShelfBoosts8To12kHzWithoutMidExplosionAndCouplesReverb) {
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    auto renderAirBand = [&](float airVal) {
        nova::dsp::WorshipSynthEngine engine;
        engine.prepare(kSr, kBlock);
        auto p = engine.getParameters();
        configureCleanEngine(p);
        p.filterCutoffHz = 2200.0f;
        p.macroAir = airVal;
        engine.setParameters(p);
        engine.snapSmoothersToTarget();
        engine.noteOn(60, 0.9f);

        std::vector<float> bL(kBlock), bR(kBlock), tail(8192);
        for (int b = 0; b < 24; ++b) {
            std::fill(bL.begin(), bL.end(), 0.0f);
            std::fill(bR.begin(), bR.end(), 0.0f);
            engine.processBlock(bL.data(), bR.data(), kBlock);
            if (b >= 8) std::copy(bL.begin(), bL.end(), tail.begin() + (b - 8) * kBlock);
        }
        float midSum = 0.0f, airSum = 0.0f;
        for (int h = 4; h <= 7; ++h)  midSum += measureDftMag(tail.data(), 8192, kSr, 261.6256f * static_cast<float>(h));
        for (int h = 31; h <= 45; ++h) airSum += measureDftMag(tail.data(), 8192, kSr, 261.6256f * static_cast<float>(h));
        return std::make_pair(midSum, airSum);
    };

    const auto [midLow, airLow]   = renderAirBand(0.15f);
    const auto [midHigh, airHigh] = renderAirBand(0.95f);
    EXPECT_GT(airHigh, airLow * 1.80f);
    EXPECT_NEAR(midHigh, midLow, midLow * 0.15f);

    auto renderReverbTailAirRms = [&](float airVal, float warmthVal) {
        nova::dsp::WorshipSynthEngine engine;
        engine.prepare(kSr, kBlock);
        auto p = engine.getParameters();
        configureCleanEngine(p);
        p.filter1Enabled = false;
        p.reverbDry = 0.0f;
        p.reverbMix = 0.85f;
        p.reverbDecay = 4.0f;
        p.reverbShimmer = 0.0f;
        p.macroWarmth = warmthVal;
        p.macroAir = airVal;
        engine.setParameters(p);
        engine.snapSmoothersToTarget();
        engine.noteOn(60, 0.95f);

        nova::dsp::StateVariableFilter hp1, hp2;
        hp1.prepare(kSr); hp2.prepare(kSr);
        hp1.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, 8000.0f, 0.7071f);
        hp2.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, 8000.0f, 0.7071f);

        std::vector<float> bL(kBlock), bR(kBlock), hiBuf;
        for (int b = 0; b < 45; ++b) {
            if (b == 8) engine.noteOff(60);
            std::fill(bL.begin(), bL.end(), 0.0f);
            std::fill(bR.begin(), bR.end(), 0.0f);
            engine.processBlock(bL.data(), bR.data(), kBlock);
            for (int i = 0; i < kBlock; ++i) {
                const float hi = hp2.processSample(hp1.processSample(bL[static_cast<std::size_t>(i)]));
                if (b >= 15) hiBuf.push_back(hi);
            }
        }
        return computeRms(hiBuf.data(), static_cast<int>(hiBuf.size()));
    };

    const float revDark = renderReverbTailAirRms(0.05f, 0.85f);
    const float revAiry = renderReverbTailAirRms(0.95f, 0.20f);
    EXPECT_GT(revAiry, revDark * 1.50f);
}

TEST(NovaSynthAirAndNoiseTests, StereoNoiseTransientDecorrelationAndModel2AirBand) {
    constexpr double kSr = 48000.0;
    constexpr int kN = 16384;

    for (int model = 0; model < 4; ++model) {
        nova::dsp::MultiEngineOscillator osc;
        osc.prepare(kSr);
        nova::dsp::MultiEngineOscillator::Parameters p{};
        p.enabled = true;
        p.sourceType = 3;
        p.engine = nova::dsp::MultiEngineOscillator::EngineType::NoiseTransient;
        p.modelIndex = model;
        p.shapeMorph = 0.5f;
        p.level = 1.0f;
        osc.setParameters(p);
        osc.triggerNote(261.63f, 0.9f, 17u);

        std::vector<float> bufL(kN), bufR(kN);
        for (int i = 0; i < kN; ++i) {
            osc.processSample(261.63f, 0.0f, 0.0f, bufL[static_cast<std::size_t>(i)], bufR[static_cast<std::size_t>(i)]);
            ASSERT_TRUE(std::isfinite(bufL[static_cast<std::size_t>(i)]) && std::isfinite(bufR[static_cast<std::size_t>(i)]));
            ASSERT_LE(std::abs(bufL[static_cast<std::size_t>(i)]), 4.0f);
        }
        EXPECT_GT(computeRms(bufL.data(), kN), 0.01f);
        EXPECT_GT(computeRms(bufR.data(), kN), 0.01f);

        const int offset = (model == 3) ? 4096 : 0;
        const float corr = computePearsonCorr(bufL.data() + offset, bufR.data() + offset, kN - offset);
        EXPECT_LT(std::abs(corr), 0.15f) << "Noise model " << model << " must be stereo decorrelated!";
    }

    auto measureAirBands = [&](float shapeVal) {
        nova::dsp::MultiEngineOscillator osc;
        osc.prepare(kSr);
        nova::dsp::MultiEngineOscillator::Parameters p{};
        p.enabled = true; p.sourceType = 3; p.engine = nova::dsp::MultiEngineOscillator::EngineType::NoiseTransient;
        p.modelIndex = 2; p.shapeMorph = shapeVal; p.level = 1.0f;
        osc.setParameters(p);
        osc.triggerNote(261.63f, 0.9f, 99u);

        nova::dsp::StateVariableFilter lpLow, hpAir, lpAir, hpHi;
        lpLow.prepare(kSr); hpAir.prepare(kSr); lpAir.prepare(kSr); hpHi.prepare(kSr);
        lpLow.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, 1500.0f, 0.7071f);
        hpAir.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, 4000.0f, 0.7071f);
        lpAir.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, 14000.0f, 0.7071f);
        hpHi.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, 8500.0f, 0.7071f);

        std::vector<float> lowB(kN), airB(kN), hiB(kN);
        for (int i = 0; i < kN; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(261.63f, 0.0f, 0.0f, l, r);
            lowB[static_cast<std::size_t>(i)] = lpLow.processSample(l);
            airB[static_cast<std::size_t>(i)] = lpAir.processSample(hpAir.processSample(l));
            hiB[static_cast<std::size_t>(i)]  = hpHi.processSample(l);
        }
        return std::array<float, 3>{ computeRms(lowB.data(), kN), computeRms(airB.data(), kN), computeRms(hiB.data(), kN) };
    };

    const auto lowShape = measureAirBands(0.10f);
    const auto hiShape  = measureAirBands(0.90f);
    EXPECT_GT(lowShape[1], lowShape[0] * 4.0f);
    EXPECT_GT(hiShape[1], hiShape[0] * 4.0f);
    EXPECT_GT(hiShape[2] / std::max(1e-5f, hiShape[1]), lowShape[2] / std::max(1e-5f, lowShape[1]) * 1.15f);
}

TEST(NovaSynthAirAndNoiseTests, HostileInputsAndAcousticInvariantsForNewFeatures) {
    for (double sr : { 22050.0, 44100.0, 96000.0, 192000.0 }) {
        for (int bSize : { 1, 13, 64, 512 }) {
            nova::synth::NovaSynthProcessor proc;
            proc.prepareToPlay(sr, bSize);
            setProcParam(proc, "activeOscCount", 2.0f);
            setProcParam(proc, "osc1_enabled", 1.0f);
            setProcParam(proc, "osc1_fine", 50.0f);
            setProcParam(proc, "osc2_enabled", 1.0f);
            setProcParam(proc, "osc2_sourceType", 3.0f);
            setProcParam(proc, "osc2_model", 2.0f);
            setProcParam(proc, "osc2_filterRouting", 3.0f);
            setProcParam(proc, "osc2_level", 0.35f);
            setProcParam(proc, "filterType", 3.0f);
            setProcParam(proc, "filterKeyTrack", 1.0f);
            setProcParam(proc, "filter2KeyTrack", 1.0f);
            setProcParam(proc, "analogDrift", 1.0f);
            setProcParam(proc, "macroAir", 1.0f);
            setProcParam(proc, "reverbMix", 0.55f);
            setProcParam(proc, "ampAttack", 0.002f);
            proc.syncParametersFromAPVTS();

            juce::AudioBuffer<float> buf(2, bSize);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(105)), 0);
            midi.addEvent(juce::MidiMessage::noteOn(1, 67, static_cast<juce::uint8>(105)), 0);

            double sumSq = 0.0;
            const int blocks = std::max(12, static_cast<int>((sr * 0.05) / bSize));
            for (int b = 0; b < blocks; ++b) {
                proc.processBlock(buf, midi);
                midi.clear();
                for (int ch = 0; ch < 2; ++ch) {
                    const float* r = buf.getReadPointer(ch);
                    for (int i = 0; i < bSize; ++i) {
                        ASSERT_TRUE(std::isfinite(r[i]));
                        ASSERT_LE(std::abs(r[i]), 4.0f);
                        sumSq += static_cast<double>(r[i]) * static_cast<double>(r[i]);
                    }
                }
            }
            EXPECT_GT(sumSq, 1e-4);

            proc.triggerPanic();
            for (int b = 0; b < std::max(8, static_cast<int>((sr * 0.02) / bSize)); ++b) {
                proc.processBlock(buf, midi);
            }
            EXPECT_LT(std::max(buf.getMagnitude(0, 0, bSize), buf.getMagnitude(1, 0, bSize)), 1e-3f);
        }
    }
}
