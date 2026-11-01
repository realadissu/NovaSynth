#pragma once

#include "nova_plugin/NovaProcessorBase.h"
#include "nova_dsp/reverb/DualEngineReverb.h"

namespace nova::space {

class NovaSpaceProcessor : public nova::plugin::NovaProcessorBase {
public:
    NovaSpaceProcessor();
    ~NovaSpaceProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    [[nodiscard]] nova::dsp::DualEngineReverb::Telemetry getReverbTelemetry() const noexcept {
        return reverb_.getTelemetry();
    }

    [[nodiscard]] nova::dsp::DualEngineReverb::Parameters getCurrentParameters() const noexcept {
        return currentParams_;
    }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void registerFactoryPresets();
    void syncParametersFromAPVTS() noexcept;

    nova::dsp::DualEngineReverb reverb_;
    nova::dsp::DualEngineReverb::Parameters currentParams_{};
    double currentSampleRate_ = 48000.0;
};

} // namespace nova::space
