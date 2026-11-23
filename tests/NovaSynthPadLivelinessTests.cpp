#include <gtest/gtest.h>
#include "NovaSynthProcessor.h"
#include "ui/SourceDeck.h"
#include "nova_dsp/synth/WorshipSynthEngine.h"
#include "nova_dsp/modulation/AnalogDriftGenerator.h"
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

[[nodiscard]] float measureHannPhasorPhase(const float* buf, int n, int tStart, double sr, float freqHz) noexcept {
    double real = 0.0, imag = 0.0;
    const double omega = 2.0 * std::numbers::pi_v<double> * static_cast<double>(freqHz) / sr;
    const double denom = static_cast<double>(std::max(1, n - 1));
    for (int i = 0; i < n; ++i) {
        const double w = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi_v<double> * static_cast<double>(i) / denom));
        const double ang = omega * static_cast<double>(tStart + i);
        const double s = static_cast<double>(buf[i]) * w;
        real += s * std::cos(ang);
        imag -= s * std::sin(ang);
    }
    return static_cast<float>(std::atan2(imag, real));
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

TEST(NovaSynthPadLivelinessTests, Low12SmoothRolloffAndPresetIndexCompatibility) {
    nova::synth::NovaSynthProcessor proc;
    auto* fltChoice = dynamic_cast<juce::AudioParameterChoice*>(proc.getAPVTS().getParameter("filterType"));
    ASSERT_NE(fltChoice, nullptr);
    ASSERT_EQ(fltChoice->choices.size(), 4);
    EXPECT_TRUE(fltChoice->choices[0].contains("LOW 24"));
    EXPECT_TRUE(fltChoice->choices[1].contains("BAND 12"));
    EXPECT_TRUE(fltChoice->choices[2].contains("HIGH 24"));
    EXPECT_TRUE(fltChoice->choices[3].contains("LOW 12"));

    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;
    auto renderType = [&](bool fltOn, int fltType) {
        nova::dsp::WorshipSynthEngine engine;
        engine.prepare(kSr, kBlock);
        auto p = engine.getParameters();
        configureCleanEngine(p);
        p.filter1Enabled = fltOn;
        p.filterType = fltType;
        p.filterCutoffHz = 261.63f;
        engine.setParameters(p);
        engine.snapSmoothersToTarget();
        engine.noteOn(60, 0.9f); // 261.63 Hz saw

        std::vector<float> bL(kBlock), bR(kBlock), tail(4096);
        for (int b = 0; b < 16; ++b) {
            std::fill(bL.begin(), bL.end(), 0.0f);
            std::fill(bR.begin(), bR.end(), 0.0f);
            engine.processBlock(bL.data(), bR.data(), kBlock);
            if (b >= 8) {
                std::copy(bL.begin(), bL.end(), tail.begin() + (b - 8) * kBlock);
            }
        }
        return tail;
    };

    const auto bypass = renderType(false, 0);
    const auto lp12   = renderType(true, 3);
    const auto lp24   = renderType(true, 0);

    const float mag2fcBypass = measureDftMag(bypass.data(), 4096, kSr, 523.25f);
    const float mag2fcLp12   = measureDftMag(lp12.data(),   4096, kSr, 523.25f);
    const float mag2fcLp24   = measureDftMag(lp24.data(),   4096, kSr, 523.25f);
    const float db2fcLp12    = 20.0f * std::log10(mag2fcLp12 / std::max(1e-6f, mag2fcBypass));
    const float db2fcLp24    = 20.0f * std::log10(mag2fcLp24 / std::max(1e-6f, mag2fcBypass));

    EXPECT_GT(db2fcLp12, -14.5f);
    EXPECT_LT(db2fcLp12, -9.5f);
    EXPECT_GT(db2fcLp24, -27.5f);
    EXPECT_LT(db2fcLp24, -21.0f);

    const float mag4fcLp12 = measureDftMag(lp12.data(), 4096, kSr, 1046.5f);
    const float mag4fcLp24 = measureDftMag(lp24.data(), 4096, kSr, 1046.5f);
    EXPECT_GT(mag4fcLp12, mag4fcLp24 * 6.0f);
}

