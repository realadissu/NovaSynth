#include "MixerSlideOverDrawer.h"
#include <cmath>

namespace nova::synth::ui {

float MixerSlideOverDrawer::ConsoleFaderLookAndFeel::calculateTrackCenterX(float x, float width) noexcept {
    const float xOffset = (width < 90.0f) ? 5.0f : 0.0f;
    return x + width * 0.5f + xOffset;
}

float MixerSlideOverDrawer::ConsoleFaderLookAndFeel::calculateThumbLeft(float x, float width) noexcept {
    return calculateTrackCenterX(x, width) - 15.0f;
}

void MixerSlideOverDrawer::ConsoleFaderLookAndFeel::drawLinearSlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
    juce::Slider::SliderStyle style, juce::Slider& slider) {
    if (style != juce::Slider::LinearVertical) {
        juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0, 0, style, slider);
        return;
    }

    const float trackCenterX = calculateTrackCenterX(static_cast<float>(x), static_cast<float>(width));
    const float trackW = 4.0f;
    const float trackX = trackCenterX - trackW * 0.5f;
    const float trackY = static_cast<float>(y) + 4.0f;
    const float trackH = static_cast<float>(height) - 8.0f;

    // Fader track gutter
    g.setColour(juce::Colour(0xff050810));
    g.fillRoundedRectangle(trackX, trackY, trackW, trackH, 2.0f);
    g.setColour(juce::Colour(0xff1e2d42));
    g.drawRoundedRectangle(trackX, trackY, trackW, trackH, 2.0f, 1.0f);

    // Center slot groove
    g.setColour(juce::Colour(0xff020408));
    g.fillRect(trackX + 1.0f, trackY + 2.0f, 2.0f, trackH - 4.0f);

    // Slender Fader Cap / Thumb (Console style, 30px wide, centered on track)
    constexpr float thumbW = 30.0f;
    constexpr float thumbH = 20.0f;
    const float thumbX = trackCenterX - thumbW * 0.5f;
    const float thumbY = sliderPos - thumbH * 0.5f;
    const juce::Rectangle<float> thumbRect(thumbX, thumbY, thumbW, thumbH);

    // Drop shadow under cap
    g.setColour(juce::Colours::black.withAlpha(0.70f));
    g.fillRoundedRectangle(thumbRect.translated(0.0f, 2.5f), 3.0f);

    // Cap body gradient (dark brushed metallic console look)
    const auto capGrad = juce::ColourGradient::vertical(
        juce::Colour(0xff384b64), thumbRect.getY(),
        juce::Colour(0xff0d1522), thumbRect.getBottom());
    g.setGradientFill(capGrad);
    g.fillRoundedRectangle(thumbRect, 3.0f);

    // Cap border
    g.setColour(juce::Colour(0xff64748b));
    g.drawRoundedRectangle(thumbRect, 3.0f, 1.0f);

    // Side grip notches
    g.setColour(juce::Colour(0xff1e293b));
    g.fillRect(thumbRect.getX() + 2.0f, thumbRect.getCentreY() - 4.0f, 2.5f, 1.0f);
    g.fillRect(thumbRect.getX() + 2.0f, thumbRect.getCentreY() + 4.0f, 2.5f, 1.0f);
    g.fillRect(thumbRect.getRight() - 4.5f, thumbRect.getCentreY() - 4.0f, 2.5f, 1.0f);
    g.fillRect(thumbRect.getRight() - 4.5f, thumbRect.getCentreY() + 4.0f, 2.5f, 1.0f);

    // Center indicator line
    const auto accent = slider.findColour(juce::Slider::thumbColourId);
    g.setColour(accent.brighter(0.20f));
    g.fillRect(thumbRect.getX() + 2.5f, thumbRect.getCentreY() - 1.25f, thumbRect.getWidth() - 5.0f, 2.5f);
}

MixerSlideOverDrawer::DrawerCloseButton::DrawerCloseButton() : juce::Button("Close") {}

void MixerSlideOverDrawer::DrawerCloseButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) {
    const auto r = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(0xffef4444).withAlpha(0.22f) : juce::Colour(0xff1e293b));
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(0xffef4444) : juce::Colour(0xff475569));
    g.drawRoundedRectangle(r, 4.0f, 1.0f);

    const float pad = 6.0f;
    g.setColour(shouldDrawButtonAsHighlighted ? juce::Colours::white : juce::Colour(0xffcbd5e1));
    g.drawLine(r.getX() + pad, r.getY() + pad, r.getRight() - pad, r.getBottom() - pad, 1.6f);
    g.drawLine(r.getRight() - pad, r.getY() + pad, r.getX() + pad, r.getBottom() - pad, 1.6f);
}

void MixerSlideOverDrawer::ConsoleStripButton::setButtonText(const juce::String& text) {
    text_ = text;
    repaint();
}

void MixerSlideOverDrawer::ConsoleStripButton::setActive(bool active) {
    if (isActive_ != active) {
        isActive_ = active;
        repaint();
    }
}

