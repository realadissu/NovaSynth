#include <gtest/gtest.h>
#include "NovaSynthProcessor.h"
#include "nova_dsp/filters/StateVariableFilter.h"
#include "nova_dsp/oscillators/MultiEngineOscillator.h"
#include "nova_dsp/oscillators/PadSynthBank.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace {

[[nodiscard]] float computePearsonCorr(const float* a, const float* b, int n) noexcept {
    double meanA = 0.0, meanB = 0.0;
    for (int i = 0; i < n; ++i) {
        meanA += static_cast<double>(a[i]);
        meanB += static_cast<double>(b[i]);
    }
    meanA /= static_cast<double>(n);
    meanB /= static_cast<double>(n);
    double num = 0.0, denA = 0.0, denB = 0.0;
    for (int i = 0; i < n; ++i) {
        const double da = static_cast<double>(a[i]) - meanA;
        const double db = static_cast<double>(b[i]) - meanB;
        num += da * db;
        denA += da * da;
        denB += db * db;
    }
    const double denom = std::sqrt(denA * denB);
    return (denom > 1e-12) ? static_cast<float>(num / denom) : 0.0f;
}

[[nodiscard]] float measureNarrowbandRms(const float* buf, int n, double sr, float centerHz, float halfWidthHz) noexcept {
    const double df = sr / static_cast<double>(n);
    const int kMin = std::max(1, static_cast<int>(std::floor((centerHz - halfWidthHz) / df)));
    const int kMax = std::min(n / 2 - 1, static_cast<int>(std::ceil((centerHz + halfWidthHz) / df)));
    double powerSum = 0.0;
    for (int k = kMin; k <= kMax; ++k) {
        double real = 0.0, imag = 0.0;
        const double omega = 2.0 * std::numbers::pi_v<double> * static_cast<double>(k) / static_cast<double>(n);
        for (int i = 0; i < n; ++i) {
            const double ang = omega * static_cast<double>(i);
            real += static_cast<double>(buf[i]) * std::cos(ang);
            imag -= static_cast<double>(buf[i]) * std::sin(ang);
        }
        const double mag = 2.0 * std::hypot(real, imag) / static_cast<double>(n);
        powerSum += 0.5 * mag * mag;
    }
    return static_cast<float>(std::sqrt(powerSum));
}

[[nodiscard]] nova::dsp::MultiEngineOscillator::Parameters makePadParams(
    int model, float shape = 0.5f, float detune = 0.25f, float spread = 1.0f, float warp = 0.0f) noexcept {
    nova::dsp::MultiEngineOscillator::Parameters p{};
    p.enabled = true; p.engine = nova::dsp::MultiEngineOscillator::EngineType::OrganicTexture;
    p.modelIndex = model; p.shapeMorph = shape; p.detune = detune;
    p.scatter = 0.25f; p.spread = spread; p.warp = warp; p.level = 1.0f;
    return p;
}

[[nodiscard]] std::vector<float> renderMonoPad(
    const nova::dsp::MultiEngineOscillator::Parameters& p, float freqHz, double sr, int numSamples, uint32_t seed) {
    nova::dsp::MultiEngineOscillator osc;
    osc.prepare(sr); osc.setParameters(p); osc.triggerNote(freqHz, 0.9f, seed);
    std::vector<float> mono(static_cast<std::size_t>(numSamples));
    for (int i = 0; i < numSamples; ++i) {
        float l = 0.0f, r = 0.0f;
        osc.processSample(freqHz, 0.0f, 0.0f, l, r);
        mono[static_cast<std::size_t>(i)] = 0.5f * (l + r);
    }
    return mono;
}

struct BandpassFilter {
    nova::dsp::StateVariableFilter hp, lp;
    void prepare(double sr, float lowHz, float highHz) noexcept {
        hp.prepare(sr); lp.prepare(sr);
        hp.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, lowHz, 0.7071f);
        lp.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, highHz, 0.7071f);
    }
    [[nodiscard]] float process(float x) noexcept { return lp.processSample(hp.processSample(x)); }
};

