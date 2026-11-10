#include "JornaCenterStageColumn.h"
#include "../NovaSynthProcessor.h"
#include "NovaSynthUiHelpers.h"
#include "nova_dsp/filters/StateVariableFilter.h"
#include <cmath>
#include <algorithm>

namespace nova::synth {

void JornaCenterStageColumn::drawAllSourcesStage(juce::Graphics& g, const nova::dsp::WorshipSynthEngine::Parameters& params) {
    // --- ALL SOURCES: Live Stabilized Scope When Playing, or Composite Sum + Layer Ghosts When Idle ---
    const float midY = canvasRect_.getCentreY() + 6.0f;
    const float maxAmp = canvasRect_.getHeight() * 0.38f;
    const auto tel = proc_.getSynthTelemetry();
    const float outEnergy = (tel.outputPeakL + tel.outputPeakR) * 0.5f;
    const bool isAudioActive = (tel.activeVoiceCount > 0 || outEnergy > 0.005f);

    // Zero line
    g.setColour(juce::Colour(0xff334155).withAlpha(0.35f));
    g.drawDashedLine(juce::Line<float>(canvasRect_.getX() + 6.0f, midY, canvasRect_.getRight() - 6.0f, midY), nullptr, 0, 1.0f);

    if (isAudioActive) {
        // Stabilized Zero-Crossing Oscilloscope from live output audio
        std::size_t trigIdx = (writeCursor_ + historyWave_.size() - 192) & (historyWave_.size() - 1);
        for (int off = 240; off >= 32; --off) {
            const std::size_t uOff = static_cast<std::size_t>(off);
            const std::size_t i0 = (writeCursor_ + historyWave_.size() - uOff - 1) & (historyWave_.size() - 1);
            const std::size_t i1 = (writeCursor_ + historyWave_.size() - uOff) & (historyWave_.size() - 1);
            if (historyWave_[i0] <= 0.0f && historyWave_[i1] > 0.0f) {
                trigIdx = i1;
                break;
            }
        }

        constexpr int kScopePts = 192;
        juce::Path livePath;
        juce::Path liveFill;
        liveFill.startNewSubPath(canvasRect_.getX() + 6.0f, midY);

        for (int pt = 0; pt <= kScopePts; ++pt) {
            const float u = static_cast<float>(pt) / static_cast<float>(kScopePts);
            const float x = canvasRect_.getX() + 6.0f + u * (canvasRect_.getWidth() - 12.0f);
            const std::size_t sIdx = (trigIdx + static_cast<std::size_t>(pt)) & (historyWave_.size() - 1);
            const float s = std::clamp(historyWave_[sIdx] * 1.5f, -1.0f, 1.0f);
            const float y = midY - s * (maxAmp * 0.90f);

            if (pt == 0) livePath.startNewSubPath(x, y);
            else         livePath.lineTo(x, y);
            liveFill.lineTo(x, y);
        }
        liveFill.lineTo(canvasRect_.getRight() - 6.0f, midY);
        liveFill.closeSubPath();

        juce::ColourGradient liveGrad(juce::Colour(0xff2dd4bf).withAlpha(0.30f), canvasRect_.getX(), midY - maxAmp,
                                      juce::Colour(0xff2dd4bf).withAlpha(0.03f), canvasRect_.getX(), midY + maxAmp, false);
        g.setGradientFill(liveGrad);
        g.fillPath(liveFill);

        g.setColour(juce::Colour(0xff2dd4bf).withAlpha(0.40f));
        g.strokePath(livePath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xff2dd4bf));
        g.strokePath(livePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(juce::Colours::white.withAlpha(0.90f));
        g.strokePath(livePath, juce::PathStrokeType(0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Pulsing Green Live Scope Indicator
        g.setColour(juce::Colour(0xff22c55e));
        g.fillEllipse(canvasRect_.getX() + 12.0f, canvasRect_.getY() + 11.0f, 6.0f, 6.0f);
        g.setFont(theme_.getSectionFont(11.5f));
        g.setColour(juce::Colour(0xff2dd4bf));
        g.drawText("LIVE SCOPE", static_cast<int>(canvasRect_.getX() + 24), static_cast<int>(canvasRect_.getY() + 7), 100, 14, juce::Justification::centredLeft);
    } else {
        // Idle / Theoretical Sum of all active layers
        constexpr int kPts = 240;

        // 1. Subtle ghost curves for each active layer in its own color
        for (int i = 0; i < 8; ++i) {
            const auto& slot = params.oscSlots[static_cast<std::size_t>(i)];
            if (!slot.enabled) continue;

            juce::Path layerGhost;
            for (int pt = 0; pt <= kPts; ++pt) {
                const float u = static_cast<float>(pt) / static_cast<float>(kPts);
                const float x = canvasRect_.getX() + 6.0f + u * (canvasRect_.getWidth() - 12.0f);
                const float s = calculateOscSample(slot, u * 3.0f);
                const float y = midY - s * (maxAmp * 0.40f * std::clamp(slot.level, 0.25f, 1.0f));
                if (pt == 0) layerGhost.startNewSubPath(x, y);
                else         layerGhost.lineTo(x, y);
            }
            g.setColour(getLayerColour(i).withAlpha(0.32f));
            g.strokePath(layerGhost, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 2. Composite Sum Path + Gradient Fill
        int activeSources = 0;
        for (int i = 0; i < 8; ++i) {
            if (params.oscSlots[static_cast<std::size_t>(i)].enabled) activeSources++;
        }
        const float normFactor = activeSources > 0 ? (1.0f / std::sqrt(static_cast<float>(activeSources))) : 1.0f;

        juce::Path sumPath;
        juce::Path sumFill;
        sumFill.startNewSubPath(canvasRect_.getX() + 6.0f, midY);

        for (int pt = 0; pt <= kPts; ++pt) {
            const float u = static_cast<float>(pt) / static_cast<float>(kPts);
            const float x = canvasRect_.getX() + 6.0f + u * (canvasRect_.getWidth() - 12.0f);
            float sumVal = 0.0f;
            for (int i = 0; i < 8; ++i) {
                const auto& slot = params.oscSlots[static_cast<std::size_t>(i)];
                if (slot.enabled) {
                    sumVal += calculateOscSample(slot, u * 3.0f) * slot.level;
                }
            }
            const float comp = std::tanh(sumVal * normFactor * 0.95f);
            const float y = midY - comp * (maxAmp * 0.85f);
            if (pt == 0) sumPath.startNewSubPath(x, y);
            else         sumPath.lineTo(x, y);
            sumFill.lineTo(x, y);
        }
        sumFill.lineTo(canvasRect_.getRight() - 6.0f, midY);
        sumFill.closeSubPath();

        juce::ColourGradient sumGrad(juce::Colour(0xff2dd4bf).withAlpha(0.28f), canvasRect_.getX(), midY - maxAmp,
                                      juce::Colour(0xff2dd4bf).withAlpha(0.03f), canvasRect_.getX(), midY + maxAmp, false);
        g.setGradientFill(sumGrad);
        g.fillPath(sumFill);

        g.setColour(juce::Colour(0xff2dd4bf).withAlpha(0.38f));
        g.strokePath(sumPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xff2dd4bf));
        g.strokePath(sumPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.strokePath(sumPath, juce::PathStrokeType(0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setFont(theme_.getSectionFont(11.5f));
        g.setColour(juce::Colour(0xff2dd4bf));
        g.drawText("COMPOSITE HARMONIC SUM", static_cast<int>(canvasRect_.getX() + 12), static_cast<int>(canvasRect_.getY() + 7), 180, 14, juce::Justification::centredLeft);
    }

    // Clickable layer pills along bottom of canvas
    allSourcesBadges_.clear();
    int bX = static_cast<int>(canvasRect_.getX() + 10.0f);
    const int bY = static_cast<int>(canvasRect_.getBottom() - 24.0f);
    for (int i = 0; i < 8; ++i) {
        if (params.oscSlots[static_cast<std::size_t>(i)].enabled) {
            const juce::String name = getLayerShortName(params, i);
            const int bw = 54;
            const juce::Rectangle<int> bRect(bX, bY, bw, 18);
            allSourcesBadges_.push_back({ bRect, i });

            const auto c = getLayerColour(i);
            g.setColour(c.withAlpha(0.18f));
            g.fillRoundedRectangle(bRect.toFloat(), 3.0f);
            g.setColour(c.withAlpha(0.85f));
            g.drawRoundedRectangle(bRect.toFloat(), 3.0f, 1.0f);
            g.setFont(theme_.getLabelFont(10.5f));
            g.setColour(c);
            g.drawText(name, bRect, juce::Justification::centred);

            bX += bw + 6;
        }
    }
}

void JornaCenterStageColumn::drawFilterStage(juce::Graphics& g, const nova::dsp::WorshipSynthEngine::Parameters& params) {
    nova::dsp::StateVariableFilter svf1, svf2;
    svf1.prepare(48000.0);
    svf2.prepare(48000.0);
    auto svf1Type = nova::dsp::StateVariableFilter::Type::LowPass;
    if (params.filterType == 1) svf1Type = nova::dsp::StateVariableFilter::Type::BandPass;
    if (params.filterType == 2) svf1Type = nova::dsp::StateVariableFilter::Type::HighPass;
    svf1.setParameters(svf1Type, params.filterCutoffHz, params.filterResonance);

    auto svf2Type = nova::dsp::StateVariableFilter::Type::LowPass;
    if (params.filter2Type == 1) svf2Type = nova::dsp::StateVariableFilter::Type::BandPass;
    else if (params.filter2Type == 2) svf2Type = nova::dsp::StateVariableFilter::Type::HighPass;
    else if (params.filter2Type == 3) svf2Type = nova::dsp::StateVariableFilter::Type::Bell;
    svf2.setParameters(svf2Type, params.filter2CutoffHz, params.filter2Resonance);

    juce::Path fPath;
    fPath.startNewSubPath(canvasRect_.getX(), canvasRect_.getBottom());
    for (int i = 0; i <= 96; ++i) {
        const float nx = static_cast<float>(i) / 96.0f;
        const float freq = 20.0f * std::pow(1000.0f, nx);
        float mag = 1.0f;
        if (centerViewMode_ == 2) {
            mag = svf1.getMagnitudeAtFrequency(freq);
        } else {
            mag = svf2.getMagnitudeAtFrequency(freq);
        }
        const float db = 20.0f * std::log10(std::max(0.04f, mag));
        const float ny = std::clamp(0.40f - (db / 35.0f), 0.06f, 0.94f);
        fPath.lineTo(canvasRect_.getX() + nx * canvasRect_.getWidth(),
                     canvasRect_.getY() + ny * canvasRect_.getHeight());
    }
    fPath.lineTo(canvasRect_.getRight(), canvasRect_.getBottom());
    fPath.closeSubPath();

    juce::Colour filterCol = (centerViewMode_ == 2) ? juce::Colour(0xff38bdf8) : juce::Colour(0xff2dd4bf);
    g.setColour(filterCol.withAlpha(0.25f));
    g.fillPath(fPath);
    g.setColour(filterCol);
    g.strokePath(fPath, juce::PathStrokeType(2.0f));

    g.setFont(theme_.getSectionFont(11.5f));
    g.setColour(filterCol);
    g.drawText(centerViewMode_ == 2 ? "FILTER 1 RESPONSE (LADDER / SVF)" : "FILTER 2 RESPONSE (VOCAL CARVE)",
               static_cast<int>(canvasRect_.getX() + 12), static_cast<int>(canvasRect_.getY() + 7), 240, 14,
               juce::Justification::centredLeft);
}

void JornaCenterStageColumn::drawMacroStage(juce::Graphics& g, const nova::dsp::WorshipSynthEngine::Parameters& params) {
    g.setColour(juce::Colour(0xff334155).withAlpha(0.25f));
    for (int gx = 1; gx < 4; ++gx) {
        const float lx = canvasRect_.getX() + static_cast<float>(gx) * (canvasRect_.getWidth() / 4.0f);
        g.drawDashedLine(juce::Line<float>(lx, canvasRect_.getY() + 4.0f, lx, canvasRect_.getBottom() - 4.0f), nullptr, 0, 1.0f);
    }
    for (int gy = 1; gy < 4; ++gy) {
        const float ly = canvasRect_.getY() + static_cast<float>(gy) * (canvasRect_.getHeight() / 4.0f);
        g.drawDashedLine(juce::Line<float>(canvasRect_.getX() + 4.0f, ly, canvasRect_.getRight() - 4.0f, ly), nullptr, 0, 1.0f);
    }

    const float padX = canvasRect_.getX() + 20.0f + params.macroWarmth * (canvasRect_.getWidth() - 40.0f);
    const float padY = canvasRect_.getBottom() - 20.0f - params.macroAir * (canvasRect_.getHeight() - 40.0f);

    g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.20f));
    g.fillEllipse(padX - 20.0f, padY - 20.0f, 40.0f, 40.0f);
    g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.60f));
    g.drawEllipse(padX - 20.0f, padY - 20.0f, 40.0f, 40.0f, 1.2f);
    g.setColour(juce::Colour(0xfff59e0b));
    g.fillEllipse(padX - 5.0f, padY - 5.0f, 10.0f, 10.0f);
    g.setColour(juce::Colours::white);
    g.drawEllipse(padX - 5.0f, padY - 5.0f, 10.0f, 10.0f, 1.5f);

    g.setFont(theme_.getSectionFont(11.5f));
    g.setColour(juce::Colour(0xfff59e0b));
    g.drawText("MACRO 2D PAD: WARMTH (X) & AIR (Y)",
               static_cast<int>(canvasRect_.getX() + 12), static_cast<int>(canvasRect_.getY() + 7), 260, 14,
               juce::Justification::centredLeft);
}

} // namespace nova::synth
