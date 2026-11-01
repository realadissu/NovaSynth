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
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(theme_.bgHeader.darker(0.3f));
        g.fillRoundedRectangle(bounds, theme_.cornerRadius);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(bounds, theme_.cornerRadius, 1.0f);

        auto area = getLocalBounds().reduced(6, 4);

        // Left section: Worship Chords & Octave Controls
        auto leftControls = area.removeFromLeft(330);
        drawLeftControls(g, leftControls);

        // Right section: Velocity & Keyboard Mode
        auto rightControls = area.removeFromRight(140);
        drawRightControls(g, rightControls);

        // Center section: 61-Key Virtual Keyboard
        keyboardArea_ = area.reduced(4, 2);
        drawVirtualKeyboard(g, keyboardArea_);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        handleMouse(e, true);
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        handleMouse(e, true);
    }

    void mouseUp(const juce::MouseEvent& e) override {
        handleMouse(e, false);
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
        g.setFont(theme_.getValueFont(9.0f));
        g.setColour(theme_.textMuted);
        g.drawText("VELOCITY: 85%", topRow, juce::Justification::centredRight, false);

        auto botRow = area.removeFromBottom(22);
        vkToggleRect_ = botRow.reduced(4, 1);
        g.setColour(theme_.bgElevated);
        g.fillRoundedRectangle(vkToggleRect_.toFloat(), 3.0f);
        g.setColour(theme_.borderFocus.withAlpha(0.5f));
        g.drawRoundedRectangle(vkToggleRect_.toFloat(), 3.0f, 0.8f);
        g.setFont(theme_.getValueFont(9.0f));
        g.setColour(theme_.accentPrimary);
        g.drawText("\xe2\x8c\xa8 Virtual Keys", vkToggleRect_, juce::Justification::centred, false);
    }

    void drawVirtualKeyboard(juce::Graphics& g, juce::Rectangle<int> area) {
        // 61 Keys (C2 = 36 to C7 = 96)
        // 36 Naturals (white keys)
        whiteKeyRects_.clear();
        whiteKeyNotes_.clear();
        blackKeyRects_.clear();
        blackKeyNotes_.clear();

        const int numNaturals = 36;
        const float wkW = static_cast<float>(area.getWidth()) / static_cast<float>(numNaturals);
        const float wkH = static_cast<float>(area.getHeight());
        const float bkH = wkH * 0.62f;
        const float bkW = wkW * 0.65f;

        // Draw White Keys first
        int natIdx = 0;
        for (int note = 36; note <= 96; ++note) {
            const int noteInOct = note % 12;
            const bool isBlack = (noteInOct == 1 || noteInOct == 3 || noteInOct == 6 || noteInOct == 8 || noteInOct == 10);
            if (!isBlack) {
                const auto r = juce::Rectangle<float>(static_cast<float>(area.getX()) + static_cast<float>(natIdx) * wkW,
                                                      static_cast<float>(area.getY()), wkW - 1.0f, wkH);
                whiteKeyRects_.push_back(r.toNearestInt());
                whiteKeyNotes_.push_back(note);

                const bool isDown = (activeMidiNote_ == note);
                g.setColour(isDown ? theme_.accentPrimary.withAlpha(0.40f) : juce::Colour(0xff18202c));
                g.fillRoundedRectangle(r, 2.0f);
                g.setColour(isDown ? theme_.accentPrimary : theme_.borderSubtle.withAlpha(0.6f));
                g.drawRoundedRectangle(r, 2.0f, 0.8f);

                natIdx++;
            }
        }

        // Draw Black Keys on top
        natIdx = 0;
        for (int note = 36; note < 96; ++note) {
            const int noteInOct = note % 12;
            const bool isBlack = (noteInOct == 1 || noteInOct == 3 || noteInOct == 6 || noteInOct == 8 || noteInOct == 10);
            if (!isBlack) {
                natIdx++;
            } else {
                const float centerX = static_cast<float>(area.getX()) + static_cast<float>(natIdx) * wkW;
                const auto r = juce::Rectangle<float>(centerX - bkW * 0.5f,
                                                      static_cast<float>(area.getY()), bkW, bkH);
                blackKeyRects_.push_back(r.toNearestInt());
                blackKeyNotes_.push_back(note);

                const bool isDown = (activeMidiNote_ == note);
                g.setColour(isDown ? juce::Colour(0xff2dd4bf) : juce::Colour(0xff090c12));
                g.fillRoundedRectangle(r, 2.0f);
                g.setColour(isDown ? juce::Colours::white : theme_.borderSubtle);
                g.drawRoundedRectangle(r, 2.0f, 0.8f);
            }
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
                    proc_.triggerAuditionNoteOn(activeMidiNote_, 0.85f);
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
            proc_.triggerAuditionNoteOn(n + octaveShift_ * 12, 0.85f);
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
    int activeMidiNote_ = -1;

    juce::Rectangle<int> octDownRect_;
    juce::Rectangle<int> octDispRect_;
    juce::Rectangle<int> octUpRect_;
    juce::Rectangle<int> holdBtnRect_;
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
