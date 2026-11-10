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
      panicBtn_(theme_),
      uiSettingsBtn_(theme_) {
    setupToolbarButtons();
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

    const int panicRight = uiSettingsBtn_.getX() - 6;
    panicBtn_.setBounds(panicRight - 26, 11, 26, 26);

    const int redoRight = panicBtn_.getX() - 6;
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
    panicBtn_.updateFlash();
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
