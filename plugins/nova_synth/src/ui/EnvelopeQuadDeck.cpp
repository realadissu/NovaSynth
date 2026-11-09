#include "EnvelopeQuadDeck.h"
#include <algorithm>
#include <cmath>

namespace nova::synth {

EnvelopeQuadDeck::EnvelopeQuadDeck(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc),
      theme_(theme),
      deckContent_(*this),
      ampEnv_(proc, theme, 0, "ENV 1: AMP", juce::Colour(0xff38bdf8), "amp"),
      fltEnv_(proc, theme, 1, "ENV 2: FILTER", juce::Colour(0xffa855f7), "flt"),
      modEnv_(proc, theme, 2, "ENV 3: MOD", juce::Colour(0xff2dd4bf), "mod"),
      auxEnv_(proc, theme, 3, "ENV 4: AUX", juce::Colour(0xfff59e0b), "aux")
{
    viewport_.setViewedComponent(&deckContent_, false);
    addAndMakeVisible(viewport_);

    deckContent_.addAndMakeVisible(ampEnv_);
    deckContent_.addAndMakeVisible(fltEnv_);
    deckContent_.addAndMakeVisible(modEnv_);
    deckContent_.addAndMakeVisible(auxEnv_);

    setupCallbacks(ampEnv_);
    setupCallbacks(fltEnv_);
    setupCallbacks(modEnv_);
    setupCallbacks(auxEnv_);

    // All envelopes start collapsed in sleek compact overview mode on launch
    ampEnv_.setExpanded(false);
    fltEnv_.setExpanded(false);
    modEnv_.setExpanded(false);
    auxEnv_.setExpanded(false);

    startTimerHz(60);
}

EnvelopeQuadDeck::~EnvelopeQuadDeck() {
    stopTimer();
}

std::array<ExpandableEnvelopeCard*, 4> EnvelopeQuadDeck::getCardList() noexcept {
    return { &ampEnv_, &fltEnv_, &modEnv_, &auxEnv_ };
}

void EnvelopeQuadDeck::setupCallbacks(ExpandableEnvelopeCard& card) {
    card.onExpandRequested = [this](int envIdx, bool exp) {
        handleCardExpand(envIdx, exp);
    };
    card.onPinToggled = [this](int envIdx, bool pin) {
        handleCardPin(envIdx, pin);
    };
}

void EnvelopeQuadDeck::handleCardExpand(int envIdx, bool shouldExpand) {
    auto cards = getCardList();
    for (std::size_t i = 0; i < 4; ++i) {
        if (static_cast<int>(i) == envIdx) {
            cards[i]->setExpanded(shouldExpand);
        } else if (!cards[i]->isPinned()) {
            cards[i]->setExpanded(false);
        }
    }
    if (shouldExpand) {
        autoScrollToCard(envIdx);
    }
}

void EnvelopeQuadDeck::handleCardPin(int envIdx, bool isPinned) {
    auto cards = getCardList();
    if (envIdx >= 0 && envIdx < 4) {
        cards[static_cast<std::size_t>(envIdx)]->setPinned(isPinned);
        if (isPinned) {
            cards[static_cast<std::size_t>(envIdx)]->setExpanded(true);
            autoScrollToCard(envIdx);
        } else {
            cards[static_cast<std::size_t>(envIdx)]->setExpanded(false);
        }
    }
}

void EnvelopeQuadDeck::collapseUnpinnedCards() {
    auto cards = getCardList();
    for (auto* card : cards) {
        if (card->isVisible() && !card->isPinned()) {
            card->setExpanded(false);
        }
    }
}

void EnvelopeQuadDeck::autoScrollToCard(int envIdx) {
    auto cards = getCardList();
    if (envIdx >= 0 && envIdx < 4) {
        const int cardX = cards[static_cast<std::size_t>(envIdx)]->getX();
        const int targetW = static_cast<int>(cards[static_cast<std::size_t>(envIdx)]->isExpanded() ? 390.0f : 130.0f);
        const int viewX = viewport_.getViewPositionX();
        const int viewW = viewport_.getViewWidth();

        if (cardX < viewX) {
            viewport_.setViewPosition(std::max(0, cardX - 10), 0);
        } else if (cardX + targetW > viewX + viewW) {
            const int maxX = std::max(0, deckContent_.getWidth() - viewW);
            viewport_.setViewPosition(std::clamp(cardX + targetW - viewW + 10, 0, maxX), 0);
        }
    }
}

void EnvelopeQuadDeck::timerCallback() {
    auto cards = getCardList();
    bool needsLayout = false;

    const int availableW = viewport_.getWidth() - 14;
    int numExpanded = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        if (cards[i]->isExpanded()) numExpanded++;
    }

    const float expTarget = 390.0f;
    float compactTarget = 130.0f;

    if (numExpanded == 0) {
        compactTarget = std::max(130.0f, (static_cast<float>(availableW) - 18.0f) / 4.0f);
    } else {
        const int numCompact = 4 - numExpanded;
        if (numCompact > 0) {
            const float remW = static_cast<float>(availableW) - (static_cast<float>(numExpanded) * expTarget) - 18.0f;
            // Compact cards retain at least 130px, sliding out to the right instead of squishing!
            compactTarget = std::max(130.0f, remW / static_cast<float>(numCompact));
        }
    }

    for (std::size_t i = 0; i < 4; ++i) {
        const float target = cards[i]->isExpanded() ? expTarget : compactTarget;
        const float diff = target - cards[i]->currentWidth_;
        if (std::abs(diff) > 0.5f) {
            cards[i]->currentWidth_ += diff * 0.28f;
            needsLayout = true;
        } else {
            cards[i]->currentWidth_ = target;
        }
    }

    if (needsLayout) {
        updateDeckLayout();
    }
}

void EnvelopeQuadDeck::resized() {
    viewport_.setBounds(getLocalBounds());
    updateDeckLayout();
}

void EnvelopeQuadDeck::updateDeckLayout() {
    auto cards = getCardList();
    const int cardH = getHeight() - 14;
    int currentX = 4;

    for (std::size_t i = 0; i < 4; ++i) {
        const int w = static_cast<int>(std::round(cards[i]->currentWidth_));
        cards[i]->setBounds(currentX, 2, w, cardH);
        currentX += w + 6;
    }

    const int targetW = std::max(getWidth(), currentX + 8);
    const int targetH = getHeight() - 6;
    if (deckContent_.getWidth() != targetW || deckContent_.getHeight() != targetH) {
        deckContent_.setSize(targetW, targetH);
    }
    deckContent_.repaint();
}

EnvelopeQuadDeck::DeckContentComponent::DeckContentComponent(EnvelopeQuadDeck& owner)
    : owner_(owner)
{
}

void EnvelopeQuadDeck::DeckContentComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    owner_.viewport_.mouseWheelMove(e, wheel);
}

void EnvelopeQuadDeck::DeckContentComponent::mouseDown(const juce::MouseEvent& e) {
    if (e.eventComponent == this) {
        auto cards = owner_.getCardList();
        for (auto* card : cards) {
            if (card->isVisible() && !card->isPinned()) {
                card->setExpanded(false);
            }
        }
    }
}

} // namespace nova::synth

