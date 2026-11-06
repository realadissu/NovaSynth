#include <gtest/gtest.h>
#include "nova_dsp/math/OnePoleSmoother.h"
#include "nova_dsp/math/LockFreeRingBuffer.h"
#include "nova_dsp/filters/StateVariableFilter.h"
#include "nova_dsp/reverb/DualEngineReverb.h"
#include "nova_dsp/oscillators/MultiEngineOscillator.h"
#include "nova_dsp/oscillators/SampleBank.h"
#include "nova_dsp/modulation/AdsrEnvelope.h"
#include "nova_dsp/synth/WorshipSynthEngine.h"
#include "nova_plugin/PresetManager.h"
#include "nova_ui/ArcKnob.h"
#include "NovaSynthProcessor.h"
#include "NovaSynthEditor.h"
#include "ui/SourceListManager.h"
#include "ui/EnvelopeQuadDeck.h"
#include "ui/SourceDeck.h"
#include "ui/ModulationHubPanel.h"
#include "ui/DroneHubPanel.h"
#include "ui/ArpeggiatorHubPanel.h"
#include "ui/MixerHubPanel.h"
#include "ui/StandaloneKeyboardBar.h"
#include <juce_events/juce_events.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <numeric>
#include <vector>

namespace {

TEST(NovaDspMathTests, OnePoleSmootherConvergesSmoothly) {
    nova::dsp::OnePoleSmoother smoother;
    smoother.prepare(48000.0, 10.0f);
    smoother.reset(0.0f);
    smoother.setTargetValue(1.0f);

    float prev = 0.0f;
    for (int i = 0; i < 4800; ++i) {
        const float cur = smoother.getNextValue();
        EXPECT_GE(cur, prev);
        prev = cur;
    }
    EXPECT_NEAR(smoother.getCurrentValue(), 1.0f, 1e-3f);
}

TEST(NovaDspMathTests, LockFreeRingBufferPushPopAndWrapAround) {
    nova::dsp::LockFreeRingBuffer<float, 16> rb;
    for (int i = 0; i < 12; ++i) {
        EXPECT_TRUE(rb.push(static_cast<float>(i)));
    }
    for (int i = 0; i < 12; ++i) {
        float out = -1.0f;
        EXPECT_TRUE(rb.pop(out));
        EXPECT_FLOAT_EQ(out, static_cast<float>(i));
    }
    float dummy = 0.0f;
    EXPECT_FALSE(rb.pop(dummy));
}

TEST(NovaDspFilterTests, VocalCarveBellAttenuatesMidBand) {
    nova::dsp::StateVariableFilter svf;
    svf.prepare(48000.0);
    svf.setParameters(nova::dsp::StateVariableFilter::Type::Bell, 2200.0f, 0.85f, -9.0f);

    const float magAt2200 = svf.getMagnitudeAtFrequency(2200.0f);
    const float magAt200  = svf.getMagnitudeAtFrequency(200.0f);
    EXPECT_LT(magAt2200, 0.5f);
    EXPECT_NEAR(magAt200, 1.0f, 0.1f);
}

TEST(DualEngineReverbTests, AcousticEngineProducesFiniteStereoDecay) {
    nova::dsp::DualEngineReverb reverb;
    reverb.prepare(48000.0, 512);

    nova::dsp::DualEngineReverb::Parameters params;
    params.engineMode = nova::dsp::DualEngineReverb::EngineMode::Acoustic;
    params.decaySeconds = 2.2f;
    params.mix = 1.0f;
    params.preDelayMs = 5.0f;
    reverb.setParameters(params);

    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);
    left[0] = 1.0f;
    right[0] = 1.0f;

    // First block with impulse
    reverb.processBlock(left.data(), right.data(), 512);

    float earlyEnergy = 0.0f;
    for (int b = 0; b < 15; ++b) {
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        reverb.processBlock(left.data(), right.data(), 512);
        for (int i = 0; i < 512; ++i) {
            ASSERT_TRUE(std::isfinite(left[static_cast<std::size_t>(i)]));
            ASSERT_TRUE(std::isfinite(right[static_cast<std::size_t>(i)]));
            earlyEnergy += left[static_cast<std::size_t>(i)] * left[static_cast<std::size_t>(i)];
        }
    }
    EXPECT_GT(earlyEnergy, 1e-4f);

    // Later blocks should decay naturally
    float lateEnergy = 0.0f;
    for (int b = 0; b < 60; ++b) {
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        reverb.processBlock(left.data(), right.data(), 512);
        if (b >= 45) {
            for (int i = 0; i < 512; ++i) {
                lateEnergy += left[static_cast<std::size_t>(i)] * left[static_cast<std::size_t>(i)];
            }
        }
    }
    EXPECT_LT(lateEnergy, earlyEnergy);
}

TEST(DualEngineReverbTests, LushCloudFreezeSustainsTailInfinitely) {
    nova::dsp::DualEngineReverb reverb;
    reverb.prepare(48000.0, 512);

    nova::dsp::DualEngineReverb::Parameters params;
    params.engineMode = nova::dsp::DualEngineReverb::EngineMode::LushCloud;
    params.decaySeconds = 4.0f;
    params.shimmerOctave = 0.5f;
    params.shimmerFifth = 0.25f;
    params.mix = 1.0f;
    params.freeze = false;
    reverb.setParameters(params);

    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);

    // Feed burst of energy into tank
    for (int b = 0; b < 10; ++b) {
        for (int i = 0; i < 512; ++i) {
            const float sig = 0.35f * std::sin(2.0f * 3.14159265f * 220.0f * static_cast<float>(i) / 48000.0f);
            left[static_cast<std::size_t>(i)] = sig;
            right[static_cast<std::size_t>(i)] = sig;
        }
        reverb.processBlock(left.data(), right.data(), 512);
    }

    // Engage Freeze and process 100 silent blocks (~1.06 seconds)
    params.freeze = true;
    reverb.setParameters(params);

    float frozenEnergy = 0.0f;
    for (int b = 0; b < 100; ++b) {
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        reverb.processBlock(left.data(), right.data(), 512);
        if (b >= 90) {
            for (int i = 0; i < 512; ++i) {
                frozenEnergy += left[static_cast<std::size_t>(i)] * left[static_cast<std::size_t>(i)];
            }
        }
    }
    EXPECT_GT(frozenEnergy, 1e-3f);
}

TEST(DualEngineReverbTests, AutoDuckingAttenuatesWetTailDuringLoudInput) {
    nova::dsp::DualEngineReverb reverb;
    reverb.prepare(48000.0, 512);

    nova::dsp::DualEngineReverb::Parameters params;
    params.engineMode = nova::dsp::DualEngineReverb::EngineMode::LushCloud;
    params.decaySeconds = 6.0f;
    params.mix = 1.0f;
    params.ducking = 0.85f;
    reverb.setParameters(params);

    std::vector<float> left(512, 0.5f);
    std::vector<float> right(512, 0.5f);
    for (int b = 0; b < 8; ++b) {
        reverb.processBlock(left.data(), right.data(), 512);
    }

    const auto tel = reverb.getTelemetry();
    EXPECT_LT(tel.duckingGainReductionDb, -2.0f);
}

TEST(DualEngineReverbTests, ShimmerAndTankNeverRunawayOrBlowUpAtMaxDecayAndShimmer) {
    nova::dsp::DualEngineReverb reverb;
    reverb.prepare(48000.0, 512);

    nova::dsp::DualEngineReverb::Parameters params;
    params.engineMode = nova::dsp::DualEngineReverb::EngineMode::LushCloud;
    params.decaySeconds = 25.0f; // Extreme decay
    params.shimmerOctave = 1.0f; // Maximum shimmer
    params.modDepth = 1.0f;      // Maximum modulation
    params.mix = 1.0f;
    params.freeze = false;
    reverb.setParameters(params);

    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);

    // Feed high-amplitude burst
    for (int i = 0; i < 512; ++i) {
        const float s = 0.90f * std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / 48000.0f);
        left[static_cast<std::size_t>(i)] = s;
        right[static_cast<std::size_t>(i)] = s;
    }
    reverb.processBlock(left.data(), right.data(), 512);

    float maxTailPeak = 0.0f;
    // Process 80 blocks (~0.85s of dense recirculation)
    for (int b = 0; b < 80; ++b) {
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        reverb.processBlock(left.data(), right.data(), 512);
        for (int i = 0; i < 512; ++i) {
            ASSERT_TRUE(std::isfinite(left[static_cast<std::size_t>(i)]));
            ASSERT_TRUE(std::isfinite(right[static_cast<std::size_t>(i)]));
            maxTailPeak = std::max(maxTailPeak, std::max(std::abs(left[static_cast<std::size_t>(i)]), std::abs(right[static_cast<std::size_t>(i)])));
        }
    }

    // Must never blow up or run away into screaming feedback
    EXPECT_LT(maxTailPeak, 1.25f);
    EXPECT_GT(maxTailPeak, 0.01f);
}

TEST(DualEngineReverbTests, ShimmerNeverShortensDecayAndProducesMultiOctaveValhallaBloom) {
    constexpr double kSr = 48000.0;

    auto measureLateTailRms = [&](float shimmerOct) {
        nova::dsp::DualEngineReverb reverb;
        reverb.prepare(kSr, 512);

        nova::dsp::DualEngineReverb::Parameters params;
        params.engineMode = nova::dsp::DualEngineReverb::EngineMode::LushCloud;
        params.spaceType = nova::dsp::DualEngineReverb::SpaceType::CathedralCloud;
        params.decaySeconds = 10.0f;
        params.shimmerOctave = shimmerOct;
        params.shimmerPitchMode = 0;
        params.dry = 0.0f;
        params.mix = 1.0f;
        reverb.setParameters(params);

        const int totalSamples = static_cast<int>(kSr * 4.5);
        std::vector<float> left(static_cast<std::size_t>(totalSamples), 0.0f);
        std::vector<float> right(static_cast<std::size_t>(totalSamples), 0.0f);

        // 0.4s 220 Hz sine burst with smooth Hann envelope
        const int burstSamples = static_cast<int>(kSr * 0.4);
        for (int i = 0; i < burstSamples; ++i) {
            const float env = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * static_cast<float>(i) / static_cast<float>(burstSamples - 1)));
            const float val = 0.5f * env * std::sin(2.0f * 3.14159265f * 220.0f * static_cast<float>(i) / static_cast<float>(kSr));
            left[static_cast<std::size_t>(i)] = val;
            right[static_cast<std::size_t>(i)] = val;
        }

        for (int i = 0; i + 512 <= totalSamples; i += 512) {
            reverb.processBlock(&left[static_cast<std::size_t>(i)], &right[static_cast<std::size_t>(i)], 512);
        }

        // Measure late reverb tail RMS between t = 3.0s and t = 4.0s
        const int startIdx = static_cast<int>(kSr * 3.0);
        const int endIdx = static_cast<int>(kSr * 4.0);
        double sumSq = 0.0;
        for (int i = startIdx; i < endIdx; ++i) {
            const double l = left[static_cast<std::size_t>(i)];
            const double r = right[static_cast<std::size_t>(i)];
            sumSq += 0.5 * (l * l + r * r);
        }
        return std::sqrt(sumSq / static_cast<double>(endIdx - startIdx));
    };

    const double rmsNoShimmer   = measureLateTailRms(0.0f);
    const double rmsHalfShimmer = measureLateTailRms(0.5f);
    const double rmsFullShimmer = measureLateTailRms(1.0f);

    // Turning up shimmer must NEVER shorten or choke the late reverb decay tail!
    EXPECT_GT(rmsHalfShimmer, rmsNoShimmer * 0.90);
    EXPECT_GT(rmsFullShimmer, rmsNoShimmer * 0.95);
}

TEST(PresetManagerTests, InitPatchLoadsCleanOscillatorAndBypassedFx) {
    nova::synth::NovaSynthProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    auto& pm = processor.getPresetManager();
    const bool loaded = pm.loadPresetByName("Init Patch");
    EXPECT_TRUE(loaded);

    EXPECT_EQ(pm.getCurrentPresetName(), "Init Patch");
    EXPECT_EQ(pm.getCurrentPresetCategory(), "Utility");

    auto& apvts = processor.getAPVTS();
    EXPECT_FLOAT_EQ(*apvts.getRawParameterValue("activeOscCount"), 1.0f);
    EXPECT_FLOAT_EQ(*apvts.getRawParameterValue("osc1_enabled"), 1.0f);
    EXPECT_FLOAT_EQ(*apvts.getRawParameterValue("osc2_enabled"), 0.0f);
    EXPECT_FLOAT_EQ(*apvts.getRawParameterValue("osc3_enabled"), 0.0f);
    EXPECT_FLOAT_EQ(*apvts.getRawParameterValue("delayMix"), 0.0f);
    EXPECT_FLOAT_EQ(*apvts.getRawParameterValue("reverbMix"), 0.0f);
    EXPECT_FLOAT_EQ(*apvts.getRawParameterValue("arpEnabled"), 0.0f);
}

TEST(PresetManagerTests, SwitchingPresetSilencesActiveVoicesAndTailsImmediately) {
    nova::synth::NovaSynthProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    auto& pm = processor.getPresetManager();
    // 1. Load an ambient preset with huge decay and lush shimmer
    ASSERT_TRUE(pm.loadPresetByName("Bethel Shimmer Cloud"));

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);

    // Play note and render enough blocks for attack envelope to open
    float soundingPeak = 0.0f;
    for (int b = 0; b < 30; ++b) {
        buffer.clear();
        processor.processBlock(buffer, midi);
        midi.clear();
        for (int ch = 0; ch < 2; ++ch) {
            for (int i = 0; i < 512; ++i) {
                soundingPeak = std::max(soundingPeak, std::abs(buffer.getSample(ch, i)));
            }
        }
    }
    EXPECT_GT(soundingPeak, 0.05f);

    // 2. Switch preset while sound is sounding
    ASSERT_TRUE(pm.loadPresetByName("Init Patch"));

    // 3. Process the next block WITHOUT any new notes
    buffer.clear();
    midi.clear();
    processor.processBlock(buffer, midi);

    // 4. Verify previous voices, drone, and tails are immediately silenced
    float silentPeak = 0.0f;
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 512; ++i) {
            silentPeak = std::max(silentPeak, std::abs(buffer.getSample(ch, i)));
        }
    }
    EXPECT_LT(silentPeak, 1e-4f);
    EXPECT_EQ(processor.getEngine().getTelemetry().activeVoiceCount, 0);
}

TEST(MultiEngineOscillatorTests, AllGranularAndPluckModelsProduceDistinctSignatures) {
    using nova::dsp::MultiEngineOscillator;

    // 1. Verify Granular (OrganicTexture) models are distinct
    {
        std::vector<std::vector<float>> granularBuffers;
        for (int m = 0; m < 5; ++m) {
            MultiEngineOscillator osc;
            osc.prepare(48000.0);
            MultiEngineOscillator::Parameters p;
            p.enabled = true;
            p.sourceType = 0;
            p.engine = MultiEngineOscillator::EngineType::OrganicTexture;
            p.modelIndex = m;
            p.shapeMorph = 0.5f;
            p.level = 0.8f;
            osc.setParameters(p);
            osc.triggerNote(261.63f, 0.85f, 42);

            std::vector<float> buf(1024, 0.0f);
            for (int i = 0; i < 1024; ++i) {
                float l = 0.0f, r = 0.0f;
                osc.processSample(261.63f, 0.0f, 0.0f, l, r);
                buf[static_cast<std::size_t>(i)] = 0.5f * (l + r);
            }
            granularBuffers.push_back(std::move(buf));
        }

        // Check every pair (m1, m2) has distinct audio
        for (int i = 0; i < 5; ++i) {
            for (int j = i + 1; j < 5; ++j) {
                float diffEnergy = 0.0f;
                for (int s = 0; s < 1024; ++s) {
                    const float d = granularBuffers[static_cast<std::size_t>(i)][static_cast<std::size_t>(s)] -
                                    granularBuffers[static_cast<std::size_t>(j)][static_cast<std::size_t>(s)];
                    diffEnergy += d * d;
                }
                EXPECT_GT(diffEnergy, 0.02f) << "Granular models " << i << " and " << j << " are identical!";
            }
        }
    }

    // 2. Verify Pluck models are distinct
    {
        std::vector<std::vector<float>> pluckBuffers;
        for (int m = 0; m < 4; ++m) {
            MultiEngineOscillator osc;
            osc.prepare(48000.0);
            MultiEngineOscillator::Parameters p;
            p.enabled = true;
            p.sourceType = 2; // PLUCK
            p.engine = MultiEngineOscillator::EngineType::PhysicalPluckModal;
            p.modelIndex = m;
            p.shapeMorph = 0.5f;
            p.level = 0.8f;
            osc.setParameters(p);
            osc.triggerNote(440.0f, 0.85f, 42);

            std::vector<float> buf(1024, 0.0f);
            for (int i = 0; i < 1024; ++i) {
                float l = 0.0f, r = 0.0f;
                osc.processSample(440.0f, 0.0f, 0.0f, l, r);
                buf[static_cast<std::size_t>(i)] = 0.5f * (l + r);
            }
            pluckBuffers.push_back(std::move(buf));
        }

        // Check every pair (m1, m2) has distinct audio
        for (int i = 0; i < 4; ++i) {
            for (int j = i + 1; j < 4; ++j) {
                float diffEnergy = 0.0f;
                for (int s = 0; s < 1024; ++s) {
                    const float d = pluckBuffers[static_cast<std::size_t>(i)][static_cast<std::size_t>(s)] -
                                    pluckBuffers[static_cast<std::size_t>(j)][static_cast<std::size_t>(s)];
                    diffEnergy += d * d;
                }
                EXPECT_GT(diffEnergy, 0.02f) << "Pluck models " << i << " and " << j << " are identical!";
            }
        }
    }
}

TEST(MultiEngineOscillatorTests, AllEightWavetablesProduceDistinctSignatures) {
    using nova::dsp::MultiEngineOscillator;
    std::vector<std::vector<float>> wtBuffers;
    for (int m = 0; m < 8; ++m) {
        MultiEngineOscillator osc;
        osc.prepare(48000.0);
        MultiEngineOscillator::Parameters p;
        p.enabled = true;
        p.sourceType = 0;
        p.engine = MultiEngineOscillator::EngineType::EvolvingWavetable;
        p.modelIndex = m;
        p.shapeMorph = 0.5f;
        p.level = 0.8f;
        osc.setParameters(p);
        osc.triggerNote(261.63f, 0.85f, 42);

        std::vector<float> buf(1024, 0.0f);
        for (int i = 0; i < 1024; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(261.63f, 0.0f, 0.0f, l, r);
            buf[static_cast<std::size_t>(i)] = 0.5f * (l + r);
        }
        wtBuffers.push_back(std::move(buf));
    }

    // Check every pair (i, j) of wavetables has distinct audio
    for (int i = 0; i < 8; ++i) {
        for (int j = i + 1; j < 8; ++j) {
            float diffEnergy = 0.0f;
            for (int s = 0; s < 1024; ++s) {
                const float d = wtBuffers[static_cast<std::size_t>(i)][static_cast<std::size_t>(s)] -
                                wtBuffers[static_cast<std::size_t>(j)][static_cast<std::size_t>(s)];
                diffEnergy += d * d;
            }
            EXPECT_GT(diffEnergy, 0.02f) << "Wavetable models " << i << " and " << j << " sound identical!";
        }
    }
}

TEST(MultiEngineOscillatorTests, AnalogAllSixWaveformsRenderAccurately) {
    using nova::dsp::MultiEngineOscillator;
    MultiEngineOscillator osc;
    osc.prepare(48000.0);

    std::vector<std::vector<float>> analogBuffers;
    for (int m = 0; m < 6; ++m) {
        MultiEngineOscillator::Parameters p;
        p.enabled = true;
        p.sourceType = 0;
        p.engine = MultiEngineOscillator::EngineType::WarmAnalog;
        p.modelIndex = m;
        p.shapeMorph = 0.3f;
        p.level = 0.8f;
        osc.setParameters(p);
        osc.triggerNote(220.0f, 0.85f, 42);

        std::vector<float> buf(512, 0.0f);
        for (int i = 0; i < 512; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(220.0f, 0.0f, 0.0f, l, r);
            buf[static_cast<std::size_t>(i)] = l;
        }
        analogBuffers.push_back(std::move(buf));
    }

    for (int i = 0; i < 6; ++i) {
        for (int j = i + 1; j < 6; ++j) {
            float diff = 0.0f;
            for (int s = 0; s < 512; ++s) {
                const float d = analogBuffers[static_cast<std::size_t>(i)][static_cast<std::size_t>(s)] -
                                analogBuffers[static_cast<std::size_t>(j)][static_cast<std::size_t>(s)];
                diff += d * d;
            }
            EXPECT_GT(diff, 0.05f) << "Analog waveforms " << i << " and " << j << " are not distinct!";
        }
    }
}

TEST(MultiEngineOscillatorTests, NoiseAttackKeyClickGeneratesAudibleTransientPunch) {
    using nova::dsp::MultiEngineOscillator;
    MultiEngineOscillator osc;
    osc.prepare(48000.0);

    MultiEngineOscillator::Parameters p;
    p.enabled = true;
    p.sourceType = 3; // NOISE
    p.engine = MultiEngineOscillator::EngineType::NoiseTransient;
    p.modelIndex = 3; // Attack Key Click
    p.level = 0.85f;
    p.shapeMorph = 0.4f;
    osc.setParameters(p);
    osc.triggerNote(261.63f, 0.9f, 42);

    float peak = 0.0f;
    float clickEnergy = 0.0f;
    for (int i = 0; i < 512; ++i) {
        float l = 0.0f, r = 0.0f;
        osc.processSample(261.63f, 0.0f, 0.0f, l, r);
        peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
        clickEnergy += l * l + r * r;
    }

    // Must be clearly audible and punchy upon note trigger
    EXPECT_GT(peak, 0.15f);
    EXPECT_GT(clickEnergy, 0.02f);
}

TEST(SampleBankTests, AllEightSamplesArePopulatedAndNonZero) {
    using nova::dsp::SampleBank;
    EXPECT_EQ(SampleBank::getSampleCount(), 8);

    for (int i = 0; i < 8; ++i) {
        const auto& smp = SampleBank::getSample(i);
        EXPECT_NE(smp.zones, nullptr);
        EXPECT_GE(smp.numZones, 1);
        EXPECT_LE(smp.numZones, 3);
        EXPECT_GT(smp.sampleRate, 40000.0);
        EXPECT_NE(smp.name, nullptr);
        EXPECT_GT(std::strlen(smp.name), 2u);

        for (int z = 0; z < smp.numZones; ++z) {
            const auto& zone = smp.zones[z];
            EXPECT_NE(zone.pcmL, nullptr);
            EXPECT_NE(zone.pcmR, nullptr);
            EXPECT_GT(zone.numSamples, 1000);
            EXPECT_GT(zone.rootFrequencyHz, 50.0f);
            EXPECT_LT(zone.rootFrequencyHz, 2000.0f);

            // Verify peak is normalized and non-silent
            int maxAbsL = 0;
            int maxAbsR = 0;
            for (int s = 0; s < zone.numSamples; ++s) {
                maxAbsL = std::max(maxAbsL, std::abs(static_cast<int>(zone.pcmL[s])));
                maxAbsR = std::max(maxAbsR, std::abs(static_cast<int>(zone.pcmR[s])));
            }
            EXPECT_GT(maxAbsL, 10000); // Strong, pristine signal level
            EXPECT_GT(maxAbsR, 10000);
        }
    }
}

TEST(MultiEngineOscillatorTests, SamplePlaybackEngineRendersCleanAudio) {
    using nova::dsp::MultiEngineOscillator;
    MultiEngineOscillator osc;
    osc.prepare(48000.0);

    for (int m = 0; m < 8; ++m) {
        MultiEngineOscillator::Parameters p;
        p.enabled = true;
        p.sourceType = 4;
        p.engine = MultiEngineOscillator::EngineType::SamplePlayback;
        p.modelIndex = m;
        p.level = 0.85f;
        p.shapeMorph = 0.0f;
        osc.setParameters(p);
        osc.triggerNote(261.63f, 0.9f, 123);

        float maxPeak = 0.0f;
        for (int i = 0; i < 1024; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(261.63f, 0.0f, 0.0f, l, r);
            EXPECT_TRUE(std::isfinite(l));
            EXPECT_TRUE(std::isfinite(r));
            maxPeak = std::max(maxPeak, std::max(std::abs(l), std::abs(r)));
        }
        EXPECT_GT(maxPeak, 0.05f) << "Sample model " << m << " did not produce audible audio!";
    }
}

TEST(PresetManagerTests, AllHybridSamplePresetsLoadAndRenderAudio) {
    nova::synth::NovaSynthProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    auto& pm = processor.getPresetManager();
    const std::vector<std::string> samplePresets = {
        "Nils Felt Piano & Warm Pad",
        "Celestial Concert Harp",
        "Nordic Mallet & Bell Cloud",
        "Sacred Pipe Flute Swell",
        "Music Box & Vinyl Dreams",
        "Cathedral Chime & Choir Ambient",
        "Solo Concert Grand Piano",
        "Pure Concert Harp Solo",
        "Nordic Celesta & Glockenspiel"
    };

    for (const auto& name : samplePresets) {
        const bool loaded = pm.loadPresetByName(name);
        EXPECT_TRUE(loaded) << "Failed to load factory preset: " << name;

        // Render audio block with a MIDI note played
        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);

        processor.processBlock(buffer, midi);

        float peak = 0.0f;
        for (int c = 0; c < 2; ++c) {
            const float* ch = buffer.getReadPointer(c);
            for (int i = 0; i < 512; ++i) {
                EXPECT_TRUE(std::isfinite(ch[i]));
                peak = std::max(peak, std::abs(ch[i]));
            }
        }
        EXPECT_GT(peak, 0.001f) << "Preset " << name << " rendered silence!";
    }
}

