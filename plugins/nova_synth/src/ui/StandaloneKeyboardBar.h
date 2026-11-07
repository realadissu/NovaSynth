#pragma once

#include "nova_ui/NovaTheme.h"
#include "../NovaSynthProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <vector>

namespace nova::synth {

class StandaloneKeyboardBar : public juce::Component {
public:
    StandaloneKeyboardBar(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc), theme_(theme) {
        setOpaque(false);
        setWantsKeyboardFocus(true);
    }

    [[nodiscard]] bool isQwertyActive() const noexcept { return qwertyActive_; }
    void setQwertyActive(bool active) noexcept { qwertyActive_ = active; repaint(); }
    [[nodiscard]] float getAuditionVelocity() const noexcept { return auditionVelocity_; }
    void setAuditionVelocity(float vel) noexcept { auditionVelocity_ = std::clamp(vel, 0.05f, 1.0f); repaint(); }

    [[nodiscard]] int getVisibleWhiteKeyCount() const noexcept { return static_cast<int>(whiteKeyRects_.size()); }
    [[nodiscard]] float getWhiteKeyWidth() const noexcept { return currentWhiteKeyWidth_; }
    [[nodiscard]] juce::Rectangle<int> getPitchWheelBounds() const noexcept { return pitchWheelRect_; }
    [[nodiscard]] juce::Rectangle<int> getSwellWheelBounds() const noexcept { return swellWheelRect_; }

    void syncFromTelemetry(float modWheelVal, float pitchBendVal) noexcept {
        if (!isDraggingSwellWheel_ && std::abs(modWheelVal - lastSyncedModWheel_) > 0.003f) {
            lastSyncedModWheel_ = modWheelVal;
            repaint();
        }
        if (!isDraggingPitchWheel_ && std::abs(pitchBendVal - lastSyncedPitchBend_) > 0.01f) {
            lastSyncedPitchBend_ = pitchBendVal;
            repaint();
        }
    }

    void resized() override {
        updateLayoutRects();
    }

