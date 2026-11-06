#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <nova_ui/NovaTheme.h>
#include <nova_ui/ArcKnob.h>
#include "HorizontalViewport.h"
#include "../NovaSynthProcessor.h"

namespace nova::synth::ui {

/**
 * @brief Dedicated Worship Arpeggiator & 16/32/64-Step Rhythm Sequencer Hub Panel (Tab 4)
 *
 * Provides comprehensive control over the polyphonic arpeggiator:
 * 1. Core Engine: Arp Enable, Latch/Hold, Rate with Tempo Sync / Free Hz, Gate length, and Swing.
 * 2. Pattern Logic: 6 directional modes (Up, Down, Up/Down, Down/Up, Random, Chord Pulse), Octave Range (1..4).
 * 3. Step Matrix: Per-step Gate active toggles and Velocity sliders (16, 32, or 64 steps) with horizontal scrolling.
 * 4. Real-time Animated Playhead tracking active playback position.
 */
class ArpeggiatorHubPanel : public juce::Component, public juce::TooltipClient, private juce::Timer {
public:
    class StepMatrixComponent : public juce::Component {
    public:
        StepMatrixComponent(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
            : proc_(proc), theme_(theme) {}

        void paint(juce::Graphics& g) override {
            const int numSteps = proc_.getArpPatternLength();
            const int playhead = proc_.getEngine().getArpCurrentStep();

            for (int s = 0; s < numSteps && s < static_cast<int>(stepRects_.size()); ++s) {
                const auto sr = stepRects_[static_cast<std::size_t>(s)].toFloat();
                const bool isPlayhead = (s == playhead);
                const bool isGate = proc_.isArpStepGateActive(s);
                const float vel = proc_.getArpStepVelocity(s);

                // Step lane background
                g.setColour(isPlayhead ? juce::Colour(0xff38bdf8).withAlpha(0.18f) : juce::Colour(0xff090d16));
                g.fillRoundedRectangle(sr, 3.0f);
                g.setColour(isPlayhead ? juce::Colour(0xff38bdf8) : theme_.borderSubtle.withAlpha(0.35f));
                g.drawRoundedRectangle(sr, 3.0f, 1.0f);

                // Top Gate button indicator (height 18px)
                const auto gateRect = sr.withHeight(18.0f).reduced(2.0f);
                if (isGate) {
                    g.setColour(isPlayhead ? juce::Colours::white : juce::Colour(0xff2dd4bf));
                    g.fillRoundedRectangle(gateRect, 2.0f);
                } else {
                    g.setColour(juce::Colour(0xff1e293b));
                    g.fillRoundedRectangle(gateRect, 2.0f);
                }
                g.setFont(theme_.getLabelFont(9.0f));
                g.setColour(isGate ? juce::Colour(0xff090d16) : theme_.textMuted);
                g.drawText(juce::String(s + 1), gateRect, juce::Justification::centred);

                // Velocity bar
                if (isGate) {
                    const auto velTrack = sr.withTrimmedTop(22.0f).reduced(3.0f, 2.0f);
                    const float barH = velTrack.getHeight() * vel;
                    const auto fillBar = velTrack.withTrimmedTop(velTrack.getHeight() - barH);

                    const auto barCol = isPlayhead ? juce::Colour(0xff38bdf8) : juce::Colour(0xff2dd4bf);
                    g.setColour(barCol.withAlpha(0.75f));
                    g.fillRoundedRectangle(fillBar, 2.0f);
                }
            }
        }

        void mouseDown(const juce::MouseEvent& event) override {
            handleMouse(event.getPosition(), true);
        }

        void mouseDrag(const juce::MouseEvent& event) override {
            handleMouse(event.getPosition(), false);
        }

        void updateStepRects(int numSteps, int stepW, int totalH) {
            stepRects_.clear();
            stepRects_.reserve(static_cast<std::size_t>(numSteps));
            for (int s = 0; s < numSteps; ++s) {
                stepRects_.push_back(juce::Rectangle<int>(s * (stepW + 3), 0, stepW, totalH));
            }
            repaint();
        }

    private:
        void handleMouse(juce::Point<int> pos, bool isClick) {
            const int numSteps = proc_.getArpPatternLength();
            for (int s = 0; s < numSteps && s < static_cast<int>(stepRects_.size()); ++s) {
                const auto sr = stepRects_[static_cast<std::size_t>(s)];
                if (sr.contains(pos)) {
                    if (isClick && pos.y < sr.getY() + 20) {
                        proc_.toggleArpStepGate(s);
                    } else {
                        const float trackH = static_cast<float>(sr.getHeight() - 24);
                        if (trackH > 1.0f) {
                            const float relY = static_cast<float>(sr.getBottom() - pos.y) / trackH;
                            proc_.setArpStepVelocity(s, std::clamp(relY, 0.05f, 1.0f));
                            proc_.setArpStepGate(s, true);
                        }
                    }
                    repaint();
                    return;
                }
            }
        }

        NovaSynthProcessor& proc_;
        const nova::ui::NovaTheme& theme_;
        std::vector<juce::Rectangle<int>> stepRects_;
    };