struct ArchetypeSpectrum {
    std::array<float, 4> bandShare{};
    float centroidHz = 0.0f;
    float totalRms = 0.0f;
};

ArchetypeSpectrum measurePresetSpectrum(nova::synth::NovaSynthProcessor& proc, const char* presetName) {
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    EXPECT_TRUE(proc.getPresetManager().loadPresetByName(presetName)) << presetName;
    proc.triggerPanic();
    proc.setModWheelFromUI(0.55f);

    juce::AudioBuffer<float> buf(2, kBlock);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, 70), 0);
    for (int note : { 48, 55, 60, 64, 67 }) {
        midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(96)), 0);
    }

    for (int b = 0; b < 96; ++b) {
        buf.clear();
        proc.processBlock(buf, midi);
        midi.clear();
    }

    std::array<BandpassFilter, 4> bands;
    bands[0].prepare(kSr, 80.0f, 350.0f);
    bands[1].prepare(kSr, 350.0f, 900.0f);
    bands[2].prepare(kSr, 900.0f, 2400.0f);
    bands[3].prepare(kSr, 2400.0f, 10000.0f);

    std::array<double, 4> bandSq{};
    double totalSq = 0.0;
    int count = 0;

    for (int b = 0; b < 64; ++b) {
        buf.clear();
        proc.processBlock(buf, midi);
        const float* l = buf.getReadPointer(0);
        const float* r = buf.getReadPointer(1);
        for (int i = 0; i < kBlock; ++i) {
            const float m = 0.5f * (l[i] + r[i]);
            totalSq += static_cast<double>(m) * static_cast<double>(m);
            for (std::size_t k = 0; k < 4; ++k) {
                const float y = bands[k].process(m);
                bandSq[k] += static_cast<double>(y) * static_cast<double>(y);
            }
            ++count;
        }
    }

    const double sumBands = bandSq[0] + bandSq[1] + bandSq[2] + bandSq[3] + 1e-12;
    constexpr std::array<float, 4> kBandCenters = { 190.0f, 580.0f, 1500.0f, 5000.0f };
    ArchetypeSpectrum out;
    out.totalRms = static_cast<float>(std::sqrt(totalSq / static_cast<double>(std::max(1, count))));
    for (std::size_t k = 0; k < 4; ++k) {
        out.bandShare[k] = static_cast<float>(bandSq[k] / sumBands);
        out.centroidHz += kBandCenters[k] * out.bandShare[k];
    }
    return out;
}

} // namespace

TEST(PadSynthEngineTests, TablePeriodicityDcFreeRmsAndFiniteAcrossAllModelsAndZones) {
    const auto& bank = nova::dsp::getPadSynthBank();
    constexpr int kN = nova::dsp::PadSynthBank::kTableSize;

    for (int m = 0; m < nova::dsp::PadSynthBank::kNumModels; ++m) {
        for (int z = 0; z < nova::dsp::PadSynthBank::kNumZones; ++z) {
            const auto& zone = bank.getZone(m, z);
            ASSERT_EQ(zone.left.size(), static_cast<std::size_t>(kN));
            ASSERT_EQ(zone.right.size(), static_cast<std::size_t>(kN));

            for (const auto* ch : { zone.left.data(), zone.right.data() }) {
                double sum = 0.0, sumSq = 0.0;
                float peak = 0.0f, maxAdjDelta = 0.0f, maxSecondDiff = 0.0f;
                for (int i = 0; i < kN; ++i) {
                    const float s = ch[i];
                    ASSERT_TRUE(std::isfinite(s)) << "model=" << m << " zone=" << z;
                    peak = std::max(peak, std::abs(s));
                    sum += static_cast<double>(s);
                    sumSq += static_cast<double>(s) * static_cast<double>(s);
                    if (i > 0) maxAdjDelta = std::max(maxAdjDelta, std::abs(s - ch[i - 1]));
                    if (i > 0 && i < kN - 1) {
                        maxSecondDiff = std::max(maxSecondDiff, std::abs(ch[i + 1] - 2.0f * s + ch[i - 1]));
                    }
                }
                const float mean = static_cast<float>(sum / static_cast<double>(kN));
                const float rms = static_cast<float>(std::sqrt(sumSq / static_cast<double>(kN)));
                const float wrapDelta = std::abs(ch[0] - ch[kN - 1]);
                const float wrapSecondDiff = std::abs(ch[1] - 2.0f * ch[0] + ch[kN - 1]);

                EXPECT_LE(peak, 3.0f) << "model=" << m << " zone=" << z;
                EXPECT_LT(std::abs(mean), 1e-3f) << "model=" << m << " zone=" << z;
                EXPECT_NEAR(rms, nova::dsp::PadSynthBank::kTargetRms, 0.02f) << "model=" << m << " zone=" << z;
                EXPECT_LT(wrapDelta, std::max(0.05f, maxAdjDelta * 1.10f)) << "model=" << m << " zone=" << z;
                EXPECT_LT(wrapSecondDiff, std::max(0.05f, maxSecondDiff * 1.15f)) << "model=" << m << " zone=" << z;
            }
        }
    }
}

