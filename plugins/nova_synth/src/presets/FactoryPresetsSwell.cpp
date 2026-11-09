#include "FactoryPresets.h"
#include "nova_plugin/PresetManager.h"
#include "nova_dsp/synth/WorshipSynthEngine.h"

namespace nova::synth {

void registerFactoryPresetsSwell(nova::plugin::PresetManager& presetManager_) {
    using nova::plugin::PresetEntry;

    // =========================================================================
    // 10. SWELL & PIANO PADS (Expression / Schwellerpedal Morphing Carpets)
    // =========================================================================
    using Src = nova::dsp::ModMatrix::Source;
    using Dst = nova::dsp::ModMatrix::Destination;
    auto ms = [](Src s, Dst d, float amt) noexcept -> nova::plugin::PresetModSlot {
        return { static_cast<int>(s), static_cast<int>(d), amt, true };
    };

    // 44. Bethel Soaking Warm Pad (Ref 1: Instrumental Soaking Worship 1:00:18 - Pure Coherent Juno/Prophet Poly-Pad)
    presetManager_.registerFactoryPreset(PresetEntry{
        "Bethel Soaking Warm Pad", "Swell & Piano Pads",
        "Buttery-smooth, rock-steady vintage analog poly-pad (Velvet Pad + Warm Triangle + Analog Bed). Soft, singing velvet cushion under piano; opens gently on the Swell pedal with zero shimmer or harshness.",
        {"Swell", "Pedal", "Bethel", "Soaking", "Analog", "Warm"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 6.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.35f}, {"osc1_detune", 0.22f}, {"osc1_scatter", 0.0f}, {"osc1_warp", 0.0f}, {"osc1_filterRouting", 2.0f}, {"osc1_level", 0.52f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.10f}, {"osc2_detune", 0.18f}, {"osc2_scatter", 0.0f}, {"osc2_warp", 0.0f}, {"osc2_filterRouting", 2.0f}, {"osc2_level", 0.42f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 2.0f}, {"osc3_model", 5.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.16f}, {"osc3_scatter", 0.12f}, {"osc3_spread", 0.70f}, {"osc3_ampEnvSource", 0.0f}, {"osc3_filterRouting", 2.0f}, {"osc3_level", 0.28f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 1.0f}, {"osc4_engine", 4.0f}, {"osc4_model", 0.0f}, {"osc4_octave", -1.0f}, {"osc4_shape", 0.0f}, {"osc4_detune", 0.0f}, {"osc4_lowestNoteOnly", 1.0f}, {"osc4_filterRouting", 2.0f}, {"osc4_level", 0.08f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 780.0f}, {"filterResonance", 0.12f}, {"filterEnvAmount", 0.05f}, {"filterDrive", 0.0f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 80.0f}, {"filter2Resonance", 0.30f}, {"vocalCarve", 0.25f},
         {"filterRoutingMode", 0.0f},
         {"ampAttack", 0.48f}, {"ampDecay", 2.0f}, {"ampSustain", 0.95f}, {"ampRelease", 1.50f},
         {"fltAttack", 0.60f}, {"fltDecay", 2.0f}, {"fltSustain", 0.60f}, {"fltRelease", 1.50f},
         {"modAttack", 0.60f}, {"modDecay", 2.0f}, {"modSustain", 0.90f}, {"modRelease", 1.50f},
         {"lfo1RateHz", 0.20f}, {"lfo1Depth", 0.0f}, {"lfo2RateHz", 0.15f}, {"lfo2Depth", 0.0f},
         {"chorusEnabled", 1.0f}, {"chorusMode", 0.0f}, {"chorusMix", 0.40f}, {"chorusRateHz", 0.45f}, {"chorusDepth", 0.42f}, {"chorusWarmth", 0.70f},
         {"macroWarmth", 0.50f}, {"macroMotion", 0.20f}, {"macroShimmer", 0.0f}, {"macroAir", 0.30f},
         {"delayEnabled", 0.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.0f}, {"delayFeedback", 0.15f},
         {"reverbMix", 0.46f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.6f}, {"reverbShimmer", 0.0f}, {"reverbShimmerPitch", 0.0f}, {"reverbDucking", 0.45f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}},
        {ms(Src::ModWheel, Dst::FilterCutoff, 0.45f),
         ms(Src::ModWheel, Dst::Osc1Level,    0.20f),
         ms(Src::ModWheel, Dst::Osc3Level,    0.25f),
         ms(Src::ModWheel, Dst::ReverbMix,    0.08f)}
    });

