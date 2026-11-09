#include "JornaLowerWorkbench.h"
#include "../NovaSynthProcessor.h"
#include <algorithm>
#include <array>

namespace nova::synth {

JornaLowerWorkbench::JornaLowerWorkbench(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc),
      theme_(theme),
      signalFlowPanel_(proc, theme),
      modulationHubPanel_(proc, theme),
      fxRackPanel_(proc, theme),
      droneHubPanel_(proc, theme),
      arpeggiatorHubPanel_(proc, theme),
      mixerHubPanel_(proc, theme) {
    signalFlowPanel_.onSelectOsc = [this](int idx) { if (onSelectOsc) onSelectOsc(idx); };
    signalFlowPanel_.onSourceStateChanged = [this]() { if (onSourceStateChanged) onSourceStateChanged(); };
    signalFlowPanel_.onFilterStateChanged = [this]() { if (onFilterStateChanged) onFilterStateChanged(); };
    addChildComponent(signalFlowPanel_);
    addChildComponent(modulationHubPanel_);
    addChildComponent(fxRackPanel_);
    addChildComponent(droneHubPanel_);
    addChildComponent(arpeggiatorHubPanel_);
    addChildComponent(mixerHubPanel_);
    selectWorkbenchTab(0); // Default to FLOW signal architecture view!
}

void JornaLowerWorkbench::selectWorkbenchTab(int idx) {
    activeTab_ = std::clamp(idx, 0, 5);
    signalFlowPanel_.setVisible(activeTab_ == 0);
    modulationHubPanel_.setVisible(activeTab_ == 1);
    fxRackPanel_.setVisible(activeTab_ == 2);
    droneHubPanel_.setVisible(activeTab_ == 3);
    arpeggiatorHubPanel_.setVisible(activeTab_ == 4);
    mixerHubPanel_.setVisible(activeTab_ == 5);
    repaint();
}

void JornaLowerWorkbench::setSelectedOscIndex(int idx) {
    selectedOscIndex_ = idx;
    signalFlowPanel_.setSelectedOscIndex(idx);
    repaint();
}

void JornaLowerWorkbench::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(theme_.bgSurface);
    g.fillRoundedRectangle(bounds, theme_.cornerRadius);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(bounds, theme_.cornerRadius, 1.0f);

    // Top Workbench Tab Bar: FLOW | MODULATION | FX RACK | DRONE | ARPEGGIATOR | MIXER
    static constexpr std::array<const char*, 6> kTabs = {
        "FLOW", "MODULATION", "FX RACK", "DRONE", "ARPEGGIATOR", "MIXER"
    };
    tabRects_.clear();
    const int tabW = (getWidth() - 40) / 6;
    for (int i = 0; i < 6; ++i) {
        const juce::Rectangle<int> tr(20 + i * tabW, 6, tabW - 8, 22);
        tabRects_.push_back(tr);
        const bool isAct = (activeTab_ == i);
        if (isAct) {
            g.setColour(juce::Colour(0xff2dd4bf).withAlpha(0.22f));
            g.fillRoundedRectangle(tr.toFloat(), 4.0f);
            g.setColour(juce::Colour(0xff2dd4bf));
            g.drawRoundedRectangle(tr.toFloat(), 4.0f, 1.2f);
        }
        g.setFont(theme_.getSectionFont(13.0f));
        g.setColour(isAct ? juce::Colour(0xff2dd4bf) : theme_.textSecondary);
        g.drawText(kTabs[static_cast<std::size_t>(i)], tr, juce::Justification::centred);
    }

    phase_ += 0.06f;
}

void JornaLowerWorkbench::resized() {
    const auto content = getLocalBounds().withTrimmedTop(32).reduced(10, 6);
    signalFlowPanel_.setBounds(content);
    modulationHubPanel_.setBounds(content);
    fxRackPanel_.setBounds(content);
    droneHubPanel_.setBounds(content);
    arpeggiatorHubPanel_.setBounds(content);
    mixerHubPanel_.setBounds(content);
}

void JornaLowerWorkbench::mouseDown(const juce::MouseEvent& event) {
    const auto pos = event.getPosition();
    for (int i = 0; i < static_cast<int>(tabRects_.size()); ++i) {
        if (tabRects_[static_cast<std::size_t>(i)].contains(pos)) {
            if (i == 5) {
                if (onOpenMixerDrawer) {
                    onOpenMixerDrawer();
                }
                return;
            }
            selectWorkbenchTab(i);
            return;
        }
    }
}

} // namespace nova::synth

