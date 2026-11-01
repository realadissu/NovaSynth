#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <nova_ui/NovaTheme.h>
#include <nova_ui/ArcKnob.h>
#include "../NovaSynthProcessor.h"

namespace nova::synth::ui {

/**
 * @brief Dedicated Multi-Source Console Mixer Hub Panel (Tab 5 - far right)
 *
 * Provides a high-level console view to balance all 8 sound generators,
 * adjust stereo panning, filter routings, solo/mute states, and master output levels.
 */
class MixerHubPanel : public juce::Component, public juce::TooltipClient, private juce::Timer {
public:
    MixerHubPanel(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc),
          theme_(theme),
          masterVol_("MASTER", "%", theme, false),
          delayReturn_("DELAY", "%", theme, true),
          reverbReturn_("REVERB", "%", theme, true) {

        auto& apvts = proc_.getAPVTS();

        // 8 Source Channel Strips
        for (int i = 0; i < 8; ++i) {
            const juce::String idx = juce::String(i + 1);
            strips_[static_cast<std::size_t>(i)].level =
                std::make_unique<nova::ui::ArcKnob>("LVL", "%", theme, false);
            strips_[static_cast<std::size_t>(i)].pan =
                std::make_unique<nova::ui::ArcKnob>("PAN", "", theme, true);

            strips_[static_cast<std::size_t>(i)].level->attachToParameter(apvts, "osc" + idx + "_level");
            strips_[static_cast<std::size_t>(i)].pan->attachToParameter(apvts, "osc" + idx + "_pan");

            addAndMakeVisible(*strips_[static_cast<std::size_t>(i)].level);
            addAndMakeVisible(*strips_[static_cast<std::size_t>(i)].pan);

            // Mute / Power Toggle
            strips_[static_cast<std::size_t>(i)].powerBtn.setButtonText("ON");
            strips_[static_cast<std::size_t>(i)].powerBtn.setClickingTogglesState(true);
            strips_[static_cast<std::size_t>(i)].powerAtt =
                std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                    apvts, "osc" + idx + "_enabled", strips_[static_cast<std::size_t>(i)].powerBtn);
            addAndMakeVisible(strips_[static_cast<std::size_t>(i)].powerBtn);

            // Filter Routing Cycle Button
            strips_[static_cast<std::size_t>(i)].routeBtn.onClick = [this, i]() {
                const juce::String paramId = "osc" + juce::String(i + 1) + "_filterRouting";
                if (auto* p = proc_.getAPVTS().getParameter(paramId)) {
                    constexpr int numRoutes = 4;
                    const int cur = static_cast<int>(p->convertFrom0to1(p->getValue()) + 0.5f);
                    const int next = (cur + 1) % numRoutes;
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(next)));
                    repaint();
                }
            };
            addAndMakeVisible(strips_[static_cast<std::size_t>(i)].routeBtn);
        }

        // Master & FX Returns
        delayReturn_.attachToParameter(apvts, "delayMix");
        reverbReturn_.attachToParameter(apvts, "reverbMix");
        addAndMakeVisible(delayReturn_);
        addAndMakeVisible(reverbReturn_);
        addAndMakeVisible(masterVol_);

        startTimerHz(30);
    }

    ~MixerHubPanel() override {
        stopTimer();
    }

    juce::String getTooltip() override {
        return "8-Source Console Mixer. Balance individual oscillator levels, pans, and filter assignments.";
    }

    void timerCallback() override {
        repaint();
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(theme_.bgSurface.darker(0.18f));

        drawChannelsCard(g, cardChannelsRect_.toFloat());
        drawMasterCard(g, cardMasterRect_.toFloat());
    }

    void resized() override {
        const auto area = getLocalBounds().reduced(8, 6);
        const int totalW = area.getWidth();
        const int totalH = area.getHeight();
        constexpr int gap = 8;
        const int availW = totalW - gap;

        // 80% 8-Channel strips, 20% Master & FX Returns
        const int wChans  = static_cast<int>(static_cast<float>(availW) * 0.80f);
        const int wMaster = availW - wChans;

        cardChannelsRect_ = juce::Rectangle<int>(area.getX(), area.getY(), wChans, totalH);
        cardMasterRect_   = juce::Rectangle<int>(cardChannelsRect_.getRight() + gap, area.getY(), wMaster, totalH);

        // Layout 8 Channel Strips
        auto chArea = cardChannelsRect_.withTrimmedTop(28).reduced(8, 6);
        const int colW = (chArea.getWidth() - 7 * 4) / 8;
        const int colH = chArea.getHeight();

        static constexpr std::array<const char*, 4> kRouteNames = { "FLT 1", "FLT 2", "FLT 1+2", "DIRECT" };

        for (int i = 0; i < 8; ++i) {
            const int colX = chArea.getX() + i * (colW + 4);
            auto colR = juce::Rectangle<int>(colX, chArea.getY(), colW, colH);

            auto topPower = colR.removeFromTop(20);
            strips_[static_cast<std::size_t>(i)].powerBtn.setBounds(topPower.reduced(2, 0));

            colR.removeFromTop(4);
            auto routeR = colR.removeFromTop(18);
            strips_[static_cast<std::size_t>(i)].routeBtn.setBounds(routeR.reduced(2, 0));

            // Update route button text
            const juce::String paramId = "osc" + juce::String(i + 1) + "_filterRouting";
            int routeIdx = 0;
            if (auto* p = proc_.getAPVTS().getRawParameterValue(paramId)) {
                routeIdx = std::clamp(static_cast<int>(p->load()), 0, 3);
            }
            strips_[static_cast<std::size_t>(i)].routeBtn.setButtonText(kRouteNames[static_cast<std::size_t>(routeIdx)]);

            colR.removeFromTop(4);
            const int knobH = (colR.getHeight() - 4) / 2;
            if (strips_[static_cast<std::size_t>(i)].level) {
                strips_[static_cast<std::size_t>(i)].level->setBounds(colR.removeFromTop(knobH));
            }
            colR.removeFromTop(4);
            if (strips_[static_cast<std::size_t>(i)].pan) {
                strips_[static_cast<std::size_t>(i)].pan->setBounds(colR);
            }
        }

        // Layout Master Section
        auto mArea = cardMasterRect_.withTrimmedTop(28).reduced(8, 6);
        const int mKnobH = (mArea.getHeight() - 8) / 3;
        masterVol_.setBounds(mArea.removeFromTop(mKnobH));
        mArea.removeFromTop(4);
        delayReturn_.setBounds(mArea.removeFromTop(mKnobH));
        mArea.removeFromTop(4);
        reverbReturn_.setBounds(mArea);
    }

