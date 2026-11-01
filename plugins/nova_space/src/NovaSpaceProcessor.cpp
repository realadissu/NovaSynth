#include "NovaSpaceProcessor.h"
#include "NovaSpaceEditor.h"
#include <chrono>

namespace nova::space {

NovaSpaceProcessor::NovaSpaceProcessor()
    : NovaProcessorBase(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true),
          createParameterLayout(),
          "Nova Space") {
    registerFactoryPresets();
    syncParametersFromAPVTS();
}

juce::AudioProcessorValueTreeState::ParameterLayout NovaSpaceProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"engineMode", 1}, "Engine Mode",
        juce::StringArray{"Acoustic Space", "Lush Worship Cloud"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"spaceType", 1}, "Space Archetype",
        juce::StringArray{"Studio Room", "Warm Chamber", "Concert Hall", "Cathedral Cloud"}, 2));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"preDelayMs", 1}, "Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 250.0f, 0.1f, 0.5f), 18.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"decaySeconds", 1}, "Decay Time",
        juce::NormalisableRange<float>(0.2f, 30.0f, 0.01f, 0.4f), 2.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"size", 1}, "Room Size",
        juce::NormalisableRange<float>(0.2f, 1.5f, 0.01f), 0.80f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"diffusion", 1}, "Diffusion",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.82f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"erLevel", 1}, "Early Reflections",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    // Tone & Vocal Carve
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lowCutHz", 1}, "Low Cut",
        juce::NormalisableRange<float>(20.0f, 600.0f, 1.0f, 0.45f), 90.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"highCutHz", 1}, "High Cut",
        juce::NormalisableRange<float>(1500.0f, 20000.0f, 1.0f, 0.45f), 9500.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"highDamping", 1}, "HF Damping",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f), 0.42f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"bassMultiplier", 1}, "Bass Decay Ratio",
        juce::NormalisableRange<float>(0.5f, 1.8f, 0.01f), 1.15f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"vocalCarve", 1}, "Vocal Carve",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.20f));

    // Lush / Worship Shimmer
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"modRateHz", 1}, "Mod Rate",
        juce::NormalisableRange<float>(0.05f, 3.0f, 0.01f), 0.65f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"modDepth", 1}, "Mod Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"shimmerOctave", 1}, "Octave Shimmer",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"shimmerFifth", 1}, "Fifth Glow",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"ducking", 1}, "Auto Ducking",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"freeze", 1}, "Freeze Cloud", false));

    // Output
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"stereoWidth", 1}, "Stereo Width",
        juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mix", 1}, "Dry / Wet Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGainDb", 1}, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

