#include <gtest/gtest.h>
#include "NovaSynthProcessor.h"
#include "nova_dsp/filters/StateVariableFilter.h"
#include "nova_dsp/reverb/DualEngineReverb.h"
#include "nova_dsp/synth/WorshipSynthAirStage.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

namespace {

[[nodiscard]] float computeRms(const float* buf, int n) noexcept {
    double sq = 0.0;
    for (int i = 0; i < n; ++i) {
        sq += static_cast<double>(buf[i]) * static_cast<double>(buf[i]);
    }
    return static_cast<float>(std::sqrt(sq / static_cast<double>(std::max(1, n))));
}

[[nodiscard]] float computePearsonCorr(const float* a, const float* b, int n) noexcept {
    double meanA = 0.0, meanB = 0.0;
    for (int i = 0; i < n; ++i) {
        meanA += a[i];
        meanB += b[i];
    }
    meanA /= static_cast<double>(std::max(1, n));
    meanB /= static_cast<double>(std::max(1, n));
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

std::pair<std::vector<float>, std::vector<float>> renderMonoSineBurstThroughReverb(
    double sr, int blockSize, float shimmerOct, int pitchMode, bool injectBadInput = false) {
    nova::dsp::DualEngineReverb reverb;
    reverb.prepare(sr, blockSize);
    nova::dsp::DualEngineReverb::Parameters params;
    params.engineMode = nova::dsp::DualEngineReverb::EngineMode::LushCloud;
    params.spaceType = nova::dsp::DualEngineReverb::SpaceType::CathedralCloud;
    params.decaySeconds = 8.0f;
    params.preDelayMs = 0.0f;
    params.dry = 0.0f;
    params.mix = 1.0f;
    params.shimmerOctave = shimmerOct;
    params.shimmerPitchMode = pitchMode;
    params.shimmerHighCutHz = 6500.0f;
    reverb.setParameters(params);

    const int totalSamples = static_cast<int>(sr * 2.0);
    const int burstSamples = static_cast<int>(sr * 0.6);
    std::vector<float> outL(static_cast<std::size_t>(totalSamples), 0.0f);
    std::vector<float> outR(static_cast<std::size_t>(totalSamples), 0.0f);

    for (int i = 0; i < burstSamples; ++i) {
        const float env = 0.5f * (1.0f - std::cos(2.0f * std::numbers::pi_v<float>
            * static_cast<float>(i) / static_cast<float>(std::max(1, burstSamples - 1))));
        const float val = 0.5f * env * std::sin(2.0f * std::numbers::pi_v<float> * 220.0f
            * static_cast<float>(i) / static_cast<float>(sr));
        outL[static_cast<std::size_t>(i)] = val;
        outR[static_cast<std::size_t>(i)] = val;
    }

    if (injectBadInput && totalSamples > 16) {
        outL[4] = std::numeric_limits<float>::quiet_NaN();
        outR[5] = std::numeric_limits<float>::infinity();
    }

    for (int off = 0; off < totalSamples; off += blockSize) {
        const int chunk = std::min(blockSize, totalSamples - off);
        reverb.processBlock(&outL[static_cast<std::size_t>(off)], &outR[static_cast<std::size_t>(off)], chunk);
    }
    return { std::move(outL), std::move(outR) };
}

} // namespace

TEST(DualEngineReverbShimmerCloudTests, DetunedStereoShimmerHasTrueStereoWidthAndLateTailBloom) {
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    const auto [baseL, baseR] = renderMonoSineBurstThroughReverb(kSr, kBlock, 0.0f, 0);
    const auto [shimL, shimR] = renderMonoSineBurstThroughReverb(kSr, kBlock, 0.75f, 0);

    const int total = static_cast<int>(baseL.size());
    std::vector<float> deltaL(static_cast<std::size_t>(total), 0.0f);
    std::vector<float> deltaR(static_cast<std::size_t>(total), 0.0f);
    std::vector<float> deltaHiL(static_cast<std::size_t>(total), 0.0f);
    std::vector<float> deltaHiR(static_cast<std::size_t>(total), 0.0f);

    nova::dsp::StateVariableFilter hpDeltaL, hpDeltaR;
    hpDeltaL.prepare(kSr);
    hpDeltaR.prepare(kSr);
    hpDeltaL.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, 350.0f, 0.7071f);
    hpDeltaR.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, 350.0f, 0.7071f);

    float maxPeak = 0.0f;
    for (int i = 0; i < total; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        ASSERT_TRUE(std::isfinite(shimL[idx]) && std::isfinite(shimR[idx]));
        maxPeak = std::max(maxPeak, std::max(std::abs(shimL[idx]), std::abs(shimR[idx])));
        deltaL[idx] = shimL[idx] - baseL[idx];
        deltaR[idx] = shimR[idx] - baseR[idx];
        deltaHiL[idx] = hpDeltaL.processSample(deltaL[idx]);
        deltaHiR[idx] = hpDeltaR.processSample(deltaR[idx]);
    }
    EXPECT_LT(maxPeak, 2.0f);

    // Late tail window: 0.5s .. 1.3s
    const int lateStart = static_cast<int>(kSr * 0.5);
    const int lateEnd = static_cast<int>(kSr * 1.3);
    const int lateCount = lateEnd - lateStart;

    double midSq = 0.0, sideSq = 0.0, lateHiSq = 0.0;
    for (int i = lateStart; i < lateEnd; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        const float mid = 0.5f * (deltaL[idx] + deltaR[idx]);
        const float side = 0.5f * (deltaL[idx] - deltaR[idx]);
        midSq += static_cast<double>(mid) * mid;
        sideSq += static_cast<double>(side) * side;
        lateHiSq += 0.5 * (static_cast<double>(deltaHiL[idx]) * deltaHiL[idx]
                         + static_cast<double>(deltaHiR[idx]) * deltaHiR[idx]);
    }
    const float midRms = static_cast<float>(std::sqrt(midSq / static_cast<double>(lateCount)));
    const float sideRms = static_cast<float>(std::sqrt(sideSq / static_cast<double>(lateCount)));
    const float lateHiRms = static_cast<float>(std::sqrt(lateHiSq / static_cast<double>(lateCount)));
    const float corrLR = computePearsonCorr(deltaL.data() + lateStart, deltaR.data() + lateStart, lateCount);

    EXPECT_GT(sideRms, 0.005f);
    EXPECT_GT(sideRms / (midRms + 1e-9f), 0.35f);
    EXPECT_LT(std::abs(corrLR), 0.75f);

    // Early attack window: 0.00s .. 0.10s
    const int earlyCount = static_cast<int>(kSr * 0.10);
    double earlyHiSq = 0.0, earlyBaseSq = 0.0;
    for (int i = 0; i < earlyCount; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        earlyHiSq += 0.5 * (static_cast<double>(deltaHiL[idx]) * deltaHiL[idx]
                          + static_cast<double>(deltaHiR[idx]) * deltaHiR[idx]);
        earlyBaseSq += 0.5 * (static_cast<double>(baseL[idx]) * baseL[idx]
                            + static_cast<double>(baseR[idx]) * baseR[idx]);
    }
    const float earlyHiRms = static_cast<float>(std::sqrt(earlyHiSq / static_cast<double>(earlyCount)));
    const float earlyBaseRms = static_cast<float>(std::sqrt(earlyBaseSq / static_cast<double>(earlyCount)));

    EXPECT_GT(lateHiRms, earlyHiRms * 4.0f);
    EXPECT_LT(earlyHiRms, earlyBaseRms * 0.50f);

    // Hostile sample rates, odd buffer sizes, all pitch modes, and NaN/Inf recovery
    for (int mode = 0; mode <= 3; ++mode) {
        for (double sr : { 22050.0, 44100.0, 96000.0, 192000.0 }) {
            const int bSize = (mode == 0) ? 1 : ((mode == 1) ? 13 : ((mode == 2) ? 64 : 2048));
            const auto [hL, hR] = renderMonoSineBurstThroughReverb(sr, bSize, 0.80f, mode, mode == 2);
            float hPeak = 0.0f;
            for (std::size_t i = 0; i < hL.size(); ++i) {
                ASSERT_TRUE(std::isfinite(hL[i]) && std::isfinite(hR[i])) << "mode=" << mode << " sr=" << sr;
                hPeak = std::max(hPeak, std::max(std::abs(hL[i]), std::abs(hR[i])));
            }
            EXPECT_LE(hPeak, 4.0f) << "mode=" << mode << " sr=" << sr;
            EXPECT_GT(computeRms(hL.data() + static_cast<int>(sr * 0.5), static_cast<int>(sr * 0.5)), 1e-4f);
        }
    }
}

