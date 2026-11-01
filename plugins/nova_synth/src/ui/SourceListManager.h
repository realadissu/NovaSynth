#pragma once

#include "nova_ui/NovaTheme.h"
#include "../NovaSynthProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace nova::synth {

class SourceListManager : public juce::Component {
public:
    std::function<void(int)> onSourceSelected;
    std::function<void()> onSourceStateChanged;

    SourceListManager(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc), theme_(theme) {}

    void setSelectedSlot(int slotIndex) {
        selectedSlot_ = std::clamp(slotIndex, 0, 7);
        repaint();
    }

    [[nodiscard]] int getSelectedSlot() const noexcept { return selectedSlot_; }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(theme_.bgSurface);
        g.fillRoundedRectangle(bounds, theme_.cornerRadius);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(bounds, theme_.cornerRadius, 1.0f);

        const auto params = proc_.getCurrentParameters();
        const int activeCount = std::clamp(params.activeOscCount, 1, 8);

        // Header: "SOURCES" & "X/8" (no redundant + button)
        auto headerArea = getLocalBounds().removeFromTop(30);
        g.setColour(theme_.bgHeader.withAlpha(0.6f));
        g.fillRoundedRectangle(headerArea.toFloat().reduced(2.0f, 2.0f), theme_.cornerRadius);

        g.setFont(theme_.getSectionFont(12.5f));
        g.setColour(theme_.textPrimary);
        g.drawFittedText("SOURCES", headerArea.removeFromLeft(80).reduced(8, 0), juce::Justification::centredLeft, 1, 0.85f);

        g.setFont(theme_.getValueFont(11.5f));
        g.setColour(theme_.accentPrimary);
        g.drawFittedText(juce::String(activeCount) + "/8", headerArea.reduced(8, 0), juce::Justification::centredRight, 1, 0.85f);

        // Slot Rows: Always display all 8 source slots
        rowRects_.clear();
        powerRects_.clear();
        soloRects_.clear();

        const int availH = getHeight() - 36;
        const int rowH = std::max(20, availH / 8);

