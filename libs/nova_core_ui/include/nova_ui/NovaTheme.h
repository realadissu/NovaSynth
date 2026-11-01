#pragma once

#include <juce_graphics/juce_graphics.h>

namespace nova::ui {

/**
 * @brief Central design tokens and typography for the Morning DAW / JORNA-inspired UI kit.
 */
struct NovaTheme {
    enum class AccentPreset {
        SpaceCyan,    // Nova Space: Ice Cyan + Lush Violet
        WorshipGold,  // Nova Synth: Warm Amber Gold + Teal
        ConcertIvory  // Nova Piano: Warm Copper + Champagne Gold
    };

    // Base Dark-Flat Surfaces
    juce::Colour bgHeader     { 0xff0c0e12 };
    juce::Colour bgCanvas     { 0xff11141a };
    juce::Colour bgSurface    { 0xff171b23 };
    juce::Colour bgElevated   { 0xff1f2530 };
    juce::Colour bgControl    { 0xff13171e };

    // Hairline Borders
    juce::Colour borderSubtle { 0xff262d3b };
    juce::Colour borderFocus  { 0xff3b465c };

    // Typography Colors
    juce::Colour textPrimary  { 0xfff1f5f9 };
    juce::Colour textSecondary{ 0xff94a3b8 };
    juce::Colour textMuted    { 0xff64748b };

    // Dynamic Plugin Accents
    juce::Colour accentPrimary   { 0xff38bdf8 }; // Primary arc / active state
    juce::Colour accentSecondary { 0xffa855f7 }; // Secondary engine / modulation ring
    juce::Colour accentWarm      { 0xfff59e0b }; // Highlight / warning / freeze
    juce::Colour meterOk         { 0xff10b981 };

    float cornerRadius = 8.0f;
    float pillRadius   = 5.0f;

    enum class TextScale : int {
        Small = 0,   // Standard compact (1.12x)
        Normal = 1,  // Enhanced readability (1.32x) - Default
        Large = 2    // Stage visibility (1.52x)
    };

    void setTextScale(TextScale scale) noexcept {
        textScale_ = scale;
        switch (scale) {
            case TextScale::Small:  textScaleFactor_ = 1.12f; break;
            case TextScale::Normal: textScaleFactor_ = 1.32f; break;
            case TextScale::Large:  textScaleFactor_ = 1.52f; break;
        }
    }

    [[nodiscard]] TextScale getTextScale() const noexcept { return textScale_; }
    [[nodiscard]] float getTextScaleFactor() const noexcept { return textScaleFactor_; }

    static NovaTheme create(AccentPreset preset) noexcept;

    [[nodiscard]] juce::Font getTitleFont(float height = 18.0f) const;
    [[nodiscard]] juce::Font getSectionFont(float height = 13.5f) const;
    [[nodiscard]] juce::Font getLabelFont(float height = 12.0f) const;
    [[nodiscard]] juce::Font getValueFont(float height = 12.5f) const;

private:
    TextScale textScale_ = TextScale::Normal;
    float textScaleFactor_ = 1.32f;
};

} // namespace nova::ui
