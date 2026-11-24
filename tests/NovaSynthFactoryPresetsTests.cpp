#include <gtest/gtest.h>
#include "NovaSynthProcessor.h"
#include "nova_dsp/filters/StateVariableFilter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <vector>

namespace {

struct SvfBandpass {
    nova::dsp::StateVariableFilter hp;
    nova::dsp::StateVariableFilter lp;

    void prepare(double sr, float lowHz, float highHz) noexcept {
        hp.prepare(sr);
        lp.prepare(sr);
        hp.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, lowHz, 0.7071f);
        lp.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, highHz, 0.7071f);
    }

    [[nodiscard]] float processSample(float in) noexcept {
        return lp.processSample(hp.processSample(in));
    }
};

struct SpectralMetrics {
    float rmsTotal = 0.0f;
    float rmsFundament = 0.0f; // 80 - 250 Hz
    float rmsHarshMid = 0.0f;  // 1000 - 2500 Hz
    float rmsAir = 0.0f;       // 4000 - 12000 Hz
    float rmsHigh1200 = 0.0f;  // > 1200 Hz
    float peak = 0.0f;
};

SpectralMetrics renderWorshipChordMetrics(nova::synth::NovaSynthProcessor& proc,
                                          float swellVal,
                                          int warmupBlocks = 112,
                                          int measureBlocks = 75) {
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    proc.triggerPanic();
    proc.setModWheelFromUI(swellVal);

    juce::AudioBuffer<float> buf(2, kBlock);
    juce::MidiBuffer midi;
    const int cc1 = std::clamp(static_cast<int>(std::round(swellVal * 127.0f)), 0, 127);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, cc1), 0);

    constexpr std::array<int, 5> kChord = { 48, 55, 60, 64, 67 };
    for (int note : kChord) {
        midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(95)), 0);
    }

    for (int b = 0; b < warmupBlocks; ++b) {
        buf.clear();
        proc.processBlock(buf, midi);
        midi.clear();
    }

    SvfBandpass bpFund, bpMid, bpAir;
    bpFund.prepare(kSr, 80.0f, 250.0f);
    bpMid.prepare(kSr, 1000.0f, 2500.0f);
    bpAir.prepare(kSr, 4000.0f, 12000.0f);

    nova::dsp::StateVariableFilter hp1200;
    hp1200.prepare(kSr);
    hp1200.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, 1200.0f, 0.7071f);

    double sqTotal = 0.0, sqFund = 0.0, sqMid = 0.0, sqAir = 0.0, sqHi = 0.0;
    float maxPeak = 0.0f;
    std::size_t count = 0;

    for (int b = 0; b < measureBlocks; ++b) {
        buf.clear();
        proc.processBlock(buf, midi);
        const float* l = buf.getReadPointer(0);
        const float* r = buf.getReadPointer(1);
        for (int i = 0; i < kBlock; ++i) {
            EXPECT_TRUE(std::isfinite(l[i]) && std::isfinite(r[i]));
            maxPeak = std::max(maxPeak, std::max(std::abs(l[i]), std::abs(r[i])));
            const float m = 0.5f * (l[i] + r[i]);
            sqTotal += static_cast<double>(m) * static_cast<double>(m);
            const float f = bpFund.processSample(m);
            const float md = bpMid.processSample(m);
            const float a = bpAir.processSample(m);
            const float h = hp1200.processSample(m);
            sqFund += static_cast<double>(f) * static_cast<double>(f);
            sqMid  += static_cast<double>(md) * static_cast<double>(md);
            sqAir  += static_cast<double>(a) * static_cast<double>(a);
            sqHi   += static_cast<double>(h) * static_cast<double>(h);
            ++count;
        }
    }

    const double invN = 1.0 / static_cast<double>(std::max<std::size_t>(1, count));
    SpectralMetrics res;
    res.rmsTotal    = static_cast<float>(std::sqrt(sqTotal * invN));
    res.rmsFundament = static_cast<float>(std::sqrt(sqFund * invN));
    res.rmsHarshMid = static_cast<float>(std::sqrt(sqMid * invN));
    res.rmsAir      = static_cast<float>(std::sqrt(sqAir * invN));
    res.rmsHigh1200 = static_cast<float>(std::sqrt(sqHi * invN));
    res.peak        = maxPeak;
    return res;
}

} // namespace

