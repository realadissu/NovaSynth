#include "JornaSourcesColumn.h"
#include "NovaSynthUiHelpers.h"
#include "../NovaSynthProcessor.h"
#include "nova_dsp/oscillators/MultiEngineOscillator.h"
#include "nova_dsp/synth/WorshipSynthEngine.h"
#include <cmath>
#include <algorithm>

namespace nova::synth {

void JornaSourcesColumn::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(theme_.bgSurface);
    g.fillRoundedRectangle(bounds, theme_.cornerRadius);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(bounds, theme_.cornerRadius, 1.0f);

    const auto params = proc_.getCurrentParameters();
    const int activeCount = std::clamp(params.activeOscCount, 1, 8);
    if (selectedOsc_ >= activeCount) {
        selectedOsc_ = activeCount - 1;
    }

    // --- Header: SOURCES  N LAYERS   [+] [-]   OUT ---
    g.setFont(theme_.getSectionFont(14.5f));
    g.setColour(juce::Colour(0xff38bdf8));
    g.drawText("SOURCES", 12, 8, 70, 18, juce::Justification::centredLeft);

    g.setFont(theme_.getLabelFont(12.0f));
    g.setColour(theme_.textMuted);
    g.drawText(juce::String(activeCount) + " LAYERS", 82, 8, 65, 18, juce::Justification::centredLeft);

    // Add (+) and Remove (-) Layer pills in header
    addLayerBtnBounds_ = juce::Rectangle<int>(getWidth() - 96, 7, 24, 18);
    remLayerBtnBounds_ = juce::Rectangle<int>(getWidth() - 68, 7, 24, 18);

    g.setColour(theme_.bgElevated);
    g.fillRoundedRectangle(addLayerBtnBounds_.toFloat(), 4.0f);
    g.fillRoundedRectangle(remLayerBtnBounds_.toFloat(), 4.0f);
    g.setColour(theme_.borderFocus);
    g.drawRoundedRectangle(addLayerBtnBounds_.toFloat(), 4.0f, 1.0f);
    g.drawRoundedRectangle(remLayerBtnBounds_.toFloat(), 4.0f, 1.0f);

    g.setFont(theme_.getSectionFont(13.5f));
    g.setColour(activeCount < 8 ? juce::Colour(0xff38bdf8) : theme_.textMuted);
    g.drawText("+", addLayerBtnBounds_, juce::Justification::centred);
    g.setColour(activeCount > 1 ? juce::Colour(0xfff43f5e) : theme_.textMuted);
    g.drawText("-", remLayerBtnBounds_, juce::Justification::centred);

    g.setFont(theme_.getLabelFont(12.0f));
    g.setColour(theme_.textMuted);
    g.drawText("OUT", getWidth() - 40, 8, 30, 18, juce::Justification::centredRight);

    // --- Vertical Layer List (up to 8 layers) ---
    const int listTop = 30;
    const int listBottom = (getHeight() >= 300) ? 186 : (getHeight() - 6);
    const int rowH = std::clamp((listBottom - listTop) / std::max(1, activeCount), 16, 26);

    rowRects_.clear();
    toggleRingRects_.clear();
    namePillRects_.clear();
    engineDropRects_.clear();
    filterRoutingRects_.clear();

    for (int i = 0; i < activeCount; ++i) {
        const int ry = listTop + i * rowH;
        const juce::Rectangle<int> rowR(8, ry, getWidth() - 16, rowH - 2);
        rowRects_.push_back(rowR);

        const auto layerCol = getLayerColour(i);
        const bool isSel = (i == selectedOsc_);
        const juce::String prefix = "osc" + juce::String(i + 1) + "_";
        const bool isOn = proc_.getAPVTS().getRawParameterValue(prefix + "enabled")->load() > 0.5f;
        const auto& slotP = params.oscSlots[static_cast<std::size_t>(i)];

        // Row background + colored selection outline (just like JORNA's OSC C row!)
        g.setColour(isSel ? layerCol.withAlpha(0.13f) : theme_.bgControl.withAlpha(0.75f));
        g.fillRoundedRectangle(rowR.toFloat(), 4.0f);
        if (isSel) {
            g.setColour(layerCol.withAlpha(0.85f));
            g.drawRoundedRectangle(rowR.toFloat(), 4.0f, 1.3f);
        }

        // Left On/Off Ring Button (O)
        const juce::Rectangle<int> ringR(rowR.getX() + 6, rowR.getCentreY() - 5, 10, 10);
        toggleRingRects_.push_back(ringR);
        g.setColour(isOn ? layerCol : theme_.textMuted.withAlpha(0.35f));
        g.drawEllipse(ringR.toFloat(), 2.0f);
        if (isOn) {
            g.fillEllipse(ringR.toFloat().reduced(2.5f));
        }

        // Dynamically proportioned pills to prevent overlap on any width
        const int totalW = rowR.getWidth();
        const int nameW = std::clamp(totalW * 28 / 100, 44, 58);
        const int fltW = 44;
        const int engW = std::max(38, totalW - (20 + nameW + fltW + 10));

        // Layer Name Pill (e.g. OSC A, SUB, PLUCK, NOISE)
        const juce::Rectangle<int> nameR(rowR.getX() + 18, rowR.getY() + 1, nameW, rowR.getHeight() - 2);
        namePillRects_.push_back(nameR);
        g.setFont(theme_.getSectionFont(12.5f));
        g.setColour(isOn ? layerCol : theme_.textMuted);
        g.drawText(getLayerShortName(params, i), nameR, juce::Justification::centredLeft);

        // Engine Dropdown Pill
        const juce::Rectangle<int> engR(rowR.getX() + 18 + nameW + 4, rowR.getY() + 2, engW, rowR.getHeight() - 4);
        engineDropRects_.push_back(engR);
        g.setColour(theme_.bgElevated);
        g.fillRoundedRectangle(engR.toFloat(), 3.0f);
        g.setFont(theme_.getLabelFont(11.0f));
        g.setColour(isOn ? theme_.textPrimary : theme_.textMuted);
        g.drawText(getEngineDisplayName(params, i) + " v", engR.reduced(3, 0),
                   juce::Justification::centredLeft);

        // Filter Target Pill (FLT 1, FLT 2, FLT 1+2, DIRECT)
        const juce::Rectangle<int> fltR(rowR.getRight() - fltW, rowR.getY() + 2, fltW, rowR.getHeight() - 4);
        filterRoutingRects_.push_back(fltR);
        g.setColour(theme_.bgElevated);
        g.fillRoundedRectangle(fltR.toFloat(), 3.0f);
        g.setColour(isOn ? juce::Colour(0xff38bdf8) : theme_.textMuted);
        g.setFont(theme_.getLabelFont(10.5f));
        g.drawText(getFilterRoutingDisplayName(slotP.filterRouting), fltR, juce::Justification::centred);
    }

