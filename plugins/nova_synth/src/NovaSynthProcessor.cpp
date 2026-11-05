#include "NovaSynthProcessor.h"
#include "NovaSynthEditor.h"
#include <chrono>

namespace nova::synth {

NovaSynthProcessor::NovaSynthProcessor()
    : NovaProcessorBase(
          BusesProperties()
              .withOutput("Output", juce::AudioChannelSet::stereo(), true),
          createParameterLayout(),
          "Nova Synth") {
    registerFactoryPresets();
    syncParametersFromAPVTS();
}

juce::AudioProcessorValueTreeState::ParameterLayout NovaSynthProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"activeOscCount", 1}, "Active Source Layers", 1, 8, 4));

    const std::array<float, 8> defLevels  = { 0.78f, 0.52f, 0.40f, 0.55f, 0.45f, 0.40f, 0.35f, 0.35f };
    const std::array<int, 8>   defEngines = { 0, 1, 2, 0, 0, 0, 1, 2 };
    const std::array<int, 8>   defSourceTypes = { 0, 0, 0, 1, 2, 3, 0, 0 }; // 0: OSC, 1: SUB, 2: PLUCK, 3: NOISE
    const std::array<int, 8>   defOctaves = { 0, 1, 0, 0, -1, 1, 0, 1 };
    const std::array<bool, 8>  defEnabled = { true, true, true, false, false, false, false, false };

    for (int i = 1; i <= 8; ++i) {
        const juce::String prefix = "osc" + juce::String(i) + "_";
        const juce::String namePrefix = "OSC " + juce::String(i) + " ";
        const auto idx = static_cast<std::size_t>(i - 1);

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{prefix + "enabled", 1}, namePrefix + "Enabled", defEnabled[idx]));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "sourceType", 1}, namePrefix + "Source Type",
            juce::StringArray{"OSC (Hybrid)", "SUB (Sub Bass)", "PLUCK (Physical)", "NOISE (Transient)", "SAMPLE (Acoustic)"},
            defSourceTypes[idx]));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "engine", 1}, namePrefix + "Engine",
            juce::StringArray{"Warm Analog", "Evolving Wavetable", "Organic Texture", "Sample Playback"},
            defEngines[idx]));

        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{prefix + "model", 1}, namePrefix + "Model / Waveform", 0, 7, 0));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "filterRouting", 1}, namePrefix + "Filter Routing",
            juce::StringArray{"FLT 1", "FLT 2", "FLT 1+2", "DIRECT"}, 0));

        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{prefix + "octave", 1}, namePrefix + "Octave", -2, 2, defOctaves[idx]));

        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{prefix + "semi", 1}, namePrefix + "Semitones", -12, 12, 0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "shape", 1}, namePrefix + "Morph / Pos",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "detune", 1}, namePrefix + "Spray / Unison",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.32f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "pan", 1}, namePrefix + "Pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "level", 1}, namePrefix + "Level",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), defLevels[idx]));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "rate", 1}, namePrefix + "Rate",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.50f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "scatter", 1}, namePrefix + "Scatter",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "duration", 1}, namePrefix + "Duration",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.40f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "spread", 1}, namePrefix + "Spread",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.85f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "warp", 1}, namePrefix + "Warp",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{prefix + "lowestNoteOnly", 1}, namePrefix + "Lowest Note Only", false));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "ampEnvSource", 1}, namePrefix + "Amp Env Source",
            juce::StringArray{"Amp Env 1", "Filter Env 2", "Mod Env 3", "Aux Env 4"}, 0));
    }

    // Dual Filters & Vocal Carve
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"filter1Enabled", 1}, "Filter 1 Enabled", true));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"filterType", 1}, "Filter Type",
        juce::StringArray{"LOW 24 (Warm Ladder)", "BAND 12 (Formant)", "HIGH 24 (Air)"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"filterCutoffHz", 1}, "Filter Cutoff",
        juce::NormalisableRange<float>(60.0f, 18000.0f, 1.0f, 0.35f), 2600.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"filterResonance", 1}, "Filter Resonance",
        juce::NormalisableRange<float>(0.3f, 8.0f, 0.01f, 0.5f), 0.75f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"filterEnvAmount", 1}, "Filter Env Amt",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"filterDrive", 1}, "Analog Warmth Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.20f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"filter2Enabled", 1}, "Filter 2 Enabled", true));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"filter2Type", 1}, "Filter 2 Type",
        juce::StringArray{"LOW 24 (Warm Ladder)", "BAND 12 (Formant)", "HIGH 24 (Air)", "VOCAL CARVE (Bell Notch)"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"filter2CutoffHz", 1}, "Filter 2 Cutoff",
        juce::NormalisableRange<float>(60.0f, 18000.0f, 1.0f, 0.35f), 3600.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"filter2Resonance", 1}, "Filter 2 Resonance",
        juce::NormalisableRange<float>(0.3f, 8.0f, 0.01f, 0.5f), 0.70f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"filter2EnvAmount", 1}, "Filter 2 Env Amt",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"filter2Drive", 1}, "Filter 2 Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"vocalCarve", 1}, "Vocal Carve",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.28f));

    // Envelopes
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"ampDelay", 1}, "Amp Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f, 0.4f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"ampAttack", 1}, "Amp Attack",
        juce::NormalisableRange<float>(0.002f, 8.0f, 0.001f, 0.4f), 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"ampHold", 1}, "Amp Hold",
        juce::NormalisableRange<float>(0.0f, 3.0f, 0.001f, 0.4f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"ampDecay", 1}, "Amp Decay",
        juce::NormalisableRange<float>(0.02f, 10.0f, 0.01f, 0.45f), 1.40f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"ampSustain", 1}, "Amp Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.82f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"ampRelease", 1}, "Amp Release",
        juce::NormalisableRange<float>(0.02f, 12.0f, 0.01f, 0.45f), 2.20f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"ampVelocity", 1}, "Amp Velocity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"fltDelay", 1}, "Filter Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f, 0.4f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"fltAttack", 1}, "Filter Attack",
        juce::NormalisableRange<float>(0.002f, 8.0f, 0.001f, 0.4f), 0.65f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"fltHold", 1}, "Filter Hold",
        juce::NormalisableRange<float>(0.0f, 3.0f, 0.001f, 0.4f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"fltDecay", 1}, "Filter Decay",
        juce::NormalisableRange<float>(0.02f, 10.0f, 0.01f, 0.45f), 1.80f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"fltSustain", 1}, "Filter Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.40f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"fltRelease", 1}, "Filter Release",
        juce::NormalisableRange<float>(0.02f, 12.0f, 0.01f, 0.45f), 2.10f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"fltVelocity", 1}, "Filter Velocity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    // ENV 3 (Mod / Morph Envelope)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"modDelay", 1}, "Mod Env Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f, 0.4f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"modAttack", 1}, "Mod Env Attack",
        juce::NormalisableRange<float>(0.002f, 8.0f, 0.001f, 0.4f), 0.80f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"modHold", 1}, "Mod Env Hold",
        juce::NormalisableRange<float>(0.0f, 3.0f, 0.001f, 0.4f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"modDecay", 1}, "Mod Env Decay",
        juce::NormalisableRange<float>(0.02f, 10.0f, 0.01f, 0.45f), 2.20f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"modSustain", 1}, "Mod Env Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.50f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"modRelease", 1}, "Mod Env Release",
        juce::NormalisableRange<float>(0.02f, 12.0f, 0.01f, 0.45f), 2.50f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"modVelocity", 1}, "Mod Env Velocity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    // ENV 4 (Pitch / Aux Envelope)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"auxDelay", 1}, "Aux Env Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f, 0.4f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"auxAttack", 1}, "Aux Env Attack",
        juce::NormalisableRange<float>(0.002f, 8.0f, 0.001f, 0.4f), 0.20f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"auxHold", 1}, "Aux Env Hold",
        juce::NormalisableRange<float>(0.0f, 3.0f, 0.001f, 0.4f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"auxDecay", 1}, "Aux Env Decay",
        juce::NormalisableRange<float>(0.02f, 10.0f, 0.01f, 0.45f), 0.80f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"auxSustain", 1}, "Aux Env Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.30f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"auxRelease", 1}, "Aux Env Release",
        juce::NormalisableRange<float>(0.02f, 12.0f, 0.01f, 0.45f), 1.50f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"auxVelocity", 1}, "Aux Env Velocity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    // LFOs
    const juce::StringArray lfoShapeChoices{"Sine", "Triangle", "Saw Up", "Smooth Drift", "Pulse Gate"};
    const juce::StringArray lfoSyncDivChoices{
        "4 Bars", "2 Bars", "1 Bar", "1/2 D", "1/2", "1/4 D", "1/4", "1/8 D", "1/8", "1/8 T", "1/16", "1/32"
    };

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lfo1RateHz", 1}, "LFO 1 Rate",
        juce::NormalisableRange<float>(0.02f, 12.0f, 0.01f, 0.45f), 0.42f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lfo1Depth", 1}, "LFO 1 Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"lfo1Shape", 1}, "LFO 1 Shape", lfoShapeChoices, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"lfo1Sync", 1}, "LFO 1 Sync", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"lfo1SyncDiv", 1}, "LFO 1 Sync Division", lfoSyncDivChoices, 6)); // 1/4

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lfo2RateHz", 1}, "LFO 2 Drift Rate",
        juce::NormalisableRange<float>(0.02f, 12.0f, 0.01f, 0.45f), 0.18f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lfo2Depth", 1}, "LFO 2 Drift Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"lfo2Shape", 1}, "LFO 2 Shape", lfoShapeChoices, 3));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"lfo2Sync", 1}, "LFO 2 Sync", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"lfo2SyncDiv", 1}, "LFO 2 Sync Division", lfoSyncDivChoices, 7)); // 1/8 D

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lfo3RateHz", 1}, "LFO 3 Rate",
        juce::NormalisableRange<float>(0.02f, 12.0f, 0.01f, 0.45f), 1.20f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lfo3Depth", 1}, "LFO 3 Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"lfo3Shape", 1}, "LFO 3 Shape", lfoShapeChoices, 1));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"lfo3Sync", 1}, "LFO 3 Sync", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"lfo3SyncDiv", 1}, "LFO 3 Sync Division", lfoSyncDivChoices, 8)); // 1/8

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lfo4RateHz", 1}, "LFO 4 Rate",
        juce::NormalisableRange<float>(0.02f, 12.0f, 0.01f, 0.45f), 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lfo4Depth", 1}, "LFO 4 Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"lfo4Shape", 1}, "LFO 4 Shape", lfoShapeChoices, 4));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"lfo4Sync", 1}, "LFO 4 Sync", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"lfo4SyncDiv", 1}, "LFO 4 Sync Division", lfoSyncDivChoices, 2)); // 1 Bar

    // 4 Worship Macros
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"macroWarmth", 1}, "Warmth Macro",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.50f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"macroMotion", 1}, "Motion Macro",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"macroShimmer", 1}, "Shimmer Macro",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.40f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"macroAir", 1}, "Air Macro",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.30f));

    // Worship Drone Generator
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"droneEnabled", 1}, "Drone Pad Hold", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"droneRootKey", 1}, "Drone Root Key",
        juce::StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"droneLevel", 1}, "Drone Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"droneVoicing", 1}, "Drone Harmonic Voicing",
        juce::StringArray{"Lush 5th", "Deep Sub", "Ambient 9th", "Celestial Octaves"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"droneOctave", 1}, "Drone Octave", -2, 2, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"droneTone", 1}, "Drone Tone / Warmth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.70f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"droneGlide", 1}, "Drone Glide Time",
        juce::NormalisableRange<float>(0.1f, 5.0f, 0.05f), 1.50f));

    // Tempo & BPM
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"bpm", 1}, "Tempo BPM",
        juce::NormalisableRange<float>(40.0f, 240.0f, 0.1f), 120.0f));

    // Arpeggiator
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"arpEnabled", 1}, "Arpeggiator Enable", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"arpMode", 1}, "Arp Mode",
        juce::StringArray{"Up", "Down", "Up / Down", "Down / Up", "Random", "Worship Chord Pulse"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"arpSync", 1}, "Arp Tempo Sync", true));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"arpSyncDiv", 1}, "Arp Rate Sync",
        juce::StringArray{"1/4", "1/8", "1/8d", "1/8t", "1/16", "1/16d", "1/32"}, 4)); // default 1/16
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"arpRateHz", 1}, "Arp Rate",
        juce::NormalisableRange<float>(1.0f, 16.0f, 0.1f), 4.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"arpGate", 1}, "Arp Gate",
        juce::NormalisableRange<float>(0.05f, 1.00f, 0.01f), 0.65f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"arpOctaves", 1}, "Arp Octave Range",
        juce::StringArray{"1 Octave", "2 Octaves", "3 Octaves", "4 Octaves"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"arpLatch", 1}, "Arp Latch / Hold", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"arpSwing", 1}, "Arp Swing / Groove",
        juce::NormalisableRange<float>(0.0f, 0.75f, 0.01f), 0.0f));

    // Arp Pattern Length & Step Matrix (16, 32, or 64 steps)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"arpPatternLength", 1}, "Arp Pattern Length",
        juce::StringArray{"16 Steps", "32 Steps", "64 Steps"}, 0));

    for (int s = 1; s <= 64; ++s) {
        const float defaultVel = (s % 4 == 1) ? 0.95f : ((s % 2 == 1) ? 0.80f : 0.65f);
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"arpStepGate_" + juce::String(s), 1},
            "Arp Step " + juce::String(s) + " Gate", true));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"arpStepVel_" + juce::String(s), 1},
            "Arp Step " + juce::String(s) + " Velocity",
            juce::NormalisableRange<float>(0.05f, 1.0f, 0.01f), defaultVel));
    }

    // Embedded Stereo Tape / Ping-Pong Delay
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"delayEnabled", 1}, "Delay Enable", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"delaySync", 1}, "Delay Tempo Sync", true));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"delaySyncDiv", 1}, "Delay Time Sync",
        juce::StringArray{"1/2", "1/4", "1/4d", "1/8", "1/8d", "1/8t", "1/16", "1/16d"}, 4)); // default 1/8d (Dotted 8th!)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"delayMix", 1}, "Delay Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.28f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"delayTimeSec", 1}, "Delay Time",
        juce::NormalisableRange<float>(0.05f, 1.5f, 0.01f), 0.375f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"delayFeedback", 1}, "Delay Feedback",
        juce::NormalisableRange<float>(0.0f, 0.88f, 0.01f), 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"delaySwing", 1}, "Delay Ping-Pong Swing",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"subLowestOnly", 1}, "Sub Lowest Note Only", true));

    // Embedded Nova Space Reverb & Master
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverbDry", 1}, "Space Dry",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverbMix", 1}, "Space Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.42f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverbDecay", 1}, "Space Decay",
        juce::NormalisableRange<float>(0.5f, 25.0f, 0.1f, 0.45f), 6.5f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"reverbShimmerPitch", 1}, "Shimmer Pitch",
        juce::StringArray{"+1 Octave", "+2 Octaves", "+1 & +2 Oct", "-1 Octave"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverbShimmer", 1}, "Octave Shimmer",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.42f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverbDucking", 1}, "Tail Ducking",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"reverbFreeze", 1}, "Freeze Space", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"masterGainDb", 1}, "Master Output",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

