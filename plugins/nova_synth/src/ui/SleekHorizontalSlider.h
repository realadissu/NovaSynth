#pragma once

#include "nova_ui/NovaTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova::synth {

class SleekHorizontalSlider : public juce::Slider {
public:
    SleekHorizontalSlider(const juce::String& label, const nova::ui::NovaTheme& theme);
    ~SleekHorizontalSlider() override = default;

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void setAccentColour(juce::Colour c) noexcept;
    void setCustomLabel(const juce::String& l) noexcept;
    void paint(juce::Graphics& g) override;

private:
    juce::String label_;
    const nova::ui::NovaTheme& theme_;
    juce::Colour accentCol_{};
};

} // namespace nova::synth

