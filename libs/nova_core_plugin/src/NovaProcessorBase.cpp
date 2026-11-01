#include "nova_plugin/NovaProcessorBase.h"
#include <algorithm>

namespace nova::plugin {

NovaProcessorBase::NovaProcessorBase(const BusesProperties& ioLayouts,
                                     juce::AudioProcessorValueTreeState::ParameterLayout parameterLayout,
                                     const juce::String& pluginName)
    : juce::AudioProcessor(ioLayouts),
      pluginName_(pluginName),
      apvts_(*this, &undoManager_, "NOVA_STATE", std::move(parameterLayout)),
      presetManager_(apvts_, pluginName) {}

int NovaProcessorBase::getNumPrograms() {
    return std::max(1, static_cast<int>(presetManager_.getAllPresets().size()));
}

int NovaProcessorBase::getCurrentProgram() {
    return presetManager_.getCurrentPresetIndex();
}

void NovaProcessorBase::setCurrentProgram(int index) {
    presetManager_.loadPresetByIndex(index);
}

const juce::String NovaProcessorBase::getProgramName(int index) {
    const auto& presets = presetManager_.getAllPresets();
    if (index >= 0 && index < static_cast<int>(presets.size())) {
        return presets[static_cast<std::size_t>(index)].name;
    }
    return "Init";
}

void NovaProcessorBase::changeProgramName(int /*index*/, const juce::String& /*newName*/) {}

void NovaProcessorBase::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts_.copyState();
    state.setProperty("presetIndex", presetManager_.getCurrentPresetIndex(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void NovaProcessorBase::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts_.state.getType())) {
        auto tree = juce::ValueTree::fromXml(*xmlState);
        apvts_.replaceState(tree);
    }
}

void NovaProcessorBase::updateMetersAndCpu(float inPeak, float outPeak, double blockDurationSec, double elapsedSec) noexcept {
    const float prevIn = inputPeak_.load(std::memory_order_relaxed);
    const float prevOut = outputPeak_.load(std::memory_order_relaxed);

    inputPeak_.store(std::max(inPeak, prevIn * 0.88f), std::memory_order_relaxed);
    outputPeak_.store(std::max(outPeak, prevOut * 0.88f), std::memory_order_relaxed);

    if (blockDurationSec > 0.0) {
        const float instantCpu = static_cast<float>((elapsedSec / blockDurationSec) * 100.0);
        const float prevCpu = cpuLoadPercent_.load(std::memory_order_relaxed);
        cpuLoadPercent_.store(prevCpu * 0.92f + std::clamp(instantCpu, 0.0f, 99.9f) * 0.08f,
                              std::memory_order_relaxed);
    }
}

} // namespace nova::plugin