private:
    struct ChannelStripUI {
        std::unique_ptr<nova::ui::ArcKnob> level;
        std::unique_ptr<nova::ui::ArcKnob> pan;
        juce::TextButton powerBtn;
        juce::TextButton routeBtn;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    };

    void drawChannelsCard(juce::Graphics& g, juce::Rectangle<float> area) {
        g.setColour(theme_.bgSurface.darker(0.12f));
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(area, 6.0f, 1.0f);

        // Header Title
        g.setFont(theme_.getSectionFont(12.0f));
        g.setColour(theme_.accentPrimary);
        g.drawText("SOURCE CONSOLE MIXER", juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 180.0f, 16.0f),
                   juce::Justification::left);

        // Subtitle badge
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(theme_.textMuted);
        g.drawText("8 OSCILLATORS", juce::Rectangle<float>(area.getRight() - 100.0f, area.getY() + 7.0f, 90.0f, 14.0f),
                   juce::Justification::right);
    }

    void drawMasterCard(juce::Graphics& g, juce::Rectangle<float> area) {
        g.setColour(theme_.bgSurface.darker(0.12f));
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(area, 6.0f, 1.0f);

        // Header Title
        g.setFont(theme_.getSectionFont(12.0f));
        g.setColour(juce::Colour(0xfff59e0b));
        g.drawText("OUTPUT / FX", juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 120.0f, 16.0f),
                   juce::Justification::left);

        // Subtitle badge
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(theme_.textMuted);
        g.drawText("MASTER", juce::Rectangle<float>(area.getRight() - 70.0f, area.getY() + 7.0f, 60.0f, 14.0f),
                   juce::Justification::right);
    }

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;

    juce::Rectangle<int> cardChannelsRect_;
    juce::Rectangle<int> cardMasterRect_;

    std::array<ChannelStripUI, 8> strips_;

    nova::ui::ArcKnob masterVol_;
    nova::ui::ArcKnob delayReturn_;
    nova::ui::ArcKnob reverbReturn_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerHubPanel)
};

} // namespace nova::synth::ui
