#include "NovaSynthProcessor.h"
#include <vector>
#include <memory>

namespace nova::synth {

void NovaSynthProcessor::appendModulationAndFxParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params) {
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

    // Embedded Roland Juno-60 / Solina Stereo BBD Chorus
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"chorusEnabled", 1}, "Chorus Enable", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"chorusMode", 1}, "Chorus Mode",
        juce::StringArray{"Juno I (Warm)", "Juno II (Shimmer)", "Juno I+II (Rotary)", "Solina (String Ensemble)", "Dimension D (Spatial 3D)"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"chorusMix", 1}, "Chorus Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"chorusRateHz", 1}, "Chorus Rate",
        juce::NormalisableRange<float>(0.1f, 8.0f, 0.01f, 0.45f), 0.52f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"chorusDepth", 1}, "Chorus Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.65f));

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

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"invertSustainPedal", 1}, "Invert Sustain Pedal", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"masterTuningHz", 1}, "Master Tuning (A4 Hz)",
        juce::NormalisableRange<float>(435.0f, 445.0f, 0.1f), 440.0f));
}

} // namespace nova::synth
