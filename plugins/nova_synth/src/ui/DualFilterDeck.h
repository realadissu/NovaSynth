#pragma once

#include "nova_ui/NovaTheme.h"
#include "nova_ui/ArcKnob.h"
#include "nova_dsp/synth/WorshipSynthEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace nova::synth {

class NovaSynthProcessor;

class DualFilterDeck : public juce::Component {
public:
    std::function<void()> onFilterStateChanged;

    DualFilterDeck(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~DualFilterDeck() override = default;

    void updateModulationTelemetry(const nova::dsp::WorshipSynthEngine::Telemetry& tel);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void updateFilterFromMouse(int fltNum, juce::Point<float> pos);
    void drawFilterBlock(juce::Graphics& g, juce::Rectangle<int> area, int fltNum, bool isEnabled,
                         int fltType, float cutoffHz, float resonance, juce::Colour accentCol,
                         juce::Rectangle<int>& typeRectOut, juce::Rectangle<int>& powRectOut,
                         juce::Rectangle<float>& curveAreaOut, bool isDragging);
    void drawRoutingBridge(juce::Graphics& g, juce::Rectangle<int> area,
                           const nova::dsp::WorshipSynthEngine::Parameters& params);
    void openFilterTypeMenu(int fltNum, juce::Rectangle<int> rect);
    static juce::String getFilterTypeName(int fltNum, int type);

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;

    nova::ui::ArcKnob flt1Cutoff_;
    nova::ui::ArcKnob flt1Reso_;
    nova::ui::ArcKnob flt1Drive_;
    nova::ui::ArcKnob flt1EnvAmt_;
    nova::ui::ArcKnob flt1Warmth_;

    nova::ui::ArcKnob flt2Cutoff_;
    nova::ui::ArcKnob flt2Reso_;
    nova::ui::ArcKnob flt2Drive_;
    nova::ui::ArcKnob flt2EnvAmt_;
    nova::ui::ArcKnob flt2Carve_;

    juce::Rectangle<int> flt1TypeRect_;
    juce::Rectangle<int> flt1PowerRect_;
    juce::Rectangle<int> flt2TypeRect_;
    juce::Rectangle<int> flt2PowerRect_;
    juce::Rectangle<int> serialBtnRect_;
    juce::Rectangle<int> parallelBtnRect_;

    juce::Rectangle<float> flt1CurveArea_;
    juce::Rectangle<float> flt2CurveArea_;
    bool isDraggingFlt1_ = false;
    bool isDraggingFlt2_ = false;

    nova::dsp::WorshipSynthEngine::Telemetry lastTel_{};
    float lastEffCutoff1_ = 2800.0f;
    float lastEffCutoff2_ = 3600.0f;
};

} // namespace nova::synth
