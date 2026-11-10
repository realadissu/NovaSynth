#include "nova_ui/PluginShell.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace nova::ui {

void PluginShell::setupToolbarButtons() {
    addAndMakeVisible(prevPresetBtn_);
    addAndMakeVisible(nextPresetBtn_);
    addAndMakeVisible(presetPillBtn_);
    addChildComponent(patchesBtn_);
    addChildComponent(mixerBtn_);
    addAndMakeVisible(bpmBtn_);
    addAndMakeVisible(abSlotABtn_);
    addAndMakeVisible(abSlotBBtn_);
    addAndMakeVisible(abCopyBtn_);
    addAndMakeVisible(undoBtn_);
    addAndMakeVisible(redoBtn_);
    addAndMakeVisible(panicBtn_);
    addAndMakeVisible(uiSettingsBtn_);

    panicBtn_.setTooltip("Panic: Stop all sound & reset audio engine");
    panicBtn_.onClick = [this]() {
        panicBtn_.flash();
        processor_.triggerPanic();
    };

    prevPresetBtn_.onClick = [this]() {
        processor_.getPresetManager().loadPreviousPreset();
        refreshPresetDisplay();
    };
    nextPresetBtn_.onClick = [this]() {
        processor_.getPresetManager().loadNextPreset();
        refreshPresetDisplay();
    };
    presetPillBtn_.onClick = [this]() {
        openPresetPopupMenu();
    };
    patchesBtn_.onClick = [this]() {
        if (onPatchesClicked_) onPatchesClicked_();
    };
    mixerBtn_.onClick = [this]() {
        if (onMixerClicked_) onMixerClicked_();
    };
    bpmBtn_.onClick = [this]() {
        if (!processor_.isHostBpmSynced()) {
            juce::PopupMenu menu;
            static constexpr std::array<int, 9> kCommonTempos = { 60, 72, 80, 96, 110, 120, 128, 140, 160 };
            for (int t : kCommonTempos) {
                menu.addItem(t, juce::String(t) + " BPM", true, std::abs(processor_.getBpm() - t) < 0.5);
            }
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&bpmBtn_),
                [this](int res) {
                    if (res > 0) {
                        processor_.setBpm(static_cast<double>(res), false);
                        if (auto* p = processor_.getAPVTS().getParameter("bpm")) {
                            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(res)));
                        }
                    }
                });
        }
    };
    abSlotABtn_.onClick = [this]() {
        if (processor_.getPresetManager().isSlotBActive()) {
            processor_.getPresetManager().switchABState();
            updateABButtons();
        }
    };
    abSlotBBtn_.onClick = [this]() {
        if (!processor_.getPresetManager().isSlotBActive()) {
            processor_.getPresetManager().switchABState();
            updateABButtons();
        }
    };
    abCopyBtn_.onClick = [this]() {
        processor_.getPresetManager().copyActiveToOtherABState();
    };
    undoBtn_.onClick = [this]() {
        processor_.getUndoManager().undo();
    };
    redoBtn_.onClick = [this]() {
        processor_.getUndoManager().redo();
    };
    uiSettingsBtn_.onClick = [this]() {
        if (settingsDialog_ != nullptr && settingsDialog_->isVisible()) {
            settingsDialog_->setVisible(false);
            return;
        }
        if (settingsDialog_ == nullptr) {
            settingsDialog_ = std::make_unique<SettingsOverlayDialog>(
                processor_,
                theme_,
                [this](NovaTheme::TextScale s) {
                    if (onTextScaleChanged_ != nullptr) onTextScaleChanged_(s);
                    repaint();
                },
                onOpenAudioSettings_,
                [this]() {
                    if (settingsDialog_ != nullptr) settingsDialog_->setVisible(false);
                }
            );
            addChildComponent(settingsDialog_.get());
        }
        settingsDialog_->setBounds(getLocalBounds());
        settingsDialog_->refreshStates();
        settingsDialog_->toFront(true);
        settingsDialog_->setVisible(true);
    };

    abSlotABtn_.setTooltip("State A: Click to switch to State A");
    abSlotBBtn_.setTooltip("State B: Click to switch to State B");
    abCopyBtn_.setTooltip("Copy current State to inactive State");
    undoBtn_.setTooltip("Undo last parameter change");
    redoBtn_.setTooltip("Redo last parameter change");
}

void PluginShell::refreshPresetDisplay() {
    const auto& pm = processor_.getPresetManager();
    presetPillBtn_.setButtonText(pm.getCurrentPresetCategory() + "  /  " + pm.getCurrentPresetName());
}

