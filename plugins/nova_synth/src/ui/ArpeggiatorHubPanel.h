#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <nova_ui/NovaTheme.h>
#include <nova_ui/ArcKnob.h>
#include "HorizontalViewport.h"
#include "../NovaSynthProcessor.h"

namespace nova::synth::ui {

class ArpeggiatorHubPanel;

/**
 * @brief Modal Save / Overwrite Dialog Overlay for Arp Rhythm Presets
 *
 * Implements a Look'n'Feel-conformant modal popup for saving rhythm matrix patterns:
 * - In InputName mode: user can input a preset name and click SAVE or CANCEL.
 * - If the entered name exists: transitions to ConfirmOverwrite mode asking whether to overwrite.
 * - In ConfirmOverwrite mode:
 *   - Overwrite confirms and saves to UserArpRhythmPresets.json.
 *   - Cancel returns to InputName mode with the name preserved so the user can modify it.
 */
class ArpPatternSaveOverlay : public juce::Component {
public:
    enum class Mode {
        InputName,
        ConfirmOverwrite
    };

    ArpPatternSaveOverlay(ArpeggiatorHubPanel& owner, NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : owner_(owner), proc_(proc), theme_(theme) {

        titleLabel_.setFont(theme_.getSectionFont(12.0f));
        titleLabel_.setJustificationType(juce::Justification::centred);
        titleLabel_.setColour(juce::Label::textColourId, theme_.accentPrimary);
        addAndMakeVisible(titleLabel_);

        promptLabel_.setFont(theme_.getLabelFont(11.0f));
        promptLabel_.setJustificationType(juce::Justification::centred);
        promptLabel_.setColour(juce::Label::textColourId, theme_.textPrimary);
        addAndMakeVisible(promptLabel_);

        nameEditor_.setMultiLine(false);
        nameEditor_.setReturnKeyStartsNewLine(false);
        nameEditor_.setFont(theme_.getValueFont(12.0f));
        nameEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff090d16));
        nameEditor_.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        nameEditor_.setColour(juce::TextEditor::outlineColourId, theme_.accentPrimary.withAlpha(0.6f));
        nameEditor_.setColour(juce::TextEditor::focusedOutlineColourId, theme_.accentPrimary);
        nameEditor_.setColour(juce::TextEditor::highlightColourId, theme_.accentPrimary.withAlpha(0.35f));
        nameEditor_.onReturnKey = [this]() { handleSaveAttempt(); };
        nameEditor_.onEscapeKey = [this]() { handleCancel(); };
        addAndMakeVisible(nameEditor_);

