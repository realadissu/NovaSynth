#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <nova_ui/NovaTheme.h>
#include <nova_ui/ArcKnob.h>
#include "../NovaSynthProcessor.h"
#include <array>
#include <memory>

namespace nova::synth::ui {

/**
 * @brief Realistic Piano Key Button with authentic contours, ivory/ebony gradients,
 * drop shadows, beveled lips, and warm worship glow states.
 */
class PianoKeyButton : public juce::Button {
public:
    PianoKeyButton();
    ~PianoKeyButton() override = default;

    void setup(const juce::String& noteName, bool isBlack, const nova::ui::NovaTheme* theme);
    [[nodiscard]] bool isBlackKey() const noexcept { return isBlack_; }

    void paintButton(juce::Graphics& g, bool isHighlighted, bool isDown) override;

private:
    juce::String noteName_;
    bool isBlack_ = false;
    const nova::ui::NovaTheme* theme_ = nullptr;
};

/**
 * @brief Dedicated Worship Drone Sound Bed Hub Panel (Tab 3)
 *
 * Provides comprehensive control over the continuous ambient Drone Pad:
 * 1. Master controls: Drone Pad Hold toggle, Level, Warmth/Tone, Crossfade Glide, and Reverb Freeze.
 * 2. 12-key Chromatic Keyboard selector with instant visual key feedback.
 * 3. 4-way Harmonic Voicing selector (Lush 5th, Deep Sub, Ambient 9th, Celestial Octaves).
 * 4. Harmonic Aura real-time visualizer representing active drone resonance.
 */
class DroneHubPanel : public juce::Component, public juce::TooltipClient, private juce::Timer {
public:
    DroneHubPanel(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~DroneHubPanel() override;

    juce::String getTooltip() override;
    void timerCallback() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void drawMasterCard(juce::Graphics& g, juce::Rectangle<float> area);
    void drawKeySelectorCard(juce::Graphics& g, juce::Rectangle<float> area);
    void drawAuraCard(juce::Graphics& g, juce::Rectangle<float> area);

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;

    juce::Rectangle<int> cardMasterRect_;
    juce::Rectangle<int> cardKeysRect_;
    juce::Rectangle<int> cardAuraRect_;

    // Master Controls
    juce::TextButton droneHoldBtn_;
    juce::TextButton freezeBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> droneHoldAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAtt_;

    nova::ui::ArcKnob droneLevel_;
    nova::ui::ArcKnob droneTone_;
    nova::ui::ArcKnob droneGlide_;

    // 12 Chromatic Keys & Voicing
    std::array<PianoKeyButton, 12> keyBtns_;
    std::array<juce::TextButton, 4> voicingBtns_;
    juce::TextButton octDownBtn_;
    juce::TextButton octUpBtn_;

    juce::Rectangle<int> pianoArea_;
    float visualPhase_ = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DroneHubPanel)
};

} // namespace nova::synth::ui
