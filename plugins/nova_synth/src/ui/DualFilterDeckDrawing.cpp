#include "DualFilterDeck.h"
#include "../NovaSynthProcessor.h"
#include "nova_dsp/filters/StateVariableFilter.h"
#include <algorithm>
#include <cmath>

namespace nova::synth {

void DualFilterDeck::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(theme_.bgSurface);
    g.fillRoundedRectangle(bounds, theme_.cornerRadius);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(bounds, theme_.cornerRadius, 1.0f);

    const auto params = proc_.getCurrentParameters();

    // Layout: Filter 1 (width ~42%), Bridge (width ~16%), Filter 2 (width ~42%)
    const int bridgeW = 104;
    const int fltW = (getWidth() - bridgeW - 16) / 2;

    const juce::Rectangle<int> flt1Area(6, 6, fltW, getHeight() - 12);
    const juce::Rectangle<int> bridgeArea(flt1Area.getRight() + 4, 6, bridgeW, getHeight() - 12);
    const juce::Rectangle<int> flt2Area(bridgeArea.getRight() + 4, 6, fltW, getHeight() - 12);

    drawFilterBlock(g, flt1Area, 1, params.filter1Enabled, params.filterType, params.filterCutoffHz,
                    params.filterResonance, juce::Colour(0xff38bdf8), flt1TypeRect_, flt1PowerRect_,
                    flt1CurveArea_, isDraggingFlt1_);
    drawRoutingBridge(g, bridgeArea, params);
    drawFilterBlock(g, flt2Area, 2, params.filter2Enabled, params.filter2Type, params.filter2CutoffHz,
                    params.filter2Resonance, juce::Colour(0xffc084fc), flt2TypeRect_, flt2PowerRect_,
                    flt2CurveArea_, isDraggingFlt2_);
}

