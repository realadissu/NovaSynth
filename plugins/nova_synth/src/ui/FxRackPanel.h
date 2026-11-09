#pragma once

#include "nova_ui/NovaTheme.h"
#include "nova_ui/ArcKnob.h"
#include "nova_dsp/synth/WorshipSynthEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova::synth {

class NovaSynthProcessor;

class FxRackPanel : public juce::Component, private juce::Timer {
public:
    FxRackPanel(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~FxRackPanel() override;

    void timerCallback() override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void paint(juce::Graphics& g) override;

private:
    void drawCardBackground(juce::Graphics& g, const juce::Rectangle<int>& rect, juce::Colour bg, juce::Colour border);
    void drawToggleSwitch(juce::Graphics& g, const juce::Rectangle<int>& rect, bool isOn, juce::Colour onColor);
    void drawDelayHeader(juce::Graphics& g, const juce::Rectangle<int>& r, const nova::dsp::WorshipSynthEngine::Parameters& params);
    void drawReverbHeader(juce::Graphics& g, const juce::Rectangle<int>& r, const nova::dsp::WorshipSynthEngine::Parameters& params);
    void drawMacrosHeader(juce::Graphics& g, const juce::Rectangle<int>& r, const nova::dsp::WorshipSynthEngine::Parameters& params);
    void drawMasterHeader(juce::Graphics& g, const juce::Rectangle<int>& r, const nova::dsp::WorshipSynthEngine::Parameters& params);
    void drawDelayVisualizer(juce::Graphics& g, const juce::Rectangle<int>& rect, const nova::dsp::WorshipSynthEngine::Parameters& params);

    [[maybe_unused]] NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;

    nova::ui::ArcKnob delaySwing_;
    nova::ui::ArcKnob delayFdbk_;
    nova::ui::ArcKnob delayMix_;

    nova::ui::ArcKnob revDry_;
    nova::ui::ArcKnob revMix_;
    nova::ui::ArcKnob revDecay_;
    nova::ui::ArcKnob revShimmer_;
    nova::ui::ArcKnob revDucking_;

    nova::ui::ArcKnob macroWarmth_;
    nova::ui::ArcKnob macroMotion_;
    nova::ui::ArcKnob macroShimmer_;
    nova::ui::ArcKnob macroAir_;

    nova::ui::ArcKnob masterGain_;

    juce::Rectangle<int> cardDelayRect_;
    juce::Rectangle<int> cardReverbRect_;
    juce::Rectangle<int> cardMacrosRect_;
    juce::Rectangle<int> cardMasterRect_;

    juce::Rectangle<int> delayBypRect_;
    juce::Rectangle<int> delaySyncBtnRect_;
    juce::Rectangle<int> reverbBypRect_;
    juce::Rectangle<int> freezeBtnRect_;
    juce::Rectangle<int> shimmerPitchBtnRect_;

    juce::Rectangle<int> delayVisRect_;
    juce::Rectangle<int> reverbVisRect_;
    juce::Rectangle<int> macroVisRect_;
    juce::Rectangle<int> masterVisRect_;

    float phase_ = 0.0f;
    float cachedReverbMix_ = 0.42f;
    float smoothPeakL_ = 0.0f;
    float smoothPeakR_ = 0.0f;
    float peakHoldL_ = 0.0f;
    float peakHoldR_ = 0.0f;
    int holdCountL_ = 0;
    int holdCountR_ = 0;
};

} // namespace nova::synth