    void paint(juce::Graphics& g) override {
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

    void mouseDown(const juce::MouseEvent& e) override {
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

    void mouseDrag(const juce::MouseEvent& e) override {
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

    void mouseUp(const juce::MouseEvent& e) override {
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

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        if (swellWheelRect_.contains(e.getPosition())) {
            const float cur = proc_.getModWheelValue();
            proc_.setModWheelFromUI(std::clamp(cur + wheel.deltaY * 0.25f, 0.0f, 1.0f));
            repaint();
        }
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (!qwertyActive_) return false;
        const int code = key.getKeyCode();
        int noteOffset = -1;
        switch (std::toupper(code)) {
            case 'A': noteOffset = 0; break;  // C
            case 'W': noteOffset = 1; break;  // C#
            case 'S': noteOffset = 2; break;  // D
            case 'E': noteOffset = 3; break;  // D#
            case 'D': noteOffset = 4; break;  // E
            case 'F': noteOffset = 5; break;  // F
            case 'T': noteOffset = 6; break;  // F#
            case 'G': noteOffset = 7; break;  // G
            case 'Y':
            case 'Z': noteOffset = 8; break;  // G#
            case 'H': noteOffset = 9; break;  // A
            case 'U': noteOffset = 10; break; // A#
            case 'J': noteOffset = 11; break; // B
            case 'K': noteOffset = 12; break; // C next octave
            case 'O': noteOffset = 13; break; // C#
            case 'L': noteOffset = 14; break; // D
            case 'P': noteOffset = 15; break; // D#
            default: break;
        }

        if (noteOffset >= 0) {
            const int targetNote = 60 + noteOffset + octaveShift_ * 12;
            if (activeMidiNote_ != targetNote) {
                if (activeMidiNote_ >= 0 && !isHoldActive_) {
                    proc_.triggerAuditionNoteOff(activeMidiNote_);
                }
                activeMidiNote_ = targetNote;
                proc_.triggerAuditionNoteOn(activeMidiNote_, auditionVelocity_);
                repaint();
            }
            return true;
        }
        return false;
    }

    bool keyStateChanged(bool isKeyDown) override {
        if (!isKeyDown && activeMidiNote_ >= 0 && !isHoldActive_ && qwertyActive_) {
            proc_.triggerAuditionNoteOff(activeMidiNote_);
            activeMidiNote_ = -1;
            repaint();
            return true;
        }
        return false;
    }

private:
    void drawLeftControls(juce::Graphics& g, juce::Rectangle<int> area) {
        // Octave & Hold controls in top row
        auto topRow = area.removeFromTop(20);
        octDownRect_ = topRow.removeFromLeft(22);
        octDispRect_ = topRow.removeFromLeft(48);
        octUpRect_   = topRow.removeFromLeft(22);
        topRow.removeFromLeft(8);
        holdBtnRect_ = topRow.removeFromLeft(50);

        // Octave - / +
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

    void drawRightControls(juce::Graphics& g, juce::Rectangle<int> area) {
        // Velocity Slider readout & toggle
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

        // Crisp vector keyboard icon
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

    void updatePitchWheelFromY(int mouseY) noexcept {
        if (pitchWheelRect_.getHeight() <= 4) return;
        const float norm = 1.0f - std::clamp(
            static_cast<float>(mouseY - pitchWheelRect_.getY()) / static_cast<float>(pitchWheelRect_.getHeight()),
            0.0f, 1.0f);
        const float semitones = (norm * 2.0f - 1.0f) * 2.0f; // -2.0 .. +2.0 semitones
        proc_.setPitchBendFromUI(semitones);
    }

    void updateSwellWheelFromY(int mouseY) noexcept {
        if (swellWheelRect_.getHeight() <= 4) return;
        const float norm = 1.0f - std::clamp(
            static_cast<float>(mouseY - swellWheelRect_.getY()) / static_cast<float>(swellWheelRect_.getHeight()),
            0.0f, 1.0f);
        proc_.setModWheelFromUI(norm);
    }

    void drawPerformanceWheels(juce::Graphics& g, juce::Rectangle<int> area) {
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

            // Subtle grip ridges inside wheel slot
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

            // Hardware wheel cap / thumb notch
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

    void updateLayoutRects() {
        auto area = getLocalBounds().reduced(6, 4);
        area.removeFromLeft(316);
        auto wheelsArea = area.removeFromLeft(58);
        auto labelRow = wheelsArea.removeFromBottom(11);
        juce::ignoreUnused(labelRow);
        auto tracksArea = wheelsArea.reduced(2, 1);
        const int wheelW = (tracksArea.getWidth() - 6) / 2;
        pitchWheelRect_ = juce::Rectangle<int>(tracksArea.getX(), tracksArea.getY(), wheelW, tracksArea.getHeight());
        swellWheelRect_ = juce::Rectangle<int>(tracksArea.getRight() - wheelW, tracksArea.getY(), wheelW, tracksArea.getHeight());

        area.removeFromRight(136);
        keyboardArea_ = area.reduced(4, 2);
        rebuildKeyGeometry(keyboardArea_);
    }

    void rebuildKeyGeometry(juce::Rectangle<int> area) {
        whiteKeyRects_.clear();
        whiteKeyNotes_.clear();
        blackKeyRects_.clear();
        blackKeyNotes_.clear();

        if (area.getWidth() <= 20 || area.getHeight() <= 8) return;

        // Build all 52 natural (white) MIDI notes of a full 88-key grand piano (A0 = 21 .. C8 = 108)
        static constexpr std::array<int, 52> kAllPianoNaturals = {
            21, 23,
            24, 26, 28, 29, 31, 33, 35,
            36, 38, 40, 41, 43, 45, 47,
            48, 50, 52, 53, 55, 57, 59,
            60, 62, 64, 65, 67, 69, 71,
            72, 74, 76, 77, 79, 81, 83,
            84, 86, 88, 89, 91, 93, 95,
            96, 98, 100, 101, 103, 105, 107,
            108
        };

        // Keep white key width constant (~14.0 px) so widening the window reveals more keys up to 88 keys
        constexpr float kFixedWhiteKeyWidth = 14.0f;
        currentWhiteKeyWidth_ = kFixedWhiteKeyWidth;

        const int fittingNaturals = std::clamp(
            static_cast<int>(std::floor(static_cast<float>(area.getWidth()) / kFixedWhiteKeyWidth)),
            15, 52);

        // At 36 naturals (standard 61-key C2..C7), startNatIdx = 9 (C2=36), endNatIdx = 44 (C7=96).
        // As fittingNaturals grows toward 52, expand symmetrically outward toward A0 (idx 0) and C8 (idx 51).
        const int extraKeys = fittingNaturals - 36;
        int startNatIdx = 9 - (extraKeys / 2);
        int endNatIdx = startNatIdx + fittingNaturals - 1;
        if (startNatIdx < 0) {
            endNatIdx -= startNatIdx;
            startNatIdx = 0;
        }
        if (endNatIdx > 51) {
            startNatIdx -= (endNatIdx - 51);
            endNatIdx = 51;
        }
        startNatIdx = std::clamp(startNatIdx, 0, 51);
        endNatIdx   = std::clamp(endNatIdx, startNatIdx, 51);

        const int numNaturals = endNatIdx - startNatIdx + 1;
        const float totalKbWidth = static_cast<float>(numNaturals) * kFixedWhiteKeyWidth;
        const float startX = static_cast<float>(area.getX()) + std::max(0.0f, (static_cast<float>(area.getWidth()) - totalKbWidth) * 0.5f);

        const int firstMidiNote = kAllPianoNaturals[static_cast<std::size_t>(startNatIdx)];
        const int lastMidiNote  = kAllPianoNaturals[static_cast<std::size_t>(endNatIdx)];

        const float wkW = kFixedWhiteKeyWidth;
        const float wkH = static_cast<float>(area.getHeight());
        const float bkH = wkH * 0.62f;
        const float bkW = wkW * 0.65f;

        int natIdx = 0;
        for (int note = firstMidiNote; note <= lastMidiNote; ++note) {
            const int noteInOct = note % 12;
            const bool isBlack = (noteInOct == 1 || noteInOct == 3 || noteInOct == 6 || noteInOct == 8 || noteInOct == 10);
            if (!isBlack) {
                const auto r = juce::Rectangle<float>(startX + static_cast<float>(natIdx) * wkW,
                                                      static_cast<float>(area.getY()), wkW - 1.0f, wkH);
                whiteKeyRects_.push_back(r.toNearestInt());
                whiteKeyNotes_.push_back(note);
                natIdx++;
            }
        }

        natIdx = 0;
        for (int note = firstMidiNote; note < lastMidiNote; ++note) {
            const int noteInOct = note % 12;
            const bool isBlack = (noteInOct == 1 || noteInOct == 3 || noteInOct == 6 || noteInOct == 8 || noteInOct == 10);
            if (!isBlack) {
                natIdx++;
            } else {
                const float centerX = startX + static_cast<float>(natIdx) * wkW;
                const auto r = juce::Rectangle<float>(centerX - bkW * 0.5f,
                                                      static_cast<float>(area.getY()), bkW, bkH);
                blackKeyRects_.push_back(r.toNearestInt());
                blackKeyNotes_.push_back(note);
            }
        }
    }

    void drawVirtualKeyboard(juce::Graphics& g, juce::Rectangle<int> area) {
        rebuildKeyGeometry(area);

        // Draw White Keys first
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

        // Draw Black Keys on top
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

    void handleMouse(const juce::MouseEvent& e, bool isDown) {
        const auto pos = e.getPosition();

        // Octave buttons
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

            // Chord Buttons
            for (std::size_t i = 0; i < chordRects_.size(); ++i) {
                if (chordRects_[i].contains(pos)) {
                    triggerChord(static_cast<int>(i));
                    return;
                }
            }

            // QWERTY Key Toggle & Velocity readout
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
            // Mouse release: turn off chords unless hold active
            if (!isHoldActive_) {
                for (std::size_t i = 0; i < chordHeld_.size(); ++i) {
                    if (chordHeld_[i]) {
                        releaseChord(static_cast<int>(i));
                    }
                }
            }
        }

        // Virtual Keyboard Keys
        if (!keyboardArea_.contains(pos)) {
            if (!isDown && activeMidiNote_ >= 0 && !isHoldActive_) {
                proc_.triggerAuditionNoteOff(activeMidiNote_);
                activeMidiNote_ = -1;
                repaint();
            }
            return;
        }

        int hitNote = -1;
        // Check black keys first (they sit on top)
        for (std::size_t i = 0; i < blackKeyRects_.size(); ++i) {
            if (blackKeyRects_[i].contains(pos)) {
                hitNote = blackKeyNotes_[i] + octaveShift_ * 12;
                break;
            }
        }
        // Then white keys
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


    void triggerChord(int chordIdx) {
        releaseAllChords();
        chordHeld_[static_cast<std::size_t>(chordIdx)] = true;
        const auto notes = getChordNotes(chordIdx);
        for (int n : notes) {
            proc_.triggerAuditionNoteOn(n + octaveShift_ * 12, auditionVelocity_);
        }
        repaint();
    }

    void releaseChord(int chordIdx) {
        chordHeld_[static_cast<std::size_t>(chordIdx)] = false;
        const auto notes = getChordNotes(chordIdx);
        for (int n : notes) {
            proc_.triggerAuditionNoteOff(n + octaveShift_ * 12);
        }
        repaint();
    }

    void releaseAllChords() {
        for (std::size_t i = 0; i < chordHeld_.size(); ++i) {
            if (chordHeld_[i]) releaseChord(static_cast<int>(i));
        }
    }

    static std::vector<int> getChordNotes(int chordIdx) {
        switch (chordIdx) {
            case 0: return { 48 }; // C3 Solo Note!
            case 1: return { 48, 55, 59, 62, 64 }; // C Maj9 (C3, G3, B3, D4, E4)
            case 2: return { 41, 53, 57, 60, 64 }; // F Maj7 (F2, F3, A3, C4, E4)
            case 3: return { 45, 52, 57, 60, 64 }; // Am7 (A2, E3, A3, C4, E4)
            case 4: return { 43, 55, 57, 62, 67 }; // G6 Sus (G2, G3, A3, D4, G4)
            case 5: return { 50, 57, 60, 64, 65 }; // Dm9 (D3, A3, C4, E4, F4)
            case 6: return { 40, 52, 55, 59, 62 }; // Em7 (E2, E3, G3, B3, D4)
            default: return { 48 };
        }
    }

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int octaveShift_ = 0;
    bool isHoldActive_ = false;
    bool qwertyActive_ = false;
    float auditionVelocity_ = 0.85f;
    bool isDraggingVelocity_ = false;
    bool isDraggingPitchWheel_ = false;
    bool isDraggingSwellWheel_ = false;
    float lastSyncedModWheel_ = 0.0f;
    float lastSyncedPitchBend_ = 0.0f;
    float currentWhiteKeyWidth_ = 14.0f;
    int activeMidiNote_ = -1;

    juce::Rectangle<int> octDownRect_;
    juce::Rectangle<int> octDispRect_;
    juce::Rectangle<int> octUpRect_;
    juce::Rectangle<int> holdBtnRect_;
    juce::Rectangle<int> pitchWheelRect_;
    juce::Rectangle<int> swellWheelRect_;
    juce::Rectangle<int> velRect_;
    juce::Rectangle<int> vkToggleRect_;
    juce::Rectangle<int> keyboardArea_;

    std::vector<juce::Rectangle<int>> chordRects_;
    std::array<bool, 7> chordHeld_{};

    std::vector<juce::Rectangle<int>> whiteKeyRects_;
    std::vector<int> whiteKeyNotes_;
    std::vector<juce::Rectangle<int>> blackKeyRects_;
    std::vector<int> blackKeyNotes_;
};

} // namespace nova::synth