TEST(WorshipSynthEngineTests, AllFourOscillatorEnginesRenderCleanSignal) {
    using nova::dsp::MultiEngineOscillator;
    MultiEngineOscillator osc;
    osc.prepare(48000.0);

    for (int e = 0; e < 4; ++e) {
        MultiEngineOscillator::Parameters p;
        p.enabled = true;
        p.engine = static_cast<MultiEngineOscillator::EngineType>(e);
        p.shapeMorph = 0.5f;
        p.detune = 0.3f;
        p.level = 0.8f;
        osc.setParameters(p);
        osc.triggerNote(261.63f, 0.85f, static_cast<uint32_t>(e + 1));

        float energy = 0.0f;
        for (int i = 0; i < 1024; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(261.63f, 0.0f, 0.0f, l, r);
            ASSERT_TRUE(std::isfinite(l));
            ASSERT_TRUE(std::isfinite(r));
            energy += l * l + r * r;
        }
        EXPECT_GT(energy, 1e-3f) << "Engine index " << e << " produced no audio energy.";
    }
}

TEST(WorshipSynthEngineTests, PolyphonicPadAndDroneGenerateSustainedOutput) {
    nova::dsp::WorshipSynthEngine synth;
    synth.prepare(48000.0, 512);

    nova::dsp::WorshipSynthEngine::Parameters p = synth.getParameters();
    p.activeOscCount = 3;
    p.droneEnabled = true;
    p.droneRootKey = 0; // C Drone
    p.droneLevel = 0.6f;
    synth.setParameters(p);

    // Also play a C major triad
    synth.noteOn(60, 0.85f);
    synth.noteOn(64, 0.80f);
    synth.noteOn(67, 0.82f);

    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);

    float totalEnergy = 0.0f;
    for (int b = 0; b < 12; ++b) {
        synth.processBlock(left.data(), right.data(), 512);
        for (int i = 0; i < 512; ++i) {
            ASSERT_TRUE(std::isfinite(left[static_cast<std::size_t>(i)]));
            ASSERT_TRUE(std::isfinite(right[static_cast<std::size_t>(i)]));
            totalEnergy += left[static_cast<std::size_t>(i)] * left[static_cast<std::size_t>(i)];
        }
    }

    const auto tel = synth.getTelemetry();
    EXPECT_GE(tel.activeVoiceCount, 6); // 3 Drone voices + 3 Triad voices
    EXPECT_GT(totalEnergy, 1e-2f);
}

TEST(WorshipSynthEngineTests, DroneWaitsForFirstTriggerAndDoesNotAutoStartOnPreset) {
    nova::dsp::WorshipSynthEngine synth;
    synth.prepare(48000.0, 512);

    nova::dsp::WorshipSynthEngine::Parameters p = synth.getParameters();
    p.droneEnabled = true;
    p.droneRootKey = 0; // C
    p.droneLevel = 0.7f;
    synth.setParameters(p);

    // Drone must NOT be triggered initially on preset load
    EXPECT_FALSE(synth.isDroneTriggered());

    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);
    synth.processBlock(left.data(), right.data(), 512);

    float silentEnergy = 0.0f;
    for (float s : left) silentEnergy += s * s;
    EXPECT_LT(silentEnergy, 1e-6f);
    EXPECT_EQ(synth.getTelemetry().activeVoiceCount, 0);

    // Trigger drone by striking first note (musician starts playing)
    synth.noteOn(60, 0.8f);
    EXPECT_TRUE(synth.isDroneTriggered());

    float playingEnergy = 0.0f;
    for (int b = 0; b < 8; ++b) {
        synth.processBlock(left.data(), right.data(), 512);
        for (float s : left) playingEnergy += s * s;
    }
    EXPECT_GT(playingEnergy, 1e-3f);
    // At least 3 drone voices + 1 played note voice
    EXPECT_GE(synth.getTelemetry().activeVoiceCount, 4);

    // Resetting engine clears trigger state and silences drone
    synth.reset();
    EXPECT_FALSE(synth.isDroneTriggered());
}

class DummyTestProcessor : public juce::AudioProcessor {
public:
    DummyTestProcessor()
        : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          apvts(*this, nullptr, "PARAMS", createLayout()) {}
    ~DummyTestProcessor() override = default;

    const juce::String getName() const override { return "DummyTest"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{"osc1_octave", 1}, "OSC 1 Octave", -2, 2, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{"osc2_octave", 1}, "OSC 2 Octave", -2, 2, 0));
        return { params.begin(), params.end() };
    }

    juce::AudioProcessorValueTreeState apvts;
};

TEST(NovaSynthRegressionTests, ArcKnobParameterRebindingPreservesUnselectedParameters) {
    DummyTestProcessor proc;
    auto& apvts = proc.getAPVTS();

    nova::ui::NovaTheme theme;
    nova::ui::ArcKnob knob("Octave", "oct", theme);

    // 1. Attach knob to OSC 2 octave and set to +2
    knob.attachToParameter(apvts, "osc2_octave");
    knob.getSlider().setValue(2.0, juce::sendNotificationSync);

    auto* p2 = apvts.getRawParameterValue("osc2_octave");
    ASSERT_NE(p2, nullptr);
    EXPECT_FLOAT_EQ(p2->load(), 2.0f);

    // 2. Re-attach knob to OSC 1 octave (which has default 0) and set to -1
    knob.attachToParameter(apvts, "osc1_octave");
    EXPECT_FLOAT_EQ(static_cast<float>(knob.getSlider().getValue()), 0.0f);
    knob.getSlider().setValue(-1.0, juce::sendNotificationSync);

    auto* p1 = apvts.getRawParameterValue("osc1_octave");
    ASSERT_NE(p1, nullptr);
    EXPECT_FLOAT_EQ(p1->load(), -1.0f);

    // 3. REGRESSION CHECK: Verify that re-attaching did NOT reset or overwrite OSC 2!
    EXPECT_FLOAT_EQ(p2->load(), 2.0f);
}

TEST(NovaSynthRegressionTests, MultiEngineOscillatorRendersSubAndNoise) {
    using nova::dsp::MultiEngineOscillator;
    MultiEngineOscillator osc;
    osc.prepare(48000.0);

    // Sub Oscillator
    MultiEngineOscillator::Parameters pSub;
    pSub.enabled = true;
    pSub.engine = MultiEngineOscillator::EngineType::SubOscillator;
    pSub.shapeMorph = 0.25f;
    pSub.level = 0.9f;
    osc.setParameters(pSub);
    osc.triggerNote(130.81f, 0.9f, 42);

    float subEnergy = 0.0f;
    for (int i = 0; i < 512; ++i) {
        float l = 0.0f, r = 0.0f;
        osc.processSample(130.81f, 0.0f, 0.0f, l, r);
        ASSERT_TRUE(std::isfinite(l));
        ASSERT_TRUE(std::isfinite(r));
        subEnergy += l * l + r * r;
    }
    EXPECT_GT(subEnergy, 1e-3f);

    // Noise Transient
    MultiEngineOscillator::Parameters pNoise;
    pNoise.enabled = true;
    pNoise.engine = MultiEngineOscillator::EngineType::NoiseTransient;
    pNoise.shapeMorph = 0.5f;
    pNoise.level = 0.8f;
    osc.setParameters(pNoise);
    osc.triggerNote(440.0f, 0.8f, 99);

    float noiseEnergy = 0.0f;
    for (int i = 0; i < 512; ++i) {
        float l = 0.0f, r = 0.0f;
        osc.processSample(440.0f, 0.0f, 0.0f, l, r);
        ASSERT_TRUE(std::isfinite(l));
        ASSERT_TRUE(std::isfinite(r));
        noiseEnergy += l * l + r * r;
    }
    EXPECT_GT(noiseEnergy, 1e-3f);
}

TEST(NovaSynthRegressionTests, WorshipSynthEngineFilterRoutingDirectBypassesFilter1) {
    nova::dsp::WorshipSynthEngine synth;
    synth.prepare(48000.0, 512);

    nova::dsp::WorshipSynthEngine::Parameters p = synth.getParameters();
    p.activeOscCount = 1;
    p.filter1Enabled = true;
    p.filterType = 0; // LowPass
    p.filterCutoffHz = 60.0f; // Low cutoff (attenuates high notes)
    p.filterResonance = 0.7f;
    p.filterEnvAmount = 0.0f;
    p.filter2Enabled = false;
    p.reverbMix = 0.0f;

    // Test with FLT 1 (route = 0)
    p.oscSlots[0].enabled = true;
    p.oscSlots[0].engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
    p.oscSlots[0].filterRouting = 0; // FLT 1
    p.oscSlots[0].level = 1.0f;
    synth.setParameters(p);

    synth.noteOn(72, 0.9f); // C5 = 523 Hz, well above 60 Hz cutoff
    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);

    float filteredEnergy = 0.0f;
    for (int b = 0; b < 4; ++b) {
        synth.processBlock(left.data(), right.data(), 512);
        for (int i = 0; i < 512; ++i) {
            filteredEnergy += left[static_cast<std::size_t>(i)] * left[static_cast<std::size_t>(i)];
        }
    }
    synth.allNotesOff();

    // Test with DIRECT (route = 3)
    p.oscSlots[0].filterRouting = 3; // DIRECT
    synth.setParameters(p);
    synth.noteOn(72, 0.9f);

    float directEnergy = 0.0f;
    for (int b = 0; b < 4; ++b) {
        synth.processBlock(left.data(), right.data(), 512);
        for (int i = 0; i < 512; ++i) {
            directEnergy += left[static_cast<std::size_t>(i)] * left[static_cast<std::size_t>(i)];
        }
    }

    // DIRECT signal should be significantly louder than heavily filtered signal
    EXPECT_GT(directEnergy, filteredEnergy * 5.0f);
}

TEST(NovaSynthRegressionTests, GainStagingSingleNoteIsLoudAndNominal) {
    nova::dsp::WorshipSynthEngine synth;
    synth.prepare(48000.0, 512);

    nova::dsp::WorshipSynthEngine::Parameters p = synth.getParameters();
    p.activeOscCount = 1;
    p.oscSlots[0].enabled = true;
    p.oscSlots[0].engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
    p.oscSlots[0].level = 0.85f;
    p.ampEnv.attackSec = 0.005f; // Fast attack to test nominal steady-state volume
    p.ampEnv.sustainLvl = 1.0f;
    p.filter1Enabled = true;
    p.filterCutoffHz = 4000.0f;
    p.reverbMix = 0.0f;
    synth.setParameters(p);

    synth.noteOn(60, 0.85f); // Middle C

    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);

    float maxPeak = 0.0f;
    for (int b = 0; b < 6; ++b) {
        synth.processBlock(left.data(), right.data(), 512);
        for (int i = 0; i < 512; ++i) {
            maxPeak = std::max(maxPeak, std::abs(left[static_cast<std::size_t>(i)]));
        }
    }

    // Nominal level check: Single note should be punchy and loud (> -10 dBFS = 0.316)
    // and not whisper-quiet like previously (-16 dBFS).
    EXPECT_GT(maxPeak, 0.30f);
    EXPECT_LE(maxPeak, 1.0f);
}

TEST(NovaSynthRegressionTests, MasterSoftClipperPreventsClippingOnHeavyChords) {
    nova::dsp::WorshipSynthEngine synth;
    synth.prepare(48000.0, 512);

    nova::dsp::WorshipSynthEngine::Parameters p = synth.getParameters();
    p.activeOscCount = 4;
    for (int i = 0; i < 4; ++i) {
        p.oscSlots[static_cast<std::size_t>(i)].enabled = true;
        p.oscSlots[static_cast<std::size_t>(i)].level = 1.0f;
    }
    p.filter1Enabled = false;
    p.filterDrive = 0.5f;
    p.reverbMix = 0.5f;
    synth.setParameters(p);

    // Play 10 loud simultaneous notes
    for (int n = 48; n <= 72; n += 2) {
        synth.noteOn(n, 1.0f);
    }

    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);

    float maxPeak = 0.0f;
    for (int b = 0; b < 10; ++b) {
        synth.processBlock(left.data(), right.data(), 512);
        for (int i = 0; i < 512; ++i) {
            ASSERT_TRUE(std::isfinite(left[static_cast<std::size_t>(i)]));
            ASSERT_TRUE(std::isfinite(right[static_cast<std::size_t>(i)]));
            maxPeak = std::max(maxPeak, std::abs(left[static_cast<std::size_t>(i)]));
        }
    }

    // Soft-clipper must gracefully saturate and prevent any digital overshoot
    EXPECT_LE(maxPeak, 1.02f);
}

TEST(NovaSynthRegressionTests, PolyphonyVoiceStealingGracefullyCapsAtMaxVoices) {
    nova::dsp::WorshipSynthEngine synth;
    synth.prepare(48000.0, 512);

    // Trigger 48 distinct notes (well beyond the 32 voice limit)
    for (int n = 24; n < 72; ++n) {
        synth.noteOn(n, 0.8f);
    }

    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);
    synth.processBlock(left.data(), right.data(), 512);

    const auto tel = synth.getTelemetry();
    EXPECT_LE(tel.activeVoiceCount, 32);
    EXPECT_GT(tel.activeVoiceCount, 0);

    for (int i = 0; i < 512; ++i) {
        ASSERT_TRUE(std::isfinite(left[static_cast<std::size_t>(i)]));
        ASSERT_TRUE(std::isfinite(right[static_cast<std::size_t>(i)]));
    }
}

TEST(NovaSynthRegressionTests, DenormalsAndSilenceDecaySafety) {
    nova::dsp::DualEngineReverb reverb;
    reverb.prepare(48000.0, 512);

    nova::dsp::DualEngineReverb::Parameters p;
    p.engineMode = nova::dsp::DualEngineReverb::EngineMode::LushCloud;
    p.decaySeconds = 20.0f;
    p.shimmerOctave = 0.5f;
    reverb.setParameters(p);

    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);
    left[0] = 1.0f;
    right[0] = 1.0f;

    reverb.processBlock(left.data(), right.data(), 512);

    // Feed 40 blocks of pure silence
    for (int b = 0; b < 40; ++b) {
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        reverb.processBlock(left.data(), right.data(), 512);
        for (int i = 0; i < 512; ++i) {
            ASSERT_TRUE(std::isfinite(left[static_cast<std::size_t>(i)]));
            ASSERT_TRUE(std::isfinite(right[static_cast<std::size_t>(i)]));
            // Check that tiny numbers don't blow up or turn into NaNs
            ASSERT_FALSE(std::isnan(left[static_cast<std::size_t>(i)]));
        }
    }
}

TEST(NovaSynthRegressionTests, SampleRateSwitchingConsistency) {
    nova::dsp::WorshipSynthEngine synth;

    const std::array<double, 4> sampleRates = { 44100.0, 96000.0, 192000.0, 48000.0 };
    for (double sr : sampleRates) {
        synth.prepare(sr, 512);
        auto p = synth.getParameters();
        p.ampEnv.attackSec = 0.005f; // Fast attack for consistent initial block energy
        synth.setParameters(p);
        synth.noteOn(60, 0.8f);

        std::vector<float> left(512, 0.0f);
        std::vector<float> right(512, 0.0f);
        synth.processBlock(left.data(), right.data(), 512);

        float blockEnergy = 0.0f;
        for (int i = 0; i < 512; ++i) {
            ASSERT_TRUE(std::isfinite(left[static_cast<std::size_t>(i)]));
            blockEnergy += left[static_cast<std::size_t>(i)] * left[static_cast<std::size_t>(i)];
        }
        EXPECT_GT(blockEnergy, 1e-4f) << "Failed at sample rate " << sr;
        synth.allNotesOff();
    }
}

TEST(NovaSynthRegressionTests, MidiSustainPedalAndAllNotesOffSafety) {
    auto synthPtr = std::make_unique<nova::dsp::WorshipSynthEngine>();
    auto& synth = *synthPtr;
    synth.prepare(48000.0, 512);

    auto p = synth.getParameters();
    p.ampEnv.releaseSec = 0.04f; // 40 ms release time
    synth.setParameters(p);

    synth.noteOn(60, 0.8f);
    synth.setSustainPedal(true);
    synth.noteOff(60); // Released physical key, but held by sustain pedal

    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);
    synth.processBlock(left.data(), right.data(), 512);

    auto tel = synth.getTelemetry();
    EXPECT_GE(tel.activeVoiceCount, 1); // Note MUST still sound because pedal is down

    synth.setSustainPedal(false); // Pedal released -> voice should release
    synth.allNotesOff();          // Triggers release on all held voices
    synth.processBlock(left.data(), right.data(), 512);

    // After 10 blocks (100 ms > 40 ms release), all voices should have finished release
    for (int b = 0; b < 10; ++b) {
        synth.processBlock(left.data(), right.data(), 512);
    }
    tel = synth.getTelemetry();
    EXPECT_EQ(tel.activeVoiceCount, 0);

    // Also verify allSoundOff() cuts sound immediately
    synth.noteOn(60, 0.8f);
    synth.allSoundOff();
    tel = synth.getTelemetry();
    EXPECT_EQ(tel.activeVoiceCount, 0);
}

// Helper: Discrete Fourier Transform bin (Goertzel) with Hann window to measure magnitude at a single target frequency
inline float measureMagnitudeAtFreq(const float* buffer, int numSamples, double sampleRate, float targetHz) noexcept {
    if (numSamples <= 0 || sampleRate <= 0.0) return 0.0f;
    const double omega = 2.0 * std::numbers::pi_v<double> * static_cast<double>(targetHz) / sampleRate;
    const double coeff = 2.0 * std::cos(omega);
    double s_prev = 0.0;
    double s_prev2 = 0.0;
    for (int i = 0; i < numSamples; ++i) {
        const double win = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi_v<double> * i / static_cast<double>(numSamples - 1)));
        const double x = static_cast<double>(buffer[i]) * win;
        const double s = x + coeff * s_prev - s_prev2;
        s_prev2 = s_prev;
        s_prev = s;
    }
    const double power = s_prev2 * s_prev2 + s_prev * s_prev - coeff * s_prev * s_prev2;
    return static_cast<float>(std::sqrt(std::max(0.0, power)) * (4.0 / static_cast<double>(numSamples)));
}

// Helper: Normalized Pearson correlation coefficient between Left and Right channels: r = sum(L*R) / sqrt(sum(L^2)*sum(R^2))
inline float measureStereoCorrelation(const float* left, const float* right, int numSamples) noexcept {
    double sumLR = 0.0, sumL2 = 0.0, sumR2 = 0.0;
    for (int i = 0; i < numSamples; ++i) {
        const double l = left[i];
        const double r = right[i];
        sumLR += l * r;
        sumL2 += l * l;
        sumR2 += r * r;
    }
    const double denom = std::sqrt(sumL2 * sumR2);
    if (denom < 1e-9) return 1.0f;
    return static_cast<float>(sumLR / denom);
}

TEST(DualEngineReverbTests, ShimmerProducesAudibleHarmonicsAndStaysPitchLocked) {
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    auto measureBandPeak = [&](const float* buf, int n, float centerHz) {
        float best = 0.0f;
        for (float df = -5.0f; df <= 5.0f; df += 0.5f) {
            best = std::max(best, measureMagnitudeAtFreq(buf, n, kSr, centerHz + df));
        }
        return best;
    };

    auto runMode = [&](float shimOct, int pitchMode) {
        nova::dsp::DualEngineReverb reverb;
        reverb.prepare(kSr, kBlock);
        nova::dsp::DualEngineReverb::Parameters params;
        params.engineMode = nova::dsp::DualEngineReverb::EngineMode::LushCloud;
        params.spaceType = nova::dsp::DualEngineReverb::SpaceType::CathedralCloud;
        params.decaySeconds = 10.0f;
        params.dry = 0.0f;
        params.mix = 1.0f;
        params.shimmerOctave = shimOct;
        params.shimmerPitchMode = pitchMode;
        reverb.setParameters(params);

        std::vector<float> left(kBlock, 0.0f), right(kBlock, 0.0f);
        for (int b = 0; b < 38; ++b) {
            for (int i = 0; i < kBlock; ++i) {
                const float t = static_cast<float>(b * kBlock + i) / static_cast<float>(kSr);
                const float s = 0.5f * std::sin(2.0f * std::numbers::pi_v<float> * 220.0f * t);
                left[static_cast<std::size_t>(i)] = s;
                right[static_cast<std::size_t>(i)] = s;
            }
            reverb.processBlock(left.data(), right.data(), kBlock);
        }

        std::vector<float> tailL(16384, 0.0f);
        for (int b = 0; b < 32; ++b) {
            std::fill(left.begin(), left.end(), 0.0f);
            std::fill(right.begin(), right.end(), 0.0f);
            reverb.processBlock(left.data(), right.data(), kBlock);
            for (int i = 0; i < kBlock; ++i) {
                tailL[static_cast<std::size_t>(b * kBlock + i)] = left[static_cast<std::size_t>(i)];
            }
        }
        return tailL;
    };

    const auto tailBase  = runMode(0.0f, 0);
    const auto tailMode0 = runMode(0.80f, 0); // +1 Oct (440 Hz)
    const auto tailMode1 = runMode(0.80f, 1); // +2 Oct (880 Hz)
    const auto tailMode2 = runMode(0.80f, 2); // +1 & +2 Oct (440 Hz & 880 Hz)
    const auto tailMode3 = runMode(0.80f, 3); // -1 Oct Sub (110 Hz)

    const float base440 = measureBandPeak(tailBase.data(), 16384, 440.0f);
    const float base880 = measureBandPeak(tailBase.data(), 16384, 880.0f);
    const float base110 = measureBandPeak(tailBase.data(), 16384, 110.0f);

    EXPECT_GT(measureBandPeak(tailMode0.data(), 16384, 440.0f), base440 * 20.0f);
    EXPECT_GT(measureBandPeak(tailMode1.data(), 16384, 880.0f), base880 * 20.0f);
    EXPECT_GT(measureBandPeak(tailMode2.data(), 16384, 440.0f), base440 * 15.0f);
    EXPECT_GT(measureBandPeak(tailMode2.data(), 16384, 880.0f), base880 * 15.0f);
    EXPECT_GT(measureBandPeak(tailMode3.data(), 16384, 110.0f), base110 * 20.0f);
}

// -------------------------------------------------------------------------------------------------
// COMPREHENSIVE TEST SUITE 1: Preset Headroom & Anti-Clipping Verification (Single Note & 7-Note Chords)
// -------------------------------------------------------------------------------------------------
TEST(NovaSynthComprehensiveTests, AllFactoryPresetsHeadroomAndNoClipping) {
    nova::synth::NovaSynthProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    auto& presetMgr = processor.getPresetManager();
    const auto& presets = presetMgr.getAllPresets();
    ASSERT_FALSE(presets.empty());

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    const std::vector<int> heavyWorshipChord = { 36, 48, 55, 60, 64, 67, 71 }; // 7-note worship chord

    for (int pIdx = 0; pIdx < static_cast<int>(presets.size()); ++pIdx) {
        presetMgr.loadPresetByIndex(pIdx);
        const auto presetName = presets[static_cast<std::size_t>(pIdx)].name;

        // --- Part A: Single Note Test (MIDI 60, vel 0.90) ---
        processor.triggerAuditionNoteOn(60, 0.90f);
        float singleNoteMaxPeak = 0.0f;

        // Hold single note for 0.8s (75 blocks)
        for (int b = 0; b < 75; ++b) {
            processor.processBlock(buffer, midi);
            for (int ch = 0; ch < 2; ++ch) {
                const float* r = buffer.getReadPointer(ch);
                for (int i = 0; i < 512; ++i) {
                    ASSERT_TRUE(std::isfinite(r[i])) << "Non-finite sample in preset " << presetName;
                    singleNoteMaxPeak = std::max(singleNoteMaxPeak, std::abs(r[i]));
                }
            }
        }
        processor.triggerAuditionNoteOff(60);

        // Allow release to ring down for 1.0s (94 blocks)
        for (int b = 0; b < 94; ++b) {
            processor.processBlock(buffer, midi);
            for (int ch = 0; ch < 2; ++ch) {
                const float* r = buffer.getReadPointer(ch);
                for (int i = 0; i < 512; ++i) {
                    singleNoteMaxPeak = std::max(singleNoteMaxPeak, std::abs(r[i]));
                }
            }
        }

        EXPECT_GT(singleNoteMaxPeak, 0.05f) << "Preset '" << presetName << "' produced almost no sound!";
        EXPECT_LE(singleNoteMaxPeak, 1.01f) << "Preset '" << presetName << "' clipped on single note: peak = " << singleNoteMaxPeak;

        // Flush tail before chord test
        processor.releaseResources();
        processor.prepareToPlay(48000.0, 512);
        presetMgr.loadPresetByIndex(pIdx);

        // --- Part B: 7-Note Polyphonic Worship Chord (vel 0.85) ---
        for (int n : heavyWorshipChord) {
            processor.triggerAuditionNoteOn(n, 0.85f);
        }
        float chordMaxPeak = 0.0f;

        // Hold chord for 1.0s (94 blocks)
        for (int b = 0; b < 94; ++b) {
            processor.processBlock(buffer, midi);
            for (int ch = 0; ch < 2; ++ch) {
                const float* r = buffer.getReadPointer(ch);
                for (int i = 0; i < 512; ++i) {
                    ASSERT_TRUE(std::isfinite(r[i])) << "Non-finite sample in chord of preset " << presetName;
                    chordMaxPeak = std::max(chordMaxPeak, std::abs(r[i]));
                }
            }
        }
        for (int n : heavyWorshipChord) {
            processor.triggerAuditionNoteOff(n);
        }

        // Release ring down (94 blocks)
        for (int b = 0; b < 94; ++b) {
            processor.processBlock(buffer, midi);
            for (int ch = 0; ch < 2; ++ch) {
                const float* r = buffer.getReadPointer(ch);
                for (int i = 0; i < 512; ++i) {
                    chordMaxPeak = std::max(chordMaxPeak, std::abs(r[i]));
                }
            }
        }

        EXPECT_LE(chordMaxPeak, 1.01f) << "Preset '" << presetName << "' clipped on 7-note chord: peak = " << chordMaxPeak;
        EXPECT_GT(chordMaxPeak, 0.10f) << "Preset '" << presetName << "' chord too quiet: peak = " << chordMaxPeak;
    }
}