TEST(NovaSynthFactoryPresetsTests, CleanDropdownStructureAndGuardrails) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);

    auto& pm = proc.getPresetManager();
    const auto& allPresets = pm.getAllPresets();

    std::vector<const nova::plugin::PresetEntry*> factoryPresets;
    std::vector<juce::String> factoryCategoriesOrdered;
    for (const auto& p : allPresets) {
        if (!p.isFactory) continue;
        factoryPresets.push_back(&p);
        if (std::find(factoryCategoriesOrdered.begin(), factoryCategoriesOrdered.end(), p.category)
            == factoryCategoriesOrdered.end()) {
            factoryCategoriesOrdered.push_back(p.category);
        }
    }

    ASSERT_EQ(factoryPresets.size(), 9u);
    ASSERT_EQ(factoryCategoriesOrdered.size(), 3u);
    EXPECT_EQ(factoryCategoriesOrdered[0], "Warm Pads");
    EXPECT_EQ(factoryCategoriesOrdered[1], "Airy & Shimmer");
    EXPECT_EQ(factoryCategoriesOrdered[2], "Utility");

    const auto cats = pm.getCategories();
    ASSERT_GE(cats.size(), 4);
    EXPECT_EQ(cats[0], "All");
    EXPECT_EQ(cats[1], "Warm Pads");
    EXPECT_EQ(cats[2], "Airy & Shimmer");
    EXPECT_EQ(cats[3], "Utility");

    EXPECT_EQ(pm.getCurrentPresetIndex(), 0);
    EXPECT_EQ(pm.getCurrentPresetName(), "Soft Velvet Cushion");
    EXPECT_EQ(pm.getCurrentPresetCategory(), "Warm Pads");

    const std::vector<std::pair<juce::String, juce::String>> expectedRoster = {
        { "Soft Velvet Cushion",    "Warm Pads" },
        { "Warm Analog Bed",        "Warm Pads" },
        { "Silky Bowed Strings",    "Warm Pads" },
        { "Intimate Tape Choir",    "Warm Pads" },
        { "Airy Silk Cloud",        "Airy & Shimmer" },
        { "Crystal Glass Veil",     "Airy & Shimmer" },
        { "Vocal Mist Halo",        "Airy & Shimmer" },
        { "Silver String Swell",    "Airy & Shimmer" },
        { "Init Patch",             "Utility" }
    };
    for (std::size_t i = 0; i < expectedRoster.size(); ++i) {
        EXPECT_EQ(factoryPresets[i]->name, expectedRoster[i].first);
        EXPECT_EQ(factoryPresets[i]->category, expectedRoster[i].second);
    }

    ASSERT_TRUE(pm.loadPresetByName("Init Patch"));
    EXPECT_EQ(pm.getCurrentPresetCategory(), "Utility");

    for (std::size_t idx = 0; idx < allPresets.size(); ++idx) {
        const auto& preset = allPresets[idx];
        if (!preset.isFactory || preset.category == "Utility") continue;

        pm.loadPresetByIndex(static_cast<int>(idx));
        proc.syncParametersFromAPVTS();
        const auto params = proc.getCurrentParameters();
        auto& apvts = proc.getAPVTS();

        for (int s = 0; s < params.activeOscCount; ++s) {
            const auto& slot = params.oscSlots[static_cast<std::size_t>(s)];
            if (slot.enabled) {
                EXPECT_NE(slot.engine, nova::dsp::MultiEngineOscillator::EngineType::EvolvingWavetable)
                    << "Organ/Wavetable engine must not be used in pad '" << preset.name.toStdString() << "'";
            }
        }

        const float osc1Fine = apvts.getRawParameterValue("osc1_fine")->load();
        const float osc2Fine = apvts.getRawParameterValue("osc2_fine")->load();
        EXPECT_LT(osc1Fine * osc2Fine, 0.0f)
            << "Opposing fine tune required on '" << preset.name.toStdString() << "'";

        EXPECT_GE(params.analogDrift, 0.40f) << preset.name.toStdString();
        EXPECT_LE(params.analogDrift, 0.70f) << preset.name.toStdString();
        EXPECT_GE(params.filterKeyTrack, 0.40f) << preset.name.toStdString();
        EXPECT_LE(params.filterKeyTrack, 0.65f) << preset.name.toStdString();
        EXPECT_GE(params.filterResonance, 0.55f) << preset.name.toStdString();
        EXPECT_LE(params.filterResonance, 0.95f) << preset.name.toStdString();
        EXPECT_LE(params.filterDrive, 0.15f) << preset.name.toStdString();

        bool hasModWheelCutoff = false;
        const auto& mm = proc.getModMatrix();
        for (std::size_t m = 0; m < nova::dsp::ModMatrix::kMaxSlots; ++m) {
            const auto& sl = mm.getSlot(m);
            if (sl.enabled
                && sl.source == nova::dsp::ModMatrix::Source::ModWheel
                && sl.destination == nova::dsp::ModMatrix::Destination::FilterCutoff
                && sl.amount >= 0.35f) {
                hasModWheelCutoff = true;
            }
        }
        EXPECT_TRUE(hasModWheelCutoff) << "Missing ModWheel->FilterCutoff route on '" << preset.name.toStdString() << "'";
    }
}

