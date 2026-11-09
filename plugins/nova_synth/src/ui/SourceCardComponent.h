#pragma once

#include "nova_ui/NovaTheme.h"
#include "nova_ui/ArcKnob.h"
#include "SleekHorizontalSlider.h"
#include "../NovaSynthProcessor.h"
#include "nova_dsp/oscillators/MultiEngineOscillator.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <memory>

namespace nova::synth {

class SourceCardComponent : public juce::Component, public juce::TooltipClient {
public:
    std::function<void(int)> onCardStateChanged;
    std::function<void(int, bool)> onExpandRequested;
    std::function<void(int, bool)> onPinToggled;
    float currentWidth_ = 144.0f;

    SourceCardComponent(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme, int slotIndex);
    ~SourceCardComponent() override = default;

    void bindParameters();
    void updateDynamicLabels();
    void updateControlVisibility();

    [[nodiscard]] int getSlotIndex() const noexcept { return slotIndex_; }
    [[nodiscard]] bool isExpanded() const noexcept { return isExpanded_; }
    [[nodiscard]] bool isPinned() const noexcept { return isPinned_; }
    [[nodiscard]] const nova::ui::ArcKnob& getOctKnob() const noexcept { return octKnob_; }
    [[nodiscard]] const nova::ui::ArcKnob& getSemiKnob() const noexcept { return semiKnob_; }
    [[nodiscard]] const nova::ui::ArcKnob& getLevelKnob() const noexcept { return levelKnob_; }
    [[nodiscard]] const nova::ui::ArcKnob& getPanKnob() const noexcept { return panKnob_; }
    [[nodiscard]] const nova::ui::ArcKnob& getMorphKnob() const noexcept { return morphKnob_; }
    [[nodiscard]] const nova::ui::ArcKnob& getUnisonKnob() const noexcept { return unisonKnob_; }
    [[nodiscard]] const nova::ui::ArcKnob& getScatterKnob() const noexcept { return scatterKnob_; }
    [[nodiscard]] const nova::ui::ArcKnob& getLoopStartKnob() const noexcept { return loopStartKnob_; }
    [[nodiscard]] const nova::ui::ArcKnob& getLoopEndKnob() const noexcept { return loopEndKnob_; }
    [[nodiscard]] const nova::ui::ArcKnob& getLoopXfadeKnob() const noexcept { return loopXfadeKnob_; }
    [[nodiscard]] juce::Rectangle<int> getWavePillRect(int index) const noexcept {
        return (index >= 0 && index < 6) ? wavePillRects_[static_cast<std::size_t>(index)] : juce::Rectangle<int>{};
    }
    [[nodiscard]] juce::Rectangle<int> getVisualizerRect() const noexcept { return visualizerRect_; }
    [[nodiscard]] float getPreferredExpandedWidth() const noexcept;
    void setExpanded(bool expanded);
    void setPinned(bool pinned);
    void cycleModel(int delta);
    void openRoutingMenu();
    void openAmpEnvMenu();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    juce::String getTooltip() override;

    static juce::Colour getSlotColour(int index) noexcept;
    static juce::String getSlotShortName(int index);
    static juce::String getEngineDisplayName(const nova::dsp::MultiEngineOscillator::Parameters& slot);
    static juce::String getModelDisplayName(const nova::dsp::WorshipSynthEngine::Parameters& params, int slotIdx);
    static juce::String getFilterRoutingDisplayName(int filterRouting);
    static juce::String getAmpEnvDisplayName(int ampEnvSource);

    static void drawPillWithArrow(juce::Graphics& g, juce::Rectangle<int> rect, const juce::String& text,
                                  juce::Colour bg, juce::Colour border, juce::Colour textCol,
                                  const juce::Font& font);
    static void drawPinIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour oscCol, bool isPinned);
    static void drawWaveformIcon(juce::Graphics& g, juce::Rectangle<float> area,
                                 nova::dsp::MultiEngineOscillator::EngineType engine,
                                 int sourceType, int modelIndex, juce::Colour strokeCol, bool isActive);

private:
    void paintCompact(juce::Graphics& g, juce::Colour slotCol, const nova::dsp::MultiEngineOscillator::Parameters& slot);
    void paintExpanded(juce::Graphics& g, juce::Colour slotCol,
                       const nova::dsp::MultiEngineOscillator::Parameters& slot,
                       const nova::dsp::WorshipSynthEngine::Parameters& params);
    void drawEngineVisualizer(juce::Graphics& g, juce::Rectangle<int> area,
                              const nova::dsp::MultiEngineOscillator::Parameters& slot,
                              juce::Colour slotCol, bool isExpandedView);
    void draw3DWavetableTerrain(juce::Graphics& g, juce::Rectangle<float> bF,
                                const nova::dsp::MultiEngineOscillator::Parameters& slot, juce::Colour slotCol);
    void drawVisualizerBadge(juce::Graphics& g, juce::Rectangle<int> area,
                             const nova::dsp::MultiEngineOscillator::Parameters& slot,
                             juce::Colour slotCol, bool isExpandedView);
    void openEngineMenu();
    void openModelMenu();

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int slotIndex_ = 0;
    bool isExpanded_ = false;
    bool isPinned_ = false;

    nova::ui::ArcKnob octKnob_, semiKnob_, levelKnob_, morphKnob_, unisonKnob_, panKnob_;
    nova::ui::ArcKnob rateKnob_, scatterKnob_, durKnob_, warpKnob_;
    nova::ui::ArcKnob loopStartKnob_, loopEndKnob_, loopXfadeKnob_;

    enum class DragMarker { None, Start, LoopStart, LoopEnd };
    DragMarker activeDragMarker_ = DragMarker::None;

    SleekHorizontalSlider compactVolSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compactVolAttachment_;
    SleekHorizontalSlider spreadSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spreadAttachment_;

    juce::Rectangle<int> powerRect_, soloRect_, pinRect_, engineRect_, visualizerRect_;
    juce::Rectangle<int> modelRect_, modelPrevRect_, modelNextRect_, routingRect_, ampEnvRect_;
    juce::Rectangle<int> keyTrkRect_, lowestNoteRect_;
    std::array<juce::Rectangle<int>, 6> wavePillRects_;
    std::array<juce::Rectangle<int>, 3> subOctPillRects_;
    int hoveredWavePill_ = -1;
};

} // namespace nova::synth
