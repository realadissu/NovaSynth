#pragma once

#include "SourceListManager.h"
#include "DualVisualizerHub.h"
#include "SourceDeck.h"
#include "DualFilterDeck.h"
#include "EnvelopeQuadDeck.h"
#include "JornaLowerWorkbench.h"
#include "StandaloneKeyboardBar.h"
#include "nova_dsp/synth/WorshipSynthEngine.h"
#include "nova_ui/NovaTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace nova::synth {

class NovaSynthProcessor;

class NovaSynthWorkspacePage : public juce::Component {
public:
    std::function<void()> onOpenMixerDrawer;

    NovaSynthWorkspacePage(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~NovaSynthWorkspacePage() override = default;

    void syncSourceStates();
    void setInitialWorkbenchTab(int tabIdx);
    void updateModulationTelemetry(const nova::dsp::WorshipSynthEngine::Telemetry& tel);

    void resized() override;

private:
    [[maybe_unused]] NovaSynthProcessor& proc_;
    SourceListManager sourceList_;
    DualVisualizerHub visualizerHub_;
    SourceDeck sourceDeck_;
    DualFilterDeck filterDeck_;
    EnvelopeQuadDeck envDeck_;
    JornaLowerWorkbench workbench_;
    StandaloneKeyboardBar keyboardBar_;
    bool isSyncingSourceViews_ = false;
};

} // namespace nova::synth