    // 45. ICF Silky High Shimmer (Ref 2: Die Schoepfung singt 05:17 - Weightless High String & Velvet Sheen)
    presetManager_.registerFactoryPreset(PresetEntry{
        "ICF Silky High Shimmer", "Swell & Piano Pads",
        "Weightless, crystal-clear high-register string and velvet cushion pad with high-passed low-mids so piano chords stay completely transparent and open.",
        {"Swell", "Pedal", "ICF", "Silky", "Strings", "Velvet"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 1.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.25f}, {"osc1_detune", 0.20f}, {"osc1_scatter", 0.12f}, {"osc1_spread", 0.78f}, {"osc1_filterRouting", 2.0f}, {"osc1_level", 0.50f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 6.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.35f}, {"osc2_detune", 0.18f}, {"osc2_scatter", 0.0f}, {"osc2_spread", 0.70f}, {"osc2_filterRouting", 2.0f}, {"osc2_level", 0.44f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 2.0f}, {"osc3_model", 5.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.16f}, {"osc3_scatter", 0.12f}, {"osc3_spread", 0.80f}, {"osc3_ampEnvSource", 2.0f}, {"osc3_filterRouting", 2.0f}, {"osc3_level", 0.26f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 0.0f}, {"osc4_engine", 0.0f}, {"osc4_model", 3.0f}, {"osc4_octave", 0.0f}, {"osc4_shape", 0.06f}, {"osc4_detune", 0.10f}, {"osc4_filterRouting", 2.0f}, {"osc4_level", 0.26f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 920.0f}, {"filterResonance", 0.10f}, {"filterEnvAmount", 0.05f}, {"filterDrive", 0.0f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 85.0f}, {"filter2Resonance", 0.25f}, {"vocalCarve", 0.30f},
         {"filterRoutingMode", 0.0f},
         {"ampAttack", 0.48f}, {"ampDecay", 1.8f}, {"ampSustain", 0.90f}, {"ampRelease", 1.48f},
         {"fltAttack", 0.55f}, {"fltDecay", 1.8f}, {"fltSustain", 0.60f}, {"fltRelease", 1.40f},
         {"modAttack", 0.60f}, {"modDecay", 1.9f}, {"modSustain", 0.88f}, {"modRelease", 1.42f},
         {"lfo1RateHz", 0.20f}, {"lfo1Depth", 0.14f}, {"lfo2RateHz", 0.15f}, {"lfo2Depth", 0.12f},
         {"chorusEnabled", 1.0f}, {"chorusMode", 4.0f}, {"chorusMix", 0.42f}, {"chorusRateHz", 0.42f}, {"chorusDepth", 0.45f},
         {"macroWarmth", 0.50f}, {"macroMotion", 0.25f}, {"macroShimmer", 0.0f}, {"macroAir", 0.35f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.14f}, {"delayFeedback", 0.20f},
         {"reverbMix", 0.45f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.6f}, {"reverbShimmer", 0.0f}, {"reverbShimmerPitch", 0.0f}, {"reverbDucking", 0.45f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}},
        {ms(Src::ModWheel, Dst::FilterCutoff,  0.42f),
         ms(Src::ModWheel, Dst::Osc1Level,     0.20f),
         ms(Src::ModWheel, Dst::Osc2Level,     0.25f),
         ms(Src::ModWheel, Dst::PanSpread,     0.35f),
         ms(Src::ModWheel, Dst::ReverbMix,     0.06f),
         ms(Src::Lfo1,     Dst::Osc1Shape,     0.10f),
         ms(Src::Lfo2,     Dst::PanSpread,     0.14f)}
    });

    // 46. Beautiful Name Intro Pad (Ref 3: Hillsong Intro 0:00 - Dark Analog Pulse/Saw + Velvet Halo & Mid Pocket)
    presetManager_.registerFactoryPreset(PresetEntry{
        "Beautiful Name Intro Pad", "Swell & Piano Pads",
        "Dark, intimate analog pulse/saw foundation with a clean 1.35 kHz piano/vocal mid pocket and a delicate velvet harmonic bloom on the Swell pedal.",
        {"Swell", "Pedal", "Hillsong", "Intro", "Dark", "Velvet"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 6.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.30f}, {"osc1_detune", 0.20f}, {"osc1_scatter", 0.0f}, {"osc1_warp", 0.0f}, {"osc1_filterRouting", 2.0f}, {"osc1_level", 0.52f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.20f}, {"osc2_detune", 0.16f}, {"osc2_scatter", 0.0f}, {"osc2_warp", 0.0f}, {"osc2_filterRouting", 2.0f}, {"osc2_level", 0.42f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 2.0f}, {"osc3_model", 5.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.16f}, {"osc3_scatter", 0.12f}, {"osc3_spread", 0.75f}, {"osc3_ampEnvSource", 2.0f}, {"osc3_filterRouting", 2.0f}, {"osc3_level", 0.26f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 1.0f}, {"osc4_engine", 4.0f}, {"osc4_model", 1.0f}, {"osc4_octave", -1.0f}, {"osc4_shape", 0.0f}, {"osc4_detune", 0.0f}, {"osc4_lowestNoteOnly", 1.0f}, {"osc4_filterRouting", 2.0f}, {"osc4_level", 0.08f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 740.0f}, {"filterResonance", 0.12f}, {"filterEnvAmount", 0.04f}, {"filterDrive", 0.0f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 78.0f}, {"filter2Resonance", 0.25f}, {"vocalCarve", 0.35f},
         {"filterRoutingMode", 0.0f},
         {"ampAttack", 0.38f}, {"ampDecay", 1.7f}, {"ampSustain", 0.90f}, {"ampRelease", 1.38f},
         {"fltAttack", 0.45f}, {"fltDecay", 1.7f}, {"fltSustain", 0.50f}, {"fltRelease", 1.32f},
         {"modAttack", 0.52f}, {"modDecay", 1.8f}, {"modSustain", 0.88f}, {"modRelease", 1.35f},
         {"lfo1RateHz", 0.22f}, {"lfo1Depth", 0.14f}, {"lfo2RateHz", 0.16f}, {"lfo2Depth", 0.12f},
         {"chorusEnabled", 1.0f}, {"chorusMode", 0.0f}, {"chorusMix", 0.40f}, {"chorusRateHz", 0.45f}, {"chorusDepth", 0.42f},
         {"macroWarmth", 0.52f}, {"macroMotion", 0.20f}, {"macroShimmer", 0.0f}, {"macroAir", 0.30f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.12f}, {"delayFeedback", 0.20f},
         {"reverbMix", 0.45f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.5f}, {"reverbShimmer", 0.0f}, {"reverbShimmerPitch", 0.0f}, {"reverbDucking", 0.45f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}},
        {ms(Src::ModWheel, Dst::FilterCutoff,  0.45f),
         ms(Src::ModWheel, Dst::Osc1Level,     0.22f),
         ms(Src::ModWheel, Dst::Osc3Level,     0.26f),
         ms(Src::ModWheel, Dst::PanSpread,     0.35f),
         ms(Src::ModWheel, Dst::ReverbMix,     0.06f),
         ms(Src::Lfo1,     Dst::Osc1Shape,     0.10f),
         ms(Src::Lfo2,     Dst::PanSpread,     0.14f)}
    });

    // 47. Holy Forever Anthem Pad (Ref 4: Bethel 04:22 - Wide SuperSaw Worship Pad + Tempo-Synced 1/16th Pulse Layer)
    presetManager_.registerFactoryPreset(PresetEntry{
        "Holy Forever Anthem Pad", "Swell & Piano Pads",
        "Wide modern anthem worship pad with a tempo-synced 1/16th rhythmic synth pulse layer and singing warm analog harmonics that bloom as you push the Swell pedal.",
        {"Swell", "Pedal", "Anthem", "Pulse", "SuperSaw", "Bethel"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 4.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.18f}, {"osc1_detune", 0.22f}, {"osc1_scatter", 0.12f}, {"osc1_spread", 0.80f}, {"osc1_filterRouting", 2.0f}, {"osc1_level", 0.50f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 6.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.35f}, {"osc2_detune", 0.18f}, {"osc2_scatter", 0.0f}, {"osc2_spread", 0.70f}, {"osc2_ampEnvSource", 0.0f}, {"osc2_filterRouting", 2.0f}, {"osc2_level", 0.44f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 1.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.16f}, {"osc3_scatter", 0.12f}, {"osc3_spread", 0.75f}, {"osc3_ampEnvSource", 2.0f}, {"osc3_filterRouting", 2.0f}, {"osc3_level", 0.26f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 1.0f}, {"osc4_engine", 4.0f}, {"osc4_model", 0.0f}, {"osc4_octave", -1.0f}, {"osc4_shape", 0.0f}, {"osc4_detune", 0.0f}, {"osc4_lowestNoteOnly", 1.0f}, {"osc4_filterRouting", 2.0f}, {"osc4_level", 0.08f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 850.0f}, {"filterResonance", 0.12f}, {"filterEnvAmount", 0.06f}, {"filterDrive", 0.0f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 80.0f}, {"filter2Resonance", 0.25f}, {"vocalCarve", 0.30f},
         {"filterRoutingMode", 0.0f},
         {"ampAttack", 0.35f}, {"ampDecay", 1.8f}, {"ampSustain", 0.90f}, {"ampRelease", 1.42f},
         {"fltAttack", 0.42f}, {"fltDecay", 1.8f}, {"fltSustain", 0.56f}, {"fltRelease", 1.35f},
         {"modAttack", 0.50f}, {"modDecay", 1.8f}, {"modSustain", 0.88f}, {"modRelease", 1.38f},
         {"lfo1RateHz", 0.24f}, {"lfo1Depth", 0.16f},
         {"lfo4Waveform", 6.0f}, {"lfo4Sync", 1.0f}, {"lfo4SyncDiv", 10.0f}, {"lfo4Depth", 0.45f},
         {"chorusEnabled", 1.0f}, {"chorusMode", 1.0f}, {"chorusMix", 0.42f}, {"chorusRateHz", 0.55f}, {"chorusDepth", 0.48f},
         {"macroWarmth", 0.50f}, {"macroMotion", 0.30f}, {"macroShimmer", 0.0f}, {"macroAir", 0.35f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.15f}, {"delayFeedback", 0.22f},
         {"reverbMix", 0.45f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.6f}, {"reverbShimmer", 0.0f}, {"reverbShimmerPitch", 0.0f}, {"reverbDucking", 0.45f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}},
        {ms(Src::ModWheel, Dst::FilterCutoff,  0.45f),
         ms(Src::ModWheel, Dst::Osc1Level,     0.22f),
         ms(Src::ModWheel, Dst::Osc2Level,     0.25f),
         ms(Src::ModWheel, Dst::PanSpread,     0.35f),
         ms(Src::ModWheel, Dst::ReverbMix,     0.06f),
         ms(Src::Lfo4,     Dst::Osc3Level,     0.28f),
         ms(Src::Lfo4,     Dst::FilterCutoff,  0.08f),
         ms(Src::Lfo1,     Dst::PanSpread,     0.15f)}
    });

    // 48. Promise Velvet Vocal Bed (Ref 5: Elevation 16:40 - Deep Warm Mellotron Choir-Horn Cushion Under Piano)
    presetManager_.registerFactoryPreset(PresetEntry{
        "Promise Velvet Vocal Bed", "Swell & Piano Pads",
        "Deep, velvety vocal-formant (Mellotron choir-horn) and singing analog cushion that sits softly under a slow piano and blooms into a warm cathedral vowel pad on the Swell pedal.",
        {"Swell", "Pedal", "Elevation", "Vocal", "Velvet", "Piano"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 2.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.25f}, {"osc1_detune", 0.18f}, {"osc1_scatter", 0.12f}, {"osc1_spread", 0.72f}, {"osc1_filterRouting", 2.0f}, {"osc1_level", 0.54f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 1.0f}, {"osc2_model", 3.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.18f}, {"osc2_detune", 0.16f}, {"osc2_scatter", 0.12f}, {"osc2_warp", 0.15f}, {"osc2_filterRouting", 2.0f}, {"osc2_level", 0.44f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 6.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.30f}, {"osc3_detune", 0.16f}, {"osc3_scatter", 0.0f}, {"osc3_spread", 0.70f}, {"osc3_ampEnvSource", 2.0f}, {"osc3_filterRouting", 2.0f}, {"osc3_level", 0.38f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 1.0f}, {"osc4_engine", 4.0f}, {"osc4_model", 0.0f}, {"osc4_octave", -1.0f}, {"osc4_shape", 0.0f}, {"osc4_detune", 0.0f}, {"osc4_lowestNoteOnly", 1.0f}, {"osc4_filterRouting", 2.0f}, {"osc4_level", 0.08f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 840.0f}, {"filterResonance", 0.10f}, {"filterEnvAmount", 0.04f}, {"filterDrive", 0.0f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 80.0f}, {"filter2Resonance", 0.25f}, {"vocalCarve", 0.35f},
         {"filterRoutingMode", 0.0f},
         {"ampAttack", 0.46f}, {"ampDecay", 1.9f}, {"ampSustain", 0.92f}, {"ampRelease", 1.45f},
         {"fltAttack", 0.54f}, {"fltDecay", 1.9f}, {"fltSustain", 0.54f}, {"fltRelease", 1.38f},
         {"modAttack", 0.60f}, {"modDecay", 1.9f}, {"modSustain", 0.90f}, {"modRelease", 1.40f},
         {"lfo1RateHz", 0.18f}, {"lfo1Depth", 0.14f}, {"lfo2RateHz", 0.14f}, {"lfo2Depth", 0.12f},
         {"chorusEnabled", 1.0f}, {"chorusMode", 3.0f}, {"chorusMix", 0.44f}, {"chorusRateHz", 0.48f}, {"chorusDepth", 0.48f},
         {"macroWarmth", 0.52f}, {"macroMotion", 0.22f}, {"macroShimmer", 0.0f}, {"macroAir", 0.30f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.12f}, {"delayFeedback", 0.18f},
         {"reverbMix", 0.46f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.6f}, {"reverbShimmer", 0.0f}, {"reverbShimmerPitch", 0.0f}, {"reverbDucking", 0.45f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}},
        {ms(Src::ModWheel, Dst::FilterCutoff,  0.42f),
         ms(Src::ModWheel, Dst::Osc1Level,     0.20f),
         ms(Src::ModWheel, Dst::Osc2Level,     0.25f),
         ms(Src::ModWheel, Dst::Osc3Level,     0.25f),
         ms(Src::ModWheel, Dst::PanSpread,     0.35f),
         ms(Src::ModWheel, Dst::ReverbMix,     0.06f),
         ms(Src::Lfo2,     Dst::PanSpread,     0.14f)}
    });

    // 49. Promise Golden Brass Swell (Ref 6: Elevation 1:21:44 - Glorious CS-80 / OB-8 Warm Poly-Brass Horizon)
    presetManager_.registerFactoryPreset(PresetEntry{
        "Promise Golden Brass Swell", "Swell & Piano Pads",
        "Rich, glorious CS-80 / OB-8 analog poly-brass pad. Intimate warm saw cushion at low pedal; pushing the Swell wheel sweeps the 24dB ladder filter into a singing 1.2 kHz golden brass horizon.",
        {"Swell", "Pedal", "Elevation", "Brass", "CS80", "Golden"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 1.0f}, {"osc1_model", 6.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.25f}, {"osc1_detune", 0.18f}, {"osc1_scatter", 0.12f}, {"osc1_warp", 0.15f}, {"osc1_filterRouting", 2.0f}, {"osc1_level", 0.52f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 6.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.35f}, {"osc2_detune", 0.18f}, {"osc2_scatter", 0.0f}, {"osc2_warp", 0.0f}, {"osc2_ampEnvSource", 2.0f}, {"osc2_filterRouting", 2.0f}, {"osc2_level", 0.44f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 2.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.12f}, {"osc3_detune", 0.14f}, {"osc3_scatter", 0.0f}, {"osc3_spread", 0.70f}, {"osc3_ampEnvSource", 2.0f}, {"osc3_filterRouting", 2.0f}, {"osc3_level", 0.28f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 1.0f}, {"osc4_engine", 4.0f}, {"osc4_model", 0.0f}, {"osc4_octave", -1.0f}, {"osc4_shape", 0.0f}, {"osc4_detune", 0.0f}, {"osc4_lowestNoteOnly", 1.0f}, {"osc4_filterRouting", 2.0f}, {"osc4_level", 0.08f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 780.0f}, {"filterResonance", 0.12f}, {"filterEnvAmount", 0.06f}, {"filterDrive", 0.0f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 80.0f}, {"filter2Resonance", 0.25f}, {"vocalCarve", 0.30f},
         {"filterRoutingMode", 0.0f},
         {"ampAttack", 0.38f}, {"ampDecay", 1.8f}, {"ampSustain", 0.88f}, {"ampRelease", 1.40f},
         {"fltAttack", 0.48f}, {"fltDecay", 1.7f}, {"fltSustain", 0.52f}, {"fltRelease", 1.32f},
         {"modAttack", 0.55f}, {"modDecay", 1.8f}, {"modSustain", 0.86f}, {"modRelease", 1.35f},
         {"lfo1RateHz", 0.20f}, {"lfo1Depth", 0.14f}, {"lfo2RateHz", 0.15f}, {"lfo2Depth", 0.12f},
         {"chorusEnabled", 1.0f}, {"chorusMode", 2.0f}, {"chorusMix", 0.42f}, {"chorusRateHz", 0.50f}, {"chorusDepth", 0.48f},
         {"macroWarmth", 0.50f}, {"macroMotion", 0.24f}, {"macroShimmer", 0.0f}, {"macroAir", 0.32f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 2.0f}, {"delayMix", 0.14f}, {"delayFeedback", 0.20f},
         {"reverbMix", 0.45f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.5f}, {"reverbShimmer", 0.0f}, {"reverbShimmerPitch", 0.0f}, {"reverbDucking", 0.45f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}},
        {ms(Src::ModWheel, Dst::FilterCutoff,  0.46f),
         ms(Src::ModWheel, Dst::Osc1Level,     0.22f),
         ms(Src::ModWheel, Dst::Osc2Level,     0.26f),
         ms(Src::ModWheel, Dst::PanSpread,     0.35f),
         ms(Src::ModWheel, Dst::ReverbMix,     0.06f),
         ms(Src::Lfo1,     Dst::FilterCutoff,  0.08f),
         ms(Src::Lfo2,     Dst::PanSpread,     0.14f)}
    });

    // 50. Ethereal Grains & Bell Cloud (Ref 7: Abel Mendoza - Scooped Warm Pad + Sparkling High Granular & Celesta Cloud)
    presetManager_.registerFactoryPreset(PresetEntry{
        "Ethereal Grains & Bell Cloud", "Swell & Piano Pads",
        "Clean, warm velvet foundation that leaves 100% space for your piano in the mids, while swelling the pedal blooms subtle acoustic celesta bells and granular mist.",
        {"Swell", "Pedal", "Grains", "Ethereal", "Bells", "Ambient"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 6.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.30f}, {"osc1_detune", 0.18f}, {"osc1_scatter", 0.0f}, {"osc1_filterRouting", 2.0f}, {"osc1_level", 0.54f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 2.0f}, {"osc2_model", 5.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.25f}, {"osc2_detune", 0.16f}, {"osc2_scatter", 0.12f}, {"osc2_rate", 0.32f}, {"osc2_spread", 0.80f}, {"osc2_ampEnvSource", 2.0f}, {"osc2_filterRouting", 2.0f}, {"osc2_level", 0.42f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 2.0f}, {"osc3_model", 3.0f}, {"osc3_octave", 1.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.16f}, {"osc3_scatter", 0.14f}, {"osc3_spread", 0.82f}, {"osc3_ampEnvSource", 2.0f}, {"osc3_filterRouting", 2.0f}, {"osc3_level", 0.16f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 4.0f}, {"osc4_engine", 6.0f}, {"osc4_model", 3.0f}, {"osc4_octave", 0.0f}, {"osc4_shape", 0.08f}, {"osc4_detune", 0.10f}, {"osc4_scatter", 0.0f}, {"osc4_loopMode", 2.0f}, {"osc4_loopStart", 0.20f}, {"osc4_loopEnd", 0.88f}, {"osc4_loopXfade", 0.18f}, {"osc4_ampEnvSource", 2.0f}, {"osc4_filterRouting", 2.0f}, {"osc4_level", 0.12f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 840.0f}, {"filterResonance", 0.10f}, {"filterEnvAmount", 0.04f}, {"filterDrive", 0.0f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 82.0f}, {"filter2Resonance", 0.25f}, {"vocalCarve", 0.35f},
         {"filterRoutingMode", 0.0f},
         {"ampAttack", 0.42f}, {"ampDecay", 1.8f}, {"ampSustain", 0.88f}, {"ampRelease", 1.42f},
         {"fltAttack", 0.50f}, {"fltDecay", 1.8f}, {"fltSustain", 0.52f}, {"fltRelease", 1.34f},
         {"modAttack", 0.58f}, {"modDecay", 1.9f}, {"modSustain", 0.88f}, {"modRelease", 1.38f},
         {"lfo1RateHz", 0.24f}, {"lfo1Depth", 0.16f}, {"lfo2RateHz", 0.17f}, {"lfo2Depth", 0.14f},
         {"chorusEnabled", 1.0f}, {"chorusMode", 4.0f}, {"chorusMix", 0.42f}, {"chorusRateHz", 0.38f}, {"chorusDepth", 0.46f},
         {"macroWarmth", 0.50f}, {"macroMotion", 0.28f}, {"macroShimmer", 0.0f}, {"macroAir", 0.35f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.14f}, {"delayFeedback", 0.20f}, {"delaySwing", 0.20f},
         {"reverbMix", 0.46f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.6f}, {"reverbShimmer", 0.0f}, {"reverbShimmerPitch", 0.0f}, {"reverbDucking", 0.45f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}},
        {ms(Src::ModWheel, Dst::FilterCutoff,  0.42f),
         ms(Src::ModWheel, Dst::Osc1Level,     0.20f),
         ms(Src::ModWheel, Dst::Osc2Level,     0.28f),
         ms(Src::ModWheel, Dst::Osc4Level,     0.20f),
         ms(Src::ModWheel, Dst::PanSpread,     0.35f),
         ms(Src::ModWheel, Dst::ReverbMix,     0.06f),
         ms(Src::Lfo2,     Dst::PanSpread,     0.15f)}
    });

    // 51. Sacred Cello & Pipe Swell (Companion Patch - Bowed Cello Continuo + Liturgical Pipe Flute & Warm Velvet Pad)
    presetManager_.registerFactoryPreset(PresetEntry{
        "Sacred Cello & Pipe Swell", "Swell & Piano Pads",
        "Warm bowed baritone cello and liturgical wooden pipe flute beneath your piano; swelling up pulls out velvety analog warmth and upper acoustic breath.",
        {"Swell", "Pedal", "Cello", "Pipe", "Organ", "Cathedral"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 2.0f}, {"osc1_model", 1.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.25f}, {"osc1_detune", 0.16f}, {"osc1_scatter", 0.12f}, {"osc1_spread", 0.65f}, {"osc1_filterRouting", 2.0f}, {"osc1_level", 0.52f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 4.0f}, {"osc2_engine", 6.0f}, {"osc2_model", 7.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.10f}, {"osc2_detune", 0.10f}, {"osc2_scatter", 0.0f}, {"osc2_loopMode", 2.0f}, {"osc2_loopStart", 0.22f}, {"osc2_loopEnd", 0.88f}, {"osc2_loopXfade", 0.18f}, {"osc2_filterRouting", 2.0f}, {"osc2_level", 0.32f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 6.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.30f}, {"osc3_detune", 0.16f}, {"osc3_scatter", 0.0f}, {"osc3_spread", 0.70f}, {"osc3_ampEnvSource", 2.0f}, {"osc3_filterRouting", 2.0f}, {"osc3_level", 0.38f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 0.0f}, {"osc4_engine", 2.0f}, {"osc4_model", 4.0f}, {"osc4_octave", 0.0f}, {"osc4_shape", 0.15f}, {"osc4_detune", 0.12f}, {"osc4_filterRouting", 2.0f}, {"osc4_level", 0.24f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 800.0f}, {"filterResonance", 0.10f}, {"filterEnvAmount", 0.04f}, {"filterDrive", 0.0f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 80.0f}, {"filter2Resonance", 0.25f}, {"vocalCarve", 0.30f},
         {"filterRoutingMode", 0.0f},
         {"ampAttack", 0.45f}, {"ampDecay", 1.9f}, {"ampSustain", 0.92f}, {"ampRelease", 1.45f},
         {"fltAttack", 0.52f}, {"fltDecay", 1.9f}, {"fltSustain", 0.55f}, {"fltRelease", 1.38f},
         {"modAttack", 0.60f}, {"modDecay", 1.9f}, {"modSustain", 0.90f}, {"modRelease", 1.40f},
         {"lfo1RateHz", 0.22f}, {"lfo1Depth", 0.14f}, {"lfo2RateHz", 0.16f}, {"lfo2Depth", 0.12f},
         {"chorusEnabled", 1.0f}, {"chorusMode", 3.0f}, {"chorusMix", 0.42f}, {"chorusRateHz", 0.45f}, {"chorusDepth", 0.45f},
         {"macroWarmth", 0.52f}, {"macroMotion", 0.22f}, {"macroShimmer", 0.0f}, {"macroAir", 0.30f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.12f}, {"delayFeedback", 0.18f},
         {"reverbMix", 0.46f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.5f}, {"reverbShimmer", 0.0f}, {"reverbShimmerPitch", 0.0f}, {"reverbDucking", 0.45f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}},
        {ms(Src::ModWheel, Dst::FilterCutoff,  0.42f),
         ms(Src::ModWheel, Dst::Osc1Level,     0.22f),
         ms(Src::ModWheel, Dst::Osc3Level,     0.28f),
         ms(Src::ModWheel, Dst::PanSpread,     0.35f),
         ms(Src::ModWheel, Dst::ReverbMix,     0.06f),
         ms(Src::Lfo1,     Dst::FilterCutoff,  0.08f),
         ms(Src::Lfo2,     Dst::PanSpread,     0.14f)}
    });
}

} // namespace nova::synth