    ArpeggiatorHubPanel(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc),
          theme_(theme),
          arpRate_("RATE", "Hz", theme, false),
          arpGate_("GATE", "%", theme, false),
          arpSwing_("SWING", "%", theme, true),
          stepMatrixComp_(proc, theme) {

        auto& apvts = proc_.getAPVTS();

        // 1. Core Toggle Buttons
        arpEnableBtn_.setButtonText("ARPEGGIATOR ACTIVE");
        arpEnableBtn_.setClickingTogglesState(true);
        arpEnableAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "arpEnabled", arpEnableBtn_);
        addAndMakeVisible(arpEnableBtn_);

        arpLatchBtn_.setButtonText("LATCH / HOLD");
        arpLatchBtn_.setClickingTogglesState(true);
        arpLatchAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "arpLatch", arpLatchBtn_);
        addAndMakeVisible(arpLatchBtn_);

        // 2. Knobs
        setupRateKnob();
        arpGate_.attachToParameter(apvts, "arpGate");
        arpSwing_.attachToParameter(apvts, "arpSwing");
        addAndMakeVisible(arpRate_);
        addAndMakeVisible(arpGate_);
        addAndMakeVisible(arpSwing_);

        // 3. Directional Mode Buttons
        static constexpr std::array<const char*, 6> kModeNames = {
            "UP", "DOWN", "UP / DOWN", "DOWN / UP", "RANDOM", "CHORD PULSE"
        };
        for (int m = 0; m < 6; ++m) {
            modeBtns_[static_cast<std::size_t>(m)].setButtonText(kModeNames[static_cast<std::size_t>(m)]);
            modeBtns_[static_cast<std::size_t>(m)].onClick = [this, m]() {
                if (auto* p = proc_.getAPVTS().getParameter("arpMode")) {
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(m)));
                    repaint();
                }
            };
            addAndMakeVisible(modeBtns_[static_cast<std::size_t>(m)]);
        }

        // 4. Octave Range Buttons
        static constexpr std::array<const char*, 4> kOctaveNames = {
            "1 OCT", "2 OCT", "3 OCT", "4 OCT"
        };
        for (int o = 0; o < 4; ++o) {
            octaveBtns_[static_cast<std::size_t>(o)].setButtonText(kOctaveNames[static_cast<std::size_t>(o)]);
            octaveBtns_[static_cast<std::size_t>(o)].onClick = [this, o]() {
                if (auto* p = proc_.getAPVTS().getParameter("arpOctaves")) {
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(o)));
                    repaint();
                }
            };
            addAndMakeVisible(octaveBtns_[static_cast<std::size_t>(o)]);
        }

        // 5. Pattern Length & Rhythm Preset Dropdown (16, 32, 64 steps)
        rhythmPresetsBtn_.setButtonText("RHYTHM TEMPLATES ▾");
        rhythmPresetsBtn_.setTooltip("Choose a musical rhythm matrix preset for 16, 32 or 64 steps");
        rhythmPresetsBtn_.onClick = [this]() { showRhythmPresetsMenu(); };
        addAndMakeVisible(rhythmPresetsBtn_);

        len16Btn_.setButtonText("16");
        len32Btn_.setButtonText("32");
        len64Btn_.setButtonText("64");

        len16Btn_.onClick = [this]() { proc_.setArpPatternLength(16); updateMatrixLayout(); };
        len32Btn_.onClick = [this]() { proc_.setArpPatternLength(32); updateMatrixLayout(); };
        len64Btn_.onClick = [this]() { proc_.setArpPatternLength(64); updateMatrixLayout(); };

        addAndMakeVisible(len16Btn_);
        addAndMakeVisible(len32Btn_);
        addAndMakeVisible(len64Btn_);

        // 6. Viewport with inner Step Matrix component
        matrixViewport_.setViewedComponent(&stepMatrixComp_, false);
        addAndMakeVisible(matrixViewport_);

        startTimerHz(30);
    }

    ~ArpeggiatorHubPanel() override {
        stopTimer();
    }

    juce::String getTooltip() override {
        return "Rhythmic Worship Arpeggiator & Step Sequencer (16/32/64 steps). Use Dotted 8th for signature ambient plucks.";
    }

    void timerCallback() override {
        stepMatrixComp_.repaint();
        repaint(cardMatrixRect_);
    }

    void setupRateKnob() {
        auto& apvts = proc_.getAPVTS();
        bool isSynced = true;
        if (auto* p = apvts.getRawParameterValue("arpSync")) {
            isSynced = p->load() > 0.5f;
        }

        arpRateAtt_.reset();
        if (isSynced) {
            arpRate_.setLabel("RATE (SYNC)");
            arpRate_.attachToParameter(apvts, "arpSyncDiv");
        } else {
            arpRate_.setLabel("RATE (HZ)");
            arpRate_.attachToParameter(apvts, "arpRateHz");
        }
    }

    void toggleArpSync() {
        if (auto* p = proc_.getAPVTS().getParameter("arpSync")) {
            const float cur = p->getValue();
            p->setValueNotifyingHost(cur > 0.5f ? 0.0f : 1.0f);
            setupRateKnob();
            repaint(cardCoreRect_);
        }
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(theme_.bgSurface.darker(0.18f));

        drawCoreCard(g, cardCoreRect_.toFloat());
        drawLogicCard(g, cardLogicRect_.toFloat());
        drawMatrixCard(g, cardMatrixRect_.toFloat());
    }

    void updateMatrixLayout() {
        const int numSteps = proc_.getArpPatternLength();
        const auto mArea = cardMatrixRect_.withTrimmedTop(28).reduced(8, 6);
        const int visibleW = mArea.getWidth();
        const int visibleH = mArea.getHeight();

        // 16 steps: bars fill the visible area
        const int stepW_16 = std::max(20, (visibleW - 15 * 3) / 16);

        int stepW = stepW_16;
        int contentW = visibleW;

        if (numSteps > 16) {
            // At 32 or 64 steps: bars are 1/3 narrower (i.e. 2/3 of 16-step bar width)
            stepW = std::max(16, static_cast<int>(std::round(static_cast<float>(stepW_16) * 2.0f / 3.0f)));
            contentW = numSteps * (stepW + 3) - 3;
        }

        stepMatrixComp_.updateStepRects(numSteps, stepW, visibleH);
        stepMatrixComp_.setBounds(0, 0, contentW, visibleH);
        matrixViewport_.setBounds(mArea);
        if (numSteps == 16) {
            matrixViewport_.setViewPosition(0, 0);
        }
        repaint(cardMatrixRect_);
    }

    void resized() override {
        const auto area = getLocalBounds().reduced(8, 6);
        const int totalW = area.getWidth();
        const int totalH = area.getHeight();
        constexpr int gap = 8;
        const int availW = totalW - gap * 2;

        // Width split: Core (27%), Pattern Logic narrowed to 19%, Matrix extended to 54%!
        const int wCore   = static_cast<int>(static_cast<float>(availW) * 0.27f);
        const int wLogic  = static_cast<int>(static_cast<float>(availW) * 0.19f);
        const int wMatrix = availW - wCore - wLogic;

        cardCoreRect_   = juce::Rectangle<int>(area.getX(), area.getY(), wCore, totalH);
        cardLogicRect_  = juce::Rectangle<int>(cardCoreRect_.getRight() + gap, area.getY(), wLogic, totalH);
        cardMatrixRect_ = juce::Rectangle<int>(cardLogicRect_.getRight() + gap, area.getY(), wMatrix, totalH);

        // Layout Core Controls
        auto cArea = cardCoreRect_.withTrimmedTop(28).reduced(8, 6);
        auto topRow = cArea.removeFromTop(28);
        const int halfW = (topRow.getWidth() - 6) / 2;
        arpEnableBtn_.setBounds(topRow.removeFromLeft(halfW));
        arpLatchBtn_.setBounds(topRow.removeFromRight(halfW));

        cArea.removeFromTop(8);
        const int knobW = (cArea.getWidth() - 8) / 3;
        const int knobH = cArea.getHeight();
        const int knobY = cArea.getY();

        const int rateX  = cArea.getX();
        const int gateX  = rateX + knobW + 4;
        const int swingX = gateX + knobW + 4;

        arpRate_.setBounds(rateX, knobY, knobW, knobH);
        arpGate_.setBounds(gateX, knobY, knobW, knobH);
        arpSwing_.setBounds(swingX, knobY, knobW, knobH);

        // Sync button position between Rate dial and Gate
        const float dialCenterY = static_cast<float>(knobY) + 12.0f + static_cast<float>(knobH - 26) * 0.5f;
        constexpr int btnSize = 16;
        const int btnX = rateX + knobW - 6;
        const int btnY = static_cast<int>(std::round(dialCenterY - static_cast<float>(btnSize) * 0.5f));
        syncBtnRect_ = juce::Rectangle<int>(btnX, btnY, btnSize, btnSize);

        // Layout Logic Controls
        auto lArea = cardLogicRect_.withTrimmedTop(28).reduced(8, 6);

        // Mode buttons: 3 rows of 2
        const int modeRowH = 22;
        for (int r = 0; r < 3; ++r) {
            auto row = lArea.removeFromTop(modeRowH);
            const int btnW = (row.getWidth() - 4) / 2;
            modeBtns_[static_cast<std::size_t>(r * 2)].setBounds(row.removeFromLeft(btnW));
            modeBtns_[static_cast<std::size_t>(r * 2 + 1)].setBounds(row.removeFromRight(btnW));
            lArea.removeFromTop(4);
        }

        lArea.removeFromTop(6);
        // Octave range row
        auto octRow = lArea.removeFromTop(22);
        const int octW = (octRow.getWidth() - 9) / 4;
        for (int o = 0; o < 4; ++o) {
            octaveBtns_[static_cast<std::size_t>(o)].setBounds(octRow.removeFromLeft(octW));
            octRow.removeFromLeft(3);
        }

        // Header length buttons in cardMatrixRect_
        const int lenBtnW = 30;
        const int lenBtnH = 17;
        const int lenBtnY = cardMatrixRect_.getY() + 5;
        const int rightX = cardMatrixRect_.getRight() - 10;
        len64Btn_.setBounds(rightX - lenBtnW, lenBtnY, lenBtnW, lenBtnH);
        len32Btn_.setBounds(rightX - lenBtnW * 2 - 3, lenBtnY, lenBtnW, lenBtnH);
        len16Btn_.setBounds(rightX - lenBtnW * 3 - 6, lenBtnY, lenBtnW, lenBtnH);

        const int stepsLabelLeft = len16Btn_.getX() - 52;
        const int presetBtnW = std::clamp(stepsLabelLeft - (cardMatrixRect_.getX() + 175), 115, 155);
        const int presetBtnX = stepsLabelLeft - presetBtnW - 8;
        rhythmPresetsBtn_.setBounds(presetBtnX, lenBtnY, presetBtnW, lenBtnH + 1);

        updateMatrixLayout();
    }

    void mouseMove(const juce::MouseEvent& event) override {
        const auto pos = event.getPosition();
        const bool hoverSync = syncBtnRect_.contains(pos);
        if (hoverSync != isSyncHovered_) {
            isSyncHovered_ = hoverSync;
            setMouseCursor(isSyncHovered_ ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint(cardCoreRect_);
        }
    }

    void mouseExit(const juce::MouseEvent&) override {
        if (isSyncHovered_) {
            isSyncHovered_ = false;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint(cardCoreRect_);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override {
        const auto pos = event.getPosition();
        if (syncBtnRect_.contains(pos)) {
            toggleArpSync();
        }
    }

private:
    void drawCoreCard(juce::Graphics& g, juce::Rectangle<float> area) {
        g.setColour(theme_.bgSurface.darker(0.12f));
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(area, 6.0f, 1.0f);

        // Header Title
        g.setFont(theme_.getSectionFont(12.0f));
        g.setColour(theme_.accentPrimary);
        g.drawText("ARPEGGIATOR CORE", juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 160.0f, 16.0f),
                   juce::Justification::left);

        // Subtitle badge
        bool isArpOn = false;
        if (auto* p = proc_.getAPVTS().getRawParameterValue("arpEnabled")) {
            isArpOn = p->load() > 0.5f;
        }
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(isArpOn ? theme_.accentPrimary : theme_.textMuted);
        g.drawText(isArpOn ? "ACTIVE" : "STANDBY", juce::Rectangle<float>(area.getRight() - 80.0f, area.getY() + 7.0f, 70.0f, 14.0f),
                   juce::Justification::right);

        // Draw Sync button between Rate knob and Gate
        bool isSynced = false;
        if (auto* pS = proc_.getAPVTS().getRawParameterValue("arpSync")) {
            isSynced = pS->load() > 0.5f;
        }

        // Draw connecting line from Rate knob rim to Sync button
        const float lineY = static_cast<float>(syncBtnRect_.getCentreY());
        const float lineStartX = static_cast<float>(arpRate_.getX()) + static_cast<float>(arpRate_.getWidth()) * 0.5f + 16.0f;
        const float lineEndX = static_cast<float>(syncBtnRect_.getX());
        if (lineEndX > lineStartX) {
            g.setColour(isSynced ? theme_.accentPrimary.withAlpha(0.95f) : juce::Colour(0xff475569));
            g.drawLine(lineStartX, lineY, lineEndX, lineY, isSynced ? 1.5f : 1.2f);
        }

        drawSyncButton(g, syncBtnRect_.toFloat(), isSynced, theme_.accentPrimary, isSyncHovered_);
    }

    void drawSyncButton(juce::Graphics& g, juce::Rectangle<float> btnRect, bool isSynced, juce::Colour col, bool isHovered) {
        constexpr float radius = 3.5f;
        if (isSynced) {
            g.setColour(col.withAlpha(0.22f));
            g.fillRoundedRectangle(btnRect.expanded(1.5f), radius + 1.0f);
            const auto grad = juce::ColourGradient::vertical(
                col.withAlpha(0.38f), btnRect.getY(),
                col.withAlpha(0.20f), btnRect.getBottom());
            g.setGradientFill(grad);
            g.fillRoundedRectangle(btnRect, radius);
            g.setColour(col.withAlpha(0.95f));
            g.drawRoundedRectangle(btnRect, radius, 1.3f);
        } else {
            const auto grad = juce::ColourGradient::vertical(
                isHovered ? juce::Colour(0xff273549) : juce::Colour(0xff1e293b), btnRect.getY(),
                isHovered ? juce::Colour(0xff162030) : juce::Colour(0xff0f172a), btnRect.getBottom());
            g.setGradientFill(grad);
            g.fillRoundedRectangle(btnRect, radius);
            g.setColour(isHovered ? theme_.borderFocus : juce::Colour(0xff3b4f68));
            g.drawRoundedRectangle(btnRect, radius, 1.2f);
        }

        const auto iconCol = isSynced ? juce::Colours::white : (isHovered ? juce::Colour(0xfff1f5f9) : juce::Colour(0xffcbd5e1));
        drawChainIcon(g, btnRect.reduced(2.5f), iconCol);
    }

    static void drawChainIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour col) {
        const float s = std::min(area.getWidth(), area.getHeight());
        if (s <= 1.0f) return;

        const float cx = area.getCentreX();
        const float cy = area.getCentreY();

        const float loopCenterX = 0.26f * s;
        const float loopRadius  = 0.16f * s;
        const float legLen      = 0.12f * s;
        const float loopStroke  = std::max(1.3f, 0.13f * s);
        const float barHalfLen  = 0.19f * s;
        const float barStroke   = std::max(1.3f, 0.12f * s);

        const auto strokeType = juce::PathStrokeType(loopStroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        const auto barStrokeType = juce::PathStrokeType(barStroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        const auto xform = juce::AffineTransform::rotation(-juce::MathConstants<float>::pi * 0.25f)
                                                 .translated(cx, cy);

        // 1. Center link bar
        juce::Path bar;
        bar.startNewSubPath(-barHalfLen, 0.0f);
        bar.lineTo(barHalfLen, 0.0f);
        bar.applyTransform(xform);

        // 2. Right loop
        juce::Path rightLoop;
        rightLoop.startNewSubPath(loopCenterX - legLen, loopRadius);
        rightLoop.lineTo(loopCenterX, loopRadius);
        rightLoop.addCentredArc(loopCenterX, 0.0f, loopRadius, loopRadius,
                                0.0f,
                                juce::MathConstants<float>::pi,
                                0.0f,
                                false);
        rightLoop.lineTo(loopCenterX - legLen, -loopRadius);
        rightLoop.applyTransform(xform);

        // 3. Left loop
        juce::Path leftLoop;
        leftLoop.startNewSubPath(-loopCenterX + legLen, -loopRadius);
        leftLoop.lineTo(-loopCenterX, -loopRadius);
        leftLoop.addCentredArc(-loopCenterX, 0.0f, loopRadius, loopRadius,
                                0.0f,
                                0.0f,
                                -juce::MathConstants<float>::pi,
                                false);
        leftLoop.lineTo(-loopCenterX + legLen, loopRadius);
        leftLoop.applyTransform(xform);

        g.setColour(col);
        g.strokePath(bar, barStrokeType);
        g.strokePath(rightLoop, strokeType);
        g.strokePath(leftLoop, strokeType);
    }

    void drawLogicCard(juce::Graphics& g, juce::Rectangle<float> area) {
        g.setColour(theme_.bgSurface.darker(0.12f));
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(area, 6.0f, 1.0f);

        // Header Title
        g.setFont(theme_.getSectionFont(12.0f));
        g.setColour(theme_.accentPrimary);
        g.drawText("PATTERN LOGIC", juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 140.0f, 16.0f),
                   juce::Justification::left);

        // Highlight active Mode
        int curMode = 0;
        if (auto* p = proc_.getAPVTS().getRawParameterValue("arpMode")) {
            curMode = std::clamp(static_cast<int>(p->load()), 0, 5);
        }
        for (int m = 0; m < 6; ++m) {
            modeBtns_[static_cast<std::size_t>(m)].setToggleState(m == curMode, juce::dontSendNotification);
        }

        // Highlight active Octave
        int curOct = 0;
        if (auto* pO = proc_.getAPVTS().getRawParameterValue("arpOctaves")) {
            curOct = std::clamp(static_cast<int>(pO->load()), 0, 3);
        }
        for (int o = 0; o < 4; ++o) {
            octaveBtns_[static_cast<std::size_t>(o)].setToggleState(o == curOct, juce::dontSendNotification);
        }
    }

    void drawMatrixCard(juce::Graphics& g, juce::Rectangle<float> area) {
        g.setColour(theme_.bgSurface.darker(0.12f));
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(area, 6.0f, 1.0f);

        // Header Title
        const int numSteps = proc_.getArpPatternLength();
        g.setFont(theme_.getSectionFont(12.0f));
        g.setColour(juce::Colour(0xff38bdf8));
        g.drawText(juce::String(numSteps) + "-STEP RHYTHM MATRIX",
                   juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 165.0f, 16.0f),
                   juce::Justification::left);

        // Steps label before buttons
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(theme_.textMuted);
        g.drawText("STEPS:", juce::Rectangle<float>(static_cast<float>(len16Btn_.getX() - 48), area.getY() + 6.0f, 44.0f, 15.0f),
                   juce::Justification::right);

        // Highlight active Length button
        len16Btn_.setToggleState(numSteps == 16, juce::dontSendNotification);
        len32Btn_.setToggleState(numSteps == 32, juce::dontSendNotification);
        len64Btn_.setToggleState(numSteps == 64, juce::dontSendNotification);
    }

    struct RhythmPatternPreset {
        juce::String name;
        int length = 16;
        std::vector<bool> gates;
        std::vector<float> velocities;
    };

    static std::vector<RhythmPatternPreset> getAvailableRhythmPresets() {
        std::vector<RhythmPatternPreset> presets;

        // ---------------- 16 STEPS (1 Bar) ----------------
        // 1. Straight 16 (Full Drive)
        {
            RhythmPatternPreset p;
            p.name = "Straight 16 (Full Drive)";
            p.length = 16;
            p.gates.assign(16, true);
            p.velocities = { 1.0f, 0.70f, 0.85f, 0.70f,
                             0.95f, 0.70f, 0.85f, 0.70f,
                             1.0f,  0.70f, 0.85f, 0.70f,
                             0.95f, 0.70f, 0.85f, 0.75f };
            presets.push_back(std::move(p));
        }

        // 2. Dotted 8th Worship Pulse (Hillsong / Bethel)
        {
            RhythmPatternPreset p;
            p.name = "Dotted 8th Worship Pulse";
            p.length = 16;
            p.gates.assign(16, false);
            p.velocities.assign(16, 0.50f);
            const int hits[] = { 0, 3, 6, 9, 12, 15 };
            const float vels[] = { 1.0f, 0.85f, 0.90f, 0.85f, 0.95f, 0.80f };
            for (int i = 0; i < 6; ++i) {
                p.gates[static_cast<std::size_t>(hits[i])] = true;
                p.velocities[static_cast<std::size_t>(hits[i])] = vels[i];
            }
            presets.push_back(std::move(p));
        }

        // 3. Offbeat Syncopation (Gospel / Funk)
        {
            RhythmPatternPreset p;
            p.name = "Offbeat Syncopation";
            p.length = 16;
            p.gates.assign(16, false);
            p.velocities.assign(16, 0.50f);
            const int hits[] = { 0, 2, 5, 6, 8, 10, 13, 14 };
            const float vels[] = { 0.65f, 0.95f, 0.60f, 0.90f, 0.70f, 0.95f, 0.60f, 0.90f };
            for (int i = 0; i < 8; ++i) {
                p.gates[static_cast<std::size_t>(hits[i])] = true;
                p.velocities[static_cast<std::size_t>(hits[i])] = vels[i];
            }
            presets.push_back(std::move(p));
        }

        // 4. Four-on-the-Floor Groove
        {
            RhythmPatternPreset p;
            p.name = "Four-on-the-Floor Groove";
            p.length = 16;
            p.gates.assign(16, false);
            p.velocities.assign(16, 0.50f);
            const int hits[] = { 0, 2, 4, 6, 8, 10, 12, 14, 15 };
            const float vels[] = { 1.0f, 0.65f, 0.95f, 0.65f, 1.0f, 0.65f, 0.95f, 0.70f, 0.80f };
            for (int i = 0; i < 9; ++i) {
                p.gates[static_cast<std::size_t>(hits[i])] = true;
                p.velocities[static_cast<std::size_t>(hits[i])] = vels[i];
            }
            presets.push_back(std::move(p));
        }

        // 5. Euphoric Build
        {
            RhythmPatternPreset p;
            p.name = "Euphoric Build";
            p.length = 16;
            p.gates.assign(16, false);
            p.velocities.assign(16, 0.50f);
            const int hits[] = { 0, 4, 8, 10, 12, 13, 14, 15 };
            const float vels[] = { 0.45f, 0.55f, 0.65f, 0.75f, 0.85f, 0.90f, 0.95f, 1.0f };
            for (int i = 0; i < 8; ++i) {
                p.gates[static_cast<std::size_t>(hits[i])] = true;
                p.velocities[static_cast<std::size_t>(hits[i])] = vels[i];
            }
            presets.push_back(std::move(p));
        }

        // 6. Ambient Sparse Chime
        {
            RhythmPatternPreset p;
            p.name = "Ambient Sparse Chime";
            p.length = 16;
            p.gates.assign(16, false);
            p.velocities.assign(16, 0.50f);
            const int hits[] = { 0, 3, 8, 11 };
            const float vels[] = { 0.85f, 0.65f, 0.80f, 0.60f };
            for (int i = 0; i < 4; ++i) {
                p.gates[static_cast<std::size_t>(hits[i])] = true;
                p.velocities[static_cast<std::size_t>(hits[i])] = vels[i];
            }
            presets.push_back(std::move(p));
        }

        // ---------------- 32 STEPS (2 Bars) ----------------
        // 7. Bethel 2-Bar Anthem
        {
            RhythmPatternPreset p;
            p.name = "Bethel 2-Bar Anthem";
            p.length = 32;
            p.gates.assign(32, false);
            p.velocities.assign(32, 0.60f);
            const int b1[] = { 0, 3, 6, 9, 12, 15 };
            for (int h : b1) { p.gates[static_cast<std::size_t>(h)] = true; p.velocities[static_cast<std::size_t>(h)] = 0.90f; }
            const int b2[] = { 16, 18, 20, 22, 24, 26, 27, 28, 29, 30, 31 };
            const float b2v[] = { 0.95f, 0.70f, 0.90f, 0.70f, 1.0f, 0.80f, 0.85f, 0.90f, 0.95f, 0.98f, 1.0f };
            for (int i = 0; i < 11; ++i) {
                p.gates[static_cast<std::size_t>(b2[i])] = true;
                p.velocities[static_cast<std::size_t>(b2[i])] = b2v[i];
            }
            presets.push_back(std::move(p));
        }

        // 8. Call & Response 32
        {
            RhythmPatternPreset p;
            p.name = "Call & Response 32";
            p.length = 32;
            p.gates.assign(32, false);
            p.velocities.assign(32, 0.60f);
            const int hits[] = { 0, 2, 4, 6, 8, 10, 12, 14, 16, 20, 24, 28 };
            const float vels[] = { 0.95f, 0.70f, 0.90f, 0.70f, 0.95f, 0.70f, 0.85f, 0.65f, 1.0f, 0.80f, 0.90f, 0.75f };
            for (int i = 0; i < 12; ++i) {
                p.gates[static_cast<std::size_t>(hits[i])] = true;
                p.velocities[static_cast<std::size_t>(hits[i])] = vels[i];
            }
            presets.push_back(std::move(p));
        }

        // 9. Polyrhythmic 3+3+2 Flow
        {
            RhythmPatternPreset p;
            p.name = "Polyrhythmic 3+3+2 Flow";
            p.length = 32;
            p.gates.assign(32, false);
            p.velocities.assign(32, 0.50f);
            const int hits[] = { 0, 3, 6, 8, 11, 14, 16, 19, 22, 24, 27, 30 };
            for (int h : hits) {
                p.gates[static_cast<std::size_t>(h)] = true;
                p.velocities[static_cast<std::size_t>(h)] = (h % 8 == 0) ? 1.0f : 0.82f;
            }
            presets.push_back(std::move(p));
        }

        // 10. Progressive Velocity Swell (32)
        {
            RhythmPatternPreset p;
            p.name = "Progressive Velocity Swell";
            p.length = 32;
            p.gates.assign(32, true);
            p.velocities.resize(32);
            for (int s = 0; s < 32; ++s) {
                p.velocities[static_cast<std::size_t>(s)] = 0.35f + 0.65f * (static_cast<float>(s) / 31.0f);
            }
            presets.push_back(std::move(p));
        }

        // ---------------- 64 STEPS (4 Bars) ----------------
        // 11. 4-Bar Worship Journey
        {
            RhythmPatternPreset p;
            p.name = "4-Bar Worship Journey";
            p.length = 64;
            p.gates.assign(64, false);
            p.velocities.assign(64, 0.60f);
            for (int h : { 0, 4, 8, 12 }) { p.gates[static_cast<std::size_t>(h)] = true; p.velocities[static_cast<std::size_t>(h)] = 0.75f; }
            for (int h : { 16, 19, 22, 25, 28, 31 }) { p.gates[static_cast<std::size_t>(h)] = true; p.velocities[static_cast<std::size_t>(h)] = 0.85f; }
            for (int h : { 32, 34, 36, 38, 40, 42, 44, 46, 47 }) { p.gates[static_cast<std::size_t>(h)] = true; p.velocities[static_cast<std::size_t>(h)] = 0.90f; }
            const int b4[] = { 48, 50, 52, 54, 56, 58, 59, 60, 61, 62, 63 };
            const float b4v[] = { 1.0f, 0.80f, 0.95f, 0.80f, 1.0f, 0.85f, 0.90f, 0.92f, 0.95f, 0.98f, 1.0f };
            for (int i = 0; i < 11; ++i) {
                p.gates[static_cast<std::size_t>(b4[i])] = true;
                p.velocities[static_cast<std::size_t>(b4[i])] = b4v[i];
            }
            presets.push_back(std::move(p));
        }

        // 12. Ambient Texture Wave
        {
            RhythmPatternPreset p;
            p.name = "Ambient Texture Wave";
            p.length = 64;
            p.gates.assign(64, false);
            p.velocities.resize(64);
            for (int s = 0; s < 64; ++s) {
                const float wave = 0.5f + 0.5f * std::sin(static_cast<float>(s) * 0.3927f);
                p.gates[static_cast<std::size_t>(s)] = (s % 4 != 3);
                p.velocities[static_cast<std::size_t>(s)] = 0.40f + 0.55f * wave;
            }
            presets.push_back(std::move(p));
        }

        // 13. Epic Cinematic Ostinato
        {
            RhythmPatternPreset p;
            p.name = "Epic Cinematic Ostinato";
            p.length = 64;
            p.gates.assign(64, true);
            p.velocities.resize(64);
            for (int s = 0; s < 64; ++s) {
                if (s % 16 == 0) p.velocities[static_cast<std::size_t>(s)] = 1.0f;
                else if (s % 4 == 0) p.velocities[static_cast<std::size_t>(s)] = 0.85f;
                else if (s % 2 == 0) p.velocities[static_cast<std::size_t>(s)] = 0.70f;
                else p.velocities[static_cast<std::size_t>(s)] = 0.55f;
            }
            presets.push_back(std::move(p));
        }

        return presets;
    }

    void showRhythmPresetsMenu() {
        juce::PopupMenu m;
        const auto presets = getAvailableRhythmPresets();

        juce::PopupMenu m16;
        for (std::size_t i = 0; i < 6; ++i) {
            m16.addItem(static_cast<int>(i + 1), presets[i].name);
        }
        m.addSubMenu("16 Steps (1 Bar)", m16);

        juce::PopupMenu m32;
        for (std::size_t i = 6; i < 10; ++i) {
            m32.addItem(static_cast<int>(i + 1), presets[i].name);
        }
        m.addSubMenu("32 Steps (2 Bars)", m32);

        juce::PopupMenu m64;
        for (std::size_t i = 10; i < 13; ++i) {
            m64.addItem(static_cast<int>(i + 1), presets[i].name);
        }
        m.addSubMenu("64 Steps (4 Bars)", m64);

        m.addSeparator();

        juce::PopupMenu mUtil;
        mUtil.addItem(100, "All Steps ON (16 Steps)");
        mUtil.addItem(101, "All Steps ON (32 Steps)");
        mUtil.addItem(102, "All Steps ON (64 Steps)");
        mUtil.addItem(103, "Clear All Gates (Mute Pattern)");
        m.addSubMenu("Utilities & Tools", mUtil);

        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&rhythmPresetsBtn_),
            [this, presets](int res) {
                if (res <= 0) return;

                if (res >= 1 && res <= static_cast<int>(presets.size())) {
                    const auto& p = presets[static_cast<std::size_t>(res - 1)];
                    proc_.applyArpRhythmPattern(p.length, p.gates, p.velocities);
                } else if (res == 100) {
                    std::vector<bool> g(16, true);
                    std::vector<float> v(16, 0.85f);
                    proc_.applyArpRhythmPattern(16, g, v);
                } else if (res == 101) {
                    std::vector<bool> g(32, true);
                    std::vector<float> v(32, 0.85f);
                    proc_.applyArpRhythmPattern(32, g, v);
                } else if (res == 102) {
                    std::vector<bool> g(64, true);
                    std::vector<float> v(64, 0.85f);
                    proc_.applyArpRhythmPattern(64, g, v);
                } else if (res == 103) {
                    const int len = proc_.getArpPatternLength();
                    std::vector<bool> g(static_cast<std::size_t>(len), false);
                    std::vector<float> v(static_cast<std::size_t>(len), 0.5f);
                    proc_.applyArpRhythmPattern(len, g, v);
                }

                updateMatrixLayout();
                stepMatrixComp_.repaint();
                repaint(cardMatrixRect_);
            });
    }

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;

    juce::Rectangle<int> cardCoreRect_;
    juce::Rectangle<int> cardLogicRect_;
    juce::Rectangle<int> cardMatrixRect_;

    // Core Controls
    juce::TextButton arpEnableBtn_;
    juce::TextButton arpLatchBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> arpEnableAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> arpLatchAtt_;

    nova::ui::ArcKnob arpRate_;
    nova::ui::ArcKnob arpGate_;
    nova::ui::ArcKnob arpSwing_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> arpRateAtt_;

    juce::Rectangle<int> syncBtnRect_;
    bool isSyncHovered_ = false;

    // Pattern Logic
    std::array<juce::TextButton, 6> modeBtns_;
    std::array<juce::TextButton, 4> octaveBtns_;

    // Pattern Length Selector
    juce::TextButton len16Btn_;
    juce::TextButton len32Btn_;
    juce::TextButton len64Btn_;
    juce::TextButton rhythmPresetsBtn_;

    // Horizontal Viewport & Step Matrix
    HorizontalViewport matrixViewport_;
    StepMatrixComponent stepMatrixComp_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpeggiatorHubPanel)
};

} // namespace nova::synth::ui
