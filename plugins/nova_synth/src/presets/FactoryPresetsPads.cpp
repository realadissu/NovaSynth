#include "FactoryPresets.h"
#include "nova_plugin/PresetManager.h"

namespace nova::synth {

void registerFactoryPresetsPads(nova::plugin::PresetManager& presetManager_) {
    using nova::plugin::PresetEntry;

    // =========================================================================
    // 1. SHIMMER & CELESTIAL
    // =========================================================================

    // 1. Bethel Shimmer Cloud
    presetManager_.registerFactoryPreset(PresetEntry{
        "Bethel Shimmer Cloud", "Shimmer & Celestial",
        "Warm triangle foundation layered with evolving celestial wavetable, organic choir breath, and angel shimmer halo.",
        {"Worship", "Bethel", "Shimmer", "Cloud"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.25f}, {"osc1_detune", 0.35f}, {"osc1_scatter", 0.32f}, {"osc1_warp", 0.20f}, {"osc1_level", 0.72f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 1.0f}, {"osc2_model", 7.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.45f}, {"osc2_detune", 0.35f}, {"osc2_scatter", 0.30f}, {"osc2_warp", 0.35f}, {"osc2_level", 0.55f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 2.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.40f}, {"osc3_detune", 0.32f}, {"osc3_scatter", 0.30f}, {"osc3_spread", 0.65f}, {"osc3_level", 0.45f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 1.0f}, {"osc4_engine", 4.0f}, {"osc4_model", 0.0f}, {"osc4_octave", -1.0f}, {"osc4_shape", 0.05f}, {"osc4_detune", 0.0f}, {"osc4_lowestNoteOnly", 1.0f}, {"osc4_level", 0.48f},
         {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1100.0f}, {"filterResonance", 1.15f}, {"filterEnvAmount", 0.22f}, {"filterDrive", 0.12f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 160.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.30f},
         {"ampAttack", 0.75f}, {"ampDecay", 2.4f}, {"ampSustain", 0.88f}, {"ampRelease", 1.8f},
         {"fltAttack", 0.85f}, {"fltDecay", 2.2f}, {"fltSustain", 0.50f}, {"fltRelease", 1.7f},
         {"macroWarmth", 0.58f}, {"macroMotion", 0.50f}, {"macroShimmer", 0.65f}, {"macroAir", 0.45f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.24f}, {"delayFeedback", 0.35f},
         {"reverbMix", 0.50f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.6f}, {"reverbShimmer", 0.62f}, {"reverbShimmerPitch", 0.0f}, {"reverbDucking", 0.40f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 2. Angelic Celestial Harp & Choir
    presetManager_.registerFactoryPreset(PresetEntry{
        "Angelic Celestial Harp & Choir", "Shimmer & Celestial",
        "Acoustic harp pluck floating over an organic choir breath swell and dual-octave shimmer cloud.",
        {"Harp", "Celestial", "Choir", "Shimmer", "Ambient"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 1.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.60f}, {"osc1_detune", 0.20f}, {"osc1_scatter", 0.35f}, {"osc1_duration", 0.65f}, {"osc1_level", 0.98f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 2.0f}, {"osc2_model", 0.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.45f}, {"osc2_detune", 0.35f}, {"osc2_scatter", 0.32f}, {"osc2_spread", 0.65f}, {"osc2_ampEnvSource", 2.0f}, {"osc2_level", 0.68f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 1.0f}, {"osc3_model", 3.0f}, {"osc3_octave", 1.0f}, {"osc3_shape", 0.40f}, {"osc3_detune", 0.25f}, {"osc3_scatter", 0.28f}, {"osc3_warp", 0.30f}, {"osc3_level", 0.45f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 3.0f}, {"osc4_engine", 5.0f}, {"osc4_model", 2.0f}, {"osc4_octave", 0.0f}, {"osc4_shape", 0.65f}, {"osc4_detune", 0.35f}, {"osc4_scatter", 0.35f}, {"osc4_level", 0.16f},
         {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1450.0f}, {"filterResonance", 1.05f}, {"filterEnvAmount", 0.20f}, {"filterDrive", 0.10f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 180.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.25f},
         {"ampAttack", 0.02f}, {"ampDecay", 2.2f}, {"ampSustain", 0.70f}, {"ampRelease", 1.8f},
         {"fltAttack", 0.25f}, {"fltDecay", 2.0f}, {"fltSustain", 0.50f}, {"fltRelease", 1.7f},
         {"modAttack", 0.55f}, {"modDecay", 2.2f}, {"modSustain", 0.85f}, {"modRelease", 1.8f},
         {"macroWarmth", 0.52f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.70f}, {"macroAir", 0.48f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 2.0f}, {"delayMix", 0.26f}, {"delayFeedback", 0.38f},
         {"reverbMix", 0.52f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.8f}, {"reverbShimmer", 0.65f}, {"reverbShimmerPitch", 2.0f}, {"reverbDucking", 0.38f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 3. Ethereal Morning Dawn
    presetManager_.registerFactoryPreset(PresetEntry{
        "Ethereal Morning Dawn", "Shimmer & Celestial",
        "Airy wind and swell texture floating through a slow morphing worship harmonics shimmer wash.",
        {"Dawn", "Morning", "Choir", "Texture", "Ambient"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 2.0f}, {"osc1_model", 4.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.55f}, {"osc1_detune", 0.35f}, {"osc1_scatter", 0.35f}, {"osc1_spread", 0.70f}, {"osc1_level", 0.75f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 1.0f}, {"osc2_model", 0.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.60f}, {"osc2_detune", 0.30f}, {"osc2_scatter", 0.30f}, {"osc2_warp", 0.40f}, {"osc2_level", 0.55f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 3.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.10f}, {"osc3_detune", 0.20f}, {"osc3_scatter", 0.22f}, {"osc3_level", 0.50f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 1.0f}, {"osc4_engine", 4.0f}, {"osc4_model", 1.0f}, {"osc4_octave", -1.0f}, {"osc4_shape", 0.10f}, {"osc4_detune", 0.0f}, {"osc4_lowestNoteOnly", 1.0f}, {"osc4_level", 0.45f},
         {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 980.0f}, {"filterResonance", 0.95f}, {"filterEnvAmount", 0.24f}, {"filterDrive", 0.10f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.95f}, {"ampDecay", 2.6f}, {"ampSustain", 0.85f}, {"ampRelease", 2.0f},
         {"fltAttack", 1.10f}, {"fltDecay", 2.5f}, {"fltSustain", 0.55f}, {"fltRelease", 1.9f},
         {"macroWarmth", 0.55f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.72f}, {"macroAir", 0.50f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 2.0f}, {"delayMix", 0.26f}, {"delayFeedback", 0.38f},
         {"reverbMix", 0.52f}, {"reverbDry", 1.0f}, {"reverbDecay", 5.0f}, {"reverbShimmer", 0.68f}, {"reverbShimmerPitch", 2.0f}, {"reverbDucking", 0.38f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 4. Cathedral Prayer Drone (C)
    presetManager_.registerFactoryPreset(PresetEntry{
        "Cathedral Prayer Drone (C)", "Shimmer & Celestial",
        "Self-sustaining warm Root + Fifth + Octave worship bed in C with cello texture and deep sine sub.",
        {"Drone", "Prayer", "Infinite", "Pad"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 1.0f}, {"osc1_engine", 4.0f}, {"osc1_model", 0.0f}, {"osc1_octave", -2.0f}, {"osc1_shape", 0.20f}, {"osc1_detune", 0.0f}, {"osc1_lowestNoteOnly", 1.0f}, {"osc1_level", 0.75f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", -1.0f}, {"osc2_shape", 0.25f}, {"osc2_detune", 0.25f}, {"osc2_scatter", 0.28f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 1.0f}, {"osc3_model", 4.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.40f}, {"osc3_detune", 0.30f}, {"osc3_scatter", 0.30f}, {"osc3_warp", 0.35f}, {"osc3_level", 0.52f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 0.0f}, {"osc4_engine", 2.0f}, {"osc4_model", 1.0f}, {"osc4_octave", 0.0f}, {"osc4_shape", 0.45f}, {"osc4_detune", 0.30f}, {"osc4_scatter", 0.32f}, {"osc4_spread", 0.60f}, {"osc4_level", 0.42f},
         {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 720.0f}, {"filterResonance", 1.10f}, {"filterEnvAmount", 0.15f}, {"filterDrive", 0.15f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 0.0f}, {"filter2CutoffHz", 5000.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.30f},
         {"ampAttack", 1.00f}, {"ampDecay", 2.5f}, {"ampSustain", 0.90f}, {"ampRelease", 2.2f},
         {"macroWarmth", 0.65f}, {"macroMotion", 0.30f}, {"macroShimmer", 0.48f}, {"macroAir", 0.25f},
         {"delayEnabled", 0.0f},
         {"reverbMix", 0.52f}, {"reverbDry", 1.0f}, {"reverbDecay", 5.5f}, {"reverbShimmer", 0.48f}, {"reverbDucking", 0.35f},
         {"droneEnabled", 1.0f}, {"droneRootKey", 0.0f}, {"droneLevel", 0.52f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // =========================================================================
    // 2. WARM ANALOG PADS
    // =========================================================================

    // 5. Warm Juno 106 Pad
    presetManager_.registerFactoryPreset(PresetEntry{
        "Warm Juno 106 Pad", "Warm Analog Pads",
        "Rich analog saw and pulse layered with warm triangle sub and lush chorus drift in a silky lowpass space.",
        {"Worship", "Warm", "Juno", "Pad", "Analog"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.15f}, {"osc1_detune", 0.35f}, {"osc1_scatter", 0.35f}, {"osc1_warp", 0.20f}, {"osc1_level", 0.80f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.50f}, {"osc2_detune", 0.38f}, {"osc2_scatter", 0.38f}, {"osc2_warp", 0.28f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 1.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.10f}, {"osc3_detune", 0.0f}, {"osc3_lowestNoteOnly", 1.0f}, {"osc3_level", 0.55f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 820.0f}, {"filterResonance", 1.45f}, {"filterEnvAmount", 0.32f}, {"filterDrive", 0.22f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 110.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.20f},
         {"ampAttack", 0.55f}, {"ampDecay", 2.0f}, {"ampSustain", 0.85f}, {"ampRelease", 1.7f},
         {"fltAttack", 0.65f}, {"fltDecay", 1.8f}, {"fltSustain", 0.45f}, {"fltRelease", 1.6f},
         {"macroWarmth", 0.68f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.12f}, {"macroAir", 0.25f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.18f}, {"delayFeedback", 0.30f},
         {"reverbMix", 0.35f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.8f}, {"reverbShimmer", 0.10f}, {"reverbDucking", 0.38f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 6. Elevation Velvet Brass Swell
    presetManager_.registerFactoryPreset(PresetEntry{
        "Elevation Velvet Brass Swell", "Warm Analog Pads",
        "Expressive vintage poly-brass filter swell with analog voice drift and warm saturation.",
        {"Brass", "Swell", "Analog", "Elevation"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.20f}, {"osc1_detune", 0.42f}, {"osc1_scatter", 0.38f}, {"osc1_warp", 0.25f}, {"osc1_level", 0.82f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 4.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.30f}, {"osc2_detune", 0.45f}, {"osc2_scatter", 0.40f}, {"osc2_warp", 0.30f}, {"osc2_level", 0.68f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 1.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.45f}, {"osc3_detune", 0.22f}, {"osc3_scatter", 0.22f}, {"osc3_level", 0.50f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 520.0f}, {"filterResonance", 1.60f}, {"filterEnvAmount", 0.60f}, {"filterDrive", 0.32f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 0.0f}, {"filter2CutoffHz", 4800.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.20f},
         {"ampAttack", 0.45f}, {"ampDecay", 2.0f}, {"ampSustain", 0.88f}, {"ampRelease", 1.7f},
         {"fltAttack", 0.85f}, {"fltDecay", 2.2f}, {"fltSustain", 0.45f}, {"fltRelease", 1.6f},
         {"macroWarmth", 0.65f}, {"macroMotion", 0.40f}, {"macroShimmer", 0.22f}, {"macroAir", 0.30f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.20f}, {"delayFeedback", 0.32f},
         {"reverbMix", 0.36f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.0f}, {"reverbShimmer", 0.20f}, {"reverbDucking", 0.38f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 7. Vintage String Ensemble
    presetManager_.registerFactoryPreset(PresetEntry{
        "Vintage String Ensemble", "Warm Analog Pads",
        "Lush 70s analog string machine layer with silky high end, slow gentle attack, and wide ensemble width.",
        {"Strings", "Ensemble", "Vintage", "Solina", "Analog"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.20f}, {"osc1_detune", 0.52f}, {"osc1_scatter", 0.42f}, {"osc1_warp", 0.28f}, {"osc1_level", 0.78f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 1.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.40f}, {"osc2_detune", 0.42f}, {"osc2_scatter", 0.35f}, {"osc2_warp", 0.32f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 2.0f}, {"osc3_model", 1.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.45f}, {"osc3_detune", 0.32f}, {"osc3_scatter", 0.30f}, {"osc3_spread", 0.65f}, {"osc3_level", 0.50f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1250.0f}, {"filterResonance", 0.90f}, {"filterEnvAmount", 0.25f}, {"filterDrive", 0.18f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 140.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.20f},
         {"ampAttack", 0.65f}, {"ampDecay", 2.4f}, {"ampSustain", 0.90f}, {"ampRelease", 1.8f},
         {"fltAttack", 0.75f}, {"fltDecay", 2.2f}, {"fltSustain", 0.60f}, {"fltRelease", 1.7f},
         {"macroWarmth", 0.65f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.15f}, {"macroAir", 0.30f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.18f}, {"delayFeedback", 0.30f},
         {"reverbMix", 0.38f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.2f}, {"reverbShimmer", 0.15f}, {"reverbDucking", 0.38f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 8. Cinematic Dark Swell
    presetManager_.registerFactoryPreset(PresetEntry{
        "Cinematic Dark Swell", "Warm Analog Pads",
        "Deep, menacing cinematic swell with heavy sub-bass weight, slow sweeping filter, and cavernous space.",
        {"Cinematic", "Dark", "Swell", "Sub", "Heavy"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", -1.0f}, {"osc1_shape", 0.25f}, {"osc1_detune", 0.38f}, {"osc1_scatter", 0.35f}, {"osc1_warp", 0.25f}, {"osc1_level", 0.82f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 1.0f}, {"osc2_model", 6.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.55f}, {"osc2_detune", 0.30f}, {"osc2_scatter", 0.30f}, {"osc2_warp", 0.35f}, {"osc2_level", 0.62f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 2.0f}, {"osc3_octave", -2.0f}, {"osc3_shape", 0.25f}, {"osc3_detune", 0.0f}, {"osc3_lowestNoteOnly", 1.0f}, {"osc3_level", 0.75f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 460.0f}, {"filterResonance", 1.55f}, {"filterEnvAmount", 0.50f}, {"filterDrive", 0.38f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.90f}, {"ampDecay", 2.5f}, {"ampSustain", 0.85f}, {"ampRelease", 2.0f},
         {"fltAttack", 1.15f}, {"fltDecay", 2.4f}, {"fltSustain", 0.45f}, {"fltRelease", 1.9f},
         {"macroWarmth", 0.70f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.20f}, {"macroAir", 0.25f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.24f}, {"delayFeedback", 0.36f},
         {"reverbMix", 0.48f}, {"reverbDry", 1.0f}, {"reverbDecay", 5.0f}, {"reverbShimmer", 0.20f}, {"reverbDucking", 0.38f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

}

} // namespace nova::synth
