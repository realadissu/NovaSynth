#pragma once

#include "nova_ui/NovaTheme.h"
#include "nova_plugin/NovaProcessorBase.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace nova::ui {

/**
 * @brief Modern Studio Floating Settings Overlay Dialog
 * Replaces OS popup menus with a high-end modal dialog styled in Nova Synth obsidian aesthetic.
 */
class SettingsOverlayDialog : public juce::Component {
public:
    SettingsOverlayDialog(nova::plugin::NovaProcessorBase& processor,
                          const NovaTheme& theme,
                          std::function<void(NovaTheme::TextScale)> onScaleChanged,
                          std::function<void()> onOpenAudioSettings,
                          std::function<void()> onClose)
        : processor_(processor),
          theme_(theme),
          onScaleChanged_(std::move(onScaleChanged)),
          onOpenAudioSettings_(std::move(onOpenAudioSettings)),
          onClose_(std::move(onClose)) {
        setAlwaysOnTop(true);
        setWantsKeyboardFocus(true);

        addAndMakeVisible(closeBtn_);
        closeBtn_.onClick = [this]() { if (onClose_) onClose_(); };

        addAndMakeVisible(doneBtn_);
        doneBtn_.onClick = [this]() { if (onClose_) onClose_(); };

        // Scale Buttons
        addAndMakeVisible(scaleSmallBtn_);
        addAndMakeVisible(scaleNormalBtn_);
        addAndMakeVisible(scaleLargeBtn_);

        scaleSmallBtn_.onClick = [this]() { setScale(NovaTheme::TextScale::Small); };
        scaleNormalBtn_.onClick = [this]() { setScale(NovaTheme::TextScale::Normal); };
        scaleLargeBtn_.onClick = [this]() { setScale(NovaTheme::TextScale::Large); };

        // Voicing Buttons
        addAndMakeVisible(voiceLowestBtn_);
        addAndMakeVisible(voicePolyBtn_);

        voiceLowestBtn_.onClick = [this]() { setSubLowest(true); };
        voicePolyBtn_.onClick = [this]() { setSubLowest(false); };

        // Audio Settings Button
        if (onOpenAudioSettings_ != nullptr) {
            addAndMakeVisible(audioSetupBtn_);
            audioSetupBtn_.onClick = [this]() {
                if (onClose_) onClose_();
                if (onOpenAudioSettings_) onOpenAudioSettings_();
            };
        }

        refreshStates();
    }

    void refreshStates() {
        const auto curScale = theme_.getTextScale();
        styleToggle(scaleSmallBtn_, curScale == NovaTheme::TextScale::Small);
        styleToggle(scaleNormalBtn_, curScale == NovaTheme::TextScale::Normal);
        styleToggle(scaleLargeBtn_, curScale == NovaTheme::TextScale::Large);

        auto* subLowestParam = processor_.getAPVTS().getParameter("subLowestOnly");
        const bool isLowest = (subLowestParam == nullptr || subLowestParam->getValue() > 0.5f);
        styleToggle(voiceLowestBtn_, isLowest);
        styleToggle(voicePolyBtn_, !isLowest);
    }

    void paint(juce::Graphics& g) override {
        // 1. Semi-transparent dark scrim backdrop
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.fillRect(getLocalBounds());

        // 2. Dialog Card bounds
        const auto card = getCardBounds().toFloat();

        // Card Drop Shadow
        g.setColour(juce::Colours::black.withAlpha(0.70f));
        g.fillRoundedRectangle(card.translated(0.0f, 6.0f), 10.0f);

        // Solid Deep Obsidian Desk Background
        g.setColour(juce::Colour(0xff090e17));
        g.fillRoundedRectangle(card, 10.0f);

        // Border
        g.setColour(juce::Colour(0xff1e2d42));
        g.drawRoundedRectangle(card, 10.0f, 1.2f);

        // Header Accent Strip
        auto headerR = card.withHeight(48.0f).reduced(16.0f, 0.0f);
        g.setFont(theme_.getSectionFont(15.0f));
        g.setColour(theme_.accentPrimary);
        g.drawText("SETTINGS & PREFERENCES", headerR, juce::Justification::centredLeft);

        g.setColour(juce::Colour(0xff1e2d42));
        g.drawHorizontalLine(static_cast<int>(card.getY() + 48.0f), card.getX(), card.getRight());

        // Section Headers
        g.setFont(theme_.getLabelFont(10.5f));
        g.setColour(juce::Colour(0xff94a3b8));

        const float cX = card.getX() + 20.0f;
        g.drawText("UI LABEL & TYPOGRAPHY SCALE", cX, card.getY() + 58.0f, 300.0f, 16.0f, juce::Justification::centredLeft);
        g.drawText("SUB-BASS VOICING MODE", cX, card.getY() + 138.0f, 300.0f, 16.0f, juce::Justification::centredLeft);

        if (onOpenAudioSettings_ != nullptr) {
            g.drawText("STANDALONE AUDIO & MIDI DEVICE", cX, card.getY() + 218.0f, 300.0f, 16.0f, juce::Justification::centredLeft);
        }

        // Legal & Licensing footer
        g.setFont(theme_.getValueFont(9.0f));
        g.setColour(juce::Colour(0xff64748b));
        g.drawText("Nova Audio Library: CC0 1.0 Universal / Public Domain (VCSL) • Fonts: SIL OFL 1.1",
                   cX, card.getBottom() - 36.0f, card.getWidth() - 140.0f, 20.0f, juce::Justification::centredLeft);
    }

