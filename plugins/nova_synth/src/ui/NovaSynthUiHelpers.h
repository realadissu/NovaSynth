#pragma once

#include "nova_dsp/synth/WorshipSynthEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova::synth {

juce::Colour getLayerColour(int index) noexcept;
juce::String getLayerShortName(const nova::dsp::WorshipSynthEngine::Parameters& params, int slotIndex);
juce::String getEngineDisplayName(const nova::dsp::WorshipSynthEngine::Parameters& params, int slotIndex);
juce::String getModelDisplayName(const nova::dsp::WorshipSynthEngine::Parameters& params, int slotIndex);
juce::String getFilterRoutingDisplayName(int filterRouting);

} // namespace nova::synth

