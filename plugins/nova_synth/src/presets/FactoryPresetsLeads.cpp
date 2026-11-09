#include "FactoryPresets.h"
#include "nova_plugin/PresetManager.h"

namespace nova::synth {

void registerFactoryPresetsLeads(nova::plugin::PresetManager& presetManager_) {
    using nova::plugin::PresetEntry;

    // =========================================================================
    // 5. ARPS & SEQUENCES
    // =========================================================================

    // 21. Sunday Morning Arp Pulse
    presetManager_.registerFactoryPreset(PresetEntry{
        "Sunday Morning Arp Pulse", "Arps & Sequences",
        "Driving 1/16 rhythmic worship arpeggiator with physical mallet bite and tight low end.",
        {"Arp", "Pulse", "Pluck", "Motion", "Rhythm"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.65f}, {"osc1_detune", 0.18f}, {"osc1_scatter", 0.28f}, {"osc1_duration", 0.48f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.35f}, {"osc2_detune", 0.25f}, {"osc2_scatter", 0.25f}, {"osc2_level", 0.55f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 3.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.10f}, {"osc3_lowestNoteOnly", 1.0f}, {"osc3_level", 0.50f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 2600.0f}, {"filterResonance", 1.10f}, {"filterEnvAmount", 0.52f}, {"filterDrive", 0.20f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 0.0f}, {"filter2CutoffHz", 8000.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.25f},
         {"ampAttack", 0.003f}, {"ampDecay", 0.45f}, {"ampSustain", 0.25f}, {"ampRelease", 0.75f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.38f}, {"fltSustain", 0.15f}, {"fltRelease", 0.60f},
         {"macroWarmth", 0.48f}, {"macroMotion", 0.50f}, {"macroShimmer", 0.35f}, {"macroAir", 0.42f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.34f}, {"delayFeedback", 0.42f},
         {"reverbMix", 0.34f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.8f}, {"reverbShimmer", 0.30f}, {"reverbDucking", 0.38f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 1.0f}, {"arpSync", 1.0f}, {"arpSyncDiv", 4.0f}, {"arpMode", 2.0f}, {"arpGate", 0.58f}, {"subLowestOnly", 1.0f}}
    });

