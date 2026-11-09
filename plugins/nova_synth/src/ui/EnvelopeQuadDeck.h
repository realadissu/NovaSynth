#pragma once

#include "ExpandableEnvelopeCard.h"
#include "HorizontalViewport.h"
#include "nova_ui/NovaTheme.h"
#include "../NovaSynthProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace nova::synth {

// ==============================================================================
// EnvelopeQuadDeck: Horizontal scrollbarer Viewport mit weichem Accordion-Effekt
// ==============================================================================
class EnvelopeQuadDeck : public juce::Component, public juce::Timer {
public:
    EnvelopeQuadDeck(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~EnvelopeQuadDeck() override;

    std::array<ExpandableEnvelopeCard*, 4> getCardList() noexcept;

    void setupCallbacks(ExpandableEnvelopeCard& card);
    void handleCardExpand(int envIdx, bool shouldExpand);
    void handleCardPin(int envIdx, bool isPinned);
    void collapseUnpinnedCards();
    void autoScrollToCard(int envIdx);
    void timerCallback() override;
    void resized() override;

private:
    void updateDeckLayout();

    class DeckContentComponent : public juce::Component {
    public:
        explicit DeckContentComponent(EnvelopeQuadDeck& owner);
        ~DeckContentComponent() override = default;

        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
        void mouseDown(const juce::MouseEvent& e) override;

    private:
        EnvelopeQuadDeck& owner_;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckContentComponent)
    };

    [[maybe_unused]] NovaSynthProcessor& proc_;
    [[maybe_unused]] const nova::ui::NovaTheme& theme_;
    HorizontalViewport viewport_;
    DeckContentComponent deckContent_{*this};

    ExpandableEnvelopeCard ampEnv_;
    ExpandableEnvelopeCard fltEnv_;
    ExpandableEnvelopeCard modEnv_;
    ExpandableEnvelopeCard auxEnv_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeQuadDeck)
};

} // namespace nova::synth
