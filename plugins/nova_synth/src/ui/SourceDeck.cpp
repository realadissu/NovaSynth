#include "SourceDeck.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nova::synth {

// ==============================================================================
// AddSourceCardComponent
// ==============================================================================
AddSourceCardComponent::AddSourceCardComponent(const nova::ui::NovaTheme& theme)
    : theme_(theme)
{
}

void AddSourceCardComponent::setCount(int activeCount) {
    if (activeCount_ != activeCount) {
        activeCount_ = activeCount;
        repaint();
    }
}

void AddSourceCardComponent::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    // Hintergrund mit dezentem Hover-Highlight
    g.setColour(isMouseOver() ? theme_.bgControl.brighter(0.08f) : theme_.bgControl.withAlpha(0.35f));
    g.fillRoundedRectangle(bounds, theme_.cornerRadius);

    // Gestrichelter Rand
    g.setColour(isMouseOver() ? theme_.accentPrimary : theme_.accentPrimary.withAlpha(0.45f));
    g.drawRoundedRectangle(bounds, theme_.cornerRadius, 1.2f);

    // Plus Icon
    g.setFont(theme_.getTitleFont(26.0f));
    g.setColour(isMouseOver() ? juce::Colours::white : theme_.accentPrimary);
    g.drawText("+", getLocalBounds().reduced(0, 30), juce::Justification::centred, false);

    // Text
    g.setFont(theme_.getValueFont(9.5f));
    g.setColour(isMouseOver() ? theme_.textPrimary : theme_.textMuted);
    g.drawFittedText("Add Source (" + juce::String(activeCount_) + "/8)",
                     getLocalBounds().removeFromBottom(28), juce::Justification::centred, 1, 0.85f);
}

void AddSourceCardComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    if (auto* vp = findParentComponentOfClass<juce::Viewport>()) {
        vp->mouseWheelMove(e, wheel);
    }
}

void AddSourceCardComponent::mouseEnter(const juce::MouseEvent&) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    repaint();
}

void AddSourceCardComponent::mouseExit(const juce::MouseEvent&) {
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void AddSourceCardComponent::mouseDown(const juce::MouseEvent&) {
    if (onAddClicked) {
        onAddClicked();
    }
}

// ==============================================================================
// SourceDeck::DeckContentComponent
// ==============================================================================
SourceDeck::DeckContentComponent::DeckContentComponent(SourceDeck& owner)
    : owner_(owner)
{
}

void SourceDeck::DeckContentComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    owner_.viewport_.mouseWheelMove(e, wheel);
}

void SourceDeck::DeckContentComponent::mouseDown(const juce::MouseEvent& e) {
    if (e.eventComponent == this) {
        for (auto& card : owner_.cards_) {
            if (card->isVisible() && !card->isPinned()) {
                card->setExpanded(false);
            }
        }
    }
}

// ==============================================================================
// SourceDeck
// ==============================================================================
SourceDeck::SourceDeck(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc), theme_(theme), addCard_(theme)
{
    viewport_.setViewedComponent(&deckContent_, false);
    addAndMakeVisible(viewport_);

    for (int i = 0; i < 8; ++i) {
        auto card = std::make_unique<SourceCardComponent>(proc_, theme_, i);
        card->onCardStateChanged = [this](int /*slot*/) {
            refreshCards();
            if (onSourceStateChanged) onSourceStateChanged();
        };
        card->onExpandRequested = [this](int slotIdx, bool shouldExpand) {
            handleCardExpand(slotIdx, shouldExpand);
        };
        card->onPinToggled = [this](int slotIdx, bool isPinned) {
            handleCardPin(slotIdx, isPinned);
        };
        deckContent_.addChildComponent(*card);
        cards_.push_back(std::move(card));
    }

    addCard_.onAddClicked = [this]() {
        auto& apvts = proc_.getAPVTS();
        const auto params = proc_.getCurrentParameters();
        for (int i = 0; i < 8; ++i) {
            if (!params.oscSlots[static_cast<std::size_t>(i)].enabled) {
                const juce::String prefix = "osc" + juce::String(i + 1) + "_";
                if (auto* p = apvts.getParameter(prefix + "enabled")) {
                    p->setValueNotifyingHost(1.0f);
                }
                if (auto* pCount = apvts.getParameter("activeOscCount")) {
                    const int nextCount = std::max(proc_.getCurrentParameters().activeOscCount, i + 1);
                    pCount->setValueNotifyingHost(pCount->convertTo0to1(static_cast<float>(nextCount)));
                }
                proc_.syncParametersFromAPVTS();
                refreshCards();
                handleCardExpand(i, true);
                if (onSourceStateChanged) onSourceStateChanged();
                return;
            }
        }
    };
    deckContent_.addChildComponent(addCard_);

    // All sources start collapsed in sleek square compact overview mode on launch
    for (auto& card : cards_) {
        card->setExpanded(false);
    }

    refreshCards();
    startTimerHz(60);
}

SourceDeck::~SourceDeck() {
    stopTimer();
}

void SourceDeck::handleCardExpand(int slotIdx, bool shouldExpand) {
    for (int i = 0; i < 8; ++i) {
        if (i == slotIdx) {
            cards_[static_cast<std::size_t>(i)]->setExpanded(shouldExpand);
        } else if (!cards_[static_cast<std::size_t>(i)]->isPinned()) {
            cards_[static_cast<std::size_t>(i)]->setExpanded(false);
        }
    }
    autoScrollToCard(slotIdx);
}