TEST(NovaSynthFactoryPresetsTests, AllFactoryPresetsAcousticInvariantsSwellPanicAndRoundtrip) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);

    auto& pm = proc.getPresetManager();
    const auto& allPresets = pm.getAllPresets();

    for (std::size_t idx = 0; idx < allPresets.size(); ++idx) {
        if (!allPresets[idx].isFactory) continue;
        const auto name = allPresets[idx].name.toStdString();

        for (float swell : { 0.0f, 0.35f, 0.70f, 1.0f }) {
            pm.loadPresetByIndex(static_cast<int>(idx));
            const auto metrics = renderWorshipChordMetrics(proc, swell, 128, 36);
            EXPECT_LT(metrics.peak, 3.5f) << name << " swell=" << swell;
            EXPECT_LE(metrics.peak, 1.01f) << name << " swell=" << swell;
            EXPECT_GT(metrics.rmsTotal, 0.002f) << name << " swell=" << swell;
            const float crest = metrics.peak / (metrics.rmsTotal + 1e-9f);
            EXPECT_LT(crest, 6.5f) << "Crest/resonance spike on " << name << " swell=" << swell;
        }

        proc.triggerPanic();
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < 10; ++b) {
            buf.clear();
            proc.processBlock(buf, midi);
        }
        const float postPanicPeak = std::max(buf.getMagnitude(0, 0, 512), buf.getMagnitude(1, 0, 512));
        EXPECT_LT(postPanicPeak, 1e-3f) << "Audio did not decay after panic on " << name;

        pm.loadPresetByIndex(static_cast<int>(idx));
        proc.syncParametersFromAPVTS();
        const auto expectedParams = proc.getCurrentParameters();

        juce::MemoryBlock state;
        proc.getStateInformation(state);
        ASSERT_GT(state.getSize(), 0u);

        nova::synth::NovaSynthProcessor procRestored;
        procRestored.prepareToPlay(48000.0, 512);
        procRestored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        procRestored.syncParametersFromAPVTS();
        const auto restoredParams = procRestored.getCurrentParameters();

        EXPECT_EQ(restoredParams.filterType, expectedParams.filterType) << name;
        EXPECT_NEAR(restoredParams.filterCutoffHz, expectedParams.filterCutoffHz, 1.0f) << name;
        EXPECT_NEAR(restoredParams.filterKeyTrack, expectedParams.filterKeyTrack, 1e-3f) << name;
        EXPECT_NEAR(restoredParams.analogDrift, expectedParams.analogDrift, 1e-3f) << name;
        EXPECT_NEAR(restoredParams.oscSlots[0].fineCents, expectedParams.oscSlots[0].fineCents, 0.2f) << name;
        EXPECT_NEAR(restoredParams.macroAir, expectedParams.macroAir, 1e-3f) << name;
        EXPECT_EQ(restoredParams.chorusMode, expectedParams.chorusMode) << name;
        EXPECT_NEAR(restoredParams.reverbDecay, expectedParams.reverbDecay, 0.1f) << name;

        for (std::size_t m = 0; m < nova::dsp::ModMatrix::kMaxSlots; ++m) {
            const auto& a = proc.getModMatrix().getSlot(m);
            const auto& b = procRestored.getModMatrix().getSlot(m);
            EXPECT_EQ(a.enabled, b.enabled) << name << " modSlot=" << m;
            EXPECT_EQ(a.source, b.source) << name << " modSlot=" << m;
            EXPECT_EQ(a.destination, b.destination) << name << " modSlot=" << m;
            EXPECT_NEAR(a.amount, b.amount, 1e-4f) << name << " modSlot=" << m;
        }
    }
}

