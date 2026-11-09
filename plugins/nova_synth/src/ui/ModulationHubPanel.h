#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "nova_ui/NovaTheme.h"
#include "nova_ui/ArcKnob.h"
#include "nova_dsp/modulation/ModMatrix.h"
#include "nova_dsp/modulation/LfoGenerator.h"
#include "../NovaSynthProcessor.h"
#include <array>
#include <memory>
#include <vector>

namespace nova::synth::ui {

juce::String getOscShapeParamName(const nova::dsp::MultiEngineOscillator::Parameters& slot);
juce::String getDynamicDestinationName(nova::dsp::ModMatrix::Destination dst,
                                      const nova::dsp::WorshipSynthEngine::Parameters& params);

class ModulationHubPanel : public juce::Component, public juce::TooltipClient, private juce::Timer {
public:
    struct LfoKnobPair {
        std::unique_ptr<nova::ui::ArcKnob> rate;
        std::unique_ptr<nova::ui::ArcKnob> depth;
    };

    struct MatrixRowUI {
        juce::Rectangle<int> srcRect, arrowRect, dstRect, amtRect, bypassRect, delRect;
    };

    ModulationHubPanel(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~ModulationHubPanel() override;

    void updateLfoRateKnobAttachment(int lfoIdx, bool isSynced);
    void toggleLfoSync(int lfoIdx);

    [[nodiscard]] int getLfoShapeHeight(int index = 0) const noexcept {
        return (index >= 0 && index < 4) ? lfoShapeRects_[static_cast<std::size_t>(index)].getHeight() : 0;
    }
    [[nodiscard]] int getLfoKnobHeight(int index = 0) const noexcept {
        return (index >= 0 && index < 4 && lfoKnobs_[static_cast<std::size_t>(index)].rate)
            ? lfoKnobs_[static_cast<std::size_t>(index)].rate->getHeight() : 0;
    }
    [[nodiscard]] const nova::ui::ArcKnob* getLfoRateKnob(int index = 0) const noexcept {
        return (index >= 0 && index < 4) ? lfoKnobs_[static_cast<std::size_t>(index)].rate.get() : nullptr;
    }
    [[nodiscard]] const nova::ui::ArcKnob* getLfoDepthKnob(int index = 0) const noexcept {
        return (index >= 0 && index < 4) ? lfoKnobs_[static_cast<std::size_t>(index)].depth.get() : nullptr;
    }
    [[nodiscard]] bool isLfoSynced(int index = 0) const noexcept;
    [[nodiscard]] juce::Rectangle<int> getSyncBtnRect(int index = 0) const noexcept {
        return (index >= 0 && index < 4) ? syncBtnRects_[static_cast<std::size_t>(index)] : juce::Rectangle<int>{};
    }

    void timerCallback() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    juce::String getTooltip() override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    void cycleLfoShape(int lfoIdx, bool forward);
    void addNewMatrixRoute();
    void deleteMatrixRoute(std::size_t slotIdx);
    void showSourceMenu(std::size_t slotIdx, juce::Rectangle<int> targetRect);
    void showDestinationMenu(std::size_t slotIdx, juce::Rectangle<int> targetRect);

private:
    void drawLfoCard(juce::Graphics& g, juce::Rectangle<float> area);
    void drawSyncButton(juce::Graphics& g, juce::Rectangle<float> btnRect, bool isSynced, juce::Colour col, bool isHovered = false);
    static void drawSyncIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour col);
    void drawMiniWaveform(juce::Graphics& g, juce::Rectangle<float> area, nova::dsp::LfoGenerator::Shape shape, juce::Colour col, int lfoIdx);
    void drawMacrosCard(juce::Graphics& g, juce::Rectangle<float> area);
    void drawMatrixCard(juce::Graphics& g, juce::Rectangle<float> area);

    void layoutLfoControls();
    void layoutMacroControls();
    void updateMatrixRowRects();

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;

    juce::Rectangle<int> cardLfoRect_;
    juce::Rectangle<int> cardMacrosRect_;
    juce::Rectangle<int> cardMatrixRect_;

    std::array<LfoKnobPair, 4> lfoKnobs_;
    std::array<juce::Rectangle<int>, 4> lfoShapeRects_;
    std::array<juce::Rectangle<int>, 4> syncBtnRects_;
    int hoveredSyncBtn_ = -1;

    nova::ui::ArcKnob macroWarmth_;
    nova::ui::ArcKnob macroMotion_;
    nova::ui::ArcKnob macroShimmer_;
    nova::ui::ArcKnob macroAir_;

    float whlVisual_ = 0.0f;
    float velVisual_ = 0.0f;
    float atVisual_  = 0.0f;
    float pbdVisual_ = 0.5f;

    juce::Rectangle<int> addRouteBtnRect_;
    std::vector<MatrixRowUI> matrixRowRects_;
    int matrixScrollOffset_ = 0;

    int draggingSlotIdx_ = -1;
    float dragStartAmount_ = 0.0f;
    float dragStartX_ = 0.0f;
    juce::Rectangle<float> dragAmtRect_;

    std::array<float, 4> lfoPhases_{ 0.0f, 0.25f, 0.50f, 0.75f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationHubPanel)
};

} // namespace nova::synth::ui