void DualFilterDeck::drawFilterBlock(juce::Graphics& g, juce::Rectangle<int> area, int fltNum, bool isEnabled,
                                     int fltType, float cutoffHz, float resonance, juce::Colour accentCol,
                                     juce::Rectangle<int>& typeRectOut, juce::Rectangle<int>& powRectOut,
                                     juce::Rectangle<float>& curveAreaOut, bool isDragging) {
    // Sub-panel box
    const auto bF = area.toFloat();
    g.setColour(theme_.bgControl.withAlpha(0.40f));
    g.fillRoundedRectangle(bF, 5.0f);
    g.setColour(isEnabled ? accentCol.withAlpha(0.35f) : theme_.borderSubtle);
    g.drawRoundedRectangle(bF, 5.0f, 0.8f);

    // Header
    auto header = area.reduced(6, 4).removeFromTop(20);
    powRectOut = header.removeFromLeft(18);
    
    // Exact circle power button (never oval)
    const auto powCircle = powRectOut.withSizeKeepingCentre(10, 10).toFloat();
    g.setColour(isEnabled ? accentCol : theme_.textMuted.withAlpha(0.35f));
    if (isEnabled) g.fillEllipse(powCircle);
    else g.drawEllipse(powCircle, 1.4f);

    g.setFont(theme_.getSectionFont(11.0f));
    g.setColour(isEnabled ? juce::Colours::white : theme_.textMuted);
    g.drawFittedText("FILTER " + juce::String(fltNum), header.removeFromLeft(68).reduced(4, 0),
                     juce::Justification::centredLeft, 1, 0.85f);

    // Type Pill with generous width
    typeRectOut = header.removeFromRight(88).reduced(1, 0);
    g.setColour(theme_.bgElevated);
    g.fillRoundedRectangle(typeRectOut.toFloat(), 3.0f);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(typeRectOut.toFloat(), 3.0f, 0.8f);

    auto rType = typeRectOut.reduced(3, 1);
    const float triW = 5.0f, triH = 3.5f;
    auto triRect = rType.removeFromRight(static_cast<int>(triW + 3)).toFloat().withSizeKeepingCentre(triW, triH);
    juce::Path tri;
    tri.addTriangle(triRect.getX(), triRect.getY(), triRect.getRight(), triRect.getY(), triRect.getCentreX(), triRect.getBottom());
    g.setColour(accentCol.withAlpha(0.80f));
    g.fillPath(tri);

    g.setFont(theme_.getValueFont(10.0f));
    g.setColour(accentCol);
    g.drawFittedText(getFilterTypeName(fltNum, fltType), rType, juce::Justification::centred, 1, 0.85f);

    // Interactive Frequenzgang-Kurve (Response Curve)
    const float curveH = 46.0f;
    const auto curveArea = juce::Rectangle<float>(static_cast<float>(area.getX() + 8),
                                                  static_cast<float>(area.getY() + 26),
                                                  static_cast<float>(area.getWidth() - 16),
                                                  curveH);
    curveAreaOut = curveArea;

    g.setColour(theme_.bgSurface.darker(0.3f));
    g.fillRoundedRectangle(curveArea, 3.0f);
    g.setColour(theme_.borderSubtle.withAlpha(0.5f));
    g.drawRoundedRectangle(curveArea, 3.0f, 0.8f);

    // Grid lines
    g.setColour(theme_.borderSubtle.withAlpha(0.25f));
    g.drawHorizontalLine(static_cast<int>(curveArea.getY() + curveArea.getHeight() * 0.5f),
                         curveArea.getX(), curveArea.getRight());

    // Plot SVF Magnitude Response (Analytical DSP calculation)
    juce::Path curve;
    juce::Path fillPath;
    const float w = curveArea.getWidth(), h = curveArea.getHeight();
    const float minFreq = 20.0f, maxFreq = 20000.0f;
    const float logMin = std::log10(minFreq), logMax = std::log10(maxFreq);
    const float normCutoff = std::clamp((std::log10(std::max(cutoffHz, minFreq)) - logMin) / (logMax - logMin), 0.0f, 1.0f);

    nova::dsp::StateVariableFilter svf;
    svf.prepare(44100.0);
    auto svfType = nova::dsp::StateVariableFilter::Type::LowPass;
    if (fltType == 1) svfType = nova::dsp::StateVariableFilter::Type::BandPass;
    else if (fltType == 2) svfType = nova::dsp::StateVariableFilter::Type::HighPass;
    else if (fltType == 3) svfType = nova::dsp::StateVariableFilter::Type::Bell;

    const float q = std::max(0.15f, resonance);
    svf.setParameters(svfType, cutoffHz, q, fltType == 3 ? (resonance * 10.0f - 5.0f) : 0.0f);

    fillPath.startNewSubPath(curveArea.getX(), curveArea.getBottom());

    for (int i = 0; i <= 64; ++i) {
        const float normX = static_cast<float>(i) / 64.0f;
        const float x = curveArea.getX() + normX * w;
        const float freqHz = std::pow(10.0f, logMin + normX * (logMax - logMin));

        float mag = svf.getMagnitudeAtFrequency(freqHz);
        if (fltNum == 1 && (fltType == 0 || fltType == 2)) {
            // Cascaded 24dB / 4-pole slope
            mag = mag * mag;
        }

        // Map magnitude (dB) to vertical display height: -36dB to +12dB
        const float db = 20.0f * std::log10(std::max(1e-3f, mag));
        const float normH = std::clamp((db + 36.0f) / 48.0f, 0.0f, 1.0f);
        const float y = std::clamp(curveArea.getBottom() - normH * (h * 0.90f),
                                   curveArea.getY() + 2.0f, curveArea.getBottom());

        if (i == 0) curve.startNewSubPath(x, y);
        else curve.lineTo(x, y);
        fillPath.lineTo(x, y);
    }

    fillPath.lineTo(curveArea.getRight(), curveArea.getBottom());
    fillPath.closeSubPath();

    // Draw fill & glowing stroke
    g.setColour(accentCol.withAlpha(isEnabled ? 0.14f : 0.03f));
    g.fillPath(fillPath);
    g.setColour(isEnabled ? accentCol : theme_.textMuted.withAlpha(0.4f));
    g.strokePath(curve, juce::PathStrokeType(1.5f));

    // Interactive Cutoff & Reso Node Handle (Circle, Never Oval)
    float cutMag = svf.getMagnitudeAtFrequency(cutoffHz);
    if (fltNum == 1 && (fltType == 0 || fltType == 2)) {
        cutMag = cutMag * cutMag;
    }
    const float cutDb = 20.0f * std::log10(std::max(1e-3f, cutMag));
    const float cutNormH = std::clamp((cutDb + 36.0f) / 48.0f, 0.0f, 1.0f);
    const float dotX = curveArea.getX() + normCutoff * w;
    const float dotY = std::clamp(curveArea.getBottom() - cutNormH * (h * 0.90f),
                                  curveArea.getY() + 4.0f, curveArea.getBottom() - 4.0f);

    if (isDragging) {
        g.setColour(accentCol.withAlpha(0.35f));
        g.fillEllipse(dotX - 8.0f, dotY - 8.0f, 16.0f, 16.0f);
    }
    g.setColour(juce::Colours::white);
    g.fillEllipse(dotX - 3.5f, dotY - 3.5f, 7.0f, 7.0f);
    g.setColour(accentCol);
    g.drawEllipse(dotX - 4.5f, dotY - 4.5f, 9.0f, 9.0f, 1.4f);

    // 5. Live Modulated Dynamic Response Node & Ghost Curve (dancing with LFO / Envelope)
    const float effCutoff = (fltNum == 1) ? lastEffCutoff1_ : lastEffCutoff2_;
    if (isEnabled && std::abs(effCutoff - cutoffHz) > 8.0f) {
        const float normLiveCutoff = std::clamp((std::log10(std::max(effCutoff, minFreq)) - logMin) /
                                                (logMax - logMin), 0.0f, 1.0f);
        nova::dsp::StateVariableFilter liveSvf;
        liveSvf.prepare(44100.0);
        liveSvf.setParameters(svfType, effCutoff, q, fltType == 3 ? (resonance * 10.0f - 5.0f) : 0.0f);

        // Subtle animated ghost curve
        juce::Path liveCurve;
        for (int i = 0; i <= 32; ++i) {
            const float nX = static_cast<float>(i) / 32.0f;
            const float lx = curveArea.getX() + nX * w;
            const float lFreq = std::pow(10.0f, logMin + nX * (logMax - logMin));
            float lMag = liveSvf.getMagnitudeAtFrequency(lFreq);
            if (fltNum == 1 && (fltType == 0 || fltType == 2)) lMag = lMag * lMag;
            const float lDb = 20.0f * std::log10(std::max(1e-3f, lMag));
            const float lNormH = std::clamp((lDb + 36.0f) / 48.0f, 0.0f, 1.0f);
            const float ly = std::clamp(curveArea.getBottom() - lNormH * (h * 0.90f),
                                        curveArea.getY() + 2.0f, curveArea.getBottom());
            if (i == 0) liveCurve.startNewSubPath(lx, ly);
            else liveCurve.lineTo(lx, ly);
        }
        g.setColour(theme_.accentSecondary.withAlpha(0.35f));
        g.strokePath(liveCurve, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Animated connecting beam along filter curve
        float liveCutMag = liveSvf.getMagnitudeAtFrequency(effCutoff);
        if (fltNum == 1 && (fltType == 0 || fltType == 2)) liveCutMag = liveCutMag * liveCutMag;
        const float liveCutDb = 20.0f * std::log10(std::max(1e-3f, liveCutMag));
        const float liveCutNormH = std::clamp((liveCutDb + 36.0f) / 48.0f, 0.0f, 1.0f);
        const float liveDotX = curveArea.getX() + normLiveCutoff * w;
        const float liveDotY = std::clamp(curveArea.getBottom() - liveCutNormH * (h * 0.90f),
                                          curveArea.getY() + 4.0f, curveArea.getBottom() - 4.0f);

        // Subtle connecting line between static node and live modulated node
        g.setColour(theme_.accentSecondary.withAlpha(0.35f));
        g.drawLine(dotX, dotY, liveDotX, liveDotY, 1.0f);

        // Glowing live modulation pip (dancing with LFO)
        g.setColour(theme_.accentSecondary.withAlpha(0.40f));
        g.fillEllipse(liveDotX - 5.5f, liveDotY - 5.5f, 11.0f, 11.0f);
        g.setColour(theme_.accentSecondary);
        g.fillEllipse(liveDotX - 3.5f, liveDotY - 3.5f, 7.0f, 7.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(liveDotX - 1.5f, liveDotY - 1.5f, 3.0f, 3.0f);
    }
}

void DualFilterDeck::drawRoutingBridge(juce::Graphics& g, juce::Rectangle<int> area,
                                       const nova::dsp::WorshipSynthEngine::Parameters& params) {
    const auto bF = area.toFloat();
    g.setColour(juce::Colour(0xff0e131d));
    g.fillRoundedRectangle(bF, 5.0f);
    g.setColour(theme_.borderSubtle.withAlpha(0.65f));
    g.drawRoundedRectangle(bF, 5.0f, 0.8f);

    // Header: "ROUTING BRIDGE"
    g.setFont(theme_.getSectionFont(10.0f));
    g.setColour(theme_.textMuted.brighter(0.2f));
    g.drawFittedText("ROUTING BRIDGE", area.getX(), area.getY() + 4, area.getWidth(), 14, juce::Justification::centred, 1, 0.85f);

    // Mode Pill Buttons (SERIAL / PARALLEL)
    const int btnW = 45, btnH = 20, btnY = area.getY() + 22;
    serialBtnRect_   = juce::Rectangle<int>(area.getCentreX() - btnW - 1, btnY, btnW, btnH);
    parallelBtnRect_ = juce::Rectangle<int>(area.getCentreX() + 1, btnY, btnW, btnH);

    const bool isSerial = (params.filterRoutingMode == 0);

    // Serial Button
    g.setColour(isSerial ? theme_.accentPrimary.withAlpha(0.28f) : theme_.bgElevated.withAlpha(0.6f));
    g.fillRoundedRectangle(serialBtnRect_.toFloat(), 3.0f);
    g.setColour(isSerial ? theme_.accentPrimary : theme_.borderSubtle);
    g.drawRoundedRectangle(serialBtnRect_.toFloat(), 3.0f, isSerial ? 1.2f : 0.8f);
    g.setFont(theme_.getValueFont(9.5f));
    g.setColour(isSerial ? juce::Colours::white : theme_.textMuted);
    g.drawFittedText("SERIAL", serialBtnRect_, juce::Justification::centred, 1, 0.85f);

    // Parallel Button
    g.setColour(!isSerial ? theme_.accentPrimary.withAlpha(0.28f) : theme_.bgElevated.withAlpha(0.6f));
    g.fillRoundedRectangle(parallelBtnRect_.toFloat(), 3.0f);
    g.setColour(!isSerial ? theme_.accentPrimary : theme_.borderSubtle);
    g.drawRoundedRectangle(parallelBtnRect_.toFloat(), 3.0f, !isSerial ? 1.2f : 0.8f);
    g.setFont(theme_.getValueFont(9.5f));
    g.setColour(!isSerial ? juce::Colours::white : theme_.textMuted);
    g.drawFittedText("PARALLEL", parallelBtnRect_, juce::Justification::centred, 1, 0.85f);

    // Schematic Diagram Card in Center
    const int diagY = btnY + 26;
    const int diagH = area.getBottom() - diagY - 28;
    const juce::Rectangle<float> diagCard(static_cast<float>(area.getX() + 5), static_cast<float>(diagY),
                                         static_cast<float>(area.getWidth() - 10), static_cast<float>(diagH));
    g.setColour(theme_.bgSurface.darker(0.35f));
    g.fillRoundedRectangle(diagCard, 4.0f);
    g.setColour(theme_.borderSubtle.withAlpha(0.4f));
    g.drawRoundedRectangle(diagCard, 4.0f, 0.8f);

    const float midX = diagCard.getCentreX(), midY = diagCard.getCentreY();
    const juce::Colour colFlt1(0xff38bdf8), colFlt2(0xffc084fc);

    if (isSerial) {
        // SERIAL FLOW: IN -> [ FLT 1 ] -> [ FLT 2 ] -> OUT
        const float blockW = 32.0f, blockH = 22.0f;
        const float f1X = midX - blockW - 5.0f, f2X = midX + 5.0f;
        const float blockY = midY - blockH * 0.5f - 4.0f;

        const juce::Rectangle<float> f1Box(f1X, blockY, blockW, blockH);
        const juce::Rectangle<float> f2Box(f2X, blockY, blockW, blockH);

        // Flow Wires
        g.setColour(theme_.borderFocus.withAlpha(0.6f));
        g.drawHorizontalLine(static_cast<int>(blockY + blockH * 0.5f), diagCard.getX() + 6.0f, f1X);

        const float arrowY = blockY + blockH * 0.5f;
        g.drawHorizontalLine(static_cast<int>(arrowY), f1Box.getRight(), f2X);
        juce::Path arrow1;
        arrow1.startNewSubPath(midX - 1.0f, arrowY - 3.5f);
        arrow1.lineTo(midX + 2.5f, arrowY);
        arrow1.lineTo(midX - 1.0f, arrowY + 3.5f);
        g.setColour(theme_.accentPrimary);
        g.strokePath(arrow1, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(theme_.borderFocus.withAlpha(0.6f));
        g.drawHorizontalLine(static_cast<int>(arrowY), f2Box.getRight(), diagCard.getRight() - 6.0f);
        juce::Path arrow2;
        arrow2.startNewSubPath(diagCard.getRight() - 8.0f, arrowY - 3.5f);
        arrow2.lineTo(diagCard.getRight() - 4.5f, arrowY);
        arrow2.lineTo(diagCard.getRight() - 8.0f, arrowY + 3.5f);
        g.setColour(theme_.textMuted);
        g.strokePath(arrow2, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // FLT 1 Block
        g.setColour(colFlt1.withAlpha(params.filter1Enabled ? 0.22f : 0.06f));
        g.fillRoundedRectangle(f1Box, 3.0f);
        g.setColour(params.filter1Enabled ? colFlt1 : theme_.borderSubtle);
        g.drawRoundedRectangle(f1Box, 3.0f, 0.9f);
        g.setFont(theme_.getValueFont(9.0f));
        g.setColour(params.filter1Enabled ? juce::Colours::white : theme_.textMuted);
        g.drawFittedText("FLT 1", f1Box.toNearestInt(), juce::Justification::centred, 1, 0.85f);

        // FLT 2 Block
        g.setColour(colFlt2.withAlpha(params.filter2Enabled ? 0.22f : 0.06f));
        g.fillRoundedRectangle(f2Box, 3.0f);
        g.setColour(params.filter2Enabled ? colFlt2 : theme_.borderSubtle);
        g.drawRoundedRectangle(f2Box, 3.0f, 0.9f);
        g.setFont(theme_.getValueFont(9.0f));
        g.setColour(params.filter2Enabled ? juce::Colours::white : theme_.textMuted);
        g.drawFittedText("FLT 2", f2Box.toNearestInt(), juce::Justification::centred, 1, 0.85f);

        g.setFont(theme_.getValueFont(8.0f));
        g.setColour(theme_.textMuted);
        g.drawFittedText("1 -> 2 SERIES", static_cast<int>(diagCard.getX()), static_cast<int>(diagCard.getBottom() - 14.0f),
                         static_cast<int>(diagCard.getWidth()), 12, juce::Justification::centred, 1, 0.85f);
    } else {
        // PARALLEL FLOW: IN splits to FLT 1 (top) and FLT 2 (bottom) -> merges to OUT
        const float blockW = 38.0f, blockH = 17.0f;
        const float blockX = midX - blockW * 0.5f;
        const float f1Y = midY - blockH - 4.0f, f2Y = midY + 4.0f;

        const juce::Rectangle<float> f1Box(blockX, f1Y, blockW, blockH);
        const juce::Rectangle<float> f2Box(blockX, f2Y, blockW, blockH);

        const float inX = diagCard.getX() + 6.0f, splitX = blockX - 6.0f;
        const float outX = diagCard.getRight() - 6.0f, mergeX = blockX + blockW + 6.0f;

        g.setColour(theme_.borderFocus.withAlpha(0.6f));
        g.drawHorizontalLine(static_cast<int>(midY), inX, splitX);
        g.drawVerticalLine(static_cast<int>(splitX), f1Y + blockH * 0.5f, f2Y + blockH * 0.5f);
        g.drawHorizontalLine(static_cast<int>(f1Y + blockH * 0.5f), splitX, blockX);
        g.drawHorizontalLine(static_cast<int>(f2Y + blockH * 0.5f), splitX, blockX);

        g.drawHorizontalLine(static_cast<int>(f1Y + blockH * 0.5f), blockX + blockW, mergeX);
        g.drawHorizontalLine(static_cast<int>(f2Y + blockH * 0.5f), blockX + blockW, mergeX);
        g.drawVerticalLine(static_cast<int>(mergeX), f1Y + blockH * 0.5f, f2Y + blockH * 0.5f);
        g.drawHorizontalLine(static_cast<int>(midY), mergeX, outX);

        juce::Path arrowOut;
        arrowOut.startNewSubPath(outX - 3.0f, midY - 3.0f);
        arrowOut.lineTo(outX, midY);
        arrowOut.lineTo(outX - 3.0f, midY + 3.0f);
        g.setColour(theme_.textMuted);
        g.strokePath(arrowOut, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // FLT 1 Block (Top)
        g.setColour(colFlt1.withAlpha(params.filter1Enabled ? 0.22f : 0.06f));
        g.fillRoundedRectangle(f1Box, 3.0f);
        g.setColour(params.filter1Enabled ? colFlt1 : theme_.borderSubtle);
        g.drawRoundedRectangle(f1Box, 3.0f, 0.9f);
        g.setFont(theme_.getValueFont(8.5f));
        g.setColour(params.filter1Enabled ? juce::Colours::white : theme_.textMuted);
        g.drawFittedText("FLT 1", f1Box.toNearestInt(), juce::Justification::centred, 1, 0.85f);

        // FLT 2 Block (Bottom)
        g.setColour(colFlt2.withAlpha(params.filter2Enabled ? 0.22f : 0.06f));
        g.fillRoundedRectangle(f2Box, 3.0f);
        g.setColour(params.filter2Enabled ? colFlt2 : theme_.borderSubtle);
        g.drawRoundedRectangle(f2Box, 3.0f, 0.9f);
        g.setFont(theme_.getValueFont(8.5f));
        g.setColour(params.filter2Enabled ? juce::Colours::white : theme_.textMuted);
        g.drawFittedText("FLT 2", f2Box.toNearestInt(), juce::Justification::centred, 1, 0.85f);
    }

    // Sub Bypass Badge at bottom
    const juce::Rectangle<float> subBadge(static_cast<float>(area.getX() + 6),
                                          static_cast<float>(area.getBottom() - 22),
                                          static_cast<float>(area.getWidth() - 12), 17.0f);
    g.setColour(theme_.bgElevated.withAlpha(0.7f));
    g.fillRoundedRectangle(subBadge, 3.0f);
    g.setColour(juce::Colour(0xff22c55e).withAlpha(0.35f));
    g.drawRoundedRectangle(subBadge, 3.0f, 0.8f);

    // Glowing green dot indicator
    g.setColour(juce::Colour(0xff22c55e));
    g.fillEllipse(subBadge.getX() + 5.0f, subBadge.getCentreY() - 2.5f, 5.0f, 5.0f);
    g.setFont(theme_.getValueFont(8.0f));
    g.setColour(juce::Colour(0xff86efac));
    g.drawFittedText("SUB DIRECT BYPASS", static_cast<int>(subBadge.getX() + 12),
                     static_cast<int>(subBadge.getY()), static_cast<int>(subBadge.getWidth() - 14),
                     static_cast<int>(subBadge.getHeight()), juce::Justification::centred, 1, 0.85f);
}

} // namespace nova::synth

