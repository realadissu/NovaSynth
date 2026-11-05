#include "nova_ui/MorningLookAndFeel.h"
#include <cmath>
#include <algorithm>

namespace nova::ui {

MorningLookAndFeel::MorningLookAndFeel(NovaTheme theme)
    : theme_(theme) {
    setColour(juce::ResizableWindow::backgroundColourId, theme_.bgCanvas);
    setColour(juce::Slider::textBoxTextColourId, theme_.textPrimary);
    setColour(juce::Slider::textBoxBackgroundColourId, theme_.bgControl);
    setColour(juce::Slider::textBoxOutlineColourId, theme_.borderSubtle);
    setColour(juce::TextButton::buttonColourId, theme_.bgElevated);
    setColour(juce::TextButton::buttonOnColourId, theme_.accentPrimary.withAlpha(0.22f));
    setColour(juce::TextButton::textColourOffId, theme_.textSecondary);
    setColour(juce::TextButton::textColourOnId, theme_.textPrimary);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xfe0d1420));
}

void MorningLookAndFeel::setTheme(const NovaTheme& newTheme) noexcept {
    theme_ = newTheme;
}

void MorningLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPosProportional, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& slider) {
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Check if slider uses secondary accent via component property
    const bool useSecondary = static_cast<bool>(slider.getProperties().getWithDefault("useSecondaryAccent", false));
    const auto activeColour = useSecondary ? theme_.accentSecondary : theme_.accentPrimary;

    // 1. Inner dark flat dial body with subtle 1px rim (crisp and large)
    const float bodyRadius = radius * 0.82f;
    g.setColour(theme_.bgControl);
    g.fillEllipse(centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
    g.setColour(slider.isMouseOverOrDragging() ? activeColour.withAlpha(0.55f) : theme_.borderSubtle);
    g.drawEllipse(centre.x - bodyRadius,               centre.y - bodyRadius,
                  bodyRadius * 2.0f, bodyRadius * 2.0f, 1.2f);

    // 2. Background track arc
    const float arcRadius = radius * 0.92f;
    const float strokeWidth = std::clamp(radius * 0.12f, 2.5f, 4.5f);

    juce::Path backgroundArc;
    backgroundArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius,
                                0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(theme_.bgElevated.brighter(0.06f));
    g.strokePath(backgroundArc, juce::PathStrokeType(strokeWidth, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

    // 3. Active value arc + subtle glow
    if (slider.isEnabled() && sliderPosProportional > 0.002f) {
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius,
                               0.0f, rotaryStartAngle, toAngle, true);

        g.setColour(activeColour.withAlpha(0.18f));
        g.strokePath(valueArc, juce::PathStrokeType(strokeWidth * 2.2f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

        g.setColour(activeColour);
        g.strokePath(valueArc, juce::PathStrokeType(strokeWidth, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    // 4. Optional outer modulation ring (if "modAmount" property is set)
    const float modAmount = static_cast<float>(slider.getProperties().getWithDefault("modAmount", 0.0));
    if (std::abs(modAmount) > 0.005f) {
        const float arcSpan = rotaryEndAngle - rotaryStartAngle;
        const float modEndAngle = std::clamp(
            toAngle + modAmount * arcSpan,
            rotaryStartAngle, rotaryEndAngle);

        const float liveMod = static_cast<float>(slider.getProperties().getWithDefault("liveModOffset", 0.0));
        const bool hasLiveMotion = std::abs(liveMod) > 0.005f;

        // 4a. Modulation Range Halo (total destination depth assigned in ModMatrix)
        juce::Path modArc;
        modArc.addCentredArc(centre.x, centre.y, radius * 0.98f, radius * 0.98f,
                             0.0f, std::min(toAngle, modEndAngle), std::max(toAngle, modEndAngle), true);
        g.setColour(theme_.accentSecondary.withAlpha(hasLiveMotion ? 0.30f : 0.85f));
        g.strokePath(modArc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        // 4b. Live Animated Dynamic Sweep & Glowing Indicator Pip
        if (hasLiveMotion) {
            const float liveAngle = std::clamp(toAngle + liveMod * modAmount * arcSpan, rotaryStartAngle, rotaryEndAngle);
            if (std::abs(liveAngle - toAngle) > 0.01f) {
                juce::Path liveArc;
                liveArc.addCentredArc(centre.x, centre.y, radius * 0.98f, radius * 0.98f,
                                      0.0f, std::min(toAngle, liveAngle), std::max(toAngle, liveAngle), true);
                g.setColour(theme_.accentSecondary.withAlpha(0.90f));
                g.strokePath(liveArc, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
            }

            // 4c. Crisp Glowing Pip dancing on the rim
            const float pipRadius = radius * 0.98f;
            const float pipX = centre.x + pipRadius * std::sin(liveAngle);
            const float pipY = centre.y - pipRadius * std::cos(liveAngle);

            g.setColour(theme_.accentSecondary.withAlpha(0.40f));
            g.fillEllipse(pipX - 4.5f, pipY - 4.5f, 9.0f, 9.0f);
            g.setColour(theme_.accentSecondary);
            g.fillEllipse(pipX - 2.5f, pipY - 2.5f, 5.0f, 5.0f);
            g.setColour(juce::Colours::white);
            g.fillEllipse(pipX - 1.2f, pipY - 1.2f, 2.4f, 2.4f);
        }
    }

    // 5. Crisp pointer line + dot inside dial body or Center Label
    const auto centerLabel = slider.getProperties().getWithDefault("centerLabel", juce::String()).toString();
    const float sinA = std::sin(toAngle);
    const float cosA = -std::cos(toAngle);

    if (centerLabel.isNotEmpty()) {
        g.setFont(theme_.getTitleFont(std::clamp(bodyRadius * 1.15f, 13.0f, 22.0f)));
        g.setColour(slider.isMouseOverOrDragging() ? juce::Colours::white : activeColour.brighter(0.25f));
        g.drawText(centerLabel, juce::Rectangle<float>(centre.x - bodyRadius, centre.y - bodyRadius,
                                                       bodyRadius * 2.0f, bodyRadius * 2.0f),
                   juce::Justification::centred, false);

        // Pointer tick on the outer edge of the dial
        const float tickOuter = bodyRadius * 0.95f;
        const float tickInner = bodyRadius * 0.65f;
        g.setColour(theme_.textPrimary);
        g.drawLine(centre.x + sinA * tickInner, centre.y + cosA * tickInner,
                   centre.x + sinA * tickOuter, centre.y + cosA * tickOuter, 2.0f);
    } else {
        const float pointerLen = bodyRadius * 0.72f;
        const float pointerInner = bodyRadius * 0.28f;
        g.setColour(theme_.textPrimary);
        g.drawLine(centre.x + sinA * pointerInner, centre.y + cosA * pointerInner,
                   centre.x + sinA * pointerLen,   centre.y + cosA * pointerLen, 2.0f);
    }
}

void MorningLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& backgroundColour,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown) {
    const auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool isOn = button.getToggleState();
    const bool useWarm = static_cast<bool>(button.getProperties().getWithDefault("warmAccent", false));
    const auto accent = useWarm ? theme_.accentWarm : theme_.accentPrimary;

    // Respect custom backgroundColour if set on button or toggle state
    const bool hasCustomBg = button.isColourSpecified(juce::TextButton::buttonColourId)
                          || button.isColourSpecified(juce::TextButton::buttonOnColourId);
    juce::Colour fill = hasCustomBg ? backgroundColour
                                    : (isOn ? accent.withAlpha(0.22f) : theme_.bgElevated);

    if (shouldDrawButtonAsDown) {
        fill = fill.brighter(0.15f);
    } else if (shouldDrawButtonAsHighlighted) {
        fill = fill.brighter(0.08f);
    }

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, theme_.pillRadius);

    const bool isBrightFill = fill.getBrightness() > 0.55f;
    juce::Colour borderCol;
    if (isBrightFill) {
        borderCol = fill.brighter(0.2f);
    } else if (isOn) {
        borderCol = accent;
    } else if (shouldDrawButtonAsHighlighted) {
        borderCol = theme_.borderFocus;
    } else {
        borderCol = theme_.borderSubtle;
    }

    g.setColour(borderCol);
    g.drawRoundedRectangle(bounds, theme_.pillRadius, (isOn || isBrightFill) ? 1.4f : 1.0f);
}

void MorningLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool /*shouldDrawButtonAsDown*/) {
    g.setFont(theme_.getSectionFont(13.5f));
    const bool isOn = button.getToggleState();
    juce::Colour textCol;
    if (button.isColourSpecified(isOn ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)) {
        textCol = button.findColour(isOn ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
    } else {
        textCol = isOn ? theme_.textPrimary
                       : (shouldDrawButtonAsHighlighted ? theme_.textPrimary : theme_.textSecondary);
    }
    g.setColour(textCol);
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(4, 2),
                     juce::Justification::centred, 1, 1.0f);
}

juce::Font MorningLookAndFeel::getLabelFont(juce::Label& /*label*/) {
    return theme_.getValueFont(13.0f);
}

void MorningLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height) {
    const auto r = juce::Rectangle<int>(0, 0, width, height).toFloat();
    // Deep obsidian slate background matching Nova Synth hardware aesthetic
    g.setColour(juce::Colour(0xfe0d1420));
    g.fillRoundedRectangle(r, 6.0f);

    // Sleek cyan glowing border
    g.setColour(theme_.accentPrimary.withAlpha(0.50f));
    g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.2f);
}

void MorningLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                           bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                                           bool hasSubMenu, const juce::String& text,
                                           const juce::String& /*shortcutKeyText*/,
                                           const juce::Drawable* /*icon*/, const juce::Colour* textColour) {
    if (isSeparator) {
        auto r = area.reduced(8, 0);
        g.setColour(theme_.borderSubtle);
        g.fillRect(r.removeFromTop(1));
        return;
    }

    const auto r = area.reduced(3, 1).toFloat();

    // Text bounds with uniform left padding so all menu items align vertically
    auto textR = area.reduced(10, 0);
    textR.removeFromLeft(16);

    // 1. Background Fill and Right-Pointing Triangle ▶ indicator:
    if (isHighlighted && isActive) {
        // Solid bright Space Cyan highlight for hovered item
        g.setColour(theme_.accentPrimary);
        g.fillRoundedRectangle(r, 4.0f);

        // Solid dark right-pointing triangle ▶
        const float tx = static_cast<float>(area.getX() + 8);
        const float ty = static_cast<float>(area.getCentreY());
        juce::Path tri;
        tri.startNewSubPath(tx, ty - 4.5f);
        tri.lineTo(tx + 5.5f, ty);
        tri.lineTo(tx, ty + 4.5f);
        tri.closeSubPath();
        g.setColour(juce::Colour(0xff090d16));
        g.fillPath(tri);
    } else if (isTicked) {
        // Subtle Space Cyan tint for currently active/selected item
        g.setColour(theme_.accentPrimary.withAlpha(0.18f));
        g.fillRoundedRectangle(r, 4.0f);

        // Cyan right-pointing triangle ▶
        const float tx = static_cast<float>(area.getX() + 8);
        const float ty = static_cast<float>(area.getCentreY());
        juce::Path tri;
        tri.startNewSubPath(tx, ty - 4.5f);
        tri.lineTo(tx + 5.5f, ty);
        tri.lineTo(tx, ty + 4.5f);
        tri.closeSubPath();
        g.setColour(theme_.accentPrimary);
        g.fillPath(tri);
    }

    // 2. Text styling
    g.setFont(theme_.getValueFont(13.0f).boldened());
    if (!isActive) {
        g.setColour(theme_.textMuted);
    } else if (isHighlighted) {
        g.setColour(juce::Colour(0xff090d16)); // Bold dark obsidian text over bright cyan
    } else if (isTicked) {
        g.setColour(theme_.accentPrimary);     // Cyan text for selected item
    } else if (textColour != nullptr) {
        g.setColour(*textColour);
    } else {
        g.setColour(juce::Colour(0xffcbd5e1)); // Crisp clean slate grey
    }

    // Preserve natural casing for readability
    g.drawFittedText(text, textR, juce::Justification::centredLeft, 1, 1.0f);

    if (hasSubMenu) {
        const float x = static_cast<float>(area.getRight() - 14);
        const float y = static_cast<float>(area.getCentreY());
        juce::Path arrow;
        arrow.startNewSubPath(x, y - 4.5f);
        arrow.lineTo(x + 4.5f, y);
        arrow.lineTo(x, y + 4.5f);
        arrow.closeSubPath();
        g.setColour(isHighlighted ? juce::Colour(0xff090d16) : theme_.accentPrimary);
        g.fillPath(arrow);
    }
}

void MorningLookAndFeel::drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area,
                                                    const juce::String& sectionName) {
    g.setFont(theme_.getSectionFont(11.0f));
    g.setColour(theme_.accentPrimary);
    g.drawFittedText(sectionName.toUpperCase(), area.reduced(10, 0), juce::Justification::centredLeft, 1, 1.0f);
    g.setColour(theme_.borderSubtle);
    g.drawHorizontalLine(area.getBottom() - 1, static_cast<float>(area.getX() + 8), static_cast<float>(area.getRight() - 8));
}

juce::Font MorningLookAndFeel::getPopupMenuFont() {
    return theme_.getValueFont(13.0f);
}

void MorningLookAndFeel::getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                                   int standardMenuItemHeight, int& idealWidth, int& idealHeight) {
    if (isSeparator) {
        idealWidth = 50;
        idealHeight = standardMenuItemHeight > 0 ? standardMenuItemHeight / 10 : 6;
    } else {
        auto font = getPopupMenuFont();
        idealHeight = standardMenuItemHeight > 0 ? standardMenuItemHeight : 28;
        idealWidth = juce::GlyphArrangement::getStringWidthInt(font, text) + 64;
    }
}

void MorningLookAndFeel::drawPopupMenuUpDownArrow(juce::Graphics& g, int width, int height, bool isScrollUpArrow) {
    const float cx = static_cast<float>(width) * 0.5f;
    const float cy = static_cast<float>(height) * 0.5f;
    juce::Path p;
    if (isScrollUpArrow) {
        p.startNewSubPath(cx - 6.0f, cy + 2.5f);
        p.lineTo(cx, cy - 3.5f);
        p.lineTo(cx + 6.0f, cy + 2.5f);
    } else {
        p.startNewSubPath(cx - 6.0f, cy - 2.5f);
        p.lineTo(cx, cy + 3.5f);
        p.lineTo(cx + 6.0f, cy - 2.5f);
    }
    g.setColour(theme_.accentPrimary);
    g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void MorningLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar& /*scrollbar*/,
                                       int x, int y, int width, int height,
                                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                                       bool isMouseOver, bool /*isMouseDown*/) {
    const auto track = juce::Rectangle<int>(x, y, width, height).toFloat();
    g.setColour(theme_.bgControl.withAlpha(0.6f));
    g.fillRoundedRectangle(track, 3.0f);

    juce::Rectangle<float> thumb;
    if (isScrollbarVertical) {
        thumb = juce::Rectangle<float>(static_cast<float>(x + 1), static_cast<float>(thumbStartPosition),
                                       static_cast<float>(width - 2), static_cast<float>(thumbSize));
    } else {
        thumb = juce::Rectangle<float>(static_cast<float>(thumbStartPosition), static_cast<float>(y + 1),
                                       static_cast<float>(thumbSize), static_cast<float>(height - 2));
    }

    const auto thumbCol = isMouseOver ? theme_.accentPrimary : theme_.accentPrimary.withAlpha(0.55f);
    g.setColour(thumbCol);
    g.fillRoundedRectangle(thumb, 3.0f);
}

int MorningLookAndFeel::getDefaultScrollbarWidth() {
    return 8;
}

void MorningLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                      int buttonX, int buttonY, int buttonW, int buttonH,
                                      juce::ComboBox& box) {
    const auto r = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);
    g.setColour(theme_.bgControl);
    g.fillRoundedRectangle(r, 4.0f);

    // Glowing cyan/slate border matching reference dropdown style
    g.setColour(box.hasKeyboardFocus(true) ? theme_.accentPrimary : theme_.accentPrimary.withAlpha(0.65f));
    g.drawRoundedRectangle(r, 4.0f, 1.2f);

    // Solid filled downward triangle ▼ in bright cyan
    const float cx = static_cast<float>(buttonX + buttonW / 2);
    const float cy = static_cast<float>(buttonY + buttonH / 2);
    juce::Path p;
    p.startNewSubPath(cx - 4.5f, cy - 2.5f);
    p.lineTo(cx + 4.5f, cy - 2.5f);
    p.lineTo(cx, cy + 3.0f);
    p.closeSubPath();
    g.setColour(theme_.accentPrimary);
    g.fillPath(p);
}

