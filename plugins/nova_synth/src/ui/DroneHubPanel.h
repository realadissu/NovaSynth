#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <nova_ui/NovaTheme.h>
#include <nova_ui/ArcKnob.h>
#include "../NovaSynthProcessor.h"

namespace nova::synth::ui {

/**
 * @brief Realistic Piano Key Button with authentic contours, ivory/ebony gradients,
 * drop shadows, beveled lips, and warm worship glow states.
 */
class PianoKeyButton : public juce::Button {
public:
    PianoKeyButton() : juce::Button("Key") {
        setClickingTogglesState(false);
    }

    void setup(const juce::String& noteName, bool isBlack, const nova::ui::NovaTheme* theme) {
        noteName_ = noteName;
        isBlack_ = isBlack;
        theme_ = theme;
        setButtonText(noteName);
        setTooltip("Drone Root Note: " + noteName);
    }

    [[nodiscard]] bool isBlackKey() const noexcept { return isBlack_; }

    void paintButton(juce::Graphics& g, bool isHighlighted, bool isDown) override {
        juce::ignoreUnused(isDown);
        const auto bounds = getLocalBounds().toFloat();
        const bool active = getToggleState();

        if (isBlack_) {
            // 1. Physical 3D Drop Shadow onto the white keys below
            g.setColour(juce::Colours::black.withAlpha(0.48f));
            g.fillRoundedRectangle(bounds.translated(0.0f, 2.5f).reduced(0.5f, 0.0f), 2.5f);

            // 2. Black Key Body with rounded bottom corners
            const auto keyRect = bounds.reduced(0.5f, 0.0f);
            juce::Path keyPath;
            keyPath.addRoundedRectangle(keyRect.getX(), keyRect.getY(), keyRect.getWidth(), keyRect.getHeight(),
                                        2.5f, 2.5f, false, false, true, true);

            if (active) {
                // Active Glowing Amber Ebony Key
                juce::ColourGradient grad(juce::Colour(0xfff59e0b), keyRect.getCentreX(), keyRect.getY(),
                                          juce::Colour(0xffb45309), keyRect.getCentreX(), keyRect.getBottom(), false);
                g.setGradientFill(grad);
                g.fillPath(keyPath);

                // Front beveled facet
                g.setColour(juce::Colour(0xff78350f));
                g.fillRoundedRectangle(keyRect.getX(), keyRect.getBottom() - 5.0f, keyRect.getWidth(), 5.0f, 1.5f);

                g.setColour(juce::Colour(0xfffef3c7));
                g.strokePath(keyPath, juce::PathStrokeType(1.2f));
            } else {
                // Classic Matte Ebony Black Key with 3D front facet
                const auto topColor = isHighlighted ? juce::Colour(0xff2d3748) : juce::Colour(0xff1e242f);
                const auto botColor = isHighlighted ? juce::Colour(0xff1a202c) : juce::Colour(0xff0d1117);
                juce::ColourGradient grad(topColor, keyRect.getCentreX(), keyRect.getY(),
                                          botColor, keyRect.getCentreX(), keyRect.getBottom(), false);
                g.setGradientFill(grad);
                g.fillPath(keyPath);

                // Front beveled slope
                g.setColour(juce::Colour(0xff080c12));
                g.fillRoundedRectangle(keyRect.getX(), keyRect.getBottom() - 5.0f, keyRect.getWidth(), 5.0f, 1.5f);

                // Subtle edge reflection
                g.setColour(juce::Colour(0xff475569).withAlpha(0.40f));
                g.strokePath(keyPath, juce::PathStrokeType(1.0f));
            }

            // Note Name label
            g.setFont(theme_ ? theme_->getLabelFont(10.0f).boldened() : juce::Font(juce::FontOptions{10.0f}.withStyle("Bold")));
            g.setColour(active ? juce::Colours::white : juce::Colour(0xff94a3b8));
            g.drawText(noteName_, bounds.withTrimmedBottom(6.0f), juce::Justification::centredBottom, false);
        } else {
            // White Key Body with authentic ivory contours
            const auto keyRect = bounds;
            juce::Path keyPath;
            // Realistic rounded bottom corners (radius 3.5px)
            keyPath.addRoundedRectangle(keyRect.getX(), keyRect.getY(), keyRect.getWidth(), keyRect.getHeight() - 1.0f,
                                        3.5f, 3.5f, false, false, true, true);

            if (active) {
                // Active Warm Ivory/Amber Glowing Key
                juce::ColourGradient grad(juce::Colour(0xfffef3c7), keyRect.getCentreX(), keyRect.getY(),
                                          juce::Colour(0xfff59e0b), keyRect.getCentreX(), keyRect.getBottom(), false);
                g.setGradientFill(grad);
                g.fillPath(keyPath);

                // Front bevel lip
                g.setColour(juce::Colour(0xffd97706));
                g.fillRoundedRectangle(keyRect.getX(), keyRect.getBottom() - 4.0f, keyRect.getWidth(), 3.0f, 1.5f);

                g.setColour(juce::Colour(0xffb45309));
                g.strokePath(keyPath, juce::PathStrokeType(1.2f));
            } else {
                // Realistic Ivory Key with subtle top-to-bottom shading
                const auto topColor = isHighlighted ? juce::Colour(0xffffffff) : juce::Colour(0xfff8fafc);
                const auto botColor = isHighlighted ? juce::Colour(0xfff1f5f9) : juce::Colour(0xffe2e8f0);
                juce::ColourGradient grad(topColor, keyRect.getCentreX(), keyRect.getY(),
                                          botColor, keyRect.getCentreX(), keyRect.getBottom(), false);
                g.setGradientFill(grad);
                g.fillPath(keyPath);

                // Front bevel lip
                g.setColour(juce::Colour(0xffcbd5e1));
                g.fillRoundedRectangle(keyRect.getX(), keyRect.getBottom() - 4.0f, keyRect.getWidth(), 3.0f, 1.5f);

                // Key slit / border
                g.setColour(juce::Colour(0xff94a3b8).withAlpha(0.45f));
                g.strokePath(keyPath, juce::PathStrokeType(1.0f));
            }

            // Note Name label in lower third of key (below black keys)
            const auto labelArea = bounds.withTrimmedTop(bounds.getHeight() * 0.70f).withTrimmedBottom(4.0f);
            g.setFont(theme_ ? theme_->getLabelFont(11.0f).boldened() : juce::Font(juce::FontOptions{11.0f}.withStyle("Bold")));
            g.setColour(active ? juce::Colour(0xff451a03) : juce::Colour(0xff1e293b));
            g.drawText(noteName_, labelArea, juce::Justification::centred, false);
        }
    }

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
    DroneHubPanel(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc),
          theme_(theme),
          droneLevel_("LEVEL", "%", theme, false),
          droneTone_("WARMTH", "%", theme, true),
          droneGlide_("GLIDE", "s", theme, false) {
        
        auto& apvts = proc_.getAPVTS();

        // 1. Master Pad Hold Toggle Button
        droneHoldBtn_.setButtonText("DRONE PAD HOLD");
        droneHoldBtn_.setClickingTogglesState(true);
        droneHoldBtn_.getProperties().set("warmAccent", true);
        droneHoldAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "droneEnabled", droneHoldBtn_);
        addAndMakeVisible(droneHoldBtn_);