TEST(NovaSynthPadLivelinessTests, FilterKeyTrackingRaisesCutoffForHighNotes) {
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    auto renderNoteRms = [&](int midiNote, float keyTrack1, float keyTrack2, bool useFlt2) {
        nova::synth::NovaSynthProcessor proc;
        proc.prepareToPlay(kSr, kBlock);
        setProcParam(proc, "activeOscCount", 1.0f);
        setProcParam(proc, "osc1_enabled", 1.0f);
        setProcParam(proc, "osc1_sourceType", 0.0f);
        setProcParam(proc, "osc1_engine", 0.0f);
        setProcParam(proc, "osc1_model", 0.0f);
        setProcParam(proc, "osc1_detune", 0.0f);
        setProcParam(proc, "osc1_level", 0.9f);
        setProcParam(proc, "osc1_filterRouting", useFlt2 ? 1.0f : 0.0f);
        setProcParam(proc, "osc2_enabled", 0.0f);
        setProcParam(proc, "osc3_enabled", 0.0f);
        setProcParam(proc, "filter1Enabled", useFlt2 ? 0.0f : 1.0f);
        setProcParam(proc, "filterType", 3.0f);
        setProcParam(proc, "filterCutoffHz", 400.0f);
        setProcParam(proc, "filterResonance", 0.71f);
        setProcParam(proc, "filterEnvAmount", 0.0f);
        setProcParam(proc, "filterKeyTrack", keyTrack1);
        setProcParam(proc, "filter2Enabled", useFlt2 ? 1.0f : 0.0f);
        setProcParam(proc, "filter2Type", 0.0f);
        setProcParam(proc, "filter2CutoffHz", 400.0f);
        setProcParam(proc, "filter2Resonance", 0.71f);
        setProcParam(proc, "filter2EnvAmount", 0.0f);
        setProcParam(proc, "filter2KeyTrack", keyTrack2);
        setProcParam(proc, "macroWarmth", 0.50f);
        setProcParam(proc, "macroAir", 0.30f);
        setProcParam(proc, "analogDrift", 0.0f);
        setProcParam(proc, "chorusEnabled", 0.0f);
        setProcParam(proc, "delayEnabled", 0.0f);
        setProcParam(proc, "reverbMix", 0.0f);
        setProcParam(proc, "ampAttack", 0.002f);
        setProcParam(proc, "ampSustain", 1.0f);
        proc.syncParametersFromAPVTS();

        juce::AudioBuffer<float> buf(2, kBlock);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, midiNote, static_cast<juce::uint8>(105)), 0);
        double sumSq = 0.0;
        int n = 0;
        for (int b = 0; b < 12; ++b) {
            proc.processBlock(buf, midi);
            midi.clear();
            if (b >= 6) {
                const float* l = buf.getReadPointer(0);
                for (int i = 0; i < kBlock; ++i) {
                    sumSq += static_cast<double>(l[i]) * static_cast<double>(l[i]);
                    ++n;
                }
            }
        }
        return static_cast<float>(std::sqrt(sumSq / static_cast<double>(std::max(1, n))));
    };

    const float c2Kt0 = renderNoteRms(36, 0.0f, 0.0f, false);
    const float c6Kt0 = renderNoteRms(84, 0.0f, 0.0f, false);
    const float c2Kt1 = renderNoteRms(36, 1.0f, 0.0f, false);
    const float c6Kt1 = renderNoteRms(84, 1.0f, 0.0f, false);

    EXPECT_GT(c6Kt1, c6Kt0 * 3.0f) << "Filter 1 key-tracking must open cutoff for C6!";
    EXPECT_LT(c2Kt1, c2Kt0 * 0.85f) << "Filter 1 key-tracking must lower cutoff below C4!";

    const float f2C6Kt0 = renderNoteRms(84, 0.0f, 0.0f, true);
    const float f2C6Kt1 = renderNoteRms(84, 0.0f, 1.0f, true);
    EXPECT_GT(f2C6Kt1, f2C6Kt0 * 2.5f) << "Filter 2 key-tracking must open cutoff for C6!";
}