TEST(PadSynthEngineTests, TrueStereoDecorrelationAndControlsInBankAndOscillator) {
    const auto& bank = nova::dsp::getPadSynthBank();
    constexpr int kN = nova::dsp::PadSynthBank::kTableSize;

    for (int m = 0; m < nova::dsp::PadSynthBank::kNumModels; ++m) {
        for (int z = 0; z < nova::dsp::PadSynthBank::kNumZones; ++z) {
            const auto& zone = bank.getZone(m, z);
            const float corr = computePearsonCorr(zone.left.data(), zone.right.data(), kN);
            EXPECT_LT(std::abs(corr), 0.15f) << "Bank L/R correlated at model=" << m << " zone=" << z;
        }
    }

    constexpr double kSr = 48000.0;
    constexpr int kRenderN = 48000;
    for (int m = 0; m < nova::dsp::PadSynthBank::kNumModels; ++m) {
        nova::dsp::MultiEngineOscillator osc;
        osc.prepare(kSr);
        auto p = makePadParams(m, 0.55f, 0.30f, 1.0f);
        osc.setParameters(p);
        osc.triggerNote(261.63f, 0.9f, 100u + static_cast<uint32_t>(m));

        std::vector<float> bufL(kRenderN), bufR(kRenderN);
        for (int i = 0; i < kRenderN; ++i) {
            osc.processSample(261.63f, 0.0f, 0.0f, bufL[static_cast<std::size_t>(i)], bufR[static_cast<std::size_t>(i)]);
        }
        EXPECT_LT(computePearsonCorr(bufL.data(), bufR.data(), kRenderN), 0.35f) << "model=" << m;

        if (m == 0 || m == 1 || m == 3) {
            p.spread = 0.0f;
            osc.setParameters(p);
            osc.triggerNote(261.63f, 0.9f, 200u + static_cast<uint32_t>(m));
            std::fill(bufL.begin(), bufL.end(), 0.0f);
            std::fill(bufR.begin(), bufR.end(), 0.0f);
            for (int i = 0; i < kRenderN; ++i) {
                osc.processSample(261.63f, 0.0f, 0.0f, bufL[static_cast<std::size_t>(i)], bufR[static_cast<std::size_t>(i)]);
            }
            EXPECT_GT(computePearsonCorr(bufL.data(), bufR.data(), kRenderN), 0.99f) << "model=" << m;
        }
    }

    auto measureChoirF2Ratio = [&](float shapeMorph) {
        const auto mono = renderMonoPad(makePadParams(0, shapeMorph, 0.25f, 1.0f), 130.81f, kSr, kRenderN, 42u);
        BandpassFilter f2Band;
        f2Band.prepare(kSr, 1800.0f, 2500.0f);
        double sqTot = 0.0, sqF2 = 0.0;
        for (int i = 0; i < kRenderN; ++i) {
            const float s = mono[static_cast<std::size_t>(i)];
            const float f2 = f2Band.process(s);
            if (i >= 4000) {
                sqTot += static_cast<double>(s) * static_cast<double>(s);
                sqF2 += static_cast<double>(f2) * static_cast<double>(f2);
            }
        }
        return static_cast<float>(std::sqrt(sqF2 / (sqTot + 1e-12)));
    };
    EXPECT_GT(measureChoirF2Ratio(1.0f), measureChoirF2Ratio(0.0f) * 2.0f);

    auto renderTapeOctave = [&](float warpVal) {
        const auto mono = renderMonoPad(makePadParams(2, 0.65f, 0.20f, 1.0f, warpVal), 261.63f, kSr, 16384, 77u);
        return measureNarrowbandRms(mono.data(), 16384, kSr, 523.25f, 18.0f);
    };
    EXPECT_GT(renderTapeOctave(0.85f), renderTapeOctave(0.0f) * 1.35f);
}