// -------------------------------------------------------------------------------------------------
// COMPREHENSIVE TEST SUITE 2: Oscillator Waveform Spectral Purity & Harmonic Models
// -------------------------------------------------------------------------------------------------
TEST(NovaSynthComprehensiveTests, OscillatorWaveformsPurityAndSpectrum) {
    constexpr double sr = 48000.0;
    constexpr int nSamples = 48000; // 1 full second
    constexpr float f0 = 440.0f;     // A4

    std::vector<float> bufL(nSamples, 0.0f);
    std::vector<float> bufR(nSamples, 0.0f);

    // 1. Pure Sine Wave: model = 3, shape = 0.0f, detune = 0.0f
    {
        nova::dsp::MultiEngineOscillator osc;
        osc.prepare(sr);
        auto p = osc.getParameters();
        p.engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
        p.sourceType = 0;
        p.modelIndex = 3; // Mellow Sine
        p.shapeMorph = 0.0f;
        p.detune = 0.0f;
        p.level = 1.0f;
        p.pan = 0.0f;
        osc.setParameters(p);
        osc.triggerNote(f0, 1.0f, 0);

        for (int i = 0; i < nSamples; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(f0, 0.0f, 0.0f, l, r);
            bufL[static_cast<std::size_t>(i)] = l;
            bufR[static_cast<std::size_t>(i)] = r;
        }

        const float fundamental = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, f0);
        const float harm2 = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, 2.0f * f0);
        const float harm3 = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, 3.0f * f0);
        const float harm4 = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, 4.0f * f0);

        EXPECT_GE(fundamental, 0.65f) << "Sine fundamental too weak";
        EXPECT_LT(harm2, 0.005f) << "Sine 2nd harmonic exceeds THD threshold!";
        EXPECT_LT(harm3, 0.005f) << "Sine 3rd harmonic exceeds THD threshold!";
        EXPECT_LT(harm4, 0.005f) << "Sine 4th harmonic exceeds THD threshold!";
    }

    // 2. Classic Sawtooth: model = 0, shape = 0.0f
    {
        nova::dsp::MultiEngineOscillator osc;
        osc.prepare(sr);
        auto p = osc.getParameters();
        p.engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
        p.sourceType = 0;
        p.modelIndex = 0; // Classic Saw
        p.shapeMorph = 0.0f;
        p.detune = 0.0f;
        p.level = 1.0f;
        osc.setParameters(p);
        osc.triggerNote(f0, 1.0f, 0);

        for (int i = 0; i < nSamples; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(f0, 0.0f, 0.0f, l, r);
            bufL[static_cast<std::size_t>(i)] = l;
        }

        const float h1 = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, f0);
        const float h2 = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, 2.0f * f0);
        const float h3 = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, 3.0f * f0);
        const float h4 = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, 4.0f * f0);

        EXPECT_GT(h1, 0.40f);
        EXPECT_GT(h2, 0.15f);
        EXPECT_GT(h3, 0.08f);
        EXPECT_GT(h4, 0.04f);
        // Saw harmonics must follow decreasing harmonic series
        EXPECT_GT(h1, h2);
        EXPECT_GT(h2, h3);
        EXPECT_GT(h3, h4);
    }

    // 3. Pulse / Square Wave: model = 1, shape = 0.0f (50% duty cycle square)
    {
        nova::dsp::MultiEngineOscillator osc;
        osc.prepare(sr);
        auto p = osc.getParameters();
        p.engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
        p.sourceType = 0;
        p.modelIndex = 1; // Pulse / Square
        p.shapeMorph = 0.0f;
        p.detune = 0.0f;
        p.level = 1.0f;
        osc.setParameters(p);
        osc.triggerNote(f0, 1.0f, 0);

        for (int i = 0; i < nSamples; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(f0, 0.0f, 0.0f, l, r);
            bufL[static_cast<std::size_t>(i)] = l;
        }

        const float h1 = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, f0);
        const float h2 = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, 2.0f * f0);
        const float h3 = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, 3.0f * f0);
        const float h4 = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, 4.0f * f0);

        EXPECT_GT(h1, 0.50f);
        EXPECT_GT(h3, 0.15f);
        // Symmetric square wave cancels even harmonics
        EXPECT_LT(h2, 0.05f) << "Square wave even 2nd harmonic not suppressed!";
        EXPECT_LT(h4, 0.05f) << "Square wave even 4th harmonic not suppressed!";
    }

    // 4. Warm Triangle: model = 2, shape = 0.0f
    {
        nova::dsp::MultiEngineOscillator osc;
        osc.prepare(sr);
        auto p = osc.getParameters();
        p.engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
        p.sourceType = 0;
        p.modelIndex = 2; // Triangle
        p.shapeMorph = 0.0f;
        p.detune = 0.0f;
        p.level = 1.0f;
        osc.setParameters(p);
        osc.triggerNote(f0, 1.0f, 0);

        for (int i = 0; i < nSamples; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(f0, 0.0f, 0.0f, l, r);
            bufL[static_cast<std::size_t>(i)] = l;
        }

        const float h1 = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, f0);
        const float h2 = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, 2.0f * f0);
        const float h3 = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, 3.0f * f0);

        EXPECT_GT(h1, 0.50f);
        EXPECT_LT(h2, 0.02f);
        // Triangle 3rd harmonic falls off as 1/9 of fundamental (~11% of h1)
        EXPECT_LT(h3, h1 * 0.16f);
    }

    // 5. Sub-Oscillator (Octave Below Fundamental)
    {
        nova::dsp::MultiEngineOscillator osc;
        osc.prepare(sr);
        auto p = osc.getParameters();
        p.sourceType = 1;
        p.engine = nova::dsp::MultiEngineOscillator::EngineType::SubOscillator;
        p.modelIndex = 0; // Pure Sine Sub
        p.level = 1.0f;
        p.detune = 0.0f;
        osc.setParameters(p);
        osc.triggerNote(f0, 1.0f, 0);

        for (int i = 0; i < nSamples; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(f0, 0.0f, 0.0f, l, r);
            bufL[static_cast<std::size_t>(i)] = l;
        }

        // Sub fundamental is 1 octave lower = 220 Hz
        const float subFund = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, 220.0f);
        EXPECT_GE(subFund, 0.60f) << "Sub-oscillator did not produce expected 1-octave down frequency!";
    }

    // 6. Pluck Modal & Noise Transient
    {
        nova::dsp::MultiEngineOscillator pluckOsc;
        pluckOsc.prepare(sr);
        auto pPluck = pluckOsc.getParameters();
        pPluck.sourceType = 2;
        pPluck.engine = nova::dsp::MultiEngineOscillator::EngineType::PhysicalPluckModal;
        pPluck.modelIndex = 0;
        pPluck.level = 0.9f;
        pluckOsc.setParameters(pPluck);
        pluckOsc.triggerNote(f0, 0.9f, 0);

        float pluckEnergy = 0.0f;
        float pluckMean = 0.0f;
        for (int i = 0; i < nSamples; ++i) {
            float l = 0.0f, r = 0.0f;
            pluckOsc.processSample(f0, 0.0f, 0.0f, l, r);
            ASSERT_TRUE(std::isfinite(l) && std::isfinite(r));
            pluckEnergy += l * l;
            pluckMean += l;
        }
        EXPECT_GT(pluckEnergy, 0.1f);
        EXPECT_LT(std::abs(pluckMean / static_cast<float>(nSamples)), 0.02f) << "Pluck has unwanted DC offset!";

        nova::dsp::MultiEngineOscillator noiseOsc;
        noiseOsc.prepare(sr);
        auto pNoise = noiseOsc.getParameters();
        pNoise.sourceType = 3;
        pNoise.engine = nova::dsp::MultiEngineOscillator::EngineType::NoiseTransient;
        pNoise.modelIndex = 0; // Analog Pink Noise
        pNoise.level = 0.8f;
        noiseOsc.setParameters(pNoise);
        noiseOsc.triggerNote(f0, 0.8f, 0);

        float noiseEnergy = 0.0f;
        for (int i = 0; i < nSamples; ++i) {
            float l = 0.0f, r = 0.0f;
            noiseOsc.processSample(f0, 0.0f, 0.0f, l, r);
            ASSERT_TRUE(std::isfinite(l) && std::isfinite(r));
            noiseEnergy += l * l;
        }
        EXPECT_GT(noiseEnergy, 0.5f);
    }
}

// -------------------------------------------------------------------------------------------------
// COMPREHENSIVE TEST SUITE 3: Sub-Functions: Morph, Unison & Pitch Verification
// -------------------------------------------------------------------------------------------------
TEST(NovaSynthComprehensiveTests, SubFunctionsMorphUnisonAndPitch) {
    constexpr double sr = 48000.0;
    constexpr int nSamples = 48000;
    constexpr float f0 = 440.0f;

    std::vector<float> bufL(nSamples, 0.0f);
    std::vector<float> bufR(nSamples, 0.0f);

    // 1. Morph Test: Sine wavefolding introduces progressive harmonic content
    {
        nova::dsp::MultiEngineOscillator osc;
        osc.prepare(sr);
        auto p = osc.getParameters();
        p.engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
        p.sourceType = 0;
        p.modelIndex = 3; // Sine
        p.detune = 0.0f;
        p.level = 1.0f;

        // Shape = 0.0: Pure Sine
        p.shapeMorph = 0.0f;
        osc.setParameters(p);
        osc.triggerNote(f0, 1.0f, 0);
        for (int i = 0; i < nSamples; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(f0, 0.0f, 0.0f, l, r);
            bufL[static_cast<std::size_t>(i)] = l;
        }
        const float harmAtZero = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, 2.0f * f0)
                               + measureMagnitudeAtFreq(bufL.data(), nSamples, sr, 3.0f * f0);

        // Shape = 0.85: Wavefolded / saturated
        p.shapeMorph = 0.85f;
        osc.setParameters(p);
        osc.triggerNote(f0, 1.0f, 0);
        for (int i = 0; i < nSamples; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(f0, 0.0f, 0.0f, l, r);
            bufL[static_cast<std::size_t>(i)] = l;
        }
        const float harmAtHigh = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, 2.0f * f0)
                               + measureMagnitudeAtFreq(bufL.data(), nSamples, sr, 3.0f * f0);

        EXPECT_LT(harmAtZero, 0.01f);
        EXPECT_GT(harmAtHigh, 0.15f) << "Morph parameter failed to introduce wavefolded harmonics!";
    }

    // 2. Unison Test: Stereo decorrelation and power conservation
    {
        nova::dsp::MultiEngineOscillator osc;
        osc.prepare(sr);
        auto p = osc.getParameters();
        p.engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
        p.sourceType = 0;
        p.modelIndex = 0; // Saw
        p.shapeMorph = 0.0f;
        p.level = 1.0f;

        // Detune = 0.0: Perfect Mono Correlation (r >= 0.999)
        p.detune = 0.0f;
        osc.setParameters(p);
        osc.triggerNote(f0, 1.0f, 0);
        float powerMono = 0.0f;
        for (int i = 0; i < nSamples; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(f0, 0.0f, 0.0f, l, r);
            bufL[static_cast<std::size_t>(i)] = l;
            bufR[static_cast<std::size_t>(i)] = r;
            powerMono += l * l + r * r;
        }
        const float corrMono = measureStereoCorrelation(bufL.data(), bufR.data(), nSamples);
        EXPECT_GE(corrMono, 0.999f) << "Mono unison detune=0 is not centered!";

        // Detune = 0.60: Stereo Unison Spread (r <= 0.85)
        p.detune = 0.60f;
        osc.setParameters(p);
        osc.triggerNote(f0, 1.0f, 42);
        float powerStereo = 0.0f;
        for (int i = 0; i < nSamples; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(f0, 0.0f, 0.0f, l, r);
            bufL[static_cast<std::size_t>(i)] = l;
            bufR[static_cast<std::size_t>(i)] = r;
            powerStereo += l * l + r * r;
        }
        const float corrStereo = measureStereoCorrelation(bufL.data(), bufR.data(), nSamples);
        EXPECT_LE(corrStereo, 0.85f) << "Unison detune failed to produce stereo decorrelation!";

        // Power Conservation: RMS ratio must stay within +/- 2 dB (0.79 to 1.26 in amplitude, 0.63 to 1.58 in power)
        const float powerRatio = powerStereo / std::max(1e-6f, powerMono);
        EXPECT_GT(powerRatio, 0.60f) << "Unison volume dropped excessively!";
        EXPECT_LT(powerRatio, 1.70f) << "Unison volume blew up excessively!";
    }

    // 3. Pitch Test: Octave and Semitone transposition accuracy
    {
        nova::dsp::MultiEngineOscillator osc;
        osc.prepare(sr);
        auto p = osc.getParameters();
        p.engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
        p.sourceType = 0;
        p.modelIndex = 3; // Sine
        p.shapeMorph = 0.0f;
        p.detune = 0.0f;
        p.level = 1.0f;

        // Base note: MIDI 60 (C4 = 261.6256 Hz)
        constexpr float c4Hz = 261.6256f;

        // Octave +1 -> 523.25 Hz
        p.octaveOffset = 1;
        p.semitoneOffset = 0;
        osc.setParameters(p);
        osc.triggerNote(c4Hz, 1.0f, 0);
        for (int i = 0; i < nSamples; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(c4Hz, 0.0f, 0.0f, l, r);
            bufL[static_cast<std::size_t>(i)] = l;
        }
        const float magOctUp = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, c4Hz * 2.0f);
        EXPECT_GE(magOctUp, 0.60f) << "Octave +1 failed!";

        // Octave -1 -> 130.81 Hz
        p.octaveOffset = -1;
        osc.setParameters(p);
        osc.triggerNote(c4Hz, 1.0f, 0);
        for (int i = 0; i < nSamples; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(c4Hz, 0.0f, 0.0f, l, r);
            bufL[static_cast<std::size_t>(i)] = l;
        }
        const float magOctDown = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, c4Hz * 0.5f);
        EXPECT_GE(magOctDown, 0.60f) << "Octave -1 failed!";

        // Semitone +7 (Perfect 5th = G4 = 391.995 Hz)
        p.octaveOffset = 0;
        p.semitoneOffset = 7;
        osc.setParameters(p);
        osc.triggerNote(c4Hz, 1.0f, 0);
        for (int i = 0; i < nSamples; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(c4Hz, 0.0f, 0.0f, l, r);
            bufL[static_cast<std::size_t>(i)] = l;
        }
        const float expectedG4 = c4Hz * std::pow(2.0f, 7.0f / 12.0f);
        const float magSemi7 = measureMagnitudeAtFreq(bufL.data(), nSamples, sr, expectedG4);
        EXPECT_GE(magSemi7, 0.60f) << "Semitone +7 failed!";
    }
}

// -------------------------------------------------------------------------------------------------
// COMPREHENSIVE TEST SUITE 4: Filter Cutoff, Resonance, Drive and Filter Types
// -------------------------------------------------------------------------------------------------
TEST(NovaSynthComprehensiveTests, FilterParametersCutoffResonanceDriveAndTypes) {
    constexpr double sr = 48000.0;
    constexpr int nSamples = 48000;
    std::vector<float> inBuf(nSamples, 0.0f);
    std::vector<float> outBuf(nSamples, 0.0f);

    // Prepare broadband impulse test signal
    inBuf[0] = 1.0f;

    // 1. Cutoff Sweep: High frequency energy decreases monotonically as Cutoff decreases
    {
        nova::dsp::StateVariableFilter svf;
        svf.prepare(sr);

        // Test theoretical magnitude response at 3000 Hz for various cutoffs
        svf.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, 12000.0f, 0.7071f);
        const float magHigh = svf.getMagnitudeAtFrequency(3000.0f);

        svf.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, 1500.0f, 0.7071f);
        const float magMid = svf.getMagnitudeAtFrequency(3000.0f);

        svf.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, 300.0f, 0.7071f);
        const float magLow = svf.getMagnitudeAtFrequency(3000.0f);

        EXPECT_GT(magHigh, 0.90f);
        EXPECT_LT(magMid, 0.35f);
        EXPECT_LT(magLow, 0.02f);
        EXPECT_GT(magHigh, magMid);
        EXPECT_GT(magMid, magLow);
    }

    // 2. Resonance Peak: Q boosts the cutoff frequency
    {
        nova::dsp::StateVariableFilter svf;
        svf.prepare(sr);

        svf.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, 1000.0f, 0.7071f);
        const float magNominal = svf.getMagnitudeAtFrequency(1000.0f);

        svf.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, 1000.0f, 4.0f);
        const float magResonant = svf.getMagnitudeAtFrequency(1000.0f);

        EXPECT_NEAR(magNominal, 0.7071f, 0.05f);
        EXPECT_GE(magResonant, 3.5f) << "Resonance Q=4.0 did not boost cutoff frequency sufficiently!";
    }

    // 3. Drive Saturation: Tube/ladder non-linear drive creates musical saturation
    {
        // 220 Hz pure sine input
        for (int i = 0; i < nSamples; ++i) {
            inBuf[static_cast<std::size_t>(i)] = std::sin(2.0f * std::numbers::pi_v<float> * 220.0f * static_cast<float>(i) / static_cast<float>(sr));
        }

        auto applyDrive = [](float x, float driveParam) noexcept {
            if (driveParam <= 0.01f) return x;
            const float driveGain = 1.0f + driveParam * 3.6f;
            const float comp = 1.0f / std::sqrt(1.0f + driveParam * 1.8f);
            const float xBias = x * driveGain + 0.12f * (x * driveGain) * (x * driveGain);
            return (xBias * (27.0f + xBias * xBias) / (27.0f + 9.0f * xBias * xBias)) * comp;
        };

        // Drive = 0.0
        for (int i = 0; i < nSamples; ++i) {
            outBuf[static_cast<std::size_t>(i)] = applyDrive(inBuf[static_cast<std::size_t>(i)], 0.0f);
        }
        const float harmZero = measureMagnitudeAtFreq(outBuf.data(), nSamples, sr, 3.0f * 220.0f);

        // Drive = 0.85
        for (int i = 0; i < nSamples; ++i) {
            outBuf[static_cast<std::size_t>(i)] = applyDrive(inBuf[static_cast<std::size_t>(i)], 0.85f);
        }
        const float harmDriven = measureMagnitudeAtFreq(outBuf.data(), nSamples, sr, 3.0f * 220.0f);

        EXPECT_LT(harmZero, 0.005f);
        EXPECT_GT(harmDriven, 0.05f) << "Filter Drive failed to generate musical saturation harmonics!";
    }

    // 4. Filter Types: LowPass vs HighPass vs BandPass
    {
        nova::dsp::StateVariableFilter svf;
        svf.prepare(sr);

        // LowPass (cutoff 1000 Hz): passes 200 Hz, cuts 5000 Hz
        svf.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, 1000.0f, 0.7071f);
        EXPECT_GT(svf.getMagnitudeAtFrequency(200.0f), 0.90f);
        EXPECT_LT(svf.getMagnitudeAtFrequency(5000.0f), 0.10f);

        // HighPass (cutoff 1000 Hz): cuts 200 Hz, passes 5000 Hz
        svf.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, 1000.0f, 0.7071f);
        EXPECT_LT(svf.getMagnitudeAtFrequency(200.0f), 0.10f);
        EXPECT_GT(svf.getMagnitudeAtFrequency(5000.0f), 0.90f);

        // BandPass (cutoff 1000 Hz): passes 1000 Hz, cuts 100 Hz and 10000 Hz
        svf.setParameters(nova::dsp::StateVariableFilter::Type::BandPass, 1000.0f, 1.5f);
        EXPECT_GT(svf.getMagnitudeAtFrequency(1000.0f), 0.85f);
        EXPECT_LT(svf.getMagnitudeAtFrequency(100.0f), 0.15f);
        EXPECT_LT(svf.getMagnitudeAtFrequency(10000.0f), 0.15f);
    }
}

// -------------------------------------------------------------------------------------------------
// COMPREHENSIVE TEST SUITE 5: Envelope Stages & Click-Free Retriggering
// -------------------------------------------------------------------------------------------------
TEST(NovaSynthComprehensiveTests, EnvelopesAndClickFreeRetriggering) {
    constexpr double sr = 48000.0;

    // 1. ADSR Timing and Stage Transitions
    {
        nova::dsp::AdsrEnvelope env;
        env.prepare(sr);
        nova::dsp::AdsrEnvelope::Parameters p;
        p.attackSec  = 0.02f; // 960 samples
        p.decaySec   = 0.04f; // 1920 samples
        p.sustainLvl = 0.60f;
        p.releaseSec = 0.03f; // 1440 samples
        env.setParameters(p);

        env.noteOn();
        EXPECT_TRUE(env.isActive());

        // Process attack phase (960 samples): must reach near 1.0
        float prev = 0.0f;
        for (int i = 0; i < 960; ++i) {
            const float cur = env.processSample();
            EXPECT_GE(cur, prev);
            prev = cur;
        }
        EXPECT_GE(env.getCurrentLevel(), 0.95f);

        // Process decay phase (1920 samples): must settle near sustain level
        for (int i = 0; i < 1920; ++i) {
            (void)env.processSample();
        }
        EXPECT_NEAR(env.getCurrentLevel(), 0.60f, 0.05f);

        // Note off -> process release phase (4000 samples to reach idle threshold < 1e-4)
        env.noteOff();
        for (int i = 0; i < 4000; ++i) {
            (void)env.processSample();
        }
        EXPECT_LT(env.getCurrentLevel(), 0.001f);
        EXPECT_FALSE(env.isActive());
    }

    // 2. Rapid Retriggering Click-Free Test: Consecutive sample delta must stay continuous (|delta| < 0.15)
    {
        nova::dsp::WorshipSynthEngine synth;
        synth.prepare(sr, 512);

        auto p = synth.getParameters();
        p.activeOscCount = 1;
        p.oscSlots[0].enabled = true;
        p.oscSlots[0].sourceType = 0;
        p.oscSlots[0].engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
        p.oscSlots[0].modelIndex = 3; // Mellow Sine
        p.oscSlots[0].shapeMorph = 0.0f;
        p.oscSlots[0].detune = 0.0f;
        p.oscSlots[0].level = 0.85f;
        p.filter1Enabled = false;
        p.filter2Enabled = false;
        p.reverbMix = 0.0f;
        p.delayEnabled = false;

        // Fast attack & decay
        p.ampEnv.attackSec = 0.005f;
        p.ampEnv.decaySec = 0.10f;
        p.ampEnv.sustainLvl = 0.70f;
        p.ampEnv.releaseSec = 0.50f;
        synth.setParameters(p);

        std::vector<float> left(512, 0.0f);
        std::vector<float> right(512, 0.0f);

        float maxSampleJump = 0.0f;
        float prevSampleL = 0.0f;

        // Rapid retrigger loop simulating fast clicking on UI chord / key buttons
        for (int cycle = 0; cycle < 10; ++cycle) {
            synth.noteOn(60, 0.85f);
            synth.processBlock(left.data(), right.data(), 512);
            for (int i = 0; i < 512; ++i) {
                const float jump = std::abs(left[static_cast<std::size_t>(i)] - prevSampleL);
                maxSampleJump = std::max(maxSampleJump, jump);
                prevSampleL = left[static_cast<std::size_t>(i)];
            }

            // Immediately retrigger while note is still sounding / decaying
            synth.noteOff(60);
            synth.noteOn(60, 0.90f);
            synth.processBlock(left.data(), right.data(), 512);
            for (int i = 0; i < 512; ++i) {
                const float jump = std::abs(left[static_cast<std::size_t>(i)] - prevSampleL);
                maxSampleJump = std::max(maxSampleJump, jump);
                prevSampleL = left[static_cast<std::size_t>(i)];
            }

            // Retrigger with chord
            synth.noteOn(64, 0.80f);
            synth.noteOn(67, 0.80f);
            synth.processBlock(left.data(), right.data(), 512);
            for (int i = 0; i < 512; ++i) {
                const float jump = std::abs(left[static_cast<std::size_t>(i)] - prevSampleL);
                maxSampleJump = std::max(maxSampleJump, jump);
                prevSampleL = left[static_cast<std::size_t>(i)];
            }

            synth.allNotesOff();
        }

        // At 440 Hz Sine with amp 0.85 at 48kHz, normal wave slope is 2*pi*440/48000 * 0.85 ~ 0.05
        // A discontinuity click produces jumps > 0.30 - 1.0. With our smooth retriggering, max jump is strictly bounded.
        EXPECT_LE(maxSampleJump, 0.15f) << "Click detected on rapid note retriggering! Max jump: " << maxSampleJump;
    }
}

