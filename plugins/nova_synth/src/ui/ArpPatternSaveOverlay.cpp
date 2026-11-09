#include "ArpPatternSaveOverlay.h"
#include "ArpeggiatorHubPanel.h"
#include "../NovaSynthProcessor.h"

namespace nova::synth::ui {

ArpPatternSaveOverlay::ArpPatternSaveOverlay(ArpeggiatorHubPanel& owner, NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
    : owner_(owner), proc_(proc), theme_(theme) {
    titleLabel_.setFont(theme_.getSectionFont(12.0f));
    titleLabel_.setJustificationType(juce::Justification::centred);
    titleLabel_.setColour(juce::Label::textColourId, theme_.accentPrimary);
    addAndMakeVisible(titleLabel_);

    promptLabel_.setFont(theme_.getLabelFont(11.0f));
    promptLabel_.setJustificationType(juce::Justification::centred);
    promptLabel_.setColour(juce::Label::textColourId, theme_.textPrimary);
    addAndMakeVisible(promptLabel_);

    nameEditor_.setMultiLine(false);
    nameEditor_.setReturnKeyStartsNewLine(false);
    nameEditor_.setFont(theme_.getValueFont(12.0f));
    nameEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff090d16));
    nameEditor_.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    nameEditor_.setColour(juce::TextEditor::outlineColourId, theme_.accentPrimary.withAlpha(0.6f));
    nameEditor_.setColour(juce::TextEditor::focusedOutlineColourId, theme_.accentPrimary);
    nameEditor_.setColour(juce::TextEditor::highlightColourId, theme_.accentPrimary.withAlpha(0.35f));
    nameEditor_.onReturnKey = [this]() { handleSaveAttempt(); };
    nameEditor_.onEscapeKey = [this]() { handleCancel(); };
    addAndMakeVisible(nameEditor_);

    cancelBtn_.setButtonText("CANCEL");
    cancelBtn_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    cancelBtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffcbd5e1));
    cancelBtn_.onClick = [this]() { handleCancel(); };
    addAndMakeVisible(cancelBtn_);

    saveBtn_.setButtonText("SAVE");
    saveBtn_.setColour(juce::TextButton::buttonColourId, theme_.accentPrimary);
    saveBtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff090d16));
    saveBtn_.onClick = [this]() { handleSaveAttempt(); };
    addAndMakeVisible(saveBtn_);

    overwriteBtn_.setButtonText("OVERWRITE");
    overwriteBtn_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff59e0b));
    overwriteBtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff090d16));
    overwriteBtn_.onClick = [this]() { performSave(pendingName_); };
    addChildComponent(overwriteBtn_);
}

void ArpPatternSaveOverlay::startSave(const juce::String& defaultName) {
    mode_ = Mode::InputName;
    pendingName_ = defaultName;
    titleLabel_.setText("SAVE RHYTHM PRESET", juce::dontSendNotification);
    promptLabel_.setText("Enter a name for this rhythm matrix pattern:", juce::dontSendNotification);
    nameEditor_.setText(defaultName, false);
    nameEditor_.selectAll();
    nameEditor_.setVisible(true);
    saveBtn_.setVisible(true);
    overwriteBtn_.setVisible(false);
    cancelBtn_.setVisible(true);
    resized();
    repaint();
    nameEditor_.grabKeyboardFocus();
}

void ArpPatternSaveOverlay::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xd0000000));

    const auto r = dialogBoxRect_.toFloat();
    const auto grad = juce::ColourGradient::vertical(
        juce::Colour(0xff141c2c), r.getY(),
        juce::Colour(0xff090e18), r.getBottom());
    g.setGradientFill(grad);
    g.fillRoundedRectangle(r, 8.0f);

    g.setColour(mode_ == Mode::ConfirmOverwrite ? juce::Colour(0xfff59e0b).withAlpha(0.85f)
                                                : theme_.accentPrimary.withAlpha(0.85f));
    g.drawRoundedRectangle(r, 8.0f, 1.5f);
}

