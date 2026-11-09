#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <nova_ui/NovaTheme.h>
#include <nova_ui/ArcKnob.h>
#include "HorizontalViewport.h"
#include "../NovaSynthProcessor.h"
#include <array>
#include <memory>
#include <vector>

namespace nova::synth::ui {

class ArpPatternSaveOverlay;

/**
 * @brief Dedicated Worship Arpeggiator & 16/32/64-Step Rhythm Sequencer Hub Panel (Tab 4)
 */
class ArpeggiatorHubPanel : public juce::Component, public juce::TooltipClient, private juce::Timer {
public:
    friend class ArpPatternSaveOverlay;

    class StepMatrixComponent : public juce::Component {
    public:
        StepMatrixComponent(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
        std::function<void()> onModified;
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void updateStepRects(int numSteps, int stepW, int totalH);
    private:
        void handleMouse(juce::Point<int> pos, bool isClick);
        NovaSynthProcessor& proc_;
        const nova::ui::NovaTheme& theme_;
        std::vector<juce::Rectangle<int>> stepRects_;
    };

    ArpeggiatorHubPanel(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~ArpeggiatorHubPanel() override;

    juce::String getTooltip() override;
    void timerCallback() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;

    [[nodiscard]] juce::String getCurrentPresetName() const noexcept { return currentPresetName_; }
    [[nodiscard]] bool isPatternModified() const noexcept { return isModified_; }

    void markModified();
    void updatePresetDisplay();
    void selectPreviousPreset();
    void selectNextPreset();
    void showRhythmPresetsMenu();
    void openSavePresetDialog();
    void onPresetSaved(const juce::String& name);
    void setupRateKnob();
    void toggleArpSync();
    void updateMatrixLayout();

private:
    void drawCoreCard(juce::Graphics& g, juce::Rectangle<float> area);
    void drawSyncButton(juce::Graphics& g, juce::Rectangle<float> btnRect, bool isSynced, juce::Colour col, bool isHovered);
    static void drawChainIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour col);
    void drawLogicCard(juce::Graphics& g, juce::Rectangle<float> area);
    void drawMatrixCard(juce::Graphics& g, juce::Rectangle<float> area);

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;

    juce::Rectangle<int> cardCoreRect_;
    juce::Rectangle<int> cardLogicRect_;
    juce::Rectangle<int> cardMatrixRect_;

    // Core Controls
    juce::TextButton arpEnableBtn_;
    juce::TextButton arpLatchBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> arpEnableAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> arpLatchAtt_;

    nova::ui::ArcKnob arpRate_;
    nova::ui::ArcKnob arpGate_;
    nova::ui::ArcKnob arpSwing_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> arpRateAtt_;

    juce::Rectangle<int> syncBtnRect_;
    bool isSyncHovered_ = false;

    // Pattern Logic
    std::array<juce::TextButton, 6> modeBtns_;
    std::array<juce::TextButton, 4> octaveBtns_;

    // Pattern Length Selector
    juce::TextButton len16Btn_;
    juce::TextButton len32Btn_;
    juce::TextButton len64Btn_;

    // Rhythm Preset Navigation & Saving
    juce::TextButton prevPresetBtn_;
    juce::TextButton presetNameBtn_;
    juce::TextButton nextPresetBtn_;
    juce::TextButton savePresetBtn_;

    juce::String currentPresetName_{ "Straight 16 (Full Drive)" };
    bool isModified_{ false };

    // Horizontal Viewport & Step Matrix
    HorizontalViewport matrixViewport_;
    StepMatrixComponent stepMatrixComp_;
    std::unique_ptr<ArpPatternSaveOverlay> saveOverlay_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpeggiatorHubPanel)
};

} // namespace nova::synth::ui