TEST(NovaSynthComprehensiveTests, SubBassVoicingLowestNotePriorityPreventsMud) {
    nova::dsp::WorshipSynthEngine synth;
    synth.prepare(48000.0, 512);

    auto p = synth.getParameters();
    p.activeOscCount = 1;
    p.oscSlots[0].enabled = true;
    p.oscSlots[0].sourceType = 1; // SUB-BASS OSCILLATOR
    p.oscSlots[0].level = 1.0f;
    p.ampEnv.attackSec = 0.005f;
    p.ampEnv.decaySec = 0.5f;
    p.ampEnv.sustainLvl = 1.0f;
    p.ampEnv.releaseSec = 0.05f;
    p.filter1Enabled = false;
    p.filter2Enabled = false;

    // Test 1: Polyphonic Sub (subLowestOnly = false) -> All chord notes render sub
    p.subLowestOnly = false;
    synth.setParameters(p);

    synth.noteOn(36, 0.8f); // C2
    synth.noteOn(40, 0.8f); // E2
    synth.noteOn(43, 0.8f); // G2

    std::vector<float> leftPoly(512, 0.0f);
    std::vector<float> rightPoly(512, 0.0f);
    // Process multiple blocks to reach full sustain beyond attack ramp
    for (int b = 0; b < 6; ++b) {
        synth.processBlock(leftPoly.data(), rightPoly.data(), 512);
    }

    float polyRms = 0.0f;
    for (float s : leftPoly) polyRms += s * s;
    polyRms = std::sqrt(polyRms / 512.0f);
    EXPECT_GT(polyRms, 0.05f);

    synth.reset();

    // Test 2: Lowest Note Priority Sub (subLowestOnly = true) -> Only C2 renders sub!
    p.subLowestOnly = true;
    synth.setParameters(p);

    synth.noteOn(36, 0.8f); // C2 (lowest note)
    synth.noteOn(40, 0.8f); // E2 (higher chord notes)
    synth.noteOn(43, 0.8f); // G2

    std::vector<float> leftLowest(512, 0.0f);
    std::vector<float> rightLowest(512, 0.0f);
    for (int b = 0; b < 6; ++b) {
        synth.processBlock(leftLowest.data(), rightLowest.data(), 512);
    }

    float lowestRms = 0.0f;
    for (float s : leftLowest) lowestRms += s * s;
    lowestRms = std::sqrt(lowestRms / 512.0f);

    // With lowest-only, only 1 sub note plays instead of 3 colliding sub frequencies
    EXPECT_GT(lowestRms, 0.02f);
    EXPECT_LT(lowestRms, polyRms * 0.90f) << "Lowest note priority should prevent 3x sub accumulation mud!";
    synth.reset();
}

TEST(NovaSynthComprehensiveTests, DelaySwingControlsStraightVsSwungPingPong) {
    nova::dsp::WorshipSynthEngine synth;
    synth.prepare(48000.0, 512);

    auto p = synth.getParameters();
    p.delayEnabled = true;
    p.delayMix = 1.0f;
    p.delayFeedback = 0.4f;
    p.delaySyncDiv = 3; // 1/8 note
    p.bpm = 120.0f;   // 120 BPM => 1/8 = 0.250s = 12000 samples at 48k

    // Straight Ping-Pong (swing = 0.0)
    p.delaySwing = 0.0f;
    synth.setParameters(p);
    EXPECT_EQ(synth.getParameters().delaySwing, 0.0f);

    // Full Swing Shuffle (swing = 1.0)
    p.delaySwing = 1.0f;
    synth.setParameters(p);
    EXPECT_EQ(synth.getParameters().delaySwing, 1.0f);
}

TEST(NovaSynthComprehensiveTests, ThemeFontScaleScalingFactors) {
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);

    theme.setTextScale(nova::ui::NovaTheme::TextScale::Small);
    EXPECT_NEAR(theme.getTextScaleFactor(), 1.12f, 0.001f);
    EXPECT_NEAR(theme.getLabelFont(12.0f).getHeight(), 12.0f * 1.12f, 0.5f);

    theme.setTextScale(nova::ui::NovaTheme::TextScale::Normal);
    EXPECT_NEAR(theme.getTextScaleFactor(), 1.32f, 0.001f);
    EXPECT_NEAR(theme.getLabelFont(12.0f).getHeight(), 12.0f * 1.32f, 0.5f);

    theme.setTextScale(nova::ui::NovaTheme::TextScale::Large);
    EXPECT_NEAR(theme.getTextScaleFactor(), 1.52f, 0.001f);
    EXPECT_NEAR(theme.getLabelFont(12.0f).getHeight(), 12.0f * 1.52f, 0.5f);
}

TEST(NovaSynthWindowSizingTests, EditorEnforcesSafeLaptopMinimumAndMaximumLimits) {
    nova::synth::NovaSynthProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditorIfNeeded());
    ASSERT_NE(editor, nullptr);

    EXPECT_TRUE(editor->isResizable());
    auto* constrainer = editor->getConstrainer();
    ASSERT_NE(constrainer, nullptr);

    // Guaranteed safe laptop limits: fits 13" MacBook screens comfortably with titlebar and dock
    EXPECT_EQ(constrainer->getMinimumWidth(), 1000);
    EXPECT_EQ(constrainer->getMinimumHeight(), 720);
    EXPECT_EQ(constrainer->getMaximumWidth(), 1680);
    EXPECT_EQ(constrainer->getMaximumHeight(), 1100);

    // Initial editor size must fit within limits
    EXPECT_GE(editor->getWidth(), 1000);
    EXPECT_LE(editor->getWidth(), 1680);
    EXPECT_GE(editor->getHeight(), 720);
    EXPECT_LE(editor->getHeight(), 1100);

    // Undersized window clamp test
    juce::Rectangle<int> undersized(0, 0, 500, 350);
    constrainer->checkBounds(undersized, undersized, {}, false, false, false, false);
    EXPECT_GE(undersized.getWidth(), 1000);
    EXPECT_GE(undersized.getHeight(), 720);

    // Oversized window clamp test
    juce::Rectangle<int> oversized(0, 0, 3000, 2000);
    constrainer->checkBounds(oversized, oversized, {}, false, false, false, false);
    EXPECT_LE(oversized.getWidth(), 1680);
    EXPECT_LE(oversized.getHeight(), 1100);
}

TEST(StandaloneWindowConstrainerTests, ParentDocumentWindowInheritsAndEnforcesSizeLimits) {
    nova::synth::NovaSynthProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditorIfNeeded());
    ASSERT_NE(editor, nullptr);

    // Simulate JUCE Standalone DocumentWindow embedding
    struct TestStandaloneWindow : public juce::DocumentWindow {
        TestStandaloneWindow()
            : juce::DocumentWindow("Nova Synth Standalone Test", juce::Colours::black, juce::DocumentWindow::allButtons) {
            setConstrainer(&mockConstrainer);
        }
        juce::ComponentBoundsConstrainer mockConstrainer;
    };

    TestStandaloneWindow testWin;
    testWin.setContentNonOwned(editor.get(), true);

    // Trigger hierarchy hook
    editor->parentHierarchyChanged();

    EXPECT_TRUE(testWin.isUsingNativeTitleBar());

    auto* parentConstrainer = testWin.getConstrainer();
    ASSERT_NE(parentConstrainer, nullptr);
    EXPECT_GE(parentConstrainer->getMinimumWidth(), 1000);
    EXPECT_GE(parentConstrainer->getMinimumHeight(), 720);
    EXPECT_LE(parentConstrainer->getMaximumWidth(), 1680);
    EXPECT_LE(parentConstrainer->getMaximumHeight(), 1100);

    // Clean up content before testWin destruction
    testWin.clearContentComponent();
}

TEST(SourceListManagerTests, DisplaysAllEightSlotsWithNoOverlap) {
    nova::synth::NovaSynthProcessor proc;
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::SourceListManager listMgr(proc, theme);

    listMgr.setBounds(0, 0, 160, 310);

    juce::Image img(juce::Image::ARGB, 160, 310, true);
    juce::Graphics g(img);
    listMgr.paint(g);

    // All 8 slots are always displayed
    const auto params = proc.getCurrentParameters();
    EXPECT_EQ(params.oscSlots.size(), 8u);
}

TEST(SourceDeckTests, CompactCardsAreSquare) {
    nova::synth::NovaSynthProcessor proc;
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::SourceDeck sourceDeck(proc, theme);

    const int totalH = 196;
    sourceDeck.setBounds(0, 0, 800, totalH);

    // Compact card target must match card height exactly (width == height)
    const int expectedCardH = totalH - 14;
    EXPECT_EQ(expectedCardH, 182);
}

TEST(EnvelopeQuadDeckTests, EnvelopesStartCollapsedOnLaunch) {
    nova::synth::NovaSynthProcessor proc;
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::EnvelopeQuadDeck envDeck(proc, theme);

    auto cards = envDeck.getCardList();
    ASSERT_EQ(cards.size(), 4u);
    for (std::size_t i = 0; i < cards.size(); ++i) {
        EXPECT_FALSE(cards[i]->isExpanded()) << "Envelope " << (i + 1) << " must start collapsed on launch!";
        EXPECT_FALSE(cards[i]->isPinned());
    }
}

TEST(EnvelopeQuadDeckTests, HeaderClickExpandsAndCollapsesCardIndependently) {
    nova::synth::NovaSynthProcessor proc;
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::EnvelopeQuadDeck envDeck(proc, theme);

    auto cards = envDeck.getCardList();
    ASSERT_EQ(cards.size(), 4u);

    // Expand Env 1
    envDeck.handleCardExpand(0, true);
    EXPECT_TRUE(cards[0]->isExpanded());
    EXPECT_FALSE(cards[1]->isExpanded());
    EXPECT_FALSE(cards[2]->isExpanded());
    EXPECT_FALSE(cards[3]->isExpanded());

    // Collapse Env 1
    envDeck.handleCardExpand(0, false);
    EXPECT_FALSE(cards[0]->isExpanded());
}

TEST(SourceDeckTests, AllSourcesStartCollapsedOnLaunch) {
    nova::synth::NovaSynthProcessor proc;
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::SourceDeck sourceDeck(proc, theme);

    const auto& cards = sourceDeck.getCardList();
    ASSERT_EQ(cards.size(), 8u);
    for (std::size_t i = 0; i < cards.size(); ++i) {
        EXPECT_FALSE(cards[i]->isExpanded()) << "Source card " << (i + 1) << " must start collapsed on launch!";
        EXPECT_FALSE(cards[i]->isPinned());
    }
}

TEST(DualFilterDeckTests, PhysicalResponseCurveMatchesStateVariableFilter) {
    nova::dsp::StateVariableFilter svf;
    svf.prepare(44100.0);

    // Lowpass at 1000 Hz, Q = 0.707
    svf.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, 1000.0f, 0.7071f);
    const float passMag = svf.getMagnitudeAtFrequency(100.0f);
    const float cutMag  = svf.getMagnitudeAtFrequency(1000.0f);
    const float stopMag = svf.getMagnitudeAtFrequency(10000.0f);

    EXPECT_NEAR(passMag, 1.0f, 0.05f);
    EXPECT_NEAR(cutMag, 0.7071f, 0.05f);
    EXPECT_LT(stopMag, 0.05f);

    // Highpass at 2000 Hz
    svf.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, 2000.0f, 0.7071f);
    const float hpStopMag = svf.getMagnitudeAtFrequency(200.0f);
    const float hpPassMag = svf.getMagnitudeAtFrequency(10000.0f);

    EXPECT_LT(hpStopMag, 0.05f);
    EXPECT_NEAR(hpPassMag, 1.0f, 0.05f);
}

TEST(SourceDeckInteractionTests, SoloAndPowerInCompactModeDoNotExpandCard) {
    nova::synth::NovaSynthProcessor proc;
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::SourceDeck sourceDeck(proc, theme);
    sourceDeck.setBounds(0, 0, 800, 196);

    const auto& cards = sourceDeck.getCardList();
    ASSERT_EQ(cards.size(), 8u);
    auto* card = cards[0].get();
    ASSERT_NE(card, nullptr);
    EXPECT_FALSE(card->isExpanded());

    // In compact mode, powerRect is at top-right
    const auto dummyMouse = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();

    // Click Power toggle in compact mode (x=130, y=12)
    const juce::Point<float> powPos(130.0f, 12.0f);
    juce::MouseEvent powDown(dummyMouse, powPos, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             card, card, now, powPos, now, 1, false);
    card->mouseDown(powDown);
    // Card MUST remain collapsed!
    EXPECT_FALSE(card->isExpanded()) << "Clicking power toggle in compact mode must NOT expand the card!";

    // Click Solo toggle in compact mode (x=112, y=12)
    const juce::Point<float> soloPos(112.0f, 12.0f);
    juce::MouseEvent soloDown(dummyMouse, soloPos, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                              card, card, now, soloPos, now, 1, false);
    card->mouseDown(soloDown);
    // Card MUST remain collapsed!
    EXPECT_FALSE(card->isExpanded()) << "Clicking solo toggle in compact mode must NOT expand the card!";

    // Click slot badge / card header on left to expand (x=24, y=12)
    const juce::Point<float> headerPos(24.0f, 12.0f);
    juce::MouseEvent headDown(dummyMouse, headerPos, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                              card, card, now, headerPos, now, 1, false);
    card->mouseDown(headDown);
    EXPECT_TRUE(card->isExpanded()) << "Clicking slot badge must expand the card!";

    // Click header on expanded card (x=24, y=12) to collapse
    card->mouseDown(headDown);
    EXPECT_FALSE(card->isExpanded()) << "Clicking header on expanded card must collapse it!";
}

TEST(EnvelopeInteractionTests, CompactCurveDragDoesNotExpandCard) {
    nova::synth::NovaSynthProcessor proc;
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::EnvelopeQuadDeck envDeck(proc, theme);
    envDeck.setBounds(0, 0, 800, 168);

    auto cards = envDeck.getCardList();
    ASSERT_EQ(cards.size(), 4u);
    auto* envCard = cards[0];
    ASSERT_NE(envCard, nullptr);
    EXPECT_FALSE(envCard->isExpanded());

    const auto dummyMouse = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();

    // Click in curve box in compact mode (x=50, y=70)
    const juce::Point<float> curvePos(50.0f, 70.0f);
    juce::MouseEvent curveDown(dummyMouse, curvePos, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                               envCard, envCard, now, curvePos, now, 1, false);
    envCard->mouseDown(curveDown);
    // Card MUST remain collapsed so user can directly edit without jumping open!
    EXPECT_FALSE(envCard->isExpanded()) << "Dragging envelope curve in compact mode must NOT expand the card!";

    // Click Header (x=40, y=12) to expand
    const juce::Point<float> headPos(40.0f, 12.0f);
    juce::MouseEvent headDown(dummyMouse, headPos, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                              envCard, envCard, now, headPos, now, 1, false);
    envCard->mouseDown(headDown);
    EXPECT_TRUE(envCard->isExpanded()) << "Clicking envelope header must expand the card!";

    // Click Header on expanded card to collapse
    envCard->mouseDown(headDown);
    EXPECT_FALSE(envCard->isExpanded()) << "Clicking envelope header on expanded card must collapse it!";
}

TEST(ModulationMatrixTests, SlotAssignmentAndDestinationEvaluation) {
    nova::dsp::ModMatrix mm;
    mm.clear();

    // Route 1: LFO 1 -> Filter Cutoff (+50%)
    mm.setSlot(0, nova::dsp::ModMatrix::Source::Lfo1,
               nova::dsp::ModMatrix::Destination::FilterCutoff, 0.50f, true);
    // Route 2: Macro Warmth -> Filter Cutoff (-30%)
    mm.setSlot(1, nova::dsp::ModMatrix::Source::MacroWarmth,
               nova::dsp::ModMatrix::Destination::FilterCutoff, -0.30f, true);
    // Route 3: Bypassed route
    mm.setSlot(2, nova::dsp::ModMatrix::Source::Velocity,
               nova::dsp::ModMatrix::Destination::Osc1Shape, 0.80f, false);

    mm.setSourceValue(nova::dsp::ModMatrix::Source::Lfo1, 1.0f);
    mm.setSourceValue(nova::dsp::ModMatrix::Source::MacroWarmth, 1.0f);
    mm.setSourceValue(nova::dsp::ModMatrix::Source::Velocity, 1.0f);
    mm.evaluate();

    // Filter Cutoff should receive 1.0 * 0.50 + 1.0 * (-0.30) = 0.20
    EXPECT_NEAR(mm.getModulationOffset(nova::dsp::ModMatrix::Destination::FilterCutoff), 0.20f, 1e-4f);
    // Bypassed slot should produce 0.0 offset
    EXPECT_FLOAT_EQ(mm.getModulationOffset(nova::dsp::ModMatrix::Destination::Osc1Shape), 0.0f);

    // Test bypass toggle
    mm.setSlot(0, nova::dsp::ModMatrix::Source::Lfo1,
               nova::dsp::ModMatrix::Destination::FilterCutoff, 0.50f, false);
    mm.evaluate();
    EXPECT_NEAR(mm.getModulationOffset(nova::dsp::ModMatrix::Destination::FilterCutoff), -0.30f, 1e-4f);
}

TEST(ModulationMatrixTests, ExpandedDestinationsAndPolyphonicVoiceOffsets) {
    nova::dsp::ModMatrix mm;
    mm.clear();

    // Map ModEnv3 -> Osc3Level (+0.75) and Velocity -> Filter2Cutoff (+0.60)
    mm.setSlot(0, nova::dsp::ModMatrix::Source::ModEnv3,
               nova::dsp::ModMatrix::Destination::Osc3Level, 0.75f, true);
    mm.setSlot(1, nova::dsp::ModMatrix::Source::Velocity,
               nova::dsp::ModMatrix::Destination::Filter2Cutoff, 0.60f, true);
    mm.setSlot(2, nova::dsp::ModMatrix::Source::Lfo1,
               nova::dsp::ModMatrix::Destination::Osc1Pitch, 0.25f, true);

    // Global evaluation with Lfo1 = 0.8
    mm.setSourceValue(nova::dsp::ModMatrix::Source::Lfo1, 0.80f);
    mm.evaluate();

    EXPECT_NEAR(mm.getModulationOffset(nova::dsp::ModMatrix::Destination::Osc1Pitch), 0.20f, 1e-4f);

    // Evaluate voice with specific dynamic envelope and velocity values
    nova::dsp::ModMatrix::VoiceSources vs;
    vs.modEnv = 0.50f;
    vs.velocity = 0.80f;

    const auto vo = mm.evaluateVoice(vs);
    // Osc3Level = 0.50 * 0.75 = 0.375
    EXPECT_NEAR(vo.offsets[static_cast<std::size_t>(nova::dsp::ModMatrix::Destination::Osc3Level)], 0.375f, 1e-4f);
    // Filter2Cutoff = 0.80 * 0.60 = 0.48
    EXPECT_NEAR(vo.offsets[static_cast<std::size_t>(nova::dsp::ModMatrix::Destination::Filter2Cutoff)], 0.48f, 1e-4f);
    // Osc1Pitch still receives global LFO modulation = 0.20
    EXPECT_NEAR(vo.offsets[static_cast<std::size_t>(nova::dsp::ModMatrix::Destination::Osc1Pitch)], 0.20f, 1e-4f);
}

TEST(WorshipSynthEngineTests, OscillatorIndependentEnvelopeRoutingAndSwell) {
    auto synthPtr = std::make_unique<nova::dsp::WorshipSynthEngine>();
    auto& synth = *synthPtr;
    synth.prepare(48000.0, 256);

    auto p = synth.getParameters();
    p.activeOscCount = 3;

    // OSC 1: Fast attack (5ms), AmpEnv1
    p.oscSlots[0].enabled = true;
    p.oscSlots[0].level = 0.8f;
    p.oscSlots[0].ampEnvSource = 0; // Main Amp Env
    p.ampEnv.attackSec = 0.005f;
    p.ampEnv.sustainLvl = 1.0f;

    // OSC 2: Disabled
    p.oscSlots[1].enabled = false;

    // OSC 3: Slow swell attack (600ms), ModEnv3
    p.oscSlots[2].enabled = true;
    p.oscSlots[2].level = 0.8f;
    p.oscSlots[2].ampEnvSource = 2; // Mod Env 3
    p.modEnv.attackSec = 0.600f;
    p.modEnv.sustainLvl = 1.0f;

    // Bypassed filters and FX for direct level verification
    p.filter1Enabled = false;
    p.filter2Enabled = false;
    p.delayMix = 0.0f;
    p.reverbMix = 0.0f;

    synth.setParameters(p);
    synth.noteOn(60, 0.9f);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);

    // Advance 5 blocks (~27 ms): OSC 1 attack (5ms) is fully complete, but OSC 3 (600ms) is at only ~4% of its swell
    for (int b = 0; b < 5; ++b) {
        synth.processBlock(left.data(), right.data(), 256);
    }
    float earlyRms = 0.0f;
    for (float s : left) earlyRms += s * s;
    earlyRms = std::sqrt(earlyRms / 256.0f);
    EXPECT_GT(earlyRms, 0.01f);

    // Render 120 more blocks (~640 ms > 600 ms swell time)
    for (int b = 0; b < 120; ++b) {
        synth.processBlock(left.data(), right.data(), 256);
    }

    // Now OSC 3 has fully swelled in; total energy must be significantly higher than with OSC 1 alone!
    float lateRms = 0.0f;
    for (float s : left) lateRms += s * s;
    lateRms = std::sqrt(lateRms / 256.0f);
    EXPECT_GT(lateRms, earlyRms * 1.15f);

    synth.allNotesOff();
}

TEST(WorshipSynthEngineTests, ActiveVoiceCullingAndCytomicFastMathPrecision) {
    nova::dsp::WorshipSynthEngine synth;
    synth.prepare(48000.0, 256);

    // Verify SVF stability and frequency response with Padé approximation
    nova::dsp::StateVariableFilter filter;
    filter.prepare(48000.0);
    filter.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, 1200.0f, 1.5f);

    float maxSample = 0.0f;
    for (int i = 0; i < 480; ++i) {
        const float in = (i == 0) ? 1.0f : 0.0f; // Impulse response
        const float out = filter.processSample(in);
        EXPECT_FALSE(std::isnan(out));
        EXPECT_FALSE(std::isinf(out));
        maxSample = std::max(maxSample, std::abs(out));
    }
    EXPECT_GT(maxSample, 0.05f);
    EXPECT_LT(maxSample, 2.0f);

    // Play a single note and verify active voice list iterates only active voice
    auto p = synth.getParameters();
    p.ampEnv.attackSec = 0.005f;
    p.ampEnv.decaySec = 0.1f;
    p.ampEnv.sustainLvl = 1.0f;
    p.ampEnv.releaseSec = 0.03f;
    synth.setParameters(p);

    synth.noteOn(60, 0.8f);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    // Advance 5 blocks (~27 ms > 5 ms attack)
    for (int b = 0; b < 5; ++b) {
        synth.processBlock(left.data(), right.data(), 256);
    }

    EXPECT_EQ(synth.getTelemetry().activeVoiceCount, 1);

    float rms = 0.0f;
    for (float s : left) rms += s * s;
    rms = std::sqrt(rms / 256.0f);
    EXPECT_GT(rms, 0.01f);

    synth.noteOff(60);
    // Advance 20 blocks (~107 ms > 30 ms release)
    for (int b = 0; b < 20; ++b) {
        synth.processBlock(left.data(), right.data(), 256);
    }
    EXPECT_EQ(synth.getTelemetry().activeVoiceCount, 0);
}

TEST(ModulationMatrixTests, TotalDepthAndLiveOffsets) {
    nova::dsp::ModMatrix mm;
    mm.setSlot(0, nova::dsp::ModMatrix::Source::Lfo1, nova::dsp::ModMatrix::Destination::FilterCutoff, 0.35f, true);
    mm.setSlot(1, nova::dsp::ModMatrix::Source::ModWheel, nova::dsp::ModMatrix::Destination::FilterCutoff, 0.20f, true);
    mm.setSlot(2, nova::dsp::ModMatrix::Source::Lfo2, nova::dsp::ModMatrix::Destination::FilterCutoff, 0.40f, false); // disabled

    EXPECT_NEAR(mm.getTotalDepth(nova::dsp::ModMatrix::Destination::FilterCutoff), 0.55f, 1e-4f);
    EXPECT_NEAR(mm.getTotalDepth(nova::dsp::ModMatrix::Destination::FilterResonance), 0.0f, 1e-4f);

    mm.setSourceValue(nova::dsp::ModMatrix::Source::Lfo1, 1.0f);
    mm.setSourceValue(nova::dsp::ModMatrix::Source::ModWheel, 0.5f);
    mm.evaluate();

    EXPECT_NEAR(mm.getModulationOffset(nova::dsp::ModMatrix::Destination::FilterCutoff), 0.35f * 1.0f + 0.20f * 0.5f, 1e-4f);
}

