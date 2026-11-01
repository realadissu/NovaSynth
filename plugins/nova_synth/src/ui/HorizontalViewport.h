#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>

namespace nova::synth {

/**
 * @brief Horizontal Viewport with bidirectional mousewheel support and sleek modern scrollbars.
 */
class HorizontalViewport : public juce::Viewport {
public:
    HorizontalViewport() {
        setScrollBarsShown(false, true, false, true);
        setScrollBarThickness(6);
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        float d = 0.0f;
        if (std::abs(wheel.deltaX) > 0.0001f) {
            d = wheel.deltaX;
        } else {
            d = wheel.deltaY;
        }

        if (std::abs(d) > 0.00001f) {
            const float factor = wheel.isSmooth ? 240.0f : 55.0f;
            accumulatedScroll_ -= d * factor;

            const int pixelMove = static_cast<int>(accumulatedScroll_);
            if (pixelMove != 0) {
                accumulatedScroll_ -= static_cast<float>(pixelMove);
                const int currentX = getViewPositionX();
                const int maxX = std::max(0, (getViewedComponent() != nullptr ? getViewedComponent()->getWidth() : 0) - getWidth());
                const int targetX = std::clamp(currentX + pixelMove, 0, maxX);
                setViewPosition(targetX, 0);
            }
        } else {
            juce::Viewport::mouseWheelMove(e, wheel);
        }
    }

private:
    float accumulatedScroll_ = 0.0f;
};

} // namespace nova::synth