void NovaSynthProcessor::registerFactoryPresets() {
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
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.25f}, {"osc1_detune", 0.35f}, {"osc1_level", 0.72f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 1.0f}, {"osc2_model", 7.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.45f}, {"osc2_detune", 0.35f}, {"osc2_level", 0.55f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 2.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.40f}, {"osc3_detune", 0.32f}, {"osc3_level", 0.45f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 1.0f}, {"osc4_engine", 4.0f}, {"osc4_model", 0.0f}, {"osc4_octave", -1.0f}, {"osc4_shape", 0.05f}, {"osc4_detune", 0.0f}, {"osc4_level", 0.48f},
         {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1100.0f}, {"filterResonance", 1.15f}, {"filterEnvAmount", 0.22f}, {"filterDrive", 0.12f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 160.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.30f},
         {"ampAttack", 0.85f}, {"ampDecay", 2.8f}, {"ampSustain", 0.88f}, {"ampRelease", 3.8f},
         {"fltAttack", 0.95f}, {"fltDecay", 2.5f}, {"fltSustain", 0.50f}, {"fltRelease", 3.2f},
         {"macroWarmth", 0.58f}, {"macroMotion", 0.50f}, {"macroShimmer", 0.65f}, {"macroAir", 0.45f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.24f}, {"delayFeedback", 0.38f},
         {"reverbMix", 0.52f}, {"reverbDry", 1.0f}, {"reverbDecay", 9.5f}, {"reverbShimmer", 0.65f}, {"reverbShimmerPitch", 0.0f}, {"reverbDucking", 0.30f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 2. Angelic Celestial Harp & Choir
    presetManager_.registerFactoryPreset(PresetEntry{
        "Angelic Celestial Harp & Choir", "Shimmer & Celestial",
        "Acoustic harp pluck floating over an organic choir breath swell and dual-octave shimmer cloud.",
        {"Harp", "Celestial", "Choir", "Shimmer", "Ambient"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 1.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.60f}, {"osc1_detune", 0.20f}, {"osc1_level", 0.80f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 2.0f}, {"osc2_model", 0.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.45f}, {"osc2_detune", 0.35f}, {"osc2_level", 0.58f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 1.0f}, {"osc3_model", 3.0f}, {"osc3_octave", 1.0f}, {"osc3_shape", 0.40f}, {"osc3_detune", 0.25f}, {"osc3_level", 0.45f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 3.0f}, {"osc4_engine", 5.0f}, {"osc4_model", 2.0f}, {"osc4_octave", 0.0f}, {"osc4_shape", 0.65f}, {"osc4_detune", 0.35f}, {"osc4_level", 0.16f},
         {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1450.0f}, {"filterResonance", 1.05f}, {"filterEnvAmount", 0.20f}, {"filterDrive", 0.10f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 180.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.25f},
         {"ampAttack", 0.02f}, {"ampDecay", 2.5f}, {"ampSustain", 0.70f}, {"ampRelease", 3.6f},
         {"fltAttack", 0.25f}, {"fltDecay", 2.2f}, {"fltSustain", 0.50f}, {"fltRelease", 3.0f},
         {"macroWarmth", 0.52f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.70f}, {"macroAir", 0.48f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 2.0f}, {"delayMix", 0.28f}, {"delayFeedback", 0.42f},
         {"reverbMix", 0.56f}, {"reverbDry", 1.0f}, {"reverbDecay", 11.5f}, {"reverbShimmer", 0.70f}, {"reverbShimmerPitch", 2.0f}, {"reverbDucking", 0.25f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 3. Ethereal Morning Dawn
    presetManager_.registerFactoryPreset(PresetEntry{
        "Ethereal Morning Dawn", "Shimmer & Celestial",
        "Airy wind and swell texture floating through a slow morphing worship harmonics shimmer wash.",
        {"Dawn", "Morning", "Choir", "Texture", "Ambient"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 2.0f}, {"osc1_model", 4.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.55f}, {"osc1_detune", 0.35f}, {"osc1_level", 0.75f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 1.0f}, {"osc2_model", 0.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.60f}, {"osc2_detune", 0.30f}, {"osc2_level", 0.55f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 3.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.10f}, {"osc3_detune", 0.20f}, {"osc3_level", 0.50f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 1.0f}, {"osc4_engine", 4.0f}, {"osc4_model", 1.0f}, {"osc4_octave", -1.0f}, {"osc4_shape", 0.10f}, {"osc4_detune", 0.0f}, {"osc4_level", 0.45f},
         {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 980.0f}, {"filterResonance", 0.95f}, {"filterEnvAmount", 0.24f}, {"filterDrive", 0.10f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 1.2f}, {"ampDecay", 3.2f}, {"ampSustain", 0.85f}, {"ampRelease", 4.2f},
         {"fltAttack", 1.4f}, {"fltDecay", 3.0f}, {"fltSustain", 0.55f}, {"fltRelease", 3.5f},
         {"macroWarmth", 0.55f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.72f}, {"macroAir", 0.50f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 2.0f}, {"delayMix", 0.28f}, {"delayFeedback", 0.42f},
         {"reverbMix", 0.58f}, {"reverbDry", 1.0f}, {"reverbDecay", 13.0f}, {"reverbShimmer", 0.72f}, {"reverbShimmerPitch", 2.0f}, {"reverbDucking", 0.25f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 4. Cathedral Prayer Drone (C)
    presetManager_.registerFactoryPreset(PresetEntry{
        "Cathedral Prayer Drone (C)", "Shimmer & Celestial",
        "Self-sustaining warm Root + Fifth + Octave worship bed in C with cello texture and deep sine sub.",
        {"Drone", "Prayer", "Infinite", "Pad"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 1.0f}, {"osc1_engine", 4.0f}, {"osc1_model", 0.0f}, {"osc1_octave", -2.0f}, {"osc1_shape", 0.20f}, {"osc1_detune", 0.0f}, {"osc1_level", 0.75f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", -1.0f}, {"osc2_shape", 0.25f}, {"osc2_detune", 0.25f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 1.0f}, {"osc3_model", 4.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.40f}, {"osc3_detune", 0.30f}, {"osc3_level", 0.52f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 0.0f}, {"osc4_engine", 2.0f}, {"osc4_model", 1.0f}, {"osc4_octave", 0.0f}, {"osc4_shape", 0.45f}, {"osc4_detune", 0.30f}, {"osc4_level", 0.42f},
         {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 720.0f}, {"filterResonance", 1.10f}, {"filterEnvAmount", 0.15f}, {"filterDrive", 0.15f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 0.0f}, {"filter2CutoffHz", 5000.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.30f},
         {"ampAttack", 1.20f}, {"ampDecay", 2.5f}, {"ampSustain", 0.90f}, {"ampRelease", 4.5f},
         {"macroWarmth", 0.65f}, {"macroMotion", 0.30f}, {"macroShimmer", 0.48f}, {"macroAir", 0.25f},
         {"delayEnabled", 0.0f},
         {"reverbMix", 0.55f}, {"reverbDry", 1.0f}, {"reverbDecay", 12.0f}, {"reverbShimmer", 0.50f}, {"reverbDucking", 0.20f},
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
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.15f}, {"osc1_detune", 0.35f}, {"osc1_level", 0.80f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.50f}, {"osc2_detune", 0.38f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 1.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.10f}, {"osc3_detune", 0.0f}, {"osc3_level", 0.55f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 820.0f}, {"filterResonance", 1.45f}, {"filterEnvAmount", 0.32f}, {"filterDrive", 0.22f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 110.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.20f},
         {"ampAttack", 0.65f}, {"ampDecay", 2.2f}, {"ampSustain", 0.85f}, {"ampRelease", 2.6f},
         {"fltAttack", 0.75f}, {"fltDecay", 2.0f}, {"fltSustain", 0.45f}, {"fltRelease", 2.4f},
         {"macroWarmth", 0.68f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.12f}, {"macroAir", 0.25f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.18f}, {"delayFeedback", 0.30f},
         {"reverbMix", 0.35f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.8f}, {"reverbShimmer", 0.10f}, {"reverbDucking", 0.20f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 6. Elevation Velvet Brass Swell
    presetManager_.registerFactoryPreset(PresetEntry{
        "Elevation Velvet Brass Swell", "Warm Analog Pads",
        "Expressive vintage poly-brass filter swell with analog voice drift and warm saturation.",
        {"Brass", "Swell", "Analog", "Elevation"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.20f}, {"osc1_detune", 0.42f}, {"osc1_level", 0.82f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 4.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.30f}, {"osc2_detune", 0.45f}, {"osc2_level", 0.68f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 1.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.45f}, {"osc3_detune", 0.22f}, {"osc3_level", 0.50f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 520.0f}, {"filterResonance", 1.60f}, {"filterEnvAmount", 0.60f}, {"filterDrive", 0.32f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 0.0f}, {"filter2CutoffHz", 4800.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.20f},
         {"ampAttack", 0.50f}, {"ampDecay", 2.2f}, {"ampSustain", 0.88f}, {"ampRelease", 2.4f},
         {"fltAttack", 0.95f}, {"fltDecay", 2.5f}, {"fltSustain", 0.45f}, {"fltRelease", 2.4f},
         {"macroWarmth", 0.65f}, {"macroMotion", 0.40f}, {"macroShimmer", 0.22f}, {"macroAir", 0.30f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.20f}, {"delayFeedback", 0.35f},
         {"reverbMix", 0.38f}, {"reverbDry", 1.0f}, {"reverbDecay", 5.5f}, {"reverbShimmer", 0.20f}, {"reverbDucking", 0.25f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 7. Vintage String Ensemble
    presetManager_.registerFactoryPreset(PresetEntry{
        "Vintage String Ensemble", "Warm Analog Pads",
        "Lush 70s analog string machine layer with silky high end, slow gentle attack, and wide ensemble width.",
        {"Strings", "Ensemble", "Vintage", "Solina", "Analog"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.20f}, {"osc1_detune", 0.52f}, {"osc1_level", 0.78f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 1.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.40f}, {"osc2_detune", 0.42f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 2.0f}, {"osc3_model", 1.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.45f}, {"osc3_detune", 0.32f}, {"osc3_level", 0.50f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1250.0f}, {"filterResonance", 0.90f}, {"filterEnvAmount", 0.25f}, {"filterDrive", 0.18f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 140.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.20f},
         {"ampAttack", 0.75f}, {"ampDecay", 2.8f}, {"ampSustain", 0.90f}, {"ampRelease", 2.8f},
         {"fltAttack", 0.85f}, {"fltDecay", 2.6f}, {"fltSustain", 0.60f}, {"fltRelease", 2.5f},
         {"macroWarmth", 0.65f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.15f}, {"macroAir", 0.30f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.18f}, {"delayFeedback", 0.30f},
         {"reverbMix", 0.42f}, {"reverbDry", 1.0f}, {"reverbDecay", 5.5f}, {"reverbShimmer", 0.15f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 8. Cinematic Dark Swell
    presetManager_.registerFactoryPreset(PresetEntry{
        "Cinematic Dark Swell", "Warm Analog Pads",
        "Deep, menacing cinematic swell with heavy sub-bass weight, slow sweeping filter, and cavernous space.",
        {"Cinematic", "Dark", "Swell", "Sub", "Heavy"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", -1.0f}, {"osc1_shape", 0.25f}, {"osc1_detune", 0.38f}, {"osc1_level", 0.82f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 1.0f}, {"osc2_model", 6.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.55f}, {"osc2_detune", 0.30f}, {"osc2_level", 0.62f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 2.0f}, {"osc3_octave", -2.0f}, {"osc3_shape", 0.25f}, {"osc3_detune", 0.0f}, {"osc3_level", 0.75f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 460.0f}, {"filterResonance", 1.55f}, {"filterEnvAmount", 0.50f}, {"filterDrive", 0.38f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 1.1f}, {"ampDecay", 3.0f}, {"ampSustain", 0.85f}, {"ampRelease", 3.5f},
         {"fltAttack", 1.4f}, {"fltDecay", 2.8f}, {"fltSustain", 0.45f}, {"fltRelease", 3.0f},
         {"macroWarmth", 0.70f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.20f}, {"macroAir", 0.25f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.25f}, {"delayFeedback", 0.40f},
         {"reverbMix", 0.52f}, {"reverbDry", 1.0f}, {"reverbDecay", 10.0f}, {"reverbShimmer", 0.20f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // =========================================================================
    // 3. BASSES & FOUNDATION
    // =========================================================================

    // 9. Moog Analog Punch Bass
    presetManager_.registerFactoryPreset(PresetEntry{
        "Moog Analog Punch Bass", "Basses & Foundation",
        "Classic 24dB warm ladder filter, driven sub + 2-voice analog saw, snappy envelope with rich low-end punch.",
        {"Moog", "Bass", "Analog", "Punch", "Warm"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.20f}, {"osc1_detune", 0.25f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.40f}, {"osc2_detune", 0.20f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 0.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.0f}, {"osc3_level", 0.75f},
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
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 1.0f}, {"osc1_engine", 4.0f}, {"osc1_model", 2.0f}, {"osc1_octave", -1.0f}, {"osc1_shape", 0.45f}, {"osc1_detune", 0.0f}, {"osc1_level", 0.95f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.30f}, {"osc2_detune", 0.0f}, {"osc2_level", 0.50f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 3.0f}, {"osc3_engine", 5.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.40f}, {"osc3_detune", 0.0f}, {"osc3_level", 0.15f},
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
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.20f}, {"osc1_detune", 0.52f}, {"osc1_level", 0.82f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 0.0f}, {"osc2_octave", 0.0f}, {"osc2_semi", 0.0f}, {"osc2_shape", 0.35f}, {"osc2_detune", 0.62f}, {"osc2_level", 0.78f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 0.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.10f}, {"osc3_detune", 0.0f}, {"osc3_level", 0.75f},
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
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.80f}, {"osc1_detune", 0.15f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.60f}, {"osc2_detune", 0.25f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 1.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.25f}, {"osc3_detune", 0.0f}, {"osc3_level", 0.70f},
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
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.10f}, {"osc1_detune", 0.15f}, {"osc1_level", 0.88f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.50f}, {"osc2_detune", 0.20f}, {"osc2_level", 0.55f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 0.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.15f}, {"osc3_detune", 0.0f}, {"osc3_level", 0.55f},
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
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 1.0f}, {"osc1_engine", 4.0f}, {"osc1_model", 2.0f}, {"osc1_octave", -1.0f}, {"osc1_shape", 0.45f}, {"osc1_detune", 0.0f}, {"osc1_level", 0.90f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.25f}, {"osc2_detune", 0.25f}, {"osc2_level", 0.60f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 1.0f}, {"osc3_model", 6.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.35f}, {"osc3_detune", 0.30f}, {"osc3_level", 0.48f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 3.0f}, {"osc4_engine", 5.0f}, {"osc4_model", 2.0f}, {"osc4_octave", 0.0f}, {"osc4_shape", 0.70f}, {"osc4_detune", 0.50f}, {"osc4_level", 0.22f},
         {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1050.0f}, {"filterResonance", 0.85f}, {"filterEnvAmount", 0.30f}, {"filterDrive", 0.28f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 0.0f}, {"filter2CutoffHz", 4500.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.25f},
         {"ampAttack", 0.55f}, {"ampDecay", 2.2f}, {"ampSustain", 0.85f}, {"ampRelease", 2.8f},
         {"macroWarmth", 0.65f}, {"macroMotion", 0.25f}, {"macroShimmer", 0.25f}, {"macroAir", 0.35f},
         {"delayEnabled", 0.0f},
         {"reverbMix", 0.38f}, {"reverbDry", 1.0f}, {"reverbDecay", 6.5f}, {"reverbShimmer", 0.22f}, {"reverbDucking", 0.30f},
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
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.72f}, {"osc1_detune", 0.18f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 1.0f}, {"osc2_model", 5.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.60f}, {"osc2_detune", 0.22f}, {"osc2_level", 0.50f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 2.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.10f}, {"osc3_level", 0.40f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 1.0f}, {"filterCutoffHz", 2600.0f}, {"filterResonance", 1.25f}, {"filterEnvAmount", 0.55f}, {"filterDrive", 0.20f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 3.0f}, {"filter2CutoffHz", 2200.0f}, {"filter2Resonance", 0.85f}, {"vocalCarve", 0.35f},
         {"ampAttack", 0.003f}, {"ampDecay", 0.95f}, {"ampSustain", 0.25f}, {"ampRelease", 1.2f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.65f}, {"fltSustain", 0.20f}, {"fltRelease", 1.1f},
         {"macroWarmth", 0.48f}, {"macroMotion", 0.40f}, {"macroShimmer", 0.40f}, {"macroAir", 0.48f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.38f}, {"delayFeedback", 0.48f},
         {"reverbMix", 0.35f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.8f}, {"reverbShimmer", 0.30f}, {"reverbDucking", 0.28f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 16. Glass Kalimba / Bell
    presetManager_.registerFactoryPreset(PresetEntry{
        "Glass Kalimba / Bell", "Plucks & Mallets",
        "Crystalline, pure glass kalimba with delicate acoustic body and wide shimmer reverb halo.",
        {"Kalimba", "Bell", "Glass", "Shimmer", "Pluck"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 1.0f}, {"osc1_octave", 1.0f}, {"osc1_shape", 0.82f}, {"osc1_detune", 0.20f}, {"osc1_level", 0.82f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 1.0f}, {"osc2_model", 3.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.70f}, {"osc2_detune", 0.25f}, {"osc2_level", 0.58f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 3.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.15f}, {"osc3_detune", 0.10f}, {"osc3_level", 0.42f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 3400.0f}, {"filterResonance", 0.95f}, {"filterEnvAmount", 0.40f}, {"filterDrive", 0.12f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 160.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.20f},
         {"ampAttack", 0.002f}, {"ampDecay", 1.2f}, {"ampSustain", 0.12f}, {"ampRelease", 1.8f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.80f}, {"fltSustain", 0.10f}, {"fltRelease", 1.5f},
         {"macroWarmth", 0.45f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.52f}, {"macroAir", 0.50f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.35f}, {"delayFeedback", 0.48f},
         {"reverbMix", 0.48f}, {"reverbDry", 1.0f}, {"reverbDecay", 6.5f}, {"reverbShimmer", 0.45f}, {"reverbDucking", 0.25f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 17. Punchy Marimba Mallet
    presetManager_.registerFactoryPreset(PresetEntry{
        "Punchy Marimba Mallet", "Plucks & Mallets",
        "Natural wooden marimba bar strike with warm fundamental, snappy transient, and fast acoustic decay.",
        {"Marimba", "Mallet", "Wood", "Organic", "Acoustic"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.60f}, {"osc1_detune", 0.15f}, {"osc1_level", 0.88f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.25f}, {"osc2_detune", 0.12f}, {"osc2_level", 0.62f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 3.0f}, {"osc3_engine", 5.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.40f}, {"osc3_detune", 0.20f}, {"osc3_level", 0.22f},
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
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.75f}, {"osc1_detune", 0.15f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 0.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.30f}, {"osc2_detune", 0.10f}, {"osc2_level", 0.45f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 3.0f}, {"osc3_engine", 5.0f}, {"osc3_model", 1.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.50f}, {"osc3_detune", 0.30f}, {"osc3_level", 0.20f},
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
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.40f}, {"osc1_detune", 0.20f}, {"osc1_level", 0.80f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 2.0f}, {"osc2_engine", 3.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.50f}, {"osc2_detune", 0.15f}, {"osc2_level", 0.50f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 1.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.30f}, {"osc3_detune", 0.15f}, {"osc3_level", 0.45f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1750.0f}, {"filterResonance", 0.90f}, {"filterEnvAmount", 0.45f}, {"filterDrive", 0.22f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.004f}, {"ampDecay", 1.0f}, {"ampSustain", 0.25f}, {"ampRelease", 0.70f},
         {"fltAttack", 0.003f}, {"fltDecay", 0.75f}, {"fltSustain", 0.18f}, {"fltRelease", 0.60f},
         {"macroWarmth", 0.70f}, {"macroMotion", 0.35f}, {"macroShimmer", 0.12f}, {"macroAir", 0.28f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 2.0f}, {"delayMix", 0.22f}, {"delayFeedback", 0.35f},
         {"reverbMix", 0.32f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.6f}, {"reverbShimmer", 0.15f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 20. EDM Staccato Pluck
    presetManager_.registerFactoryPreset(PresetEntry{
        "EDM Staccato Pluck", "Plucks & Mallets",
        "Ultra-tight, punchy supersaw pluck with razor-sharp attack, ready for club and pop anthems.",
        {"EDM", "Pluck", "Staccato", "SuperSaw", "Bright"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.25f}, {"osc1_detune", 0.60f}, {"osc1_level", 0.82f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.45f}, {"osc2_detune", 0.50f}, {"osc2_level", 0.62f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 0.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.15f}, {"osc3_detune", 0.0f}, {"osc3_level", 0.52f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 3100.0f}, {"filterResonance", 1.35f}, {"filterEnvAmount", 0.70f}, {"filterDrive", 0.30f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.002f}, {"ampDecay", 0.32f}, {"ampSustain", 0.0f}, {"ampRelease", 0.15f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.20f}, {"fltSustain", 0.0f}, {"fltRelease", 0.12f},
         {"macroWarmth", 0.45f}, {"macroMotion", 0.40f}, {"macroShimmer", 0.18f}, {"macroAir", 0.45f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.32f}, {"delayFeedback", 0.40f},
         {"reverbMix", 0.28f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.0f}, {"reverbShimmer", 0.18f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // =========================================================================
    // 5. ARPS & SEQUENCES
    // =========================================================================

    // 21. Sunday Morning Arp Pulse
    presetManager_.registerFactoryPreset(PresetEntry{
        "Sunday Morning Arp Pulse", "Arps & Sequences",
        "Driving 1/16 rhythmic worship arpeggiator with physical mallet bite and tight low end.",
        {"Arp", "Pulse", "Pluck", "Motion", "Rhythm"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.65f}, {"osc1_detune", 0.18f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.35f}, {"osc2_detune", 0.25f}, {"osc2_level", 0.55f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 3.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.10f}, {"osc3_level", 0.50f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 2600.0f}, {"filterResonance", 1.10f}, {"filterEnvAmount", 0.52f}, {"filterDrive", 0.20f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 0.0f}, {"filter2CutoffHz", 8000.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.25f},
         {"ampAttack", 0.003f}, {"ampDecay", 0.45f}, {"ampSustain", 0.25f}, {"ampRelease", 0.85f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.38f}, {"fltSustain", 0.15f}, {"fltRelease", 0.65f},
         {"macroWarmth", 0.48f}, {"macroMotion", 0.50f}, {"macroShimmer", 0.35f}, {"macroAir", 0.42f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.35f}, {"delayFeedback", 0.45f},
         {"reverbMix", 0.36f}, {"reverbDry", 1.0f}, {"reverbDecay", 5.0f}, {"reverbShimmer", 0.32f}, {"reverbDucking", 0.35f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 1.0f}, {"arpSync", 1.0f}, {"arpSyncDiv", 4.0f}, {"arpMode", 2.0f}, {"arpGate", 0.58f}, {"subLowestOnly", 1.0f}}
    });

    // 22. Celestial 1/8d Halo Arp
    presetManager_.registerFactoryPreset(PresetEntry{
        "Celestial 1/8d Halo Arp", "Arps & Sequences",
        "Flowing dotted-8th worship arpeggio swimming through an angelic shimmer reverb cloud.",
        {"Celestial", "Halo", "Arp", "Dotted", "Worship"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 1.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.55f}, {"osc1_detune", 0.25f}, {"osc1_level", 0.80f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 2.0f}, {"osc2_engine", 3.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.75f}, {"osc2_detune", 0.18f}, {"osc2_level", 0.60f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.15f}, {"osc3_level", 0.45f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 2400.0f}, {"filterResonance", 1.10f}, {"filterEnvAmount", 0.45f}, {"filterDrive", 0.18f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 2.0f}, {"filter2CutoffHz", 200.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.25f},
         {"ampAttack", 0.003f}, {"ampDecay", 0.60f}, {"ampSustain", 0.20f}, {"ampRelease", 0.90f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.45f}, {"fltSustain", 0.15f}, {"fltRelease", 0.70f},
         {"macroWarmth", 0.48f}, {"macroMotion", 0.55f}, {"macroShimmer", 0.65f}, {"macroAir", 0.50f},
         {"arpEnabled", 1.0f}, {"arpSync", 1.0f}, {"arpSyncDiv", 2.0f}, {"arpMode", 0.0f}, {"arpGate", 0.60f}, {"arpOctaves", 1.0f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.35f}, {"delayFeedback", 0.48f},
         {"reverbMix", 0.48f}, {"reverbDry", 1.0f}, {"reverbDecay", 8.0f}, {"reverbShimmer", 0.58f}, {"reverbShimmerPitch", 0.0f}, {"reverbDucking", 0.28f},
         {"droneEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 23. 1/8 Triplet Ambient Sequence
    presetManager_.registerFactoryPreset(PresetEntry{
        "1/8 Triplet Ambient Sequence", "Arps & Sequences",
        "Dreamy 1/8 triplet polyphonic sequence with soft analog filter sweeps and lush stereo space.",
        {"Triplet", "Ambient", "Sequence", "Fluid", "Space"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.25f}, {"osc1_detune", 0.35f}, {"osc1_level", 0.75f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.30f}, {"osc2_detune", 0.15f}, {"osc2_level", 0.60f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 2.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.50f}, {"osc3_detune", 0.30f}, {"osc3_level", 0.40f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1950.0f}, {"filterResonance", 1.05f}, {"filterEnvAmount", 0.40f}, {"filterDrive", 0.20f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.005f}, {"ampDecay", 0.75f}, {"ampSustain", 0.25f}, {"ampRelease", 1.1f},
         {"fltAttack", 0.004f}, {"fltDecay", 0.55f}, {"fltSustain", 0.20f}, {"fltRelease", 0.90f},
         {"macroWarmth", 0.55f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.35f}, {"macroAir", 0.38f},
         {"arpEnabled", 1.0f}, {"arpSync", 1.0f}, {"arpSyncDiv", 3.0f}, {"arpMode", 2.0f}, {"arpGate", 0.70f}, {"arpOctaves", 0.0f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 3.0f}, {"delayMix", 0.28f}, {"delayFeedback", 0.40f},
         {"reverbMix", 0.42f}, {"reverbDry", 1.0f}, {"reverbDecay", 5.5f}, {"reverbShimmer", 0.28f}, {"reverbDucking", 0.20f},
         {"droneEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 24. Dark Cyberwave Pulse
    presetManager_.registerFactoryPreset(PresetEntry{
        "Dark Cyberwave Pulse", "Arps & Sequences",
        "Aggressive 1/16 driving synthwave bassline/lead sequence with high filter drive and punchy saturation.",
        {"Cyberwave", "Synthwave", "Pulse", "Dark", "Driving"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.20f}, {"osc1_detune", 0.45f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", -1.0f}, {"osc2_shape", 0.60f}, {"osc2_detune", 0.30f}, {"osc2_level", 0.70f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 0.0f}, {"osc3_octave", -2.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.0f}, {"osc3_level", 0.65f},
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
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.30f}, {"osc1_detune", 0.65f}, {"osc1_level", 0.82f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 0.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.20f}, {"osc2_detune", 0.55f}, {"osc2_level", 0.62f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 0.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.10f}, {"osc3_detune", 0.0f}, {"osc3_level", 0.52f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 3200.0f}, {"filterResonance", 1.25f}, {"filterEnvAmount", 0.58f}, {"filterDrive", 0.22f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.003f}, {"ampDecay", 0.45f}, {"ampSustain", 0.25f}, {"ampRelease", 0.35f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.32f}, {"fltSustain", 0.18f}, {"fltRelease", 0.28f},
         {"macroWarmth", 0.45f}, {"macroMotion", 0.60f}, {"macroShimmer", 0.28f}, {"macroAir", 0.48f},
         {"arpEnabled", 1.0f}, {"arpSync", 1.0f}, {"arpSyncDiv", 4.0f}, {"arpMode", 2.0f}, {"arpGate", 0.65f}, {"arpOctaves", 2.0f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.32f}, {"delayFeedback", 0.42f},
         {"reverbMix", 0.35f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.5f}, {"reverbShimmer", 0.22f},
         {"droneEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 26. Clockwork Mallet 1/32
    presetManager_.registerFactoryPreset(PresetEntry{
        "Clockwork Mallet 1/32", "Arps & Sequences",
        "Rapid, intricate 1/32 staccato clockwork sequence with crisp physical mallet transients.",
        {"Clockwork", "Micro-Arp", "Staccato", "Intricate", "Fast"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.70f}, {"osc1_detune", 0.15f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.20f}, {"osc2_detune", 0.12f}, {"osc2_level", 0.55f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 3.0f}, {"osc3_engine", 5.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.35f}, {"osc3_detune", 0.20f}, {"osc3_level", 0.20f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 2800.0f}, {"filterResonance", 1.15f}, {"filterEnvAmount", 0.52f}, {"filterDrive", 0.18f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.002f}, {"ampDecay", 0.35f}, {"ampSustain", 0.10f}, {"ampRelease", 0.20f},
         {"fltAttack", 0.002f}, {"fltDecay", 0.25f}, {"fltSustain", 0.08f}, {"fltRelease", 0.15f},
         {"macroWarmth", 0.48f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.20f}, {"macroAir", 0.40f},
         {"arpEnabled", 1.0f}, {"arpSync", 1.0f}, {"arpSyncDiv", 6.0f}, {"arpMode", 4.0f}, {"arpGate", 0.40f}, {"arpOctaves", 1.0f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.28f}, {"delayFeedback", 0.38f},
         {"reverbMix", 0.30f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.2f}, {"reverbShimmer", 0.15f},
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
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 5.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.45f}, {"osc1_detune", 0.35f}, {"osc1_level", 0.82f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 4.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.30f}, {"osc2_detune", 0.45f}, {"osc2_level", 0.68f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 3.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.25f}, {"osc3_detune", 0.10f}, {"osc3_level", 0.52f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 3600.0f}, {"filterResonance", 1.45f}, {"filterEnvAmount", 0.48f}, {"filterDrive", 0.32f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 0.0f}, {"filter2CutoffHz", 8500.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.25f},
         {"ampAttack", 0.005f}, {"ampDecay", 1.2f}, {"ampSustain", 0.75f}, {"ampRelease", 1.6f},
         {"macroWarmth", 0.45f}, {"macroMotion", 0.38f}, {"macroShimmer", 0.38f}, {"macroAir", 0.50f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.32f}, {"delayFeedback", 0.42f},
         {"reverbMix", 0.38f}, {"reverbDry", 1.0f}, {"reverbDecay", 5.8f}, {"reverbShimmer", 0.35f}, {"reverbDucking", 0.25f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 28. Vocal Formant Solo Lead
    presetManager_.registerFactoryPreset(PresetEntry{
        "Vocal Formant Solo Lead", "Leads & Poly-Synths",
        "Singing vocal lead using Filter 2 Vocal Carve formant shaping, smooth body, and wide tape delay.",
        {"Vocal", "Formant", "Singing", "Lead", "Expressive"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.25f}, {"osc1_detune", 0.30f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.40f}, {"osc2_detune", 0.25f}, {"osc2_level", 0.60f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 1.0f}, {"osc3_engine", 4.0f}, {"osc3_model", 0.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.15f}, {"osc3_detune", 0.0f}, {"osc3_level", 0.45f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 2200.0f}, {"filterResonance", 1.15f}, {"filterEnvAmount", 0.40f}, {"filterDrive", 0.22f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 1.0f}, {"filter2CutoffHz", 1450.0f}, {"filter2Resonance", 1.80f}, {"vocalCarve", 0.60f},
         {"ampAttack", 0.015f}, {"ampDecay", 1.5f}, {"ampSustain", 0.75f}, {"ampRelease", 0.80f},
         {"fltAttack", 0.020f}, {"fltDecay", 1.2f}, {"fltSustain", 0.60f}, {"fltRelease", 0.70f},
         {"macroWarmth", 0.52f}, {"macroMotion", 0.45f}, {"macroShimmer", 0.35f}, {"macroAir", 0.45f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.32f}, {"delayFeedback", 0.42f},
         {"reverbMix", 0.32f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.0f}, {"reverbShimmer", 0.22f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 29. Analog Velvet Flute Lead
    presetManager_.registerFactoryPreset(PresetEntry{
        "Analog Velvet Flute Lead", "Leads & Poly-Synths",
        "Mellow, organic singing flute lead with soft vibrato drift, breath transient, and warm vintage saturation.",
        {"Flute", "Woodwind", "Velvet", "Lead", "Organic"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 1.0f}, {"osc1_shape", 0.35f}, {"osc1_detune", 0.15f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 3.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.10f}, {"osc2_detune", 0.10f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 3.0f}, {"osc3_engine", 5.0f}, {"osc3_model", 2.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.45f}, {"osc3_detune", 0.25f}, {"osc3_level", 0.22f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1950.0f}, {"filterResonance", 1.00f}, {"filterEnvAmount", 0.25f}, {"filterDrive", 0.18f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.035f}, {"ampDecay", 1.8f}, {"ampSustain", 0.80f}, {"ampRelease", 0.65f},
         {"fltAttack", 0.040f}, {"fltDecay", 1.5f}, {"fltSustain", 0.65f}, {"fltRelease", 0.55f},
         {"macroWarmth", 0.65f}, {"macroMotion", 0.35f}, {"macroShimmer", 0.18f}, {"macroAir", 0.48f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.25f}, {"delayFeedback", 0.32f},
         {"reverbMix", 0.35f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.8f}, {"reverbShimmer", 0.18f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 30. 70s Moog Singing Lead
    presetManager_.registerFactoryPreset(PresetEntry{
        "70s Moog Singing Lead", "Leads & Poly-Synths",
        "Classic 70s prog-rock and fusion singing saw lead, rich ladder filter bite and creamy harmonics.",
        {"Moog", "70s", "Prog", "Lead", "Singing"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.20f}, {"osc1_detune", 0.30f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.35f}, {"osc2_detune", 0.25f}, {"osc2_level", 0.70f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 0.0f}, {"osc3_octave", -1.0f}, {"osc3_shape", 0.15f}, {"osc3_detune", 0.15f}, {"osc3_level", 0.50f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 2600.0f}, {"filterResonance", 1.85f}, {"filterEnvAmount", 0.50f}, {"filterDrive", 0.35f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.008f}, {"ampDecay", 1.4f}, {"ampSustain", 0.80f}, {"ampRelease", 0.50f},
         {"fltAttack", 0.008f}, {"fltDecay", 1.2f}, {"fltSustain", 0.65f}, {"fltRelease", 0.48f},
         {"macroWarmth", 0.68f}, {"macroMotion", 0.35f}, {"macroShimmer", 0.12f}, {"macroAir", 0.32f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 2.0f}, {"delayMix", 0.28f}, {"delayFeedback", 0.38f},
         {"reverbMix", 0.28f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.5f}, {"reverbShimmer", 0.12f},
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
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 0.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.30f}, {"osc1_detune", 0.15f}, {"osc1_level", 0.80f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 3.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.20f}, {"osc2_detune", 0.12f}, {"osc2_level", 0.50f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 3.0f}, {"osc3_engine", 5.0f}, {"osc3_model", 1.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.65f}, {"osc3_detune", 0.40f}, {"osc3_level", 0.32f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1650.0f}, {"filterResonance", 0.85f}, {"filterEnvAmount", 0.35f}, {"filterDrive", 0.28f},
         {"filter2Enabled", 1.0f}, {"filter2Type", 0.0f}, {"filter2CutoffHz", 5000.0f}, {"filter2Resonance", 0.70f}, {"vocalCarve", 0.20f},
         {"ampAttack", 0.008f}, {"ampDecay", 1.4f}, {"ampSustain", 0.55f}, {"ampRelease", 1.5f},
         {"fltAttack", 0.006f}, {"fltDecay", 1.2f}, {"fltSustain", 0.45f}, {"fltRelease", 1.4f},
         {"macroWarmth", 0.70f}, {"macroMotion", 0.22f}, {"macroShimmer", 0.05f}, {"macroAir", 0.20f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.16f}, {"delayFeedback", 0.28f},
         {"reverbMix", 0.28f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.0f}, {"reverbShimmer", 0.0f}, {"reverbDucking", 0.15f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 32. Worship DX Electric Piano
    presetManager_.registerFactoryPreset(PresetEntry{
        "Worship DX Electric Piano", "Keys & Electric Pianos",
        "Classic 80s FM electric piano bell tine, lush stereo chorus, and ambient hall space.",
        {"EPiano", "DX7", "FM", "Bell", "Worship"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 0.0f}, {"osc1_engine", 1.0f}, {"osc1_model", 3.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.65f}, {"osc1_detune", 0.20f}, {"osc1_level", 0.82f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.20f}, {"osc2_detune", 0.15f}, {"osc2_level", 0.62f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 3.0f}, {"osc3_octave", 1.0f}, {"osc3_shape", 0.10f}, {"osc3_detune", 0.10f}, {"osc3_level", 0.42f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 3200.0f}, {"filterResonance", 0.85f}, {"filterEnvAmount", 0.40f}, {"filterDrive", 0.18f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.004f}, {"ampDecay", 1.6f}, {"ampSustain", 0.45f}, {"ampRelease", 1.8f},
         {"fltAttack", 0.003f}, {"fltDecay", 1.2f}, {"fltSustain", 0.35f}, {"fltRelease", 1.4f},
         {"macroWarmth", 0.55f}, {"macroMotion", 0.40f}, {"macroShimmer", 0.28f}, {"macroAir", 0.42f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.26f}, {"delayFeedback", 0.38f},
         {"reverbMix", 0.38f}, {"reverbDry", 1.0f}, {"reverbDecay", 5.2f}, {"reverbShimmer", 0.22f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 33. Soft Felt Piano Layer
    presetManager_.registerFactoryPreset(PresetEntry{
        "Soft Felt Piano Layer", "Keys & Electric Pianos",
        "Muted felt hammer attack layered with a soft analog sustain and high celestial air shimmer.",
        {"Felt", "Piano", "Muted", "Shimmer", "Intimate"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 2.0f}, {"osc1_engine", 3.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.50f}, {"osc1_detune", 0.15f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 0.0f}, {"osc2_model", 2.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.25f}, {"osc2_detune", 0.12f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 2.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 1.0f}, {"osc3_shape", 0.55f}, {"osc3_detune", 0.30f}, {"osc3_level", 0.40f},
         {"osc4_enabled", 0.0f}, {"osc5_enabled", 0.0f}, {"osc6_enabled", 0.0f}, {"osc7_enabled", 0.0f}, {"osc8_enabled", 0.0f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 1750.0f}, {"filterResonance", 0.85f}, {"filterEnvAmount", 0.32f}, {"filterDrive", 0.15f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.006f}, {"ampDecay", 1.8f}, {"ampSustain", 0.55f}, {"ampRelease", 2.2f},
         {"fltAttack", 0.005f}, {"fltDecay", 1.4f}, {"fltSustain", 0.40f}, {"fltRelease", 1.8f},
         {"macroWarmth", 0.65f}, {"macroMotion", 0.30f}, {"macroShimmer", 0.42f}, {"macroAir", 0.35f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 1.0f}, {"delayMix", 0.22f}, {"delayFeedback", 0.32f},
         {"reverbMix", 0.45f}, {"reverbDry", 1.0f}, {"reverbDecay", 7.0f}, {"reverbShimmer", 0.40f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

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
         {"ampAttack", 0.005f}, {"ampDecay", 1.0f}, {"ampSustain", 1.0f}, {"ampRelease", 0.15f},
         {"fltAttack", 0.01f}, {"fltDecay", 1.0f}, {"fltSustain", 1.0f}, {"fltRelease", 0.15f},
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
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 2.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.05f}, {"osc1_detune", 0.10f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 4.0f}, {"osc2_engine", 6.0f}, {"osc2_model", 0.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.0f}, {"osc2_detune", 0.12f}, {"osc2_level", 0.88f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 0.0f}, {"osc3_model", 3.0f}, {"osc3_octave", 1.0f}, {"osc3_shape", 0.20f}, {"osc3_detune", 0.15f}, {"osc3_level", 0.45f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 0.0f}, {"osc4_engine", 2.0f}, {"osc4_model", 2.0f}, {"osc4_octave", 0.0f}, {"osc4_shape", 0.45f}, {"osc4_detune", 0.25f}, {"osc4_level", 0.35f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 2800.0f}, {"filterResonance", 0.85f}, {"filterEnvAmount", 0.25f}, {"filterDrive", 0.10f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.005f}, {"ampDecay", 2.2f}, {"ampSustain", 0.65f}, {"ampRelease", 2.0f},
         {"fltAttack", 0.01f}, {"fltDecay", 1.8f}, {"fltSustain", 0.50f}, {"fltRelease", 1.6f},
         {"macroWarmth", 0.70f}, {"macroMotion", 0.25f}, {"macroShimmer", 0.20f}, {"macroAir", 0.30f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.20f}, {"delayFeedback", 0.28f},
         {"reverbMix", 0.38f}, {"reverbDry", 1.0f}, {"reverbDecay", 6.5f}, {"reverbShimmer", 0.15f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 36. Celestial Concert Harp
    presetManager_.registerFactoryPreset(PresetEntry{
        "Celestial Concert Harp", "Pluck",
        "Pristine acoustic concert harp pluck layered with worship shimmer cloud and physical modal harmonics.",
        {"Harp", "Pluck", "Worship", "Celestial", "Shimmer"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 1.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.0f}, {"osc1_detune", 0.15f}, {"osc1_level", 0.92f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 2.0f}, {"osc2_engine", 3.0f}, {"osc2_model", 1.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.60f}, {"osc2_detune", 0.20f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 1.0f}, {"osc3_model", 7.0f}, {"osc3_octave", 1.0f}, {"osc3_shape", 0.50f}, {"osc3_detune", 0.30f}, {"osc3_level", 0.40f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 6500.0f}, {"filterResonance", 0.95f}, {"filterEnvAmount", 0.30f}, {"filterDrive", 0.05f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.003f}, {"ampDecay", 2.5f}, {"ampSustain", 0.50f}, {"ampRelease", 2.2f},
         {"fltAttack", 0.005f}, {"fltDecay", 2.0f}, {"fltSustain", 0.45f}, {"fltRelease", 1.8f},
         {"macroWarmth", 0.55f}, {"macroMotion", 0.30f}, {"macroShimmer", 0.55f}, {"macroAir", 0.40f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.24f}, {"delayFeedback", 0.35f},
         {"reverbMix", 0.45f}, {"reverbDry", 1.0f}, {"reverbDecay", 8.0f}, {"reverbShimmer", 0.50f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 37. Nordic Mallet & Bell Cloud
    presetManager_.registerFactoryPreset(PresetEntry{
        "Nordic Mallet & Bell Cloud", "Lead",
        "Acoustic celesta bell and African kalimba layered with deep analog sub for Scandinavian indie melodies.",
        {"Mallet", "Celesta", "Kalimba", "Bell", "Nordic"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 3.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.0f}, {"osc1_detune", 0.15f}, {"osc1_level", 0.88f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 4.0f}, {"osc2_engine", 6.0f}, {"osc2_model", 6.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.10f}, {"osc2_detune", 0.20f}, {"osc2_level", 0.70f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 4.0f}, {"osc3_engine", 6.0f}, {"osc3_model", 5.0f}, {"osc3_octave", 1.0f}, {"osc3_shape", 0.0f}, {"osc3_detune", 0.22f}, {"osc3_level", 0.65f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 1.0f}, {"osc4_engine", 4.0f}, {"osc4_model", 0.0f}, {"osc4_octave", -1.0f}, {"osc4_shape", 0.15f}, {"osc4_detune", 0.0f}, {"osc4_level", 0.75f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 4500.0f}, {"filterResonance", 0.90f}, {"filterEnvAmount", 0.25f}, {"filterDrive", 0.12f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.004f}, {"ampDecay", 1.8f}, {"ampSustain", 0.45f}, {"ampRelease", 1.8f},
         {"fltAttack", 0.005f}, {"fltDecay", 1.4f}, {"fltSustain", 0.35f}, {"fltRelease", 1.5f},
         {"macroWarmth", 0.60f}, {"macroMotion", 0.35f}, {"macroShimmer", 0.30f}, {"macroAir", 0.35f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.25f}, {"delayFeedback", 0.30f},
         {"reverbMix", 0.36f}, {"reverbDry", 1.0f}, {"reverbDecay", 5.5f}, {"reverbShimmer", 0.25f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 38. Sacred Pipe Flute Swell
    presetManager_.registerFactoryPreset(PresetEntry{
        "Sacred Pipe Flute Swell", "Pad",
        "Majestic cathedral pipe organ flute layered with vocal choir breath and warm church drawbars.",
        {"Organ", "Flute", "Cathedral", "Pad", "Sacred"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 7.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.25f}, {"osc1_detune", 0.18f}, {"osc1_level", 0.90f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 0.0f}, {"osc2_engine", 2.0f}, {"osc2_model", 0.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.55f}, {"osc2_detune", 0.35f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 1.0f}, {"osc3_model", 1.0f}, {"osc3_octave", 1.0f}, {"osc3_shape", 0.40f}, {"osc3_detune", 0.25f}, {"osc3_level", 0.48f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 3200.0f}, {"filterResonance", 0.85f}, {"filterEnvAmount", 0.20f}, {"filterDrive", 0.08f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.85f}, {"ampDecay", 2.5f}, {"ampSustain", 0.80f}, {"ampRelease", 3.0f},
         {"fltAttack", 0.95f}, {"fltDecay", 2.2f}, {"fltSustain", 0.70f}, {"fltRelease", 2.8f},
         {"macroWarmth", 0.70f}, {"macroMotion", 0.25f}, {"macroShimmer", 0.35f}, {"macroAir", 0.45f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.22f}, {"delayFeedback", 0.25f},
         {"reverbMix", 0.48f}, {"reverbDry", 1.0f}, {"reverbDecay", 9.0f}, {"reverbShimmer", 0.30f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 39. Music Box & Vinyl Dreams
    presetManager_.registerFactoryPreset(PresetEntry{
        "Music Box & Vinyl Dreams", "Arp",
        "Vintage kalimba tine & glockenspiel arpeggiator cushioned in authentic velvet vinyl dust atmosphere.",
        {"Arp", "MusicBox", "Vintage", "Vinyl", "Lofi"},
        {{"activeOscCount", 3.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 6.0f}, {"osc1_octave", 1.0f}, {"osc1_shape", 0.10f}, {"osc1_detune", 0.15f}, {"osc1_level", 0.85f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 4.0f}, {"osc2_engine", 6.0f}, {"osc2_model", 4.0f}, {"osc2_octave", 1.0f}, {"osc2_shape", 0.0f}, {"osc2_detune", 0.18f}, {"osc2_level", 0.72f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 3.0f}, {"osc3_engine", 5.0f}, {"osc3_model", 1.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.65f}, {"osc3_detune", 0.30f}, {"osc3_level", 0.40f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 3800.0f}, {"filterResonance", 0.90f}, {"filterEnvAmount", 0.30f}, {"filterDrive", 0.15f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.003f}, {"ampDecay", 0.85f}, {"ampSustain", 0.30f}, {"ampRelease", 1.2f},
         {"fltAttack", 0.004f}, {"fltDecay", 0.70f}, {"fltSustain", 0.25f}, {"fltRelease", 1.0f},
         {"macroWarmth", 0.65f}, {"macroMotion", 0.40f}, {"macroShimmer", 0.25f}, {"macroAir", 0.30f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.28f}, {"delayFeedback", 0.38f},
         {"reverbMix", 0.35f}, {"reverbDry", 1.0f}, {"reverbDecay", 4.8f}, {"reverbShimmer", 0.20f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 1.0f}, {"arpRate", 4.0f}, {"subLowestOnly", 1.0f}}
    });

    // 40. Cathedral Chime & Choir Ambient
    presetManager_.registerFactoryPreset(PresetEntry{
        "Cathedral Chime & Choir Ambient", "Ambient",
        "Singing acoustic hand chimes floating into an expansive choral swell with deep infinite space reverb.",
        {"Ambient", "Chime", "Choir", "Atmosphere", "Cinematic"},
        {{"activeOscCount", 4.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 5.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.05f}, {"osc1_detune", 0.20f}, {"osc1_level", 0.86f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 4.0f}, {"osc2_engine", 6.0f}, {"osc2_model", 7.0f}, {"osc2_octave", -1.0f}, {"osc2_shape", 0.30f}, {"osc2_detune", 0.25f}, {"osc2_level", 0.68f},
         {"osc3_enabled", 1.0f}, {"osc3_sourceType", 0.0f}, {"osc3_engine", 2.0f}, {"osc3_model", 0.0f}, {"osc3_octave", 0.0f}, {"osc3_shape", 0.60f}, {"osc3_detune", 0.40f}, {"osc3_level", 0.78f},
         {"osc4_enabled", 1.0f}, {"osc4_sourceType", 0.0f}, {"osc4_engine", 1.0f}, {"osc4_model", 3.0f}, {"osc4_octave", 1.0f}, {"osc4_shape", 0.55f}, {"osc4_detune", 0.32f}, {"osc4_level", 0.52f},
         {"filter1Enabled", 1.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 2600.0f}, {"filterResonance", 0.88f}, {"filterEnvAmount", 0.22f}, {"filterDrive", 0.05f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 1.20f}, {"ampDecay", 3.2f}, {"ampSustain", 0.85f}, {"ampRelease", 4.2f},
         {"fltAttack", 1.40f}, {"fltDecay", 2.8f}, {"fltSustain", 0.75f}, {"fltRelease", 3.8f},
         {"macroWarmth", 0.75f}, {"macroMotion", 0.35f}, {"macroShimmer", 0.60f}, {"macroAir", 0.50f},
         {"delayEnabled", 1.0f}, {"delaySync", 1.0f}, {"delaySyncDiv", 4.0f}, {"delayMix", 0.25f}, {"delayFeedback", 0.32f},
         {"reverbMix", 0.55f}, {"reverbDry", 0.95f}, {"reverbDecay", 12.0f}, {"reverbShimmer", 0.55f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 41. Solo Concert Grand Piano (Pristine True-Stereo Multi-Zone Acoustic Grand)
    presetManager_.registerFactoryPreset(PresetEntry{
        "Solo Concert Grand Piano", "Acoustic",
        "Pure, unadorned Steinway concert grand piano in full stereo width with authentic multi-octave acoustic resonance.",
        {"Piano", "Grand", "Acoustic", "Solo", "Steinway"},
        {{"activeOscCount", 1.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 0.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.0f}, {"osc1_detune", 0.12f}, {"osc1_level", 0.95f},
         {"osc2_enabled", 0.0f}, {"osc3_enabled", 0.0f}, {"osc4_enabled", 0.0f},
         {"filter1Enabled", 0.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 18000.0f}, {"filterResonance", 0.707f}, {"filterEnvAmount", 0.0f}, {"filterDrive", 0.0f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.001f}, {"ampDecay", 4.5f}, {"ampSustain", 0.70f}, {"ampRelease", 2.5f},
         {"fltAttack", 0.001f}, {"fltDecay", 4.5f}, {"fltSustain", 1.0f}, {"fltRelease", 2.5f},
         {"macroWarmth", 0.50f}, {"macroMotion", 0.0f}, {"macroShimmer", 0.0f}, {"macroAir", 0.25f},
         {"delayEnabled", 0.0f}, {"delayMix", 0.0f},
         {"reverbMix", 0.18f}, {"reverbDry", 1.0f}, {"reverbDecay", 2.8f}, {"reverbShimmer", 0.0f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 42. Pure Concert Harp Solo
    presetManager_.registerFactoryPreset(PresetEntry{
        "Pure Concert Harp Solo", "Acoustic",
        "Organic concert harp pluck with authentic room acoustics and natural acoustic decay across multiple octaves.",
        {"Harp", "Pluck", "Acoustic", "Solo", "Organic"},
        {{"activeOscCount", 1.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 1.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.0f}, {"osc1_detune", 0.14f}, {"osc1_level", 0.95f},
         {"osc2_enabled", 0.0f}, {"osc3_enabled", 0.0f}, {"osc4_enabled", 0.0f},
         {"filter1Enabled", 0.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 18000.0f}, {"filterResonance", 0.707f}, {"filterEnvAmount", 0.0f}, {"filterDrive", 0.0f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.001f}, {"ampDecay", 4.2f}, {"ampSustain", 0.60f}, {"ampRelease", 2.2f},
         {"fltAttack", 0.001f}, {"fltDecay", 4.2f}, {"fltSustain", 1.0f}, {"fltRelease", 2.2f},
         {"macroWarmth", 0.50f}, {"macroMotion", 0.0f}, {"macroShimmer", 0.0f}, {"macroAir", 0.30f},
         {"delayEnabled", 0.0f}, {"delayMix", 0.0f},
         {"reverbMix", 0.22f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.2f}, {"reverbShimmer", 0.0f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });

    // 43. Nordic Celesta & Glockenspiel
    presetManager_.registerFactoryPreset(PresetEntry{
        "Nordic Celesta & Glockenspiel", "Acoustic",
        "Layered acoustic celesta mallet bell and orchestral steel chimes with pristine high-frequency air.",
        {"Celesta", "Glockenspiel", "Bell", "Acoustic", "Mallet"},
        {{"activeOscCount", 2.0f},
         {"osc1_enabled", 1.0f}, {"osc1_sourceType", 4.0f}, {"osc1_engine", 6.0f}, {"osc1_model", 3.0f}, {"osc1_octave", 0.0f}, {"osc1_shape", 0.0f}, {"osc1_detune", 0.15f}, {"osc1_level", 0.88f},
         {"osc2_enabled", 1.0f}, {"osc2_sourceType", 4.0f}, {"osc2_engine", 6.0f}, {"osc2_model", 4.0f}, {"osc2_octave", 0.0f}, {"osc2_shape", 0.0f}, {"osc2_detune", 0.18f}, {"osc2_level", 0.65f},
         {"osc3_enabled", 0.0f}, {"osc4_enabled", 0.0f},
         {"filter1Enabled", 0.0f}, {"filterType", 0.0f}, {"filterCutoffHz", 18000.0f}, {"filterResonance", 0.707f}, {"filterEnvAmount", 0.0f}, {"filterDrive", 0.0f},
         {"filter2Enabled", 0.0f},
         {"ampAttack", 0.001f}, {"ampDecay", 3.8f}, {"ampSustain", 0.50f}, {"ampRelease", 2.0f},
         {"fltAttack", 0.001f}, {"fltDecay", 3.8f}, {"fltSustain", 1.0f}, {"fltRelease", 2.0f},
         {"macroWarmth", 0.50f}, {"macroMotion", 0.0f}, {"macroShimmer", 0.10f}, {"macroAir", 0.35f},
         {"delayEnabled", 0.0f}, {"delayMix", 0.0f},
         {"reverbMix", 0.25f}, {"reverbDry", 1.0f}, {"reverbDecay", 3.8f}, {"reverbShimmer", 0.10f},
         {"droneEnabled", 0.0f}, {"arpEnabled", 0.0f}, {"subLowestOnly", 1.0f}}
    });
}

void NovaSynthProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    currentSampleRate_ = sampleRate;
    synthEngine_.prepare(sampleRate, samplesPerBlock);

    arpPatternLengthParam_ = apvts_.getRawParameterValue("arpPatternLength");
    for (int s = 0; s < 64; ++s) {
        arpStepGateParams_[static_cast<std::size_t>(s)] = apvts_.getRawParameterValue("arpStepGate_" + juce::String(s + 1));
        arpStepVelParams_[static_cast<std::size_t>(s)]  = apvts_.getRawParameterValue("arpStepVel_" + juce::String(s + 1));
    }

    syncParametersFromAPVTS();
}

void NovaSynthProcessor::releaseResources() {
    synthEngine_.reset();
}

bool NovaSynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void NovaSynthProcessor::triggerAuditionNoteOn(int midiNote, float velocity) noexcept {
    uiNoteQueue_.pushOverwrite(PendingNoteEvent{ midiNote, velocity, true });
}

void NovaSynthProcessor::triggerAuditionNoteOff(int midiNote) noexcept {
    uiNoteQueue_.pushOverwrite(PendingNoteEvent{ midiNote, 0.0f, false });
}

void NovaSynthProcessor::syncParametersFromAPVTS() noexcept {
    using nova::dsp::WorshipSynthEngine;
    using nova::dsp::MultiEngineOscillator;

    WorshipSynthEngine::Parameters p;
    p.activeOscCount = static_cast<int>(apvts_.getRawParameterValue("activeOscCount")->load());

    const uint8_t sMask = soloMask_.load(std::memory_order_relaxed);
    const uint8_t mMask = muteMask_.load(std::memory_order_relaxed);

    for (int i = 1; i <= WorshipSynthEngine::kNumOscSlots; ++i) {
        const juce::String prefix = "osc" + juce::String(i) + "_";
        auto& slot = p.oscSlots[static_cast<std::size_t>(i - 1)];
        const bool userEnabled = apvts_.getRawParameterValue(prefix + "enabled")->load() > 0.5f;
        const bool isSoloActive = (sMask != 0);
        const bool isThisSoloed = ((sMask & (1 << (i - 1))) != 0);
        const bool isThisMuted = ((mMask & (1 << (i - 1))) != 0);

        // Real Studio Mixer Mute: independent of userEnabled (engine power switch)
        const bool passesSolo = !isSoloActive || isThisSoloed;
        slot.enabled = userEnabled && !isThisMuted && passesSolo;
        if (slot.enabled) {
            p.activeOscCount = std::max(p.activeOscCount, i);
        }

        const int sType = static_cast<int>(apvts_.getRawParameterValue(prefix + "sourceType")->load());
        slot.sourceType = sType;
        slot.modelIndex = static_cast<int>(apvts_.getRawParameterValue(prefix + "model")->load());
        slot.filterRouting = static_cast<int>(apvts_.getRawParameterValue(prefix + "filterRouting")->load());

        if (sType == 1) {
            slot.engine = MultiEngineOscillator::EngineType::SubOscillator;
        } else if (sType == 2) {
            slot.engine = MultiEngineOscillator::EngineType::PhysicalPluckModal;
        } else if (sType == 3) {
            slot.engine = MultiEngineOscillator::EngineType::NoiseTransient;
        } else if (sType == 4) {
            slot.engine = MultiEngineOscillator::EngineType::SamplePlayback;
        } else {
            const int eng = static_cast<int>(apvts_.getRawParameterValue(prefix + "engine")->load());
            if (eng == 3) {
                slot.engine = MultiEngineOscillator::EngineType::SamplePlayback;
            } else {
                slot.engine = static_cast<MultiEngineOscillator::EngineType>(std::clamp(eng, 0, 2));
            }
        }

        slot.octaveOffset   = static_cast<int>(apvts_.getRawParameterValue(prefix + "octave")->load());
        slot.semitoneOffset = static_cast<int>(apvts_.getRawParameterValue(prefix + "semi")->load());
        slot.shapeMorph     = apvts_.getRawParameterValue(prefix + "shape")->load();
        slot.detune         = apvts_.getRawParameterValue(prefix + "detune")->load();
        slot.pan            = apvts_.getRawParameterValue(prefix + "pan")->load();
        slot.level          = apvts_.getRawParameterValue(prefix + "level")->load();
        if (auto* pR = apvts_.getRawParameterValue(prefix + "rate")) slot.rate = pR->load();
        if (auto* pSc = apvts_.getRawParameterValue(prefix + "scatter")) slot.scatter = pSc->load();
        if (auto* pDu = apvts_.getRawParameterValue(prefix + "duration")) slot.duration = pDu->load();
        if (auto* pSp = apvts_.getRawParameterValue(prefix + "spread")) slot.spread = pSp->load();
        if (auto* pW = apvts_.getRawParameterValue(prefix + "warp")) slot.warp = pW->load();
        if (auto* pL = apvts_.getRawParameterValue(prefix + "lowestNoteOnly")) slot.lowestNoteOnly = pL->load() > 0.5f;
        if (auto* pEnv = apvts_.getRawParameterValue(prefix + "ampEnvSource")) slot.ampEnvSource = static_cast<int>(pEnv->load());
    }

    p.filter1Enabled   = apvts_.getRawParameterValue("filter1Enabled")->load() > 0.5f;
    p.filterType       = static_cast<int>(apvts_.getRawParameterValue("filterType")->load());
    p.filterCutoffHz   = apvts_.getRawParameterValue("filterCutoffHz")->load();
    p.filterResonance  = apvts_.getRawParameterValue("filterResonance")->load();
    p.filterEnvAmount  = apvts_.getRawParameterValue("filterEnvAmount")->load();
    p.filterDrive      = apvts_.getRawParameterValue("filterDrive")->load();

    p.filter2Enabled   = apvts_.getRawParameterValue("filter2Enabled")->load() > 0.5f;
    p.filter2Type      = static_cast<int>(apvts_.getRawParameterValue("filter2Type")->load());
    p.filter2CutoffHz  = apvts_.getRawParameterValue("filter2CutoffHz")->load();
    p.filter2Resonance = apvts_.getRawParameterValue("filter2Resonance")->load();
    p.filter2EnvAmount = apvts_.getRawParameterValue("filter2EnvAmount")->load();
    p.filter2Drive     = apvts_.getRawParameterValue("filter2Drive")->load();
    p.vocalCarve       = apvts_.getRawParameterValue("vocalCarve")->load();

    if (auto* pDel = apvts_.getRawParameterValue("ampDelay")) p.ampEnv.delaySec = pDel->load();
    p.ampEnv.attackSec     = apvts_.getRawParameterValue("ampAttack")->load();
    if (auto* pHold = apvts_.getRawParameterValue("ampHold")) p.ampEnv.holdSec = pHold->load();
    p.ampEnv.decaySec      = apvts_.getRawParameterValue("ampDecay")->load();
    p.ampEnv.sustainLvl    = apvts_.getRawParameterValue("ampSustain")->load();
    p.ampEnv.releaseSec    = apvts_.getRawParameterValue("ampRelease")->load();
    if (auto* pVel = apvts_.getRawParameterValue("ampVelocity")) p.ampEnv.velocitySens = pVel->load();

    if (auto* pDel = apvts_.getRawParameterValue("fltDelay")) p.filterEnv.delaySec = pDel->load();
    p.filterEnv.attackSec  = apvts_.getRawParameterValue("fltAttack")->load();
    if (auto* pHold = apvts_.getRawParameterValue("fltHold")) p.filterEnv.holdSec = pHold->load();
    p.filterEnv.decaySec   = apvts_.getRawParameterValue("fltDecay")->load();
    p.filterEnv.sustainLvl = apvts_.getRawParameterValue("fltSustain")->load();
    p.filterEnv.releaseSec = apvts_.getRawParameterValue("fltRelease")->load();
    if (auto* pVel = apvts_.getRawParameterValue("fltVelocity")) p.filterEnv.velocitySens = pVel->load();

    if (auto* pDel = apvts_.getRawParameterValue("modDelay")) p.modEnv.delaySec = pDel->load();
    if (auto* pA = apvts_.getRawParameterValue("modAttack"))  p.modEnv.attackSec  = pA->load();
    if (auto* pHold = apvts_.getRawParameterValue("modHold")) p.modEnv.holdSec = pHold->load();
    if (auto* pD = apvts_.getRawParameterValue("modDecay"))   p.modEnv.decaySec   = pD->load();
    if (auto* pS = apvts_.getRawParameterValue("modSustain")) p.modEnv.sustainLvl = pS->load();
    if (auto* pR = apvts_.getRawParameterValue("modRelease")) p.modEnv.releaseSec = pR->load();
    if (auto* pVel = apvts_.getRawParameterValue("modVelocity")) p.modEnv.velocitySens = pVel->load();

    if (auto* pDel = apvts_.getRawParameterValue("auxDelay")) p.auxEnv.delaySec = pDel->load();
    if (auto* pA = apvts_.getRawParameterValue("auxAttack"))  p.auxEnv.attackSec  = pA->load();
    if (auto* pHold = apvts_.getRawParameterValue("auxHold")) p.auxEnv.holdSec = pHold->load();
    if (auto* pD = apvts_.getRawParameterValue("auxDecay"))   p.auxEnv.decaySec   = pD->load();
    if (auto* pS = apvts_.getRawParameterValue("auxSustain")) p.auxEnv.sustainLvl = pS->load();
    if (auto* pR = apvts_.getRawParameterValue("auxRelease")) p.auxEnv.releaseSec = pR->load();
    if (auto* pVel = apvts_.getRawParameterValue("auxVelocity")) p.auxEnv.velocitySens = pVel->load();

    p.lfo1RateHz = apvts_.getRawParameterValue("lfo1RateHz")->load();
    p.lfo1Depth  = apvts_.getRawParameterValue("lfo1Depth")->load();
    if (auto* pS = apvts_.getRawParameterValue("lfo1Shape"))
        p.lfo1Shape = static_cast<nova::dsp::LfoGenerator::Shape>(static_cast<int>(pS->load()));
    if (auto* pSync = apvts_.getRawParameterValue("lfo1Sync")) p.lfo1Sync = pSync->load() > 0.5f;
    if (auto* pDiv = apvts_.getRawParameterValue("lfo1SyncDiv")) p.lfo1SyncDiv = static_cast<int>(pDiv->load());

    p.lfo2RateHz = apvts_.getRawParameterValue("lfo2RateHz")->load();
    p.lfo2Depth  = apvts_.getRawParameterValue("lfo2Depth")->load();
    if (auto* pS = apvts_.getRawParameterValue("lfo2Shape"))
        p.lfo2Shape = static_cast<nova::dsp::LfoGenerator::Shape>(static_cast<int>(pS->load()));
    if (auto* pSync = apvts_.getRawParameterValue("lfo2Sync")) p.lfo2Sync = pSync->load() > 0.5f;
    if (auto* pDiv = apvts_.getRawParameterValue("lfo2SyncDiv")) p.lfo2SyncDiv = static_cast<int>(pDiv->load());

    if (auto* pR = apvts_.getRawParameterValue("lfo3RateHz")) p.lfo3RateHz = pR->load();
    if (auto* pD = apvts_.getRawParameterValue("lfo3Depth"))  p.lfo3Depth  = pD->load();
    if (auto* pS = apvts_.getRawParameterValue("lfo3Shape"))
        p.lfo3Shape = static_cast<nova::dsp::LfoGenerator::Shape>(static_cast<int>(pS->load()));
    if (auto* pSync = apvts_.getRawParameterValue("lfo3Sync")) p.lfo3Sync = pSync->load() > 0.5f;
    if (auto* pDiv = apvts_.getRawParameterValue("lfo3SyncDiv")) p.lfo3SyncDiv = static_cast<int>(pDiv->load());

    if (auto* pR = apvts_.getRawParameterValue("lfo4RateHz")) p.lfo4RateHz = pR->load();
    if (auto* pD = apvts_.getRawParameterValue("lfo4Depth"))  p.lfo4Depth  = pD->load();
    if (auto* pS = apvts_.getRawParameterValue("lfo4Shape"))
        p.lfo4Shape = static_cast<nova::dsp::LfoGenerator::Shape>(static_cast<int>(pS->load()));
    if (auto* pSync = apvts_.getRawParameterValue("lfo4Sync")) p.lfo4Sync = pSync->load() > 0.5f;
    if (auto* pDiv = apvts_.getRawParameterValue("lfo4SyncDiv")) p.lfo4SyncDiv = static_cast<int>(pDiv->load());

    p.macroWarmth  = apvts_.getRawParameterValue("macroWarmth")->load();
    p.macroMotion  = apvts_.getRawParameterValue("macroMotion")->load();
    p.macroShimmer = apvts_.getRawParameterValue("macroShimmer")->load();
    p.macroAir     = apvts_.getRawParameterValue("macroAir")->load();

    p.droneEnabled = apvts_.getRawParameterValue("droneEnabled")->load() > 0.5f;
    p.droneRootKey = static_cast<int>(apvts_.getRawParameterValue("droneRootKey")->load());
    p.droneLevel   = apvts_.getRawParameterValue("droneLevel")->load();
    if (auto* pDv = apvts_.getRawParameterValue("droneVoicing")) p.droneVoicing = static_cast<int>(pDv->load());
    if (auto* pDo = apvts_.getRawParameterValue("droneOctave"))  p.droneOctave = static_cast<int>(pDo->load());
    if (auto* pDt = apvts_.getRawParameterValue("droneTone"))    p.droneTone = pDt->load();
    if (auto* pDg = apvts_.getRawParameterValue("droneGlide"))   p.droneGlide = pDg->load();

    if (isHostBpmSynced_) {
        p.bpm = static_cast<float>(currentBpm_);
    } else if (auto* pBpm = apvts_.getRawParameterValue("bpm")) {
        p.bpm = pBpm->load();
        currentBpm_ = p.bpm;
        setBpm(currentBpm_, false);
    }

    p.arpEnabled = apvts_.getRawParameterValue("arpEnabled")->load() > 0.5f;
    p.arpMode    = static_cast<WorshipSynthEngine::ArpMode>(
        std::clamp(static_cast<int>(apvts_.getRawParameterValue("arpMode")->load()), 0, 5));
    if (auto* pAs = apvts_.getRawParameterValue("arpSync")) p.arpSync = pAs->load() > 0.5f;
    if (auto* pAsd = apvts_.getRawParameterValue("arpSyncDiv")) p.arpSyncDiv = static_cast<int>(pAsd->load());
    p.arpRateHz  = apvts_.getRawParameterValue("arpRateHz")->load();
    p.arpGate    = apvts_.getRawParameterValue("arpGate")->load();
    if (auto* pOct = apvts_.getRawParameterValue("arpOctaves")) p.arpOctaves = static_cast<int>(pOct->load()) + 1;
    if (auto* pLat = apvts_.getRawParameterValue("arpLatch")) p.arpLatch = pLat->load() > 0.5f;
    if (auto* pSwg = apvts_.getRawParameterValue("arpSwing")) p.arpSwing = pSwg->load();

    if (arpPatternLengthParam_ != nullptr) {
        const int lenIdx = static_cast<int>(arpPatternLengthParam_->load());
        p.arpPatternLength = (lenIdx == 1) ? 32 : ((lenIdx == 2) ? 64 : 16);
    } else if (auto* pLen = apvts_.getRawParameterValue("arpPatternLength")) {
        const int lenIdx = static_cast<int>(pLen->load());
        p.arpPatternLength = (lenIdx == 1) ? 32 : ((lenIdx == 2) ? 64 : 16);
    }

    for (int s = 0; s < 64; ++s) {
        if (arpStepGateParams_[static_cast<std::size_t>(s)] != nullptr) {
            p.arpStepGates[static_cast<std::size_t>(s)] = (arpStepGateParams_[static_cast<std::size_t>(s)]->load() > 0.5f);
        } else if (auto* pG = apvts_.getRawParameterValue("arpStepGate_" + juce::String(s + 1))) {
            p.arpStepGates[static_cast<std::size_t>(s)] = (pG->load() > 0.5f);
        }

        if (arpStepVelParams_[static_cast<std::size_t>(s)] != nullptr) {
            p.arpStepVelocities[static_cast<std::size_t>(s)] = arpStepVelParams_[static_cast<std::size_t>(s)]->load();
        } else if (auto* pV = apvts_.getRawParameterValue("arpStepVel_" + juce::String(s + 1))) {
            p.arpStepVelocities[static_cast<std::size_t>(s)] = pV->load();
        }
    }

    p.delayEnabled  = apvts_.getRawParameterValue("delayEnabled")->load() > 0.5f;
    if (auto* pDs = apvts_.getRawParameterValue("delaySync")) p.delaySync = pDs->load() > 0.5f;
    if (auto* pDsd = apvts_.getRawParameterValue("delaySyncDiv")) p.delaySyncDiv = static_cast<int>(pDsd->load());
    p.delayMix      = apvts_.getRawParameterValue("delayMix")->load();
    p.delayTimeSec  = apvts_.getRawParameterValue("delayTimeSec")->load();
    p.delayFeedback = apvts_.getRawParameterValue("delayFeedback")->load();
    if (auto* pSw = apvts_.getRawParameterValue("delaySwing")) p.delaySwing = pSw->load();
    if (auto* pSubLow = apvts_.getRawParameterValue("subLowestOnly")) p.subLowestOnly = pSubLow->load() > 0.5f;

    p.reverbDry     = apvts_.getRawParameterValue("reverbDry")->load();
    p.reverbMix     = apvts_.getRawParameterValue("reverbMix")->load();
    p.reverbDecay   = apvts_.getRawParameterValue("reverbDecay")->load();
    p.reverbShimmer = apvts_.getRawParameterValue("reverbShimmer")->load();
    if (auto* pPit = apvts_.getRawParameterValue("reverbShimmerPitch")) {
        p.reverbShimmerPitch = static_cast<int>(pPit->load());
    }
    p.reverbDucking = apvts_.getRawParameterValue("reverbDucking")->load();
    p.reverbFreeze  = apvts_.getRawParameterValue("reverbFreeze")->load() > 0.5f;
    p.masterGainDb  = apvts_.getRawParameterValue("masterGainDb")->load();

    currentParams_ = p;
    synthEngine_.setParameters(p);
}

void NovaSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    const auto startTime = std::chrono::high_resolution_clock::now();

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    if (auto* playHead = getPlayHead()) {
        if (auto posOpt = playHead->getPosition()) {
            if (auto bpmOpt = posOpt->getBpm()) {
                if (*bpmOpt >= 20.0 && *bpmOpt <= 400.0) {
                    currentBpm_ = *bpmOpt;
                    isHostBpmSynced_ = true;
                    setBpm(*bpmOpt, true);
                }
            }
        }
    }

    const bool isPresetChange = presetChangePending_.exchange(false, std::memory_order_acq_rel);
    if (isPresetChange) {
        synthEngine_.reset();
    }

    syncParametersFromAPVTS();

    if (isPresetChange) {
        synthEngine_.snapSmoothersToTarget();
    }

    PendingNoteEvent uiEv{};
    while (uiNoteQueue_.pop(uiEv)) {
        if (uiEv.isNoteOn) {
            synthEngine_.noteOn(uiEv.note, uiEv.velocity);
        } else {
            synthEngine_.noteOff(uiEv.note);
        }
    }

    for (const auto metadata : midiMessages) {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn()) {
            synthEngine_.noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
        } else if (msg.isNoteOff()) {
            synthEngine_.noteOff(msg.getNoteNumber());
        } else if (msg.isSustainPedalOn()) {
            synthEngine_.setSustainPedal(true);
        } else if (msg.isSustainPedalOff()) {
            synthEngine_.setSustainPedal(false);
        } else if (msg.isPitchWheel()) {
            const float bend = (static_cast<float>(msg.getPitchWheelValue()) - 8192.0f) / 8192.0f;
            synthEngine_.setPitchBend(bend * 2.0f);
        } else if (msg.isController() && msg.getControllerNumber() == 1) {
            synthEngine_.setModWheel(static_cast<float>(msg.getControllerValue()) / 127.0f);
        } else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
            synthEngine_.allNotesOff();
        }
    }

    if (buffer.getNumChannels() >= 2 && numSamples > 0) {
        synthEngine_.processBlock(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    const double elapsedSec = std::chrono::duration<double>(endTime - startTime).count();
    const double blockDurSec = static_cast<double>(numSamples) / std::max(1.0, currentSampleRate_);

    const auto tel = synthEngine_.getTelemetry();
    const float outPeak = std::max(tel.outputPeakL, tel.outputPeakR);
    updateMetersAndCpu(outPeak * 0.85f, outPeak, blockDurSec, elapsedSec);

    // Update real-time mixer meters with decay
    const float pkL = tel.outputPeakL;
    const float pkR = tel.outputPeakR;
    constexpr float decay = 0.88f;

    const float curL = masterPeakL_.load(std::memory_order_relaxed);
    const float curR = masterPeakR_.load(std::memory_order_relaxed);
    masterPeakL_.store(std::max(pkL, curL * decay), std::memory_order_relaxed);
    masterPeakR_.store(std::max(pkR, curR * decay), std::memory_order_relaxed);

    const auto curP = currentParams_;
    for (int i = 0; i < 8; ++i) {
        float srcPk = 0.0f;
        if (i < curP.activeOscCount && curP.oscSlots[static_cast<std::size_t>(i)].enabled) {
            const float lvl = curP.oscSlots[static_cast<std::size_t>(i)].level;
            srcPk = std::max(pkL, pkR) * lvl;
        }
        const float curSrc = sourcePeaks_[static_cast<std::size_t>(i)].load(std::memory_order_relaxed);
        sourcePeaks_[static_cast<std::size_t>(i)].store(std::max(srcPk, curSrc * decay), std::memory_order_relaxed);
    }
}

void NovaSynthProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto& mm = synthEngine_.getModMatrix();
    auto modTree = apvts_.state.getOrCreateChildWithName("ModMatrix", nullptr);
    modTree.removeAllChildren(nullptr);
    for (std::size_t i = 0; i < nova::dsp::ModMatrix::kMaxSlots; ++i) {
        const auto& slot = mm.getSlot(i);
        if (slot.enabled || slot.source != nova::dsp::ModMatrix::Source::None) {
            juce::ValueTree slotTree("Slot");
            slotTree.setProperty("index", static_cast<int>(i), nullptr);
            slotTree.setProperty("src", static_cast<int>(slot.source), nullptr);
            slotTree.setProperty("dst", static_cast<int>(slot.destination), nullptr);
            slotTree.setProperty("amount", slot.amount, nullptr);
            slotTree.setProperty("enabled", slot.enabled, nullptr);
            modTree.addChild(slotTree, -1, nullptr);
        }
    }
    nova::plugin::NovaProcessorBase::getStateInformation(destData);
}

void NovaSynthProcessor::setStateInformation(const void* data, int sizeInBytes) {
    nova::plugin::NovaProcessorBase::setStateInformation(data, sizeInBytes);
    auto modTree = apvts_.state.getChildWithName("ModMatrix");
    if (modTree.isValid()) {
        auto& mm = synthEngine_.getModMatrix();
        mm.clear();
        for (int i = 0; i < modTree.getNumChildren(); ++i) {
            auto slotTree = modTree.getChild(i);
            const int idx = slotTree.getProperty("index", -1);
            if (idx >= 0 && idx < static_cast<int>(nova::dsp::ModMatrix::kMaxSlots)) {
                const auto src = static_cast<nova::dsp::ModMatrix::Source>(static_cast<int>(slotTree.getProperty("src", 0)));
                const auto dst = static_cast<nova::dsp::ModMatrix::Destination>(static_cast<int>(slotTree.getProperty("dst", 0)));
                const float amt = static_cast<float>(slotTree.getProperty("amount", 0.0f));
                const bool en = slotTree.getProperty("enabled", true);
                mm.setSlot(static_cast<std::size_t>(idx), src, dst, amt, en);
            }
        }
    }
}

int NovaSynthProcessor::getArpPatternLength() const noexcept {
    if (arpPatternLengthParam_ != nullptr) {
        const int idx = static_cast<int>(arpPatternLengthParam_->load());
        return (idx == 1) ? 32 : ((idx == 2) ? 64 : 16);
    }
    return 16;
}

void NovaSynthProcessor::setArpPatternLength(int length) {
    const int idx = (length == 32) ? 1 : ((length == 64) ? 2 : 0);
    if (auto* p = apvts_.getParameter("arpPatternLength")) {
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(idx)));
    }
}

bool NovaSynthProcessor::isArpStepGateActive(int step) const noexcept {
    if (step >= 0 && step < 64) {
        if (arpStepGateParams_[static_cast<std::size_t>(step)] != nullptr) {
            return arpStepGateParams_[static_cast<std::size_t>(step)]->load() > 0.5f;
        }
        if (auto* p = apvts_.getRawParameterValue("arpStepGate_" + juce::String(step + 1))) {
            return p->load() > 0.5f;
        }
    }
    return true;
}

void NovaSynthProcessor::setArpStepGate(int step, bool active) {
    if (step >= 0 && step < 64) {
        if (auto* p = apvts_.getParameter("arpStepGate_" + juce::String(step + 1))) {
            p->setValueNotifyingHost(active ? 1.0f : 0.0f);
        }
    }
}

void NovaSynthProcessor::toggleArpStepGate(int step) {
    setArpStepGate(step, !isArpStepGateActive(step));
}

float NovaSynthProcessor::getArpStepVelocity(int step) const noexcept {
    if (step >= 0 && step < 64) {
        if (arpStepVelParams_[static_cast<std::size_t>(step)] != nullptr) {
            return arpStepVelParams_[static_cast<std::size_t>(step)]->load();
        }
        if (auto* p = apvts_.getRawParameterValue("arpStepVel_" + juce::String(step + 1))) {
            return p->load();
        }
    }
    return 0.8f;
}

void NovaSynthProcessor::setArpStepVelocity(int step, float velocity) {
    if (step >= 0 && step < 64) {
        if (auto* p = apvts_.getParameter("arpStepVel_" + juce::String(step + 1))) {
            p->setValueNotifyingHost(p->convertTo0to1(std::clamp(velocity, 0.05f, 1.0f)));
        }
    }
}

juce::AudioProcessorEditor* NovaSynthProcessor::createEditor() {
    return new NovaSynthEditor(*this);
}

} // namespace nova::synth

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new nova::synth::NovaSynthProcessor();
}