        cancelBtn_.setButtonText("CANCEL");
        cancelBtn_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
        cancelBtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffcbd5e1));
        cancelBtn_.onClick = [this]() { handleCancel(); };
        addAndMakeVisible(cancelBtn_);

        saveBtn_.setButtonText("SAVE");
        saveBtn_.setColour(juce::TextButton::buttonColourId, theme_.accentPrimary);
        saveBtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff090d16));
        saveBtn_.onClick = [this]() { handleSaveAttempt(); };
        addAndMakeVisible(saveBtn_);

        overwriteBtn_.setButtonText("OVERWRITE");
        overwriteBtn_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff59e0b));
        overwriteBtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff090d16));
        overwriteBtn_.onClick = [this]() { performSave(pendingName_); };
        addChildComponent(overwriteBtn_);
    }

    void startSave(const juce::String& defaultName) {
        mode_ = Mode::InputName;
        pendingName_ = defaultName;
        titleLabel_.setText("SAVE RHYTHM PRESET", juce::dontSendNotification);
        promptLabel_.setText("Enter a name for this rhythm matrix pattern:", juce::dontSendNotification);
        nameEditor_.setText(defaultName, false);
        nameEditor_.selectAll();
        nameEditor_.setVisible(true);
        saveBtn_.setVisible(true);
        overwriteBtn_.setVisible(false);
        cancelBtn_.setVisible(true);
        resized();
        repaint();
        nameEditor_.grabKeyboardFocus();
    }

    void paint(juce::Graphics& g) override {
        // Semi-transparent backdrop
        g.fillAll(juce::Colour(0xd0000000));

        // Dialog box card
        const auto r = dialogBoxRect_.toFloat();
        const auto grad = juce::ColourGradient::vertical(
            juce::Colour(0xff141c2c), r.getY(),
            juce::Colour(0xff090e18), r.getBottom());
        g.setGradientFill(grad);
        g.fillRoundedRectangle(r, 8.0f);

        // Border glow
        g.setColour(mode_ == Mode::ConfirmOverwrite ? juce::Colour(0xfff59e0b).withAlpha(0.85f)
                                                    : theme_.accentPrimary.withAlpha(0.85f));
        g.drawRoundedRectangle(r, 8.0f, 1.5f);
    }

    void resized() override {
        const int boxW = 380;
        const int boxH = (mode_ == Mode::ConfirmOverwrite) ? 170 : 160;
        dialogBoxRect_ = juce::Rectangle<int>(
            (getWidth() - boxW) / 2,
            std::max(10, (getHeight() - boxH) / 2),
            boxW,
            boxH);

        auto inner = dialogBoxRect_.reduced(16, 14);
        titleLabel_.setBounds(inner.removeFromTop(20));
        inner.removeFromTop(8);

        if (mode_ == Mode::InputName) {
            promptLabel_.setBounds(inner.removeFromTop(18));
            inner.removeFromTop(8);
            nameEditor_.setBounds(inner.removeFromTop(26).reduced(24, 0));
            inner.removeFromTop(12);

            auto btnRow = inner.removeFromTop(26).reduced(40, 0);
            const int btnW = (btnRow.getWidth() - 12) / 2;
            cancelBtn_.setBounds(btnRow.removeFromLeft(btnW));
            saveBtn_.setBounds(btnRow.removeFromRight(btnW));
        } else {
            promptLabel_.setBounds(inner.removeFromTop(44));
            inner.removeFromTop(12);

            auto btnRow = inner.removeFromTop(26).reduced(30, 0);
            const int btnW = (btnRow.getWidth() - 12) / 2;
            cancelBtn_.setBounds(btnRow.removeFromLeft(btnW));
            overwriteBtn_.setBounds(btnRow.removeFromRight(btnW));
        }
    }

    void mouseDown(const juce::MouseEvent& event) override {
        if (!dialogBoxRect_.contains(event.getPosition())) {
            if (mode_ == Mode::ConfirmOverwrite) {
                switchToInputMode();
            } else {
                handleCancel();
            }
        }
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key == juce::KeyPress::escapeKey) {
            handleCancel();
            return true;
        }
        if (key == juce::KeyPress::returnKey) {
            if (mode_ == Mode::ConfirmOverwrite) {
                performSave(pendingName_);
            } else {
                handleSaveAttempt();
            }
            return true;
        }
        return false;
    }

