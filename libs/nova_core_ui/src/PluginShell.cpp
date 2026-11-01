#include "nova_ui/PluginShell.h"
#include <algorithm>

namespace nova::ui {

PluginShell::PluginShell(nova::plugin::NovaProcessorBase& processor,
                         const juce::String& pluginTitle,
                         const juce::String& pluginSubtitle,
                         const NovaTheme& theme)
    : processor_(processor),
      pluginTitle_(pluginTitle),
      pluginSubtitle_(pluginSubtitle),
      theme_(theme),
      uiSettingsBtn_(theme_) {
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
    addAndMakeVisible(uiSettingsBtn_);

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

    updateABButtons();
    refreshPresetDisplay();
    startTimerHz(30);
}

PluginShell::~PluginShell() {
    stopTimer();
}

int PluginShell::addPage(const juce::String& pageTitle, std::unique_ptr<juce::Component> pageComponent) {
    PageEntry entry;
    entry.title = pageTitle;
    entry.content = std::move(pageComponent);
    if (entry.content != nullptr) {
        addChildComponent(entry.content.get());
    }
    pages_.push_back(std::move(entry));
    const int idx = static_cast<int>(pages_.size()) - 1;
    if (pages_.size() == 1) {
        selectPage(0);
    }
    resized();
    return idx;
}

void PluginShell::selectPage(int pageIndex) {
    if (pageIndex < 0 || pageIndex >= static_cast<int>(pages_.size())) {
        return;
    }
    activePageIndex_ = pageIndex;
    for (int i = 0; i < static_cast<int>(pages_.size()); ++i) {
        if (auto* comp = pages_[static_cast<std::size_t>(i)].content.get()) {
            comp->setVisible(i == activePageIndex_);
        }
    }
    resized();
    repaint();
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

void PluginShell::paint(juce::Graphics& g) {
    g.fillAll(theme_.bgCanvas);

    // Top Navigation Bar
    g.setColour(theme_.bgHeader);
    g.fillRect(0, 0, getWidth(), kTopBarHeight);
    g.setColour(theme_.borderSubtle);
    g.drawHorizontalLine(kTopBarHeight - 1, 0.0f, static_cast<float>(getWidth()));

    // Brand Logo & Plugin Title
    int titleX = 16;
    if (appIcon_.isValid()) {
        constexpr float logoH = 22.0f;
        const float aspect = static_cast<float>(appIcon_.getWidth()) / static_cast<float>(std::max(1, appIcon_.getHeight()));
        const float logoW = logoH * aspect;
        const float logoY = (static_cast<float>(kTopBarHeight) - logoH) * 0.5f;
        const juce::Rectangle<float> logoBounds(14.0f, logoY, logoW, logoH);

        // Render sleek infinity vector logo cleanly without harsh box/clipping
        g.drawImage(appIcon_, logoBounds, juce::RectanglePlacement::centred);

        titleX = static_cast<int>(logoBounds.getRight() + 10.0f);
    }

    g.setFont(theme_.getTitleFont(19.0f));
    g.setColour(theme_.textPrimary);
    if (pluginSubtitle_.isNotEmpty()) {
        g.drawText(pluginTitle_, juce::Rectangle<int>(titleX, 5, 120, 22), juce::Justification::centredLeft, false);
        g.setFont(theme_.getLabelFont(11.0f));
        g.setColour(theme_.accentPrimary);
        g.drawText(pluginSubtitle_.toUpperCase(), juce::Rectangle<int>(titleX, 25, 120, 16), juce::Justification::centredLeft, false);
    } else {
        g.drawText(pluginTitle_, juce::Rectangle<int>(titleX, 0, 120, kTopBarHeight), juce::Justification::centredLeft, false);
    }

    // Page Navigation Tabs (only if multiple pages exist, e.g. NovaSpace)
    if (pages_.size() > 1) {
        for (int i = 0; i < static_cast<int>(pages_.size()); ++i) {
            const auto& page = pages_[static_cast<std::size_t>(i)];
            if (page.tabBounds.isEmpty()) continue;
            const bool isActive = (i == activePageIndex_);
            const auto tabF = page.tabBounds.toFloat();

            g.setColour(isActive ? theme_.accentPrimary.withAlpha(0.20f) : theme_.bgElevated.withAlpha(0.6f));
            g.fillRoundedRectangle(tabF, theme_.pillRadius);

            g.setColour(isActive ? theme_.accentPrimary : theme_.borderSubtle);
            g.drawRoundedRectangle(tabF, theme_.pillRadius, isActive ? 1.5f : 1.0f);

            g.setFont(theme_.getSectionFont(12.5f));
            g.setColour(isActive ? theme_.textPrimary : theme_.textSecondary);
            g.drawText(page.title.toUpperCase(), page.tabBounds, juce::Justification::centred, false);
        }
    }

    // Right-hand Live Meters (IN / OUT / CPU)
    const int meterRight = getWidth() - 14;
    const int meterWidth = 120;
    const int meterLeft = meterRight - meterWidth;

    const float inPeak = std::clamp(processor_.getInputPeakLevel(), 0.0f, 1.0f);
    const float outPeak = std::clamp(processor_.getOutputPeakLevel(), 0.0f, 1.0f);

    g.setFont(theme_.getLabelFont(10.0f));
    g.setColour(theme_.textMuted);
    g.drawText("IN", meterLeft, 9, 20, 13, juce::Justification::centredLeft);
    g.drawText("OUT", meterLeft, 24, 24, 13, juce::Justification::centredLeft);

    const juce::Rectangle<float> inBar(static_cast<float>(meterLeft + 24), 12.0f, 48.0f, 6.0f);
    const juce::Rectangle<float> outBar(static_cast<float>(meterLeft + 24), 27.0f, 48.0f, 6.0f);
    g.setColour(theme_.bgControl);
    g.fillRoundedRectangle(inBar, 3.0f);
    g.fillRoundedRectangle(outBar, 3.0f);

    g.setColour(theme_.meterOk);
    g.fillRoundedRectangle(inBar.withWidth(inBar.getWidth() * inPeak), 3.0f);
    g.setColour(outPeak > 0.95f ? theme_.accentWarm : theme_.accentPrimary);
    g.fillRoundedRectangle(outBar.withWidth(outBar.getWidth() * outPeak), 3.0f);

    // Smoothed, calm CPU readout (no nervous flickering!)
    g.setFont(theme_.getValueFont(11.5f));
    g.setColour(theme_.textSecondary);
    const juce::String cpuText = (displayedCpu_ < 0.1f ? "0.0" : juce::String(displayedCpu_, 1)) + "%";
    g.drawText(cpuText, meterLeft + 74, 14, 46, 20, juce::Justification::centredRight);
}

void PluginShell::resized() {
    // 1. Left title area width
    const int titleW = appIcon_.isValid()
        ? static_cast<int>(14.0f + 22.0f * (static_cast<float>(appIcon_.getWidth()) / static_cast<float>(std::max(1, appIcon_.getHeight()))) + 10.0f + 115.0f)
        : (pluginSubtitle_.isNotEmpty() ? 162 : 142);

    // 2. Right-Hand Controls anchored right-to-left from meterLeft - 16
    // Guarantees Settings button never overlaps IN/OUT meters!
    const int meterRight = getWidth() - 14;
    const int meterWidth = 120;
    const int meterLeft = meterRight - meterWidth;

    const int settingsRight = meterLeft - 16;
    uiSettingsBtn_.setBounds(settingsRight - 26, 11, 26, 26);

    const int redoRight = uiSettingsBtn_.getX() - 6;
    redoBtn_.setBounds(redoRight - 38, 11, 38, 26);

    const int undoRight = redoBtn_.getX() - 4;
    undoBtn_.setBounds(undoRight - 38, 11, 38, 26);

    const int abCopyRight = undoBtn_.getX() - 6;
    abCopyBtn_.setBounds(abCopyRight - 42, 11, 42, 26);

    const int abSlotBRight = abCopyBtn_.getX() - 4;
    abSlotBBtn_.setBounds(abSlotBRight - 24, 11, 24, 26);

    const int abSlotARight = abSlotBBtn_.getX() - 2;
    abSlotABtn_.setBounds(abSlotARight - 24, 11, 24, 26);

    const int bpmRight = abSlotABtn_.getX() - 8;
    bpmBtn_.setBounds(bpmRight - 88, 11, 88, 26);

    const int rightControlsStart = bpmBtn_.getX();

    // 3. Preset Selection Group (directly after Title!)
    const int presetX = titleW + 8;
    const int availSpace = std::max(160, rightControlsStart - presetX - 160);
    const int presetPillW = std::clamp(availSpace - 100, 150, 210);

    prevPresetBtn_.setBounds(presetX, 11, 24, 26);
    presetPillBtn_.setBounds(presetX + 26, 11, presetPillW, 26);
    nextPresetBtn_.setBounds(presetX + 26 + presetPillW + 2, 11, 24, 26);
    const int presetRight = presetX + 26 + presetPillW + 2 + 24;

    // 4. Action Buttons (PATCHES, MIXER) directly following Preset Group!
    int btnX = presetRight + 10;
    if (patchesBtn_.isVisible()) {
        patchesBtn_.setBounds(btnX, 11, 68, 26);
        btnX += 74;
    }
    if (mixerBtn_.isVisible()) {
        mixerBtn_.setBounds(btnX, 11, 60, 26);
        btnX += 66;
    }

    // 5. Page tabs (if multiple pages exist, e.g. NovaSpace)
    if (pages_.size() > 1) {
        for (auto& page : pages_) {
            page.tabBounds = juce::Rectangle<int>(btnX, 11, 64, 26);
            btnX += 70;
        }
    } else if (pages_.size() == 1) {
        pages_[0].tabBounds = {};
    }

    // Modal dialog overlay
    if (settingsDialog_ != nullptr && settingsDialog_->isVisible()) {
        settingsDialog_->setBounds(getLocalBounds());
    }

    // Active Page Content
    const auto pageBounds = getLocalBounds().withTrimmedTop(kTopBarHeight).reduced(10, 10);
    for (auto& page : pages_) {
        if (page.content != nullptr) {
            page.content->setBounds(pageBounds);
        }
    }
}

void PluginShell::mouseDown(const juce::MouseEvent& event) {
    const auto pos = event.getPosition();
    if (pos.y > kTopBarHeight) return;

    if (pages_.size() > 1) {
        for (int i = 0; i < static_cast<int>(pages_.size()); ++i) {
            if (pages_[static_cast<std::size_t>(i)].tabBounds.contains(pos)) {
                selectPage(i);
                return;
            }
        }
    }
}

void PluginShell::timerCallback() {
    refreshPresetDisplay();
    const bool isSynced = processor_.isHostBpmSynced();
    const double curBpm = processor_.getBpm();
    bpmBtn_.setButtonText(juce::String(curBpm, 1) + (isSynced ? " (DAW)" : " BPM"));

    // Smooth CPU with exponential lowpass filter (EMA) to prevent flickering
    const float rawCpu = std::clamp(processor_.getCpuLoadPercent(), 0.0f, 100.0f);
    smoothedCpu_ = smoothedCpu_ * 0.94f + rawCpu * 0.06f;
    if (++cpuUpdateCounter_ >= 15) { // Update displayed text at ~2.0 Hz (every 500 ms)
        cpuUpdateCounter_ = 0;
        displayedCpu_ = smoothedCpu_;
    }

    // Sync Undo/Redo button availability
    const bool canUndo = processor_.getUndoManager().canUndo();
    const bool canRedo = processor_.getUndoManager().canRedo();
    if (undoBtn_.isEnabled() != canUndo) {
        undoBtn_.setEnabled(canUndo);
        undoBtn_.setAlpha(canUndo ? 1.0f : 0.45f);
    }
    if (redoBtn_.isEnabled() != canRedo) {
        redoBtn_.setEnabled(canRedo);
        redoBtn_.setAlpha(canRedo ? 1.0f : 0.45f);
    }

    updateABButtons();

    repaint(getWidth() - 150, 0, 150, kTopBarHeight);
}

} // namespace nova::ui
