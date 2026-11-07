#pragma once

#include "nova_ui/NovaTheme.h"
#include "nova_plugin/NovaProcessorBase.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

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

        // Master Reference Tuning (435.0 .. 445.0 Hz in 0.1 Hz steps, default 440.0 Hz)
        if (processor_.getAPVTS().getParameter("masterTuningHz") != nullptr) {
            addAndMakeVisible(tuningDecBtn_);
            addAndMakeVisible(tuningSlider_);
            addAndMakeVisible(tuningIncBtn_);
            addAndMakeVisible(tuningResetBtn_);

            tuningSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
            tuningSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 78, 28);
            tuningSlider_.setRange(435.0, 445.0, 0.1);
            tuningSlider_.setTextValueSuffix(" Hz");
            tuningSlider_.setDoubleClickReturnValue(true, 440.0);
            tuningSlider_.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff0e1726));
            tuningSlider_.setColour(juce::Slider::trackColourId, theme_.accentPrimary);
            tuningSlider_.setColour(juce::Slider::thumbColourId, juce::Colours::white);
            tuningSlider_.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe2e8f0));
            tuningSlider_.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0e1726));
            tuningSlider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff1e2d42));

            tuningSlider_.onValueChange = [this]() {
                if (!isUpdatingTuningUi_) {
                    setMasterTuning(static_cast<float>(tuningSlider_.getValue()));
                }
            };
            tuningDecBtn_.onClick = [this]() {
                setMasterTuning(getCurrentMasterTuning() - 0.1f);
            };
            tuningIncBtn_.onClick = [this]() {
                setMasterTuning(getCurrentMasterTuning() + 0.1f);
            };
            tuningResetBtn_.onClick = [this]() {
                setMasterTuning(440.0f);
            };
        }

        // Sustain Pedal Polarity Buttons (only if supported by processor)
        if (processor_.getAPVTS().getParameter("invertSustainPedal") != nullptr) {
            addAndMakeVisible(pedalNormalBtn_);
            addAndMakeVisible(pedalInvertBtn_);
            pedalNormalBtn_.onClick = [this]() { setInvertPedal(false); };
            pedalInvertBtn_.onClick = [this]() { setInvertPedal(true); };
        }

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

        if (processor_.getAPVTS().getParameter("masterTuningHz") != nullptr) {
            const float hz = getCurrentMasterTuning();
            isUpdatingTuningUi_ = true;
            tuningSlider_.setValue(static_cast<double>(hz), juce::dontSendNotification);
            isUpdatingTuningUi_ = false;
            styleToggle(tuningDecBtn_, false);
            styleToggle(tuningIncBtn_, false);
            styleToggle(tuningResetBtn_, std::abs(hz - 440.0f) < 0.05f);
        }

        if (auto* invPedalParam = processor_.getAPVTS().getParameter("invertSustainPedal")) {
            const bool isInverted = (invPedalParam->getValue() > 0.5f);
            styleToggle(pedalNormalBtn_, !isInverted);
            styleToggle(pedalInvertBtn_, isInverted);
        }
    }

    void paint(juce::Graphics& g) override {
        // 1. Semi-transparent dark scrim backdrop
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.fillRect(getLocalBounds());

        // 2. Dialog Card bounds
        const auto card = getCardBounds();
        const auto cardF = card.toFloat();

        // Card Drop Shadow
        g.setColour(juce::Colours::black.withAlpha(0.70f));
        g.fillRoundedRectangle(cardF.translated(0.0f, 6.0f), 10.0f);

        // Solid Deep Obsidian Desk Background
        g.setColour(juce::Colour(0xff090e17));
        g.fillRoundedRectangle(cardF, 10.0f);

        // Border
        g.setColour(juce::Colour(0xff1e2d42));
        g.drawRoundedRectangle(cardF, 10.0f, 1.2f);

        // Header Accent Strip
        auto headerR = cardF.withHeight(48.0f).reduced(16.0f, 0.0f);
        g.setFont(theme_.getSectionFont(15.0f));
        g.setColour(theme_.accentPrimary);
        g.drawText("SETTINGS & PREFERENCES", headerR, juce::Justification::centredLeft);

        g.setColour(juce::Colour(0xff1e2d42));
        g.drawHorizontalLine(card.getY() + 48, cardF.getX(), cardF.getRight());

        // Section Headers
        g.setFont(theme_.getLabelFont(10.5f));
        g.setColour(juce::Colour(0xff94a3b8));

        const int cX = card.getX() + 20;
        g.drawText("UI LABEL & TYPOGRAPHY SCALE", juce::Rectangle<int>(cX, card.getY() + 58, 300, 16), juce::Justification::centredLeft);

        int nextSectionY = card.getY() + 132;
        if (processor_.getAPVTS().getParameter("masterTuningHz") != nullptr) {
            g.drawText("MASTER TUNING (A4 REFERENCE: 435.0 - 445.0 HZ)", juce::Rectangle<int>(cX, nextSectionY, 400, 16), juce::Justification::centredLeft);
            nextSectionY += 72;
        }

        if (processor_.getAPVTS().getParameter("invertSustainPedal") != nullptr) {
            g.drawText("SUSTAIN PEDAL POLARITY", juce::Rectangle<int>(cX, nextSectionY, 300, 16), juce::Justification::centredLeft);
            nextSectionY += 72;
        }

        if (onOpenAudioSettings_ != nullptr) {
            g.drawText("STANDALONE AUDIO & MIDI DEVICE", juce::Rectangle<int>(cX, nextSectionY, 300, 16), juce::Justification::centredLeft);
        }

        // Legal & Licensing footer
        g.setFont(theme_.getValueFont(9.0f));
        g.setColour(juce::Colour(0xff64748b));
        g.drawText("Nova Audio Library: CC0 1.0 Universal / Public Domain (VCSL) | Fonts: SIL OFL 1.1",
                   juce::Rectangle<int>(cX, card.getBottom() - 36, card.getWidth() - 140, 20), juce::Justification::centredLeft);
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

        int nextRowY = card.getY() + 154;
        if (processor_.getAPVTS().getParameter("masterTuningHz") != nullptr) {
            const int x0 = card.getX() + 20;
            const int totalW = card.getWidth() - 40;
            constexpr int stepW = 36;
            constexpr int resetW = 94;
            constexpr int gap = 6;
            const int sliderW = totalW - stepW * 2 - resetW - gap * 3;

            tuningDecBtn_.setBounds(x0, nextRowY, stepW, 32);
            tuningSlider_.setBounds(x0 + stepW + gap, nextRowY, sliderW, 32);
            tuningIncBtn_.setBounds(x0 + stepW + gap + sliderW + gap, nextRowY, stepW, 32);
            tuningResetBtn_.setBounds(x0 + totalW - resetW, nextRowY, resetW, 32);
            nextRowY += 72;
        }

        // Sustain Pedal Polarity Buttons
        if (processor_.getAPVTS().getParameter("invertSustainPedal") != nullptr) {
            const int pW = (card.getWidth() - 40 - 10) / 2;
            pedalNormalBtn_.setBounds(card.getX() + 20, nextRowY, pW, 32);
            pedalInvertBtn_.setBounds(card.getX() + 20 + pW + 10, nextRowY, pW, 32);
            nextRowY += 72;
        }

        // Audio Setup Button
        if (onOpenAudioSettings_ != nullptr) {
            audioSetupBtn_.setBounds(card.getX() + 20, nextRowY, card.getWidth() - 40, 32);
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
        int cardH = 205;
        if (processor_.getAPVTS().getParameter("masterTuningHz") != nullptr) cardH += 72;
        if (processor_.getAPVTS().getParameter("invertSustainPedal") != nullptr) cardH += 72;
        if (onOpenAudioSettings_ != nullptr) cardH += 72;
        return juce::Rectangle<int>((getWidth() - cardW) / 2, (getHeight() - cardH) / 2, cardW, cardH);
    }

    void setScale(NovaTheme::TextScale scale) {
        theme_.setTextScale(scale);
        if (onScaleChanged_) onScaleChanged_(scale);
        refreshStates();
    }

    [[nodiscard]] float getCurrentMasterTuning() const noexcept {
        if (auto* raw = processor_.getAPVTS().getRawParameterValue("masterTuningHz")) {
            const float v = std::clamp(raw->load(std::memory_order_relaxed), 435.0f, 445.0f);
            return std::round(v * 10.0f) * 0.1f;
        }
        return 440.0f;
    }

    void setMasterTuning(float hz) {
        const float clamped = std::round(std::clamp(hz, 435.0f, 445.0f) * 10.0f) * 0.1f;
        if (auto* p = processor_.getAPVTS().getParameter("masterTuningHz")) {
            p->setValueNotifyingHost(p->convertTo0to1(clamped));
        }
        processor_.getPresetManager().saveGlobalPreferences();
        refreshStates();
    }

    void setInvertPedal(bool inverted) {
        if (auto* p = processor_.getAPVTS().getParameter("invertSustainPedal")) {
            p->setValueNotifyingHost(inverted ? 1.0f : 0.0f);
        }
        processor_.getPresetManager().saveGlobalPreferences();
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
    bool isUpdatingTuningUi_ = false;

    // Close button (X)
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

    juce::TextButton tuningDecBtn_{ "-0.1" };
    juce::Slider     tuningSlider_;
    juce::TextButton tuningIncBtn_{ "+0.1" };
    juce::TextButton tuningResetBtn_{ "440.0 HZ" };

    juce::TextButton pedalNormalBtn_{ "NORMAL (POLARITY +)" };
    juce::TextButton pedalInvertBtn_{ "INVERTED (POLARITY -)" };

    juce::TextButton audioSetupBtn_{ "OPEN AUDIO & MIDI SETTINGS..." };
    juce::TextButton doneBtn_{ "DONE" };
};

} // namespace nova::ui
