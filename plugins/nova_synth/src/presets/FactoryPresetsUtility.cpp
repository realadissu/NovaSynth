#include "FactoryPresets.h"
#include "nova_plugin/PresetManager.h"
#include "nova_dsp/synth/WorshipSynthEngine.h"

namespace nova::synth {

void registerFactoryPresetsUtility(nova::plugin::PresetManager& presetManager_) {
    using nova::plugin::PresetEntry;
    using Src = nova::dsp::ModMatrix::Source;
    using Dst = nova::dsp::ModMatrix::Destination;
    auto ms = [](Src s, Dst d, float amt) noexcept -> nova::plugin::PresetModSlot {
        return { static_cast<int>(s), static_cast<int>(d), amt, true };
    };

    // 9. Init Patch
    presetManager_.registerFactoryPreset(PresetEntry{
        "Init Patch", "Utility",
        "Clean initialization patch with a single saw oscillator, open 12 dB lowpass filter, zero drift, and bypassed effects.",
        {"Init", "Utility", "Clean", "Template"},
        {{"activeOscCount", 1.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f},
         {"osc1_filterRouting", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_semi", 0.0f}, {"osc1_fine", 0.0f},
         {"osc1_shape", 0.0f}, {"osc1_detune", 0.0f}, {"osc1_scatter", 0.0f}, {"osc1_spread", 0.0f},
         {"osc1_pan", 0.0f}, {"osc1_lowestNoteOnly", 0.0f}, {"osc1_ampEnvSource", 0.0f}, {"osc1_level", 0.75f},
         {"osc2_enabled", 0.0f}, {"osc3_enabled", 0.0f}, {"osc4_enabled", 0.0f},
         {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 3.0f}, {"filterCutoffHz", 16000.0f}, {"filterResonance", 0.71f},
         {"filterEnvAmount", 0.0f}, {"filterKeyTrack", 0.0f}, {"filterDrive", 0.0f},
         {"filter2Enabled", 0.0f}, {"filter2Type", 0.0f}, {"filter2CutoffHz", 16000.0f}, {"filter2Resonance", 0.71f},
         {"filter2EnvAmount", 0.0f}, {"filter2KeyTrack", 0.0f}, {"filter2Drive", 0.0f},
         {"vocalCarve", 0.0f}, {"filterRoutingMode", 0.0f},
         {"analogDrift", 0.0f},
         {"ampAttack", 0.005f}, {"ampDecay", 1.0f}, {"ampSustain", 0.85f}, {"ampRelease", 0.18f},
         {"fltAttack", 0.01f}, {"fltDecay", 1.0f}, {"fltSustain", 0.50f}, {"fltRelease", 0.18f},
         {"lfo1Depth", 0.0f}, {"lfo2Depth", 0.0f}, {"lfo3Depth", 0.0f}, {"lfo4Depth", 0.0f},
         {"macroWarmth", 0.50f}, {"macroMotion", 0.0f}, {"macroShimmer", 0.0f}, {"macroAir", 0.30f},
         {"chorusEnabled", 0.0f}, {"chorusMode", 0.0f}, {"chorusMix", 0.0f},
         {"chorusRateHz", 0.50f}, {"chorusDepth", 0.50f}, {"chorusWarmth", 0.50f},
         {"delayEnabled", 0.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f},
         {"delayMix", 0.0f}, {"delayFeedback", 0.0f}, {"delaySwing", 0.0f},
         {"reverbDry", 1.0f}, {"reverbMix", 0.0f}, {"reverbDecay", 2.5f}, {"reverbPreDelay", 12.0f},
         {"reverbShimmer", 0.0f}, {"reverbShimmerPitch", 0.0f}, {"reverbDucking", 0.0f}, {"reverbFreeze", 0.0f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}, {"masterGainDb", 0.0f}},
        {ms(Src::ModWheel, Dst::FilterCutoff, 0.45f)}
    });
}

} // namespace nova::synth