TEST(PadSynthEngineTests, MeasurableSonicDiversityAcrossFourPresetArchetypes) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);

    const auto prophet = measurePresetSpectrum(proc, "Warm Prophet Pad");
    const auto strings = measurePresetSpectrum(proc, "Velvet Bowed Strings");
    const auto choir   = measurePresetSpectrum(proc, "Warm Mellotron Choir");
    const auto crystal = measurePresetSpectrum(proc, "Crystal Glass Cloud");

    for (const auto& s : { prophet, strings, choir, crystal }) {
        EXPECT_GT(s.totalRms, 0.01f);
    }

    EXPECT_GT(crystal.centroidHz, choir.centroidHz);
    EXPECT_GT(crystal.centroidHz, prophet.centroidHz);
    EXPECT_GT(crystal.bandShare[3], choir.bandShare[3] * 1.5f);
    EXPECT_GT(strings.bandShare[3], choir.bandShare[3]);

    const std::array<std::pair<const char*, ArchetypeSpectrum>, 4> items = {{
        { "Warm Prophet Pad", prophet },
        { "Velvet Bowed Strings", strings },
        { "Warm Mellotron Choir", choir },
        { "Crystal Glass Cloud", crystal }
    }};
    for (std::size_t i = 0; i < items.size(); ++i) {
        for (std::size_t j = i + 1; j < items.size(); ++j) {
            double distSq = 0.0;
            for (std::size_t b = 0; b < 4; ++b) {
                const double d = static_cast<double>(items[i].second.bandShare[b])
                               - static_cast<double>(items[j].second.bandShare[b]);
                distSq += d * d;
            }
            const float dist = static_cast<float>(std::sqrt(distSq));
            EXPECT_GT(dist, 0.06f)
                << items[i].first << " vs " << items[j].first << " are spectrally too close!";
        }
    }
}