TEST(NovaSynthFactoryPresetsTests, SpectralBalanceAiryVsWarmAndControlledMidrange) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    auto& pm = proc.getPresetManager();

    auto analyzePreset = [&](const char* name) {
        EXPECT_TRUE(pm.loadPresetByName(name)) << name;
        return renderWorshipChordMetrics(proc, 0.65f, 128, 75);
    };

    const auto velvet = analyzePreset("Soft Velvet Cushion");
    const auto choir  = analyzePreset("Intimate Tape Choir");
    const auto airy   = analyzePreset("Airy Silk Cloud");
    const auto halo   = analyzePreset("Vocal Mist Halo");

    const float airRatioVelvet = velvet.rmsAir / (velvet.rmsTotal + 1e-9f);
    const float airRatioChoir  = choir.rmsAir / (choir.rmsTotal + 1e-9f);
    const float airRatioAiry   = airy.rmsAir / (airy.rmsTotal + 1e-9f);
    const float airRatioHalo   = halo.rmsAir / (halo.rmsTotal + 1e-9f);

    EXPECT_GT(airRatioAiry, airRatioChoir * 2.5f);
    EXPECT_GT(airRatioHalo, airRatioChoir * 2.5f);
    EXPECT_GT(airRatioAiry, airRatioVelvet * 1.5f);
    EXPECT_GT(airRatioHalo, airRatioVelvet * 1.5f);

    for (const auto& [label, m] : std::array<std::pair<const char*, SpectralMetrics>, 4>{{
             { "Soft Velvet Cushion", velvet },
             { "Intimate Tape Choir", choir },
             { "Airy Silk Cloud", airy },
             { "Vocal Mist Halo", halo } }}) {
        EXPECT_GT(m.rmsFundament, 0.005f) << label;
        EXPECT_LT(m.rmsHarshMid, m.rmsFundament)
            << "1-2.5 kHz harsh midrange must remain controlled below 80-250 Hz fundament on " << label;
    }
}

TEST(NovaSynthFactoryPresetsTests, SwellDynamicsOpenBrightnessAcrossAllEightPads) {
    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    auto& pm = proc.getPresetManager();

    int testedPads = 0;
    const auto& allPresets = pm.getAllPresets();
    for (std::size_t idx = 0; idx < allPresets.size(); ++idx) {
        const auto& preset = allPresets[idx];
        if (!preset.isFactory || preset.category == "Utility") continue;

        ++testedPads;
        pm.loadPresetByIndex(static_cast<int>(idx));
        const auto lowSwell = renderWorshipChordMetrics(proc, 0.10f, 128, 64);

        pm.loadPresetByIndex(static_cast<int>(idx));
        const auto highSwell = renderWorshipChordMetrics(proc, 1.00f, 128, 64);

        const float lowBrightRatio  = lowSwell.rmsHigh1200 / (lowSwell.rmsTotal + 1e-9f);
        const float highBrightRatio = highSwell.rmsHigh1200 / (highSwell.rmsTotal + 1e-9f);

        EXPECT_LE(highSwell.peak, 1.01f) << preset.name.toStdString();
        EXPECT_GT(highSwell.rmsHigh1200, lowSwell.rmsHigh1200 * 1.30f)
            << "Swell must open high-frequency energy on " << preset.name.toStdString();
        EXPECT_GT(highBrightRatio, lowBrightRatio * 1.08f)
            << "Swell must increase relative spectral brightness on " << preset.name.toStdString();
    }
    EXPECT_EQ(testedPads, 8);
}
