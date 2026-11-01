#pragma once

#include "nova_ui/NovaTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova::ui {

/**
 * @brief Morning DAW / JORNA-inspired Vector LookAndFeel.
 * Replaces default JUCE widgets with minimalist flat cards, precision vector arcs, and crisp typography.
 */
class MorningLookAndFeel : public juce::LookAndFeel_V4 {
public:
    explicit MorningLookAndFeel(NovaTheme theme = NovaTheme::create(NovaTheme::AccentPreset::SpaceCyan));
    ~MorningLookAndFeel() override = default;

    void setTheme(const NovaTheme& newTheme) noexcept;
    [[nodiscard]] const NovaTheme& getTheme() const noexcept { return theme_; }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    juce::Font getLabelFont(juce::Label& label) override;

    // Custom Cyberpunk / Modern Worship Popup & Dropdown Styling
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                           bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;

    void drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area,
                                    const juce::String& sectionName) override;

    juce::Font getPopupMenuFont() override;
    int getPopupMenuBorderSize() override { return 4; }
    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                   int standardMenuItemHeight, int& idealWidth, int& idealHeight) override;

    void drawPopupMenuUpDownArrow(juce::Graphics& g, int width, int height, bool isScrollUpArrow) override;
    void drawScrollbar(juce::Graphics& g, juce::ScrollBar& scrollbar,
                       int x, int y, int width, int height,
                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                       bool isMouseOver, bool isMouseDown) override;
    int getDefaultScrollbarWidth() override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    // Window chrome & native styling overrides
    void drawCornerResizer(juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging) override;
    void drawDocumentWindowTitleBar(juce::DocumentWindow& window, juce::Graphics& g,
                                    int w, int h, int titleSpaceX, int titleSpaceW,
                                    const juce::Image* icon, bool drawTitleTextOnLeft) override;

private:
    NovaTheme theme_;
};

} // namespace nova::ui