    if (getHeight() >= 300) {
        // --- Divider & Selected Oscillator Detail Header ---
        const int detailTop = 192;
        g.setColour(theme_.borderSubtle);
        g.drawHorizontalLine(detailTop - 3, 8.0f, static_cast<float>(getWidth() - 8));

        const auto selCol = getLayerColour(selectedOsc_);
        const auto& selSlot = params.oscSlots[static_cast<std::size_t>(selectedOsc_)];

        g.setFont(theme_.getSectionFont(14.5f));
        g.setColour(selCol);
        g.drawText(getLayerShortName(params, selectedOsc_), 12, detailTop, 60, 18, juce::Justification::centredLeft);

        g.setFont(theme_.getLabelFont(11.5f));
        g.setColour(theme_.textMuted);
        juce::String subDesc = "WAVETABLE / ANALOG / GRANULAR / PLUCK";
        using ET = nova::dsp::MultiEngineOscillator::EngineType;
        if (selSlot.sourceType == 1 || selSlot.engine == ET::SubOscillator) {
            subDesc = "DEDICATED SUB BASS GENERATOR";
        } else if (selSlot.sourceType == 2 || selSlot.engine == ET::PhysicalPluckModal) {
            subDesc = "KARPLUS-STRONG PHYSICAL PLUCK";
        } else if (selSlot.sourceType == 3 || selSlot.engine == ET::NoiseTransient) {
            subDesc = "ATMOSPHERIC TEXTURE & VINYL DUST";
        }
        g.drawText(subDesc, 68, detailTop, getWidth() - 78, 18, juce::Justification::centredRight);

        // Waveform / Preset Sub-Bar (< WARM ANALOG / WAVETABLE > across full width)
        waveCycleRect_ = juce::Rectangle<int>(10, detailTop + 20, getWidth() - 20, 22);
        g.setColour(theme_.bgControl);
        g.fillRoundedRectangle(waveCycleRect_.toFloat(), 4.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(waveCycleRect_.toFloat(), 4.0f, 1.0f);
        g.setFont(theme_.getSectionFont(12.5f));
        g.setColour(theme_.textPrimary);
        g.drawText("<  " + getModelDisplayName(params, selectedOsc_).toUpperCase() + "  >", waveCycleRect_,
                   juce::Justification::centred);

        // --- Bottom SPREAD 100% Bar (JORNA green horizontal bar) ---
        spreadBarRect_ = juce::Rectangle<int>(12, getHeight() - 24, getWidth() - 24, 14);
        const float spreadNorm = std::clamp(selSlot.detune, 0.0f, 1.0f);

        g.setFont(theme_.getLabelFont(11.5f));
        g.setColour(theme_.textSecondary);
        g.drawText("SPREAD", spreadBarRect_.getX(), spreadBarRect_.getY() - 12, 60, 12,
                   juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff22c55e));
        g.drawText(juce::String(static_cast<int>(spreadNorm * 100.0f)) + "%",
                   spreadBarRect_.getRight() - 45, spreadBarRect_.getY() - 12, 45, 12,
                   juce::Justification::centredRight);

        const auto barTrack = spreadBarRect_.withTrimmedTop(6).withHeight(5).toFloat();
        g.setColour(theme_.bgControl);
        g.fillRoundedRectangle(barTrack, 2.5f);
        g.setColour(juce::Colour(0xff22c55e));
        g.fillRoundedRectangle(barTrack.withWidth(barTrack.getWidth() * spreadNorm), 2.5f);
    } else {
        waveCycleRect_ = {};
        spreadBarRect_ = {};
    }
}

} // namespace nova::synth