void MixerSlideOverDrawer::ConsoleStripButton::setColors(juce::Colour activeBg, juce::Colour activeBorder, juce::Colour activeText,
                                                         juce::Colour inactiveBg, juce::Colour inactiveBorder, juce::Colour inactiveText) {
    activeBg_ = activeBg;
    activeBorder_ = activeBorder;
    activeText_ = activeText;
    inactiveBg_ = inactiveBg;
    inactiveBorder_ = inactiveBorder;
    inactiveText_ = inactiveText;
    repaint();
}

void MixerSlideOverDrawer::ConsoleStripButton::setFont(juce::Font font) {
    font_ = font;
    repaint();
}

void MixerSlideOverDrawer::ConsoleStripButton::paint(juce::Graphics& g) {
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    juce::Colour bg = isActive_ ? activeBg_ : inactiveBg_;
    juce::Colour border = isActive_ ? activeBorder_ : inactiveBorder_;
    juce::Colour textCol = isActive_ ? activeText_ : inactiveText_;

    if (isMouseDown_) {
        bg = bg.brighter(0.18f);
    } else if (isHovered_) {
        bg = bg.brighter(0.10f);
        if (!isActive_) border = border.brighter(0.35f);
    }

    g.setColour(bg);
    g.fillRoundedRectangle(r, 3.5f);
    g.setColour(border);
    g.drawRoundedRectangle(r, 3.5f, isActive_ ? 1.4f : 1.0f);
    g.setFont(font_);
    g.setColour(textCol);
    g.drawText(text_, getLocalBounds(), juce::Justification::centred, false);
}

void MixerSlideOverDrawer::ConsoleStripButton::mouseEnter(const juce::MouseEvent&) {
    isHovered_ = true;
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    repaint();
}

