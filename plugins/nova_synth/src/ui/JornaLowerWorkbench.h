#pragma once

#include "SignalFlowPanel.h"
#include "FxRackPanel.h"
#include "ModulationHubPanel.h"
#include "DroneHubPanel.h"
#include "ArpeggiatorHubPanel.h"
#include "MixerHubPanel.h"
#include "nova_ui/NovaTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

namespace nova::synth {

class NovaSynthProcessor;

class JornaLowerWorkbench : public juce::Component {
public:
    std::function<void(int)> onSelectOsc;
    std::function<void()> onOpenMixerDrawer;
    std::function<void()> onSourceStateChanged;
    std::function<void()> onFilterStateChanged;

    JornaLowerWorkbench(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~JornaLowerWorkbench() override = default;

    void selectWorkbenchTab(int idx);
    void setSelectedOscIndex(int idx);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    [[maybe_unused]] NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int activeTab_ = 0;
    int selectedOscIndex_ = 0;
    float phase_ = 0.0f;
    std::vector<juce::Rectangle<int>> tabRects_;

    SignalFlowPanel signalFlowPanel_;
    ui::ModulationHubPanel modulationHubPanel_;
    FxRackPanel fxRackPanel_;
    ui::DroneHubPanel droneHubPanel_;
    ui::ArpeggiatorHubPanel arpeggiatorHubPanel_;
    ui::MixerHubPanel mixerHubPanel_;
};

} // namespace nova::synth