    void resized() override {
        const auto card = getCardBounds();
        closeBtn_.setBounds(card.getRight() - 36, card.getY() + 12, 24, 24);

        // Scale Buttons (row at y = 80)
        const int sY = card.getY() + 80;
        const int sW = (card.getWidth() - 40 - 16) / 3;
        scaleSmallBtn_.setBounds(card.getX() + 20, sY, sW, 32);
        scaleNormalBtn_.setBounds(card.getX() + 20 + sW + 8, sY, sW, 32);
        scaleLargeBtn_.setBounds(card.getX() + 20 + (sW + 8) * 2, sY, sW, 32);

        // Voicing Buttons (row at y = 160)
        const int vY = card.getY() + 160;
        const int vW = (card.getWidth() - 40 - 10) / 2;
        voiceLowestBtn_.setBounds(card.getX() + 20, vY, vW, 32);
        voicePolyBtn_.setBounds(card.getX() + 20 + vW + 10, vY, vW, 32);

        // Audio Setup Button (row at y = 240)
        if (onOpenAudioSettings_ != nullptr) {
            audioSetupBtn_.setBounds(card.getX() + 20, card.getY() + 240, card.getWidth() - 40, 32);
        }

        // Done button in bottom right
        doneBtn_.setBounds(card.getRight() - 100, card.getBottom() - 42, 80, 28);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (!getCardBounds().contains(e.getPosition())) {
            if (onClose_) onClose_();
        }
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key == juce::KeyPress::escapeKey) {
            if (onClose_) onClose_();
            return true;
        }
        return false;
    }

private:
    [[nodiscard]] juce::Rectangle<int> getCardBounds() const {
        constexpr int cardW = 480;
        constexpr int cardH = 350;
        return juce::Rectangle<int>((getWidth() - cardW) / 2, (getHeight() - cardH) / 2, cardW, cardH);
    }

    void setScale(NovaTheme::TextScale scale) {
        theme_.setTextScale(scale);
        if (onScaleChanged_) onScaleChanged_(scale);
        refreshStates();
    }

    void setSubLowest(bool lowestOnly) {
        if (auto* p = processor_.getAPVTS().getParameter("subLowestOnly")) {
            p->setValueNotifyingHost(lowestOnly ? 1.0f : 0.0f);
        }
        refreshStates();
    }

    void styleToggle(juce::TextButton& btn, bool active) {
        if (active) {
            btn.setColour(juce::TextButton::buttonColourId, theme_.accentPrimary);
            btn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff090d16));
        } else {
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0e1726));
            btn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffcbd5e1));
        }
        btn.repaint();
    }

    nova::plugin::NovaProcessorBase& processor_;
    NovaTheme theme_;
    std::function<void(NovaTheme::TextScale)> onScaleChanged_;
    std::function<void()> onOpenAudioSettings_;
    std::function<void()> onClose_;

    // Close button (✕)
    struct VectorCloseBtn : public juce::Button {
        VectorCloseBtn() : juce::Button("Close") {}
        void paintButton(juce::Graphics& g, bool isHovered, bool) override {
            const auto r = getLocalBounds().toFloat().reduced(1.0f);
            g.setColour(isHovered ? juce::Colour(0xffef4444).withAlpha(0.20f) : juce::Colour(0xff1e293b));
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(isHovered ? juce::Colour(0xffef4444) : juce::Colour(0xff475569));
            g.drawRoundedRectangle(r, 4.0f, 1.0f);

            const float pad = 6.0f;
            g.setColour(isHovered ? juce::Colours::white : juce::Colour(0xffcbd5e1));
            g.drawLine(r.getX() + pad, r.getY() + pad, r.getRight() - pad, r.getBottom() - pad, 1.6f);
            g.drawLine(r.getRight() - pad, r.getY() + pad, r.getX() + pad, r.getBottom() - pad, 1.6f);
        }
    } closeBtn_;

    juce::TextButton scaleSmallBtn_{ "SMALL (COMPACT)" };
    juce::TextButton scaleNormalBtn_{ "NORMAL (+15%)" };
    juce::TextButton scaleLargeBtn_{ "LARGE (+30%)" };

    juce::TextButton voiceLowestBtn_{ "LOWEST NOTE (MONO)" };
    juce::TextButton voicePolyBtn_{ "POLYPHONIC (CHORDS)" };

    juce::TextButton audioSetupBtn_{ "OPEN AUDIO & MIDI SETTINGS..." };
    juce::TextButton doneBtn_{ "DONE" };
};

} // namespace nova::ui
