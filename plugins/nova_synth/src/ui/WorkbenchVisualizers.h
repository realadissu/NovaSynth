#pragma once

#include "nova_dsp/synth/WorshipSynthEngine.h"
#include "nova_ui/NovaTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova::synth {

/**
 * Visualizer drawing routines for the lower workbench modules in NovaSynth.
 */
void drawReverbVisualizer(juce::Graphics& g,
                          const juce::Rectangle<int>& rect,
                          const nova::dsp::WorshipSynthEngine::Parameters& params,
                          const nova::ui::NovaTheme& theme,
                          float phase);

void drawMacrosVisualizer(juce::Graphics& g,
                          const juce::Rectangle<int>& rect,
                          const nova::dsp::WorshipSynthEngine::Parameters& params,
                          const nova::ui::NovaTheme& theme,
                          float phase);

void drawMasterVisualizer(juce::Graphics& g,
                          const juce::Rectangle<int>& rect,
                          const nova::ui::NovaTheme& theme,
                          float smoothPeakL,
                          float smoothPeakR,
                          float peakHoldL,
                          float peakHoldR);

} // namespace nova::synth