    // 22. Celestial 1/8d Halo Arp
    presetManager_.registerFactoryPreset(PresetEntry{
        "Celestial 1/8d Halo Arp", "Arps & Sequences",
        "Flowing dotted-8th worship arpeggio swimming through an angelic shimmer reverb cloud.",
        {"Celestial", "Halo", "Arp", "Dotted", "Worship"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 1.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.55f}, {"osc1_detune", 0.25f}, {"osc1_scatter", 0.28f}, {"osc1_warp", 0.32f}, {"osc1_level", 0.80f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 2.0f}, {"osc2_engine", 3.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.75f}, {"osc2_detune", 0.18f}, {"osc2_scatter", 0.28f}, {"osc2_duration", 0.52f}, {"osc2_level", 0.60f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.15f}, {"osc3_scatter", 0.20f}, {"osc3_level", 0.45f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 2400.0f}, {"filterResonance", 1.10f}, {"filterEnvAmount", 0.45f}, {"filterDrive", 0.18f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 200.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.25f},
         {"ampAttack", 0.003f}, {"ampDecay", 0.60f}, {"ampSustain", 0.20f}, {"ampRelease", 0.85f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.45f}, {"fltSustain", 0.15f}, {"fltRelease", 0.65f},
         {"macroWarmth", 0.48f}, {"macroMotion", 0.55f}, {"macroShimmer", 0.65f}, {"macroAir", 0.50f},
         {"arpEnabled", 1.0f}, {"arpSync", 1.0f}, {"arpSyncDiv", 2.0f}, {"arpMode", 0.0f}, {"arpGate", 0.60f}, {"arpOctaves", 1.0f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.34f}, {"delayFeedback", 0.44f},
         {"reverbMix", 0.44f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.5f}, {"reverbShimmer", 0.52f}, {"reverbShimmerPitch", 0.0f}, {"reverbDucking", 0.38f},
         {"droneEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 23. 1/8 Triplet Ambient Sequence
    presetManager_.registerFactoryPreset(PresetEntry{
        "1/8 Triplet Ambient Sequence", "Arps & Sequences",
        "Dreamy 1/8 triplet polyphonic sequence with soft analog filter sweeps and lush stereo space.",
        {"Triplet", "Ambient", "Sequence", "Fluid", "Space"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.25f}, {"osc1_detune", 0.35f}, {"osc1_scatter", 0.32f}, {"osc1_level", 0.75f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.30f}, {"osc2_detune", 0.15f}, {"osc2_scatter", 0.20f}, {"osc2_level", 0.60f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 2.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.50f}, {"osc3_detune", 0.30f}, {"osc3_scatter", 0.30f}, {"osc3_spread", 0.60f}, {"osc3_level", 0.40f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1950.0f}, {"filterResonance", 1.05f}, {"filterEnvAmount", 0.40f}, {"filterDrive", 0.20f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.005f}, {"ampDecay", 0.75f}, {"ampSustain", 0.25f}, {"ampRelease", 1.0f},
         {"fltAttack", 0.004f}, {"fltDecay", 0.55f}, {"fltSustain", 0.20f}, {"fltRelease", 0.85f},
         {"macroWarmth", 0.55f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.35f}, {"macroAir", 0.38f},
         {"arpEnabled", 1.0f}, {"arpSync", 1.0f}, {"arpSyncDiv", 3.0f}, {"arpMode", 2.0f}, {"arpGate", 0.70f}, {"arpOctaves", 0.0f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 3.0f}, {"delayMix", 0.26f}, {"delayFeedback", 0.38f},
         {"reverbMix", 0.38f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.0f}, {"reverbShimmer", 0.28f}, {"reverbDucking", 0.35f},
         {"droneEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 24. Dark Cyberwave Pulse
    presetManager_.registerFactoryPreset(PresetEntry{
        "Dark Cyberwave Pulse", "Arps & Sequences",
        "Aggressive 1/16 driving synthwave bassline/lead sequence with high filter drive and punchy saturation.",
        {"Cyberwave", "Synthwave", "Pulse", "Dark", "Driving"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.20f}, {"osc1_detune", 0.45f}, {"osc1_scatter", 0.38f}, {"osc1_warp", 0.30f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", -1.0f}, {"osc2_shape", 0.60f}, {"osc2_detune", 0.30f}, {"osc2_scatter", 0.28f}, {"osc2_level", 0.70f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 0.0f}, {"osc3_octave", -2.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.0f}, {"osc3_lowestNoteOnly", 1.0f}, {"osc3_level", 0.65f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1650.0f}, {"filterResonance", 2.10f}, {"filterEnvAmount", 0.65f}, {"filterDrive", 0.45f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.002f}, {"ampDecay", 0.40f}, {"ampSustain", 0.15f}, {"ampRelease", 0.25f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.28f}, {"fltSustain", 0.10f}, {"fltRelease", 0.20f},
         {"macroWarmth", 0.65f}, {"macroMotion", 0.50f}, {"macroShimmer", 0.10f}, {"macroAir", 0.30f},
         {"arpEnabled", 1.0f}, {"arpSync", 1.0f}, {"arpSyncDiv", 4.0f}, {"arpMode", 0.0f}, {"arpGate", 0.45f}, {"arpOctaves", 1.0f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.25f}, {"delayFeedback", 0.38f},
         {"reverbMix", 0.20f}, {"reverbDry", 1.0f}, {"reverbDecay", 2.5f}, {"reverbShimmer", 0.0f},
         {"droneEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 25. Euphoric Poly-Arp 1/16
    presetManager_.registerFactoryPreset(PresetEntry{
        "Euphoric Poly-Arp 1/16", "Arps & Sequences",
        "Uplifting, soaring festival/anthem arpeggiator with bright detuned supersaws and energetic rhythmic drive.",
        {"Euphoric", "Festival", "Trance", "Arp", "Anthem"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.30f}, {"osc1_detune", 0.65f}, {"osc1_scatter", 0.48f}, {"osc1_warp", 0.32f}, {"osc1_level", 0.82f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 0.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.20f}, {"osc2_detune", 0.55f}, {"osc2_scatter", 0.42f}, {"osc2_level", 0.62f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 0.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.10f}, {"osc3_detune", 0.0f}, {"osc3_lowestNoteOnly", 1.0f}, {"osc3_level", 0.52f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 3200.0f}, {"filterResonance", 1.25f}, {"filterEnvAmount", 0.58f}, {"filterDrive", 0.22f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.003f}, {"ampDecay", 0.45f}, {"ampSustain", 0.25f}, {"ampRelease", 0.35f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.32f}, {"fltSustain", 0.18f}, {"fltRelease", 0.28f},
         {"macroWarmth", 0.45f}, {"macroMotion", 0.60f}, {"macroShimmer", 0.28f}, {"macroAir", 0.48f},
         {"arpEnabled", 1.0f}, {"arpSync", 1.0f}, {"arpSyncDiv", 4.0f}, {"arpMode", 2.0f}, {"arpGate", 0.65f}, {"arpOctaves", 2.0f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.32f}, {"delayFeedback", 0.40f},
         {"reverbMix", 0.34f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.6f}, {"reverbShimmer", 0.22f}, {"reverbDucking", 0.35f},
         {"droneEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 26. Clockwork Mallet 1/32
    presetManager_.registerFactoryPreset(PresetEntry{
        "Clockwork Mallet 1/32", "Arps & Sequences",
        "Rapid, intricate 1/32 staccato clockwork sequence with crisp physical mallet transients.",
        {"Clockwork", "Micro-Arp", "Staccato", "Intricate", "Fast"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.70f}, {"osc1_detune", 0.15f}, {"osc1_scatter", 0.25f}, {"osc1_duration", 0.40f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.20f}, {"osc2_detune", 0.12f}, {"osc2_scatter", 0.18f}, {"osc2_level", 0.55f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 3.0f}, {"osc3_engine", 5.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.35f}, {"osc3_detune", 0.20f}, {"osc3_duration", 0.22f}, {"osc3_level", 0.20f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 2800.0f}, {"filterResonance", 1.15f}, {"filterEnvAmount", 0.52f}, {"filterDrive", 0.18f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.002f}, {"ampDecay", 0.35f}, {"ampSustain", 0.10f}, {"ampRelease", 0.20f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.25f}, {"fltSustain", 0.08f}, {"fltRelease", 0.15f},
         {"macroWarmth", 0.48f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.20f}, {"macroAir", 0.40f},
         {"arpEnabled", 1.0f}, {"arpSync", 1.0f}, {"arpSyncDiv", 6.0f}, {"arpMode", 4.0f}, {"arpGate", 0.40f}, {"arpOctaves", 1.0f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.28f}, {"delayFeedback", 0.38f},
         {"reverbMix", 0.30f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.0f}, {"reverbShimmer", 0.15f},
         {"droneEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // =========================================================================
    // 6. LEADS & POLY-SYNTHS
    // =========================================================================

    // 27. Majestic Praise Poly-Lead
    presetManager_.registerFactoryPreset(PresetEntry{
        "Majestic Praise Poly-Lead", "Leads & Poly-Synths",
        "Aggressive hard sync and supersaw hybrid cutting cleanly through heavy drums & bass.",
        {"Lead", "Praise", "SuperSaw", "Sync", "Anthem"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 5.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.45f}, {"osc1_detune", 0.35f}, {"osc1_scatter", 0.35f}, {"osc1_warp", 0.30f}, {"osc1_level", 0.82f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 4.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.30f}, {"osc2_detune", 0.45f}, {"osc2_scatter", 0.40f}, {"osc2_level", 0.68f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 3.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.25f}, {"osc3_detune", 0.10f}, {"osc3_lowestNoteOnly", 1.0f}, {"osc3_level", 0.52f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 3600.0f}, {"filterResonance", 1.45f}, {"filterEnvAmount", 0.48f}, {"filterDrive", 0.32f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 0.0f}, {"filter2CutoffHz", 8500.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.25f},
         {"ampAttack", 0.005f}, {"ampDecay", 1.2f}, {"ampSustain", 0.75f}, {"ampRelease", 1.3f},
         {"macroWarmth", 0.45f}, {"macroMotion", 0.38f}, {"macroShimmer", 0.38f}, {"macroAir", 0.50f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.30f}, {"delayFeedback", 0.40f},
         {"reverbMix", 0.36f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.2f}, {"reverbShimmer", 0.32f}, {"reverbDucking", 0.35f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 28. Vocal Formant Solo Lead
    presetManager_.registerFactoryPreset(PresetEntry{
        "Vocal Formant Solo Lead", "Leads & Poly-Synths",
        "Singing vocal lead using Filter 2 Vocal Carve formant shaping, smooth body, and wide tape delay.",
        {"Vocal", "Formant", "Singing", "Lead", "Expressive"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.25f}, {"osc1_detune", 0.30f}, {"osc1_scatter", 0.28f}, {"osc1_filterRouting", 2.0f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.40f}, {"osc2_detune", 0.25f}, {"osc2_scatter", 0.25f}, {"osc2_filterRouting", 2.0f}, {"osc2_level", 0.60f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 0.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.15f}, {"osc3_detune", 0.0f}, {"osc3_lowestNoteOnly", 1.0f}, {"osc3_level", 0.45f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 2200.0f}, {"filterResonance", 1.15f}, {"filterEnvAmount", 0.40f}, {"filterDrive", 0.22f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 1.0f}, {"filter2CutoffHz", 1450.0f}, {"filter2Resonance", 1.80f}, {"vocalCarve", 0.60f},
         {"ampAttack", 0.015f}, {"ampDecay", 1.5f}, {"ampSustain", 0.75f}, {"ampRelease", 0.80f},
         {"fltAttack", 0.020f}, {"fltDecay", 1.2f}, {"fltSustain", 0.60f}, {"fltRelease", 0.70f},
         {"macroWarmth", 0.52f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.35f}, {"macroAir", 0.45f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.32f}, {"delayFeedback", 0.42f},
         {"reverbMix", 0.32f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.6f}, {"reverbShimmer", 0.22f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 29. Analog Velvet Flute Lead
    presetManager_.registerFactoryPreset(PresetEntry{
        "Analog Velvet Flute Lead", "Leads & Poly-Synths",
        "Mellow, organic singing flute lead with soft vibrato drift, breath transient, and warm vintage saturation.",
        {"Flute", "Woodwind", "Velvet", "Lead", "Organic"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 1.0f}, {"osc1_shape", 0.35f}, {"osc1_detune", 0.15f}, {"osc1_scatter", 0.18f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 3.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.10f}, {"osc2_detune", 0.10f}, {"osc2_scatter", 0.12f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 3.0f}, {"osc3_engine", 5.0f}, {"osc3_model", 2.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.45f}, {"osc3_detune", 0.25f}, {"osc3_scatter", 0.30f}, {"osc3_level", 0.22f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1950.0f}, {"filterResonance", 1.00f}, {"filterEnvAmount", 0.25f}, {"filterDrive", 0.18f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.035f}, {"ampDecay", 1.8f}, {"ampSustain", 0.80f}, {"ampRelease", 0.65f},
         {"fltAttack", 0.040f}, {"fltDecay", 1.5f}, {"fltSustain", 0.65f}, {"fltRelease", 0.55f},
         {"macroWarmth", 0.65f}, {"macroMotion", 0.35f}, {"macroShimmer", 0.18f}, {"macroAir", 0.48f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.25f}, {"delayFeedback", 0.32f},
         {"reverbMix", 0.34f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.8f}, {"reverbShimmer", 0.18f}, {"reverbDucking", 0.32f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 30. 70s Moog Singing Lead
    presetManager_.registerFactoryPreset(PresetEntry{
        "70s Moog Singing Lead", "Leads & Poly-Synths",
        "Classic 70s prog-rock and fusion singing saw lead, rich ladder filter bite and creamy harmonics.",
        {"Moog", "70s", "Prog", "Lead", "Singing"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.20f}, {"osc1_detune", 0.30f}, {"osc1_scatter", 0.28f}, {"osc1_warp", 0.22f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.35f}, {"osc2_detune", 0.25f}, {"osc2_scatter", 0.25f}, {"osc2_level", 0.70f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 0.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.15f}, {"osc3_detune", 0.15f}, {"osc3_scatter", 0.18f}, {"osc3_level", 0.50f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 2600.0f}, {"filterResonance", 1.85f}, {"filterEnvAmount", 0.50f}, {"filterDrive", 0.35f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.008f}, {"ampDecay", 1.4f}, {"ampSustain", 0.80f}, {"ampRelease", 0.50f},
         {"fltAttack", 0.008f}, {"fltDecay", 1.2f}, {"fltSustain", 0.65f}, {"fltRelease", 0.48f},
         {"macroWarmth", 0.68f}, {"macroMotion", 0.35f}, {"macroShimmer", 0.12f}, {"macroAir", 0.32f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 2.0f}, {"delayMix", 0.28f}, {"delayFeedback", 0.38f},
         {"reverbMix", 0.28f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.2f}, {"reverbShimmer", 0.12f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // =========================================================================
    // 7. KEYS & ELECTRIC PIANOS
    // =========================================================================

    // 31. Tape Lofi Worship Keys
    presetManager_.registerFactoryPreset(PresetEntry{
        "Tape Lofi Worship Keys", "Keys & Electric Pianos",
        "Warm vintage triangle tone with subtle vinyl dust texture and intimate room space.",
        {"Keys", "Lofi", "Tape", "Vinyl", "Vintage"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.30f}, {"osc1_detune", 0.15f}, {"osc1_scatter", 0.20f}, {"osc1_level", 0.80f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 3.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.20f}, {"osc2_detune", 0.12f}, {"osc2_scatter", 0.15f}, {"osc2_level", 0.50f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 3.0f}, {"osc3_engine", 5.0f}, {"osc3_model", 1.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.65f}, {"osc3_detune", 0.40f}, {"osc3_rate", 0.42f}, {"osc3_scatter", 0.48f}, {"osc3_spread", 0.65f}, {"osc3_level", 0.32f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1650.0f}, {"filterResonance", 0.85f}, {"filterEnvAmount", 0.35f}, {"filterDrive", 0.28f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 0.0f}, {"filter2CutoffHz", 5000.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.20f},
         {"ampAttack", 0.008f}, {"ampDecay", 1.4f}, {"ampSustain", 0.55f}, {"ampRelease", 1.3f},
         {"fltAttack", 0.006f}, {"fltDecay", 1.2f}, {"fltSustain", 0.45f}, {"fltRelease", 1.2f},
         {"macroWarmth", 0.70f}, {"macroMotion", 0.22f}, {"macroShimmer", 0.05f}, {"macroAir", 0.20f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.16f}, {"delayFeedback", 0.28f},
         {"reverbMix", 0.28f}, {"reverbDry", 1.0f}, {"reverbDecay", 2.8f}, {"reverbShimmer", 0.0f}, {"reverbDucking", 0.28f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 32. Worship DX Electric Piano
    presetManager_.registerFactoryPreset(PresetEntry{
        "Worship DX Electric Piano", "Keys & Electric Pianos",
        "Classic 80s FM electric piano bell tine, lush stereo chorus, and ambient hall space.",
        {"EPiano", "DX7", "FM", "Bell", "Worship"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 1.0f}, {"osc1_model", 3.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.65f}, {"osc1_detune", 0.20f}, {"osc1_scatter", 0.25f}, {"osc1_warp", 0.32f}, {"osc1_level", 0.82f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.20f}, {"osc2_detune", 0.15f}, {"osc2_scatter", 0.18f}, {"osc2_level", 0.62f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 3.0f}, {"osc3_octave", 1.0f}, {"osc3_shape", 0.10f}, {"osc3_detune", 0.10f}, {"osc3_scatter", 0.12f}, {"osc3_level", 0.42f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 3200.0f}, {"filterResonance", 0.85f}, {"filterEnvAmount", 0.40f}, {"filterDrive", 0.18f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.004f}, {"ampDecay", 1.6f}, {"ampSustain", 0.45f}, {"ampRelease", 1.5f},
         {"fltAttack", 0.003f}, {"fltDecay", 1.2f}, {"fltSustain", 0.35f}, {"fltRelease", 1.3f},
         {"macroWarmth", 0.55f}, {"macroMotion", 0.40f}, {"macroShimmer", 0.28f}, {"macroAir", 0.42f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.24f}, {"delayFeedback", 0.34f},
         {"reverbMix", 0.36f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.8f}, {"reverbShimmer", 0.22f}, {"reverbDucking", 0.35f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 33. Soft Felt Piano Layer
    presetManager_.registerFactoryPreset(PresetEntry{
        "Soft Felt Piano Layer", "Keys & Electric Pianos",
        "Muted felt hammer attack layered with a soft analog sustain and high celestial air shimmer.",
        {"Felt", "Piano", "Muted", "Shimmer", "Intimate"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.50f}, {"osc1_detune", 0.15f}, {"osc1_scatter", 0.22f}, {"osc1_duration", 0.58f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.25f}, {"osc2_detune", 0.12f}, {"osc2_scatter", 0.18f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 2.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 1.0f}, {"osc3_shape", 0.55f}, {"osc3_detune", 0.30f}, {"osc3_scatter", 0.30f}, {"osc3_spread", 0.65f}, {"osc3_ampEnvSource", 2.0f}, {"osc3_level", 0.40f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1750.0f}, {"filterResonance", 0.85f}, {"filterEnvAmount", 0.32f}, {"filterDrive", 0.15f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.006f}, {"ampDecay", 1.6f}, {"ampSustain", 0.55f}, {"ampRelease", 1.6f},
         {"fltAttack", 0.005f}, {"fltDecay", 1.4f}, {"fltSustain", 0.40f}, {"fltRelease", 1.5f},
         {"modAttack", 0.45f}, {"modDecay", 1.8f}, {"modSustain", 0.75f}, {"modRelease", 1.5f},
         {"macroWarmth", 0.65f}, {"macroMotion", 0.30f}, {"macroShimmer", 0.42f}, {"macroAir", 0.35f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.20f}, {"delayFeedback", 0.30f},
         {"reverbMix", 0.40f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.2f}, {"reverbShimmer", 0.38f}, {"reverbDucking", 0.38f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

}

} // namespace nova::synth