void MixerSlideOverDrawer::ConsoleStripButton::mouseExit(const juce::MouseEvent&) {
    isHovered_ = false;
    isMouseDown_ = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void MixerSlideOverDrawer::ConsoleStripButton::mouseDown(const juce::MouseEvent&) {
    isMouseDown_ = true;
    repaint();
}

void MixerSlideOverDrawer::ConsoleStripButton::mouseUp(const juce::MouseEvent& event) {
    if (isMouseDown_) {
        isMouseDown_ = false;
        repaint();
        if (reallyContains(event.getPosition(), true)) {
            if (onClick) onClick();
        }
    }
}

MixerSlideOverDrawer::OctaveStepperComponent::OctaveStepperComponent(
    const nova::ui::NovaTheme& t, juce::Image minusImg, juce::Image plusImg)
    : theme(t), minusIcon(std::move(minusImg)), plusIcon(std::move(plusImg)) {}

void MixerSlideOverDrawer::OctaveStepperComponent::paint(juce::Graphics& g) {
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(theme.bgElevated.darker(0.12f));
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(theme.borderSubtle.withAlpha(0.6f));
    g.drawRoundedRectangle(r, 4.0f, 1.0f);

    const float btnW = 22.0f;
    const auto leftR = r.withWidth(btnW).reduced(2.0f);
    const auto rightR = r.withTrimmedLeft(r.getWidth() - btnW).reduced(2.0f);
    const auto midR = r.withTrimmedLeft(btnW).withTrimmedRight(btnW);

    g.setColour(isLeftHover_ ? juce::Colour(0xff1e293b) : juce::Colour(0xff090f18));
    g.fillRoundedRectangle(leftR, 3.5f);
    g.setColour(isLeftHover_ ? theme.accentPrimary : juce::Colour(0xff334155));
    g.drawRoundedRectangle(leftR, 3.5f, 1.0f);

    const auto mRect = juce::Rectangle<float>(leftR.getCentreX() - 4.5f, leftR.getCentreY() - 1.2f, 9.0f, 2.4f);
    g.setColour(isLeftHover_ ? juce::Colours::white : juce::Colour(0xffffffff));
    g.fillRoundedRectangle(mRect, 1.2f);

    g.setColour(isRightHover_ ? juce::Colour(0xff1e293b) : juce::Colour(0xff090f18));
    g.fillRoundedRectangle(rightR, 3.5f);
    g.setColour(isRightHover_ ? theme.accentPrimary : juce::Colour(0xff334155));
    g.drawRoundedRectangle(rightR, 3.5f, 1.0f);

    const auto hRect = juce::Rectangle<float>(rightR.getCentreX() - 4.5f, rightR.getCentreY() - 1.2f, 9.0f, 2.4f);
    const auto vRect = juce::Rectangle<float>(rightR.getCentreX() - 1.2f, rightR.getCentreY() - 4.5f, 2.4f, 9.0f);
    g.setColour(isRightHover_ ? juce::Colours::white : juce::Colour(0xffffffff));
    g.fillRoundedRectangle(hRect, 1.2f);
    g.fillRoundedRectangle(vRect, 1.2f);

    g.setFont(theme.getSectionFont(10.0f));
    g.setColour(theme.textPrimary);
    const juce::String str = (currentOctave > 0 ? "+" : "") + juce::String(currentOctave) + " OCT";
    g.drawText(str, midR, juce::Justification::centred);
}

void MixerSlideOverDrawer::OctaveStepperComponent::mouseMove(const juce::MouseEvent& event) {
    const bool l = event.x < 24;
    const bool r = event.x > getWidth() - 24;
    if (l != isLeftHover_ || r != isRightHover_) {
        isLeftHover_ = l;
        isRightHover_ = r;
        setMouseCursor((l || r) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void MixerSlideOverDrawer::OctaveStepperComponent::mouseExit(const juce::MouseEvent&) {
    isLeftHover_ = false;
    isRightHover_ = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void MixerSlideOverDrawer::OctaveStepperComponent::mouseDown(const juce::MouseEvent& event) {
    if (event.x < 24) {
        if (onStep) onStep(-1);
    } else if (event.x > getWidth() - 24) {
        if (onStep) onStep(1);
    }
}

MixerSlideOverDrawer::MixerSlideOverDrawer(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc),
      theme_(theme),
      masterFader_(juce::Slider::LinearVertical, juce::Slider::TextBoxBelow),
      delayReturn_("DELAY", "%", theme, false),
      reverbReturn_("REVERB", "%", theme, false) {
    setWantsKeyboardFocus(true);
    setInterceptsMouseClicks(true, true);

    const auto minusImg = loadIcon("octave_minus_png", "octave_minus.png");
    const auto plusImg  = loadIcon("octave_plus_png", "octave_plus.png");
    auto& apvts = proc_.getAPVTS();

    for (int i = 0; i < 8; ++i) {
        const juce::String idx = juce::String(i + 1);
        auto& strip = strips_[static_cast<std::size_t>(i)];

        strip.octStepper = std::make_unique<OctaveStepperComponent>(theme_, minusImg, plusImg);
        strip.octStepper->onStep = [this, i](int delta) {
            const juce::String paramId = "osc" + juce::String(i + 1) + "_octave";
            if (auto* p = proc_.getAPVTS().getParameter(paramId)) {
                const int cur = static_cast<int>(std::round(p->convertFrom0to1(p->getValue())));
                const int next = std::clamp(cur + delta, -2, 2);
                if (next != cur) {
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(next)));
                    updateDimming();
                    repaint();
                }
            }
        };
        addAndMakeVisible(*strip.octStepper);

        strip.flt1Btn.setButtonText("FLT 1");
        strip.flt1Btn.setFont(theme_.getSectionFont(10.0f));
        strip.flt1Btn.onClick = [this, i]() { toggleFilterRouting(i, 1); };
        addAndMakeVisible(strip.flt1Btn);

        strip.flt2Btn.setButtonText("FLT 2");
        strip.flt2Btn.setFont(theme_.getSectionFont(10.0f));
        strip.flt2Btn.onClick = [this, i]() { toggleFilterRouting(i, 2); };
        addAndMakeVisible(strip.flt2Btn);

        strip.pan = std::make_unique<nova::ui::ArcKnob>("PAN", "", theme, false);
        strip.pan->attachToParameter(apvts, "osc" + idx + "_pan");
        addAndMakeVisible(*strip.pan);

        strip.soloBtn.setButtonText("S");
        strip.soloBtn.setFont(theme_.getSectionFont(11.0f));
        strip.soloBtn.onClick = [this, i]() {
            const bool curSolo = proc_.isSlotSoloed(i);
            proc_.setSlotSolo(i, !curSolo);
            updateDimming();
            repaint();
        };
        addAndMakeVisible(strip.soloBtn);

        strip.muteBtn.setButtonText("M");
        strip.muteBtn.setFont(theme_.getSectionFont(11.0f));
        strip.muteBtn.onClick = [this, i]() {
            const bool curMute = proc_.isSlotMuted(i);
            proc_.setSlotMute(i, !curMute);
            updateDimming();
            repaint();
        };
        addAndMakeVisible(strip.muteBtn);

        strip.fader.setLookAndFeel(&faderLookAndFeel_);
        strip.fader.setSliderStyle(juce::Slider::LinearVertical);
        strip.fader.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 16);
        strip.fader.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff1e2d42));
        strip.fader.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff060a12));
        strip.fader.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        strip.fader.setColour(juce::Slider::thumbColourId, getLayerColour(i));

        strip.faderAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "osc" + idx + "_level", strip.fader);

        strip.fader.textFromValueFunction = [](double val) {
            if (val <= 0.005) return juce::String("-inf");
            const float db = 20.0f * std::log10(static_cast<float>(val));
            return juce::String(db, 1) + " dB";
        };
        strip.fader.updateText();
        addAndMakeVisible(strip.fader);
    }

    initMasterAndEffectsControls();
}

} // namespace nova::synth::ui

