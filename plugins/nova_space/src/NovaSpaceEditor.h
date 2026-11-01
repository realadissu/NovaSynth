#pragma once

#include "NovaSpaceProcessor.h"
#include "nova_ui/NovaTheme.h"
#include "nova_ui/MorningLookAndFeel.h"
#include "nova_ui/PluginShell.h"
#include "nova_ui/SectionDeck.h"
#include "nova_ui/ArcKnob.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace nova::space {

class NovaSpaceEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit NovaSpaceEditor(NovaSpaceProcessor& proc);
    ~NovaSpaceEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    void configureParentWindow();

    NovaSpaceProcessor& processor_;
    nova::ui::NovaTheme theme_;
    nova::ui::MorningLookAndFeel lookAndFeel_;
    nova::ui::PluginShell shell_;

    juce::Component::SafePointer<juce::DocumentWindow> parentDocWindow_;
    juce::Component::SafePointer<juce::Button> standaloneOptionsButton_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NovaSpaceEditor)
};

} // namespace nova::space
