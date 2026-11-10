#pragma once

#include "nova_ui/NovaTheme.h"
#include "../NovaSynthProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <vector>

namespace nova::synth {

class StandaloneKeyboardBar : public juce::Component {
public:
    StandaloneKeyboardBar(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~StandaloneKeyboardBar() override = default;

    [[nodiscard]] bool isQwertyActive() const noexcept;
    void setQwertyActive(bool active) noexcept;
    [[nodiscard]] float getAuditionVelocity() const noexcept;
    void setAuditionVelocity(float vel) noexcept;

    [[nodiscard]] int getVisibleWhiteKeyCount() const noexcept;
    [[nodiscard]] float getWhiteKeyWidth() const noexcept;
    [[nodiscard]] juce::Rectangle<int> getPitchWheelBounds() const noexcept;
    [[nodiscard]] juce::Rectangle<int> getSwellWheelBounds() const noexcept;

    void syncFromTelemetry(float modWheelVal, float pitchBendVal) noexcept;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;

private:
    void drawLeftControls(juce::Graphics& g, juce::Rectangle<int> area);
    void drawRightControls(juce::Graphics& g, juce::Rectangle<int> area);
    void updatePitchWheelFromY(int mouseY) noexcept;
    void updateSwellWheelFromY(int mouseY) noexcept;
    void drawPerformanceWheels(juce::Graphics& g, juce::Rectangle<int> area);
    void updateLayoutRects();
    void rebuildKeyGeometry(juce::Rectangle<int> area);
    void drawVirtualKeyboard(juce::Graphics& g, juce::Rectangle<int> area);
    void handleMouse(const juce::MouseEvent& e, bool isDown);
    void triggerChord(int chordIdx);
    void releaseChord(int chordIdx);
    void releaseAllChords();
    static std::vector<int> getChordNotes(int chordIdx);

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
