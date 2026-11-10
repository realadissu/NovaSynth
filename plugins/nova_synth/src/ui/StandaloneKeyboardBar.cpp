#include "StandaloneKeyboardBar.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace nova::synth {

StandaloneKeyboardBar::StandaloneKeyboardBar(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc), theme_(theme) {
    setOpaque(false);
    setWantsKeyboardFocus(true);
}

bool StandaloneKeyboardBar::isQwertyActive() const noexcept {
    return qwertyActive_;
}

void StandaloneKeyboardBar::setQwertyActive(bool active) noexcept {
    qwertyActive_ = active;
    repaint();
}

float StandaloneKeyboardBar::getAuditionVelocity() const noexcept {
    return auditionVelocity_;
}

void StandaloneKeyboardBar::setAuditionVelocity(float vel) noexcept {
    auditionVelocity_ = std::clamp(vel, 0.05f, 1.0f);
    repaint();
}

int StandaloneKeyboardBar::getVisibleWhiteKeyCount() const noexcept {
    return static_cast<int>(whiteKeyRects_.size());
}

float StandaloneKeyboardBar::getWhiteKeyWidth() const noexcept {
    return currentWhiteKeyWidth_;
}

juce::Rectangle<int> StandaloneKeyboardBar::getPitchWheelBounds() const noexcept {
    return pitchWheelRect_;
}

juce::Rectangle<int> StandaloneKeyboardBar::getSwellWheelBounds() const noexcept {
    return swellWheelRect_;
}

void StandaloneKeyboardBar::syncFromTelemetry(float modWheelVal, float pitchBendVal) noexcept {
    if (!isDraggingSwellWheel_ && std::abs(modWheelVal - lastSyncedModWheel_) > 0.003f) {
        lastSyncedModWheel_ = modWheelVal;
        repaint();
    }
    if (!isDraggingPitchWheel_ && std::abs(pitchBendVal - lastSyncedPitchBend_) > 0.01f) {
        lastSyncedPitchBend_ = pitchBendVal;
        repaint();
    }
}

void StandaloneKeyboardBar::updatePitchWheelFromY(int mouseY) noexcept {
    if (pitchWheelRect_.getHeight() <= 4) return;
    const float norm = 1.0f - std::clamp(
        static_cast<float>(mouseY - pitchWheelRect_.getY()) / static_cast<float>(pitchWheelRect_.getHeight()),
        0.0f, 1.0f);
    const float semitones = (norm * 2.0f - 1.0f) * 2.0f; // -2.0 .. +2.0 semitones
    proc_.setPitchBendFromUI(semitones);
}

void StandaloneKeyboardBar::updateSwellWheelFromY(int mouseY) noexcept {
    if (swellWheelRect_.getHeight() <= 4) return;
    const float norm = 1.0f - std::clamp(
        static_cast<float>(mouseY - swellWheelRect_.getY()) / static_cast<float>(swellWheelRect_.getHeight()),
        0.0f, 1.0f);
    proc_.setModWheelFromUI(norm);
}

void StandaloneKeyboardBar::resized() {
    updateLayoutRects();
}

void StandaloneKeyboardBar::updateLayoutRects() {
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

void StandaloneKeyboardBar::rebuildKeyGeometry(juce::Rectangle<int> area) {
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

bool StandaloneKeyboardBar::keyPressed(const juce::KeyPress& key) {
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

bool StandaloneKeyboardBar::keyStateChanged(bool isKeyDown) {
    if (!isKeyDown && activeMidiNote_ >= 0 && !isHoldActive_ && qwertyActive_) {
        proc_.triggerAuditionNoteOff(activeMidiNote_);
        activeMidiNote_ = -1;
        repaint();
        return true;
    }
    return false;
}

void StandaloneKeyboardBar::triggerChord(int chordIdx) {
    releaseAllChords();
    chordHeld_[static_cast<std::size_t>(chordIdx)] = true;
    const auto notes = getChordNotes(chordIdx);
    for (int n : notes) {
        proc_.triggerAuditionNoteOn(n + octaveShift_ * 12, auditionVelocity_);
    }
    repaint();
}

void StandaloneKeyboardBar::releaseChord(int chordIdx) {
    chordHeld_[static_cast<std::size_t>(chordIdx)] = false;
    const auto notes = getChordNotes(chordIdx);
    for (int n : notes) {
        proc_.triggerAuditionNoteOff(n + octaveShift_ * 12);
    }
    repaint();
}

void StandaloneKeyboardBar::releaseAllChords() {
    for (std::size_t i = 0; i < chordHeld_.size(); ++i) {
        if (chordHeld_[i]) releaseChord(static_cast<int>(i));
    }
}

std::vector<int> StandaloneKeyboardBar::getChordNotes(int chordIdx) {
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

} // namespace nova::synth
