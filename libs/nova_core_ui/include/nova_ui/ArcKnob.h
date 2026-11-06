#pragma once

#include "nova_ui/NovaTheme.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

namespace nova::ui {

/**
 * @brief Morning DAW / JORNA style compact rotary arc control.
 * Features a crisp top label, precision vector dial with modulation ring,
 * clean borderless value readout, and right-click Radial Pie Menu.
 */
class ArcKnob : public juce::Component {
public:
    ArcKnob(const juce::String& labelText,
            const juce::String& suffix,
            const NovaTheme& theme,
            bool useSecondaryAccent = false);
    ~ArcKnob() override = default;

    void attachToParameter(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId);
    void setModulation(float rangeDelta, float liveOffset = 0.0f);
    void setModulationAmount(float normalizedDelta);
    void setLabel(const juce::String& newLabel);
    void setSuffix(const juce::String& newSuffix);
    void setCenterLabel(const juce::String& text);

    [[nodiscard]] juce::Slider& getSlider() noexcept { return slider_; }
    [[nodiscard]] const juce::Slider& getSlider() const noexcept { return slider_; }
    [[nodiscard]] const juce::String& getSuffix() const noexcept { return suffix_; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        if (auto* parent = getParentComponent()) {
            parent->mouseWheelMove(e, wheel);
        }
    }

private:
    void openRadialMenu(juce::Point<int> screenPos);
    void setupTextFormatters();

    juce::String labelText_;
    juce::String suffix_;
    const NovaTheme& theme_;
    juce::Slider slider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
    float defaultValue_ = 0.0f;
    bool hasCenterLabel_ = false;
    bool isChoiceParam_ = false;
};

} // namespace nova::ui
