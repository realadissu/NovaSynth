#pragma once

#include "NovaSynthProcessor.h"
#include "nova_ui/NovaTheme.h"
#include "nova_ui/MorningLookAndFeel.h"
#include "nova_ui/PluginShell.h"
#include "nova_ui/SectionDeck.h"
#include "nova_ui/ArcKnob.h"
#include "ui/MixerSlideOverDrawer.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace nova::synth {

class NovaSynthEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit NovaSynthEditor(NovaSynthProcessor& proc);
    ~NovaSynthEditor() override;

    void setInitialWorkbenchTab(int tabIdx);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    void configureParentWindow();

    NovaSynthProcessor& processor_;
    nova::ui::NovaTheme theme_;
    nova::ui::MorningLookAndFeel lookAndFeel_;
    nova::ui::PluginShell shell_;
    ui::MixerSlideOverDrawer mixerDrawer_{processor_, theme_};
    juce::TooltipWindow tooltipWindow_{this, 400};

    std::function<void(int)> onSetWorkbenchTab_;

    juce::Component::SafePointer<juce::DocumentWindow> parentDocWindow_;
    juce::Component::SafePointer<juce::Button> standaloneOptionsButton_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NovaSynthEditor)
};

} // namespace nova::synth