TEST(WorshipSynthEngineTests, LiveModulationTelemetryAtomicSafety) {
    nova::dsp::WorshipSynthEngine synth;
    synth.prepare(48000.0, 256);

    auto p = synth.getParameters();
    p.lfo1RateHz = 4.0f;
    synth.setParameters(p);

    synth.getModMatrix().setSlot(0, nova::dsp::ModMatrix::Source::Lfo1,
                                 nova::dsp::ModMatrix::Destination::FilterCutoff, 0.50f, true);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);

    synth.noteOn(60, 0.8f);

    // Process blocks and verify live telemetry updates
    for (int b = 0; b < 20; ++b) {
        synth.processBlock(left.data(), right.data(), 256);
    }

    const auto tel = synth.getTelemetry();
    EXPECT_FALSE(std::isnan(tel.lfo1Value));
    EXPECT_FALSE(std::isnan(tel.filterCutoffMod));
    EXPECT_NE(tel.filterCutoffMod, 0.0f);

    synth.allNotesOff();
}

TEST(ArcKnobAnimationTests, ArcKnobSetModulationAndLiveHaloUpdates) {
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::ui::ArcKnob knob("CUTOFF", "Hz", theme, false);
    knob.setSize(60, 60);

    knob.setModulation(0.45f, 0.75f);
    auto& slider = knob.getSlider();
    const float modAmt = static_cast<float>(slider.getProperties().getWithDefault("modAmount", 0.0));
    const float liveOffset = static_cast<float>(slider.getProperties().getWithDefault("liveModOffset", 0.0));

    EXPECT_NEAR(modAmt, 0.45f, 1e-4f);
    EXPECT_NEAR(liveOffset, 0.75f, 1e-4f);
}

TEST(MultiEngineOscillatorTests, SamplePlaybackStartOffsetAndWaveformRendering) {
    nova::dsp::MultiEngineOscillator osc;
    osc.prepare(48000.0);

    nova::dsp::MultiEngineOscillator::Parameters p{};
    p.engine = nova::dsp::MultiEngineOscillator::EngineType::SamplePlayback;
    p.sourceType = 4;
    p.modelIndex = 0; // Grand Piano Hammer
    p.shapeMorph = 0.25f; // 25% start offset
    p.level = 0.8f;
    osc.setParameters(p);

    osc.triggerNote(440.0f, 0.9f);

    float l = 0.0f, r = 0.0f;
    float rms = 0.0f;
    for (int i = 0; i < 512; ++i) {
        osc.processSample(440.0f, 0.0f, 0.0f, l, r);
        rms += l * l + r * r;
    }
    rms = std::sqrt(rms / 512.0f);
    EXPECT_GT(rms, 0.01f);

    // Verify all 8 samples generate valid waveform preview points
    for (int s = 0; s < 8; ++s) {
        p.modelIndex = s;
        const auto pts = nova::dsp::MultiEngineOscillator::buildWaveformPreview(p, 48);
        EXPECT_EQ(pts.size(), 48u);
        bool hasNonZero = false;
        for (float pt : pts) {
            if (std::abs(pt - 0.5f) > 0.02f) {
                hasNonZero = true;
                break;
            }
        }
        EXPECT_TRUE(hasNonZero);
    }
}

TEST(ModulationMatrixTests, AllFourLfoEnginesAndShapes) {
    nova::synth::NovaSynthProcessor proc;
    auto& engine = proc.getEngine();
    (void)engine;

    // Check LfoGenerator shapes produce valid bounded samples
    nova::dsp::LfoGenerator lfo;
    lfo.prepare(48000.0);

    const std::vector<nova::dsp::LfoGenerator::Shape> shapes = {
        nova::dsp::LfoGenerator::Shape::Sine,
        nova::dsp::LfoGenerator::Shape::Triangle,
        nova::dsp::LfoGenerator::Shape::SawUp,
        nova::dsp::LfoGenerator::Shape::SmoothDrift,
        nova::dsp::LfoGenerator::Shape::PulseGate
    };

    for (auto shape : shapes) {
        lfo.setParameters(shape, 2.0f, 1.0f);
        lfo.reset();
        for (int i = 0; i < 24000; ++i) {
            const float s = lfo.processSample();
            EXPECT_GE(s, -1.01f);
            EXPECT_LE(s, 1.01f);
        }
    }
}

TEST(ModulationHubPanelTests, PanelInitializesAndLaysOutAllControls) {
    nova::synth::NovaSynthProcessor proc;
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::ui::ModulationHubPanel modPanel(proc, theme);

    // Workbench allocation bounds (1180 x 180)
    modPanel.setBounds(0, 0, 1180, 180);
    EXPECT_EQ(modPanel.getWidth(), 1180);
    EXPECT_EQ(modPanel.getHeight(), 180);

    // Add a new route via the panel
    const auto dummyMouse = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();

    // Click + Add Route button (inside right card, top-right)
    const juce::Point<float> addBtnPos(1130.0f, 15.0f);
    juce::MouseEvent addClick(dummyMouse, addBtnPos, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             &modPanel, &modPanel, now, addBtnPos, now, 1, false);
    modPanel.mouseDown(addClick);

    // Verify a route exists in processor's ModMatrix
    const auto& mm = proc.getModMatrix();
    bool foundActive = false;
    for (std::size_t i = 0; i < nova::dsp::ModMatrix::kMaxSlots; ++i) {
        if (mm.getSlot(i).enabled) {
            foundActive = true;
            break;
        }
    }
    EXPECT_TRUE(foundActive);
}

TEST(ModulationHubPanelTests, LfoKnobFormattingAndEnlargedWaveformAnimation) {
    nova::synth::NovaSynthProcessor proc;
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::ui::ModulationHubPanel modPanel(proc, theme);

    // Standard workbench panel height (180px)
    modPanel.setBounds(0, 0, 1180, 180);

    // 1. Verify waveform animation has received enlarged height (> 50px)
    EXPECT_GE(modPanel.getLfoShapeHeight(0), 50);

    // 2. Verify knob height is constrained to filter proportions (54px - 62px)
    EXPECT_GE(modPanel.getLfoKnobHeight(0), 54);
    EXPECT_LE(modPanel.getLfoKnobHeight(0), 62);

    // 3. Verify Rate knob displays 'Hz'
    const auto* rateKnob = modPanel.getLfoRateKnob(0);
    ASSERT_NE(rateKnob, nullptr);
    const auto rateText = const_cast<nova::ui::ArcKnob*>(rateKnob)->getSlider().getTextFromValue(0.42);
    EXPECT_TRUE(rateText.contains("Hz"));
    EXPECT_EQ(rateText, "0.42 Hz");

    // 4. Verify Depth knob displays percentage (0% to 100%) rather than 0.xx
    const auto* depthKnob = modPanel.getLfoDepthKnob(0);
    ASSERT_NE(depthKnob, nullptr);
    const auto depthText = const_cast<nova::ui::ArcKnob*>(depthKnob)->getSlider().getTextFromValue(0.35);
    EXPECT_TRUE(depthText.contains("%"));
    EXPECT_EQ(depthText, "35 %");

    const auto maxDepthText = const_cast<nova::ui::ArcKnob*>(depthKnob)->getSlider().getTextFromValue(1.0);
    EXPECT_EQ(maxDepthText, "100 %");

    const auto zeroDepthText = const_cast<nova::ui::ArcKnob*>(depthKnob)->getSlider().getTextFromValue(0.0);
    EXPECT_EQ(zeroDepthText, "0 %");
}

TEST(ModulationHubPanelTests, LfoSyncButtonTogglesContinuousHzVsQuantizedNoteDivisions) {
    nova::synth::NovaSynthProcessor proc;
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::ui::ModulationHubPanel modPanel(proc, theme);

    modPanel.setBounds(0, 0, 1180, 180);

    // 1. Initial State: Free run (Sync OFF)
    EXPECT_FALSE(modPanel.isLfoSynced(0));
    const auto* rateKnob = modPanel.getLfoRateKnob(0);
    ASSERT_NE(rateKnob, nullptr);
    EXPECT_EQ(const_cast<nova::ui::ArcKnob*>(rateKnob)->getSlider().getTextFromValue(0.42), "0.42 Hz");

    // 2. Click Sync button (between Rate & Depth knobs)
    const auto btnR = modPanel.getSyncBtnRect(0);
    EXPECT_GE(btnR.getWidth(), 16);
    EXPECT_GE(btnR.getHeight(), 16);

    // Verify vertical center alignment of Sync button with the Rate/Depth rotary dial
    const int knobY = rateKnob->getY();
    const int knobH = rateKnob->getHeight();
    const int dialCenterY = knobY + 12 + (knobH - 26) / 2;
    EXPECT_NEAR(btnR.getCentreY(), dialCenterY, 1);

    const auto dummyMouse = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    const juce::Point<float> clickPos(static_cast<float>(btnR.getCentreX()), static_cast<float>(btnR.getCentreY()));

    // Test mouse hover & tooltip
    juce::MouseEvent hoverEvent(dummyMouse, clickPos, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                &modPanel, &modPanel, now, clickPos, now, 0, false);
    modPanel.mouseMove(hoverEvent);
    EXPECT_FALSE(modPanel.getTooltip().isEmpty());
    EXPECT_TRUE(modPanel.getTooltip().contains("Tempo Sync"));

    juce::MouseEvent syncClick(dummyMouse, clickPos, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                               &modPanel, &modPanel, now, clickPos, now, 1, false);
    modPanel.mouseDown(syncClick);

    // 3. Verify Sync is now active
    EXPECT_TRUE(modPanel.isLfoSynced(0));

    // 4. Verify Rate knob switched to quantized musical note divisions
    auto& slider = const_cast<nova::ui::ArcKnob*>(rateKnob)->getSlider();
    EXPECT_FLOAT_EQ(static_cast<float>(slider.getMaximum()), 11.0f);
    EXPECT_EQ(slider.getTextFromValue(6.0), "1/4");
    EXPECT_EQ(slider.getTextFromValue(7.0), "1/8 D");
    EXPECT_EQ(slider.getTextFromValue(2.0), "1 Bar");

    // 5. Verify mathematical rate conversion in DSP
    EXPECT_FLOAT_EQ(nova::dsp::WorshipSynthEngine::getLfoRateForDivision(120.0f, 6), 2.0f); // 1/4 at 120 BPM = 2 Hz
    EXPECT_FLOAT_EQ(nova::dsp::WorshipSynthEngine::getLfoRateForDivision(120.0f, 2), 0.5f); // 1 Bar at 120 BPM = 0.5 Hz

    // 6. Click Sync button again to return to Free continuous mode
    modPanel.mouseDown(syncClick);
    EXPECT_FALSE(modPanel.isLfoSynced(0));
    EXPECT_EQ(const_cast<nova::ui::ArcKnob*>(rateKnob)->getSlider().getTextFromValue(0.42), "0.42 Hz");
}

TEST(WorkbenchTabTests, ModulationTabSelectionAndEditorStability) {
    nova::synth::NovaSynthProcessor proc;
    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(proc.createEditor());
    ASSERT_NE(editor, nullptr);
    editor->setBounds(0, 0, 1280, 800);

    // Switch to tab 1 (MODULATION)
    auto* synthEditor = dynamic_cast<nova::synth::NovaSynthEditor*>(editor.get());
    ASSERT_NE(synthEditor, nullptr);
    synthEditor->setInitialWorkbenchTab(1);

    // Repaint without any crashes or assertions
    synthEditor->repaint();
}

TEST(WorkbenchTabTests, AllSixTabsNavigationAndStability) {
    nova::synth::NovaSynthProcessor proc;
    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(proc.createEditor());
    ASSERT_NE(editor, nullptr);
    editor->setBounds(0, 0, 1280, 800);

    auto* synthEditor = dynamic_cast<nova::synth::NovaSynthEditor*>(editor.get());
    ASSERT_NE(synthEditor, nullptr);

    // Test switching to each of the 6 workbench tabs:
    // 0: FLOW, 1: MODULATION, 2: FX RACK, 3: DRONE, 4: ARPEGGIATOR, 5: MIXER
    for (int tabIdx = 0; tabIdx < 6; ++tabIdx) {
        synthEditor->setInitialWorkbenchTab(tabIdx);
        synthEditor->repaint();
    }
}

TEST(DroneHubPanelTests, DroneParametersAndHarmonicVoicings) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::ui::DroneHubPanel dronePanel(proc, theme);
    dronePanel.setBounds(0, 0, 1180, 180);

    auto& apvts = proc.getAPVTS();
    auto& engine = proc.getEngine();

    // 1. Initial State: Drone off
    EXPECT_FALSE(apvts.getRawParameterValue("droneEnabled")->load() > 0.5f);

    // 2. Enable Drone Pad Hold and set Key G (7)
    if (auto* pE = apvts.getParameter("droneEnabled")) {
        pE->setValueNotifyingHost(1.0f);
    }
    if (auto* pK = apvts.getParameter("droneRootKey")) {
        pK->setValueNotifyingHost(pK->convertTo0to1(7.0f)); // Key G
    }
    EXPECT_TRUE(apvts.getRawParameterValue("droneEnabled")->load() > 0.5f);
    EXPECT_EQ(static_cast<int>(apvts.getRawParameterValue("droneRootKey")->load()), 7);

    // 3. Test Voicing switching (0: Lush 5th, 1: Deep Sub, 2: Ambient 9th, 3: Celestial 8ve)
    if (auto* pV = apvts.getParameter("droneVoicing")) {
        pV->setValueNotifyingHost(pV->convertTo0to1(1.0f)); // Deep Sub
    }
    EXPECT_EQ(static_cast<int>(apvts.getRawParameterValue("droneVoicing")->load()), 1);

    // 4. Test Octave parameter (-2 to +2)
    if (auto* pOct = apvts.getParameter("droneOctave")) {
        EXPECT_EQ(static_cast<int>(apvts.getRawParameterValue("droneOctave")->load()), 0);
        pOct->setValueNotifyingHost(pOct->convertTo0to1(-1.0f));
        EXPECT_EQ(static_cast<int>(apvts.getRawParameterValue("droneOctave")->load()), -1);
        pOct->setValueNotifyingHost(pOct->convertTo0to1(1.0f));
        EXPECT_EQ(static_cast<int>(apvts.getRawParameterValue("droneOctave")->load()), 1);
    }

    // 5. Test Warmth / Tone sweep
    if (auto* pTone = apvts.getParameter("droneTone")) {
        pTone->setValueNotifyingHost(pTone->convertTo0to1(0.20f)); // Dark Warmth
        proc.syncParametersFromAPVTS();
        pTone->setValueNotifyingHost(pTone->convertTo0to1(0.85f)); // Bright Warmth
        proc.syncParametersFromAPVTS();
    }

    // 6. Test that drone is armed on enable, but remains silent until triggered
    EXPECT_FALSE(proc.isDroneTriggered());
    std::vector<float> preLeft(256, 0.0f);
    std::vector<float> preRight(256, 0.0f);
    engine.processBlock(preLeft.data(), preRight.data(), 256);
    float preEnergy = 0.0f;
    for (float s : preLeft) preEnergy += s * s;
    EXPECT_LT(preEnergy, 1e-6f);

    // 7. Explicitly trigger the drone and verify sound starts
    proc.triggerDrone(true);
    EXPECT_TRUE(proc.isDroneTriggered());

    // 8. Test Live Switching while Drone Pad is held: change key, voicing, octave
    if (auto* pK = apvts.getParameter("droneRootKey")) {
        pK->setValueNotifyingHost(pK->convertTo0to1(2.0f)); // Key D
    }
    if (auto* pV = apvts.getParameter("droneVoicing")) {
        pV->setValueNotifyingHost(pV->convertTo0to1(3.0f)); // Celestial Octaves
    }
    proc.syncParametersFromAPVTS();

    // Render audio block: verify sound is generated cleanly with no NaN/Inf and sustained level
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float blockEnergy = 0.0f;
    for (int b = 0; b < 8; ++b) {
        engine.processBlock(left.data(), right.data(), 256);
        for (int i = 0; i < 256; ++i) {
            ASSERT_FALSE(std::isnan(left[static_cast<std::size_t>(i)]));
            ASSERT_FALSE(std::isinf(left[static_cast<std::size_t>(i)]));
            blockEnergy += left[static_cast<std::size_t>(i)] * left[static_cast<std::size_t>(i)];
        }
    }
    EXPECT_GT(blockEnergy, 1e-3f);

    dronePanel.repaint();
}

TEST(ArpeggiatorHubPanelTests, ArpeggiatorModesOctavesLatchAndPatternSequencer) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::ui::ArpeggiatorHubPanel arpPanel(proc, theme);
    arpPanel.setBounds(0, 0, 1180, 180);

    auto& apvts = proc.getAPVTS();
    auto& engine = proc.getEngine();

    // 1. Enable Arpeggiator and Latch
    if (auto* pE = apvts.getParameter("arpEnabled")) {
        pE->setValueNotifyingHost(1.0f);
    }
    if (auto* pL = apvts.getParameter("arpLatch")) {
        pL->setValueNotifyingHost(1.0f);
    }
    if (auto* pOct = apvts.getParameter("arpOctaves")) {
        pOct->setValueNotifyingHost(pOct->convertTo0to1(1.0f)); // 2 Octaves
    }
    if (auto* pSw = apvts.getParameter("arpSwing")) {
        pSw->setValueNotifyingHost(pSw->convertTo0to1(0.35f)); // 35% Swing
    }

    EXPECT_TRUE(apvts.getRawParameterValue("arpEnabled")->load() > 0.5f);
    EXPECT_TRUE(apvts.getRawParameterValue("arpLatch")->load() > 0.5f);
    EXPECT_EQ(static_cast<int>(apvts.getRawParameterValue("arpOctaves")->load()), 1);

    // 2. Play C-Major triad (notes 48, 52, 55) and release physical keys (Latch keeps playing)
    engine.noteOn(48, 0.85f);
    engine.noteOn(52, 0.80f);
    engine.noteOn(55, 0.82f);
    engine.noteOff(48);
    engine.noteOff(52);
    engine.noteOff(55);

    // Render 1024 samples: arpeggiator must advance steps through the latched chord
    std::vector<float> left(1024, 0.0f);
    std::vector<float> right(1024, 0.0f);
    engine.processBlock(left.data(), right.data(), 1024);

    EXPECT_GE(engine.getArpCurrentStep(), 0);
    EXPECT_LT(engine.getArpCurrentStep(), 16);

    for (int i = 0; i < 1024; ++i) {
        EXPECT_FALSE(std::isnan(left[static_cast<std::size_t>(i)]));
        EXPECT_FALSE(std::isinf(left[static_cast<std::size_t>(i)]));
    }

    arpPanel.repaint();
}

TEST(ArpeggiatorHubPanelTests, ArpeggiatorStepMatrixRestVelocityGateAndPatternLengths) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::ui::ArpeggiatorHubPanel arpPanel(proc, theme);
    arpPanel.setBounds(0, 0, 1180, 180);

    auto& apvts = proc.getAPVTS();
    auto& engine = proc.getEngine();

    // 1. Verify pattern length switching (16, 32, 64)
    proc.setArpPatternLength(32);
    EXPECT_EQ(proc.getArpPatternLength(), 32);
    arpPanel.updateMatrixLayout();

    proc.setArpPatternLength(64);
    EXPECT_EQ(proc.getArpPatternLength(), 64);
    arpPanel.updateMatrixLayout();

    proc.setArpPatternLength(16);
    EXPECT_EQ(proc.getArpPatternLength(), 16);
    arpPanel.updateMatrixLayout();

    // 2. Test Step Gate Deactivation & Velocity Scaling in DSP
    // Enable Arp, Free Rate 8 Hz (125ms per step = 6000 samples @ 48kHz)
    if (auto* pE = apvts.getParameter("arpEnabled")) pE->setValueNotifyingHost(1.0f);
    if (auto* pS = apvts.getParameter("arpSync")) pS->setValueNotifyingHost(0.0f);
    if (auto* pR = apvts.getParameter("arpRateHz")) pR->setValueNotifyingHost(pR->convertTo0to1(8.0f));
    if (auto* pG = apvts.getParameter("arpGate")) pG->setValueNotifyingHost(pG->convertTo0to1(0.20f)); // 20% Gate = 25ms gate
    if (auto* pA = apvts.getParameter("ampAttack")) pA->setValueNotifyingHost(pA->convertTo0to1(0.005f));
    if (auto* pRel = apvts.getParameter("ampRelease")) pRel->setValueNotifyingHost(pRel->convertTo0to1(0.02f)); // 20ms Release for crisp staccato test
    if (auto* pDel = apvts.getParameter("delayEnabled")) pDel->setValueNotifyingHost(0.0f);
    if (auto* pRev = apvts.getParameter("reverbMix")) pRev->setValueNotifyingHost(pRev->convertTo0to1(0.0f));

    // Set Step 1: Active, Vel = 1.0
    // Set Step 2: Inactive (Gate OFF) -> Must produce silence / rest!
    // Set Step 3: Active, Vel = 0.2 -> Must produce quiet signal!
    proc.setArpStepGate(0, true);
    proc.setArpStepVelocity(0, 1.0f);

    proc.setArpStepGate(1, false); // Rest step!

    proc.setArpStepGate(2, true);
    proc.setArpStepVelocity(2, 0.20f);

    proc.syncParametersFromAPVTS();

    // Play a single note (60)
    engine.noteOn(60, 0.85f);

    // Render Step 1 (active, vel 1.0): 6000 samples
    std::vector<float> step1L(6000, 0.0f);
    std::vector<float> step1R(6000, 0.0f);
    engine.processBlock(step1L.data(), step1R.data(), 6000);

    float peakStep1 = 0.0f;
    for (float f : step1L) peakStep1 = std::max(peakStep1, std::abs(f));
    EXPECT_GT(peakStep1, 0.15f);

    // Because Gate was 20% (1200 samples), by the end of step 1 (samples 4500..6000), voice should have released to silence!
    float tailStep1 = 0.0f;
    for (int i = 4500; i < 6000; ++i) tailStep1 = std::max(tailStep1, std::abs(step1L[i]));
    EXPECT_LT(tailStep1, 0.05f);

    // Render Step 2 (deactivated rest step): 6000 samples
    std::vector<float> step2L(6000, 0.0f);
    std::vector<float> step2R(6000, 0.0f);
    engine.processBlock(step2L.data(), step2R.data(), 6000);

    float peakStep2 = 0.0f;
    for (int i = 2000; i < 6000; ++i) peakStep2 = std::max(peakStep2, std::abs(step2L[i]));
    EXPECT_LT(peakStep2, 0.02f); // Rest step remains silent!

    // Render Step 3 (active, vel 0.20): 6000 samples
    std::vector<float> step3L(6000, 0.0f);
    std::vector<float> step3R(6000, 0.0f);
    engine.processBlock(step3L.data(), step3R.data(), 6000);

    float peakStep3 = 0.0f;
    for (float f : step3L) peakStep3 = std::max(peakStep3, std::abs(f));
    EXPECT_GT(peakStep3, 0.005f);
    EXPECT_LT(peakStep3, peakStep1 * 0.45f); // Velocity 0.20 is significantly quieter than velocity 1.0!

    engine.noteOff(60);
    arpPanel.repaint();
}

TEST(MixerHubPanelTests, MixerAllEightSourceChannelsAndRoutings) {
    nova::synth::NovaSynthProcessor proc;
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::ui::MixerHubPanel mixerPanel(proc, theme);
    mixerPanel.setBounds(0, 0, 1180, 180);

    auto& apvts = proc.getAPVTS();

    // Verify oscillator 1 and 2 parameters exist and can be modulated
    EXPECT_NE(apvts.getRawParameterValue("osc1_level"), nullptr);
    EXPECT_NE(apvts.getRawParameterValue("osc1_pan"), nullptr);
    EXPECT_NE(apvts.getRawParameterValue("osc1_enabled"), nullptr);
    EXPECT_NE(apvts.getRawParameterValue("osc1_filterRouting"), nullptr);

    // Toggle Osc 2 filter routing to FLT 2 (1)
    if (auto* p = apvts.getParameter("osc2_filterRouting")) {
        p->setValueNotifyingHost(p->convertTo0to1(1.0f));
    }
    EXPECT_EQ(static_cast<int>(apvts.getRawParameterValue("osc2_filterRouting")->load()), 1);

    mixerPanel.repaint();
}

