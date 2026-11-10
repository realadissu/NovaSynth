#include "StandaloneKeyboardBar.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace nova::synth {

void StandaloneKeyboardBar::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(theme_.bgHeader.darker(0.3f));
    g.fillRoundedRectangle(bounds, theme_.cornerRadius);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(bounds, theme_.cornerRadius, 1.0f);

    auto area = getLocalBounds().reduced(6, 4);

    // Left section: Worship Chords & Octave Controls
    auto leftControls = area.removeFromLeft(316);
    drawLeftControls(g, leftControls);

    // Hardware Pitch & Mod/Swell Wheels right next to the keyboard
    auto wheelsArea = area.removeFromLeft(58);
    drawPerformanceWheels(g, wheelsArea);

    // Right section: Velocity & Keyboard Mode
    auto rightControls = area.removeFromRight(136);
    drawRightControls(g, rightControls);

    // Center section: Dynamic-Range Virtual Keyboard (Fixed Key Width, reveals up to 88 keys as widened)
    keyboardArea_ = area.reduced(4, 2);
    drawVirtualKeyboard(g, keyboardArea_);
}

void StandaloneKeyboardBar::drawLeftControls(juce::Graphics& g, juce::Rectangle<int> area) {
    auto topRow = area.removeFromTop(20);
    octDownRect_ = topRow.removeFromLeft(22);
    octDispRect_ = topRow.removeFromLeft(48);
    octUpRect_   = topRow.removeFromLeft(22);
    topRow.removeFromLeft(8);
    holdBtnRect_ = topRow.removeFromLeft(50);

    g.setColour(theme_.bgElevated);
    g.fillRoundedRectangle(octDownRect_.toFloat(), 3.0f);
    g.fillRoundedRectangle(octUpRect_.toFloat(), 3.0f);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(octDownRect_.toFloat(), 3.0f, 0.8f);
    g.drawRoundedRectangle(octUpRect_.toFloat(), 3.0f, 0.8f);
    g.setFont(theme_.getValueFont(11.0f));
    g.setColour(theme_.textPrimary);
    g.drawText("<", octDownRect_, juce::Justification::centred, false);
    g.drawText(">", octUpRect_, juce::Justification::centred, false);

    g.setFont(theme_.getValueFont(9.5f));
    g.setColour(theme_.textSecondary);
    g.drawText(juce::String("OCT ") + (octaveShift_ >= 0 ? "+" : "") + juce::String(octaveShift_),
               octDispRect_, juce::Justification::centred, false);

    // HOLD button
    g.setColour(isHoldActive_ ? theme_.accentPrimary.withAlpha(0.35f) : theme_.bgElevated);
    g.fillRoundedRectangle(holdBtnRect_.toFloat(), 3.0f);
    g.setColour(isHoldActive_ ? theme_.accentPrimary : theme_.borderSubtle);
    g.drawRoundedRectangle(holdBtnRect_.toFloat(), 3.0f, isHoldActive_ ? 1.2f : 0.8f);
    g.setFont(theme_.getValueFont(9.5f));
    g.setColour(isHoldActive_ ? juce::Colours::white : theme_.textMuted);
    g.drawText("HOLD", holdBtnRect_, juce::Justification::centred, false);

    // Bottom row: Worship Chord buttons + C3 Solo
    auto chordRow = area.removeFromBottom(24);
    static constexpr std::array<const char*, 7> kChordNames = {
        "C3", "C Maj9", "F Maj7", "Am7", "G6 Sus", "Dm9", "Em7"
    };
    chordRects_.clear();
    const int btnW = (chordRow.getWidth() - 12) / 7;
    for (int i = 0; i < 7; ++i) {
        const juce::Rectangle<int> cr(chordRow.getX() + i * (btnW + 2), chordRow.getY(), btnW, chordRow.getHeight());
        chordRects_.push_back(cr);
        const bool isHeld = chordHeld_[static_cast<std::size_t>(i)];

        g.setColour(isHeld ? theme_.accentPrimary.withAlpha(0.35f) : theme_.bgElevated);
        g.fillRoundedRectangle(cr.toFloat(), 3.0f);
        g.setColour(isHeld ? theme_.accentPrimary : theme_.borderSubtle.withAlpha(0.6f));
        g.drawRoundedRectangle(cr.toFloat(), 3.0f, isHeld ? 1.2f : 0.8f);
        g.setFont(theme_.getValueFont(8.5f));
        g.setColour(isHeld ? juce::Colours::white : (i == 0 ? juce::Colour(0xff2dd4bf) : theme_.textSecondary));
        g.drawText(kChordNames[static_cast<std::size_t>(i)], cr, juce::Justification::centred, false);
    }
}

