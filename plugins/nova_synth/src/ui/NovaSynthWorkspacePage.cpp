#include "NovaSynthWorkspacePage.h"
#include "../NovaSynthProcessor.h"
#include <algorithm>

namespace nova::synth {

NovaSynthWorkspacePage::NovaSynthWorkspacePage(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc),
      sourceList_(proc, theme),
      visualizerHub_(proc, theme),
      sourceDeck_(proc, theme),
      filterDeck_(proc, theme),
      envDeck_(proc, theme),
      workbench_(proc, theme),
      keyboardBar_(proc, theme) {
    addAndMakeVisible(sourceList_);
    addAndMakeVisible(visualizerHub_);
    addAndMakeVisible(sourceDeck_);
    addAndMakeVisible(filterDeck_);
    addAndMakeVisible(envDeck_);
    addAndMakeVisible(workbench_);
    addAndMakeVisible(keyboardBar_);

    workbench_.onOpenMixerDrawer = [this]() {
        if (onOpenMixerDrawer) {
            onOpenMixerDrawer();
        }
    };

    sourceList_.onSourceSelected = [this](int slotIdx) {
        const auto params = proc_.getCurrentParameters();
        if (slotIdx >= 0 && slotIdx < 8 && !params.oscSlots[static_cast<std::size_t>(slotIdx)].enabled) {
            const juce::String prefix = "osc" + juce::String(slotIdx + 1) + "_";
            if (auto* p = proc_.getAPVTS().getParameter(prefix + "enabled")) {
                p->setValueNotifyingHost(1.0f);
            }
            sourceDeck_.refreshCards();
        }
        sourceDeck_.scrollToSlot(slotIdx);
        workbench_.setSelectedOscIndex(slotIdx);
    };

    auto notifyAllSourceViews = [this]() {
        if (isSyncingSourceViews_) return;
        isSyncingSourceViews_ = true;
        sourceList_.repaint();
        sourceDeck_.refreshCards();
        workbench_.repaint();
        isSyncingSourceViews_ = false;
    };

    sourceList_.onSourceStateChanged = notifyAllSourceViews;
    sourceDeck_.onSourceStateChanged = notifyAllSourceViews;
    workbench_.onSourceStateChanged  = notifyAllSourceViews;

    filterDeck_.onFilterStateChanged = [this]() {
        workbench_.repaint();
    };
    workbench_.onFilterStateChanged = [this]() {
        filterDeck_.repaint();
    };
}

void NovaSynthWorkspacePage::syncSourceStates() {
    if (isSyncingSourceViews_) return;
    isSyncingSourceViews_ = true;
    sourceList_.repaint();
    sourceDeck_.refreshCards();
    workbench_.repaint();
    filterDeck_.repaint();
    isSyncingSourceViews_ = false;
}

void NovaSynthWorkspacePage::setInitialWorkbenchTab(int tabIdx) {
    workbench_.selectWorkbenchTab(tabIdx);
}

void NovaSynthWorkspacePage::updateModulationTelemetry(const nova::dsp::WorshipSynthEngine::Telemetry& tel) {
    filterDeck_.updateModulationTelemetry(tel);
    keyboardBar_.syncFromTelemetry(tel.modWheelValue, tel.pitchBendValue);
}

void NovaSynthWorkspacePage::resized() {
    auto area = getLocalBounds().reduced(8, 6);

    // Zone 8: Bottom Standalone Keyboard (62px)
    keyboardBar_.setBounds(area.removeFromBottom(62));
    area.removeFromBottom(6);

    // Zone 7: Modular Workbench (220px) - Displays 6 sources in Flow before scrolling
    workbench_.setBounds(area.removeFromBottom(220));
    area.removeFromBottom(6);

    // Zone 5 & 6: Dual Filter Deck & Envelope Quad Deck (168px)
    auto midArea = area.removeFromBottom(168);
    area.removeFromBottom(6);

    const int filterW = static_cast<int>(static_cast<float>(midArea.getWidth()) * 0.44f);
    filterDeck_.setBounds(midArea.removeFromLeft(filterW));
    midArea.removeFromLeft(6);
    envDeck_.setBounds(midArea);

    // Zone 2 (left column 160px) & Zone 3 (visualizer hub) + Zone 4 (source deck)
    sourceList_.setBounds(area.removeFromLeft(160));
    area.removeFromLeft(6);

    auto upperRight = area;
    const int visualizerH = std::clamp(static_cast<int>(static_cast<float>(upperRight.getHeight()) * 0.36f), 108, 122);
    visualizerHub_.setBounds(upperRight.removeFromTop(visualizerH));
    upperRight.removeFromTop(6);
    sourceDeck_.setBounds(upperRight);
}

} // namespace nova::synth