TEST(DualEngineReverbShimmerCloudTests, WorshipSynthAirStageCapsShimmerHighCutToSilkyRange) {
    nova::dsp::DualEngineReverb::Parameters revParams;
    float prevCut = 0.0f;
    for (float air : { -0.5f, 0.0f, 0.35f, 0.70f, 1.0f, 1.5f }) {
        for (float warmth : { 0.0f, 0.5f, 1.0f }) {
            nova::dsp::WorshipSynthAirStage::applyReverbAirCoupling(air, warmth, revParams);
            EXPECT_GE(revParams.shimmerHighCutHz, 4800.0f);
            EXPECT_LE(revParams.shimmerHighCutHz, 8200.0f);
        }
        nova::dsp::WorshipSynthAirStage::applyReverbAirCoupling(air, 0.5f, revParams);
        EXPECT_GE(revParams.shimmerHighCutHz, prevCut);
        prevCut = revParams.shimmerHighCutHz;
    }
}

TEST(DualEngineReverbShimmerCloudTests, AllEightPadsCloudAestheticsHeadroomSwellAndNaming) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    auto& pm = proc.getPresetManager();
    const auto& presets = pm.getAllPresets();

    const std::array<const char*, 8> forbidden = {
        "worship", "bethel", "hillsong", "elevation", "valhalla", "jorna", "strymon", "nord"
    };

    int padCount = 0;
    for (int idx = 0; idx < static_cast<int>(presets.size()); ++idx) {
        const auto& preset = presets[static_cast<std::size_t>(idx)];
        if (!preset.isFactory || preset.category == "Utility") continue;
        ++padCount;

        juce::String text = preset.name + " " + preset.description;
        for (const auto& tag : preset.tags) text += " " + tag;
        const juce::String lower = text.toLowerCase();
        for (const char* term : forbidden) {
            EXPECT_FALSE(lower.contains(term)) << term << " in " << preset.name.toStdString();
        }

        pm.loadPresetByIndex(idx);
        proc.syncParametersFromAPVTS();
        const auto p = proc.getCurrentParameters();
        const std::string name = preset.name.toStdString();

        EXPECT_NEAR(p.macroShimmer, 0.0f, 1e-5f) << name;
        EXPECT_LE(p.reverbDucking, 0.08f + 1e-5f) << name;

        if (preset.category == "Warm Pads") {
            EXPECT_FLOAT_EQ(p.reverbShimmer, 0.0f) << name;
            EXPECT_GE(p.reverbDry, 0.36f - 1e-4f) << name;
            EXPECT_LE(p.reverbDry, 0.48f + 1e-4f) << name;
            EXPECT_GE(p.reverbMix, 0.72f - 1e-4f) << name;
            EXPECT_LE(p.reverbMix, 0.86f + 1e-4f) << name;
            EXPECT_GE(p.ampEnv.attackSec, 0.70f - 1e-4f) << name;
            EXPECT_LE(p.ampEnv.attackSec, 1.05f + 1e-4f) << name;
            EXPECT_GE(p.ampEnv.releaseSec, 2.80f - 1e-4f) << name;
            EXPECT_LE(p.ampEnv.releaseSec, 3.80f + 1e-4f) << name;
        } else {
            EXPECT_EQ(preset.category, "Airy & Shimmer") << name;
            EXPECT_GE(p.reverbShimmer, 0.04f - 1e-4f) << name;
            EXPECT_LE(p.reverbShimmer, 0.14f + 1e-4f) << name;

            for (int s = 0; s < p.activeOscCount; ++s) {
                const auto& slot = p.oscSlots[static_cast<std::size_t>(s)];
                if (slot.enabled && slot.engine == nova::dsp::MultiEngineOscillator::EngineType::OrganicTexture) {
                    EXPECT_GE(slot.warp, 0.0f) << name << " slot=" << s;
                    EXPECT_LE(slot.warp, 0.10f + 1e-4f) << name << " slot=" << s;
                }
            }

            bool hasModWheelShim = false;
            for (std::size_t m = 0; m < nova::dsp::ModMatrix::kMaxSlots; ++m) {
                const auto& sl = proc.getModMatrix().getSlot(m);
                if (sl.enabled
                    && sl.source == nova::dsp::ModMatrix::Source::ModWheel
                    && sl.destination == nova::dsp::ModMatrix::Destination::ShimmerAmount) {
                    EXPECT_GE(sl.amount, 0.20f - 1e-4f) << name;
                    EXPECT_LE(sl.amount, 0.30f + 1e-4f) << name;
                    hasModWheelShim = true;
                }
            }
            EXPECT_TRUE(hasModWheelShim) << name;
        }

        // Swell dynamics & headroom across CC11 = 0, 64, 127
        float rmsLow = 0.0f, rmsHigh = 0.0f;
        for (int cc11 : { 0, 64, 127 }) {
            pm.loadPresetByIndex(idx);
            proc.triggerPanic();
            proc.setModWheelFromUI(static_cast<float>(cc11) / 127.0f);

            juce::AudioBuffer<float> buf(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 11, cc11), 0);
            for (int note : { 48, 55, 60, 64, 67 }) {
                midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(96)), 0);
            }

            float peak = 0.0f;
            double sq = 0.0;
            int cnt = 0;
            for (int b = 0; b < 140; ++b) {
                buf.clear();
                proc.processBlock(buf, midi);
                midi.clear();
                const float* l = buf.getReadPointer(0);
                const float* r = buf.getReadPointer(1);
                for (int i = 0; i < 512; ++i) {
                    ASSERT_TRUE(std::isfinite(l[i]) && std::isfinite(r[i])) << name;
                    peak = std::max(peak, std::max(std::abs(l[i]), std::abs(r[i])));
                    if (b >= 100) {
                        sq += 0.5 * (static_cast<double>(l[i]) * l[i] + static_cast<double>(r[i]) * r[i]);
                        ++cnt;
                    }
                }
            }
            const float rms = static_cast<float>(std::sqrt(sq / static_cast<double>(std::max(1, cnt))));
            EXPECT_LT(peak, 1.5f) << name << " cc11=" << cc11;
            EXPECT_GT(rms, 0.002f) << name << " cc11=" << cc11;
            if (cc11 == 0) rmsLow = rms;
            if (cc11 == 127) rmsHigh = rms;
        }
        EXPECT_GT(rmsHigh, rmsLow * 2.5f) << name;

        proc.triggerPanic();
        juce::AudioBuffer<float> pBuf(2, 512);
        juce::MidiBuffer pMidi;
        for (int b = 0; b < 8; ++b) proc.processBlock(pBuf, pMidi);
        EXPECT_LT(std::max(pBuf.getMagnitude(0, 0, 512), pBuf.getMagnitude(1, 0, 512)), 1e-3f) << name;
    }
    EXPECT_EQ(padCount, 8);

    for (const char* roundtripPreset : { "Vocal Mist Halo", "Silky Bowed Strings" }) {
        ASSERT_TRUE(pm.loadPresetByName(roundtripPreset));
        proc.syncParametersFromAPVTS();
        const auto expParams = proc.getCurrentParameters();
        juce::MemoryBlock state;
        proc.getStateInformation(state);

        nova::synth::NovaSynthProcessor restored;
        restored.prepareToPlay(48000.0, 512);
        restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        restored.syncParametersFromAPVTS();
        const auto gotParams = restored.getCurrentParameters();
        EXPECT_NEAR(gotParams.reverbShimmer, expParams.reverbShimmer, 1e-4f) << roundtripPreset;
        EXPECT_NEAR(gotParams.reverbMix, expParams.reverbMix, 1e-4f) << roundtripPreset;
        EXPECT_NEAR(gotParams.ampEnv.releaseSec, expParams.ampEnv.releaseSec, 1e-3f) << roundtripPreset;
    }
}
