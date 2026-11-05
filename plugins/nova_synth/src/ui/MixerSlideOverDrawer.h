#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <nova_ui/NovaTheme.h>
#include <nova_ui/ArcKnob.h>
#include "../NovaSynthProcessor.h"
#include <BinaryData.h>
#include <array>
#include <memory>
#include <cmath>

namespace nova::synth::ui {

/**
 * @brief Professional DAW Console Slide-Over Mixer Drawer (Version 4)
 *
 * Implements refined Concept A with authentic studio ergonomics:
 * - Seamless filleted handle tab (MIXER) flowing organically into the drawer contour (matching screenshot reference).
 * - User-provided squircle + and - icons for Octave Stepper.
 * - Full-sized, prominent ArcKnobs for PAN, DELAY, and REVERB (matching filter deck).
 * - Dual stacked buttons for Filter 1 and Filter 2 with independent On/Off states.
 * - Slender 30px metallic console fader caps with center indicator lines.
 * - Clear dB readouts and real-time tri-color peak meters.
 * - Mute (M) illuminated red when muted; soft resting state (no pitch-black holes).
 * - Vector Close button (✕) and conditional SOLO RESET badge.
 */
class MixerSlideOverDrawer : public juce::Component, private juce::Timer {
public:
    // Sleek Custom DAW Console Fader LookAndFeel
    struct ConsoleFaderLookAndFeel : public juce::LookAndFeel_V4 {
        static float calculateTrackCenterX(float x, float width) noexcept {
            const float xOffset = (width < 90.0f) ? 5.0f : 0.0f;
            return x + width * 0.5f + xOffset;
        }

        static float calculateThumbLeft(float x, float width) noexcept {
            return calculateTrackCenterX(x, width) - 15.0f; // 30px thumb width
        }

        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                              juce::Slider::SliderStyle style, juce::Slider& slider) override {
            if (style != juce::Slider::LinearVertical) {
                juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0, 0, style, slider);
                return;
            }

            const float trackCenterX = calculateTrackCenterX(static_cast<float>(x), static_cast<float>(width));
            const float trackW = 4.0f;
            const float trackX = trackCenterX - trackW * 0.5f;
            const float trackY = static_cast<float>(y) + 4.0f;
            const float trackH = static_cast<float>(height) - 8.0f;

            // Fader track gutter (deep in-track groove)
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