void NovaSpaceProcessor::registerFactoryPresets() {
    using nova::plugin::PresetEntry;

    presetManager_.registerFactoryPreset(PresetEntry{
        "Concert Grand Hall", "Acoustic Spaces",
        "Natural, phase-pure concert hall tailored for Live Piano and acoustic instruments.",
        {"Piano", "Hall", "Natural"},
        {{"engineMode", 0.0f}, {"spaceType", 2.0f}, {"preDelayMs", 22.0f}, {"decaySeconds", 2.4f},
         {"size", 0.88f}, {"diffusion", 0.84f}, {"erLevel", 0.42f}, {"lowCutHz", 85.0f},
         {"highCutHz", 11500.0f}, {"highDamping", 0.40f}, {"bassMultiplier", 1.10f},
         {"vocalCarve", 0.15f}, {"modRateHz", 0.35f}, {"modDepth", 0.12f},
         {"shimmerOctave", 0.0f}, {"shimmerFifth", 0.0f}, {"ducking", 0.05f},
         {"freeze", 0.0f}, {"stereoWidth", 1.05f}, {"mix", 0.30f}, {"outputGainDb", 0.0f}}
    });

    presetManager_.registerFactoryPreset(PresetEntry{
        "Intimate Wood Chamber", "Acoustic Spaces",
        "Warm acoustic chamber with articulate early reflections for felt piano and vocals.",
        {"Chamber", "Warm", "Intimate"},
        {{"engineMode", 0.0f}, {"spaceType", 1.0f}, {"preDelayMs", 12.0f}, {"decaySeconds", 1.45f},
         {"size", 0.65f}, {"diffusion", 0.78f}, {"erLevel", 0.55f}, {"lowCutHz", 110.0f},
         {"highCutHz", 9200.0f}, {"highDamping", 0.52f}, {"bassMultiplier", 1.05f},
         {"vocalCarve", 0.10f}, {"modRateHz", 0.30f}, {"modDepth", 0.08f},
         {"shimmerOctave", 0.0f}, {"shimmerFifth", 0.0f}, {"ducking", 0.0f},
         {"freeze", 0.0f}, {"stereoWidth", 1.0f}, {"mix", 0.26f}, {"outputGainDb", 0.0f}}
    });

    presetManager_.registerFactoryPreset(PresetEntry{
        "Bethel Shimmer Cloud", "Worship Clouds",
        "Silky octave shimmer cloud with Vocal Carve and automatic ducking for Worship Pads.",
        {"Worship", "Shimmer", "Cloud"},
        {{"engineMode", 1.0f}, {"spaceType", 3.0f}, {"preDelayMs", 38.0f}, {"decaySeconds", 7.5f},
         {"size", 1.18f}, {"diffusion", 0.92f}, {"erLevel", 0.22f}, {"lowCutHz", 135.0f},
         {"highCutHz", 7800.0f}, {"highDamping", 0.55f}, {"bassMultiplier", 1.25f},
         {"vocalCarve", 0.45f}, {"modRateHz", 0.72f}, {"modDepth", 0.55f},
         {"shimmerOctave", 0.58f}, {"shimmerFifth", 0.18f}, {"ducking", 0.32f},
         {"freeze", 0.0f}, {"stereoWidth", 1.25f}, {"mix", 0.48f}, {"outputGainDb", 0.0f}}
    });

    presetManager_.registerFactoryPreset(PresetEntry{
        "Angelic Fifths Cathedral", "Worship Clouds",
        "Harmonic +7st fifth and +12st octave bloom creating an orchestral halo behind keys.",
        {"Worship", "Fifths", "Cathedral"},
        {{"engineMode", 1.0f}, {"spaceType", 3.0f}, {"preDelayMs", 45.0f}, {"decaySeconds", 9.2f},
         {"size", 1.28f}, {"diffusion", 0.95f}, {"erLevel", 0.18f}, {"lowCutHz", 150.0f},
         {"highCutHz", 6800.0f}, {"highDamping", 0.60f}, {"bassMultiplier", 1.30f},
         {"vocalCarve", 0.50f}, {"modRateHz", 0.58f}, {"modDepth", 0.62f},
         {"shimmerOctave", 0.42f}, {"shimmerFifth", 0.52f}, {"ducking", 0.28f},
         {"freeze", 0.0f}, {"stereoWidth", 1.35f}, {"mix", 0.50f}, {"outputGainDb", 0.0f}}
    });

    presetManager_.registerFactoryPreset(PresetEntry{
        "Sunday Keys Warm Wash", "Worship Clouds",
        "Dark, velvety ambient tail with strong sidechain ducking so fast chord changes never blur.",
        {"Worship", "Warm", "Ducking"},
        {{"engineMode", 1.0f}, {"spaceType", 2.0f}, {"preDelayMs", 28.0f}, {"decaySeconds", 5.6f},
         {"size", 1.05f}, {"diffusion", 0.88f}, {"erLevel", 0.25f}, {"lowCutHz", 120.0f},
         {"highCutHz", 5400.0f}, {"highDamping", 0.65f}, {"bassMultiplier", 1.20f},
         {"vocalCarve", 0.40f}, {"modRateHz", 0.80f}, {"modDepth", 0.48f},
         {"shimmerOctave", 0.22f}, {"shimmerFifth", 0.0f}, {"ducking", 0.52f},
         {"freeze", 0.0f}, {"stereoWidth", 1.20f}, {"mix", 0.44f}, {"outputGainDb", 0.0f}}
    });

    presetManager_.registerFactoryPreset(PresetEntry{
        "Infinite Prayer Pad", "Shimmer & Freeze",
        "Massive 18-second atmospheric cloud designed for sustaining prayer and transition beds.",
        {"Freeze", "Infinite", "Drone"},
        {{"engineMode", 1.0f}, {"spaceType", 3.0f}, {"preDelayMs", 55.0f}, {"decaySeconds", 18.0f},
         {"size", 1.38f}, {"diffusion", 0.95f}, {"erLevel", 0.12f}, {"lowCutHz", 140.0f},
         {"highCutHz", 6500.0f}, {"highDamping", 0.48f}, {"bassMultiplier", 1.35f},
         {"vocalCarve", 0.55f}, {"modRateHz", 0.50f}, {"modDepth", 0.70f},
         {"shimmerOctave", 0.65f}, {"shimmerFifth", 0.30f}, {"ducking", 0.15f},
         {"freeze", 0.0f}, {"stereoWidth", 1.40f}, {"mix", 0.58f}, {"outputGainDb", 0.0f}}
    });
}

void NovaSpaceProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    currentSampleRate_ = sampleRate;
    reverb_.prepare(sampleRate, samplesPerBlock);
    syncParametersFromAPVTS();
}

void NovaSpaceProcessor::releaseResources() {
    reverb_.reset();
}

bool NovaSpaceProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::stereo()) {
        return false;
    }
    return (mainIn == juce::AudioChannelSet::stereo() || mainIn == juce::AudioChannelSet::mono());
}

void NovaSpaceProcessor::syncParametersFromAPVTS() noexcept {
    using nova::dsp::DualEngineReverb;
    DualEngineReverb::Parameters p;

    const int engIdx = static_cast<int>(apvts_.getRawParameterValue("engineMode")->load());
    p.engineMode = (engIdx == 1) ? DualEngineReverb::EngineMode::LushCloud
                                 : DualEngineReverb::EngineMode::Acoustic;

    const int spaceIdx = static_cast<int>(apvts_.getRawParameterValue("spaceType")->load());
    p.spaceType = static_cast<DualEngineReverb::SpaceType>(std::clamp(spaceIdx, 0, 3));

    p.preDelayMs     = apvts_.getRawParameterValue("preDelayMs")->load();
    p.decaySeconds   = apvts_.getRawParameterValue("decaySeconds")->load();
    p.size           = apvts_.getRawParameterValue("size")->load();
    p.diffusion      = apvts_.getRawParameterValue("diffusion")->load();
    p.erLevel        = apvts_.getRawParameterValue("erLevel")->load();

    p.lowCutHz       = apvts_.getRawParameterValue("lowCutHz")->load();
    p.highCutHz      = apvts_.getRawParameterValue("highCutHz")->load();
    p.highDamping    = apvts_.getRawParameterValue("highDamping")->load();
    p.bassMultiplier = apvts_.getRawParameterValue("bassMultiplier")->load();
    p.vocalCarve     = apvts_.getRawParameterValue("vocalCarve")->load();

    p.modRateHz      = apvts_.getRawParameterValue("modRateHz")->load();
    p.modDepth       = apvts_.getRawParameterValue("modDepth")->load();
    p.shimmerOctave  = apvts_.getRawParameterValue("shimmerOctave")->load();
    p.shimmerFifth   = apvts_.getRawParameterValue("shimmerFifth")->load();
    p.ducking        = apvts_.getRawParameterValue("ducking")->load();
    p.freeze         = apvts_.getRawParameterValue("freeze")->load() > 0.5f;

    p.stereoWidth    = apvts_.getRawParameterValue("stereoWidth")->load();
    p.mix            = apvts_.getRawParameterValue("mix")->load();
    p.outputGainDb   = apvts_.getRawParameterValue("outputGainDb")->load();

    currentParams_ = p;
    reverb_.setParameters(p);
}

void NovaSpaceProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;
    const auto startTime = std::chrono::high_resolution_clock::now();

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, numSamples);
    }

    if (totalNumInputChannels == 1 && totalNumOutputChannels >= 2) {
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }

    syncParametersFromAPVTS();

    if (buffer.getNumChannels() >= 2 && numSamples > 0) {
        reverb_.processBlock(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    const double elapsedSec = std::chrono::duration<double>(endTime - startTime).count();
    const double blockDurSec = static_cast<double>(numSamples) / std::max(1.0, currentSampleRate_);

    const auto tel = reverb_.getTelemetry();
    updateMetersAndCpu(tel.inputPeak, tel.outputPeak, blockDurSec, elapsedSec);
}

juce::AudioProcessorEditor* NovaSpaceProcessor::createEditor() {
    return new NovaSpaceEditor(*this);
}

} // namespace nova::space

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new nova::space::NovaSpaceProcessor();
}
