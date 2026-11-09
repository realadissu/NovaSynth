#include "FactoryPresets.h"
#include "nova_plugin/PresetManager.h"

namespace nova::synth {

void registerFactoryPresetsPlucks(nova::plugin::PresetManager& presetManager_) {
    using nova::plugin::PresetEntry;

    // =========================================================================
    // 3. BASSES & FOUNDATION
    // =========================================================================

    // 9. Moog Analog Punch Bass
    presetManager_.registerFactoryPreset(PresetEntry{
        "Moog Analog Punch Bass", "Basses & Foundation",
        "Classic 24dB warm ladder filter, driven sub + 2-voice analog saw, snappy envelope with rich low-end punch.",
        {"Moog", "Bass", "Analog", "Punch", "Warm"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.20f}, {"osc1_detune", 0.25f}, {"osc1_scatter", 0.25f}, {"osc1_warp", 0.20f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.40f}, {"osc2_detune", 0.20f}, {"osc2_scatter", 0.20f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 0.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.0f}, {"osc3_lowestNoteOnly", 1.0f}, {"osc3_level", 0.75f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 680.0f}, {"filterResonance", 1.65f}, {"filterEnvAmount", 0.65f}, {"filterDrive", 0.35f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.003f}, {"ampDecay", 0.55f}, {"ampSustain", 0.15f}, {"ampRelease", 0.20f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.38f}, {"fltSustain", 0.10f}, {"fltRelease", 0.18f},
         {"macroWarmth", 0.70f}, {"macroMotion", 0.20f}, {"macroShimmer", 0.0f}, {"macroAir", 0.15f},
         {"delayEnabled", 0.0f},
         {"reverbMix", 0.08f}, {"reverbDry", 1.0f}, {"reverbDecay", 1.2f}, {"reverbShimmer", 0.0f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 10. 808 Saturated Sub
    presetManager_.registerFactoryPreset(PresetEntry{
        "808 Saturated Sub", "Basses & Foundation",
        "Deep, earth-shaking 808 sub with warm 3rd harmonic saturation that cuts through small speakers.",
        {"808", "Sub", "Bass", "Trap", "Saturated"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 1.0f}, {"osc1_engine", 4.0f}, {"osc1_model", 2.0f}, {"osc1_octave", -1.0f}, {"osc1_shape", 0.45f}, {"osc1_detune", 0.0f}, {"osc1_lowestNoteOnly", 1.0f}, {"osc1_level", 0.95f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.30f}, {"osc2_detune", 0.0f}, {"osc2_scatter", 0.0f}, {"osc2_level", 0.50f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 3.0f}, {"osc3_engine", 5.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.40f}, {"osc3_detune", 0.0f}, {"osc3_duration", 0.18f}, {"osc3_level", 0.15f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 380.0f}, {"filterResonance", 0.80f}, {"filterEnvAmount", 0.25f}, {"filterDrive", 0.40f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.003f}, {"ampDecay", 1.4f}, {"ampSustain", 0.30f}, {"ampRelease", 0.40f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.55f}, {"fltSustain", 0.18f}, {"fltRelease", 0.28f},
         {"macroWarmth", 0.75f}, {"macroMotion", 0.10f}, {"macroShimmer", 0.0f}, {"macroAir", 0.10f},
         {"delayEnabled", 0.0f},
         {"reverbMix", 0.0f}, {"reverbDry", 1.0f}, {"reverbDecay", 0.8f}, {"reverbShimmer", 0.0f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 11. Reese Stereo Motion Bass
    presetManager_.registerFactoryPreset(PresetEntry{
        "Reese Stereo Motion Bass", "Basses & Foundation",
        "Classic rolling, detuned Reese bass with slow phase-cancellation movement and massive low end.",
        {"Reese", "DnB", "Bass", "Stereo", "Motion"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.20f}, {"osc1_detune", 0.52f}, {"osc1_scatter", 0.42f}, {"osc1_warp", 0.28f}, {"osc1_level", 0.82f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 0.0f}, {"osc2_octave", 0.0f}, {"osc2_semi", 0.0f}, {"osc2_shape", 0.35f}, {"osc2_detune", 0.62f}, {"osc2_scatter", 0.48f}, {"osc2_warp", 0.35f}, {"osc2_level", 0.78f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 0.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.10f}, {"osc3_detune", 0.0f}, {"osc3_lowestNoteOnly", 1.0f}, {"osc3_level", 0.75f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 950.0f}, {"filterResonance", 1.20f}, {"filterEnvAmount", 0.35f}, {"filterDrive", 0.35f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 0.0f}, {"filter2CutoffHz", 4800.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.25f},
         {"ampAttack", 0.015f}, {"ampDecay", 1.8f}, {"ampSustain", 0.85f}, {"ampRelease", 0.50f},
         {"fltAttack", 0.040f}, {"fltDecay", 2.0f}, {"fltSustain", 0.55f}, {"fltRelease", 0.50f},
         {"macroWarmth", 0.70f}, {"macroMotion", 0.55f}, {"macroShimmer", 0.05f}, {"macroAir", 0.20f},
         {"delayEnabled", 0.0f},
         {"reverbMix", 0.12f}, {"reverbDry", 1.0f}, {"reverbDecay", 1.8f}, {"reverbShimmer", 0.0f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 12. Slap FM Punch Bass
    presetManager_.registerFactoryPreset(PresetEntry{
        "Slap FM Punch Bass", "Basses & Foundation",
        "Snappy, metallic slap-bass transient with tight body, perfect for funk, pop, and rhythmic verses.",
        {"Slap", "Bass", "Funk", "FM", "Punch"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.80f}, {"osc1_detune", 0.15f}, {"osc1_scatter", 0.25f}, {"osc1_duration", 0.45f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.60f}, {"osc2_detune", 0.25f}, {"osc2_scatter", 0.22f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 1.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.25f}, {"osc3_detune", 0.0f}, {"osc3_lowestNoteOnly", 1.0f}, {"osc3_level", 0.70f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1650.0f}, {"filterResonance", 1.45f}, {"filterEnvAmount", 0.55f}, {"filterDrive", 0.28f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 3.0f}, {"filter2CutoffHz", 1600.0f}, {"filter2Resonance", 0.85f}, {"vocalCarve", 0.35f},
         {"ampAttack", 0.002f}, {"ampDecay", 0.48f}, {"ampSustain", 0.15f}, {"ampRelease", 0.18f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.32f}, {"fltSustain", 0.10f}, {"fltRelease", 0.18f},
         {"macroWarmth", 0.50f}, {"macroMotion", 0.30f}, {"macroShimmer", 0.05f}, {"macroAir", 0.35f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 6.0f}, {"delayMix", 0.15f}, {"delayFeedback", 0.25f},
         {"reverbMix", 0.10f}, {"reverbDry", 1.0f}, {"reverbDecay", 1.2f}, {"reverbShimmer", 0.0f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 13. Acid 303 Resonant Bass
    presetManager_.registerFactoryPreset(PresetEntry{
        "Acid 303 Resonant Bass", "Basses & Foundation",
        "Screaming 303 squelch with aggressive resonance, biting tube drive, and accent decay.",
        {"Acid", "303", "Squelch", "Resonance", "Drive"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.10f}, {"osc1_detune", 0.15f}, {"osc1_scatter", 0.18f}, {"osc1_level", 0.88f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.50f}, {"osc2_detune", 0.20f}, {"osc2_scatter", 0.20f}, {"osc2_level", 0.55f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 0.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.15f}, {"osc3_detune", 0.0f}, {"osc3_lowestNoteOnly", 1.0f}, {"osc3_level", 0.55f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1050.0f}, {"filterResonance", 2.80f}, {"filterEnvAmount", 0.75f}, {"filterDrive", 0.48f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.002f}, {"ampDecay", 0.38f}, {"ampSustain", 0.0f}, {"ampRelease", 0.14f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.26f}, {"fltSustain", 0.0f}, {"fltRelease", 0.12f},
         {"macroWarmth", 0.65f}, {"macroMotion", 0.35f}, {"macroShimmer", 0.0f}, {"macroAir", 0.25f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.24f}, {"delayFeedback", 0.40f},
         {"reverbMix", 0.15f}, {"reverbDry", 1.0f}, {"reverbDecay", 1.6f}, {"reverbShimmer", 0.0f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 14. Deep Hillsong Worship Sub & Pad
    presetManager_.registerFactoryPreset(PresetEntry{
        "Deep Hillsong Worship Sub & Pad", "Basses & Foundation",
        "Dedicated driven 808 sub bass anchor with high airy string pad floating over the mix.",
        {"Sub", "Bass", "Pad", "Foundation", "Worship"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 1.0f}, {"osc1_engine", 4.0f}, {"osc1_model", 2.0f}, {"osc1_octave", -1.0f}, {"osc1_shape", 0.45f}, {"osc1_detune", 0.0f}, {"osc1_lowestNoteOnly", 1.0f}, {"osc1_level", 0.90f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.25f}, {"osc2_detune", 0.25f}, {"osc2_scatter", 0.28f}, {"osc2_level", 0.60f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 1.0f}, {"osc3_model", 6.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.35f}, {"osc3_detune", 0.30f}, {"osc3_scatter", 0.30f}, {"osc3_warp", 0.30f}, {"osc3_level", 0.48f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 3.0f}, {"osc4_engine", 5.0f}, {"osc4_model", 2.0f}, {"osc4_octave", 0.0f}, {"osc4_shape", 0.70f}, {"osc4_detune", 0.50f}, {"osc4_scatter", 0.40f}, {"osc4_level", 0.22f},
         {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1050.0f}, {"filterResonance", 0.85f}, {"filterEnvAmount", 0.30f}, {"filterDrive", 0.28f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 0.0f}, {"filter2CutoffHz", 4500.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.25f},
         {"ampAttack", 0.48f}, {"ampDecay", 2.0f}, {"ampSustain", 0.85f}, {"ampRelease", 1.8f},
         {"macroWarmth", 0.65f}, {"macroMotion", 0.25f}, {"macroShimmer", 0.25f}, {"macroAir", 0.35f},
         {"delayEnabled", 0.0f},
         {"reverbMix", 0.36f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.2f}, {"reverbShimmer", 0.22f}, {"reverbDucking", 0.38f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // =========================================================================
    // 4. PLUCKS & MALLETS
    // =========================================================================

    // 15. Hillsong 1/8d Pluck Lead
    presetManager_.registerFactoryPreset(PresetEntry{
        "Hillsong 1/8d Pluck Lead", "Plucks & Mallets",
        "Punchy Karplus-Strong physical pluck + glass bell over dotted-8th Hillsong signature worship delay.",
        {"Pluck", "Lead", "Delay", "Hillsong", "Worship"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.72f}, {"osc1_detune", 0.18f}, {"osc1_scatter", 0.30f}, {"osc1_duration", 0.55f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 1.0f}, {"osc2_model", 5.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.60f}, {"osc2_detune", 0.22f}, {"osc2_scatter", 0.25f}, {"osc2_warp", 0.30f}, {"osc2_level", 0.50f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 2.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.10f}, {"osc3_scatter", 0.15f}, {"osc3_level", 0.40f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 1.0f}, {"filterCutoffHz", 2600.0f}, {"filterResonance", 1.25f}, {"filterEnvAmount", 0.55f}, {"filterDrive", 0.20f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 3.0f}, {"filter2CutoffHz", 2200.0f}, {"filter2Resonance", 0.85f}, {"vocalCarve", 0.35f},
         {"ampAttack", 0.003f}, {"ampDecay", 0.95f}, {"ampSustain", 0.25f}, {"ampRelease", 1.1f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.65f}, {"fltSustain", 0.20f}, {"fltRelease", 1.0f},
         {"macroWarmth", 0.48f}, {"macroMotion", 0.40f}, {"macroShimmer", 0.40f}, {"macroAir", 0.48f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.36f}, {"delayFeedback", 0.44f},
         {"reverbMix", 0.35f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.8f}, {"reverbShimmer", 0.30f}, {"reverbDucking", 0.35f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 16. Glass Kalimba / Bell
    presetManager_.registerFactoryPreset(PresetEntry{
        "Glass Kalimba / Bell", "Plucks & Mallets",
        "Crystalline, pure glass kalimba with delicate acoustic body and wide shimmer reverb halo.",
        {"Kalimba", "Bell", "Glass", "Shimmer", "Pluck"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 1.0f}, {"osc1_octave", 1.0f}, {"osc1_shape", 0.82f}, {"osc1_detune", 0.20f}, {"osc1_scatter", 0.32f}, {"osc1_duration", 0.60f}, {"osc1_level", 0.82f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 1.0f}, {"osc2_model", 3.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.70f}, {"osc2_detune", 0.25f}, {"osc2_scatter", 0.28f}, {"osc2_warp", 0.35f}, {"osc2_level", 0.58f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 3.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.15f}, {"osc3_detune", 0.10f}, {"osc3_scatter", 0.15f}, {"osc3_level", 0.42f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 3400.0f}, {"filterResonance", 0.95f}, {"filterEnvAmount", 0.40f}, {"filterDrive", 0.12f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 160.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.20f},
         {"ampAttack", 0.002f}, {"ampDecay", 1.2f}, {"ampSustain", 0.12f}, {"ampRelease", 1.4f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.80f}, {"fltSustain", 0.10f}, {"fltRelease", 1.2f},
         {"macroWarmth", 0.45f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.52f}, {"macroAir", 0.50f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.32f}, {"delayFeedback", 0.42f},
         {"reverbMix", 0.44f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.2f}, {"reverbShimmer", 0.42f}, {"reverbDucking", 0.35f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 17. Punchy Marimba Mallet
    presetManager_.registerFactoryPreset(PresetEntry{
        "Punchy Marimba Mallet", "Plucks & Mallets",
        "Natural wooden marimba bar strike with warm fundamental, snappy transient, and fast acoustic decay.",
        {"Marimba", "Mallet", "Wood", "Organic", "Acoustic"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.60f}, {"osc1_detune", 0.15f}, {"osc1_scatter", 0.28f}, {"osc1_duration", 0.42f}, {"osc1_level", 0.88f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.25f}, {"osc2_detune", 0.12f}, {"osc2_scatter", 0.15f}, {"osc2_level", 0.62f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 3.0f}, {"osc3_engine", 5.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.40f}, {"osc3_detune", 0.20f}, {"osc3_duration", 0.20f}, {"osc3_level", 0.22f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 2400.0f}, {"filterResonance", 1.20f}, {"filterEnvAmount", 0.50f}, {"filterDrive", 0.18f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.002f}, {"ampDecay", 0.60f}, {"ampSustain", 0.0f}, {"ampRelease", 0.28f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.42f}, {"fltSustain", 0.0f}, {"fltRelease", 0.22f},
         {"macroWarmth", 0.62f}, {"macroMotion", 0.25f}, {"macroShimmer", 0.08f}, {"macroAir", 0.30f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 3.0f}, {"delayMix", 0.18f}, {"delayFeedback", 0.28f},
         {"reverbMix", 0.22f}, {"reverbDry", 1.0f}, {"reverbDecay", 2.2f}, {"reverbShimmer", 0.0f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 18. Muted Acoustic Guitar Pluck
    presetManager_.registerFactoryPreset(PresetEntry{
        "Muted Acoustic Guitar Pluck", "Plucks & Mallets",
        "Muted acoustic steel string guitar with finger-pick warmth, fast damping, and subtle vintage tape room.",
        {"Guitar", "Acoustic", "Steel", "Muted", "Pluck"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.75f}, {"osc1_detune", 0.15f}, {"osc1_scatter", 0.28f}, {"osc1_duration", 0.38f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 0.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.30f}, {"osc2_detune", 0.10f}, {"osc2_scatter", 0.15f}, {"osc2_level", 0.45f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 3.0f}, {"osc3_engine", 5.0f}, {"osc3_model", 1.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.50f}, {"osc3_detune", 0.30f}, {"osc3_scatter", 0.35f}, {"osc3_level", 0.20f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1950.0f}, {"filterResonance", 1.05f}, {"filterEnvAmount", 0.45f}, {"filterDrive", 0.18f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.003f}, {"ampDecay", 0.50f}, {"ampSustain", 0.05f}, {"ampRelease", 0.22f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.35f}, {"fltSustain", 0.02f}, {"fltRelease", 0.18f},
         {"macroWarmth", 0.60f}, {"macroMotion", 0.22f}, {"macroShimmer", 0.05f}, {"macroAir", 0.32f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.20f}, {"delayFeedback", 0.28f},
         {"reverbMix", 0.20f}, {"reverbDry", 1.0f}, {"reverbDecay", 1.8f}, {"reverbShimmer", 0.0f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 19. Neo-Soul Velvet Tine
    presetManager_.registerFactoryPreset(PresetEntry{
        "Neo-Soul Velvet Tine", "Plucks & Mallets",
        "Warm vintage tine pluck with velvety harmonic body, perfect for chords and neo-soul progressions.",
        {"Tine", "Soul", "Neo-Soul", "Velvet", "Warm"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.40f}, {"osc1_detune", 0.20f}, {"osc1_scatter", 0.22f}, {"osc1_level", 0.80f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 2.0f}, {"osc2_engine", 3.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.50f}, {"osc2_detune", 0.15f}, {"osc2_scatter", 0.25f}, {"osc2_duration", 0.55f}, {"osc2_level", 0.50f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 1.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.30f}, {"osc3_detune", 0.15f}, {"osc3_scatter", 0.20f}, {"osc3_level", 0.45f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1750.0f}, {"filterResonance", 0.90f}, {"filterEnvAmount", 0.45f}, {"filterDrive", 0.22f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.004f}, {"ampDecay", 1.0f}, {"ampSustain", 0.25f}, {"ampRelease", 0.70f},
         {"fltAttack", 0.003f}, {"fltDecay", 0.75f}, {"fltSustain", 0.18f}, {"fltRelease", 0.60f},
         {"macroWarmth", 0.70f}, {"macroMotion", 0.35f}, {"macroShimmer", 0.12f}, {"macroAir", 0.28f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 2.0f}, {"delayMix", 0.22f}, {"delayFeedback", 0.32f},
         {"reverbMix", 0.30f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.0f}, {"reverbShimmer", 0.15f}, {"reverbDucking", 0.32f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 20. EDM Staccato Pluck
    presetManager_.registerFactoryPreset(PresetEntry{
        "EDM Staccato Pluck", "Plucks & Mallets",
        "Ultra-tight, punchy supersaw pluck with razor-sharp attack, ready for club and pop anthems.",
        {"EDM", "Pluck", "Staccato", "SuperSaw", "Bright"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.25f}, {"osc1_detune", 0.60f}, {"osc1_scatter", 0.45f}, {"osc1_warp", 0.30f}, {"osc1_level", 0.82f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.45f}, {"osc2_detune", 0.50f}, {"osc2_scatter", 0.40f}, {"osc2_level", 0.62f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 0.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.15f}, {"osc3_detune", 0.0f}, {"osc3_lowestNoteOnly", 1.0f}, {"osc3_level", 0.52f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 3100.0f}, {"filterResonance", 1.35f}, {"filterEnvAmount", 0.70f}, {"filterDrive", 0.30f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.002f}, {"ampDecay", 0.32f}, {"ampSustain", 0.0f}, {"ampRelease", 0.15f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.20f}, {"fltSustain", 0.0f}, {"fltRelease", 0.12f},
         {"macroWarmth", 0.45f}, {"macroMotion", 0.40f}, {"macroShimmer", 0.18f}, {"macroAir", 0.45f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.32f}, {"delayFeedback", 0.40f},
         {"reverbMix", 0.28f}, {"reverbDry", 1.0f}, {"reverbDecay", 2.8f}, {"reverbShimmer", 0.18f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

}

} // namespace nova::synth