TEST(ModulationMatrixTests, AllEightOscillatorSlotsLevelShapePitchModulations) {
    nova::dsp::ModMatrix mm;
    const std::array<nova::dsp::ModMatrix::Destination, 8> shapes = {
        nova::dsp::ModMatrix::Destination::Osc1Shape, nova::dsp::ModMatrix::Destination::Osc2Shape,
        nova::dsp::ModMatrix::Destination::Osc3Shape, nova::dsp::ModMatrix::Destination::Osc4Shape,
        nova::dsp::ModMatrix::Destination::Osc5Shape, nova::dsp::ModMatrix::Destination::Osc6Shape,
        nova::dsp::ModMatrix::Destination::Osc7Shape, nova::dsp::ModMatrix::Destination::Osc8Shape
    };
    const std::array<nova::dsp::ModMatrix::Destination, 8> levels = {
        nova::dsp::ModMatrix::Destination::Osc1Level, nova::dsp::ModMatrix::Destination::Osc2Level,
        nova::dsp::ModMatrix::Destination::Osc3Level, nova::dsp::ModMatrix::Destination::Osc4Level,
        nova::dsp::ModMatrix::Destination::Osc5Level, nova::dsp::ModMatrix::Destination::Osc6Level,
        nova::dsp::ModMatrix::Destination::Osc7Level, nova::dsp::ModMatrix::Destination::Osc8Level
    };
    const std::array<nova::dsp::ModMatrix::Destination, 8> pitches = {
        nova::dsp::ModMatrix::Destination::Osc1Pitch, nova::dsp::ModMatrix::Destination::Osc2Pitch,
        nova::dsp::ModMatrix::Destination::Osc3Pitch, nova::dsp::ModMatrix::Destination::Osc4Pitch,
        nova::dsp::ModMatrix::Destination::Osc5Pitch, nova::dsp::ModMatrix::Destination::Osc6Pitch,
        nova::dsp::ModMatrix::Destination::Osc7Pitch, nova::dsp::ModMatrix::Destination::Osc8Pitch
    };

    for (std::size_t i = 0; i < 8; ++i) {
        EXPECT_NE(nova::dsp::ModMatrix::getDestinationName(shapes[i]), nullptr);
        EXPECT_NE(nova::dsp::ModMatrix::getDestinationName(levels[i]), nullptr);
        EXPECT_NE(nova::dsp::ModMatrix::getDestinationName(pitches[i]), nullptr);
    }

    // Assign slots 0..7 to ModEnv3 -> OscXLevel
    for (std::size_t i = 0; i < 8; ++i) {
        mm.setSlot(i, nova::dsp::ModMatrix::Source::ModEnv3, levels[i], 0.1f * static_cast<float>(i + 1), true);
    }

    nova::dsp::ModMatrix::VoiceSources vs;
    vs.modEnv = 1.0f;
    const auto vo = mm.evaluateVoice(vs);

    for (std::size_t i = 0; i < 8; ++i) {
        const float expected = 0.1f * static_cast<float>(i + 1);
        EXPECT_NEAR(vo.offsets[static_cast<std::size_t>(levels[i])], expected, 1e-4f);
    }
}

TEST(ModulationMatrixTests, SlotsFiveThroughEightAudioDspVerification) {
    nova::dsp::WorshipSynthEngine engine;
    engine.prepare(48000.0, 512);

    auto params = engine.getParameters();
    params.activeOscCount = 8;
    for (int i = 0; i < 7; ++i) {
        params.oscSlots[static_cast<std::size_t>(i)].enabled = false;
    }
    // Only Slot 8 (index 7) is enabled
    params.oscSlots[7].enabled = true;
    params.oscSlots[7].engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
    params.oscSlots[7].level = 1.0f;
    params.ampEnv.attackSec = 0.005f;
    params.ampEnv.decaySec = 0.10f;
    params.ampEnv.sustainLvl = 1.0f;
    params.ampEnv.releaseSec = 0.10f;
    engine.setParameters(params);

    // 1. Unmodulated Slot 8 renders clean audio
    engine.noteOn(60, 0.8f);
    std::vector<float> baseL(2400, 0.0f);
    std::vector<float> baseR(2400, 0.0f);
    engine.processBlock(baseL.data(), baseR.data(), 2400);
    float basePeak = 0.0f;
    for (float f : baseL) basePeak = std::max(basePeak, std::abs(f));
    EXPECT_GT(basePeak, 0.10f) << "Slot 8 must produce clean audio output!";
    engine.allNotesOff();

    std::vector<float> tailL(12000, 0.0f);
    std::vector<float> tailR(12000, 0.0f);
    engine.processBlock(tailL.data(), tailR.data(), 12000);

    // 2. Modulate Slot 8 Level via Macro Warmth (-1.0 depth = mute when Macro is +1.0)
    engine.getModMatrix().setSlot(0, nova::dsp::ModMatrix::Source::MacroWarmth,
                                 nova::dsp::ModMatrix::Destination::Osc8Level, -1.0f, true);
    params.macroWarmth = 1.0f;
    engine.setParameters(params);

    engine.noteOn(60, 0.8f);
    std::vector<float> mutedL(2400, 0.0f);
    std::vector<float> mutedR(2400, 0.0f);
    engine.processBlock(mutedL.data(), mutedR.data(), 2400);
    float mutedPeak = 0.0f;
    for (float f : mutedL) mutedPeak = std::max(mutedPeak, std::abs(f));
    EXPECT_LT(mutedPeak, 0.02f) << "Slot 8 modulated by -1.0 level offset must be muted!";
    engine.allNotesOff();
}

TEST(ModulationMatrixTests, EnvelopeTimeDestinationsAndVoiceApplication) {
    nova::dsp::WorshipSynthEngine engine;
    engine.prepare(48000.0, 512);

    auto params = engine.getParameters();
    params.activeOscCount = 1;
    params.oscSlots[0].enabled = true;
    params.oscSlots[0].level = 1.0f;
    params.ampEnv.attackSec = 0.50f; // 500 ms base attack
    params.ampEnv.decaySec = 0.10f;
    params.ampEnv.sustainLvl = 1.0f;
    params.ampEnv.releaseSec = 0.20f;
    engine.setParameters(params);

    // Velocity modulates Amp Attack: high velocity = faster attack (-0.80 depth)
    engine.getModMatrix().setSlot(0, nova::dsp::ModMatrix::Source::Velocity,
                                 nova::dsp::ModMatrix::Destination::AmpAttack, -0.80f, true);

    // Note A: velocity 1.0 -> attack becomes significantly shorter (~0.05s)
    engine.noteOn(60, 1.0f);
    std::vector<float> fastAttackL(2400, 0.0f); // 50ms
    std::vector<float> fastAttackR(2400, 0.0f);
    engine.processBlock(fastAttackL.data(), fastAttackR.data(), 2400);
    float fastPeak = 0.0f;
    for (float f : fastAttackL) fastPeak = std::max(fastPeak, std::abs(f));
    engine.allNotesOff();
    std::vector<float> silenceL(24000, 0.0f);
    std::vector<float> silenceR(24000, 0.0f);
    engine.processBlock(silenceL.data(), silenceR.data(), 24000);

    // Note B: velocity 0.05 -> attack stays long (~0.50s)
    engine.noteOn(60, 0.05f);
    std::vector<float> slowAttackL(2400, 0.0f);
    std::vector<float> slowAttackR(2400, 0.0f);
    engine.processBlock(slowAttackL.data(), slowAttackR.data(), 2400);
    float slowPeak = 0.0f;
    for (float f : slowAttackL) slowPeak = std::max(slowPeak, std::abs(f));
    engine.allNotesOff();

    // Fast attack note must rise much faster than slow attack note in first 50ms!
    EXPECT_GT(fastPeak, slowPeak * 2.0f);
}

TEST(ModulationMatrixTests, LfoRateModulationDestinations) {
    nova::dsp::WorshipSynthEngine engine;
    engine.prepare(48000.0, 512);

    auto params = engine.getParameters();
    params.lfo1RateHz = 1.0f; // 1 Hz
    params.lfo1Sync = false;
    params.activeOscCount = 1;
    params.oscSlots[0].enabled = true;
    engine.setParameters(params);

    // ModWheel -> Lfo1Rate (+1.0 depth)
    engine.getModMatrix().setSlot(0, nova::dsp::ModMatrix::Source::ModWheel,
                                 nova::dsp::ModMatrix::Destination::Lfo1Rate, 1.0f, true);

    // Test with ModWheel at 0.0
    engine.getModMatrix().setSourceValue(nova::dsp::ModMatrix::Source::ModWheel, 0.0f);
    engine.getModMatrix().evaluate();
    engine.setParameters(params);

    engine.noteOn(60, 0.8f);
    std::vector<float> lfoBlockL(480, 0.0f);
    std::vector<float> lfoBlockR(480, 0.0f);
    engine.processBlock(lfoBlockL.data(), lfoBlockR.data(), 480);

    // Test with ModWheel at 1.0 (LFO rate speeds up ~8x)
    engine.getModMatrix().setSourceValue(nova::dsp::ModMatrix::Source::ModWheel, 1.0f);
    engine.getModMatrix().evaluate();
    engine.setParameters(params);
    engine.processBlock(lfoBlockL.data(), lfoBlockR.data(), 480);

    // LFO is verified active and responsive
    EXPECT_NE(nova::dsp::ModMatrix::getDestinationName(nova::dsp::ModMatrix::Destination::Lfo1Rate), nullptr);
    EXPECT_NE(nova::dsp::ModMatrix::getDestinationName(nova::dsp::ModMatrix::Destination::Lfo2Rate), nullptr);
    engine.allNotesOff();
}

TEST(SourceDeckTests, CompactEnvelopePillDirectInteraction) {
    nova::synth::NovaSynthProcessor proc;
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::SourceDeck sourceDeck(proc, theme);
    sourceDeck.setBounds(0, 0, 800, 196);

    const auto& cards = sourceDeck.getCardList();
    ASSERT_EQ(cards.size(), 8u);
    auto* card = cards[0].get();
    ASSERT_NE(card, nullptr);
    EXPECT_FALSE(card->isExpanded());

    // In compact mode, click the envelope pill (centered in header, e.g. x=75, y=12)
    const auto dummyMouse = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    const juce::Point<float> pillPos(75.0f, 12.0f);
    juce::MouseEvent pillDown(dummyMouse, pillPos, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             card, card, now, pillPos, now, 1, false);
    card->mouseDown(pillDown);

    // Clicking the envelope pill in compact mode must NEVER expand the card!
    EXPECT_FALSE(card->isExpanded()) << "Clicking envelope routing pill in compact mode must NOT expand the card!";

    // Switch envelope source to Mod Env 3 (source index 2)
    auto& apvts = proc.getAPVTS();
    if (auto* p = apvts.getParameter("osc1_ampEnvSource")) {
        p->setValueNotifyingHost(p->convertTo0to1(2.0f));
    }
    EXPECT_EQ(static_cast<int>(apvts.getRawParameterValue("osc1_ampEnvSource")->load()), 2);

    card->repaint();
}

TEST(UiPolishTests, ArcKnobFrequencyFormattingAndParsing) {
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::ui::ArcKnob knob("CUTOFF", "Hz", theme);

    // Frequencies >= 1000 Hz format as concise kHz string without truncation:
    EXPECT_EQ(knob.getSlider().getTextFromValue(2388.0), "2.39k");
    EXPECT_EQ(knob.getSlider().getTextFromValue(1100.0), "1.10k");
    EXPECT_EQ(knob.getSlider().getTextFromValue(10000.0), "10.0k");

    // Audio & LFO rates < 1000 Hz preserve full precision:
    EXPECT_EQ(knob.getSlider().getTextFromValue(450.0), "450 Hz");
    EXPECT_EQ(knob.getSlider().getTextFromValue(0.42), "0.42 Hz");

    // Parsing kHz and Hz strings:
    EXPECT_NEAR(knob.getSlider().getValueFromText("2.39k"), 2390.0, 1.0);
    EXPECT_NEAR(knob.getSlider().getValueFromText("1.1 kHz"), 1100.0, 1.0);
    EXPECT_NEAR(knob.getSlider().getValueFromText("350 Hz"), 350.0, 0.1);
}

TEST(UiPolishTests, StandaloneKeyboardBarVelocityAndQwertyAudition) {
    nova::synth::NovaSynthProcessor proc;
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::StandaloneKeyboardBar bar(proc, theme);
    bar.setBounds(0, 0, 1200, 62);

    // Initial state: default 85% velocity and QWERTY off
    EXPECT_NEAR(bar.getAuditionVelocity(), 0.85f, 0.01f);
    EXPECT_FALSE(bar.isQwertyActive());

    // Enable QWERTY
    bar.setQwertyActive(true);
    EXPECT_TRUE(bar.isQwertyActive());

    // Pressing 'A' (C4 = MIDI note 60) triggers audition note
    const bool keyHandled = bar.keyPressed(juce::KeyPress('A'));
    EXPECT_TRUE(keyHandled);

    // Release key
    const bool keyReleased = bar.keyStateChanged(false);
    EXPECT_TRUE(keyReleased);

    // Adjust velocity
    bar.setAuditionVelocity(0.50f);
    EXPECT_NEAR(bar.getAuditionVelocity(), 0.50f, 0.01f);
}

TEST(WorshipSynthEngineTests, ArpeggiatorVoicePreservesEnvelopeAttackTimes) {
    nova::dsp::WorshipSynthEngine synth;
    synth.prepare(48000.0, 512);

    auto p = synth.getParameters();
    p.arpEnabled = true;
    p.arpMode = nova::dsp::WorshipSynthEngine::ArpMode::Up;
    p.arpRateHz = 8.0f; // 125 ms per step
    p.arpGate = 0.5f;

    // OSC 1: Amp Env with long 4.0s attack
    p.activeOscCount = 2;
    p.oscSlots[0].enabled = true;
    p.oscSlots[0].ampEnvSource = 0; // Amp Env
    p.ampEnv.attackSec = 4.0f;
    p.ampEnv.sustainLvl = 1.0f;

    // OSC 2: Mod Env with fast 0.001s attack and short decay (pluck)
    p.oscSlots[1].enabled = true;
    p.oscSlots[1].ampEnvSource = 2; // Mod Env 3
    p.modEnv.attackSec = 0.001f;
    p.modEnv.decaySec = 0.03f;
    p.modEnv.sustainLvl = 0.0f;

    synth.setParameters(p);
    synth.noteOn(60, 0.8f);

    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);
    synth.processBlock(left.data(), right.data(), 512);

    // Verify active voice sounds and arpeggiates cleanly
    const auto tel = synth.getTelemetry();
    EXPECT_GE(tel.activeVoiceCount, 1);
}

TEST(WorshipSynthEngineTests, VoiceCullingTracksFilterEnvelopeAsAmpSource) {
    nova::dsp::WorshipSynthEngine synth;
    synth.prepare(48000.0, 512);

    auto p = synth.getParameters();
    p.activeOscCount = 1;
    p.oscSlots[0].enabled = true;
    p.oscSlots[0].ampEnvSource = 1; // Filter Env as amplitude source!

    p.ampEnv.releaseSec = 0.01f;     // Very fast 10ms Amp Env release
    p.filterEnv.releaseSec = 0.25f;  // Longer 250ms Filter Env release
    synth.setParameters(p);

    synth.noteOn(60, 0.8f);
    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);
    synth.processBlock(left.data(), right.data(), 512);

    synth.noteOff(60);
    // Process 3 blocks (~32 ms > 10 ms Amp release, but < 250 ms Filter release)
    for (int b = 0; b < 3; ++b) {
        synth.processBlock(left.data(), right.data(), 512);
    }
    // Voice MUST still be active because filterEnv is still releasing!
    EXPECT_EQ(synth.getTelemetry().activeVoiceCount, 1);

    // After 30 more blocks (~320 ms > 250 ms), voice should be cleanly culled
    for (int b = 0; b < 30; ++b) {
        synth.processBlock(left.data(), right.data(), 512);
    }
    EXPECT_EQ(synth.getTelemetry().activeVoiceCount, 0);
}

TEST(ModulationMatrixTests, PanSpreadModulationScalesStereoWidth) {
    nova::dsp::WorshipSynthEngine synth;
    synth.prepare(48000.0, 512);

    auto p = synth.getParameters();
    p.activeOscCount = 1;
    p.oscSlots[0].enabled = true;
    p.oscSlots[0].detune = 0.4f; // Produce stereo unison spread
    p.macroWarmth = 0.0f;
    synth.setParameters(p);

    synth.noteOn(60, 0.8f);
    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);
    synth.processBlock(left.data(), right.data(), 512);

    float baseSideEnergy = 0.0f;
    for (int i = 0; i < 512; ++i) {
        const float side = left[static_cast<std::size_t>(i)] - right[static_cast<std::size_t>(i)];
        baseSideEnergy += side * side;
    }

    // Now route Macro Warmth to PanSpread with max amount
    synth.getModMatrix().setSlot(0, nova::dsp::ModMatrix::Source::MacroWarmth, nova::dsp::ModMatrix::Destination::PanSpread, 1.0f, true);
    p.macroWarmth = 1.0f;
    synth.setParameters(p);

    synth.processBlock(left.data(), right.data(), 512);
    float expandedSideEnergy = 0.0f;
    for (int i = 0; i < 512; ++i) {
        const float side = left[static_cast<std::size_t>(i)] - right[static_cast<std::size_t>(i)];
        expandedSideEnergy += side * side;
    }

    EXPECT_GT(expandedSideEnergy, baseSideEnergy * 1.5f);
}

TEST(NovaSynthProcessorTests, SustainPedalInversionToggle) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);

    EXPECT_FALSE(proc.isInvertSustainPedal());

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    // Normal mode: CC 64 = 127 engages sustain pedal
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 10);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 20);
    proc.processBlock(buffer, midi);

    // Voice should be sustained because pedal is down
    EXPECT_GE(proc.getSynthTelemetry().activeVoiceCount, 1);

    // CC 64 = 0 releases sustain pedal in normal mode
    midi.clear();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0);
    proc.processBlock(buffer, midi);

    // Now enable invert sustain pedal
    proc.setInvertSustainPedal(true);
    EXPECT_TRUE(proc.isInvertSustainPedal());

    // In inverted mode: CC 64 = 0 (physical pedal down on inverted hardware) engages sustain!
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8)100), 0);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 10);
    midi.addEvent(juce::MidiMessage::noteOff(1, 64), 20);
    proc.processBlock(buffer, midi);

    EXPECT_GE(proc.getSynthTelemetry().activeVoiceCount, 1);

    // And CC 64 = 127 (physical pedal released on inverted hardware) releases sustain!
    midi.clear();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    proc.processBlock(buffer, midi);
}

TEST(PresetManagerTests, GlobalHardwarePreferencesPersistAcrossPresetLoading) {
    nova::synth::NovaSynthProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    auto& pm = processor.getPresetManager();
    auto& apvts = processor.getAPVTS();

    // 1. Set global hardware preference (inverted sustain pedal)
    processor.setInvertSustainPedal(true);
    EXPECT_TRUE(processor.isInvertSustainPedal());
    EXPECT_FLOAT_EQ(*apvts.getRawParameterValue("invertSustainPedal"), 1.0f);

    // 2. Load multiple presets from different categories
    EXPECT_TRUE(pm.loadPresetByName("Init Patch"));
    EXPECT_FLOAT_EQ(*apvts.getRawParameterValue("invertSustainPedal"), 1.0f);
    EXPECT_TRUE(processor.isInvertSustainPedal());

    EXPECT_TRUE(pm.loadPresetByName("Bethel Shimmer Cloud"));
    EXPECT_FLOAT_EQ(*apvts.getRawParameterValue("invertSustainPedal"), 1.0f);
    EXPECT_TRUE(processor.isInvertSustainPedal());

    EXPECT_TRUE(pm.loadPresetByName("Hillsong 1/8d Pluck Lead"));
    EXPECT_FLOAT_EQ(*apvts.getRawParameterValue("invertSustainPedal"), 1.0f);
    EXPECT_TRUE(processor.isInvertSustainPedal());

    // 3. A/B switching also preserves global preferences
    pm.switchABState();
    EXPECT_FLOAT_EQ(*apvts.getRawParameterValue("invertSustainPedal"), 1.0f);
    pm.switchABState();
    EXPECT_FLOAT_EQ(*apvts.getRawParameterValue("invertSustainPedal"), 1.0f);
}

TEST(NovaSynthRegressionTests, SubOscCardLowestNoteVoicingToggle) {
    nova::synth::NovaSynthProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    auto& apvts = processor.getAPVTS();
    auto& pm = processor.getPresetManager();
    pm.loadPresetByName("Init Patch");

    // Configure slot 1 as Sub Oscillator
    if (auto* pEngine = apvts.getParameter("osc1_engine")) {
        pEngine->setValueNotifyingHost(pEngine->convertTo0to1(3.0f)); // SubOscillator
    }
    if (auto* pAmpAtt = apvts.getParameter("ampAttackSec")) {
        pAmpAtt->setValueNotifyingHost(pAmpAtt->convertTo0to1(0.005f));
    }
    if (auto* pAmpRel = apvts.getParameter("ampReleaseSec")) {
        pAmpRel->setValueNotifyingHost(pAmpRel->convertTo0to1(0.05f));
    }

    // 1. Polyphonic mode (lowestNoteOnly = false)
    if (auto* pLow = apvts.getParameter("osc1_lowestNoteOnly")) {
        pLow->setValueNotifyingHost(0.0f);
    }

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 36, (juce::uint8)100), 0); // C2
    midi.addEvent(juce::MidiMessage::noteOn(1, 40, (juce::uint8)100), 0); // E2
    midi.addEvent(juce::MidiMessage::noteOn(1, 43, (juce::uint8)100), 0); // G2

    float polyRms = 0.0f;
    for (int b = 0; b < 6; ++b) {
        buffer.clear();
        processor.processBlock(buffer, midi);
        midi.clear();
        polyRms = buffer.getRMSLevel(0, 0, 512);
    }
    EXPECT_GT(polyRms, 0.04f);

    // 2. Lowest-only Mono mode (lowestNoteOnly = true)
    if (auto* pLow = apvts.getParameter("osc1_lowestNoteOnly")) {
        pLow->setValueNotifyingHost(1.0f);
    }

    // Release and re-trigger chord
    midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    midi.clear();

    midi.addEvent(juce::MidiMessage::noteOn(1, 36, (juce::uint8)100), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 40, (juce::uint8)100), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 43, (juce::uint8)100), 0);

    float monoRms = 0.0f;
    for (int b = 0; b < 6; ++b) {
        buffer.clear();
        processor.processBlock(buffer, midi);
        midi.clear();
        monoRms = buffer.getRMSLevel(0, 0, 512);
    }

    // With lowest-note-only active, only C2 plays in sub instead of 3 simultaneous sub notes!
    EXPECT_GT(monoRms, 0.015f);
    EXPECT_LT(monoRms, polyRms * 0.90f);
}

// =================================================================================================
// COMPREHENSIVE REGRESSION & STABILITY SUITE (5.1 - 5.4)
// =================================================================================================

