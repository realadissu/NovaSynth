#pragma once

#include "nova_ui/NovaTheme.h"
#include "nova_ui/ArcKnob.h"
#include "../NovaSynthProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

namespace nova::synth {

// ==============================================================================
// ExpandableEnvelopeCard: Akkordeon-Karte für DAHDSR + Velocity
// ==============================================================================
class ExpandableEnvelopeCard : public juce::Component {
public:
    enum class DragNode { None, Delay, Attack, Hold, DecaySustain, Release, Velocity };

    std::function<void(int, bool)> onExpandRequested;
    std::function<void(int, bool)> onPinToggled;

    ExpandableEnvelopeCard(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme,
                           int envIndex, const juce::String& title, juce::Colour accentCol,
                           const juce::String& prefix);
    ~ExpandableEnvelopeCard() override = default;

    void setExpanded(bool exp);
    [[nodiscard]] bool isExpanded() const noexcept;
    void setPinned(bool pin);
    [[nodiscard]] bool isPinned() const noexcept;

    [[nodiscard]] juce::Point<float> getNodeDel() const noexcept;
    [[nodiscard]] juce::Point<float> getNodeAtk() const noexcept;
    [[nodiscard]] juce::Point<float> getNodeHold() const noexcept;
    [[nodiscard]] juce::Point<float> getNodeDec() const noexcept;
    [[nodiscard]] juce::Point<float> getNodeSusEnd() const noexcept;
    [[nodiscard]] juce::Point<float> getNodeRel() const noexcept;
    [[nodiscard]] const nova::ui::ArcKnob& getAtkKnob() const noexcept;
    [[nodiscard]] const nova::ui::ArcKnob& getDecKnob() const noexcept;
    [[nodiscard]] const nova::ui::ArcKnob& getSusKnob() const noexcept;
    [[nodiscard]] const nova::ui::ArcKnob& getRelKnob() const noexcept;

    void updateKnobVisibility();
    void paint(juce::Graphics& g) override;
    void resized() override;

    [[nodiscard]] DragNode getHitNode(juce::Point<float> pos) const noexcept;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    float currentWidth_ = 135.0f;

private:
    void drawAdsrCurve(juce::Graphics& g, juce::Rectangle<float> area);
    void drawHandleDot(juce::Graphics& g, float x, float y, float radius, bool isHighlighted);
    static void drawPinIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour oscCol, bool isPinned);

    [[maybe_unused]] NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int envIndex_ = 0;
    juce::String title_;
    juce::Colour accentCol_;
    juce::String prefix_;
    bool isExpanded_ = false;
    bool isPinned_ = false;

    nova::ui::ArcKnob delKnob_, atkKnob_, holdKnob_, decKnob_, susKnob_, relKnob_;
    juce::Slider velSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velAttachment_;

    juce::Rectangle<int> pinRect_, velBox_;
    juce::Rectangle<float> curveBox_;

    float cachedDelW_ = 0.0f, cachedAtkW_ = 20.0f, cachedHoldW_ = 0.0f, cachedDecW_ = 20.0f, cachedSusW_ = 14.0f;
    DragNode activeDrag_ = DragNode::None;
    DragNode hoveredNode_ = DragNode::None;
    juce::Point<float> dragStartPos_;
    float dragStartDel_ = 0.0f, dragStartAtk_ = 0.45f, dragStartHold_ = 0.0f;
    float dragStartDec_ = 1.40f, dragStartSus_ = 0.82f, dragStartRel_ = 2.20f;
    juce::Point<float> nodeDel_, nodeAtk_, nodeHold_, nodeDec_, nodeSusEnd_, nodeRel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExpandableEnvelopeCard)
};

} // namespace nova::synth
