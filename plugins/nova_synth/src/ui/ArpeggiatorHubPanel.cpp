#include "ArpeggiatorHubPanel.h"
#include "ArpPatternSaveOverlay.h"
#include "../NovaSynthProcessor.h"
#include <algorithm>
#include <cmath>

namespace nova::synth::ui {

// StepMatrixComponent interaction
ArpeggiatorHubPanel::StepMatrixComponent::StepMatrixComponent(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc), theme_(theme) {}

void ArpeggiatorHubPanel::StepMatrixComponent::mouseDown(const juce::MouseEvent& event) {
    handleMouse(event.getPosition(), true);
}

void ArpeggiatorHubPanel::StepMatrixComponent::mouseDrag(const juce::MouseEvent& event) {
    handleMouse(event.getPosition(), false);
}

void ArpeggiatorHubPanel::StepMatrixComponent::updateStepRects(int numSteps, int stepW, int totalH) {
    stepRects_.clear();
    stepRects_.reserve(static_cast<std::size_t>(numSteps));
    for (int s = 0; s < numSteps; ++s) {
        stepRects_.push_back(juce::Rectangle<int>(s * (stepW + 3), 0, stepW, totalH));
    }
    repaint();
}

void ArpeggiatorHubPanel::StepMatrixComponent::handleMouse(juce::Point<int> pos, bool isClick) {
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

// ArpeggiatorHubPanel
ArpeggiatorHubPanel::ArpeggiatorHubPanel(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
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
    arpEnableAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "arpEnabled", arpEnableBtn_);
    addAndMakeVisible(arpEnableBtn_);

    arpLatchBtn_.setButtonText("LATCH / HOLD");
    arpLatchBtn_.setClickingTogglesState(true);
    arpLatchAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "arpLatch", arpLatchBtn_);
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
    static constexpr std::array<const char*, 4> kOctaveNames = { "1 OCT", "2 OCT", "3 OCT", "4 OCT" };
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

    stepMatrixComp_.onModified = [this]() { markModified(); };

    // 7. Viewport with inner Step Matrix component
    matrixViewport_.setViewedComponent(&stepMatrixComp_, false);
    addAndMakeVisible(matrixViewport_);

    startTimerHz(30);
}

ArpeggiatorHubPanel::~ArpeggiatorHubPanel() {
    stopTimer();
}

juce::String ArpeggiatorHubPanel::getTooltip() {
    return "Rhythmic Worship Arpeggiator & Step Sequencer (16/32/64 steps). Use Dotted 8th for signature ambient plucks.";
}

void ArpeggiatorHubPanel::timerCallback() {
    stepMatrixComp_.repaint();
    repaint(cardMatrixRect_);
}

void ArpeggiatorHubPanel::setupRateKnob() {
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

void ArpeggiatorHubPanel::toggleArpSync() {
    if (auto* p = proc_.getAPVTS().getParameter("arpSync")) {
        const float cur = p->getValue();
        p->setValueNotifyingHost(cur > 0.5f ? 0.0f : 1.0f);
        setupRateKnob();
        repaint(cardCoreRect_);
    }
}

void ArpeggiatorHubPanel::updateMatrixLayout() {
    const int numSteps = proc_.getArpPatternLength();
    const auto mArea = cardMatrixRect_.withTrimmedTop(28).reduced(8, 6);
    const int visibleW = mArea.getWidth();
    const int visibleH = mArea.getHeight();

    // 16 steps: bars fill the visible area
    const int stepW_16 = std::max(20, (visibleW - 15 * 3) / 16);
    int stepW = stepW_16;
    int contentW = visibleW;

    if (numSteps > 16) {
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

void ArpeggiatorHubPanel::resized() {
    const auto area = getLocalBounds().reduced(8, 6);
    const int totalW = area.getWidth();
    const int totalH = area.getHeight();
    constexpr int gap = 8;
    const int availW = totalW - gap * 2;

    const int wCore   = static_cast<int>(static_cast<float>(availW) * 0.27f);
    const int wLogic  = static_cast<int>(static_cast<float>(availW) * 0.19f);
    const int wMatrix = availW - wCore - wLogic;

    cardCoreRect_   = juce::Rectangle<int>(area.getX(), area.getY(), wCore, totalH);
    cardLogicRect_  = juce::Rectangle<int>(cardCoreRect_.getRight() + gap, area.getY(), wLogic, totalH);
    cardMatrixRect_ = juce::Rectangle<int>(cardLogicRect_.getRight() + gap, area.getY(), wMatrix, totalH);

    // Core Controls
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

    // Logic Controls
    auto lArea = cardLogicRect_.withTrimmedTop(28).reduced(8, 6);
    const int modeRowH = 22;
    for (int r = 0; r < 3; ++r) {
        auto row = lArea.removeFromTop(modeRowH);
        const int btnW = (row.getWidth() - 4) / 2;
        modeBtns_[static_cast<std::size_t>(r * 2)].setBounds(row.removeFromLeft(btnW));
        modeBtns_[static_cast<std::size_t>(r * 2 + 1)].setBounds(row.removeFromRight(btnW));
        lArea.removeFromTop(4);
    }

    lArea.removeFromTop(6);
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

void ArpeggiatorHubPanel::mouseMove(const juce::MouseEvent& event) {
    const auto pos = event.getPosition();
    const bool hoverSync = syncBtnRect_.contains(pos);
    if (hoverSync != isSyncHovered_) {
        isSyncHovered_ = hoverSync;
        setMouseCursor(isSyncHovered_ ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint(cardCoreRect_);
    }
}

void ArpeggiatorHubPanel::mouseExit(const juce::MouseEvent&) {
    if (isSyncHovered_) {
        isSyncHovered_ = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint(cardCoreRect_);
    }
}

void ArpeggiatorHubPanel::mouseDown(const juce::MouseEvent& event) {
    const auto pos = event.getPosition();
    if (syncBtnRect_.contains(pos)) {
        toggleArpSync();
    }
}

void ArpeggiatorHubPanel::openSavePresetDialog() {
    if (saveOverlay_ == nullptr) {
        saveOverlay_ = std::make_unique<ArpPatternSaveOverlay>(*this, proc_, theme_);
        addAndMakeVisible(*saveOverlay_);
    }
    saveOverlay_->setBounds(getLocalBounds());
    saveOverlay_->startSave(currentPresetName_);
    saveOverlay_->setVisible(true);
    saveOverlay_->toFront(true);
}

void ArpeggiatorHubPanel::onPresetSaved(const juce::String& name) {
    currentPresetName_ = name;
    isModified_ = false;
    updatePresetDisplay();
    stepMatrixComp_.repaint();
    repaint(cardMatrixRect_);
}

} // namespace nova::synth::ui