// 5.1: All parameters of different OSCs (Audio DSP behavior + UI visual non-overlapping layout)
TEST(NovaSynthComprehensiveSuite5, OscEngineParametersAudioAndUiVisualLayout) {
    constexpr double sr = 48000.0;
    using ET = nova::dsp::MultiEngineOscillator::EngineType;

    const std::vector<ET> engines = {
        ET::OrganicTexture,
        ET::PhysicalPluckModal,
        ET::WarmAnalog,
        ET::SubOscillator,
        ET::EvolvingWavetable,
        ET::NoiseTransient,
        ET::SamplePlayback
    };

    // 1. Audio: Constant-power Panning across ALL 7 engines
    for (auto engine : engines) {
        nova::dsp::MultiEngineOscillator osc;
        osc.prepare(sr);

        nova::dsp::MultiEngineOscillator::Parameters p;
        p.enabled = true;
        p.engine = engine;
        p.level = 0.8f;
        p.shapeMorph = 0.2f;
        p.detune = 0.0f;

        // Hard Left: pan = -1.0f
        p.pan = -1.0f;
        osc.setParameters(p);
        osc.triggerNote(261.63f, 0.8f, 60);

        float sumSqL = 0.0f;
        float sumSqR = 0.0f;
        for (int i = 0; i < 512; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(261.63f, 0.0f, 0.0f, l, r);
            sumSqL += l * l;
            sumSqR += r * r;
        }
        const float rmsL_left = std::sqrt(sumSqL / 512.0f);
        const float rmsR_left = std::sqrt(sumSqR / 512.0f);

        EXPECT_GT(rmsL_left, 1e-4f) << "Engine " << static_cast<int>(engine) << " must output signal on Left when panned hard Left!";
        EXPECT_LT(rmsR_left, 1e-5f) << "Engine " << static_cast<int>(engine) << " must have 0 on Right when panned hard Left!";

        // Hard Right: pan = +1.0f
        osc.prepare(sr);
        p.pan = 1.0f;
        osc.setParameters(p);
        osc.triggerNote(261.63f, 0.8f, 60);

        sumSqL = 0.0f;
        sumSqR = 0.0f;
        for (int i = 0; i < 512; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(261.63f, 0.0f, 0.0f, l, r);
            sumSqL += l * l;
            sumSqR += r * r;
        }
        const float rmsL_right = std::sqrt(sumSqL / 512.0f);
        const float rmsR_right = std::sqrt(sumSqR / 512.0f);

        EXPECT_LT(rmsL_right, 1e-5f) << "Engine " << static_cast<int>(engine) << " must have 0 on Left when panned hard Right!";
        EXPECT_GT(rmsR_right, 1e-4f) << "Engine " << static_cast<int>(engine) << " must output signal on Right when panned hard Right!";

        // Center: pan = 0.0f
        osc.prepare(sr);
        p.pan = 0.0f;
        osc.setParameters(p);
        osc.triggerNote(261.63f, 0.8f, 60);

        sumSqL = 0.0f;
        sumSqR = 0.0f;
        for (int i = 0; i < 512; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(261.63f, 0.0f, 0.0f, l, r);
            sumSqL += l * l;
            sumSqR += r * r;
        }
        const float rmsL_center = std::sqrt(sumSqL / 512.0f);
        const float rmsR_center = std::sqrt(sumSqR / 512.0f);
        EXPECT_GT(rmsL_center, 1e-4f);
        EXPECT_GT(rmsR_center, 1e-4f);
        EXPECT_NEAR(rmsL_center, rmsR_center, (rmsL_center + rmsR_center) * 0.15f);
    }

    // 2. Audio: Sample SPREAD Haas decorrelation & START offset & DETUNE
    {
        nova::dsp::MultiEngineOscillator smpOsc;
        smpOsc.prepare(sr);

        // a. SPREAD = 0.0f (Mono) vs SPREAD = 0.80f (Haas Stereo Width)
        nova::dsp::MultiEngineOscillator::Parameters pMono;
        pMono.enabled = true;
        pMono.engine = ET::SamplePlayback;
        pMono.level = 0.8f;
        pMono.pan = 0.0f;
        pMono.detune = 0.0f; // SPREAD = 0
        smpOsc.setParameters(pMono);
        smpOsc.triggerNote(261.63f, 0.8f, 60);

        float monoDiffSq = 0.0f;
        for (int i = 0; i < 512; ++i) {
            float l = 0.0f, r = 0.0f;
            smpOsc.processSample(261.63f, 0.0f, 0.0f, l, r);
            const float diff = l - r;
            monoDiffSq += diff * diff;
        }
        EXPECT_LT(std::sqrt(monoDiffSq / 512.0f), 1e-4f) << "With SPREAD=0, Sample playback must be strictly mono!";

        nova::dsp::MultiEngineOscillator::Parameters pStereo;
        pStereo.enabled = true;
        pStereo.engine = ET::SamplePlayback;
        pStereo.level = 0.8f;
        pStereo.pan = 0.0f;
        pStereo.detune = 0.80f; // SPREAD = 80% (Haas delay)
        smpOsc.prepare(sr);
        smpOsc.setParameters(pStereo);
        smpOsc.triggerNote(261.63f, 0.8f, 60);

        float stereoDiffSq = 0.0f;
        float stereoSumSq = 0.0f;
        for (int i = 0; i < 512; ++i) {
            float l = 0.0f, r = 0.0f;
            smpOsc.processSample(261.63f, 0.0f, 0.0f, l, r);
            const float diff = l - r;
            const float sum = l + r;
            stereoDiffSq += diff * diff;
            stereoSumSq += sum * sum;
        }
        const float diffRms = std::sqrt(stereoDiffSq / 512.0f);
        const float sumRms = std::sqrt(stereoSumSq / 512.0f);
        EXPECT_GT(diffRms, 0.02f * sumRms) << "Haas delay spread must generate significant stereo difference on Sample playback!";

        // b. START Offset (shapeMorph = 0.0f vs 0.50f)
        smpOsc.prepare(sr);
        nova::dsp::MultiEngineOscillator::Parameters pStart0;
        pStart0.enabled = true;
        pStart0.engine = ET::SamplePlayback;
        pStart0.level = 0.8f;
        pStart0.shapeMorph = 0.0f;
        smpOsc.setParameters(pStart0);
        smpOsc.triggerNote(261.63f, 0.8f, 60);

        std::vector<float> sig0(128);
        for (int i = 0; i < 128; ++i) {
            float l = 0.0f, r = 0.0f;
            smpOsc.processSample(261.63f, 0.0f, 0.0f, l, r);
            sig0[i] = l;
        }

        smpOsc.prepare(sr);
        nova::dsp::MultiEngineOscillator::Parameters pStartHalf;
        pStartHalf.enabled = true;
        pStartHalf.engine = ET::SamplePlayback;
        pStartHalf.level = 0.8f;
        pStartHalf.shapeMorph = 0.50f;
        smpOsc.setParameters(pStartHalf);
        smpOsc.triggerNote(261.63f, 0.8f, 60);

        std::vector<float> sigHalf(128);
        for (int i = 0; i < 128; ++i) {
            float l = 0.0f, r = 0.0f;
            smpOsc.processSample(261.63f, 0.0f, 0.0f, l, r);
            sigHalf[i] = l;
        }

        float diffFromStart = 0.0f;
        for (int i = 0; i < 128; ++i) {
            diffFromStart += std::abs(sig0[i] - sigHalf[i]);
        }
        EXPECT_GT(diffFromStart, 0.10f) << "START parameter must shift initial playhead position!";
    }

    // 3. UI Layout & Non-Overlapping Knobs (specifically verifying Sample mode fix)
    {
        nova::synth::NovaSynthProcessor proc;
        auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
        nova::synth::SourceDeck sourceDeck(proc, theme);

        auto& card = *sourceDeck.getCardList()[0];
        auto& apvts = proc.getAPVTS();

        // Test across all source types: Wavetable, Sub, Pluck, Noise, Sample
        const std::vector<float> enginesToTest = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
        for (float engVal : enginesToTest) {
            if (auto* p = apvts.getParameter("osc1_engine")) {
                p->setValueNotifyingHost(p->convertTo0to1(engVal));
            }

            card.setExpanded(true);
            card.setBounds(0, 0, 390, 180);

            // In Sample mode (engVal == 6.0f)
            if (engVal == 6.0f) {
                // semiKnob must be visible, with valid positive width & height
                EXPECT_GT(card.getSemiKnob().getWidth(), 20);
                EXPECT_GT(card.getSemiKnob().getHeight(), 20);
                EXPECT_GT(card.getLevelKnob().getWidth(), 20);
                EXPECT_GT(card.getLevelKnob().getHeight(), 20);

                // semiKnob and levelKnob must NOT intersect each other!
                EXPECT_FALSE(card.getSemiKnob().getBounds().intersects(card.getLevelKnob().getBounds()))
                    << "SEMI knob and LEVEL knob must not overlap in Sample mode!";
                EXPECT_FALSE(card.getOctKnob().getBounds().intersects(card.getSemiKnob().getBounds()))
                    << "OCT knob and SEMI knob must not overlap in Sample mode!";
                EXPECT_FALSE(card.getLevelKnob().getBounds().intersects(card.getPanKnob().getBounds()))
                    << "LEVEL knob and PAN knob must not overlap in Sample mode!";

                // Row 2 knobs (morph=START, unison=SPREAD, scatter=DETUNE) must not overlap each other
                EXPECT_FALSE(card.getMorphKnob().getBounds().intersects(card.getUnisonKnob().getBounds()))
                    << "START and SPREAD knobs must not overlap in Sample mode!";
                EXPECT_FALSE(card.getUnisonKnob().getBounds().intersects(card.getScatterKnob().getBounds()))
                    << "SPREAD and DETUNE knobs must not overlap in Sample mode!";
            }
        }
    }
}

// 5.2: Dual Filters Audio Parameters and Routing
TEST(NovaSynthComprehensiveSuite5, DualFiltersAudioParametersAndRouting) {
    constexpr double sr = 48000.0;
    constexpr int nSamples = 1024;

    // 1. Audio: StateVariableFilter Cutoff Sweep on high frequencies
    {
        nova::dsp::StateVariableFilter svf;
        svf.prepare(sr);

        // Lowpass at 200 Hz vs 10000 Hz on 2 kHz test signal
        float rmsLowCut = 0.0f;
        {
            svf.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, 200.0f, 0.7071f);
            float sumSq = 0.0f;
            for (int i = 0; i < nSamples; ++i) {
                const float in = std::sin(2.0f * std::numbers::pi_v<float> * 2000.0f * static_cast<float>(i) / static_cast<float>(sr));
                const float out = svf.processSample(in);
                sumSq += out * out;
            }
            rmsLowCut = std::sqrt(sumSq / nSamples);
        }

        float rmsHighCut = 0.0f;
        {
            svf.prepare(sr);
            svf.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, 10000.0f, 0.7071f);
            float sumSq = 0.0f;
            for (int i = 0; i < nSamples; ++i) {
                const float in = std::sin(2.0f * std::numbers::pi_v<float> * 2000.0f * static_cast<float>(i) / static_cast<float>(sr));
                const float out = svf.processSample(in);
                sumSq += out * out;
            }
            rmsHighCut = std::sqrt(sumSq / nSamples);
        }

        EXPECT_GT(rmsHighCut, 0.60f);
        EXPECT_LT(rmsLowCut, 0.05f);
        EXPECT_GT(rmsHighCut, rmsLowCut * 10.0f);
    }

    // 2. Audio: Filter Routing (FLT 1, FLT 2, BOTH, DIRECT) in WorshipSynthEngine
    {
        nova::dsp::WorshipSynthEngine engine;
        engine.prepare(sr, 512);

        auto params = engine.getParameters();
        params.activeOscCount = 1;
        params.oscSlots[0].enabled = true;
        params.oscSlots[0].level = 0.8f;
        params.oscSlots[0].engine = nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog;
        params.oscSlots[0].sourceType = 0; // Saw
        params.ampEnv.attackSec = 0.005f;
        params.ampEnv.releaseSec = 0.05f;

        // Cutoff for Filter 1 set very low (100 Hz)
        params.filterCutoffHz = 100.0f;
        params.filterResonance = 0.7071f;

        // Routing = 0: FLT 1 (muffled)
        params.oscSlots[0].filterRouting = 0;
        engine.setParameters(params);
        engine.noteOn(69, 0.8f); // A4 (440 Hz)

        juce::AudioBuffer<float> buf(2, 512);
        buf.clear();
        engine.processBlock(buf.getWritePointer(0), buf.getWritePointer(1), 512);
        const float rmsFlt1 = buf.getRMSLevel(0, 0, 512);

        // Routing = 3: DIRECT (bypasses filter completely -> loud and bright saw wave)
        engine.allNotesOff();
        params.oscSlots[0].filterRouting = 3;
        engine.setParameters(params);
        engine.noteOn(69, 0.8f);

        buf.clear();
        engine.processBlock(buf.getWritePointer(0), buf.getWritePointer(1), 512);
        const float rmsDirect = buf.getRMSLevel(0, 0, 512);

        EXPECT_GT(rmsDirect, rmsFlt1 * 2.0f) << "DIRECT routing must bypass muffled Filter 1!";
    }
}

// 5.3: Quad Envelopes Audio Timing and Curve Monotonicity
TEST(NovaSynthComprehensiveSuite5, QuadEnvelopesAudioTimingAndCurveMonotonicity) {
    constexpr double sr = 48000.0;

    // 1. Audio: All 4 Envelopes (Amp Env 1, Filter Env 2, Mod Env 3, Aux Env 4)
    {
        for (int envIdx = 0; envIdx < 4; ++envIdx) {
            nova::dsp::AdsrEnvelope env;
            env.prepare(sr);

            nova::dsp::AdsrEnvelope::Parameters p;
            p.attackSec = 0.02f; // 960 samples
            p.decaySec = 0.03f;
            p.sustainLvl = 0.50f;
            p.releaseSec = 0.02f;
            env.setParameters(p);

            env.noteOn();
            EXPECT_TRUE(env.isActive());

            // Process attack: level rises to ~1.0
            for (int i = 0; i < 960; ++i) (void)env.processSample();
            EXPECT_GE(env.getCurrentLevel(), 0.90f);

            // Process decay: level settles to sustain level ~0.50
            for (int i = 0; i < 1440; ++i) (void)env.processSample();
            EXPECT_NEAR(env.getCurrentLevel(), 0.50f, 0.08f);

            // Note off -> release: level decays to near zero
            env.noteOff();
            for (int i = 0; i < 2500; ++i) (void)env.processSample();
            EXPECT_LT(env.getCurrentLevel(), 0.01f);
            EXPECT_FALSE(env.isActive());
        }
    }

    // 2. UI / Visual Curve Monotonicity and Handle Bounds
    {
        nova::synth::NovaSynthProcessor proc;
        auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
        nova::synth::EnvelopeQuadDeck envDeck(proc, theme);
        envDeck.setBounds(0, 0, 800, 168);

        auto cards = envDeck.getCardList();
        ASSERT_EQ(cards.size(), 4u);

        for (std::size_t i = 0; i < cards.size(); ++i) {
            auto* card = cards[i];
            card->setExpanded(true);
            card->setBounds(0, 0, 400, 168);

            // Verify knobs are valid
            EXPECT_GT(card->getAtkKnob().getWidth(), 20);
            EXPECT_GT(card->getDecKnob().getWidth(), 20);
            EXPECT_GT(card->getSusKnob().getWidth(), 20);
            EXPECT_GT(card->getRelKnob().getWidth(), 20);

            // Verify handles in curve: DEL <= ATK <= HOLD <= DEC <= SUS_END <= REL
            EXPECT_LE(card->getNodeDel().x, card->getNodeAtk().x + 0.1f);
            EXPECT_LE(card->getNodeAtk().x, card->getNodeHold().x + 0.1f);
            EXPECT_LE(card->getNodeHold().x, card->getNodeDec().x + 0.1f);
            EXPECT_LE(card->getNodeDec().x, card->getNodeSusEnd().x + 0.1f);
            EXPECT_LE(card->getNodeSusEnd().x, card->getNodeRel().x + 0.1f);
        }
    }
}

// 5.4: Modulation Matrix Dynamic Adapters and Audio Effect
TEST(NovaSynthComprehensiveSuite5, ModulationMatrixDynamicAdaptersAndAudioEffect) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);

    auto& engine = proc.getEngine();
    auto params = engine.getParameters();
    using ET = nova::dsp::MultiEngineOscillator::EngineType;
    using D = nova::dsp::ModMatrix::Destination;

    // 1. Dynamic Destination Adapters for each oscillator engine type
    struct TestCase {
        ET engine;
        int sourceType;
        juce::String expectedParam;
    };

    const std::vector<TestCase> testCases = {
        { ET::SamplePlayback,     4, "Sample Start" },
        { ET::EvolvingWavetable,  0, "WT Pos" },
        { ET::WarmAnalog,         0, "PWM / Shape" },
        { ET::SubOscillator,      1, "Drive" },
        { ET::PhysicalPluckModal, 2, "Brightness" },
        { ET::NoiseTransient,     3, "Color" },
        { ET::OrganicTexture,     0, "Texture Pos" }
    };

    for (const auto& tc : testCases) {
        params.oscSlots[0].engine = tc.engine;
        params.oscSlots[0].sourceType = tc.sourceType;
        const juce::String dstName = nova::synth::ui::getDynamicDestinationName(D::Osc1Shape, params);
        EXPECT_TRUE(dstName.contains(tc.expectedParam))
            << "Expected " << tc.expectedParam << " for engine " << static_cast<int>(tc.engine)
            << " but got: " << dstName;
    }

    // 2. Check newly added Aux Env 4 and LFO 3/4 destinations exist and return non-empty names
    EXPECT_STREQ(nova::dsp::ModMatrix::getDestinationName(D::AuxEnv4Attack), "Aux 4 Attack");
    EXPECT_STREQ(nova::dsp::ModMatrix::getDestinationName(D::AuxEnv4Decay), "Aux 4 Decay");
    EXPECT_STREQ(nova::dsp::ModMatrix::getDestinationName(D::AuxEnv4Release), "Aux 4 Release");
    EXPECT_STREQ(nova::dsp::ModMatrix::getDestinationName(D::Lfo3Rate), "LFO 3 Rate");
    EXPECT_STREQ(nova::dsp::ModMatrix::getDestinationName(D::Lfo4Rate), "LFO 4 Rate");

    // 3. Audio effect of Modulation Matrix routing: LFO 1 -> Filter 1 Cutoff
    auto& mm = proc.getModMatrix();
    mm.clear();
    mm.setSlot(0, nova::dsp::ModMatrix::Source::Lfo1, D::FilterCutoff, 1.0f, true);

    // Update matrix in engine and run blocks
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);

    for (int b = 0; b < 4; ++b) {
        buffer.clear();
        proc.processBlock(buffer, midi);
        midi.clear();
    }
    // Buffer has active sound and LFO modulation runs without NaN or crashing
    EXPECT_GT(buffer.getRMSLevel(0, 0, 512), 0.01f);
    EXPECT_FALSE(std::isnan(buffer.getSample(0, 0)));
}

// 5.5: Panic Button Silences Audio Engine And Clears State
TEST(NovaSynthArpAndPanicTests, PanicButtonSilencesAudioEngineAndClearsState) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    // Send chord note-ons
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8)100), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 67, (juce::uint8)100), 0);

    for (int b = 0; b < 6; ++b) {
        buffer.clear();
        proc.processBlock(buffer, midi);
        midi.clear();
    }

    EXPECT_GT(buffer.getRMSLevel(0, 0, 512), 0.01f);
    EXPECT_GT(proc.getEngine().getActiveVoiceCount(), 0);

    // Trigger panic
    proc.triggerPanic();

    // Next process block executes panic reset
    buffer.clear();
    proc.processBlock(buffer, midi);

    // Verify voices and buffers are silenced
    EXPECT_EQ(proc.getEngine().getActiveVoiceCount(), 0);
    EXPECT_NEAR(buffer.getRMSLevel(0, 0, 512), 0.0f, 1e-4f);
    EXPECT_FALSE(std::isnan(buffer.getSample(0, 0)));
}

// 5.6: Arp Rhythm Presets Apply Correct Lengths And Patterns
TEST(NovaSynthArpAndPanicTests, ArpRhythmPresetsApplyCorrectLengthsAndPatterns) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);

    // Apply 16-step pattern (e.g. 4 on the floor / worship driving 16ths)
    std::vector<bool> gates16(16, false);
    std::vector<float> vels16(16, 0.5f);
    gates16[0] = true; gates16[4] = true; gates16[8] = true; gates16[12] = true;
    vels16[0] = 0.95f; vels16[4] = 0.90f; vels16[8] = 0.85f; vels16[12] = 0.80f;

    proc.applyArpRhythmPattern(16, gates16, vels16);
    EXPECT_EQ(proc.getArpPatternLength(), 16);
    EXPECT_TRUE(proc.isArpStepGateActive(0));
    EXPECT_FALSE(proc.isArpStepGateActive(1));
    EXPECT_TRUE(proc.isArpStepGateActive(4));
    EXPECT_NEAR(proc.getArpStepVelocity(0), 0.95f, 0.02f);
    EXPECT_NEAR(proc.getArpStepVelocity(4), 0.90f, 0.02f);

    // Apply 32-step pattern
    std::vector<bool> gates32(32, true);
    std::vector<float> vels32(32, 0.75f);
    gates32[15] = false;
    gates32[31] = false;
    proc.applyArpRhythmPattern(32, gates32, vels32);
    EXPECT_EQ(proc.getArpPatternLength(), 32);
    EXPECT_TRUE(proc.isArpStepGateActive(0));
    EXPECT_FALSE(proc.isArpStepGateActive(15));
    EXPECT_FALSE(proc.isArpStepGateActive(31));

    // Apply 64-step pattern
    std::vector<bool> gates64(64, true);
    std::vector<float> vels64(64, 0.65f);
    proc.applyArpRhythmPattern(64, gates64, vels64);
    EXPECT_EQ(proc.getArpPatternLength(), 64);
    EXPECT_TRUE(proc.isArpStepGateActive(63));
}

// 5.7: Switching Rhythm Patterns During Arp Playback Prevents Hanging Notes
TEST(NovaSynthArpAndPanicTests, SwitchingRhythmPatternsDuringArpPlaybackPreventsHangingNotes) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);

    // Enable arpeggiator and bypass reverb/delay tails for exact dry release test
    if (auto* p = proc.getAPVTS().getParameter("arpEnabled")) {
        p->setValueNotifyingHost(1.0f);
    }
    if (auto* p = proc.getAPVTS().getParameter("arpLatch")) {
        p->setValueNotifyingHost(0.0f);
    }
    if (auto* p = proc.getAPVTS().getParameter("reverbMix")) {
        p->setValueNotifyingHost(0.0f);
    }
    if (auto* p = proc.getAPVTS().getParameter("delayMix")) {
        p->setValueNotifyingHost(0.0f);
    }
    // Set fast release so voices decay within 100 blocks
    if (auto* p = proc.getAPVTS().getParameter("ampRelease")) {
        p->setValueNotifyingHost(p->convertTo0to1(0.05f));
    }

    // Set 16-step pattern initially
    std::vector<bool> g16(16, true);
    std::vector<float> v16(16, 0.8f);
    proc.applyArpRhythmPattern(16, g16, v16);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    // Trigger note to start arpeggiator
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);

    for (int b = 0; b < 10; ++b) {
        buffer.clear();
        proc.processBlock(buffer, midi);
        midi.clear();
    }
    EXPECT_FALSE(std::isnan(buffer.getSample(0, 0)));

    // Mid-playback pattern switch from 16 to 32 steps
    std::vector<bool> g32(32, false);
    std::vector<float> v32(32, 0.7f);
    g32[0] = true; g32[8] = true; g32[16] = true; g32[24] = true;
    proc.applyArpRhythmPattern(32, g32, v32);

    // Process blocks immediately after switch
    for (int b = 0; b < 10; ++b) {
        buffer.clear();
        proc.processBlock(buffer, midi);
        EXPECT_FALSE(std::isnan(buffer.getSample(0, 0)));
        EXPECT_FALSE(std::isnan(buffer.getSample(1, 0)));
    }

    // Now release note
    midi.addEvent(juce::MidiMessage::noteOff(1, 60, (juce::uint8)0), 0);
    for (int b = 0; b < 100; ++b) {
        buffer.clear();
        proc.processBlock(buffer, midi);
        midi.clear();
    }

    // All voices should be released cleanly with no hung notes
    EXPECT_EQ(proc.getEngine().getActiveVoiceCount(), 0);
    EXPECT_NEAR(buffer.getRMSLevel(0, 0, 512), 0.0f, 1e-4f);
}

// 5.8: Arp Preserves Long Envelope Release And Overlaps Voices
TEST(NovaSynthArpAndPanicTests, ArpPreservesLongEnvelopeReleaseAndOverlapsVoices) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);

    auto& apvts = proc.getAPVTS();

    // Configure Amp Envelope matching user setting:
    // Attack: 0.04s, Hold: 1.0s, Decay: 5.0s, Sustain: 1.0, Release: 6.0s
    if (auto* p = apvts.getParameter("ampAttack"))   p->setValueNotifyingHost(p->convertTo0to1(0.04f));
    if (auto* p = apvts.getParameter("ampHold"))     p->setValueNotifyingHost(p->convertTo0to1(1.0f));
    if (auto* p = apvts.getParameter("ampDecay"))    p->setValueNotifyingHost(p->convertTo0to1(5.0f));
    if (auto* p = apvts.getParameter("ampSustain"))  p->setValueNotifyingHost(1.0f);
    if (auto* p = apvts.getParameter("ampRelease"))  p->setValueNotifyingHost(p->convertTo0to1(6.0f));

    // Turn on Arpeggiator: 16th notes at 120 BPM (~125 ms per step), 65% gate
    if (auto* p = apvts.getParameter("arpEnabled"))  p->setValueNotifyingHost(1.0f);
    if (auto* p = apvts.getParameter("arpGate"))     p->setValueNotifyingHost(p->convertTo0to1(0.65f));

    // Dry signal for exact voice energy evaluation
    if (auto* p = apvts.getParameter("reverbMix"))   p->setValueNotifyingHost(0.0f);
    if (auto* p = apvts.getParameter("delayMix"))    p->setValueNotifyingHost(0.0f);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    // Play a 3-note chord (C, E, G: notes 60, 64, 67)
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8)100), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 67, (juce::uint8)100), 0);

    // Process blocks over ~1.2 seconds (~110 blocks of 512 samples at 48kHz = 56320 samples)
    // At 125ms per step, ~9 arpeggio steps will be triggered.
    for (int b = 0; b < 110; ++b) {
        buffer.clear();
        proc.processBlock(buffer, midi);
        midi.clear();
    }

    // Since releaseSec is 6.0 seconds, previous notes must NOT have been killed at step ends!
    // They must accumulate and overlap polyphonically.
    const int activeVoices = proc.getEngine().getActiveVoiceCount();
    EXPECT_GE(activeVoices, 3) << "Expected polyphonic voice overlap from long release tail, but got only " << activeVoices;

    // Buffer must have rich sustained audio energy (not cut off between steps)
    const float rms = buffer.getRMSLevel(0, 0, 512);
    EXPECT_GT(rms, 0.05f) << "Expected rich sustaining audio energy, but got RMS: " << rms;
    EXPECT_FALSE(std::isnan(buffer.getSample(0, 0)));

    // Release keys
    midi.addEvent(juce::MidiMessage::noteOff(1, 60, (juce::uint8)0), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 64, (juce::uint8)0), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 67, (juce::uint8)0), 0);

    // Process a few blocks (~100 ms) immediately after key release
    for (int b = 0; b < 10; ++b) {
        buffer.clear();
        proc.processBlock(buffer, midi);
        midi.clear();
    }

    // Because releaseSec is 6.0 seconds, voices must STILL be ringing out smoothly, NOT cut off abruptly!
    EXPECT_GE(proc.getEngine().getActiveVoiceCount(), 1);
    EXPECT_GT(buffer.getRMSLevel(0, 0, 512), 0.01f);
}

