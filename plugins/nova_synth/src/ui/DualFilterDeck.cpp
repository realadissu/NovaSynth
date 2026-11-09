#include "DualFilterDeck.h"
#include "../NovaSynthProcessor.h"
#include <algorithm>
#include <cmath>

namespace nova::synth {

DualFilterDeck::DualFilterDeck(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc),
      theme_(theme),
      flt1Cutoff_("CUTOFF", "Hz", theme, false),
      flt1Reso_("RESO", "", theme, false),
      flt1Drive_("DRIVE", "", theme, false),
      flt1EnvAmt_("ENV AMT", "", theme, false),
      flt1Warmth_("WARMTH", "", theme, false),
      flt2Cutoff_("CUTOFF", "Hz", theme, false),
      flt2Reso_("RESO", "", theme, false),
      flt2Drive_("DRIVE", "", theme, false),
      flt2EnvAmt_("ENV AMT", "", theme, false),
      flt2Carve_("CARVE", "", theme, true) {
    addAndMakeVisible(flt1Cutoff_);
    addAndMakeVisible(flt1Reso_);
    addAndMakeVisible(flt1Drive_);
    addAndMakeVisible(flt1EnvAmt_);
    addAndMakeVisible(flt1Warmth_);

    addAndMakeVisible(flt2Cutoff_);
    addAndMakeVisible(flt2Reso_);
    addAndMakeVisible(flt2Drive_);
    addAndMakeVisible(flt2EnvAmt_);
    addAndMakeVisible(flt2Carve_);

    auto& apvts = proc_.getAPVTS();
    flt1Cutoff_.attachToParameter(apvts, "filterCutoffHz");
    flt1Reso_.attachToParameter(apvts,   "filterResonance");
    flt1Drive_.attachToParameter(apvts,  "filterDrive");
    flt1EnvAmt_.attachToParameter(apvts, "filterEnvAmount");
    flt1Warmth_.attachToParameter(apvts, "macroWarmth");

    flt2Cutoff_.attachToParameter(apvts, "filter2CutoffHz");
    flt2Reso_.attachToParameter(apvts,   "filter2Resonance");
    flt2Drive_.attachToParameter(apvts,  "filter2Drive");
    flt2EnvAmt_.attachToParameter(apvts, "filter2EnvAmount");
    flt2Carve_.attachToParameter(apvts,  "vocalCarve");
}

void DualFilterDeck::updateModulationTelemetry(const nova::dsp::WorshipSynthEngine::Telemetry& tel) {
    lastTel_ = tel;
    const auto& modMatrix = proc_.getModMatrix();

    // 1. Filter 1 Cutoff
    const float cut1Depth = modMatrix.getTotalDepth(nova::dsp::ModMatrix::Destination::FilterCutoff);
    const float cut1Live = (cut1Depth != 0.0f)
        ? std::clamp(tel.filterCutoffMod / (1.8f * std::max(0.01f, std::abs(cut1Depth))), -1.0f, 1.0f)
        : 0.0f;
    flt1Cutoff_.setModulation(cut1Depth, cut1Live);

    // 2. Filter 1 Reso
    const float reso1Depth = modMatrix.getTotalDepth(nova::dsp::ModMatrix::Destination::FilterResonance);
    const float reso1Live = (reso1Depth != 0.0f)
        ? std::clamp(tel.filterResoMod / (0.5f * std::max(0.01f, std::abs(reso1Depth))), -1.0f, 1.0f)
        : 0.0f;
    flt1Reso_.setModulation(reso1Depth, reso1Live);

    // 3. Filter 2 Cutoff
    const float cut2Depth = modMatrix.getTotalDepth(nova::dsp::ModMatrix::Destination::Filter2Cutoff);
    const float cut2Live = (cut2Depth != 0.0f)
        ? std::clamp(tel.filter2CutoffMod / (1.8f * std::max(0.01f, std::abs(cut2Depth))), -1.0f, 1.0f)
        : 0.0f;
    flt2Cutoff_.setModulation(cut2Depth, cut2Live);

    // 4. Filter 2 Reso
    const float reso2Depth = modMatrix.getTotalDepth(nova::dsp::ModMatrix::Destination::Filter2Resonance);
    const float reso2Live = (reso2Depth != 0.0f)
        ? std::clamp(tel.filter2ResoMod / (0.5f * std::max(0.01f, std::abs(reso2Depth))), -1.0f, 1.0f)
        : 0.0f;
    flt2Reso_.setModulation(reso2Depth, reso2Live);

    // Live modulated filter curve position
    const auto params = proc_.getCurrentParameters();
    const float effCut1 = std::clamp(
        params.filterCutoffHz * std::exp2(tel.filterCutoffMod + tel.lastEnvVal * params.filterEnvAmount * 2.1f),
        20.0f, 20000.0f);
    const float effCut2 = std::clamp(
        params.filter2CutoffHz * std::exp2(tel.filter2CutoffMod + tel.lastEnvVal * params.filter2EnvAmount * 2.1f),
        20.0f, 20000.0f);

    if (std::abs(effCut1 - lastEffCutoff1_) / std::max(20.0f, lastEffCutoff1_) > 0.005f ||
        std::abs(effCut2 - lastEffCutoff2_) / std::max(20.0f, lastEffCutoff2_) > 0.005f) {
        lastEffCutoff1_ = effCut1;
        lastEffCutoff2_ = effCut2;
        repaint();
    }
}

void DualFilterDeck::resized() {
    const int bridgeW = 104;
    const int fltW = (getWidth() - bridgeW - 16) / 2;
    const int flt1X = 6;
    const int flt2X = flt1X + fltW + bridgeW + 8;

    const int knobH = std::clamp(getHeight() - 86, 56, 72);
    const int knobY = getHeight() - knobH - 8;
    const int knobW = (fltW - 14) / 5;

    // Filter 1 Knobs
    flt1Cutoff_.setBounds(flt1X + 4, knobY, knobW, knobH);
    flt1Reso_.setBounds(flt1X + 4 + knobW, knobY, knobW, knobH);
    flt1Drive_.setBounds(flt1X + 4 + knobW * 2, knobY, knobW, knobH);
    flt1EnvAmt_.setBounds(flt1X + 4 + knobW * 3, knobY, knobW, knobH);
    flt1Warmth_.setBounds(flt1X + 4 + knobW * 4, knobY, knobW, knobH);

    // Filter 2 Knobs
    flt2Cutoff_.setBounds(flt2X + 4, knobY, knobW, knobH);
    flt2Reso_.setBounds(flt2X + 4 + knobW, knobY, knobW, knobH);
    flt2Drive_.setBounds(flt2X + 4 + knobW * 2, knobY, knobW, knobH);
    flt2EnvAmt_.setBounds(flt2X + 4 + knobW * 3, knobY, knobW, knobH);
    flt2Carve_.setBounds(flt2X + 4 + knobW * 4, knobY, knobW, knobH);
}

void DualFilterDeck::mouseDown(const juce::MouseEvent& e) {
    const auto pos = e.getPosition();
    auto& apvts = proc_.getAPVTS();

    // Filter 1 Power (Perfect circle click detection)
    if (flt1PowerRect_.contains(pos)) {
        const auto params = proc_.getCurrentParameters();
        if (auto* p = apvts.getParameter("filter1Enabled")) {
            p->setValueNotifyingHost(params.filter1Enabled ? 0.0f : 1.0f);
        }
        if (onFilterStateChanged) onFilterStateChanged();
        repaint();
        return;
    }

    // Filter 2 Power
    if (flt2PowerRect_.contains(pos)) {
        const auto params = proc_.getCurrentParameters();
        if (auto* p = apvts.getParameter("filter2Enabled")) {
            p->setValueNotifyingHost(params.filter2Enabled ? 0.0f : 1.0f);
        }
        if (onFilterStateChanged) onFilterStateChanged();
        repaint();
        return;
    }

    // Filter 1 Type
    if (flt1TypeRect_.contains(pos)) {
        openFilterTypeMenu(1, flt1TypeRect_);
        return;
    }

    // Filter 2 Type
    if (flt2TypeRect_.contains(pos)) {
        openFilterTypeMenu(2, flt2TypeRect_);
        return;
    }

    // Routing buttons
    if (serialBtnRect_.contains(pos)) {
        if (auto* p = apvts.getParameter("filterRoutingMode")) {
            p->setValueNotifyingHost(0.0f);
        }
        if (onFilterStateChanged) onFilterStateChanged();
        repaint();
        return;
    }
    if (parallelBtnRect_.contains(pos)) {
        if (auto* p = apvts.getParameter("filterRoutingMode")) {
            p->setValueNotifyingHost(1.0f);
        }
        if (onFilterStateChanged) onFilterStateChanged();
        repaint();
        return;
    }

    // Curve dragging Filter 1
    if (flt1CurveArea_.contains(e.position)) {
        isDraggingFlt1_ = true;
        updateFilterFromMouse(1, e.position);
        return;
    }

    // Curve dragging Filter 2
    if (flt2CurveArea_.contains(e.position)) {
        isDraggingFlt2_ = true;
        updateFilterFromMouse(2, e.position);
        return;
    }
}

void DualFilterDeck::mouseDrag(const juce::MouseEvent& e) {
    if (isDraggingFlt1_) {
        updateFilterFromMouse(1, e.position);
    } else if (isDraggingFlt2_) {
        updateFilterFromMouse(2, e.position);
    }
}

void DualFilterDeck::mouseUp(const juce::MouseEvent& /*e*/) {
    isDraggingFlt1_ = false;
    isDraggingFlt2_ = false;
    repaint();
}

void DualFilterDeck::updateFilterFromMouse(int fltNum, juce::Point<float> pos) {
    auto& apvts = proc_.getAPVTS();
    const auto& area = (fltNum == 1) ? flt1CurveArea_ : flt2CurveArea_;
    if (area.getWidth() <= 1.0f || area.getHeight() <= 1.0f) return;

    // X: 20 Hz to 20,000 Hz Logarithmic
    const float normX = std::clamp((pos.x - area.getX()) / area.getWidth(), 0.0f, 1.0f);
    const float cutoffHz = 20.0f * std::pow(1000.0f, normX);

    // Y: 0.05 to 0.98 Resonance
    const float normY = std::clamp(1.0f - (pos.y - area.getY()) / area.getHeight(), 0.0f, 1.0f);
    const float reso = std::clamp(0.05f + normY * 0.93f, 0.05f, 0.98f);

    if (fltNum == 1) {
        if (auto* p = apvts.getParameter("filterCutoffHz")) {
            p->setValueNotifyingHost(p->convertTo0to1(cutoffHz));
        }
        if (auto* p = apvts.getParameter("filterResonance")) {
            p->setValueNotifyingHost(p->convertTo0to1(reso));
        }
    } else {
        if (auto* p = apvts.getParameter("filter2CutoffHz")) {
            p->setValueNotifyingHost(p->convertTo0to1(cutoffHz));
        }
        if (auto* p = apvts.getParameter("filter2Resonance")) {
            p->setValueNotifyingHost(p->convertTo0to1(reso));
        }
    }
    repaint();
}

void DualFilterDeck::openFilterTypeMenu(int fltNum, juce::Rectangle<int> rect) {
    juce::PopupMenu m;
    if (fltNum == 1) {
        const auto params = proc_.getCurrentParameters();
        m.addItem(1, "Lowpass 24dB", true, params.filterType == 0);
        m.addItem(2, "Bandpass 12dB", true, params.filterType == 1);
        m.addItem(3, "Highpass 12dB", true, params.filterType == 2);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(rect)).withStandardItemHeight(28),
            [this](int res) {
                if (res >= 1 && res <= 3) {
                    if (auto* p = proc_.getAPVTS().getParameter("filterType")) {
                        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(res - 1)));
                    }
                    repaint();
                }
            });
    } else {
        const auto params = proc_.getCurrentParameters();
        m.addItem(1, "Lowpass 12dB", true, params.filter2Type == 0);
        m.addItem(2, "Bandpass 12dB", true, params.filter2Type == 1);
        m.addItem(3, "Highpass 12dB", true, params.filter2Type == 2);
        m.addItem(4, "Bell / Vocal Carve", true, params.filter2Type == 3);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(rect)).withStandardItemHeight(28),
            [this](int res) {
                if (res >= 1 && res <= 4) {
                    if (auto* p = proc_.getAPVTS().getParameter("filter2Type")) {
                        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(res - 1)));
                    }
                    repaint();
                }
            });
    }
}

juce::String DualFilterDeck::getFilterTypeName(int fltNum, int type) {
    if (fltNum == 1) {
        switch (type) {
            case 0: return "LOW 24";
            case 1: return "BAND 12";
            case 2: return "HIGH 12";
            default: return "LOW 24";
        }
    } else {
        switch (type) {
            case 0: return "LOW 12";
            case 1: return "BAND 12";
            case 2: return "HIGH 12";
            case 3: return "BELL";
            default: return "BELL";
        }
    }
}

} // namespace nova::synth
