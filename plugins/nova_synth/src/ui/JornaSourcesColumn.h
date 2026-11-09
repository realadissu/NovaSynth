#pragma once

#include "nova_ui/NovaTheme.h"
#include "nova_ui/ArcKnob.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

namespace nova::synth {

class NovaSynthProcessor;

class JornaSourcesColumn : public juce::Component {
public:
    std::function<void(int)> onSelectedOscChanged;

    JornaSourcesColumn(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~JornaSourcesColumn() override = default;

    [[nodiscard]] int getSelectedOscIndex() const noexcept { return selectedOsc_; }

    void rebindSelectedOscillator(int newIndex);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

private:
    void openSourceTypeMenuForSlot(int slotIdx, juce::Rectangle<int> targetRect);
    void openEngineMenuForSlot(int slotIdx, juce::Rectangle<int> targetRect);
    void openModelMenuForSlot(int slotIdx, juce::Rectangle<int> targetRect, juce::Point<int> clickPos);
    void openFilterRoutingMenuForSlot(int slotIdx, juce::Rectangle<int> targetRect);
    void updateSpreadFromMouse(float mouseX);

    [[maybe_unused]] NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int selectedOsc_ = 0;

    std::vector<juce::Rectangle<int>> rowRects_;
    std::vector<juce::Rectangle<int>> toggleRingRects_;
    std::vector<juce::Rectangle<int>> namePillRects_;
    std::vector<juce::Rectangle<int>> engineDropRects_;
    std::vector<juce::Rectangle<int>> filterRoutingRects_;
    juce::Rectangle<int> addLayerBtnBounds_;
    juce::Rectangle<int> remLayerBtnBounds_;
    juce::Rectangle<int> waveCycleRect_;
    juce::Rectangle<int> spreadBarRect_;

    nova::ui::ArcKnob levelKnob_;
    nova::ui::ArcKnob posKnob_;
    nova::ui::ArcKnob sprayKnob_;
    nova::ui::ArcKnob panKnob_;
    nova::ui::ArcKnob pitchKnob_;
    nova::ui::ArcKnob semiKnob_;
    nova::ui::ArcKnob driveKnob_;
    nova::ui::ArcKnob carveKnob_;
};

} // namespace nova::synth