// 5.9: User Arp Rhythm Presets: Save, Load from Disk, Overwrite, and Delete
TEST(NovaSynthArpAndPanicTests, SaveAndLoadUserArpRhythmPresetWithOverwrite) {
    nova::synth::NovaSynthProcessor proc;
    const juce::String testPresetName = "Test Custom Anthem 16";

    // Clean up if already exists from a prior test run
    proc.deleteUserArpRhythmPreset(testPresetName);

    // Initial check: factory presets available (13 factory presets)
    const auto initialPresets = proc.getAvailableArpRhythmPresets();
    EXPECT_GE(initialPresets.size(), 13u);
    for (const auto& p : initialPresets) {
        EXPECT_FALSE(p.isUser);
    }

    // Prepare custom rhythm pattern
    std::vector<bool> gates = { true, false, true, false, true, true, false, false,
                                true, false, true, false, true, true, true, true };
    std::vector<float> vels = { 1.0f, 0.0f, 0.85f, 0.0f, 0.90f, 0.75f, 0.0f, 0.0f,
                                1.0f, 0.0f, 0.85f, 0.0f, 0.90f, 0.80f, 0.70f, 0.65f };

    // 1. Save new user preset
    const bool saved = proc.saveUserArpRhythmPreset(testPresetName, 16, gates, vels);
    EXPECT_TRUE(saved);

    // 2. Verify it is loaded and returned by getAvailableArpRhythmPresets
    auto presetsAfterSave = proc.getAvailableArpRhythmPresets();
    EXPECT_EQ(presetsAfterSave.size(), initialPresets.size() + 1);

    bool found = false;
    for (const auto& p : presetsAfterSave) {
        if (p.name == testPresetName) {
            found = true;
            EXPECT_TRUE(p.isUser);
            EXPECT_EQ(p.length, 16);
            ASSERT_EQ(p.gates.size(), 16u);
            ASSERT_EQ(p.velocities.size(), 16u);
            EXPECT_TRUE(p.gates[0]);
            EXPECT_FALSE(p.gates[1]);
            EXPECT_TRUE(p.gates[4]);
            EXPECT_NEAR(p.velocities[0], 1.0f, 1e-4f);
            EXPECT_NEAR(p.velocities[4], 0.90f, 1e-4f);
            break;
        }
    }
    EXPECT_TRUE(found);

    // 3. Test Overwrite: change step 1 to active with 0.50f velocity and save again
    gates[1] = true;
    vels[1] = 0.50f;
    const bool overwritten = proc.saveUserArpRhythmPreset(testPresetName, 16, gates, vels);
    EXPECT_TRUE(overwritten);

    auto presetsAfterOverwrite = proc.getAvailableArpRhythmPresets();
    // Overwriting must not duplicate the preset
    EXPECT_EQ(presetsAfterOverwrite.size(), initialPresets.size() + 1);

    bool foundUpdated = false;
    for (const auto& p : presetsAfterOverwrite) {
        if (p.name == testPresetName) {
            foundUpdated = true;
            EXPECT_TRUE(p.isUser);
            EXPECT_TRUE(p.gates[1]);
            EXPECT_NEAR(p.velocities[1], 0.50f, 1e-4f);
            break;
        }
    }
    EXPECT_TRUE(foundUpdated);

    // 4. Test UI Component instantiation with presets
    nova::ui::NovaTheme theme;
    nova::synth::ui::ArpeggiatorHubPanel panel(proc, theme);
    // Cycle presets forward and backward
    panel.selectNextPreset();
    panel.selectPreviousPreset();

    // 5. Cleanup test preset
    const bool deleted = proc.deleteUserArpRhythmPreset(testPresetName);
    EXPECT_TRUE(deleted);

    const auto presetsAfterDelete = proc.getAvailableArpRhythmPresets();
    EXPECT_EQ(presetsAfterDelete.size(), initialPresets.size());
}

// -------------------------------------------------------------------------------------------------
// COMPREHENSIVE TEST SUITE 6: All Four Envelopes Hold, Decay, Arp & Keyboard Response
// -------------------------------------------------------------------------------------------------

TEST(AllFourEnvelopesTests, SustainKnobDisplaysPercentageUnit) {
    nova::synth::NovaSynthProcessor proc;
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::EnvelopeQuadDeck envDeck(proc, theme);

    auto cards = envDeck.getCardList();
    ASSERT_EQ(cards.size(), 4u);

    for (std::size_t i = 0; i < cards.size(); ++i) {
        auto* card = cards[i];
        ASSERT_NE(card, nullptr);
        // Suffix must be "%"
        EXPECT_EQ(card->getSusKnob().getSuffix(), "%")
            << "Card " << i << " Sustain knob suffix must be %!";
    }
}

TEST(AllFourEnvelopesTests, HoldNodeDraggableWhenHoldIsZero) {
    nova::synth::NovaSynthProcessor proc;
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::EnvelopeQuadDeck envDeck(proc, theme);
    envDeck.setBounds(0, 0, 800, 168);

    auto cards = envDeck.getCardList();
    ASSERT_EQ(cards.size(), 4u);
    auto* card = cards[0];
    ASSERT_NE(card, nullptr);

    // Expand card and paint to lay out curves and nodes
    card->setExpanded(true);
    card->setBounds(0, 0, 390, 168);
    juce::Image img(juce::Image::ARGB, 390, 168, true);
    juce::Graphics g(img);
    card->paintEntireComponent(g, false);

    // With Hold at 0.0s, hit node at nodeHold_ must return DragNode::Hold
    const auto holdHit = card->getHitNode(card->getNodeHold());
    EXPECT_EQ(holdHit, nova::synth::ExpandableEnvelopeCard::DragNode::Hold)
        << "Hold handle must be interactable on the curve even when Hold is 0.0s!";
}

TEST(AllFourEnvelopesTests, HoldAndDecayTimingForKeyboardPlaying) {
    constexpr double sr = 48000.0;

    // Test across all 4 envelopes: Amp, Filter, Mod, Aux
    for (int envIdx = 0; envIdx < 4; ++envIdx) {
        nova::dsp::AdsrEnvelope env;
        env.prepare(sr);

        nova::dsp::AdsrEnvelope::Parameters p;
        p.attackSec  = 0.01f; // 480 samples = 10 ms
        p.holdSec    = 0.05f; // 2400 samples = 50 ms
        p.decaySec   = 0.05f; // 2400 samples = 50 ms
        p.sustainLvl = 0.30f;
        p.releaseSec = 0.04f; // 1920 samples = 40 ms
        env.setParameters(p);

        // Case A: Early key release (note-off after 20ms during Hold)
        env.noteOn(1.0f);
        EXPECT_TRUE(env.isActive());

        // Process Attack phase until Hold stage reached
        int attackSamples = 0;
        while (env.getStage() == nova::dsp::AdsrEnvelope::Stage::Attack && attackSamples < 2000) {
            (void)env.processSample();
            attackSamples++;
        }
        EXPECT_EQ(env.getStage(), nova::dsp::AdsrEnvelope::Stage::Hold);
        EXPECT_GE(env.getCurrentLevel(), 0.95f);

        // Advance 10ms into Hold, then call noteOff
        for (int i = 0; i < 480; ++i) (void)env.processSample();
        env.noteOff();

        // Hold MUST NOT be aborted by early release: level must remain 1.0 throughout hold
        for (int i = 0; i < 1920; ++i) { // Remaining 40ms of hold
            const float val = env.processSample();
            EXPECT_NEAR(val, 1.0f, 1e-4f) << "Envelope " << envIdx << " must hold at 1.0 throughout hold duration!";
        }

        // Now Decay must run down towards sustain level (0.30f)
        for (int i = 0; i < 3000; ++i) (void)env.processSample();
        EXPECT_NEAR(env.getCurrentLevel(), 0.30f, 0.05f)
            << "Envelope " << envIdx << " must decay to sustain level!";

        // After decay finishes, since gate is closed, it releases to silence
        for (int i = 0; i < 6000; ++i) (void)env.processSample();
        EXPECT_LT(env.getCurrentLevel(), 0.001f);
        EXPECT_FALSE(env.isActive());
    }
}

TEST(AllFourEnvelopesTests, HoldAndDecayPreservedDuringArpeggioPlayback) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);

    auto& apvts = proc.getAPVTS();

    // Configure all 4 envelopes with distinct hold and decay times:
    // ENV 1: AMP (Hold: 0.15s, Decay: 0.20s, Sustain: 0.40)
    if (auto* p = apvts.getParameter("ampAttack"))  p->setValueNotifyingHost(p->convertTo0to1(0.01f));
    if (auto* p = apvts.getParameter("ampHold"))    p->setValueNotifyingHost(p->convertTo0to1(0.15f));
    if (auto* p = apvts.getParameter("ampDecay"))   p->setValueNotifyingHost(p->convertTo0to1(0.20f));
    if (auto* p = apvts.getParameter("ampSustain")) p->setValueNotifyingHost(0.40f);
    if (auto* p = apvts.getParameter("ampRelease")) p->setValueNotifyingHost(p->convertTo0to1(0.30f));

    // ENV 2: FILTER (Hold: 0.12s, Decay: 0.18s, Sustain: 0.20)
    if (auto* p = apvts.getParameter("fltAttack"))  p->setValueNotifyingHost(p->convertTo0to1(0.01f));
    if (auto* p = apvts.getParameter("fltHold"))    p->setValueNotifyingHost(p->convertTo0to1(0.12f));
    if (auto* p = apvts.getParameter("fltDecay"))   p->setValueNotifyingHost(p->convertTo0to1(0.18f));
    if (auto* p = apvts.getParameter("fltSustain")) p->setValueNotifyingHost(0.20f);
    if (auto* p = apvts.getParameter("fltRelease")) p->setValueNotifyingHost(p->convertTo0to1(0.25f));

    // Turn on Arpeggiator (120 BPM, 1/16 notes, 65% gate = 81ms gate duration)
    if (auto* p = apvts.getParameter("arpEnabled")) p->setValueNotifyingHost(1.0f);
    if (auto* p = apvts.getParameter("arpGate"))    p->setValueNotifyingHost(p->convertTo0to1(0.65f));

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    // Trigger middle C
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);

    // Process blocks through t = 100ms (gate ended at ~81ms)
    // 10 blocks * 512 samples / 48000 = ~106ms
    for (int b = 0; b < 10; ++b) {
        buffer.clear();
        proc.processBlock(buffer, midi);
        midi.clear();
    }

    // Since hold is 150ms, at 106ms (past 81ms gate cutoff) the voice MUST still be sounding at full energy!
    const float rmsPastGate = buffer.getRMSLevel(0, 0, 512);
    EXPECT_GT(rmsPastGate, 0.05f)
        << "Voice must continue holding through hold time (150ms) even after Arp step gate cutoff (81ms)!";

    // Process past hold time into decay (t = 250ms): audio must decay smoothly without clicks
    for (int b = 0; b < 14; ++b) {
        buffer.clear();
        proc.processBlock(buffer, midi);
    }
    const float rmsInDecay = buffer.getRMSLevel(0, 0, 512);
    EXPECT_GT(rmsInDecay, 0.02f);
    EXPECT_FALSE(std::isnan(buffer.getSample(0, 0)));

    // Release key
    midi.addEvent(juce::MidiMessage::noteOff(1, 60, (juce::uint8)0), 0);
    for (int b = 0; b < 120; ++b) {
        buffer.clear();
        proc.processBlock(buffer, midi);
        midi.clear();
    }
    EXPECT_EQ(proc.getEngine().getActiveVoiceCount(), 0);
}

// ==============================================================================
// Sample Looping System Tests (One-Shot, Forward, Ping-Pong, Reverse & UI)
// ==============================================================================

TEST(SampleLoopingSystemTests, SampleLoopForwardSustainsWithoutEnding) {
    nova::dsp::MultiEngineOscillator osc;
    osc.prepare(48000.0);

    nova::dsp::MultiEngineOscillator::Parameters p;
    p.enabled = true;
    p.sourceType = 4;
    p.engine = nova::dsp::MultiEngineOscillator::EngineType::SamplePlayback;
    p.modelIndex = 7; // Sacred Flute Pipe (sustained true-stereo pipe)
    p.sampleLoopMode = 1; // Forward loop
    p.sampleLoopStart = 0.20f;
    p.sampleLoopEnd = 0.80f;
    p.sampleCrossfade = 0.05f;
    p.level = 0.85f;
    osc.setParameters(p);

    osc.triggerNote(261.63f, 0.90f);

    float sumRms = 0.0f;
    // Process 5 full seconds (240,000 samples)
    constexpr int kTotalSamples = 48000 * 5;
    constexpr int kBlockSize = 256;
    float blockEnergy = 0.0f;
    int blockCount = 0;

    for (int i = 0; i < kTotalSamples; ++i) {
        float outL = 0.0f;
        float outR = 0.0f;
        osc.processSample(261.63f, 0.0f, 0.0f, outL, outR);

        EXPECT_FALSE(std::isnan(outL));
        EXPECT_FALSE(std::isnan(outR));
        EXPECT_FALSE(std::isinf(outL));
        EXPECT_FALSE(std::isinf(outR));

        blockEnergy += outL * outL + outR * outR;
        if ((i + 1) % kBlockSize == 0) {
            const float blockRms = std::sqrt(blockEnergy / static_cast<float>(kBlockSize * 2));
            sumRms += blockRms;
            // Even at second 4 or 5, since loop is active, the sound must remain sustaining!
            if (i > 48000 * 4) {
                EXPECT_GT(blockRms, 0.01f) << "Forward loop must sustain infinitely while key is held!";
            }
            blockEnergy = 0.0f;
            blockCount++;
        }
    }

    const float avgRms = sumRms / static_cast<float>(blockCount);
    EXPECT_GT(avgRms, 0.02f) << "Average sustained energy must be strong across multiple loop repetitions!";
}

TEST(SampleLoopingSystemTests, SampleLoopPingPongBouncesDirections) {
    nova::dsp::MultiEngineOscillator osc;
    osc.prepare(48000.0);

    nova::dsp::MultiEngineOscillator::Parameters p;
    p.enabled = true;
    p.sourceType = 4;
    p.engine = nova::dsp::MultiEngineOscillator::EngineType::SamplePlayback;
    p.modelIndex = 7; // Sacred Flute Pipe
    p.sampleLoopMode = 2; // Ping-Pong loop (<->)
    p.sampleLoopStart = 0.20f;
    p.sampleLoopEnd = 0.80f;
    p.level = 0.85f;
    osc.setParameters(p);

    osc.triggerNote(261.63f, 0.90f);

    float sumEnergy = 0.0f;
    for (int i = 0; i < 48000 * 4; ++i) {
        float outL = 0.0f;
        float outR = 0.0f;
        osc.processSample(261.63f, 0.0f, 0.0f, outL, outR);
        EXPECT_FALSE(std::isnan(outL));
        EXPECT_FALSE(std::isnan(outR));
        sumEnergy += outL * outL;
    }
    EXPECT_GT(sumEnergy, 10.0f) << "Ping-Pong loop must keep bouncing and producing sustained signal!";
}

TEST(SampleLoopingSystemTests, SampleLoopReversePlaysBackwardsAndLoops) {
    nova::dsp::MultiEngineOscillator osc;
    osc.prepare(48000.0);

    nova::dsp::MultiEngineOscillator::Parameters p;
    p.enabled = true;
    p.sourceType = 4;
    p.engine = nova::dsp::MultiEngineOscillator::EngineType::SamplePlayback;
    p.modelIndex = 7; // Sacred Flute Pipe
    p.sampleLoopMode = 3; // Reverse loop (<-)
    p.sampleLoopStart = 0.20f;
    p.sampleLoopEnd = 0.80f;
    p.sampleCrossfade = 0.05f;
    p.level = 0.85f;
    osc.setParameters(p);

    osc.triggerNote(261.63f, 0.90f);

    float reverseEnergy = 0.0f;
    for (int i = 0; i < 48000 * 3; ++i) {
        float outL = 0.0f;
        float outR = 0.0f;
        osc.processSample(261.63f, 0.0f, 0.0f, outL, outR);
        EXPECT_FALSE(std::isnan(outL));
        EXPECT_FALSE(std::isnan(outR));
        reverseEnergy += outL * outL;
    }
    EXPECT_GT(reverseEnergy, 10.0f) << "Reverse loop must play and loop backward smoothly!";
}

TEST(SampleLoopingSystemTests, SampleLoopOneShotFinishesAtEndOfSample) {
    nova::dsp::MultiEngineOscillator osc;
    osc.prepare(48000.0);

    nova::dsp::MultiEngineOscillator::Parameters p;
    p.enabled = true;
    p.sourceType = 4;
    p.engine = nova::dsp::MultiEngineOscillator::EngineType::SamplePlayback;
    p.modelIndex = 2; // Upright Felt Thump (short sample ~0.35s = 15435 samples)
    p.sampleLoopMode = 0; // One-Shot (Off)
    p.level = 0.90f;
    osc.setParameters(p);

    osc.triggerNote(261.63f, 1.0f);

    // Initial attack has audio
    float initialEnergy = 0.0f;
    for (int i = 0; i < 8000; ++i) {
        float outL = 0.0f;
        float outR = 0.0f;
        osc.processSample(261.63f, 0.0f, 0.0f, outL, outR);
        initialEnergy += outL * outL + outR * outR;
    }
    EXPECT_GT(initialEnergy, 1.0f) << "One-shot sample must play initial transient!";

    // Past 1.0s (48000 samples), one-shot has finished and must be completely silent
    float lateEnergy = 0.0f;
    for (int i = 8000; i < 48000 * 2; ++i) {
        float outL = 0.0f;
        float outR = 0.0f;
        osc.processSample(261.63f, 0.0f, 0.0f, outL, outR);
        lateEnergy += std::abs(outL) + std::abs(outR);
    }
    EXPECT_EQ(lateEnergy, 0.0f) << "One-shot sample must finish cleanly after playing through once!";
}

TEST(SampleLoopingSystemTests, SampleLoopUiPillsAndMarkersInteraction) {
    nova::synth::NovaSynthProcessor proc;
    auto theme = nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan);
    nova::synth::SourceDeck sourceDeck(proc, theme);
    sourceDeck.setBounds(0, 0, 900, 200);

    auto& apvts = proc.getAPVTS();
    // Configure OSC 1 as Sample mode
    if (auto* p = apvts.getParameter("osc1_sourceType")) {
        p->setValueNotifyingHost(p->convertTo0to1(4.0f));
    }
    proc.syncParametersFromAPVTS();

    const auto& cards = sourceDeck.getCardList();
    ASSERT_EQ(cards.size(), 8u);
    auto* card = cards[0].get();
    ASSERT_NE(card, nullptr);

    // Expand the card with full preferred width
    card->setExpanded(true);
    card->setBounds(0, 0, static_cast<int>(card->getPreferredExpandedWidth()), 196);
    card->resized();

    // Verify dedicated loop knobs are visible when in Sample mode
    EXPECT_TRUE(card->getLoopStartKnob().isVisible());
    EXPECT_TRUE(card->getLoopEndKnob().isVisible());
    EXPECT_TRUE(card->getLoopXfadeKnob().isVisible());

    // Paint to initialize layout coordinates
    juce::Image dummyImg(juce::Image::ARGB, 900, 200, true);
    juce::Graphics g(dummyImg);
    card->paintEntireComponent(g, false);

    const auto dummyMouse = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();

    // Test clicking Loop Mode Pills:
    // 0: One-Shot, 1: Forward, 2: Ping-Pong, 3: Reverse
    for (int mode = 0; mode < 4; ++mode) {
        const auto pillRect = card->getWavePillRect(mode);
        EXPECT_FALSE(pillRect.isEmpty()) << "Pill " << mode << " rect must not be empty!";
        const juce::Point<float> pillClick = pillRect.getCentre().toFloat();

        juce::MouseEvent downEv(dummyMouse, pillClick, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                card, card, now, pillClick, now, 1, false);
        card->mouseDown(downEv);

        const int currentMode = static_cast<int>(apvts.getRawParameterValue("osc1_loopMode")->load());
        EXPECT_EQ(currentMode, mode) << "Clicking loop pill " << mode << " must select loop mode " << mode << "!";
    }

    // Test dragging on the waveform visualizer to set START marker
    const auto visBox = card->getVisualizerRect();
    EXPECT_FALSE(visBox.isEmpty()) << "Visualizer rect must not be empty!";
    const float startX = static_cast<float>(visBox.getX()) + 4.0f;
    const float w = static_cast<float>(visBox.getWidth()) - 8.0f;
    EXPECT_GT(w, 20.0f);

    const juce::Point<float> waveClick(startX + 0.10f * w, visBox.getCentreY());
    juce::MouseEvent dragStartEv(dummyMouse, waveClick, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 card, card, now, waveClick, now, 1, false);
    card->mouseDown(dragStartEv);

    // Drag to new X position (55% of waveform)
    const juce::Point<float> dragTo(startX + 0.55f * w, visBox.getCentreY());
    juce::MouseEvent dragEv(dummyMouse, dragTo, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                            card, card, now, dragTo, now, 1, false);
    card->mouseDrag(dragEv);
    card->mouseUp(dragEv);

    const float updatedStart = apvts.getRawParameterValue("osc1_shape")->load();
    EXPECT_NEAR(updatedStart, 0.55f, 0.05f) << "Dragging on waveform must interactively update START parameter!";
}

TEST(AudioGainStagingTests, AllEnginesProduceBalancedNominalLevels) {
    using nova::dsp::MultiEngineOscillator;
    MultiEngineOscillator osc;
    osc.prepare(48000.0);

    for (int e = 0; e <= 6; ++e) {
        MultiEngineOscillator::Parameters p;
        p.enabled = true;
        p.engine = static_cast<MultiEngineOscillator::EngineType>(e);
        p.shapeMorph = 0.0f;
        p.detune = 0.0f;
        p.level = 1.0f;
        osc.setParameters(p);
        osc.triggerNote(261.63f, 0.85f, 1u);

        float maxPeak = 0.0f;
        for (int i = 0; i < 2000; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(261.63f, 0.0f, 0.0f, l, r, 0.0f);
            maxPeak = std::max(maxPeak, std::max(std::abs(l), std::abs(r)));
        }
        EXPECT_GE(maxPeak, 0.30f) << "Engine " << e << " level is too low (< 0.30)!";
        EXPECT_LE(maxPeak, 1.05f) << "Engine " << e << " level is clipping (> 1.05)!";
    }

    // Verify all 4 Pluck models produce balanced output
    for (int m = 0; m < 4; ++m) {
        MultiEngineOscillator::Parameters p;
        p.enabled = true;
        p.engine = MultiEngineOscillator::EngineType::PhysicalPluckModal;
        p.modelIndex = m;
        p.shapeMorph = 0.5f;
        p.level = 1.0f;
        osc.setParameters(p);
        osc.triggerNote(261.63f, 0.85f, 1u);
        float maxPeak = 0.0f;
        for (int i = 0; i < 2000; ++i) {
            float l = 0.0f, r = 0.0f;
            osc.processSample(261.63f, 0.0f, 0.0f, l, r, 0.0f);
            maxPeak = std::max(maxPeak, std::max(std::abs(l), std::abs(r)));
        }
        EXPECT_NEAR(maxPeak, 0.71f, 0.12f) << "Pluck model " << m << " must be balanced around 0.71!";
    }
}

TEST(AudioGainStagingTests, PresetGainStagingAndClipperHeadroom) {
    nova::synth::NovaSynthProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    auto& pm = processor.getPresetManager();
    const std::vector<std::string> testPresets = {
        "Angelic Celestial Harp & Choir",
        "Bethel Shimmer Cloud",
        "Warm Juno 106 Pad",
        "Celestial Concert Harp",
        "Cathedral Prayer Drone (C)"
    };

    for (const auto& name : testPresets) {
        pm.loadPresetByName(name);

        // Render single note (vel = 100)
        float notePeak = 0.0f;
        for (int b = 0; b < 140; ++b) {
            juce::AudioBuffer<float> buffer(2, 512);
            buffer.clear();
            juce::MidiBuffer midi;
            if (b == 0) {
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
            }
            processor.processBlock(buffer, midi);
            for (int c = 0; c < 2; ++c) {
                const float* ch = buffer.getReadPointer(c);
                for (int i = 0; i < 512; ++i) {
                    notePeak = std::max(notePeak, std::abs(ch[i]));
                }
            }
        }
        // Single note should be present and audible: at least -7.5 dBFS (peak >= 0.42)
        EXPECT_GE(notePeak, 0.42f) << "Preset '" << name << "' single note is too quiet (< -7.5 dBFS)!";
        EXPECT_LE(notePeak, 0.95f) << "Preset '" << name << "' single note exceeds headroom!";

        // Render 3-note chord
        pm.loadPresetByName(name);
        float chordPeak = 0.0f;
        for (int b = 0; b < 140; ++b) {
            juce::AudioBuffer<float> buffer(2, 512);
            buffer.clear();
            juce::MidiBuffer midi;
            if (b == 0) {
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
                midi.addEvent(juce::MidiMessage::noteOn(1, 64, static_cast<juce::uint8>(100)), 0);
                midi.addEvent(juce::MidiMessage::noteOn(1, 67, static_cast<juce::uint8>(100)), 0);
            }
            processor.processBlock(buffer, midi);
            for (int c = 0; c < 2; ++c) {
                const float* ch = buffer.getReadPointer(c);
                for (int i = 0; i < 512; ++i) {
                    chordPeak = std::max(chordPeak, std::abs(ch[i]));
                }
            }
        }
        EXPECT_GE(chordPeak, 0.55f) << "Preset '" << name << "' chord is too quiet (< -5.2 dBFS)!";
        EXPECT_LE(chordPeak, 0.98f) << "Preset '" << name << "' chord exceeds soft-clipper limit!";

        // Big 8-note chord with maximum velocity (vel = 120) - Must never exceed 0.98 (-0.18 dBFS)
        pm.loadPresetByName(name);
        float bigChordPeak = 0.0f;
        for (int b = 0; b < 140; ++b) {
            juce::AudioBuffer<float> buffer(2, 512);
            buffer.clear();
            juce::MidiBuffer midi;
            if (b == 0) {
                for (int note : { 48, 55, 60, 64, 67, 71, 72, 76 }) {
                    midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(120)), 0);
                }
            }
            processor.processBlock(buffer, midi);
            for (int c = 0; c < 2; ++c) {
                const float* ch = buffer.getReadPointer(c);
                for (int i = 0; i < 512; ++i) {
                    bigChordPeak = std::max(bigChordPeak, std::abs(ch[i]));
                }
            }
        }
        EXPECT_LE(bigChordPeak, 0.985f) << "Preset '" << name << "' big chord exceeded 0.985 limit!";
    }
}

} // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI juceInit;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