void StandaloneKeyboardBar::drawRightControls(juce::Graphics& g, juce::Rectangle<int> area) {
    auto topRow = area.removeFromTop(20);
    velRect_ = topRow;
    g.setFont(theme_.getValueFont(9.5f));
    g.setColour(isDraggingVelocity_ ? theme_.accentPrimary : theme_.textSecondary);
    const int velPct = static_cast<int>(std::round(auditionVelocity_ * 100.0f));
    g.drawText("VELOCITY: " + juce::String(velPct) + "%", topRow, juce::Justification::centredRight, false);

    auto botRow = area.removeFromBottom(22);
    vkToggleRect_ = botRow.reduced(4, 1);
    g.setColour(qwertyActive_ ? theme_.accentPrimary.withAlpha(0.25f) : theme_.bgElevated);
    g.fillRoundedRectangle(vkToggleRect_.toFloat(), 3.0f);
    g.setColour(qwertyActive_ ? theme_.accentPrimary : theme_.borderSubtle.withAlpha(0.7f));
    g.drawRoundedRectangle(vkToggleRect_.toFloat(), 3.0f, qwertyActive_ ? 1.2f : 0.8f);

    auto r = vkToggleRect_.toFloat().reduced(4.0f, 3.0f);
    auto iconRect = r.removeFromLeft(14.0f).withSizeKeepingCentre(12.0f, 9.0f);
    g.setColour(qwertyActive_ ? juce::Colours::white : theme_.accentPrimary);
    g.drawRoundedRectangle(iconRect, 1.5f, 1.0f);
    g.fillRect(iconRect.getX() + 2.5f, iconRect.getY() + 2.5f, 2.0f, 1.5f);
    g.fillRect(iconRect.getX() + 6.5f, iconRect.getY() + 2.5f, 2.0f, 1.5f);
    g.fillRect(iconRect.getX() + 4.5f, iconRect.getY() + 5.0f, 3.0f, 1.5f);

    g.setFont(theme_.getValueFont(9.0f));
    g.setColour(qwertyActive_ ? juce::Colours::white : theme_.textSecondary);
    g.drawText(qwertyActive_ ? "QWERTY: ON" : "QWERTY: OFF", r.toNearestInt(), juce::Justification::centred, false);
}

void StandaloneKeyboardBar::drawPerformanceWheels(juce::Graphics& g, juce::Rectangle<int> area) {
    auto labelRow = area.removeFromBottom(11);
    auto tracksArea = area.reduced(2, 1);
    const int wheelW = (tracksArea.getWidth() - 6) / 2;
    pitchWheelRect_ = juce::Rectangle<int>(tracksArea.getX(), tracksArea.getY(), wheelW, tracksArea.getHeight());
    swellWheelRect_ = juce::Rectangle<int>(tracksArea.getRight() - wheelW, tracksArea.getY(), wheelW, tracksArea.getHeight());

    auto drawWheel = [&](juce::Rectangle<int> rect, float normPos, bool bipolar, bool activeDrag, juce::Colour accentCol) {
        const auto rf = rect.toFloat();
        g.setColour(juce::Colour(0xff090c12));
        g.fillRoundedRectangle(rf, 3.0f);
        g.setColour(activeDrag ? accentCol : theme_.borderSubtle.withAlpha(0.75f));
        g.drawRoundedRectangle(rf, 3.0f, activeDrag ? 1.2f : 0.8f);

        g.setColour(theme_.borderSubtle.withAlpha(0.25f));
        for (int i = 1; i <= 5; ++i) {
            const float ry = rf.getY() + rf.getHeight() * (static_cast<float>(i) / 6.0f);
            g.drawHorizontalLine(static_cast<int>(ry), rf.getX() + 3.0f, rf.getRight() - 3.0f);
        }

        const float clampedNorm = std::clamp(normPos, 0.0f, 1.0f);
        const float thumbY = rf.getBottom() - 4.0f - clampedNorm * (rf.getHeight() - 8.0f);

        if (bipolar) {
            const float midY = rf.getCentreY();
            const float fillTop = std::min(midY, thumbY);
            const float fillH = std::abs(thumbY - midY);
            if (fillH > 0.5f) {
                g.setColour(accentCol.withAlpha(0.35f));
                g.fillRect(rf.getX() + 2.0f, fillTop, rf.getWidth() - 4.0f, fillH);
            }
        } else {
            const float fillH = rf.getBottom() - 2.0f - thumbY;
            if (fillH > 0.5f) {
                g.setColour(accentCol.withAlpha(0.38f));
                g.fillRoundedRectangle(rf.getX() + 2.0f, thumbY, rf.getWidth() - 4.0f, fillH, 2.0f);
            }
        }

        const juce::Rectangle<float> cap(rf.getX() + 1.5f, thumbY - 3.0f, rf.getWidth() - 3.0f, 6.0f);
        g.setColour(activeDrag ? juce::Colours::white : accentCol);
        g.fillRoundedRectangle(cap, 2.0f);
    };

    const float pbNorm = std::clamp((proc_.getPitchBendSemitones() / 2.0f) * 0.5f + 0.5f, 0.0f, 1.0f);
    const float mwNorm = std::clamp(proc_.getModWheelValue(), 0.0f, 1.0f);

    drawWheel(pitchWheelRect_, pbNorm, true,  isDraggingPitchWheel_, theme_.accentPrimary);
    drawWheel(swellWheelRect_, mwNorm, false, isDraggingSwellWheel_, juce::Colour(0xff2dd4bf));

    g.setFont(theme_.getValueFont(7.8f));
    g.setColour(theme_.textSecondary);
    g.drawText("PIT", juce::Rectangle<int>(pitchWheelRect_.getX() - 2, labelRow.getY(), pitchWheelRect_.getWidth() + 4, labelRow.getHeight()),
               juce::Justification::centred, false);
    g.setColour(mwNorm > 0.01f ? juce::Colour(0xff2dd4bf) : theme_.textSecondary);
    g.drawText("SWL", juce::Rectangle<int>(swellWheelRect_.getX() - 2, labelRow.getY(), swellWheelRect_.getWidth() + 4, labelRow.getHeight()),
               juce::Justification::centred, false);
}