TEST(NovaSynthPadLivelinessTests, PerVoiceAnalogDriftCreatesOrganicChordVariationWithoutClicks) {
    nova::dsp::AnalogDriftGenerator dA, dB;
    dA.prepare(48000.0);
    dB.prepare(48000.0);
    dA.trigger(101u, true);
    dB.trigger(202u, true);

    EXPECT_FLOAT_EQ(dA.getPitchDriftCents(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(dA.getCutoffDriftFactor(0.0f), 1.0f);

    std::vector<float> pA, pB, cA;
    for (int i = 0; i < 2000; ++i) {
        dA.stepControlRate(16);
        dB.stepControlRate(16);
        const float pa = dA.getPitchDriftCents(1.0f);
        const float pb = dB.getPitchDriftCents(1.0f);
        const float ca = dA.getCutoffDriftFactor(1.0f);
        ASSERT_GE(pa, -6.0f); ASSERT_LE(pa, 6.0f);
        ASSERT_GE(ca, 0.94f); ASSERT_LE(ca, 1.06f);
        pA.push_back(pa); pB.push_back(pb); cA.push_back(ca);
    }
    EXPECT_LT(std::abs(computePearsonCorr(pA.data(), pB.data(), static_cast<int>(pA.size()))), 0.70f);
    EXPECT_LT(std::abs(computePearsonCorr(pA.data(), cA.data(), static_cast<int>(pA.size()))), 0.70f);

    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;
    auto measureChordDriftSpread = [&](float driftVal) -> float {
        nova::dsp::WorshipSynthEngine engine;
        engine.prepare(kSr, kBlock);
        auto p = engine.getParameters();
        configureCleanEngine(p);
        p.oscSlots[0].modelIndex = 3; // Pure sine
        p.filterCutoffHz = 1800.0f;
        p.analogDrift = driftVal;
        engine.setParameters(p);
        engine.snapSmoothersToTarget();
        for (int note : { 60, 64, 67, 71 }) {
            engine.noteOn(note, 0.8f);
        }

        std::vector<float> bL(kBlock), bR(kBlock), window;
        float maxStep = 0.0f, prev = 0.0f, peak = 0.0f;
        bool allFinite = true;
        std::array<float, 4> c4Phases{};
        int winStartSample = 14 * kBlock;
        for (int b = 0; b < 110; ++b) {
            std::fill(bL.begin(), bL.end(), 0.0f);
            std::fill(bR.begin(), bR.end(), 0.0f);
            engine.processBlock(bL.data(), bR.data(), kBlock);
            for (int i = 0; i < kBlock; ++i) {
                const float l = bL[static_cast<std::size_t>(i)];
                const float r = bR[static_cast<std::size_t>(i)];
                if (!std::isfinite(l) || !std::isfinite(r)) allFinite = false;
                peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
                if (b >= 10) maxStep = std::max(maxStep, std::abs(l - prev));
                prev = l;
            }
            if (b >= 14) {
                window.insert(window.end(), bL.begin(), bL.end());
                if (window.size() == 12288) {
                    const int slot = (b - 14) / 24;
                    if (slot < 4) {
                        c4Phases[static_cast<std::size_t>(slot)] =
                            measureHannPhasorPhase(window.data(), 12288, winStartSample, kSr, 261.6256f);
                    }
                    winStartSample += 12288;
                    window.clear();
                }
            }
        }
        EXPECT_TRUE(allFinite);
        EXPECT_LE(peak, 4.0f);
        EXPECT_LT(maxStep, 0.08f);
        float totalWander = 0.0f;
        for (std::size_t k = 0; k + 1 < c4Phases.size(); ++k) {
            const float diff = std::remainder(c4Phases[k + 1] - c4Phases[k], 2.0f * std::numbers::pi_v<float>);
            totalWander += std::abs(diff);
        }
        return totalWander;
    };

    const float phaseWanderOff = measureChordDriftSpread(0.0f);
    const float phaseWanderOn  = measureChordDriftSpread(1.0f);
    EXPECT_LT(phaseWanderOff, 0.03f);
    EXPECT_GT(phaseWanderOn, 0.12f) << "Analog drift should produce measurable slow pitch/timbre movement!";
}

TEST(NovaSynthPadLivelinessTests, OscFineCentsDetunesFundamentalAndRoundtripsState) {
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(kSr, kBlock);
    setProcParam(proc, "activeOscCount", 1.0f);
    setProcParam(proc, "osc1_enabled", 1.0f);
    setProcParam(proc, "osc1_sourceType", 0.0f);
    setProcParam(proc, "osc1_engine", 0.0f);
    setProcParam(proc, "osc1_model", 3.0f); // Sine
    setProcParam(proc, "osc1_shape", 0.0f);
    setProcParam(proc, "osc1_detune", 0.0f);
    setProcParam(proc, "osc1_fine", 35.0f);
    setProcParam(proc, "osc2_enabled", 0.0f);
    setProcParam(proc, "osc3_enabled", 0.0f);
    setProcParam(proc, "filter1Enabled", 0.0f);
    setProcParam(proc, "filter2Enabled", 0.0f);
    setProcParam(proc, "analogDrift", 0.0f);
    setProcParam(proc, "chorusEnabled", 0.0f);
    setProcParam(proc, "delayEnabled", 0.0f);
    setProcParam(proc, "reverbMix", 0.0f);
    setProcParam(proc, "ampAttack", 0.002f);
    setProcParam(proc, "ampSustain", 1.0f);
    proc.syncParametersFromAPVTS();

    juce::AudioBuffer<float> buf(2, kBlock);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(110)), 0);
    std::vector<float> audio;
    for (int b = 0; b < 40; ++b) {
        proc.processBlock(buf, midi);
        midi.clear();
        if (b >= 8) {
            const float* l = buf.getReadPointer(0);
            audio.insert(audio.end(), l, l + kBlock);
        }
    }

    const float detunedHz = 440.0f * std::pow(2.0f, 35.0f / 1200.0f);
    const float magDetuned = measureDftMag(audio.data(), static_cast<int>(audio.size()), kSr, detunedHz);
    const float mag440     = measureDftMag(audio.data(), static_cast<int>(audio.size()), kSr, 440.0f);
    EXPECT_GT(magDetuned, 0.10f);
    EXPECT_GT(magDetuned, mag440 * 4.0f);

    setProcParam(proc, "osc1_fine", -27.5f);
    setProcParam(proc, "osc2_fine", 41.3f);
    setProcParam(proc, "filterType", 3.0f);
    setProcParam(proc, "filterKeyTrack", 0.72f);
    setProcParam(proc, "filter2KeyTrack", 0.35f);
    setProcParam(proc, "analogDrift", 0.68f);

    juce::MemoryBlock state;
    proc.getStateInformation(state);

    nova::synth::NovaSynthProcessor proc2;
    proc2.prepareToPlay(kSr, kBlock);
    proc2.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    proc2.syncParametersFromAPVTS();

    const auto p2 = proc2.getCurrentParameters();
    EXPECT_NEAR(p2.oscSlots[0].fineCents, -27.5f, 0.15f);
    EXPECT_NEAR(p2.oscSlots[1].fineCents, 41.3f, 0.15f);
    EXPECT_EQ(p2.filterType, 3);
    EXPECT_NEAR(p2.filterKeyTrack, 0.72f, 0.02f);
    EXPECT_NEAR(p2.filter2KeyTrack, 0.35f, 0.02f);
    EXPECT_NEAR(p2.analogDrift, 0.68f, 0.02f);

    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::SourceDeck deck(proc2, theme);
    auto& card = *deck.getCardList()[0];
    card.setExpanded(true);
    card.setBounds(0, 0, 390, 180);
    EXPECT_TRUE(card.getFineKnob().isVisible());
    EXPECT_GT(card.getFineKnob().getWidth(), 20);
    EXPECT_FALSE(card.getSemiKnob().getBounds().intersects(card.getFineKnob().getBounds()));
    EXPECT_FALSE(card.getFineKnob().getBounds().intersects(card.getLevelKnob().getBounds()));
}