TEST(PadSynthEngineTests, BandlimitedHighNotesNoOrganFifthsHostileRatesAndRoundtrip) {
    constexpr double kSr = 48000.0;
    constexpr int kN = 16384;

    for (int m = 0; m < nova::dsp::PadSynthBank::kNumModels; ++m) {
        for (float f0 : { 523.25f, 1046.50f }) {
            const auto mono = renderMonoPad(makePadParams(m, 0.55f, 0.30f, 1.0f, 0.30f), f0, kSr, kN, 55u);
            float peak = 0.0f;
            double sumSq = 0.0;
            for (float s : mono) {
                ASSERT_TRUE(std::isfinite(s));
                peak = std::max(peak, std::abs(s));
                sumSq += static_cast<double>(s) * static_cast<double>(s);
            }
            EXPECT_LE(peak, 3.0f);
            EXPECT_GT(std::sqrt(sumSq / kN), 0.05f);

            const float fundRms = measureNarrowbandRms(mono.data(), kN, kSr, f0, 22.0f);
            const float aliasProbe = measureNarrowbandRms(mono.data(), kN, kSr, f0 * 0.37f, 25.0f);
            EXPECT_LT(aliasProbe, fundRms * 0.12f) << "Aliasing foldback on model=" << m << " f0=" << f0;
        }

        const auto mono = renderMonoPad(makePadParams(m, 0.50f, 0.25f, 1.0f), 261.63f, kSr, kN, 33u);
        const float fundRms = measureNarrowbandRms(mono.data(), kN, kSr, 261.63f, 16.0f);
        const float fifthRms = measureNarrowbandRms(mono.data(), kN, kSr, 261.63f * 1.5f, 16.0f);
        EXPECT_GT(fundRms, 0.04f);
        EXPECT_LT(fifthRms, fundRms * 0.15f) << "Model " << m << " emitted forced 1.5x fifth";
    }

    for (const char* presetName : { "Velvet Bowed Strings", "Crystal Glass Cloud" }) {
        for (double sr : { 22050.0, 44100.0, 96000.0, 192000.0 }) {
            for (int bSize : { 1, 13, 64, 2048 }) {
                nova::synth::NovaSynthProcessor proc;
                proc.prepareToPlay(sr, bSize);
                ASSERT_TRUE(proc.getPresetManager().loadPresetByName(presetName));
                proc.triggerAuditionNoteOn(60, 0.90f);
                proc.triggerAuditionNoteOn(64, 0.90f);

                juce::AudioBuffer<float> buf(2, bSize);
                juce::MidiBuffer midi;
                const int blocks = std::max(8, static_cast<int>((sr * 0.10) / bSize));
                float peak = 0.0f;
                double sumSq = 0.0;
                for (int b = 0; b < blocks; ++b) {
                    proc.processBlock(buf, midi);
                    for (int ch = 0; ch < 2; ++ch) {
                        const float* ptr = buf.getReadPointer(ch);
                        for (int i = 0; i < bSize; ++i) {
                            ASSERT_TRUE(std::isfinite(ptr[i]));
                            peak = std::max(peak, std::abs(ptr[i]));
                            sumSq += static_cast<double>(ptr[i]) * static_cast<double>(ptr[i]);
                        }
                    }
                }
                EXPECT_LE(peak, 4.0f);
                EXPECT_GT(sumSq, 1e-4);

                proc.triggerPanic();
                const int panicBlocks = std::max(16, static_cast<int>((sr * 0.05) / bSize));
                for (int b = 0; b < panicBlocks; ++b) {
                    proc.processBlock(buf, midi);
                }
                EXPECT_LT(std::max(buf.getMagnitude(0, 0, bSize), buf.getMagnitude(1, 0, bSize)), 1e-3f);
            }
        }
    }

    nova::synth::NovaSynthProcessor procA;
    procA.prepareToPlay(48000.0, 512);
    ASSERT_TRUE(procA.getPresetManager().loadPresetByName("Cathedral Vocal Halo"));
    procA.syncParametersFromAPVTS();
    const auto paramsA = procA.getCurrentParameters();
    juce::MemoryBlock state;
    procA.getStateInformation(state);

    nova::synth::NovaSynthProcessor procB;
    procB.prepareToPlay(48000.0, 512);
    procB.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    procB.syncParametersFromAPVTS();
    const auto paramsB = procB.getCurrentParameters();
    for (int s : { 0, 1 }) {
        const auto& a = paramsA.oscSlots[static_cast<std::size_t>(s)];
        const auto& b = paramsB.oscSlots[static_cast<std::size_t>(s)];
        EXPECT_EQ(b.engine, nova::dsp::MultiEngineOscillator::EngineType::OrganicTexture);
        EXPECT_EQ(b.modelIndex, a.modelIndex);
        EXPECT_NEAR(b.warp, a.warp, 1e-3f);
        EXPECT_NEAR(b.rate, a.rate, 1e-3f);
        EXPECT_NEAR(b.scatter, a.scatter, 1e-3f);
        EXPECT_NEAR(b.spread, a.spread, 1e-3f);
        EXPECT_NEAR(b.shapeMorph, a.shapeMorph, 1e-3f);
        EXPECT_NEAR(b.detune, a.detune, 1e-3f);
    }
    EXPECT_GT(paramsB.oscSlots[0].warp, 0.10f);
    EXPECT_GT(paramsB.oscSlots[1].warp, 0.30f);
}