            // Vibrant center indicator line with layer/master accent colour
            const auto accent = slider.findColour(juce::Slider::thumbColourId);
            g.setColour(accent.brighter(0.20f));
            g.fillRect(thumbRect.getX() + 2.5f, thumbRect.getCentreY() - 1.25f, thumbRect.getWidth() - 5.0f, 2.5f);
        }
    };

    // Vector Close Button
    struct DrawerCloseButton : public juce::Button {
        DrawerCloseButton() : juce::Button("Close") {}

        void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override {
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
    };

    // Dedicated DAW Console Strip Button (immune to global LookAndFeel overrides)
    class ConsoleStripButton : public juce::Component {
    public:
        std::function<void()> onClick;

        void setButtonText(const juce::String& text) {
            text_ = text;
            repaint();
        }

        void setActive(bool active) {
            if (isActive_ != active) {
                isActive_ = active;
                repaint();
            }
        }

        [[nodiscard]] bool isActive() const noexcept { return isActive_; }

        void setColors(juce::Colour activeBg, juce::Colour activeBorder, juce::Colour activeText,
                       juce::Colour inactiveBg, juce::Colour inactiveBorder, juce::Colour inactiveText) {
            activeBg_ = activeBg;
            activeBorder_ = activeBorder;
            activeText_ = activeText;
            inactiveBg_ = inactiveBg;
            inactiveBorder_ = inactiveBorder;
            inactiveText_ = inactiveText;
            repaint();
        }

        void setFont(juce::Font font) {
            font_ = font;
            repaint();
        }

        void paint(juce::Graphics& g) override {
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

            // Fill button background
            g.setColour(bg);
            g.fillRoundedRectangle(r, 3.5f);

            // Draw border
            g.setColour(border);
            g.drawRoundedRectangle(r, 3.5f, isActive_ ? 1.4f : 1.0f);

            // Draw text
            g.setFont(font_);
            g.setColour(textCol);
            g.drawText(text_, getLocalBounds(), juce::Justification::centred, false);
        }

        void mouseEnter(const juce::MouseEvent&) override {
            isHovered_ = true;
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
            repaint();
        }

        void mouseExit(const juce::MouseEvent&) override {
            isHovered_ = false;
            isMouseDown_ = false;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint();
        }

        void mouseDown(const juce::MouseEvent&) override {
            isMouseDown_ = true;
            repaint();
        }

        void mouseUp(const juce::MouseEvent& event) override {
            if (isMouseDown_) {
                isMouseDown_ = false;
                repaint();
                if (reallyContains(event.getPosition(), true)) {
                    if (onClick) onClick();
                }
            }
        }

    private:
        juce::String text_;
        bool isActive_ = false;
        bool isHovered_ = false;
        bool isMouseDown_ = false;
        juce::Font font_{juce::FontOptions(11.0f)};

        juce::Colour activeBg_{0xff38bdf8};
        juce::Colour activeBorder_{0xff38bdf8};
        juce::Colour activeText_{0xffffffff};

        juce::Colour inactiveBg_{0xff0d1522};
        juce::Colour inactiveBorder_{0xff1e293b};
        juce::Colour inactiveText_{0xff64748b};
    };

    // Octave Stepper Pill with radiant high-contrast squircle buttons
    struct OctaveStepperComponent : public juce::Component {
        std::function<void(int delta)> onStep;
        int currentOctave = 0;
        const nova::ui::NovaTheme& theme;
        juce::Image minusIcon;
        juce::Image plusIcon;

        OctaveStepperComponent(const nova::ui::NovaTheme& t, juce::Image minusImg, juce::Image plusImg)
            : theme(t), minusIcon(std::move(minusImg)), plusIcon(std::move(plusImg)) {}

        void paint(juce::Graphics& g) override {
            const auto r = getLocalBounds().toFloat().reduced(0.5f);

            // Background pill
            g.setColour(theme.bgElevated.darker(0.12f));
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(theme.borderSubtle.withAlpha(0.6f));
            g.drawRoundedRectangle(r, 4.0f, 1.0f);

            const float btnW = 22.0f;
            const auto leftR = r.withWidth(btnW).reduced(2.0f);
            const auto rightR = r.withTrimmedLeft(r.getWidth() - btnW).reduced(2.0f);
            const auto midR = r.withTrimmedLeft(btnW).withTrimmedRight(btnW);

            // 1. Left Squircle Button: Minus [-]
            g.setColour(isLeftHover_ ? juce::Colour(0xff1e293b) : juce::Colour(0xff090f18));
            g.fillRoundedRectangle(leftR, 3.5f);
            g.setColour(isLeftHover_ ? theme.accentPrimary : juce::Colour(0xff334155));
            g.drawRoundedRectangle(leftR, 3.5f, 1.0f);

            // Radiant bright white minus glyph (2.4px thick pill)
            const auto mRect = juce::Rectangle<float>(leftR.getCentreX() - 4.5f, leftR.getCentreY() - 1.2f, 9.0f, 2.4f);
            g.setColour(isLeftHover_ ? juce::Colours::white : juce::Colour(0xffffffff));
            g.fillRoundedRectangle(mRect, 1.2f);

            // 2. Right Squircle Button: Plus [+]
            g.setColour(isRightHover_ ? juce::Colour(0xff1e293b) : juce::Colour(0xff090f18));
            g.fillRoundedRectangle(rightR, 3.5f);
            g.setColour(isRightHover_ ? theme.accentPrimary : juce::Colour(0xff334155));
            g.drawRoundedRectangle(rightR, 3.5f, 1.0f);

            // Radiant bright white plus glyph (2.4px thick cross pills)
            const auto hRect = juce::Rectangle<float>(rightR.getCentreX() - 4.5f, rightR.getCentreY() - 1.2f, 9.0f, 2.4f);
            const auto vRect = juce::Rectangle<float>(rightR.getCentreX() - 1.2f, rightR.getCentreY() - 4.5f, 2.4f, 9.0f);
            g.setColour(isRightHover_ ? juce::Colours::white : juce::Colour(0xffffffff));
            g.fillRoundedRectangle(hRect, 1.2f);
            g.fillRoundedRectangle(vRect, 1.2f);

            // 3. Center Octave Text
            g.setFont(theme.getSectionFont(10.0f));
            g.setColour(theme.textPrimary);
            const juce::String str = (currentOctave > 0 ? "+" : "") + juce::String(currentOctave) + " OCT";
            g.drawText(str, midR, juce::Justification::centred);
        }

        void mouseMove(const juce::MouseEvent& event) override {
            const bool l = event.x < 24;
            const bool r = event.x > getWidth() - 24;
            if (l != isLeftHover_ || r != isRightHover_) {
                isLeftHover_ = l;
                isRightHover_ = r;
                setMouseCursor((l || r) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
                repaint();
            }
        }

        void mouseExit(const juce::MouseEvent&) override {
            isLeftHover_ = false;
            isRightHover_ = false;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint();
        }

        void mouseDown(const juce::MouseEvent& event) override {
            if (event.x < 24) {
                if (onStep) onStep(-1);
            } else if (event.x > getWidth() - 24) {
                if (onStep) onStep(1);
            }
        }

        bool isLeftHover_ = false;
        bool isRightHover_ = false;
    };

    MixerSlideOverDrawer(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc),
          theme_(theme),
          masterFader_(juce::Slider::LinearVertical, juce::Slider::TextBoxBelow),
          delayReturn_("DELAY", "%", theme, false),  // Full-sized ArcKnob
          reverbReturn_("REVERB", "%", theme, false) { // Full-sized ArcKnob

        setWantsKeyboardFocus(true);
        setInterceptsMouseClicks(true, true);

        // Load Octave Icons
        const auto minusImg = loadIcon("octave_minus_png", "octave_minus.png");
        const auto plusImg  = loadIcon("octave_plus_png", "octave_plus.png");

        auto& apvts = proc_.getAPVTS();

        // 1. Channel Strips (1..8)
        for (int i = 0; i < 8; ++i) {
            const juce::String idx = juce::String(i + 1);
            auto& strip = strips_[static_cast<std::size_t>(i)];

            // Octave Stepper
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

            // Filter 1 Button
            strip.flt1Btn.setButtonText("FLT 1");
            strip.flt1Btn.setFont(theme_.getSectionFont(10.0f));
            strip.flt1Btn.onClick = [this, i]() {
                toggleFilterRouting(i, 1);
            };
            addAndMakeVisible(strip.flt1Btn);

            // Filter 2 Button
            strip.flt2Btn.setButtonText("FLT 2");
            strip.flt2Btn.setFont(theme_.getSectionFont(10.0f));
            strip.flt2Btn.onClick = [this, i]() {
                toggleFilterRouting(i, 2);
            };
            addAndMakeVisible(strip.flt2Btn);

            // Pan Knob (Full-Sized, non-compact)
            strip.pan = std::make_unique<nova::ui::ArcKnob>("PAN", "", theme, false);
            strip.pan->attachToParameter(apvts, "osc" + idx + "_pan");
            addAndMakeVisible(*strip.pan);

            // Solo Button (S)
            strip.soloBtn.setButtonText("S");
            strip.soloBtn.setFont(theme_.getSectionFont(11.0f));
            strip.soloBtn.onClick = [this, i]() {
                const bool curSolo = proc_.isSlotSoloed(i);
                proc_.setSlotSolo(i, !curSolo);
                updateDimming();
                repaint();
            };
            addAndMakeVisible(strip.soloBtn);

            // Mute Button (M) - Independent Studio Mixer Audio Mute
            strip.muteBtn.setButtonText("M");
            strip.muteBtn.setFont(theme_.getSectionFont(11.0f));
            strip.muteBtn.onClick = [this, i]() {
                const bool curMute = proc_.isSlotMuted(i);
                proc_.setSlotMute(i, !curMute);
                updateDimming();
                repaint();
            };
            addAndMakeVisible(strip.muteBtn);

            // Fader with dB text formatting (high contrast)
            strip.fader.setLookAndFeel(&faderLookAndFeel_);
            strip.fader.setSliderStyle(juce::Slider::LinearVertical);
            strip.fader.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 16);
            strip.fader.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff1e2d42));
            strip.fader.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff060a12));
            strip.fader.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
            strip.fader.setColour(juce::Slider::thumbColourId, getLayerColour(i));

            strip.faderAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                apvts, "osc" + idx + "_level", strip.fader);

            // Attach dB formatting AFTER SliderAttachment so it is preserved
            strip.fader.textFromValueFunction = [](double val) {
                if (val <= 0.005) return juce::String("-inf");
                const float db = 20.0f * std::log10(static_cast<float>(val));
                return juce::String(db, 1) + " dB";
            };
            strip.fader.updateText();

            addAndMakeVisible(strip.fader);
        }

        // 2. Master Section
        masterFader_.setLookAndFeel(&faderLookAndFeel_);
        masterFader_.setSliderStyle(juce::Slider::LinearVertical);
        masterFader_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 16);
        masterFader_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xfff59e0b).withAlpha(0.40f));
        masterFader_.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff060a12));
        masterFader_.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfffbbf24));
        masterFader_.setColour(juce::Slider::thumbColourId, juce::Colour(0xfff59e0b));

        masterFaderAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "masterGainDb", masterFader_);

        masterFader_.textFromValueFunction = [](double val) {
            if (val <= -23.5) return juce::String("-inf");
            return (val > 0.05 ? "+" : "") + juce::String(val, 1) + " dB";
        };
        masterFader_.updateText();

        addAndMakeVisible(masterFader_);

        delayReturn_.attachToParameter(apvts, "delayMix");
        reverbReturn_.attachToParameter(apvts, "reverbMix");
        addAndMakeVisible(delayReturn_);
        addAndMakeVisible(reverbReturn_);

        // Close Button
        closeBtn_.onClick = [this]() { setDrawerOpen(false); };
        addAndMakeVisible(closeBtn_);

        // Solo Reset Badge Button
        clearSolosBtn_.setButtonText("SOLO RESET");
        clearSolosBtn_.onClick = [this]() {
            proc_.clearAllSolos();
            updateDimming();
            repaint();
        };
        addAndMakeVisible(clearSolosBtn_);

        updateDimming();
    }

    ~MixerSlideOverDrawer() override {
        stopTimer();
        for (auto& strip : strips_) {
            strip.fader.setLookAndFeel(nullptr);
        }
        masterFader_.setLookAndFeel(nullptr);
    }

    bool isDrawerOpen() const noexcept { return isOpen_; }

    void setDrawerOpen(bool open) {
        if (isOpen_ == open) return;
        isOpen_ = open;
        if (isOpen_) {
            grabKeyboardFocus();
            updateDimming();
        }
        startTimerHz(60);
    }

    void toggleDrawer() {
        setDrawerOpen(!isOpen_);
    }

    bool hitTest(int x, int y) override {
        // Never intercept top header bar or bottom keyboard bar
        if (y < topY_ || y > bottomY_) {
            return false;
        }

        // When drawer is open or animating, intercept workspace area
        if (isOpen_ || openProgress_ > 0.001f) {
            return true;
        }

        // When closed, ONLY the edge handle tab intercepts clicks
        return handleRect_.expanded(0, 18).contains(x, y);
    }

    void paint(juce::Graphics& g) override {
        if (openProgress_ <= 0.001f) {
            // Draw clean organic closed handle tab curving into the screen from the right
            drawClosedTab(g, getWidth(), handleRect_.toFloat());
            return;
        }

        // 1. Scrim (dimmed backdrop over the middle workspace area)
        const auto scrimArea = juce::Rectangle<int>(0, topY_, drawerRect_.getX(), bottomY_ - topY_);
        if (!scrimArea.isEmpty()) {
            g.setColour(juce::Colours::black.withAlpha(0.60f * openProgress_));
            g.fillRect(scrimArea);
        }

        // 2. Build paths: fill path (closed) and stroke path (open, NO RIGHT BORDER!)
        const auto drwF = drawerRect_.toFloat();
        const auto hF   = handleRect_.toFloat();
        const float winRight = static_cast<float>(getWidth());
        const auto fillPath   = buildDrawerFillPath(drwF, hF, winRight);
        const auto strokePath = buildDrawerStrokePath(drwF, hF, winRight);

        // Left soft drop shadow behind drawer and handle tab
        const auto shadowGrad = juce::ColourGradient::horizontal(
            juce::Colours::black.withAlpha(0.65f * openProgress_), hF.getX() - 16.0f,
            juce::Colours::transparentBlack, hF.getX() + 6.0f);
        g.setGradientFill(shadowGrad);
        g.fillRect(juce::Rectangle<float>(hF.getX() - 16.0f, drwF.getY(), 16.0f, drwF.getHeight()));

        // Solid High-Contrast Studio Desk background fill (Deep obsidian slate 0xff080c14)
        // (Fill path only, with perfectly rounded corners - no square rect bleed!)
        g.setColour(juce::Colour(0xff080c14));
        g.fillPath(fillPath);

        // 1.1: Distinct, thicker border stroke (2.0f stroke): Top, Left, Tab, Bottom (NO RIGHT BORDER!)
        const float strokeW = isHandleHovered_ ? 2.4f : 2.0f;
        g.setColour(isHandleHovered_ ? theme_.accentPrimary : juce::Colour(0xff4b6584));
        g.strokePath(strokePath, juce::PathStrokeType(strokeW, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

        // Top subtle highlight line
        g.setColour(juce::Colours::white.withAlpha(0.08f * openProgress_));
        g.drawHorizontalLine(drawerRect_.getY() + 1, drwF.getX() + 10.0f, winRight);

        // 3. Tab Interior Details (Grip Dots + Vertical Text "MIXER")
        drawTabDetails(g, hF);

        // 4. Drawer Header
        drawHeader(g);

        // 5. Channel Strips (8 Sources)
        drawChannelStrips(g);

        // 6. Master Bus Strip
        drawMasterSection(g);
    }

    void resized() override {
        const int totalW = getWidth();
        const int totalH = getHeight();
        if (totalW <= 0 || totalH <= 0) return;

        // Floating Mixer bounds: floats below top bar (y = 58) and above piano bar (bottom = totalH - 74)
        topY_ = 58;
        bottomY_ = std::max(topY_ + 380, totalH - 74);
        const int mixerH = bottomY_ - topY_;

        // Drawer width: ~70% of UI width
        const int drawerW = std::clamp(static_cast<int>(static_cast<float>(totalW) * 0.70f), 680, 980);
        const int closedX = totalW;
        const int openedX = totalW - drawerW; // EXACTLY FLUSH with right window edge!
        const int currentX = static_cast<int>(std::round(static_cast<float>(closedX) - static_cast<float>(closedX - openedX) * openProgress_));

        // Drawer right edge is ALWAYS flush with totalW (0 px gap)
        drawerRect_ = juce::Rectangle<int>(currentX, topY_, totalW - currentX, mixerH);

        // Handle Tab: 28px wide, 98px high, vertically centered on drawer left edge
        constexpr int hW = 28;
        constexpr int hH = 98;
        const int hY = topY_ + (mixerH - hH) / 2;
        handleRect_ = juce::Rectangle<int>(drawerRect_.getX() - hW, hY, hW, hH);

        // Drawer Internal Layout
        auto dArea = drawerRect_.reduced(10, 8);

        // Header (36px)
        auto headerArea = dArea.removeFromTop(36);
        closeBtn_.setBounds(headerArea.removeFromRight(26).withSizeKeepingCentre(22, 22));
        headerArea.removeFromRight(8);

        // Solo Reset Button (only shown if solo active)
        const bool hasSolo = proc_.hasAnySolo();
        clearSolosBtn_.setVisible(hasSolo);
        if (hasSolo) {
            clearSolosBtn_.setBounds(headerArea.removeFromRight(88).reduced(0, 4));
        }

        dArea.removeFromTop(6);

        // Split width: 8 Sources (81%) and Master (19%)
        constexpr int gap = 8;
        const int availW = dArea.getWidth() - gap;
        const int wChans = static_cast<int>(static_cast<float>(availW) * 0.81f);

        channelsCardRect_ = dArea.removeFromLeft(wChans);
        dArea.removeFromLeft(gap);
        masterCardRect_ = dArea;

        // Layout 8 Source Channel Strips
        const int colW = (channelsCardRect_.getWidth() - 7 * 4) / 8;
        const int colH = channelsCardRect_.getHeight();

        for (int i = 0; i < 8; ++i) {
            const int colX = channelsCardRect_.getX() + i * (colW + 4);
            auto cCol = juce::Rectangle<int>(colX, channelsCardRect_.getY(), colW, colH);
            auto& strip = strips_[static_cast<std::size_t>(i)];

            // 1. Header space (Channel Name, Layer color, Engine badge): 38px
            cCol.removeFromTop(38);

            cCol.removeFromTop(6);

            // 2. Octave Stepper ([- 0 OCT +]): 24px
            if (strip.octStepper) {
                strip.octStepper->setBounds(cCol.removeFromTop(24).reduced(2, 0));
            }

            cCol.removeFromTop(6);

            // 3. Dual Stacked Filter Routing Buttons: FLT 1 & FLT 2
            strip.flt1Btn.setBounds(cCol.removeFromTop(19).reduced(4, 0));
            cCol.removeFromTop(3);
            strip.flt2Btn.setBounds(cCol.removeFromTop(19).reduced(4, 0));

            cCol.removeFromTop(8);

            // 4. Pan Knob: Full-Sized ArcKnob (64px high, matching filter deck!)
            if (strip.pan) {
                strip.pan->setBounds(cCol.removeFromTop(64).reduced(2, 0));
            }

            cCol.removeFromTop(8);

            // 5. Solo / Mute row: 24px (comfortable click targets)
            auto smRow = cCol.removeFromTop(24).reduced(4, 0);
            const int btnW = (smRow.getWidth() - 4) / 2;
            strip.soloBtn.setBounds(smRow.removeFromLeft(btnW));
            strip.muteBtn.setBounds(smRow.removeFromRight(btnW));

            cCol.removeFromTop(12);

            // 6. Fader & Meter Area (remaining height: balanced throw ~155px)
            const int meterW = 7;
            cCol.removeFromRight(1);
            stripMeterRects_[static_cast<std::size_t>(i)] = cCol.removeFromRight(meterW).withTrimmedBottom(20);
            cCol.removeFromRight(2);
            strip.fader.setBounds(cCol);
        }

        // Layout Master Section
        auto mCol = masterCardRect_;
        mCol.removeFromTop(38); // Match header height

        mCol.removeFromTop(8);

        // FX Returns (Delay & Reverb): Full-sized ArcKnobs (68px high!)
        delayReturn_.setBounds(mCol.removeFromTop(68).reduced(4, 0));
        mCol.removeFromTop(6);
        reverbReturn_.setBounds(mCol.removeFromTop(68).reduced(4, 0));

        mCol.removeFromTop(16);

        // Master Fader & Dual Meter
        constexpr int mMeterW = 12;
        mCol.removeFromRight(6);
        masterMeterRect_ = mCol.removeFromRight(mMeterW).withTrimmedBottom(20);
        mCol.removeFromRight(6);
        masterFader_.setBounds(mCol);
    }

    void mouseMove(const juce::MouseEvent& event) override {
        const auto pos = event.getPosition();
        const bool hoverH = handleRect_.expanded(0, 18).contains(pos);
        if (hoverH != isHandleHovered_) {
            isHandleHovered_ = hoverH;
            setMouseCursor(isHandleHovered_ ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint(handleRect_.expanded(0, 18));
        }
    }

    void mouseExit(const juce::MouseEvent&) override {
        if (isHandleHovered_) {
            isHandleHovered_ = false;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint(handleRect_.expanded(0, 18));
        }
    }

    void mouseDown(const juce::MouseEvent& event) override {
        const auto pos = event.getPosition();

        // 1. Clicked the Handle tab (including smooth shoulders)
        if (handleRect_.expanded(0, 18).contains(pos)) {
            toggleDrawer();
            return;
        }

        // 2. Clicked the darkened Scrim (backdrop)
        if (isOpen_ && pos.x < drawerRect_.getX()) {
            setDrawerOpen(false);
            return;
        }

        // 3. Clicked any Channel Header area to toggle Enable / Mute
        if (isOpen_ && channelsCardRect_.contains(pos)) {
            const int colW = (channelsCardRect_.getWidth() - 7 * 4) / 8;
            for (int i = 0; i < 8; ++i) {
                const int colX = channelsCardRect_.getX() + i * (colW + 4);
                const juce::Rectangle<int> headerArea(colX, channelsCardRect_.getY(), colW, 38);
                if (headerArea.contains(pos)) {
                    const juce::String paramId = "osc" + juce::String(i + 1) + "_enabled";
                    if (auto* p = proc_.getAPVTS().getParameter(paramId)) {
                        const float cur = p->getValue();
                        p->setValueNotifyingHost(cur > 0.5f ? 0.0f : 1.0f);
                        updateDimming();
                        repaint();
                    }
                    return;
                }
            }
        }
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key == juce::KeyPress::escapeKey && isOpen_) {
            setDrawerOpen(false);
            return true;
        }
        return false;
    }

private:
    struct ChannelStripUI {
        std::unique_ptr<OctaveStepperComponent> octStepper;
        ConsoleStripButton flt1Btn;
        ConsoleStripButton flt2Btn;
        std::unique_ptr<nova::ui::ArcKnob> pan;
        ConsoleStripButton soloBtn;
        ConsoleStripButton muteBtn;
        juce::Slider fader;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> faderAtt;
    };

    void toggleFilterRouting(int slotIdx, int targetFilter) {
        const juce::String paramId = "osc" + juce::String(slotIdx + 1) + "_filterRouting";
        if (auto* p = proc_.getAPVTS().getParameter(paramId)) {
            const int curRoute = static_cast<int>(p->convertFrom0to1(p->getValue()) + 0.5f);
            bool flt1 = (curRoute == 0 || curRoute == 2);
            bool flt2 = (curRoute == 1 || curRoute == 2);

            if (targetFilter == 1) flt1 = !flt1;
            if (targetFilter == 2) flt2 = !flt2;

            int newRoute = 3; // DIRECT
            if (flt1 && flt2) newRoute = 2; // 1+2
            else if (flt1)   newRoute = 0; // FLT 1
            else if (flt2)   newRoute = 1; // FLT 2

            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(newRoute)));
            updateDimming();
            repaint();
        }
    }

    void updateDimming() {
        for (int i = 0; i < 8; ++i) {
            auto& strip = strips_[static_cast<std::size_t>(i)];
            const juce::String enabledId = "osc" + juce::String(i + 1) + "_enabled";
            bool isEnabled = true;
            if (auto* p = proc_.getAPVTS().getRawParameterValue(enabledId)) {
                isEnabled = p->load() > 0.5f;
            }

            const auto col = getLayerColour(i);
            const bool isSolo = proc_.isSlotSoloed(i);
            const bool isMuted = proc_.isSlotMuted(i);

            // Channel Dimming:
            // - Powered off (oscX_enabled == false): 45% alpha
            // - Powered on & muted: 75% alpha
            // - Powered on & unmuted: 100% full vibrant brightness
            const float a = isEnabled ? (isMuted ? 0.75f : 1.0f) : 0.45f;
            if (strip.pan) strip.pan->setAlpha(a);
            strip.fader.setAlpha(a);
            if (strip.octStepper) {
                strip.octStepper->setAlpha(a);
                const juce::String octId = "osc" + juce::String(i + 1) + "_octave";
                if (auto* pOct = proc_.getAPVTS().getRawParameterValue(octId)) {
                    strip.octStepper->currentOctave = static_cast<int>(std::round(pOct->load()));
                }
            }

            // Update Filter 1 & Filter 2 button states (colored with OSC color when active!)
            const juce::String routeId = "osc" + juce::String(i + 1) + "_filterRouting";
            int routeVal = 0;
            if (auto* pR = proc_.getAPVTS().getRawParameterValue(routeId)) {
                routeVal = static_cast<int>(pR->load());
            }
            const bool hasFlt1 = (routeVal == 0 || routeVal == 2);
            const bool hasFlt2 = (routeVal == 1 || routeVal == 2);

            // 3. Highlight active Filter buttons with Channel/OSC color
            strip.flt1Btn.setColors(
                col.withAlpha(0.38f), col, juce::Colours::white,
                juce::Colour(0xff0d1522), juce::Colour(0xff1e293b), juce::Colour(0xff64748b));
            strip.flt1Btn.setActive(hasFlt1);

            strip.flt2Btn.setColors(
                col.withAlpha(0.38f), col, juce::Colours::white,
                juce::Colour(0xff0d1522), juce::Colour(0xff1e293b), juce::Colour(0xff64748b));
            strip.flt2Btn.setActive(hasFlt2);

            // 4. Active Solo in signature gold yellow
            strip.soloBtn.setColors(
                juce::Colour(0xffeab308), juce::Colour(0xfffacc15), juce::Colour(0xff090d16),
                juce::Colour(0xff0d1522), juce::Colour(0xff1e293b), juce::Colour(0xff64748b));
            strip.soloBtn.setActive(isSolo);

            // 5. Active Mute with darker red (real independent mixer audio mute!)
            strip.muteBtn.setColors(
                juce::Colour(0xff7f1d1d), juce::Colour(0xffdc2626), juce::Colours::white,
                juce::Colour(0xff0d1522), juce::Colour(0xff1e293b), juce::Colour(0xff64748b));
            strip.muteBtn.setActive(isMuted);
        }
    }

    void timerCallback() override {
        // Animation easing: smooth 60fps spring towards target
        const float target = isOpen_ ? 1.0f : 0.0f;
        const float diff = target - openProgress_;

        if (std::abs(diff) > 0.002f) {
            openProgress_ += diff * 0.28f;
            resized();
            repaint();
        } else {
            if (std::abs(openProgress_ - target) > 0.0001f) {
                openProgress_ = target;
                resized();
                repaint();
            }
            if (!isOpen_) {
                stopTimer(); // 0% CPU overhead when closed
                return;
            }
        }

        if (isOpen_) {
            // Live sync solo state and channel buttons
            const bool hasSolo = proc_.hasAnySolo();
            if (clearSolosBtn_.isVisible() != hasSolo) {
                clearSolosBtn_.setVisible(hasSolo);
            }
            updateDimming();
            repaint(); // Live meter redraws
        }
    }

    // Mathematical S-filleted path forming contiguous Drawer + Handle Tab (Filled)
    static juce::Path buildDrawerFillPath(juce::Rectangle<float> dRect, juce::Rectangle<float> hArea, float winRight) {
        juce::Path p;
        const float dX = dRect.getX();
        const float dY = dRect.getY();
        const float dB = dRect.getBottom();
        const float hTop = hArea.getY();
        const float hBottom = hArea.getBottom();
        const float hLeft = hArea.getX();
        constexpr float cr = 10.0f;
        constexpr float filletH = 18.0f;
        constexpr float kappa = 0.55228475f;

        // Start at top right
        p.startNewSubPath(winRight, dY);

        // 1. Top edge going left to top-left rounded corner
        p.lineTo(dX + cr, dY);

        // 2. Top-Left Corner: smooth 90 deg quarter circle to (dX, dY + cr)
        p.cubicTo(dX + cr * (1.0f - kappa), dY,
                  dX, dY + cr * (1.0f - kappa),
                  dX, dY + cr);

        // 3. Left edge going down to the start of Handle Tab fillet
        p.lineTo(dX, hTop - filletH);

        // 4. Smooth S-curve fillet outward into handle tab
        p.cubicTo(dX, hTop,
                  hLeft, hTop,
                  hLeft, hTop + filletH);

        // 5. Straight vertical left edge of the Handle Tab
        p.lineTo(hLeft, hBottom - filletH);

        // 6. Smooth S-curve fillet inward back to drawer left edge
        p.cubicTo(hLeft, hBottom,
                  dX, hBottom,
                  dX, hBottom + filletH);

        // 7. Left edge going down to bottom-left rounded corner
        p.lineTo(dX, dB - cr);

        // 8. Bottom-Left Corner: smooth 90 deg quarter circle to (dX + cr, dB)
        p.cubicTo(dX, dB - cr * (1.0f - kappa),
                  dX + cr * (1.0f - kappa), dB,
                  dX + cr, dB);

        // 9. Bottom edge going right all the way to winRight
        p.lineTo(winRight, dB);

        // 10. Up right edge to top-right
        p.lineTo(winRight, dY);

        // 11. Close cleanly
        p.closeSubPath();
        return p;
    }

    // Mathematical S-filleted path forming contiguous Drawer + Handle Tab Border (Open Path: NO RIGHT BORDER!)
    static juce::Path buildDrawerStrokePath(juce::Rectangle<float> dRect, juce::Rectangle<float> hArea, float winRight) {
        juce::Path p;
        const float dX = dRect.getX();
        const float dY = dRect.getY();
        const float dB = dRect.getBottom();
        const float hTop = hArea.getY();
        const float hBottom = hArea.getBottom();
        const float hLeft = hArea.getX();
        constexpr float cr = 10.0f;
        constexpr float filletH = 18.0f;
        constexpr float kappa = 0.55228475f;

        // Open subpath starting at top right
        p.startNewSubPath(winRight, dY);

        // 1. Top edge going left to top-left rounded corner
        p.lineTo(dX + cr, dY);

        // 2. Top-Left Corner
        p.cubicTo(dX + cr * (1.0f - kappa), dY,
                  dX, dY + cr * (1.0f - kappa),
                  dX, dY + cr);

        // 3. Left edge going down to Handle Tab
        p.lineTo(dX, hTop - filletH);

        // 4. Smooth S-curve fillet outward into handle tab
        p.cubicTo(dX, hTop,
                  hLeft, hTop,
                  hLeft, hTop + filletH);

        // 5. Straight vertical left edge of the Handle Tab
        p.lineTo(hLeft, hBottom - filletH);

        // 6. Smooth S-curve fillet inward back to drawer left edge
        p.cubicTo(hLeft, hBottom,
                  dX, hBottom,
                  dX, hBottom + filletH);

        // 7. Left edge going down to bottom-left corner
        p.lineTo(dX, dB - cr);

        // 8. Bottom-Left Corner
        p.cubicTo(dX, dB - cr * (1.0f - kappa),
                  dX + cr * (1.0f - kappa), dB,
                  dX + cr, dB);

        // 9. Bottom edge going right to winRight (STOPS HERE, DOES NOT CLOSE!)
        p.lineTo(winRight, dB);

        return p;
    }

    // Closed state: organic curved tab protruding from right window edge (NO RIGHT STROKE!)
    void drawClosedTab(juce::Graphics& g, int totalW, juce::Rectangle<float> hArea) {
        juce::Path pFill;
        const float winRight = static_cast<float>(totalW);
        const float hTop = hArea.getY();
        const float hBottom = hArea.getBottom();
        const float hLeft = hArea.getX();
        constexpr float filletH = 18.0f;

        // Fill Path (closed against right edge)
        pFill.startNewSubPath(winRight, hTop - filletH);
        pFill.cubicTo(winRight, hTop,
                      hLeft, hTop,
                      hLeft, hTop + filletH);
        pFill.lineTo(hLeft, hBottom - filletH);
        pFill.cubicTo(hLeft, hBottom,
                      winRight, hBottom,
                      winRight, hBottom + filletH);
        pFill.closeSubPath();

        // Background
        const auto bgGrad = juce::ColourGradient::horizontal(
            isHandleHovered_ ? juce::Colour(0xff223249) : juce::Colour(0xff111a28), hLeft,
            theme_.bgSurface.darker(0.16f), winRight);
        g.setGradientFill(bgGrad);
        g.fillPath(pFill);

        // Stroke Path (OPEN PATH: NO RIGHT BORDER!)
        juce::Path pStroke;
        pStroke.startNewSubPath(winRight, hTop - filletH);
        pStroke.cubicTo(winRight, hTop,
                        hLeft, hTop,
                        hLeft, hTop + filletH);
        pStroke.lineTo(hLeft, hBottom - filletH);
        pStroke.cubicTo(hLeft, hBottom,
                        winRight, hBottom,
                        winRight, hBottom + filletH);

        const float strokeW = isHandleHovered_ ? 2.4f : 2.0f;
        g.setColour(isHandleHovered_ ? theme_.accentPrimary : juce::Colour(0xff4b6584));
        g.strokePath(pStroke, juce::PathStrokeType(strokeW, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

        drawTabDetails(g, hArea);
    }

    // Handle interior details: subtle vertical grip texture + elegant "MIXER" text
    void drawTabDetails(juce::Graphics& g, juce::Rectangle<float> hArea) {
        const float hLeft = hArea.getX();
        const float hCenterY = hArea.getCentreY();

        // 1. Grip Dots Texture (matching screenshot reference :::)
        g.setColour(isHandleHovered_ ? theme_.accentPrimary.withAlpha(0.85f) : juce::Colour(0xff64748b));
        const float dotX1 = hLeft + 5.5f;
        const float dotX2 = hLeft + 9.0f;
        for (int row = -2; row <= 2; ++row) {
            const float dotY = hCenterY + static_cast<float>(row) * 5.0f;
            g.fillEllipse(dotX1 - 1.0f, dotY - 1.0f, 2.0f, 2.0f);
            g.fillEllipse(dotX2 - 1.0f, dotY - 1.0f, 2.0f, 2.0f);
        }

        // 2. Refined vertical typography "M I X E R"
        g.setFont(theme_.getSectionFont(11.0f));
        static constexpr std::array<const char*, 5> kLetters = { "M", "I", "X", "E", "R" };
        const float letterH = (hArea.getHeight() - 18.0f) / 5.0f;
        const float textX = hLeft + 12.0f;
        const float textW = hArea.getRight() - textX;

        for (std::size_t i = 0; i < 5; ++i) {
            const auto lRect = juce::Rectangle<float>(textX, hArea.getY() + 9.0f + static_cast<float>(i) * letterH, textW, letterH);
            g.setColour(isHandleHovered_ ? theme_.accentPrimary : juce::Colour(0xffcbd5e1));
            g.drawText(kLetters[i], lRect, juce::Justification::centred);
        }
    }

    void drawHeader(juce::Graphics& g) {
        const auto hArea = drawerRect_.withHeight(40).reduced(12, 6).toFloat();

        // Title
        g.setFont(theme_.getSectionFont(14.0f));
        g.setColour(theme_.accentPrimary);
        g.drawText("CONSOLE MIXER", hArea.withWidth(160.0f), juce::Justification::centredLeft);

        // Subtitle
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(theme_.textMuted);
        g.drawText("8 SOUND ENGINES + MASTER BUS", hArea.withTrimmedLeft(165.0f).withWidth(210.0f), juce::Justification::centredLeft);

        // Style the Solo Reset Button if visible
        if (clearSolosBtn_.isVisible()) {
            clearSolosBtn_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff59e0b).withAlpha(0.20f));
            clearSolosBtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff59e0b));
        }
    }

    void drawChannelStrips(juce::Graphics& g) {
        // High-contrast deep obsidian background for channel deck
        g.setColour(juce::Colour(0xff0b101a));
        g.fillRoundedRectangle(channelsCardRect_.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xff223249));
        g.drawRoundedRectangle(channelsCardRect_.toFloat(), 6.0f, 1.2f);

        const auto curParams = proc_.getCurrentParameters();

        for (int i = 0; i < 8; ++i) {
            auto& strip = strips_[static_cast<std::size_t>(i)];
            const auto colW = (channelsCardRect_.getWidth() - 7 * 4) / 8;
            const int colX = channelsCardRect_.getX() + i * (colW + 4);
            const auto laneR = juce::Rectangle<float>(static_cast<float>(colX), static_cast<float>(channelsCardRect_.getY()), static_cast<float>(colW), static_cast<float>(channelsCardRect_.getHeight()));

            // Vertical channel divider
            if (i > 0) {
                g.setColour(juce::Colour(0xff182436));
                g.drawVerticalLine(colX - 2, laneR.getY() + 4.0f, laneR.getBottom() - 4.0f);
            }

            // Power state
            const juce::String enabledId = "osc" + juce::String(i + 1) + "_enabled";
            bool isEnabled = true;
            if (auto* p = proc_.getAPVTS().getRawParameterValue(enabledId)) {
                isEnabled = p->load() > 0.5f;
            }

            // 1. Channel Header
            const auto headerR = laneR.withHeight(38.0f).reduced(2.0f, 2.0f);
            const auto col = getLayerColour(i);
            const juce::String name = getLayerShortName(curParams, i);
            const juce::String eng = getEngineShortName(curParams, i);

            // Layer color top bar (subtle when muted, vibrant when active)
            g.setColour(isEnabled ? col : col.withAlpha(0.40f));
            g.fillRoundedRectangle(headerR.withHeight(3.0f), 1.5f);

            // Channel Name: Crisp white text
            g.setFont(theme_.getSectionFont(12.0f));
            g.setColour(isEnabled ? juce::Colours::white : juce::Colour(0xff94a3b8));
            g.drawText(name, headerR.withTrimmedTop(5.0f).withHeight(15.0f), juce::Justification::centred);

            // Engine type badge: Vibrant color
            g.setFont(theme_.getLabelFont(9.0f));
            g.setColour(isEnabled ? col : col.withAlpha(0.50f));
            g.drawText(eng, headerR.withTrimmedTop(20.0f).withHeight(13.0f), juce::Justification::centred);

            // 2. Draw dB scale ticks beside fader
            drawChannelFaderTicks(g, strip.fader, col);

            // 4. Draw Live Peak Meter LED Bar
            const auto mRect = stripMeterRects_[static_cast<std::size_t>(i)].toFloat();
            const float pk = isEnabled ? proc_.getSourcePeak(i) : 0.0f;
            drawMeterBar(g, mRect, pk);
        }
    }

    void drawMasterSection(juce::Graphics& g) {
        g.setColour(juce::Colour(0xff090e17));
        g.fillRoundedRectangle(masterCardRect_.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.65f));
        g.drawRoundedRectangle(masterCardRect_.toFloat(), 6.0f, 1.2f);

        // Master Header
        const auto hR = masterCardRect_.toFloat().withHeight(38.0f).reduced(6.0f, 2.0f);
        g.setColour(juce::Colour(0xfff59e0b));
        g.fillRoundedRectangle(hR.withHeight(3.0f), 1.5f);

        g.setFont(theme_.getSectionFont(12.0f));
        g.setColour(juce::Colour(0xfff59e0b));
        g.drawText("MASTER", hR.withTrimmedTop(5.0f).withHeight(15.0f), juce::Justification::centred);

        g.setFont(theme_.getLabelFont(9.0f));
        g.setColour(juce::Colour(0xffcbd5e1));
        g.drawText("OUT BUS", hR.withTrimmedTop(20.0f).withHeight(13.0f), juce::Justification::centred);

        // Draw Master Ticks
        drawMasterFaderTicks(g, masterFader_);

        // Draw Dual Master Peak Meter (L & R)
        const auto mRect = masterMeterRect_.toFloat();
        const float pkL = proc_.getMasterPeakL();
        const float pkR = proc_.getMasterPeakR();

        const float halfW = (mRect.getWidth() - 2.0f) * 0.5f;
        drawMeterBar(g, mRect.withWidth(halfW), pkL);
        drawMeterBar(g, mRect.withTrimmedLeft(halfW + 2.0f), pkR);
    }

    static float getFaderThumbLeft(const juce::Slider& slider) noexcept {
        return ConsoleFaderLookAndFeel::calculateThumbLeft(
            static_cast<float>(slider.getX()),
            static_cast<float>(slider.getWidth()));
    }

    void drawChannelFaderTicks(juce::Graphics& g, const juce::Slider& slider, juce::Colour /*accentCol*/) {
        const float thumbL = getFaderThumbLeft(slider);
        const float sliderY = static_cast<float>(slider.getY());

        struct TickDef {
            float val;
            const char* label;
            bool isMajor;
            bool isUnity;
        };

        // Standard console fader calibration (Unity 0 dB at max throw, musical logarithmic graduations)
        static constexpr std::array<TickDef, 7> kTicks = {{
            { 1.0000f, "0",    true,  true  }, // Unity gain (0 dB)
            { 0.7079f, "-3",   true,  false }, // -3 dB
            { 0.5012f, "-6",   true,  false }, // -6 dB
            { 0.2512f, "-12",  true,  false }, // -12 dB
            { 0.1259f, "",     false, false }, // -18 dB (minor tick)
            { 0.0631f, "",     false, false }, // -24 dB (minor tick)
            { 0.0000f, "-inf", true,  false }  // -inf
        }};

        for (const auto& t : kTicks) {
            const float y = sliderY + slider.getPositionOfValue(t.val);
            const float tickLen = t.isUnity ? 6.0f : (t.isMajor ? 4.5f : 3.0f);
            const float strokeW = t.isUnity ? 1.6f : 1.0f;

            if (t.isUnity) {
                g.setColour(juce::Colours::white);
            } else if (t.isMajor) {
                g.setColour(juce::Colour(0xff64748b));
            } else {
                g.setColour(juce::Colour(0xff334155));
            }

            g.drawLine(thumbL - 1.5f - tickLen, y, thumbL - 1.5f, y, strokeW);

            if (t.label != nullptr && t.label[0] != '\0') {
                if (t.isUnity) {
                    g.setFont(theme_.getValueFont(9.5f));
                    g.setColour(juce::Colours::white);
                } else {
                    g.setFont(theme_.getValueFont(9.0f));
                    g.setColour(juce::Colour(0xff94a3b8));
                }
                const auto textRect = juce::Rectangle<float>(thumbL - 25.0f, y - 6.0f, 16.0f, 12.0f);
                g.drawText(t.label, textRect, juce::Justification::centredRight, false);
            }
        }
    }

    void drawMasterFaderTicks(juce::Graphics& g, const juce::Slider& slider) {
        const float thumbL = getFaderThumbLeft(slider);
        const float sliderY = static_cast<float>(slider.getY());

        struct TickDef {
            float val;
            const char* label;
            bool isMajor;
            bool isUnity;
        };

        // Master Gain range is -24.0 dB to +12.0 dB with Unity (0 dB) at ~33% from top
        static constexpr std::array<TickDef, 13> kTicks = {{
            {  12.0f, "+12", true,  false },
            {   9.0f, "",    false, false },
            {   6.0f, "+6",  true,  false },
            {   3.0f, "",    false, false },
            {   0.0f, "0",   true,  true  }, // Unity Gain (0 dB) reference
            {  -3.0f, "",    false, false },
            {  -6.0f, "-6",  true,  false },
            {  -9.0f, "",    false, false },
            { -12.0f, "-12", true,  false },
            { -15.0f, "",    false, false },
            { -18.0f, "-18", true,  false },
            { -21.0f, "",    false, false },
            { -24.0f, "-24", true,  false }  // Minimum (-24 dB / -inf)
        }};

        for (const auto& t : kTicks) {
            const float y = sliderY + slider.getPositionOfValue(t.val);
            const float tickLen = t.isUnity ? 7.0f : (t.isMajor ? 4.5f : 3.0f);
            const float strokeW = t.isUnity ? 1.8f : 1.0f;

            if (t.isUnity) {
                g.setColour(juce::Colour(0xfff59e0b)); // Master amber highlight for 0 dB
            } else if (t.val > 0.0f) {
                g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.65f));
            } else if (t.isMajor) {
                g.setColour(juce::Colour(0xff64748b));
            } else {
                g.setColour(juce::Colour(0xff334155));
            }

            g.drawLine(thumbL - 1.5f - tickLen, y, thumbL - 1.5f, y, strokeW);

            if (t.label != nullptr && t.label[0] != '\0') {
                if (t.isUnity) {
                    g.setFont(theme_.getValueFont(10.0f));
                    g.setColour(juce::Colour(0xfffbbf24)); // Warm golden amber 0 dB
                } else if (t.val > 0.0f) {
                    g.setFont(theme_.getValueFont(9.0f));
                    g.setColour(juce::Colour(0xfff59e0b)); // Amber for boost
                } else {
                    g.setFont(theme_.getValueFont(9.0f));
                    g.setColour(juce::Colour(0xffcbd5e1)); // Clean crisp silver-gray
                }
                const auto textRect = juce::Rectangle<float>(thumbL - 27.0f, y - 6.0f, 18.0f, 12.0f);
                g.drawText(t.label, textRect, juce::Justification::centredRight, false);
            }
        }
    }

    static void drawMeterBar(juce::Graphics& g, juce::Rectangle<float> area, float level) {
        // Meter background track
        g.setColour(juce::Colour(0xff090d16));
        g.fillRoundedRectangle(area, 2.0f);
        g.setColour(juce::Colour(0xff1e293b).withAlpha(0.5f));
        g.drawRoundedRectangle(area, 2.0f, 0.8f);

        if (level <= 0.001f) return;

        // Level to height (clamp 0..1)
        const float fillH = std::clamp(level, 0.0f, 1.0f) * area.getHeight();
        const auto fillRect = area.withTrimmedTop(area.getHeight() - fillH);

        // Tri-color LED gradient: green (-inf..-12dB), yellow (-12..-3dB), red (clip > 0dB)
        const auto grad = juce::ColourGradient::vertical(
            level > 0.95f ? juce::Colour(0xffef4444) : (level > 0.70f ? juce::Colour(0xfff59e0b) : juce::Colour(0xff2dd4bf)),
            fillRect.getY(),
            juce::Colour(0xff10b981), fillRect.getBottom());
        g.setGradientFill(grad);
        g.fillRoundedRectangle(fillRect, 2.0f);
    }

    static juce::Image loadIcon(const char* resourceName, const juce::String& fallbackFilename) {
        int dataSize = 0;
        const char* data = NovaResources::getNamedResource(resourceName, dataSize);
        if (data != nullptr && dataSize > 0) {
            auto img = juce::ImageFileFormat::loadFrom(data, static_cast<std::size_t>(dataSize));
            if (img.isValid()) return img;
        }
        const auto fallbackFile = juce::File("/Users/wolfgangmeincke/Nova Plugins/Resources/Icons").getChildFile(fallbackFilename);
        if (fallbackFile.existsAsFile()) {
            auto img = juce::ImageFileFormat::loadFrom(fallbackFile);
            if (img.isValid()) return img;
        }
        return {};
    }

    static juce::Colour getLayerColour(int index) noexcept {
        static constexpr std::array<uint32_t, 8> kColours = {
            0xff38bdf8, // Cyan
            0xff2dd4bf, // Teal
            0xffec4899, // Pink
            0xffa855f7, // Purple
            0xff84cc16, // Lime
            0xfff59e0b, // Amber
            0xff06b6d4, // Deep Cyan
            0xff8b5cf6  // Violet
        };
        return juce::Colour(kColours[static_cast<std::size_t>(index % 8)]);
    }

    static juce::String getLayerShortName(const nova::dsp::WorshipSynthEngine::Parameters& p, int i) {
        const auto& s = p.oscSlots[static_cast<std::size_t>(i)];
        if (s.sourceType == 1) return "SUB";
        if (s.sourceType == 2) return "PLUCK";
        if (s.sourceType == 3) return "NOISE";
        int oscIdx = 0;
        for (int j = 0; j < i; ++j) {
            if (p.oscSlots[static_cast<std::size_t>(j)].sourceType == 0) oscIdx++;
        }
        return juce::String("OSC ") + juce::String::charToString(static_cast<char>('A' + oscIdx));
    }

    static juce::String getEngineShortName(const nova::dsp::WorshipSynthEngine::Parameters& p, int i) {
        const auto& s = p.oscSlots[static_cast<std::size_t>(i)];
        if (s.sourceType == 1) return "SUB OSC";
        if (s.sourceType == 2) return "PLUCK";
        if (s.sourceType == 3) return "NOISE";
        if (s.sourceType == 4) return "SAMPLE";
        using ET = nova::dsp::MultiEngineOscillator::EngineType;
        switch (s.engine) {
            case ET::WarmAnalog:        return "ANALOG";
            case ET::EvolvingWavetable: return "WAVETABLE";
            case ET::OrganicTexture:    return "TEXTURE";
            case ET::PhysicalPluckModal:return "PLUCK";
            case ET::SubOscillator:     return "SUB";
            case ET::NoiseTransient:    return "NOISE";
            case ET::SamplePlayback:    return "SAMPLE";
        }
        return "ANALOG";
    }

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;

    ConsoleFaderLookAndFeel faderLookAndFeel_;

    bool isOpen_ = false;
    float openProgress_ = 0.0f;
    bool isHandleHovered_ = false;

    int topY_ = 50;
    int bottomY_ = 650;

    juce::Rectangle<int> drawerRect_;
    juce::Rectangle<int> handleRect_;
    juce::Rectangle<int> channelsCardRect_;
    juce::Rectangle<int> masterCardRect_;

    std::array<ChannelStripUI, 8> strips_;
    std::array<juce::Rectangle<int>, 8> stripMeterRects_;

    // Master Section
    juce::Slider masterFader_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterFaderAtt_;
    nova::ui::ArcKnob delayReturn_;
    nova::ui::ArcKnob reverbReturn_;
    juce::Rectangle<int> masterMeterRect_;

    DrawerCloseButton closeBtn_;
    juce::TextButton clearSolosBtn_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerSlideOverDrawer)
};

} // namespace nova::synth::ui