void PluginShell::openPresetPopupMenu() {
    juce::PopupMenu menu;
    auto& pm = processor_.getPresetManager();
    const auto categories = pm.getCategories();
    const auto& allPresets = pm.getAllPresets();

    menu.addItem(9998, "Reset to Init Patch", true);
    menu.addItem(9999, "+ Save Current Preset...", true);
    menu.addSeparator();

    for (const auto& cat : categories) {
        if (cat == "All") continue;
        juce::PopupMenu subMenu;
        for (int i = 0; i < static_cast<int>(allPresets.size()); ++i) {
            if (allPresets[static_cast<std::size_t>(i)].category == cat) {
                const bool isCurrent = (i == pm.getCurrentPresetIndex());
                subMenu.addItem(i + 1, allPresets[static_cast<std::size_t>(i)].name, true, isCurrent);
            }
        }
        menu.addSubMenu(cat, subMenu);
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetPillBtn_),
                       [this](int result) {
                           if (result == 9998) {
                               processor_.getPresetManager().loadPresetByName("Init Patch");
                               refreshPresetDisplay();
                               return;
                           }
                           if (result == 9999) {
                               auto* alert = new juce::AlertWindow("Save Preset", "Enter a name for your custom preset:", juce::AlertWindow::QuestionIcon);
                               alert->addTextEditor("presetName", processor_.getPresetManager().getCurrentPresetName() + " (User)", "Preset Name:");
                               alert->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
                               alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
                               alert->enterModalState(true, juce::ModalCallbackFunction::create([this, alert](int res) {
                                   if (res == 1) {
                                       const auto name = alert->getTextEditorContents("presetName").trim();
                                       if (name.isNotEmpty()) {
                                           processor_.getPresetManager().saveUserPreset(name, "User", "Custom user preset", { "User" });
                                           refreshPresetDisplay();
                                       }
                                   }
                                   delete alert;
                               }), true);
                               return;
                           }
                           if (result > 0) {
                               processor_.getPresetManager().loadPresetByIndex(result - 1);
                               refreshPresetDisplay();
                           }
                       });
}

void PluginShell::openSettingsPopupMenu() {
    if (uiSettingsBtn_.onClick) {
        uiSettingsBtn_.onClick();
    }
}

void PluginShell::setMixerActive(bool active) {
    if (isMixerActive_ != active) {
        isMixerActive_ = active;
        mixerBtn_.setColour(juce::TextButton::buttonColourId,
            active ? theme_.accentPrimary.withAlpha(0.28f) : theme_.bgElevated);
        mixerBtn_.setColour(juce::TextButton::textColourOffId,
            active ? theme_.accentPrimary : theme_.textPrimary);
    }
}

void PluginShell::updateABButtons() {
    const bool isB = processor_.getPresetManager().isSlotBActive();

    abSlotABtn_.setToggleState(!isB, juce::dontSendNotification);
    abSlotBBtn_.setToggleState(isB, juce::dontSendNotification);

    // Active slot: Solid Space Cyan fill with bold dark obsidian text (A is active by default)
    // Inactive slot: Dark elevated background with muted text
    if (!isB) {
        // Slot A Active (Default)
        abSlotABtn_.setColour(juce::TextButton::buttonColourId, theme_.accentPrimary);
        abSlotABtn_.setColour(juce::TextButton::buttonOnColourId, theme_.accentPrimary);
        abSlotABtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff090d16));
        abSlotABtn_.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff090d16));

        abSlotBBtn_.setColour(juce::TextButton::buttonColourId, theme_.bgElevated);
        abSlotBBtn_.setColour(juce::TextButton::buttonOnColourId, theme_.bgElevated);
        abSlotBBtn_.setColour(juce::TextButton::textColourOffId, theme_.textSecondary);
        abSlotBBtn_.setColour(juce::TextButton::textColourOnId, theme_.textSecondary);
    } else {
        // Slot B Active
        abSlotABtn_.setColour(juce::TextButton::buttonColourId, theme_.bgElevated);
        abSlotABtn_.setColour(juce::TextButton::buttonOnColourId, theme_.bgElevated);
        abSlotABtn_.setColour(juce::TextButton::textColourOffId, theme_.textSecondary);
        abSlotABtn_.setColour(juce::TextButton::textColourOnId, theme_.textSecondary);

        abSlotBBtn_.setColour(juce::TextButton::buttonColourId, theme_.accentPrimary);
        abSlotBBtn_.setColour(juce::TextButton::buttonOnColourId, theme_.accentPrimary);
        abSlotBBtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff090d16));
        abSlotBBtn_.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff090d16));
    }
    abSlotABtn_.repaint();
    abSlotBBtn_.repaint();
}

} // namespace nova::ui