        // 2. Space Freeze Button
        freezeBtn_.setButtonText("FREEZE SPACE");
        freezeBtn_.setClickingTogglesState(true);
        freezeBtn_.getProperties().set("warmAccent", true);
        freezeAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "reverbFreeze", freezeBtn_);
        addAndMakeVisible(freezeBtn_);

        // 3. Knobs
        droneLevel_.attachToParameter(apvts, "droneLevel");
        droneTone_.attachToParameter(apvts,  "droneTone");
        droneGlide_.attachToParameter(apvts, "droneGlide");
        addAndMakeVisible(droneLevel_);
        addAndMakeVisible(droneTone_);
        addAndMakeVisible(droneGlide_);

        // 4. 12-Key Chromatic Selector (Authentic Piano Keybed)
        static constexpr std::array<const char*, 12> kKeyNames = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        static constexpr std::array<bool, 12> kIsBlack = {
            false, true, false, true, false, false, true, false, true, false, true, false
        };
        for (int k = 0; k < 12; ++k) {
            keyBtns_[static_cast<std::size_t>(k)].setup(
                kKeyNames[static_cast<std::size_t>(k)],
                kIsBlack[static_cast<std::size_t>(k)],
                &theme_);
            keyBtns_[static_cast<std::size_t>(k)].onClick = [this, k]() {
                if (auto* p = proc_.getAPVTS().getParameter("droneRootKey")) {
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(k)));
                    repaint();
                }
            };
            addAndMakeVisible(keyBtns_[static_cast<std::size_t>(k)]);
        }

        // 5. Voicing Mode Buttons
        static constexpr std::array<const char*, 4> kVoicingLabels = {
            "LUSH 5TH", "DEEP SUB", "AMBIENT 9TH", "CELESTIAL 8VE"
        };
        for (int v = 0; v < 4; ++v) {
            voicingBtns_[static_cast<std::size_t>(v)].setButtonText(kVoicingLabels[static_cast<std::size_t>(v)]);
            voicingBtns_[static_cast<std::size_t>(v)].setClickingTogglesState(false);
            voicingBtns_[static_cast<std::size_t>(v)].onClick = [this, v]() {
                if (auto* p = proc_.getAPVTS().getParameter("droneVoicing")) {
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(v)));
                    repaint();
                }
            };
            addAndMakeVisible(voicingBtns_[static_cast<std::size_t>(v)]);
        }

        // 6. Octave Steppers
        octDownBtn_.setButtonText("-");
        octDownBtn_.setTooltip("Transpose Drone Down One Octave (-2 to +2)");
        octDownBtn_.onClick = [this]() {
            if (auto* p = proc_.getAPVTS().getParameter("droneOctave")) {
                const int cur = std::clamp(static_cast<int>(p->convertFrom0to1(p->getValue())), -2, 2);
                if (cur > -2) {
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(cur - 1)));
                    repaint();
                }
            }
        };
        addAndMakeVisible(octDownBtn_);

        octUpBtn_.setButtonText("+");
        octUpBtn_.setTooltip("Transpose Drone Up One Octave (-2 to +2)");
        octUpBtn_.onClick = [this]() {
            if (auto* p = proc_.getAPVTS().getParameter("droneOctave")) {
                const int cur = std::clamp(static_cast<int>(p->convertFrom0to1(p->getValue())), -2, 2);
                if (cur < 2) {
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(cur + 1)));
                    repaint();
                }
            }
        };
        addAndMakeVisible(octUpBtn_);

        startTimerHz(30);
    }

    ~DroneHubPanel() override {
        stopTimer();
    }

    juce::String getTooltip() override {
        return "Continuous Worship Drone sound bed for seamless musical transitions and background ambient pad.";
    }

    void timerCallback() override {
        visualPhase_ += 0.045f;
        if (visualPhase_ > 628.3185f) visualPhase_ -= 628.3185f;
        repaint(cardAuraRect_);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(theme_.bgSurface.darker(0.18f));

        drawMasterCard(g, cardMasterRect_.toFloat());
        drawKeySelectorCard(g, cardKeysRect_.toFloat());
        drawAuraCard(g, cardAuraRect_.toFloat());
    }

    void resized() override {
        const auto area = getLocalBounds().reduced(8, 6);
        const int totalW = area.getWidth();
        const int totalH = area.getHeight();
        constexpr int gap = 8;
        const int availW = totalW - gap * 2;

        // Split: Master (30%), Key Selector (44%), Harmonic Aura (26%)
        const int wMaster = static_cast<int>(static_cast<float>(availW) * 0.30f);
        const int wKeys   = static_cast<int>(static_cast<float>(availW) * 0.44f);
        const int wAura   = availW - wMaster - wKeys;

        cardMasterRect_ = juce::Rectangle<int>(area.getX(), area.getY(), wMaster, totalH);
        cardKeysRect_   = juce::Rectangle<int>(cardMasterRect_.getRight() + gap, area.getY(), wKeys, totalH);
        cardAuraRect_   = juce::Rectangle<int>(cardKeysRect_.getRight() + gap, area.getY(), wAura, totalH);

        // Layout Master Controls
        auto mArea = cardMasterRect_.withTrimmedTop(28).reduced(8, 6);
        auto topBtns = mArea.removeFromTop(28);
        const int halfBtnW = (topBtns.getWidth() - 6) / 2;
        droneHoldBtn_.setBounds(topBtns.removeFromLeft(halfBtnW));
        freezeBtn_.setBounds(topBtns.removeFromRight(halfBtnW));

        mArea.removeFromTop(8);
        const int knobW = (mArea.getWidth() - 8) / 3;
        droneLevel_.setBounds(mArea.removeFromLeft(knobW));
        mArea.removeFromLeft(4);
        droneTone_.setBounds(mArea.removeFromLeft(knobW));
        mArea.removeFromLeft(4);
        droneGlide_.setBounds(mArea);

        // Layout Key Selector & Voicings
        auto kArea = cardKeysRect_.withTrimmedTop(28).reduced(8, 6);

        // Voicing Selector at top of keys card
        auto vRow = kArea.removeFromTop(24);
        const int vBtnW = (vRow.getWidth() - 9) / 4;
        for (int v = 0; v < 4; ++v) {
            voicingBtns_[static_cast<std::size_t>(v)].setBounds(vRow.removeFromLeft(vBtnW));
            vRow.removeFromLeft(3);
        }

        kArea.removeFromTop(10);
        // Piano Keys: 7 white keys at bottom with 1px slits, 5 black keys raised on top with 3D shadows
        pianoArea_ = kArea;
        const int pianoW = pianoArea_.getWidth();
        const int whiteW = pianoW / 7;
        const int whiteH = pianoArea_.getHeight() - 3; // 3px for red velvet felt strip at keybed rail
        const int blackW = static_cast<int>(static_cast<float>(whiteW) * 0.64f);
        const int blackH = static_cast<int>(static_cast<float>(whiteH) * 0.60f);

        // White keys: C(0), D(2), E(4), F(5), G(7), A(9), B(11)
        static constexpr std::array<int, 7> kWhites = { 0, 2, 4, 5, 7, 9, 11 };
        for (int i = 0; i < 7; ++i) {
            const int keyIdx = kWhites[static_cast<std::size_t>(i)];
            const int kx = pianoArea_.getX() + i * whiteW;
            const int kw = (i == 6) ? (pianoArea_.getRight() - kx) : (whiteW - 1);
            keyBtns_[static_cast<std::size_t>(keyIdx)].setBounds(
                kx, pianoArea_.getY() + 3, kw, whiteH);
        }

        // Black keys: C#(1), D#(3), F#(6), G#(8), A#(10)
        // Positioned perfectly over the 1px seams between white keys
        static constexpr std::array<int, 5> kBlacks = { 1, 3, 6, 8, 10 };
        static constexpr std::array<int, 5> kBlackWhiteSlots = { 0, 1, 3, 4, 5 };
        for (int i = 0; i < 5; ++i) {
            const int keyIdx = kBlacks[static_cast<std::size_t>(i)];
            const int slot = kBlackWhiteSlots[static_cast<std::size_t>(i)];
            const int seamX = pianoArea_.getX() + (slot + 1) * whiteW - 1;
            keyBtns_[static_cast<std::size_t>(keyIdx)].setBounds(
                seamX - blackW / 2, pianoArea_.getY() + 3, blackW, blackH);
            keyBtns_[static_cast<std::size_t>(keyIdx)].toFront(true);
        }

        // Octave Stepper buttons in Header
        const int octY = cardKeysRect_.getY() + 5;
        const int octRight = cardKeysRect_.getRight() - 8;
        octUpBtn_.setBounds(octRight - 18, octY, 18, 18);
        octDownBtn_.setBounds(octRight - 18 - 44 - 18, octY, 18, 18);
    }