void StandaloneKeyboardBar::drawVirtualKeyboard(juce::Graphics& g, juce::Rectangle<int> area) {
    rebuildKeyGeometry(area);

    for (std::size_t i = 0; i < whiteKeyRects_.size(); ++i) {
        const auto r = whiteKeyRects_[i].toFloat();
        const int note = whiteKeyNotes_[i];
        const bool isDown = (activeMidiNote_ == note + octaveShift_ * 12);
        const bool isMiddleC = (note == 60);
        g.setColour(isDown ? theme_.accentPrimary.withAlpha(0.40f)
                           : (isMiddleC ? juce::Colour(0xff1f2a3a) : juce::Colour(0xff18202c)));
        g.fillRoundedRectangle(r, 2.0f);
        g.setColour(isDown ? theme_.accentPrimary : theme_.borderSubtle.withAlpha(0.6f));
        g.drawRoundedRectangle(r, 2.0f, 0.8f);
    }

    for (std::size_t i = 0; i < blackKeyRects_.size(); ++i) {
        const auto r = blackKeyRects_[i].toFloat();
        const int note = blackKeyNotes_[i];
        const bool isDown = (activeMidiNote_ == note + octaveShift_ * 12);
        g.setColour(isDown ? juce::Colour(0xff2dd4bf) : juce::Colour(0xff090c12));
        g.fillRoundedRectangle(r, 2.0f);
        g.setColour(isDown ? juce::Colours::white : theme_.borderSubtle);
        g.drawRoundedRectangle(r, 2.0f, 0.8f);
    }
}

void StandaloneKeyboardBar::mouseDown(const juce::MouseEvent& e) {
    const auto pos = e.getPosition();
    if (pitchWheelRect_.contains(pos)) {
        isDraggingPitchWheel_ = true;
        if (e.getNumberOfClicks() >= 2) {
            proc_.setPitchBendFromUI(0.0f);
        } else {
            updatePitchWheelFromY(pos.y);
        }
        repaint();
        return;
    }
    if (swellWheelRect_.contains(pos)) {
        isDraggingSwellWheel_ = true;
        if (e.getNumberOfClicks() >= 2) {
            proc_.setModWheelFromUI(0.0f);
        } else {
            updateSwellWheelFromY(pos.y);
        }
        repaint();
        return;
    }
    handleMouse(e, true);
}

