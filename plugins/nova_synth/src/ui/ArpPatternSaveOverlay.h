#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <nova_ui/NovaTheme.h>

namespace nova::synth {
class NovaSynthProcessor;
}

namespace nova::synth::ui {

class ArpeggiatorHubPanel;

/**
 * @brief Modal Save / Overwrite Dialog Overlay for Arp Rhythm Presets
 */
class ArpPatternSaveOverlay : public juce::Component {
public:
    enum class Mode {
        InputName,
        ConfirmOverwrite
    };

    ArpPatternSaveOverlay(ArpeggiatorHubPanel& owner, NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~ArpPatternSaveOverlay() override = default;

    void startSave(const juce::String& defaultName);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    void handleSaveAttempt();
    void handleCancel();
    void switchToInputMode();
    void performSave(const juce::String& name);

    ArpeggiatorHubPanel& owner_;
    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    Mode mode_ = Mode::InputName;
    juce::String pendingName_;
    juce::Rectangle<int> dialogBoxRect_;

    juce::Label titleLabel_;
    juce::Label promptLabel_;
    juce::TextEditor nameEditor_;
    juce::TextButton cancelBtn_;
    juce::TextButton saveBtn_;
    juce::TextButton overwriteBtn_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpPatternSaveOverlay)
};

} // namespace nova::synth::ui
