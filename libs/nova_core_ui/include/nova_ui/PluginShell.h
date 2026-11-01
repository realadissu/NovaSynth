#pragma once

#include "nova_ui/NovaTheme.h"
#include "nova_ui/SettingsOverlayDialog.h"
#include "nova_plugin/NovaProcessorBase.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>

namespace nova::ui {

/**
 * @brief Universal Top-Bar & Page Router Shell for all Nova Plugins (Morning DAW / JORNA style).
 * Provides:
 *  - Brand + Plugin Name Badge
 *  - Top-Level Page Navigation Tabs (e.g. SPACE | TONE & DUCKING | BROWSER)
 *  - Interactive Preset Pill (<, >, Preset Popup Menu, Favorite Star)
 *  - A/B State Compare & Copy buttons
 *  - Undo / Redo buttons
 *  - Real-time Input/Output Level & CPU Meter
 */
class PluginShell : public juce::Component, private juce::Timer {
public:
    static constexpr int kTopBarHeight = 48;

    PluginShell(nova::plugin::NovaProcessorBase& processor,
                const juce::String& pluginTitle,
                const juce::String& pluginSubtitle,
                const NovaTheme& theme);
    ~PluginShell() override;

    int addPage(const juce::String& pageTitle, std::unique_ptr<juce::Component> pageComponent);
    void selectPage(int pageIndex);
    [[nodiscard]] int getActivePageIndex() const noexcept { return activePageIndex_; }

    void refreshPresetDisplay();
    void setAppIcon(const juce::Image& icon) {
        appIcon_ = icon;
        repaint(0, 0, 180, kTopBarHeight);
    }
    void setTextScaleCallback(std::function<void(NovaTheme::TextScale)> callback) {
        onTextScaleChanged_ = std::move(callback);
    }
    void setAudioSettingsCallback(std::function<void()> callback) {
        onOpenAudioSettings_ = std::move(callback);
    }
    void setPatchesCallback(std::function<void()> callback) {
        onPatchesClicked_ = std::move(callback);
        patchesBtn_.setVisible(true);
        resized();
    }
    void setMixerCallback(std::function<void()> callback) {
        onMixerClicked_ = std::move(callback);
        mixerBtn_.setVisible(true);
        resized();
    }
    void setMixerActive(bool active);
    [[nodiscard]] juce::Rectangle<int> getSettingsButtonScreenBounds() const {
        return uiSettingsBtn_.getScreenBounds();
    }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    void openPresetPopupMenu();
    void openSettingsPopupMenu();
    void updateABButtons();

    struct PageEntry {
        juce::String title;
        std::unique_ptr<juce::Component> content;
        juce::Rectangle<int> tabBounds;
    };

    nova::plugin::NovaProcessorBase& processor_;
    juce::String pluginTitle_;
    juce::String pluginSubtitle_;
    NovaTheme theme_;
    juce::Image appIcon_;
    std::function<void(NovaTheme::TextScale)> onTextScaleChanged_;
    std::function<void()> onOpenAudioSettings_;
    std::function<void()> onPatchesClicked_;
    std::function<void()> onMixerClicked_;
    bool isMixerActive_ = false;

    float smoothedCpu_ = 0.0f;
    float displayedCpu_ = 0.0f;
    int cpuUpdateCounter_ = 0;

    std::vector<PageEntry> pages_;
    int activePageIndex_ = 0;

    // Header interactive buttons
    juce::TextButton prevPresetBtn_{ "<" };
    juce::TextButton nextPresetBtn_{ ">" };
    juce::TextButton presetPillBtn_{ "Init" };
    juce::TextButton patchesBtn_{ "PATCHES" };
    juce::TextButton mixerBtn_{ "MIXER" };
    juce::TextButton bpmBtn_{ "120.0 BPM" };
    juce::TextButton abSlotABtn_{ "A" };
    juce::TextButton abSlotBBtn_{ "B" };
    juce::TextButton abCopyBtn_{ "Copy" };
    juce::TextButton undoBtn_{ "Undo" };
    juce::TextButton redoBtn_{ "Redo" };

    class SettingsGearButton : public juce::Button {
    public:
        SettingsGearButton(const NovaTheme& theme)
            : juce::Button("Settings"), theme_(theme) {}

        void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override {
            auto bounds = getLocalBounds().toFloat().reduced(1.0f);
            g.setColour(shouldDrawButtonAsHighlighted ? theme_.bgControl.brighter(0.12f) : theme_.bgElevated);
            g.fillRoundedRectangle(bounds, theme_.pillRadius);
            g.setColour(shouldDrawButtonAsHighlighted ? theme_.borderFocus : theme_.borderSubtle);
            g.drawRoundedRectangle(bounds, theme_.pillRadius, 1.0f);

            auto iconArea = bounds.reduced(5.0f);
            static const auto gearOutline = juce::Drawable::parseSVGPath(
                "M27.758,10.366l-1-1.732c-0.552-0.957-1.775-1.284-2.732-0.732L23.5,8.206C21.5,9.36,19,7.917,19,5.608V5c0-1.105-0.895-2-2-2h-2c-1.105,0-2,0.895-2,2v0.608c0,2.309-2.5,3.753-4.5,2.598L7.974,7.902C7.017,7.35,5.794,7.677,5.242,8.634l-1,1.732c-0.552,0.957-0.225,2.18,0.732,2.732L5.5,13.402c2,1.155,2,4.041,0,5.196l-0.526,0.304c-0.957,0.552-1.284,1.775-0.732,2.732l1,1.732c0.552,0.957,1.775,1.284,2.732,0.732L8.5,23.794c2-1.155,4.5,0.289,4.5,2.598V27c0,1.105,0.895,2,2,2h2c1.105,0,2-0.895,2-2v-0.608c0-2.309,2.5-3.753,4.5-2.598l0.526,0.304c0.957,0.552,2.18,0.225,2.732-0.732l1-1.732c0.552-0.957,0.225-2.18-0.732-2.732L26.5,18.598c-2-1.155-2-4.041,0-5.196l0.526-0.304C27.983,12.546,28.311,11.323,27.758,10.366z");

            juce::Path p = gearOutline;
            p.scaleToFit(iconArea.getX(), iconArea.getY(), iconArea.getWidth(), iconArea.getHeight(), true);

            const auto iconCol = shouldDrawButtonAsHighlighted ? theme_.accentPrimary : theme_.textPrimary;
            g.setColour(iconCol);
            g.strokePath(p, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            const auto centre = iconArea.getCentre();
            const float r = iconArea.getWidth() * 0.17f;
            g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 1.2f);
        }

    private:
        const NovaTheme& theme_;
    };

    SettingsGearButton uiSettingsBtn_;
    std::unique_ptr<SettingsOverlayDialog> settingsDialog_;
};

} // namespace nova::ui
