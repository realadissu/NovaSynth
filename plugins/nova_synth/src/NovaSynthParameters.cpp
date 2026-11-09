#include "NovaSynthProcessor.h"
#include <vector>
#include <memory>
#include <array>

namespace nova::synth {

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
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

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
            juce::ParameterID{prefix + "lowestNoteOnly", 1}, namePrefix + "Lowest Note Only", true));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "ampEnvSource", 1}, namePrefix + "Amp Env Source",
            juce::StringArray{"Amp Env 1", "Filter Env 2", "Mod Env 3", "Aux Env 4"}, 0));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "loopMode", 1}, namePrefix + "Loop Mode",
            juce::StringArray{"One-Shot", "Forward", "Ping-Pong", "Reverse"}, 1));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "loopStart", 1}, namePrefix + "Loop Start",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.20f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "loopEnd", 1}, namePrefix + "Loop End",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.90f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "loopXfade", 1}, namePrefix + "Loop Crossfade",
            juce::NormalisableRange<float>(0.0f, 0.25f, 0.001f), 0.05f));
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

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"filterRoutingMode", 1}, "Filter Routing Mode",
        juce::StringArray{"SERIAL (1 -> 2)", "PARALLEL (1 + 2)"}, 0));

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

    // Append LFOs, Macros, Drone, Arp, FX, Master
    appendModulationAndFxParameters(params);

    return { params.begin(), params.end() };
}

} // namespace nova::synth