private:
    void drawMasterCard(juce::Graphics& g, juce::Rectangle<float> area) {
        g.setColour(theme_.bgSurface.darker(0.12f));
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(area, 6.0f, 1.0f);

        // Header Title
        g.setFont(theme_.getSectionFont(12.0f));
        g.setColour(juce::Colour(0xfff59e0b)); // Warm Amber
        g.drawText("WORSHIP DRONE BED", juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 160.0f, 16.0f),
                   juce::Justification::left);

        // Subtitle badge
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(theme_.textMuted);
        g.drawText("INFINITE TONAL PAD", juce::Rectangle<float>(area.getRight() - 110.0f, area.getY() + 7.0f, 100.0f, 14.0f),
                   juce::Justification::right);
    }

    void drawKeySelectorCard(juce::Graphics& g, juce::Rectangle<float> area) {
        g.setColour(theme_.bgSurface.darker(0.12f));
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(area, 6.0f, 1.0f);

        // Header Title
        g.setFont(theme_.getSectionFont(12.0f));
        g.setColour(theme_.accentPrimary);
        g.drawText("ROOT KEY & HARMONIC VOICING", juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 200.0f, 16.0f),
                   juce::Justification::left);

        // Current Active Key
        int curKey = 0;
        if (auto* p = proc_.getAPVTS().getRawParameterValue("droneRootKey")) {
            curKey = std::clamp(static_cast<int>(p->load()), 0, 11);
        }

        // Current Octave display between stepper buttons
        int curOct = 0;
        if (auto* p = proc_.getAPVTS().getRawParameterValue("droneOctave")) {
            curOct = std::clamp(static_cast<int>(p->load()), -2, 2);
        }
        const juce::String octStr = "OCT " + (curOct > 0 ? "+" + juce::String(curOct) : juce::String(curOct));
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(curOct != 0 ? juce::Colour(0xfff59e0b) : theme_.textSecondary);
        g.drawText(octStr, juce::Rectangle<float>(area.getRight() - 68.0f, area.getY() + 6.0f, 42.0f, 16.0f),
                   juce::Justification::centred);

        // Highlight active Key button state
        for (int k = 0; k < 12; ++k) {
            keyBtns_[static_cast<std::size_t>(k)].setToggleState(k == curKey, juce::dontSendNotification);
        }

        // Highlight active Voicing button
        int curVoicing = 0;
        if (auto* pV = proc_.getAPVTS().getRawParameterValue("droneVoicing")) {
            curVoicing = std::clamp(static_cast<int>(pV->load()), 0, 3);
        }
        for (int v = 0; v < 4; ++v) {
            voicingBtns_[static_cast<std::size_t>(v)].setToggleState(v == curVoicing, juce::dontSendNotification);
        }

        // Red velvet acoustic felt strip along the top rail of piano keys
        if (!pianoArea_.isEmpty()) {
            g.setColour(juce::Colour(0xff881337)); // Deep burgundy felt
            g.fillRect(pianoArea_.getX(), pianoArea_.getY(), pianoArea_.getWidth(), 3);
            g.setColour(juce::Colours::black.withAlpha(0.60f));
            g.drawHorizontalLine(pianoArea_.getY(), static_cast<float>(pianoArea_.getX()), static_cast<float>(pianoArea_.getRight()));
        }
    }

    void drawAuraCard(juce::Graphics& g, juce::Rectangle<float> area) {
        g.setColour(theme_.bgSurface.darker(0.12f));
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(area, 6.0f, 1.0f);

        // Header Title
        g.setFont(theme_.getSectionFont(12.0f));
        g.setColour(juce::Colour(0xfff59e0b));
        g.drawText("HARMONIC AURA", juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 120.0f, 16.0f),
                   juce::Justification::left);

        // Subtitle badge
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(theme_.textMuted);
        g.drawText("RESONANCE", juce::Rectangle<float>(area.getRight() - 80.0f, area.getY() + 7.0f, 70.0f, 14.0f),
                   juce::Justification::right);

        // Real-time Glowing Harmonic Resonance Aura
        const auto plotArea = area.withTrimmedTop(28.0f).reduced(8.0f, 6.0f);
        g.setColour(juce::Colour(0xff090d16));
        g.fillRoundedRectangle(plotArea, 4.0f);
        g.setColour(theme_.borderSubtle.withAlpha(0.40f));
        g.drawRoundedRectangle(plotArea, 4.0f, 1.0f);

        const float cx = plotArea.getCentreX();
        const float cy = plotArea.getCentreY();
        const float maxR = std::min(plotArea.getWidth(), plotArea.getHeight()) * 0.45f;

        bool isDroneActive = false;
        if (auto* p = proc_.getAPVTS().getRawParameterValue("droneEnabled")) {
            isDroneActive = p->load() > 0.5f;
        }
        float droneLevel = 0.5f;
        if (auto* pL = proc_.getAPVTS().getRawParameterValue("droneLevel")) {
            droneLevel = pL->load();
        }

        const float intensity = isDroneActive ? (0.25f + 0.75f * std::clamp(droneLevel, 0.0f, 1.0f)) : 0.10f;
        const auto auraCol = isDroneActive ? juce::Colour(0xfff59e0b) : theme_.textMuted;

        // Draw concentric glowing harmonic rings
        for (int ring = 1; ring <= 4; ++ring) {
            const float rBase = maxR * (static_cast<float>(ring) / 4.0f);
            const float modulation = std::sin(visualPhase_ * 1.5f + static_cast<float>(ring) * 0.8f) * 3.5f * intensity;
            const float r = std::max(2.0f, rBase + modulation);

            juce::Path ringP;
            ringP.addCentredArc(cx, cy, r, r, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);

            const float ringAlpha = (1.0f - static_cast<float>(ring) * 0.18f) * intensity;
            g.setColour(auraCol.withAlpha(ringAlpha * 0.35f));
            g.strokePath(ringP, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(auraCol.withAlpha(ringAlpha * 0.85f));
            g.strokePath(ringP, juce::PathStrokeType(1.2f));
        }

        // Center Pulsing Core Dot
        const float coreR = 4.0f + 3.0f * intensity * std::abs(std::sin(visualPhase_ * 2.0f));
        g.setColour(auraCol.withAlpha(intensity * 0.40f));
        g.fillEllipse(cx - coreR - 3.0f, cy - coreR - 3.0f, (coreR + 3.0f) * 2.0f, (coreR + 3.0f) * 2.0f);
        g.setColour(isDroneActive ? juce::Colours::white : theme_.textMuted);
        g.fillEllipse(cx - coreR, cy - coreR, coreR * 2.0f, coreR * 2.0f);
    }

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
