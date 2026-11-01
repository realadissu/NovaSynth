#pragma once

#include "nova_ui/NovaTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>
#include <functional>

namespace nova::ui {

/**
 * @brief JORNA-inspired Multi-Slot Section Deck.
 * Displays a section title and a row of compact slot tabs (e.g. "ACOUSTIC ENGINE | LUSH CLOUD" or
 * "OSC 1 | OSC 2 | [+]") at the top of the panel, and renders the active slot's parameter deck directly below.
 */
class SectionDeck : public juce::Component {
public:
    SectionDeck(const juce::String& sectionTitle,
                const NovaTheme& theme,
                bool allowDynamicAdd = false);
    ~SectionDeck() override = default;

    int addSlot(const juce::String& slotTitle,
                std::unique_ptr<juce::Component> contentComponent,
                bool useSecondaryAccent = false);

    void selectSlot(int index, bool sendNotification = true);
    [[nodiscard]] int getSelectedSlotIndex() const noexcept { return activeSlotIndex_; }
    [[nodiscard]] int getNumSlots() const noexcept { return static_cast<int>(slots_.size()); }

    void setSlotMiniPreviewValues(int index, const std::vector<float>& normalizedCurve);

    std::function<void(int)> onSlotSelected;
    std::function<void()> onAddSlotRequested;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    struct SlotEntry {
        juce::String title;
        std::unique_ptr<juce::Component> content;
        bool useSecondaryAccent = false;
        juce::Rectangle<int> tabBounds;
        std::vector<float> miniPreview;
    };

    juce::String sectionTitle_;
    NovaTheme theme_;
    bool allowDynamicAdd_ = false;
    std::vector<SlotEntry> slots_;
    int activeSlotIndex_ = 0;
    juce::Rectangle<int> addBtnBounds_;
};

} // namespace nova::ui
