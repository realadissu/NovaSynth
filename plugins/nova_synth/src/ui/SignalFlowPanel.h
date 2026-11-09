#pragma once

#include "nova_ui/NovaTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include <array>

namespace nova::synth {

class NovaSynthProcessor;

class SignalFlowPanel : public juce::Component {
public:
    std::function<void(int)> onSelectOsc;
    std::function<void()> onSourceStateChanged;
    std::function<void()> onFilterStateChanged;

    SignalFlowPanel(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~SignalFlowPanel() override = default;

    void setSelectedOscIndex(int idx);
    [[nodiscard]] int getSelectedOscIndex() const noexcept { return selectedOscIndex_; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    void drawSignalFlowView(juce::Graphics& g, juce::Rectangle<float> area);

    void drawToggleSwitch(juce::Graphics& g, const juce::Rectangle<int>& rect, bool isOn, juce::Colour onColor);
    void drawJackPin(juce::Graphics& g, juce::Point<float> pt, juce::Colour col, bool isConnected, bool isHovered);
    void drawPatchCable(juce::Graphics& g, juce::Point<float> p1, juce::Point<float> p2, juce::Colour col,
                        bool isBypass = false, float bypassY = 0.0f, bool isDimmed = false);

    void handleFlowMouseDown(juce::Point<int> pos);
    void handleFlowMouseDrag(const juce::MouseEvent& event);
    void handleFlowMouseUp(const juce::MouseEvent& event);

private:
    void drawSourcesCard(juce::Graphics& g, const juce::Rectangle<float>& cardSources);
    void drawFiltersCard(juce::Graphics& g, const juce::Rectangle<float>& cardFilters);
    void drawAmpCard(juce::Graphics& g, const juce::Rectangle<float>& cardAmp, float commonFxY);
    void drawFxCard(juce::Graphics& g, const juce::Rectangle<float>& cardFx, float H, float commonFxY);
    void drawOutputCard(juce::Graphics& g, const juce::Rectangle<float>& cardOutput, float commonFxY);
    void drawPatchCablesStage(juce::Graphics& g);

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int selectedOscIndex_ = 0;
    int sourceScrollOffset_ = 0;
    float wheelAccumulator_ = 0.0f;

    juce::Rectangle<int> cardSourcesRect_;
    std::vector<juce::Rectangle<int>> flowOscPillRects_;
    std::vector<juce::Rectangle<int>> flowOscRingRects_;
    std::vector<juce::Rectangle<int>> flowOscJackRects_;
    std::vector<juce::Point<float>> oscOutPins_;

    juce::Rectangle<int> scrollUpRect_;
    juce::Rectangle<int> scrollDownRect_;
    juce::Rectangle<int> scrollTrackRect_;

    juce::Rectangle<int> toggleSourcesRect_;
    juce::Rectangle<int> toggleFiltersRect_;
    juce::Rectangle<int> toggleFxRect_;

    std::array<bool, 8> cachedOscEnabledState_ = { true, true, true, false, false, false, false, false };
    bool cachedFlt1Enabled_ = true;
    bool cachedFlt2Enabled_ = true;

    juce::Rectangle<int> flt1PillRect_;
    juce::Rectangle<int> flt2PillRect_;
    juce::Rectangle<int> flt1ToggleRect_;
    juce::Rectangle<int> flt2ToggleRect_;
    juce::Rectangle<int> dualTargetRect_;
    juce::Rectangle<int> directGutterRect_;
    juce::Point<float> flt1InPin_{};
    juce::Point<float> flt1OutPin_{};
    juce::Point<float> flt2InPin_{};
    juce::Point<float> flt2OutPin_{};
    float directGutterY_ = 0.0f;

    juce::Rectangle<int> cardAmpRect_;
    juce::Point<float> ampFltInPin_{};
    juce::Point<float> ampDirectInPin_{};
    juce::Point<float> ampOutPin_{};

    juce::Rectangle<int> delaySlotRect_;
    juce::Rectangle<int> reverbSlotRect_;
    juce::Rectangle<int> delayToggleRect_;
    juce::Rectangle<int> reverbToggleRect_;
    bool cachedDelayEnabled_ = true;
    float cachedReverbMix_ = 0.42f;
    juce::Point<float> delayInPin_{};
    juce::Point<float> delayOutPin_{};
    juce::Point<float> reverbInPin_{};
    juce::Point<float> reverbOutPin_{};

    juce::Point<float> outSummingJack_{};
    juce::Rectangle<int> masterFaderTrackRect_;
    juce::Rectangle<int> masterFaderThumbRect_;

    bool isDraggingCable_ = false;
    int dragSourceOscIndex_ = -1;
    juce::Point<float> dragStartPos_{};
    juce::Point<float> dragCurrentPos_{};
    int hoveredTarget_ = -1;
    bool isDraggingMasterFader_ = false;
};

} // namespace nova::synth