        for (int i = 0; i < 8; ++i) {
            const auto& slot = params.oscSlots[static_cast<std::size_t>(i)];
            const juce::Rectangle<int> r(6, 33 + i * rowH, getWidth() - 12, rowH - 3);
            rowRects_.push_back(r);

            const bool isSel = (i == selectedSlot_);
            const bool isEnabled = slot.enabled;
            const auto slotCol = getSlotColour(i);

            // Row background
            if (isSel) {
                g.setColour(slotCol.withAlpha(0.20f));
                g.fillRoundedRectangle(r.toFloat(), theme_.pillRadius);
                g.setColour(slotCol.withAlpha(0.90f));
                g.drawRoundedRectangle(r.toFloat(), theme_.pillRadius, 1.4f);
            } else {
                g.setColour(theme_.bgControl.withAlpha(isEnabled ? 0.70f : 0.35f));
                g.fillRoundedRectangle(r.toFloat(), theme_.pillRadius);
                g.setColour(theme_.borderSubtle.withAlpha(0.6f));
                g.drawRoundedRectangle(r.toFloat(), theme_.pillRadius, 0.9f);
            }

            auto rowArea = r;

            // Color Indicator Dot (Perfect Circle, never oval)
            auto dotArea = rowArea.removeFromLeft(16);
            auto dotSquare = dotArea.withSizeKeepingCentre(8, 8).toFloat();
            g.setColour(isEnabled ? slotCol : theme_.textMuted.withAlpha(0.4f));
            g.fillEllipse(dotSquare);

            // Right-side controls: Solo [S] & Power Toggle
            const int btnSize = std::clamp(rowH - 12, 16, 22);

            // Power Toggle Ring (Exact Circle, never oval)
            auto powR = rowArea.removeFromRight(btnSize + 4).withSizeKeepingCentre(btnSize, btnSize);
            powerRects_.push_back(powR);
            auto powSquare = powR.withSizeKeepingCentre(10, 10).toFloat();
            g.setColour(isEnabled ? slotCol : theme_.textMuted.withAlpha(0.35f));
            if (isEnabled) {
                g.fillEllipse(powSquare);
            } else {
                g.drawEllipse(powSquare, 1.4f);
            }

            // Solo Button [S]
            auto soloR = rowArea.removeFromRight(btnSize + 2).withSizeKeepingCentre(btnSize, btnSize);
            soloRects_.push_back(soloR);
            const bool isSolo = proc_.isSlotSoloed(i);
            g.setColour(isSolo ? juce::Colour(0xffeab308) : theme_.bgElevated.withAlpha(0.7f));
            g.fillRoundedRectangle(soloR.toFloat(), 3.0f);
            g.setColour(isSolo ? juce::Colour(0xfffacc15) : theme_.borderSubtle);
            g.drawRoundedRectangle(soloR.toFloat(), 3.0f, 0.8f);
            g.setFont(theme_.getLabelFont(10.5f));
            g.setColour(isSolo ? juce::Colour(0xff1e1b4b) : theme_.textMuted);
            g.drawText("S", soloR, juce::Justification::centred, false);

            // Layer Short Name & Subtitle
            auto textRect = rowArea.reduced(2, 0);
            if (rowH >= 28) {
                g.setFont(theme_.getSectionFont(11.5f));
                g.setColour(isEnabled ? (isSel ? juce::Colours::white : slotCol) : theme_.textMuted);
                g.drawFittedText(getSlotShortName(slot, i), textRect.removeFromTop(rowH / 2 + 1), juce::Justification::bottomLeft, 1, 0.85f);

                g.setFont(theme_.getValueFont(9.5f));
                g.setColour(isEnabled ? theme_.textSecondary : theme_.textMuted.withAlpha(0.6f));
                g.drawFittedText(getEngineShortName(slot), textRect, juce::Justification::topLeft, 1, 0.85f);
            } else {
                g.setFont(theme_.getSectionFont(11.0f));
                g.setColour(isEnabled ? (isSel ? juce::Colours::white : slotCol) : theme_.textMuted);
                const juce::String combined = getSlotShortName(slot, i) + " · " + getEngineShortName(slot);
                g.drawFittedText(combined, textRect, juce::Justification::centredLeft, 1, 0.85f);
            }
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const auto pos = e.getPosition();

        // Slot rows
        for (int i = 0; i < static_cast<int>(rowRects_.size()); ++i) {
            if (soloRects_[static_cast<std::size_t>(i)].contains(pos)) {
                const bool curSolo = proc_.isSlotSoloed(i);
                proc_.setSlotSolo(i, !curSolo);
                repaint();
                if (onSourceStateChanged) onSourceStateChanged();
                return;
            }

            if (powerRects_[static_cast<std::size_t>(i)].contains(pos)) {
                // Toggle power
                const auto params = proc_.getCurrentParameters();
                const bool curState = params.oscSlots[static_cast<std::size_t>(i)].enabled;
                const juce::String prefix = "osc" + juce::String(i + 1) + "_";
                if (auto* p = proc_.getAPVTS().getParameter(prefix + "enabled")) {
                    p->setValueNotifyingHost(curState ? 0.0f : 1.0f);
                }
                repaint();
                if (onSourceStateChanged) onSourceStateChanged();
                return;
            }

            if (rowRects_[static_cast<std::size_t>(i)].contains(pos)) {
                setSelectedSlot(i);
                if (onSourceSelected) onSourceSelected(i);
                repaint();
                return;
            }
        }
    }

private:
    static juce::Colour getSlotColour(int index) noexcept {
        static constexpr std::array<uint32_t, 8> kColours = {
            0xff38bdf8, // Cyan
            0xff2dd4bf, // Teal
            0xffec4899, // Magenta
            0xffa855f7, // Purple
            0xff84cc16, // Lime
            0xfff59e0b, // Amber
            0xfff43f5e, // Coral
            0xff60a5fa  // Sky Blue
        };
        return juce::Colour(kColours[static_cast<std::size_t>(std::clamp(index, 0, 7))]);
    }

    static juce::String getSlotShortName(const nova::dsp::MultiEngineOscillator::Parameters& slot, int slotIdx) {
        if (slot.sourceType == 1) return "SUB";
        if (slot.sourceType == 2) return "PLUCK";
        if (slot.sourceType == 3) return "NOISE";
        if (slot.sourceType == 4) return "SAMPLE";
        const char letter = static_cast<char>('A' + std::clamp(slotIdx, 0, 7));
        return juce::String("OSC ") + juce::String::charToString(letter);
    }

    static juce::String getEngineShortName(const nova::dsp::MultiEngineOscillator::Parameters& slot) {
        if (slot.sourceType == 1) return "Sub";
        if (slot.sourceType == 2) return "Pluck";
        if (slot.sourceType == 3) return "Noise";
        if (slot.sourceType == 4) return "Sample";
        using ET = nova::dsp::MultiEngineOscillator::EngineType;
        switch (slot.engine) {
            case ET::WarmAnalog:        return "Analog";
            case ET::EvolvingWavetable: return "Wavetable";
            case ET::OrganicTexture:    return "Granular";
            case ET::PhysicalPluckModal:return "Pluck";
            case ET::SubOscillator:     return "Sub";
            case ET::NoiseTransient:    return "Noise";
            case ET::SamplePlayback:    return "Sample";
        }
        return "Analog";
    }

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int selectedSlot_ = 0;

    std::vector<juce::Rectangle<int>> rowRects_;
    std::vector<juce::Rectangle<int>> powerRects_;
    std::vector<juce::Rectangle<int>> soloRects_;
};

} // namespace nova::synth