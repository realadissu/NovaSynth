#pragma once

#include "SourceCardComponent.h"
#include "HorizontalViewport.h"
#include "nova_ui/NovaTheme.h"
#include "../NovaSynthProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>
#include <functional>

namespace nova::synth {

// ==============================================================================
// AddSourceCardComponent: Eigenständige [+ Add Source] Karte
// ==============================================================================
class AddSourceCardComponent : public juce::Component {
public:
    std::function<void()> onAddClicked;

    explicit AddSourceCardComponent(const nova::ui::NovaTheme& theme);
    ~AddSourceCardComponent() override = default;

    void setCount(int activeCount);
    void paint(juce::Graphics& g) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    const nova::ui::NovaTheme& theme_;
    int activeCount_ = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AddSourceCardComponent)
};

// ==============================================================================
// SourceDeck: Horizontal scrollbarer Viewport mit weichem Accordion-Effekt
// ==============================================================================
class SourceDeck : public juce::Component, public juce::Timer {
public:
    std::function<void()> onSourceStateChanged;

    SourceDeck(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~SourceDeck() override;

    const std::vector<std::unique_ptr<SourceCardComponent>>& getCardList() const noexcept {
        return cards_;
    }

    void handleCardExpand(int slotIdx, bool shouldExpand);
    void handleCardPin(int slotIdx, bool isPinned);
    void collapseUnpinnedCards();
    void scrollToSlot(int slotIndex);
    void refreshCards();
    void timerCallback() override;
    void resized() override;
    void autoScrollToCard(int slotIdx);

private:
    void updateDeckLayout();

    class DeckContentComponent : public juce::Component {
    public:
        explicit DeckContentComponent(SourceDeck& owner);
        ~DeckContentComponent() override = default;

        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
        void mouseDown(const juce::MouseEvent& e) override;

    private:
        SourceDeck& owner_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckContentComponent)
    };

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    HorizontalViewport viewport_;
    DeckContentComponent deckContent_{*this};

    std::vector<std::unique_ptr<SourceCardComponent>> cards_;
    AddSourceCardComponent addCard_;

    int lastActiveCount_ = 1;
    uint64_t lastStateHash_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SourceDeck)
};

} // namespace nova::synth