void StandaloneKeyboardBar::mouseDrag(const juce::MouseEvent& e) {
    if (isDraggingPitchWheel_) {
        updatePitchWheelFromY(e.getPosition().y);
        repaint();
        return;
    }
    if (isDraggingSwellWheel_) {
        updateSwellWheelFromY(e.getPosition().y);
        repaint();
        return;
    }
    if (isDraggingVelocity_) {
        const float norm = std::clamp(static_cast<float>(e.getPosition().x - velRect_.getX()) / static_cast<float>(velRect_.getWidth()), 0.05f, 1.0f);
        auditionVelocity_ = norm;
        repaint();
        return;
    }
    handleMouse(e, true);
}

void StandaloneKeyboardBar::mouseUp(const juce::MouseEvent& e) {
    if (isDraggingPitchWheel_) {
        isDraggingPitchWheel_ = false;
        proc_.setPitchBendFromUI(0.0f); // Hardware spring-return to center!
        repaint();
        return;
    }
    if (isDraggingSwellWheel_) {
        isDraggingSwellWheel_ = false;
        repaint();
        return;
    }
    isDraggingVelocity_ = false;
    handleMouse(e, false);
}

void StandaloneKeyboardBar::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    if (swellWheelRect_.contains(e.getPosition())) {
        const float cur = proc_.getModWheelValue();
        proc_.setModWheelFromUI(std::clamp(cur + wheel.deltaY * 0.25f, 0.0f, 1.0f));
        repaint();
    }
}

void StandaloneKeyboardBar::handleMouse(const juce::MouseEvent& e, bool isDown) {
    const auto pos = e.getPosition();

    if (isDown) {
        if (octDownRect_.contains(pos)) {
            octaveShift_ = std::clamp(octaveShift_ - 1, -2, 2);
            repaint();
            return;
        }
        if (octUpRect_.contains(pos)) {
            octaveShift_ = std::clamp(octaveShift_ + 1, -2, 2);
            repaint();
            return;
        }
        if (holdBtnRect_.contains(pos)) {
            isHoldActive_ = !isHoldActive_;
            if (!isHoldActive_) proc_.triggerAuditionNoteOff(activeMidiNote_);
            repaint();
            return;
        }

        for (std::size_t i = 0; i < chordRects_.size(); ++i) {
            if (chordRects_[i].contains(pos)) {
                triggerChord(static_cast<int>(i));
                return;
            }
        }

        if (vkToggleRect_.contains(pos)) {
            qwertyActive_ = !qwertyActive_;
            if (qwertyActive_) {
                grabKeyboardFocus();
            }
            repaint();
            return;
        }
        if (velRect_.contains(pos)) {
            isDraggingVelocity_ = true;
            const float norm = std::clamp(static_cast<float>(pos.x - velRect_.getX()) / static_cast<float>(velRect_.getWidth()), 0.05f, 1.0f);
            auditionVelocity_ = norm;
            repaint();
            return;
        }
    } else {
        if (!isHoldActive_) {
            for (std::size_t i = 0; i < chordHeld_.size(); ++i) {
                if (chordHeld_[i]) {
                    releaseChord(static_cast<int>(i));
                }
            }
        }
    }

    if (!keyboardArea_.contains(pos)) {
        if (!isDown && activeMidiNote_ >= 0 && !isHoldActive_) {
            proc_.triggerAuditionNoteOff(activeMidiNote_);
            activeMidiNote_ = -1;
            repaint();
        }
        return;
    }

    int hitNote = -1;
    for (std::size_t i = 0; i < blackKeyRects_.size(); ++i) {
        if (blackKeyRects_[i].contains(pos)) {
            hitNote = blackKeyNotes_[i] + octaveShift_ * 12;
            break;
        }
    }
    if (hitNote < 0) {
        for (std::size_t i = 0; i < whiteKeyRects_.size(); ++i) {
            if (whiteKeyRects_[i].contains(pos)) {
                hitNote = whiteKeyNotes_[i] + octaveShift_ * 12;
                break;
            }
        }
    }

    if (isDown) {
        if (hitNote != activeMidiNote_) {
            if (activeMidiNote_ >= 0 && !isHoldActive_) {
                proc_.triggerAuditionNoteOff(activeMidiNote_);
            }
            activeMidiNote_ = hitNote;
            if (activeMidiNote_ >= 0) {
                proc_.triggerAuditionNoteOn(activeMidiNote_, auditionVelocity_);
            }
            repaint();
        }
    } else {
        if (activeMidiNote_ >= 0 && !isHoldActive_) {
            proc_.triggerAuditionNoteOff(activeMidiNote_);
            activeMidiNote_ = -1;
            repaint();
        }
    }
}

} // namespace nova::synth
