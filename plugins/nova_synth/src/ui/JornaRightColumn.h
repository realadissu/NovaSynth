#pragma once

#include "nova_ui/NovaTheme.h"
#include "nova_ui/ArcKnob.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace nova::synth {

class NovaSynthProcessor;

class JornaRightColumn : public juce::Component {
public:
    enum class DragNode { None, Peak, SusStart, SusEnd, End };

    struct EnvParamIds {
        juce::String attack;
        juce::String decay;
        juce::String sustain;
        juce::String release;
    };

    JornaRightColumn(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~JornaRightColumn() override = default;

    void selectFilterTab(int fltIdx);
    void selectEnvelopeTab(int envIdx);
    EnvParamIds getActiveEnvParamIds() const;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

private:
    [[maybe_unused]] NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int activeFilterTab_ = 0;
    int activeEnvTab_ = 0;

    juce::Rectangle<int> flt1TabRect_;
    juce::Rectangle<int> flt2TabRect_;
    juce::Rectangle<int> fltTypeRect_;
    std::vector<juce::Rectangle<int>> envTabRects_;
    juce::Rectangle<float> envGraphRect_;

    DragNode activeDragNode_ = DragNode::None;
    DragNode hoveredNode_ = DragNode::None;

    juce::Point<float> pPeak_{};
    juce::Point<float> pSusStart_{};
    juce::Point<float> pSusEnd_{};
    juce::Point<float> pEnd_{};
    float graphX0_ = 0.0f;
    float graphW_ = 0.0f;
    float graphYB_ = 0.0f;
    float graphH_ = 0.0f;

    nova::ui::ArcKnob cutoffKnob_;
    nova::ui::ArcKnob resoKnob_;
    nova::ui::ArcKnob carveKnob_;
    nova::ui::ArcKnob driveKnob_;
    nova::ui::ArcKnob envAmtKnob_;

    nova::ui::ArcKnob atkKnob_;
    nova::ui::ArcKnob decKnob_;
    nova::ui::ArcKnob susKnob_;
    nova::ui::ArcKnob relKnob_;
};

} // namespace nova::synth
