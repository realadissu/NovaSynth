#include "SleekHorizontalSlider.h"
#include <algorithm>
#include <cmath>

namespace nova::synth {

SleekHorizontalSlider::SleekHorizontalSlider(const juce::String& label, const nova::ui::NovaTheme& theme)
    : label_(label), theme_(theme) {
    setSliderStyle(juce::Slider::LinearHorizontal);
    setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    setScrollWheelEnabled(false);
}

void SleekHorizontalSlider::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    if (auto* vp = findParentComponentOfClass<juce::Viewport>()) {
        vp->mouseWheelMove(e, wheel);
    }
}

void SleekHorizontalSlider::setAccentColour(juce::Colour c) noexcept {
    accentCol_ = c;
    repaint();
}

void SleekHorizontalSlider::setCustomLabel(const juce::String& l) noexcept {
    label_ = l;
    repaint();
}

void SleekHorizontalSlider::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    const auto trackY = bounds.getCentreY();
    const float trackH = 4.0f;

    // Label auf der linken Seite
    float leftPad = 0.0f;
    if (label_.isNotEmpty()) {
        leftPad = 48.0f;
        auto lblRect = juce::Rectangle<float>(0.0f, 0.0f, leftPad - 5.0f, bounds.getHeight());
        g.setFont(theme_.getLabelFont(8.5f));
        g.setColour(theme_.textMuted);
        g.drawText(label_, lblRect, juce::Justification::centredRight, false);
    }

    // Value auf der rechten Seite
    const float rightPad = 38.0f;
    auto valRect = juce::Rectangle<float>(bounds.getRight() - rightPad + 4.0f, 0.0f, rightPad - 4.0f, bounds.getHeight());
    g.setFont(theme_.getValueFont(8.5f));
    g.setColour(juce::Colours::white.withAlpha(0.9f));

    juce::String valStr;
    const double val = getValue();
    if (getMaximum() <= 1.05 && getMinimum() >= 0.0) {
        valStr = juce::String(static_cast<int>(std::round(val * 100.0))) + "%";
    } else {
        valStr = juce::String(val, 2);
    }
    g.drawText(valStr, valRect, juce::Justification::centredLeft, false);

    // Track-Bereich
    const float trackX = leftPad;
    const float trackW = std::max(10.0f, bounds.getRight() - rightPad - trackX);
    const auto trackRect = juce::Rectangle<float>(trackX, trackY - trackH * 0.5f, trackW, trackH);

    // Track Background Trough
    g.setColour(theme_.bgControl.brighter(0.06f));
    g.fillRoundedRectangle(trackRect, trackH * 0.5f);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(trackRect, trackH * 0.5f, 0.7f);

    // Filled Active Track
    const double normVal = (getValue() - getMinimum()) / std::max(0.0001, getMaximum() - getMinimum());
    const float fillW = static_cast<float>(std::clamp(normVal, 0.0, 1.0)) * trackW;
    const auto col = accentCol_.isTransparent() ? theme_.accentPrimary : accentCol_;
    if (fillW > 1.0f) {
        const auto fillRect = juce::Rectangle<float>(trackX, trackY - trackH * 0.5f, fillW, trackH);
        g.setColour(col.withAlpha(0.85f));
        g.fillRoundedRectangle(fillRect, trackH * 0.5f);
    }

    // Thumb Capsule Handle
    const float thumbX = trackX + fillW;
    const float thumbW = 7.0f;
    const float thumbH = 12.0f;
    const auto thumbRect = juce::Rectangle<float>(thumbX - thumbW * 0.5f, trackY - thumbH * 0.5f, thumbW, thumbH);

    if (isMouseOverOrDragging()) {
        g.setColour(col.withAlpha(0.35f));
        g.fillRoundedRectangle(thumbRect.expanded(1.5f), 3.5f);
    }
    g.setColour(theme_.bgElevated.brighter(0.2f));
    g.fillRoundedRectangle(thumbRect, 2.5f);
    g.setColour(col);
    g.drawRoundedRectangle(thumbRect, 2.5f, 1.0f);
}

} // namespace nova::synth

