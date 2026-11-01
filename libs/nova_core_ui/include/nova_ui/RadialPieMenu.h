#pragma once

#include "nova_ui/NovaTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>

namespace nova::ui {

/**
 * @brief Morning DAW-style Radial / Pie Context Menu.
 * Opens centered under the mouse cursor for rapid one-click parameter actions.
 */
class RadialPieMenu : public juce::Component {
public:
    struct SliceItem {
        juce::String label;
        std::function<void()> action;
    };

    RadialPieMenu(const NovaTheme& theme, std::vector<SliceItem> items);
    ~RadialPieMenu() override = default;

    static void show(juce::Component* topLevelParent,
                     juce::Point<int> centerInParent,
                     const NovaTheme& theme,
                     std::vector<SliceItem> items);

    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    [[nodiscard]] int getSliceAtPoint(juce::Point<float> localPt) const noexcept;

    NovaTheme theme_;
    std::vector<SliceItem> items_;
    juce::Point<float> menuCenter_{ 110.0f, 110.0f };
    int hoveredSlice_ = -1;
};

} // namespace nova::ui
