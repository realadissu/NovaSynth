#include "ArpeggiatorHubPanel.h"
#include "../NovaSynthProcessor.h"
#include <algorithm>
#include <cmath>

namespace nova::synth::ui {

void ArpeggiatorHubPanel::StepMatrixComponent::paint(juce::Graphics& g) {
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

void ArpeggiatorHubPanel::paint(juce::Graphics& g) {
    g.fillAll(theme_.bgSurface.darker(0.18f));
    drawCoreCard(g, cardCoreRect_.toFloat());
    drawLogicCard(g, cardLogicRect_.toFloat());
    drawMatrixCard(g, cardMatrixRect_.toFloat());
}

void ArpeggiatorHubPanel::drawCoreCard(juce::Graphics& g, juce::Rectangle<float> area) {
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

void ArpeggiatorHubPanel::drawSyncButton(juce::Graphics& g, juce::Rectangle<float> btnRect, bool isSynced, juce::Colour col, bool isHovered) {
    constexpr float radius = 3.5f;
    if (isSynced) {
        g.setColour(col.withAlpha(0.22f));
        g.fillRoundedRectangle(btnRect.expanded(1.5f), radius + 1.0f);
        const auto grad = juce::ColourGradient::vertical(
            col.withAlpha(0.38f), btnRect.getY(), col.withAlpha(0.20f), btnRect.getBottom());
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

void ArpeggiatorHubPanel::drawChainIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour col) {
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
    const auto xform = juce::AffineTransform::rotation(-juce::MathConstants<float>::pi * 0.25f).translated(cx, cy);

    // 1. Center link bar
    juce::Path bar;
    bar.startNewSubPath(-barHalfLen, 0.0f);
    bar.lineTo(barHalfLen, 0.0f);
    bar.applyTransform(xform);

    // 2. Right loop
    juce::Path rightLoop;
    rightLoop.startNewSubPath(loopCenterX - legLen, loopRadius);
    rightLoop.lineTo(loopCenterX, loopRadius);
    rightLoop.addCentredArc(loopCenterX, 0.0f, loopRadius, loopRadius, 0.0f, juce::MathConstants<float>::pi, 0.0f, false);
    rightLoop.lineTo(loopCenterX - legLen, -loopRadius);
    rightLoop.applyTransform(xform);

    // 3. Left loop
    juce::Path leftLoop;
    leftLoop.startNewSubPath(-loopCenterX + legLen, -loopRadius);
    leftLoop.lineTo(-loopCenterX, -loopRadius);
    leftLoop.addCentredArc(-loopCenterX, 0.0f, loopRadius, loopRadius, 0.0f, 0.0f, -juce::MathConstants<float>::pi, false);
    leftLoop.lineTo(-loopCenterX + legLen, loopRadius);
    leftLoop.applyTransform(xform);

    g.setColour(col);
    g.strokePath(bar, barStrokeType);
    g.strokePath(rightLoop, strokeType);
    g.strokePath(leftLoop, strokeType);
}

void ArpeggiatorHubPanel::drawLogicCard(juce::Graphics& g, juce::Rectangle<float> area) {
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

void ArpeggiatorHubPanel::drawMatrixCard(juce::Graphics& g, juce::Rectangle<float> area) {
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

void ArpeggiatorHubPanel::markModified() {
    if (!isModified_) {
        isModified_ = true;
        updatePresetDisplay();
    }
}

void ArpeggiatorHubPanel::updatePresetDisplay() {
    const juce::String prefix = isModified_ ? "* " : "";
    presetNameBtn_.setButtonText(prefix + currentPresetName_);
}

void ArpeggiatorHubPanel::selectPreviousPreset() {
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

void ArpeggiatorHubPanel::selectNextPreset() {
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

void ArpeggiatorHubPanel::showRhythmPresetsMenu() {
    juce::PopupMenu m, m16, m32, m64, mUser;
    const auto presets = proc_.getAvailableArpRhythmPresets();

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

} // namespace nova::synth::ui
