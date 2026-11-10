#pragma once

#include "NovaSpaceProcessor.h"
#include "nova_ui/NovaTheme.h"
#include "nova_ui/MorningLookAndFeel.h"
#include "nova_ui/PluginShell.h"
#include "nova_ui/SectionDeck.h"
#include "nova_ui/ArcKnob.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>
#include <vector>

namespace nova::space {

class ReverbSpaceVisualizer : public juce::Component {
public:
    ReverbSpaceVisualizer(NovaSpaceProcessor& proc, const nova::ui::NovaTheme& theme);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDrag(const juce::MouseEvent& event) override;

private:
    void drawEarlyReflections(juce::Graphics& g, juce::Rectangle<float> graphArea,
                              const nova::dsp::DualEngineReverb::Parameters& params);
    void drawDecayEnvelope(juce::Graphics& g, juce::Rectangle<float> graphArea,
                           const nova::dsp::DualEngineReverb::Parameters& params,
                           bool isLush, juce::Colour activeAccent);
    void drawEnergyBars(juce::Graphics& g, juce::Rectangle<float> graphArea,
                        const nova::dsp::DualEngineReverb::Telemetry& tel,
                        juce::Colour activeAccent);
    void drawReadoutText(juce::Graphics& g, juce::Rectangle<float> graphArea,
                         const nova::dsp::DualEngineReverb::Parameters& params,
                         const nova::dsp::DualEngineReverb::Telemetry& tel);
    void setChoiceParam(const juce::String& paramId, int choiceIndex);

    NovaSpaceProcessor& proc_;
    nova::ui::NovaTheme theme_;
    std::array<juce::TextButton, 2> engineBtns_;
    std::array<juce::TextButton, 4> spaceBtns_;
    juce::TextButton freezeBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttach_;
    float animPhase_ = 0.0f;
};

class KnobRowPanel : public juce::Component {
public:
    struct KnobSpec {
        juce::String paramId;
        juce::String title;
        juce::String suffix;
        bool secondaryAccent = false;
    };

    KnobRowPanel(juce::AudioProcessorValueTreeState& apvts,
                 const nova::ui::NovaTheme& theme,
                 const std::vector<KnobSpec>& specs);
    void resized() override;

private:
    std::vector<std::unique_ptr<nova::ui::ArcKnob>> knobs_;
};

class SpaceMainPage : public juce::Component {
public:
    SpaceMainPage(NovaSpaceProcessor& proc, const nova::ui::NovaTheme& theme);
    void resized() override;

private:
    ReverbSpaceVisualizer visualizer_;
    nova::ui::SectionDeck engineDeck_;
    nova::ui::SectionDeck toneOutputDeck_;
};

class ToneEqPage : public juce::Component {
public:
    ToneEqPage(NovaSpaceProcessor& proc, const nova::ui::NovaTheme& theme);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    NovaSpaceProcessor& proc_;
    nova::ui::NovaTheme theme_;
    nova::ui::SectionDeck eqControlsDeck_;
};

class NovaSpaceEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit NovaSpaceEditor(NovaSpaceProcessor& proc);
    ~NovaSpaceEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    void configureParentWindow();

    NovaSpaceProcessor& processor_;
    nova::ui::NovaTheme theme_;
    nova::ui::MorningLookAndFeel lookAndFeel_;
    nova::ui::PluginShell shell_;

    juce::Component::SafePointer<juce::DocumentWindow> parentDocWindow_;
    juce::Component::SafePointer<juce::Button> standaloneOptionsButton_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NovaSpaceEditor)
};

} // namespace nova::space
