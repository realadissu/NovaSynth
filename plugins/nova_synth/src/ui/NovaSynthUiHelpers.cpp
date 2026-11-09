#include "NovaSynthUiHelpers.h"
#include "nova_dsp/oscillators/MultiEngineOscillator.h"
#include <array>
#include <algorithm>

namespace nova::synth {

// Signature JORNA layer colors for the 8 Oscillator Sources
juce::Colour getLayerColour(int index) noexcept {
    static constexpr std::array<uint32_t, 8> kColours = {
        0xff38bdf8, // OSC A: Cyan
        0xff2dd4bf, // OSC B: Teal
        0xffec4899, // OSC C: Pink / Magenta
        0xffa855f7, // OSC D: Purple
        0xff84cc16, // OSC E: Lime Green
        0xfff59e0b, // OSC F: Amber Gold
        0xfff43f5e, // OSC G: Coral Red
        0xff60a5fa  // OSC H: Sky Blue
    };
    return juce::Colour(kColours[static_cast<std::size_t>(std::clamp(index, 0, 7))]);
}

juce::String getLayerShortName(const nova::dsp::WorshipSynthEngine::Parameters& params, int slotIndex) {
    const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex)];
    if (slot.sourceType == 1) return "SUB";
    if (slot.sourceType == 2) return "PLUCK";
    if (slot.sourceType == 3) return "NOISE";
    if (slot.sourceType == 4) return "SAMPLE";

    int oscIdx = 0;
    for (int i = 0; i < slotIndex; ++i) {
        if (params.oscSlots[static_cast<std::size_t>(i)].sourceType == 0) {
            oscIdx++;
        }
    }
    const char letter = static_cast<char>('A' + std::clamp(oscIdx, 0, 25));
    return juce::String("OSC ") + juce::String::charToString(letter);
}

juce::String getEngineDisplayName(const nova::dsp::WorshipSynthEngine::Parameters& params, int slotIndex) {
    const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex)];
    if (slot.sourceType == 1) return "SUB OSC";
    if (slot.sourceType == 2) return "KARPLUS PLUCK";
    if (slot.sourceType == 3) return "PINK / AIR";
    if (slot.sourceType == 4) return "SAMPLE LAYER";

    using ET = nova::dsp::MultiEngineOscillator::EngineType;
    switch (slot.engine) {
        case ET::WarmAnalog:        return "WARM ANALOG";
        case ET::EvolvingWavetable: return "WAVETABLE";
        case ET::OrganicTexture:    return "GRANULAR";
        case ET::PhysicalPluckModal:return "KARPLUS PLUCK";
        case ET::SubOscillator:     return "SUB OSC";
        case ET::NoiseTransient:    return "PINK / AIR";
        case ET::SamplePlayback:    return "SAMPLE LAYER";
    }
    return "WAVETABLE";
}

juce::String getModelDisplayName(const nova::dsp::WorshipSynthEngine::Parameters& params, int slotIndex) {
    if (slotIndex < 0 || slotIndex >= params.activeOscCount) return "Classic Saw";
    const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex)];
    return nova::dsp::MultiEngineOscillator::getModelName(slot.engine, slot.sourceType, slot.modelIndex);
}

juce::String getFilterRoutingDisplayName(int filterRouting) {
    switch (filterRouting) {
        case 0: return "FLT 1";
        case 1: return "FLT 2";
        case 2: return "FLT 1+2";
        case 3: return "DIRECT";
        default: return "FLT 1";
    }
}

} // namespace nova::synth

