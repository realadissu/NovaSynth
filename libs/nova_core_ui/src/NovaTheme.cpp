#include "nova_ui/NovaTheme.h"
#include "BinaryData.h"

namespace nova::ui {

namespace {
juce::Typeface::Ptr getSharedTypefaceBold() {
    static auto tf = juce::Typeface::createSystemTypefaceFor(
        NovaResources::RajdhaniBold_ttf, NovaResources::RajdhaniBold_ttfSize);
    return tf;
}

juce::Typeface::Ptr getSharedTypefaceSemiBold() {
    static auto tf = juce::Typeface::createSystemTypefaceFor(
        NovaResources::RajdhaniSemiBold_ttf, NovaResources::RajdhaniSemiBold_ttfSize);
    return tf;
}

juce::Typeface::Ptr getSharedTypefaceMedium() {
    static auto tf = juce::Typeface::createSystemTypefaceFor(
        NovaResources::RajdhaniMedium_ttf, NovaResources::RajdhaniMedium_ttfSize);
    return tf;
}
} // namespace

NovaTheme NovaTheme::create(AccentPreset preset) noexcept {
    NovaTheme theme;
    switch (preset) {
        case AccentPreset::SpaceCyan:
            theme.accentPrimary   = juce::Colour(0xff38bdf8); // Crisp Ice Cyan
            theme.accentSecondary = juce::Colour(0xffa855f7); // Lush Cloud Violet
            theme.accentWarm      = juce::Colour(0xfff59e0b); // Amber Freeze/Glow
            break;
        case AccentPreset::WorshipGold:
            theme.accentPrimary   = juce::Colour(0xfff59e0b); // Worship Warm Gold
            theme.accentSecondary = juce::Colour(0xff2dd4bf); // Shimmer Teal
            theme.accentWarm      = juce::Colour(0xfff43f5e); // Coral Accent
            break;
        case AccentPreset::ConcertIvory:
            theme.accentPrimary   = juce::Colour(0xfffbbf24); // Champagne Brass
            theme.accentSecondary = juce::Colour(0xfffb7185); // Warm Rosewood
            theme.accentWarm      = juce::Colour(0xff38bdf8); // Stage Presence Blue
            break;
    }
    return theme;
}

juce::Font NovaTheme::getTitleFont(float height) const {
    return juce::Font(juce::FontOptions(getSharedTypefaceBold()).withHeight(height * textScaleFactor_));
}

juce::Font NovaTheme::getSectionFont(float height) const {
    return juce::Font(juce::FontOptions(getSharedTypefaceSemiBold()).withHeight(height * textScaleFactor_));
}

juce::Font NovaTheme::getLabelFont(float height) const {
    return juce::Font(juce::FontOptions(getSharedTypefaceMedium()).withHeight(height * textScaleFactor_));
}

juce::Font NovaTheme::getValueFont(float height) const {
    return juce::Font(juce::FontOptions(getSharedTypefaceSemiBold()).withHeight(height * textScaleFactor_));
}

} // namespace nova::ui