void ArpPatternSaveOverlay::resized() {
    const int boxW = 380;
    const int boxH = (mode_ == Mode::ConfirmOverwrite) ? 170 : 160;
    dialogBoxRect_ = juce::Rectangle<int>(
        (getWidth() - boxW) / 2,
        std::max(10, (getHeight() - boxH) / 2),
        boxW,
        boxH);

    auto inner = dialogBoxRect_.reduced(16, 14);
    titleLabel_.setBounds(inner.removeFromTop(20));
    inner.removeFromTop(8);

    if (mode_ == Mode::InputName) {
        promptLabel_.setBounds(inner.removeFromTop(18));
        inner.removeFromTop(8);
        nameEditor_.setBounds(inner.removeFromTop(26).reduced(24, 0));
        inner.removeFromTop(12);

        auto btnRow = inner.removeFromTop(26).reduced(40, 0);
        const int btnW = (btnRow.getWidth() - 12) / 2;
        cancelBtn_.setBounds(btnRow.removeFromLeft(btnW));
        saveBtn_.setBounds(btnRow.removeFromRight(btnW));
    } else {
        promptLabel_.setBounds(inner.removeFromTop(44));
        inner.removeFromTop(12);

        auto btnRow = inner.removeFromTop(26).reduced(30, 0);
        const int btnW = (btnRow.getWidth() - 12) / 2;
        cancelBtn_.setBounds(btnRow.removeFromLeft(btnW));
        overwriteBtn_.setBounds(btnRow.removeFromRight(btnW));
    }
}

void ArpPatternSaveOverlay::mouseDown(const juce::MouseEvent& event) {
    if (!dialogBoxRect_.contains(event.getPosition())) {
        if (mode_ == Mode::ConfirmOverwrite) {
            switchToInputMode();
        } else {
            handleCancel();
        }
    }
}

bool ArpPatternSaveOverlay::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey) {
        handleCancel();
        return true;
    }
    if (key == juce::KeyPress::returnKey) {
        if (mode_ == Mode::ConfirmOverwrite) {
            performSave(pendingName_);
        } else {
            handleSaveAttempt();
        }
        return true;
    }
    return false;
}

void ArpPatternSaveOverlay::handleSaveAttempt() {
    const juce::String entered = nameEditor_.getText().trim();
    if (entered.isEmpty()) {
        return;
    }
    pendingName_ = entered;

    const auto presets = proc_.getAvailableArpRhythmPresets();
    bool exists = false;
    for (const auto& p : presets) {
        if (p.name.equalsIgnoreCase(pendingName_)) {
            exists = true;
            break;
        }
    }

    if (exists) {
        mode_ = Mode::ConfirmOverwrite;
        titleLabel_.setText("OVERWRITE PRESET?", juce::dontSendNotification);
        promptLabel_.setText("A preset named \"" + pendingName_ + "\" already exists.\nDo you want to overwrite it?",
                             juce::dontSendNotification);
        nameEditor_.setVisible(false);
        saveBtn_.setVisible(false);
        overwriteBtn_.setVisible(true);
        cancelBtn_.setVisible(true);
        resized();
        repaint();
    } else {
        performSave(pendingName_);
    }
}

void ArpPatternSaveOverlay::handleCancel() {
    if (mode_ == Mode::ConfirmOverwrite) {
        switchToInputMode();
    } else {
        setVisible(false);
    }
}

void ArpPatternSaveOverlay::switchToInputMode() {
    mode_ = Mode::InputName;
    titleLabel_.setText("SAVE RHYTHM PRESET", juce::dontSendNotification);
    promptLabel_.setText("Enter a name for this rhythm matrix pattern:", juce::dontSendNotification);
    nameEditor_.setText(pendingName_, false);
    nameEditor_.selectAll();
    nameEditor_.setVisible(true);
    saveBtn_.setVisible(true);
    overwriteBtn_.setVisible(false);
    cancelBtn_.setVisible(true);
    resized();
    repaint();
    nameEditor_.grabKeyboardFocus();
}

void ArpPatternSaveOverlay::performSave(const juce::String& name) {
    const int len = proc_.getArpPatternLength();
    std::vector<bool> gates(static_cast<std::size_t>(len));
    std::vector<float> vels(static_cast<std::size_t>(len));
    for (int i = 0; i < len; ++i) {
        gates[static_cast<std::size_t>(i)] = proc_.isArpStepGateActive(i);
        vels[static_cast<std::size_t>(i)] = proc_.getArpStepVelocity(i);
    }
    proc_.saveUserArpRhythmPreset(name, len, gates, vels);
    owner_.onPresetSaved(name);
    setVisible(false);
}

} // namespace nova::synth::ui