void SourceDeck::handleCardPin(int slotIdx, bool isPinned) {
    cards_[static_cast<std::size_t>(slotIdx)]->setPinned(isPinned);
    if (isPinned) {
        cards_[static_cast<std::size_t>(slotIdx)]->setExpanded(true);
        autoScrollToCard(slotIdx);
    } else {
        cards_[static_cast<std::size_t>(slotIdx)]->setExpanded(false);
    }
}

void SourceDeck::collapseUnpinnedCards() {
    for (auto& card : cards_) {
        if (card->isVisible() && !card->isPinned()) {
            card->setExpanded(false);
        }
    }
}

void SourceDeck::scrollToSlot(int slotIndex) {
    if (slotIndex >= 0 && slotIndex < 8) {
        handleCardExpand(slotIndex, true);
    }
}

void SourceDeck::refreshCards() {
    const auto params = proc_.getCurrentParameters();
    for (int i = 0; i < 8; ++i) {
        const bool enabled = params.oscSlots[static_cast<std::size_t>(i)].enabled;
        cards_[static_cast<std::size_t>(i)]->setVisible(enabled);
        if (enabled) {
            cards_[static_cast<std::size_t>(i)]->updateDynamicLabels();
            cards_[static_cast<std::size_t>(i)]->updateControlVisibility();
            cards_[static_cast<std::size_t>(i)]->resized();
            cards_[static_cast<std::size_t>(i)]->repaint();
        }
    }
    updateDeckLayout();
}

void SourceDeck::timerCallback() {
    const auto params = proc_.getCurrentParameters();
    int activeCount = 0;
    uint64_t stateHash = 0;
    for (int i = 0; i < 8; ++i) {
        const auto& s = params.oscSlots[static_cast<std::size_t>(i)];
        if (s.enabled) {
            activeCount++;
            stateHash |= (1ULL << i);
        }
        stateHash ^= (static_cast<uint64_t>(s.engine) << (i * 6));
        stateHash ^= (static_cast<uint64_t>(s.sourceType) << (i * 6 + 3));
        stateHash ^= (static_cast<uint64_t>(s.modelIndex) << (i * 6 + 5));
    }

    if (stateHash != lastStateHash_) {
        lastStateHash_ = stateHash;
        lastActiveCount_ = activeCount;
        refreshCards();
        if (onSourceStateChanged) onSourceStateChanged();
    }

    // 60Hz Tweening der Karten-Breiten
    const int cardH = getHeight() - 14;
    const float compactTarget = static_cast<float>(cardH); // Quadratische Karten im kompakten Zustand

    bool needsLayout = false;
    for (auto& card : cards_) {
        if (card->isVisible()) {
            const float target = card->isExpanded() ? card->getPreferredExpandedWidth() : compactTarget;
            const float diff = target - card->currentWidth_;
            if (std::abs(diff) > 0.5f) {
                card->currentWidth_ += diff * 0.28f;
                needsLayout = true;
            } else {
                card->currentWidth_ = target;
            }
        }
    }

    if (needsLayout) {
        updateDeckLayout();
    }
}

void SourceDeck::resized() {
    viewport_.setBounds(getLocalBounds());
    updateDeckLayout();
}

void SourceDeck::autoScrollToCard(int slotIdx) {
    if (slotIdx >= 0 && slotIdx < 8 && cards_[static_cast<std::size_t>(slotIdx)]->isVisible()) {
        const int cardX = cards_[static_cast<std::size_t>(slotIdx)]->getX();
        const int targetW = static_cast<int>(cards_[static_cast<std::size_t>(slotIdx)]->isExpanded()
            ? cards_[static_cast<std::size_t>(slotIdx)]->getPreferredExpandedWidth()
            : static_cast<float>(getHeight() - 14));
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

void SourceDeck::updateDeckLayout() {
    const auto params = proc_.getCurrentParameters();
    int activeCount = 0;
    for (int i = 0; i < 8; ++i) {
        if (params.oscSlots[static_cast<std::size_t>(i)].enabled) activeCount++;
    }
    lastActiveCount_ = activeCount;
    addCard_.setCount(activeCount);

    const int cardH = getHeight() - 14;
    int currentX = 4;

    for (int i = 0; i < 8; ++i) {
        if (params.oscSlots[static_cast<std::size_t>(i)].enabled) {
            const int w = static_cast<int>(std::round(cards_[static_cast<std::size_t>(i)]->currentWidth_));
            cards_[static_cast<std::size_t>(i)]->setBounds(currentX, 2, w, cardH);
            currentX += w + 6;
        }
    }

    // "+ Add Source" Karte
    if (activeCount < 8) {
        addCard_.setVisible(true);
        addCard_.setBounds(currentX, 2, 105, cardH);
        currentX += 105 + 6;
    } else {
        addCard_.setVisible(false);
    }

    const int targetW = std::max(getWidth(), currentX + 10);
    const int targetH = getHeight() - 6;
    if (deckContent_.getWidth() != targetW || deckContent_.getHeight() != targetH) {
        deckContent_.setSize(targetW, targetH);
    }
    deckContent_.repaint();
}

} // namespace nova::synth

