#include "FactoryPresets.h"
#include "nova_plugin/PresetManager.h"

namespace nova::synth {

void registerFactoryPresetsAcoustic(nova::plugin::PresetManager& presetManager_) {
    using nova::plugin::PresetEntry;

    // =========================================================================
    // 8. UTILITY & TEMPLATES
    // =========================================================================

    // 34. Init Patch
    presetManager_.registerFactoryPreset(PresetEntry{
        "Init Patch", "Utility",
        "Clean initialization patch with a single pure analog oscillator and zero effects for sound design from scratch.",
        {"Init", "Utility", "Clean", "Default"},
        {{"activeOscCount", 1.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.0f}, {"osc1_detune", 0.0f}, {"osc1_level", 0.75f},
         {"osc2_enabled", 0.0f}, {"osc3_enabled", 0.0f}, {"osc4_enabled", 0.0f},
         {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 18000.0f}, {"filterResonance", 0.0f}, {"filterEnvAmount", 0.0f}, {"filterDrive", 0.0f},
         {"filter2Enabled", 0.0f}, {"filter2Type", 0.0f}, {"filter2CutoffHz", 18000.0f}, {"filter2Resonance", 0.0f}, {"vocalCarve", 0.0f},
         {"ampAttack", 0.005f}, {"ampDecay", 1.0f}, {"ampSustain", 0.80f}, {"ampRelease", 0.15f},
         {"fltAttack", 0.01f}, {"fltDecay", 1.0f}, {"fltSustain", 0.40f}, {"fltRelease", 0.15f},
         {"macroWarmth", 0.50f}, {"macroMotion", 0.0f}, {"macroShimmer", 0.0f}, {"macroAir", 0.0f},
         {"delayEnabled", 0.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.0f}, {"delayFeedback", 0.0f},
         {"reverbMix", 0.0f}, {"reverbDry", 1.0f}, {"reverbDecay", 2.5f}, {"reverbShimmer", 0.0f}, {"reverbShimmerPitch", 0.0f}, {"reverbDucking", 0.0f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // =========================================================================
    // 9. ACOUSTIC HYBRID & SAMPLE LAYERS
    // =========================================================================

    // 35. Nils Felt Piano & Warm Pad
    presetManager_.registerFactoryPreset(PresetEntry{
        "Nils Felt Piano & Warm Pad", "Hybrid",
        "Signature intimate felt piano hammer attack paired with acoustic Steinway resonance and warm analog pad.",
        {"Piano", "Felt", "Hybrid", "Acoustic", "Warm"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.05f}, {"osc1_detune", 0.10f}, {"osc1_scatter", 0.0f}, {"osc1_loopMode", 0.0f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 4.0f}, {"osc2_engine", 6.0f}, {"osc2_model", 0.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.0f}, {"osc2_detune", 0.12f}, {"osc2_scatter", 0.0f}, {"osc2_loopMode", 0.0f}, {"osc2_level", 0.88f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 3.0f}, {"osc3_octave", 1.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.15f}, {"osc3_scatter", 0.20f}, {"osc3_ampEnvSource", 2.0f}, {"osc3_level", 0.45f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 0.0f}, {"osc4_engine", 2.0f}, {"osc4_model", 2.0f}, {"osc4_octave", 0.0f}, {"osc4_shape", 0.45f}, {"osc4_detune", 0.25f}, {"osc4_scatter", 0.28f}, {"osc4_spread", 0.60f}, {"osc4_ampEnvSource", 2.0f}, {"osc4_level", 0.35f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 2800.0f}, {"filterResonance", 0.85f}, {"filterEnvAmount", 0.25f}, {"filterDrive", 0.10f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.005f}, {"ampDecay", 2.0f}, {"ampSustain", 0.65f}, {"ampRelease", 1.6f},
         {"fltAttack", 0.01f}, {"fltDecay", 1.6f}, {"fltSustain", 0.50f}, {"fltRelease", 1.4f},
         {"modAttack", 0.55f}, {"modDecay", 2.0f}, {"modSustain", 0.80f}, {"modRelease", 1.6f},
         {"macroWarmth", 0.70f}, {"macroMotion", 0.25f}, {"macroShimmer", 0.20f}, {"macroAir", 0.30f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.20f}, {"delayFeedback", 0.28f},
         {"reverbMix", 0.36f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.0f}, {"reverbShimmer", 0.15f}, {"reverbDucking", 0.38f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 36. Celestial Concert Harp
    presetManager_.registerFactoryPreset(PresetEntry{
        "Celestial Concert Harp", "Pluck",
        "Pristine acoustic concert harp pluck layered with worship shimmer cloud and physical modal harmonics.",
        {"Harp", "Pluck", "Worship", "Celestial", "Shimmer"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 1.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.0f}, {"osc1_detune", 0.15f}, {"osc1_scatter", 0.0f}, {"osc1_loopMode", 0.0f}, {"osc1_level", 0.98f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 2.0f}, {"osc2_engine", 3.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.60f}, {"osc2_detune", 0.20f}, {"osc2_scatter", 0.30f}, {"osc2_duration", 0.65f}, {"osc2_level", 0.78f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 1.0f}, {"osc3_model", 7.0f}, {"osc3_octave", 1.0f}, {"osc3_shape", 0.50f}, {"osc3_detune", 0.30f}, {"osc3_scatter", 0.28f}, {"osc3_warp", 0.35f}, {"osc3_level", 0.45f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 6500.0f}, {"filterResonance", 0.95f}, {"filterEnvAmount", 0.30f}, {"filterDrive", 0.05f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.003f}, {"ampDecay", 2.2f}, {"ampSustain", 0.50f}, {"ampRelease", 1.7f},
         {"fltAttack", 0.005f}, {"fltDecay", 1.8f}, {"fltSustain", 0.45f}, {"fltRelease", 1.5f},
         {"macroWarmth", 0.55f}, {"macroMotion", 0.30f}, {"macroShimmer", 0.55f}, {"macroAir", 0.40f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.24f}, {"delayFeedback", 0.35f},
         {"reverbMix", 0.42f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.5f}, {"reverbShimmer", 0.48f}, {"reverbDucking", 0.36f}, {"masterGainDb", 1.8f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 37. Nordic Mallet & Bell Cloud
    presetManager_.registerFactoryPreset(PresetEntry{
        "Nordic Mallet & Bell Cloud", "Lead",
        "Acoustic celesta bell and African kalimba layered with deep analog sub for Scandinavian indie melodies.",
        {"Mallet", "Celesta", "Kalimba", "Bell", "Nordic"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 3.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.0f}, {"osc1_detune", 0.15f}, {"osc1_scatter", 0.0f}, {"osc1_loopMode", 0.0f}, {"osc1_level", 0.88f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 4.0f}, {"osc2_engine", 6.0f}, {"osc2_model", 6.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.10f}, {"osc2_detune", 0.20f}, {"osc2_scatter", 0.0f}, {"osc2_loopMode", 0.0f}, {"osc2_level", 0.70f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 4.0f}, {"osc3_engine", 6.0f}, {"osc3_model", 5.0f}, {"osc3_octave", 1.0f}, {"osc3_shape", 0.0f}, {"osc3_detune", 0.22f}, {"osc3_scatter", 0.0f}, {"osc3_loopMode", 0.0f}, {"osc3_level", 0.65f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 1.0f}, {"osc4_engine", 4.0f}, {"osc4_model", 0.0f}, {"osc4_octave", -1.0f}, {"osc4_shape", 0.15f}, {"osc4_detune", 0.0f}, {"osc4_lowestNoteOnly", 1.0f}, {"osc4_level", 0.75f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 4500.0f}, {"filterResonance", 0.90f}, {"filterEnvAmount", 0.25f}, {"filterDrive", 0.12f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.004f}, {"ampDecay", 1.6f}, {"ampSustain", 0.45f}, {"ampRelease", 1.5f},
         {"fltAttack", 0.005f}, {"fltDecay", 1.3f}, {"fltSustain", 0.35f}, {"fltRelease", 1.3f},
         {"macroWarmth", 0.60f}, {"macroMotion", 0.35f}, {"macroShimmer", 0.30f}, {"macroAir", 0.35f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.25f}, {"delayFeedback", 0.30f},
         {"reverbMix", 0.35f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.0f}, {"reverbShimmer", 0.25f}, {"reverbDucking", 0.35f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 38. Sacred Pipe Flute Swell
    presetManager_.registerFactoryPreset(PresetEntry{
        "Sacred Pipe Flute Swell", "Pad",
        "Majestic cathedral pipe organ flute layered with vocal choir breath and warm church drawbars.",
        {"Organ", "Flute", "Cathedral", "Pad", "Sacred"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 7.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.25f}, {"osc1_detune", 0.18f}, {"osc1_scatter", 0.0f}, {"osc1_loopMode", 2.0f}, {"osc1_loopStart", 0.22f}, {"osc1_loopEnd", 0.88f}, {"osc1_loopXfade", 0.18f}, {"osc1_level", 0.90f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 2.0f}, {"osc2_model", 0.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.55f}, {"osc2_detune", 0.35f}, {"osc2_scatter", 0.32f}, {"osc2_spread", 0.65f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 1.0f}, {"osc3_model", 1.0f}, {"osc3_octave", 1.0f}, {"osc3_shape", 0.40f}, {"osc3_detune", 0.25f}, {"osc3_scatter", 0.25f}, {"osc3_warp", 0.30f}, {"osc3_level", 0.48f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 3200.0f}, {"filterResonance", 0.85f}, {"filterEnvAmount", 0.20f}, {"filterDrive", 0.08f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.65f}, {"ampDecay", 2.2f}, {"ampSustain", 0.80f}, {"ampRelease", 1.8f},
         {"fltAttack", 0.75f}, {"fltDecay", 2.0f}, {"fltSustain", 0.70f}, {"fltRelease", 1.7f},
         {"macroWarmth", 0.70f}, {"macroMotion", 0.25f}, {"macroShimmer", 0.35f}, {"macroAir", 0.45f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.22f}, {"delayFeedback", 0.25f},
         {"reverbMix", 0.44f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.4f}, {"reverbShimmer", 0.30f}, {"reverbDucking", 0.38f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 39. Music Box & Vinyl Dreams
    presetManager_.registerFactoryPreset(PresetEntry{
        "Music Box & Vinyl Dreams", "Arp",
        "Vintage kalimba tine & glockenspiel arpeggiator cushioned in authentic velvet vinyl dust atmosphere.",
        {"Arp", "MusicBox", "Vintage", "Vinyl", "Lofi"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 6.0f}, {"osc1_octave", 1.0f}, {"osc1_shape", 0.10f}, {"osc1_detune", 0.15f}, {"osc1_scatter", 0.0f}, {"osc1_loopMode", 0.0f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 4.0f}, {"osc2_engine", 6.0f}, {"osc2_model", 4.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.0f}, {"osc2_detune", 0.18f}, {"osc2_scatter", 0.0f}, {"osc2_loopMode", 0.0f}, {"osc2_level", 0.72f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 3.0f}, {"osc3_engine", 5.0f}, {"osc3_model", 1.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.65f}, {"osc3_detune", 0.30f}, {"osc3_rate", 0.45f}, {"osc3_scatter", 0.50f}, {"osc3_spread", 0.65f}, {"osc3_level", 0.40f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 3800.0f}, {"filterResonance", 0.90f}, {"filterEnvAmount", 0.30f}, {"filterDrive", 0.15f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.003f}, {"ampDecay", 0.85f}, {"ampSustain", 0.30f}, {"ampRelease", 1.0f},
         {"fltAttack", 0.004f}, {"fltDecay", 0.70f}, {"fltSustain", 0.25f}, {"fltRelease", 0.90f},
         {"macroWarmth", 0.65f}, {"macroMotion", 0.40f}, {"macroShimmer", 0.25f}, {"macroAir", 0.30f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.26f}, {"delayFeedback", 0.35f},
         {"reverbMix", 0.34f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.8f}, {"reverbShimmer", 0.20f}, {"reverbDucking", 0.35f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 1.0f}, {"arpSync", 1.0f}, {"arpSyncDiv", 4.0f}, {"arpRateHz", 4.0f}, {"subLowestOnly", 1.0f}}
    });

    // 40. Cathedral Chime & Choir Ambient
    presetManager_.registerFactoryPreset(PresetEntry{
        "Cathedral Chime & Choir Ambient", "Ambient",
        "Singing acoustic hand chimes floating into an expansive choral swell with deep infinite space reverb.",
        {"Ambient", "Chime", "Choir", "Atmosphere", "Cinematic"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 5.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.05f}, {"osc1_detune", 0.20f}, {"osc1_scatter", 0.0f}, {"osc1_loopMode", 2.0f}, {"osc1_loopStart", 0.20f}, {"osc1_loopEnd", 0.88f}, {"osc1_loopXfade", 0.18f}, {"osc1_level", 0.86f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 4.0f}, {"osc2_engine", 6.0f}, {"osc2_model", 7.0f}, {"osc2_octave", -1.0f}, {"osc2_shape", 0.30f}, {"osc2_detune", 0.25f}, {"osc2_scatter", 0.0f}, {"osc2_loopMode", 2.0f}, {"osc2_loopStart", 0.25f}, {"osc2_loopEnd", 0.88f}, {"osc2_loopXfade", 0.16f}, {"osc2_level", 0.68f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 2.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.60f}, {"osc3_detune", 0.40f}, {"osc3_scatter", 0.35f}, {"osc3_spread", 0.68f}, {"osc3_ampEnvSource", 2.0f}, {"osc3_level", 0.78f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 0.0f}, {"osc4_engine", 1.0f}, {"osc4_model", 3.0f}, {"osc4_octave", 1.0f}, {"osc4_shape", 0.55f}, {"osc4_detune", 0.32f}, {"osc4_scatter", 0.28f}, {"osc4_warp", 0.35f}, {"osc4_level", 0.52f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 2600.0f}, {"filterResonance", 0.88f}, {"filterEnvAmount", 0.22f}, {"filterDrive", 0.05f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.85f}, {"ampDecay", 2.6f}, {"ampSustain", 0.85f}, {"ampRelease", 2.0f},
         {"fltAttack", 1.00f}, {"fltDecay", 2.4f}, {"fltSustain", 0.75f}, {"fltRelease", 1.9f},
         {"modAttack", 1.15f}, {"modDecay", 2.6f}, {"modSustain", 0.88f}, {"modRelease", 1.9f},
         {"macroWarmth", 0.75f}, {"macroMotion", 0.35f}, {"macroShimmer", 0.60f}, {"macroAir", 0.50f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.24f}, {"delayFeedback", 0.30f},
         {"reverbMix", 0.50f}, {"reverbDry", 0.95f}, {"reverbDecay", 5.0f}, {"reverbShimmer", 0.52f}, {"reverbDucking", 0.38f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 41. Solo Concert Grand Piano (Pristine True-Stereo Multi-Zone Acoustic Grand)
    presetManager_.registerFactoryPreset(PresetEntry{
        "Solo Concert Grand Piano", "Acoustic",
        "Pure, unadorned Steinway concert grand piano in full stereo width with authentic multi-octave acoustic resonance.",
        {"Piano", "Grand", "Acoustic", "Solo", "Steinway"},
        {{"activeOscCount", 1.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.0f}, {"osc1_detune", 0.12f}, {"osc1_scatter", 0.0f}, {"osc1_loopMode", 0.0f}, {"osc1_level", 0.95f},
         {"osc2_enabled", 0.0f}, {"osc3_enabled", 0.0f}, {"osc4_enabled", 0.0f},
         {"filter1Enabled", 0.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 18000.0f}, {"filterResonance", 0.707f}, {"filterEnvAmount", 0.0f}, {"filterDrive", 0.0f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.001f}, {"ampDecay", 4.2f}, {"ampSustain", 0.70f}, {"ampRelease", 1.6f},
         {"fltAttack", 0.001f}, {"fltDecay", 4.2f}, {"fltSustain", 1.0f}, {"fltRelease", 1.6f},
         {"macroWarmth", 0.50f}, {"macroMotion", 0.0f}, {"macroShimmer", 0.0f}, {"macroAir", 0.25f},
         {"delayEnabled", 0.0f}, {"delayMix", 0.0f},
         {"reverbMix", 0.18f}, {"reverbDry", 1.0f}, {"reverbDecay", 2.4f}, {"reverbShimmer", 0.0f}, {"reverbDucking", 0.30f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 42. Pure Concert Harp Solo
    presetManager_.registerFactoryPreset(PresetEntry{
        "Pure Concert Harp Solo", "Acoustic",
        "Organic concert harp pluck with authentic room acoustics and natural acoustic decay across multiple octaves.",
        {"Harp", "Pluck", "Acoustic", "Solo", "Organic"},
        {{"activeOscCount", 1.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 1.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.0f}, {"osc1_detune", 0.14f}, {"osc1_scatter", 0.0f}, {"osc1_loopMode", 0.0f}, {"osc1_level", 0.95f},
         {"osc2_enabled", 0.0f}, {"osc3_enabled", 0.0f}, {"osc4_enabled", 0.0f},
         {"filter1Enabled", 0.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 18000.0f}, {"filterResonance", 0.707f}, {"filterEnvAmount", 0.0f}, {"filterDrive", 0.0f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.001f}, {"ampDecay", 4.0f}, {"ampSustain", 0.60f}, {"ampRelease", 1.6f},
         {"fltAttack", 0.001f}, {"fltDecay", 4.0f}, {"fltSustain", 1.0f}, {"fltRelease", 1.6f},
         {"macroWarmth", 0.50f}, {"macroMotion", 0.0f}, {"macroShimmer", 0.0f}, {"macroAir", 0.30f},
         {"delayEnabled", 0.0f}, {"delayMix", 0.0f},
         {"reverbMix", 0.22f}, {"reverbDry", 1.0f}, {"reverbDecay", 2.6f}, {"reverbShimmer", 0.0f}, {"reverbDucking", 0.30f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 43. Nordic Celesta & Glockenspiel
    presetManager_.registerFactoryPreset(PresetEntry{
        "Nordic Celesta & Glockenspiel", "Acoustic",
        "Layered acoustic celesta mallet bell and orchestral steel chimes with pristine high-frequency air.",
        {"Celesta", "Glockenspiel", "Bell", "Acoustic", "Mallet"},
        {{"activeOscCount", 2.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 3.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.0f}, {"osc1_detune", 0.15f}, {"osc1_scatter", 0.0f}, {"osc1_loopMode", 0.0f}, {"osc1_level", 0.88f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 4.0f}, {"osc2_engine", 6.0f}, {"osc2_model", 4.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.0f}, {"osc2_detune", 0.18f}, {"osc2_scatter", 0.0f}, {"osc2_loopMode", 0.0f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 0.0f}, {"osc4_enabled", 0.0f},
         {"filter1Enabled", 0.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 18000.0f}, {"filterResonance", 0.707f}, {"filterEnvAmount", 0.0f}, {"filterDrive", 0.0f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.001f}, {"ampDecay", 3.5f}, {"ampSustain", 0.50f}, {"ampRelease", 1.5f},
         {"fltAttack", 0.001f}, {"fltDecay", 3.5f}, {"fltSustain", 1.0f}, {"fltRelease", 1.5f},
         {"macroWarmth", 0.50f}, {"macroMotion", 0.0f}, {"macroShimmer", 0.10f}, {"macroAir", 0.35f},
         {"delayEnabled", 0.0f}, {"delayMix", 0.0f},
         {"reverbMix", 0.24f}, {"reverbDry", 1.0f}, {"reverbDecay", 2.8f}, {"reverbShimmer", 0.10f}, {"reverbDucking", 0.30f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

}

} // namespace nova::synth