void MorningLookAndFeel::drawCornerResizer(juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging) {
    juce::ignoreUnused(g, w, h, isMouseOver, isMouseDragging);
    // Intentionally empty: eliminates retro JUCE striped triangle corner resizer
}

void MorningLookAndFeel::drawDocumentWindowTitleBar(juce::DocumentWindow& window, juce::Graphics& g,
                                                     int w, int h, int titleSpaceX, int titleSpaceW,
                                                     const juce::Image* icon, bool drawTitleTextOnLeft) {
    juce::ignoreUnused(icon);
    if (w * h <= 0) return;

    // Dark glass titlebar background
    g.setColour(theme_.bgElevated);
    g.fillRect(0, 0, w, h);

    // Subtle bottom border separator
    g.setColour(theme_.borderSubtle);
    g.drawHorizontalLine(h - 1, 0.0f, static_cast<float>(w));

    // Crisp Rajdhani typography
    auto font = theme_.getTitleFont(14.0f);
    g.setFont(font);
    g.setColour(theme_.textPrimary);

    const int textW = juce::GlyphArrangement::getStringWidthInt(font, window.getName());
    int textX = drawTitleTextOnLeft ? titleSpaceX : juce::jmax(titleSpaceX, (w - textW) / 2);
    if (textX + textW > titleSpaceX + titleSpaceW)
        textX = titleSpaceX + titleSpaceW - textW;

    g.drawText(window.getName(), textX, 0, textW, h, juce::Justification::centredLeft, true);
}

} // namespace nova::ui