private:
    void handleSaveAttempt() {
        const juce::String entered = nameEditor_.getText().trim();
        if (entered.isEmpty()) {
            return;
        }
        pendingName_ = entered;

        const auto presets = proc_.getAvailableArpRhythmPresets();
        bool exists = false;
        for (const auto& p : presets) {
            if (p.name.equalsIgnoreCase(pendingName_)) {
                exists = true;
                break;
            }
        }

        if (exists) {
            mode_ = Mode::ConfirmOverwrite;
            titleLabel_.setText("OVERWRITE PRESET?", juce::dontSendNotification);
            promptLabel_.setText("A preset named \"" + pendingName_ + "\" already exists.\nDo you want to overwrite it?",
                                 juce::dontSendNotification);
            nameEditor_.setVisible(false);
            saveBtn_.setVisible(false);
            overwriteBtn_.setVisible(true);
            cancelBtn_.setVisible(true);
            resized();
            repaint();
        } else {
            performSave(pendingName_);
        }
    }

    void handleCancel() {
        if (mode_ == Mode::ConfirmOverwrite) {
            switchToInputMode();
        } else {
            setVisible(false);
        }
    }

    void switchToInputMode() {
        mode_ = Mode::InputName;
        titleLabel_.setText("SAVE RHYTHM PRESET", juce::dontSendNotification);
        promptLabel_.setText("Enter a name for this rhythm matrix pattern:", juce::dontSendNotification);
        nameEditor_.setText(pendingName_, false);
        nameEditor_.selectAll();
        nameEditor_.setVisible(true);
        saveBtn_.setVisible(true);
        overwriteBtn_.setVisible(false);
        cancelBtn_.setVisible(true);
        resized();
        repaint();
        nameEditor_.grabKeyboardFocus();
    }

    void performSave(const juce::String& name);

    ArpeggiatorHubPanel& owner_;
    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    Mode mode_ = Mode::InputName;
    juce::String pendingName_;
    juce::Rectangle<int> dialogBoxRect_;

    juce::Label titleLabel_;
    juce::Label promptLabel_;
    juce::TextEditor nameEditor_;
    juce::TextButton cancelBtn_;
    juce::TextButton saveBtn_;
    juce::TextButton overwriteBtn_;
};

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
    friend class ArpPatternSaveOverlay;

    class StepMatrixComponent : public juce::Component {
    public:
        StepMatrixComponent(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
            : proc_(proc), theme_(theme) {}

        std::function<void()> onModified;

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
                    if (onModified) onModified();
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

        // 5. Rhythm Preset Navigation & Saving
        prevPresetBtn_.setButtonText("<");
        prevPresetBtn_.setTooltip("Previous rhythm preset");
        prevPresetBtn_.onClick = [this]() { selectPreviousPreset(); };
        addAndMakeVisible(prevPresetBtn_);

        presetNameBtn_.setTooltip("Choose a musical rhythm matrix preset");
        presetNameBtn_.onClick = [this]() { showRhythmPresetsMenu(); };
        addAndMakeVisible(presetNameBtn_);

        nextPresetBtn_.setButtonText(">");
        nextPresetBtn_.setTooltip("Next rhythm preset");
        nextPresetBtn_.onClick = [this]() { selectNextPreset(); };
        addAndMakeVisible(nextPresetBtn_);

        savePresetBtn_.setButtonText("SAVE");
        savePresetBtn_.setTooltip("Save current rhythm matrix as user preset");
        savePresetBtn_.setColour(juce::TextButton::buttonColourId, theme_.accentPrimary.withAlpha(0.25f));
        savePresetBtn_.setColour(juce::TextButton::textColourOffId, theme_.accentPrimary);
        savePresetBtn_.onClick = [this]() { openSavePresetDialog(); };
        addAndMakeVisible(savePresetBtn_);

        updatePresetDisplay();

        // 6. Pattern Length Selectors
        len16Btn_.setButtonText("16");
        len32Btn_.setButtonText("32");
        len64Btn_.setButtonText("64");

        len16Btn_.onClick = [this]() {
            if (proc_.getArpPatternLength() != 16) {
                proc_.setArpPatternLength(16);
                markModified();
                updateMatrixLayout();
            }
        };
        len32Btn_.onClick = [this]() {
            if (proc_.getArpPatternLength() != 32) {
                proc_.setArpPatternLength(32);
                markModified();
                updateMatrixLayout();
            }
        };
        len64Btn_.onClick = [this]() {
            if (proc_.getArpPatternLength() != 64) {
                proc_.setArpPatternLength(64);
                markModified();
                updateMatrixLayout();
            }
        };

        addAndMakeVisible(len16Btn_);
        addAndMakeVisible(len32Btn_);
        addAndMakeVisible(len64Btn_);

        // Matrix component modification hook
        stepMatrixComp_.onModified = [this]() {
            markModified();
        };

        // 7. Viewport with inner Step Matrix component
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
        const int lenBtnW = 28;
        const int lenBtnH = 17;
        const int lenBtnY = cardMatrixRect_.getY() + 5;
        const int rightX = cardMatrixRect_.getRight() - 10;
        len64Btn_.setBounds(rightX - lenBtnW, lenBtnY, lenBtnW, lenBtnH);
        len32Btn_.setBounds(rightX - lenBtnW * 2 - 3, lenBtnY, lenBtnW, lenBtnH);
        len16Btn_.setBounds(rightX - lenBtnW * 3 - 6, lenBtnY, lenBtnW, lenBtnH);

        const int stepsLabelLeft = len16Btn_.getX() - 48;
        const int presetAreaLeft = cardMatrixRect_.getX() + 175;
        const int presetAreaRight = stepsLabelLeft - 8;
        const int availPresetsW = std::max(120, presetAreaRight - presetAreaLeft);

        const int arrowW = 18;
        const int saveW = 42;
        const int hGap = 3;
        const int nameW = std::clamp(availPresetsW - (arrowW * 2 + saveW + hGap * 3), 90, 200);
        const int totalPresetW = arrowW + hGap + nameW + hGap + arrowW + hGap + saveW;
        int curX = presetAreaRight - totalPresetW;

        prevPresetBtn_.setBounds(curX, lenBtnY, arrowW, lenBtnH);
        curX += arrowW + hGap;
        presetNameBtn_.setBounds(curX, lenBtnY, nameW, lenBtnH);
        curX += nameW + hGap;
        nextPresetBtn_.setBounds(curX, lenBtnY, arrowW, lenBtnH);
        curX += arrowW + hGap;
        savePresetBtn_.setBounds(curX, lenBtnY, saveW, lenBtnH);

        updateMatrixLayout();

        if (saveOverlay_ != nullptr) {
            saveOverlay_->setBounds(getLocalBounds());
        }
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

    [[nodiscard]] juce::String getCurrentPresetName() const noexcept { return currentPresetName_; }
    [[nodiscard]] bool isPatternModified() const noexcept { return isModified_; }

    void markModified() {
        if (!isModified_) {
            isModified_ = true;
            updatePresetDisplay();
        }
    }

    void updatePresetDisplay() {
        const juce::String prefix = isModified_ ? "* " : "";
        presetNameBtn_.setButtonText(prefix + currentPresetName_);
    }

    void selectPreviousPreset() {
        const auto presets = proc_.getAvailableArpRhythmPresets();
        if (presets.empty()) return;
        int idx = 0;
        for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
            if (presets[static_cast<std::size_t>(i)].name == currentPresetName_) {
                idx = i;
                break;
            }
        }
        const int count = static_cast<int>(presets.size());
        const int newIdx = (idx - 1 + count) % count;
        const auto& p = presets[static_cast<std::size_t>(newIdx)];
        proc_.applyArpRhythmPattern(p.length, p.gates, p.velocities);
        currentPresetName_ = p.name;
        isModified_ = false;
        updatePresetDisplay();
        updateMatrixLayout();
        stepMatrixComp_.repaint();
        repaint(cardMatrixRect_);
    }

    void selectNextPreset() {
        const auto presets = proc_.getAvailableArpRhythmPresets();
        if (presets.empty()) return;
        int idx = 0;
        for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
            if (presets[static_cast<std::size_t>(i)].name == currentPresetName_) {
                idx = i;
                break;
            }
        }
        const int count = static_cast<int>(presets.size());
        const int newIdx = (idx + 1) % count;
        const auto& p = presets[static_cast<std::size_t>(newIdx)];
        proc_.applyArpRhythmPattern(p.length, p.gates, p.velocities);
        currentPresetName_ = p.name;
        isModified_ = false;
        updatePresetDisplay();
        updateMatrixLayout();
        stepMatrixComp_.repaint();
        repaint(cardMatrixRect_);
    }

    void openSavePresetDialog() {
        if (saveOverlay_ == nullptr) {
            saveOverlay_ = std::make_unique<ArpPatternSaveOverlay>(*this, proc_, theme_);
            addAndMakeVisible(*saveOverlay_);
        }
        saveOverlay_->setBounds(getLocalBounds());
        saveOverlay_->startSave(currentPresetName_);
        saveOverlay_->setVisible(true);
        saveOverlay_->toFront(true);
    }

    void onPresetSaved(const juce::String& name) {
        currentPresetName_ = name;
        isModified_ = false;
        updatePresetDisplay();
        stepMatrixComp_.repaint();
        repaint(cardMatrixRect_);
    }

    void showRhythmPresetsMenu() {
        juce::PopupMenu m;
        const auto presets = proc_.getAvailableArpRhythmPresets();

        juce::PopupMenu m16;
        juce::PopupMenu m32;
        juce::PopupMenu m64;
        juce::PopupMenu mUser;

        for (std::size_t i = 0; i < presets.size(); ++i) {
            const int id = static_cast<int>(i + 1);
            const auto& p = presets[i];
            const bool isCurrent = (p.name == currentPresetName_);
            if (p.isUser) {
                mUser.addItem(id, p.name, true, isCurrent);
            } else if (p.length == 16) {
                m16.addItem(id, p.name, true, isCurrent);
            } else if (p.length == 32) {
                m32.addItem(id, p.name, true, isCurrent);
            } else if (p.length == 64) {
                m64.addItem(id, p.name, true, isCurrent);
            }
        }

        m.addSubMenu("16 Steps (1 Bar)", m16);
        m.addSubMenu("32 Steps (2 Bars)", m32);
        m.addSubMenu("64 Steps (4 Bars)", m64);
        if (mUser.getNumItems() > 0) {
            m.addSubMenu("User Presets", mUser);
        }

        m.addSeparator();

        juce::PopupMenu mUtil;
        mUtil.addItem(100, "All Steps ON (16 Steps)");
        mUtil.addItem(101, "All Steps ON (32 Steps)");
        mUtil.addItem(102, "All Steps ON (64 Steps)");
        mUtil.addItem(103, "Clear All Gates (Mute Pattern)");
        m.addSubMenu("Utilities & Tools", mUtil);

        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetNameBtn_),
            [this, presets](int res) {
                if (res <= 0) return;

                if (res >= 1 && res <= static_cast<int>(presets.size())) {
                    const auto& p = presets[static_cast<std::size_t>(res - 1)];
                    proc_.applyArpRhythmPattern(p.length, p.gates, p.velocities);
                    currentPresetName_ = p.name;
                    isModified_ = false;
                } else if (res == 100) {
                    std::vector<bool> g(16, true);
                    std::vector<float> v(16, 0.85f);
                    proc_.applyArpRhythmPattern(16, g, v);
                    isModified_ = true;
                } else if (res == 101) {
                    std::vector<bool> g(32, true);
                    std::vector<float> v(32, 0.85f);
                    proc_.applyArpRhythmPattern(32, g, v);
                    isModified_ = true;
                } else if (res == 102) {
                    std::vector<bool> g(64, true);
                    std::vector<float> v(64, 0.85f);
                    proc_.applyArpRhythmPattern(64, g, v);
                    isModified_ = true;
                } else if (res == 103) {
                    const int len = proc_.getArpPatternLength();
                    std::vector<bool> g(static_cast<std::size_t>(len), false);
                    std::vector<float> v(static_cast<std::size_t>(len), 0.5f);
                    proc_.applyArpRhythmPattern(len, g, v);
                    isModified_ = true;
                }

                updatePresetDisplay();
                updateMatrixLayout();
                stepMatrixComp_.repaint();
                repaint(cardMatrixRect_);
            });
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

    // Rhythm Preset Navigation & Saving
    juce::TextButton prevPresetBtn_;
    juce::TextButton presetNameBtn_;
    juce::TextButton nextPresetBtn_;
    juce::TextButton savePresetBtn_;

    juce::String currentPresetName_{ "Straight 16 (Full Drive)" };
    bool isModified_{ false };

    // Horizontal Viewport & Step Matrix
    HorizontalViewport matrixViewport_;
    StepMatrixComponent stepMatrixComp_;

    std::unique_ptr<ArpPatternSaveOverlay> saveOverlay_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpeggiatorHubPanel)
};

inline void ArpPatternSaveOverlay::performSave(const juce::String& name) {
    const int len = proc_.getArpPatternLength();
    std::vector<bool> gates(static_cast<std::size_t>(len));
    std::vector<float> vels(static_cast<std::size_t>(len));
    for (int i = 0; i < len; ++i) {
        gates[static_cast<std::size_t>(i)] = proc_.isArpStepGateActive(i);
        vels[static_cast<std::size_t>(i)] = proc_.getArpStepVelocity(i);
    }
    proc_.saveUserArpRhythmPreset(name, len, gates, vels);
    owner_.onPresetSaved(name);
    setVisible(false);
}

} // namespace nova::synth::ui
