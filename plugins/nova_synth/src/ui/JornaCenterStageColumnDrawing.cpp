#include "JornaCenterStageColumn.h"
#include "../NovaSynthProcessor.h"
#include "NovaSynthUiHelpers.h"
#include <cmath>
#include <algorithm>
#include <array>

namespace nova::synth {

float JornaCenterStageColumn::calculateOscSample(const nova::dsp::MultiEngineOscillator::Parameters& slot, float phaseNorm) noexcept {
    using ET = nova::dsp::MultiEngineOscillator::EngineType;
    const float cyc = std::fmod(std::abs(phaseNorm), 1.0f);
    const float morph = slot.shapeMorph;

    if (slot.sourceType == 1 || slot.engine == ET::SubOscillator) {
        const float sinWave = std::sin(cyc * 6.283185f);
        const float triWave = 2.0f * std::abs(2.0f * (cyc - 0.5f)) - 1.0f;
        const float sqrWave = (cyc < 0.5f ? 0.9f : -0.9f);
        switch (slot.modelIndex) {
            case 0: return sinWave;
            case 1: return (1.0f - morph * 0.5f) * triWave + (morph * 0.5f) * sinWave;
            case 2: return std::tanh(sinWave * (1.8f + morph * 2.0f)) * 0.9f;
            case 3: return sqrWave;
            default: return sinWave;
        }
    }
    if (slot.sourceType == 2 || slot.engine == ET::PhysicalPluckModal) {
        return std::exp(-cyc * (2.4f + morph * 3.0f)) * std::sin(cyc * 6.283185f * (1.0f + morph * 2.0f));
    }
    if (slot.sourceType == 3 || slot.engine == ET::NoiseTransient) {
        return 0.75f * std::sin(cyc * 37.0f) * std::cos(cyc * 21.0f) * std::exp(-cyc * 3.5f);
    }
    if (slot.engine == ET::WarmAnalog) {
        const float saw = 1.0f - 2.0f * cyc;
        const float pulse = (cyc < (0.25f + 0.5f * morph)) ? 0.9f : -0.9f;
        const float tri = 2.0f * std::abs(2.0f * (cyc - 0.5f)) - 1.0f;
        const float sinW = std::sin(cyc * 6.283185f);
        switch (slot.modelIndex) {
            case 0: return saw;
            case 1: return pulse;
            case 2: return tri;
            case 3: return sinW;
            case 4: return 0.65f * saw + 0.35f * (1.0f - 2.0f * std::fmod(cyc * 2.0f, 1.0f));
            case 5: return 0.85f * (1.0f - 2.0f * std::fmod(cyc * (1.5f + morph * 2.0f), 1.0f));
            default: return (1.0f - morph) * saw + morph * pulse;
        }
    }

    // Wavetable / Organic Texture
    const float harm = 1.0f + static_cast<float>(slot.modelIndex);
    const float w1 = std::sin(cyc * 6.283185f);
    const float w2 = 0.45f * std::sin(cyc * 6.283185f * harm + morph * 3.14159f);
    const float w3 = 0.25f * std::sin(cyc * 18.84955f);
    return (w1 + w2 + w3) / 1.70f;
}

void JornaCenterStageColumn::drawSpectrumOutput(juce::Graphics& g, const juce::Rectangle<float>& specRect) {
    g.setColour(theme_.bgSurface);
    g.fillRoundedRectangle(specRect, theme_.cornerRadius);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(specRect, theme_.cornerRadius, 1.0f);

    g.setFont(theme_.getSectionFont(13.0f));
    g.setColour(juce::Colour(0xff38bdf8));
    g.drawText("SPECTRUM", static_cast<int>(specRect.getX() + 12), static_cast<int>(specRect.getY() + 6),
               72, 16, juce::Justification::centredLeft);
    g.setFont(theme_.getLabelFont(11.0f));
    g.setColour(theme_.textMuted);
    g.drawText("OUTPUT", static_cast<int>(specRect.getX() + 84), static_cast<int>(specRect.getY() + 6),
               55, 16, juce::Justification::centredLeft);

    // WAVE | SPEC toggle text
    specModeToggleBounds_ = juce::Rectangle<int>(
        static_cast<int>(specRect.getCentreX() + 20.0f), static_cast<int>(specRect.getY() + 6.0f), 80, 16);
    g.setFont(theme_.getSectionFont(11.0f));
    g.setColour(!showSpectrumMode_ ? juce::Colour(0xff38bdf8) : theme_.textMuted);
    g.drawText("WAVE", specModeToggleBounds_.withWidth(38), juce::Justification::centred);
    g.setColour(showSpectrumMode_ ? juce::Colour(0xff38bdf8) : theme_.textMuted);
    g.drawText("SPEC", specModeToggleBounds_.withTrimmedLeft(40), juce::Justification::centred);

    // Frequency axis labels: 20  100  500  1k  5k  10k  20k
    static constexpr std::array<const char*, 7> kFreqLabels = { "20", "100", "500", "1k", "5k", "10k", "20k" };
    g.setFont(theme_.getLabelFont(10.0f));
    g.setColour(theme_.textMuted);
    const float plotLeft = specRect.getX() + 14.0f;
    const float plotRight = specRect.getRight() - 14.0f;
    for (int i = 0; i < 7; ++i) {
        const float fx = plotLeft + (static_cast<float>(i) / 6.0f) * (plotRight - plotLeft);
        g.drawText(kFreqLabels[static_cast<std::size_t>(i)],
                   static_cast<int>(fx - 15.0f), static_cast<int>(specRect.getY() + 22), 30, 12,
                   juce::Justification::centred);
    }

    // Draw Blue Filled Spectrum Mountain (or Oscilloscope Wave)
    const auto specPlot = specRect.withTrimmedTop(34.0f).reduced(12.0f, 4.0f);
    juce::Path specPath;
    specPath.startNewSubPath(specPlot.getX(), specPlot.getBottom());

    if (showSpectrumMode_) {
        for (std::size_t i = 0; i < spectrumBins_.size(); ++i) {
            const float nx = static_cast<float>(i) / static_cast<float>(spectrumBins_.size() - 1);
            const float ny = std::clamp(spectrumBins_[i], 0.02f, 0.96f);
            const float px = specPlot.getX() + nx * specPlot.getWidth();
            const float py = specPlot.getBottom() - ny * specPlot.getHeight();
            specPath.lineTo(px, py);
        }
        specPath.lineTo(specPlot.getRight(), specPlot.getBottom());
        specPath.closeSubPath();

        g.setColour(juce::Colour(0xff2563eb).withAlpha(0.65f));
        g.fillPath(specPath);
        g.setColour(juce::Colour(0xff3b82f6));
        g.strokePath(specPath, juce::PathStrokeType(1.5f));
    } else {
        juce::Path waveOut;
        for (std::size_t i = 0; i < 128; ++i) {
            const float nx = static_cast<float>(i) / 127.0f;
            const float s = historyWave_[(writeCursor_ + i * 2) & (historyWave_.size() - 1)];
            const float px = specPlot.getX() + nx * specPlot.getWidth();
            const float py = specPlot.getCentreY() - std::clamp(s * 1.6f, -0.9f, 0.9f) * (specPlot.getHeight() * 0.45f);
            if (i == 0) waveOut.startNewSubPath(px, py);
            else        waveOut.lineTo(px, py);
        }
        g.setColour(juce::Colour(0xff38bdf8));
        g.strokePath(waveOut, juce::PathStrokeType(1.8f));
    }
}

void JornaCenterStageColumn::drawCenterStageHeader(juce::Graphics& g, const juce::Rectangle<float>& stageRect, const nova::dsp::WorshipSynthEngine::Parameters& params) {
    g.setColour(theme_.bgSurface);
    g.fillRoundedRectangle(stageRect, theme_.cornerRadius);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(stageRect, theme_.cornerRadius, 1.0f);

    // Top Mode Pills: [OSC WAVE] [ALL SOURCES] [FILTER 1] [FILTER 2] [MACROS]
    static constexpr std::array<const char*, 5> kCenterTabs = {
        "OSC WAVE", "ALL SOURCES", "FILTER 1", "FILTER 2", "MACROS"
    };
    static constexpr std::array<int, 5> kTabWidths = { 76, 92, 68, 68, 64 };
    centerTabRects_.clear();
    int tx = static_cast<int>(stageRect.getX() + 10);
    const int ty = static_cast<int>(stageRect.getY() + 8);
    for (int i = 0; i < 5; ++i) {
        const int tw = kTabWidths[static_cast<std::size_t>(i)];
        const juce::Rectangle<int> tr(tx, ty, tw, 22);
        centerTabRects_.push_back(tr);
        const bool isAct = (centerViewMode_ == i);
        g.setColour(isAct ? juce::Colour(0xff38bdf8).withAlpha(0.18f) : theme_.bgElevated);
        g.fillRoundedRectangle(tr.toFloat(), 4.0f);
        g.setColour(isAct ? juce::Colour(0xff38bdf8) : theme_.borderSubtle);
        g.drawRoundedRectangle(tr.toFloat(), 4.0f, 1.0f);
        g.setFont(theme_.getSectionFont(12.5f));
        g.setColour(isAct ? theme_.textPrimary : theme_.textSecondary);
        g.drawText(kCenterTabs[static_cast<std::size_t>(i)], tr, juce::Justification::centred);
        tx += tw + 4;
    }

    // Top-right dynamic status label
    if (centerViewMode_ == 1) {
        int activeCount = 0;
        for (int i = 0; i < 8; ++i) {
            if (params.oscSlots[static_cast<std::size_t>(i)].enabled)
                activeCount++;
        }
        g.setFont(theme_.getSectionFont(13.0f));
        g.setColour(juce::Colour(0xff2dd4bf));
        g.drawText(juce::String(activeCount) + " ACTIVE SOURCES",
                   static_cast<int>(stageRect.getRight() - 145), ty, 135, 22,
                   juce::Justification::centredRight);
    } else if (centerViewMode_ == 2) {
        g.setFont(theme_.getSectionFont(13.0f));
        g.setColour(juce::Colour(0xff38bdf8));
        g.drawText(params.filterType == 0 ? "FLT 1: LADDER" : (params.filterType == 1 ? "FLT 1: BP" : "FLT 1: HP"),
                   static_cast<int>(stageRect.getRight() - 130), ty, 120, 22,
                   juce::Justification::centredRight);
    } else if (centerViewMode_ == 3) {
        g.setFont(theme_.getSectionFont(13.0f));
        g.setColour(juce::Colour(0xff2dd4bf));
        static constexpr std::array<const char*, 4> kFlt2Types = { "FLT 2: LADDER", "FLT 2: BP", "FLT 2: HP", "FLT 2: CARVE BELL" };
        g.drawText(kFlt2Types[static_cast<std::size_t>(std::clamp(params.filter2Type, 0, 3))],
                   static_cast<int>(stageRect.getRight() - 145), ty, 135, 22,
                   juce::Justification::centredRight);
    } else if (centerViewMode_ == 4) {
        g.setFont(theme_.getSectionFont(13.0f));
        g.setColour(juce::Colour(0xfff59e0b));
        g.drawText("MACRO 2D PAD",
                   static_cast<int>(stageRect.getRight() - 120), ty, 110, 22,
                   juce::Justification::centredRight);
    } else {
        const auto layerCol = getLayerColour(selectedOsc_);
        g.setFont(theme_.getSectionFont(14.0f));
        g.setColour(layerCol);
        g.drawText(getLayerShortName(params, selectedOsc_),
                   static_cast<int>(stageRect.getRight() - 65), ty, 55, 22,
                   juce::Justification::centredRight);
    }

    // Tall Canvas inside Center Stage
    canvasRect_ = stageRect.withTrimmedTop(36.0f).withTrimmedBottom(32.0f).reduced(10.0f, 2.0f);
    g.setColour(juce::Colour(0xff0b0d12));
    g.fillRoundedRectangle(canvasRect_, 6.0f);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(canvasRect_, 6.0f, 1.0f);

    // Green [HOLD] Drone Button in top-right of canvas
    holdBtnRect_ = juce::Rectangle<int>(
        static_cast<int>(canvasRect_.getRight() - 58), static_cast<int>(canvasRect_.getY() + 8), 48, 20);
    const bool isHold = params.droneEnabled;
    g.setColour(isHold ? juce::Colour(0xff22c55e).withAlpha(0.28f) : theme_.bgElevated);
    g.fillRoundedRectangle(holdBtnRect_.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff22c55e));
    g.drawRoundedRectangle(holdBtnRect_.toFloat(), 4.0f, 1.2f);
    g.setFont(theme_.getSectionFont(12.0f));
    g.setColour(isHold ? juce::Colours::white : juce::Colour(0xff22c55e));
    g.drawText("HOLD", holdBtnRect_, juce::Justification::centred);
}

void JornaCenterStageColumn::drawOscWaveStage(juce::Graphics& g, const nova::dsp::WorshipSynthEngine::Parameters& params) {
    const auto& slotP = params.oscSlots[static_cast<std::size_t>(selectedOsc_)];
    const float morph = slotP.shapeMorph;
    const float spray = slotP.detune;
    const float midY = canvasRect_.getCentreY() + 10.0f;
    const float maxAmp = canvasRect_.getHeight() * 0.40f;
    const float cursorX = canvasRect_.getX() + 24.0f + morph * (canvasRect_.getWidth() - 48.0f);
    const float cursorY = canvasRect_.getBottom() - 24.0f - spray * (canvasRect_.getHeight() - 56.0f);

    const bool isGranular = (slotP.engine == nova::dsp::MultiEngineOscillator::EngineType::OrganicTexture);
    if (isGranular) {
        const float grainW = 32.0f + morph * 96.0f;
        juce::Path grainEnv;
        grainEnv.startNewSubPath(cursorX - grainW * 0.5f, midY);
        for (int i = 0; i <= 32; ++i) {
            const float u = static_cast<float>(i) / 32.0f;
            const float gx = cursorX - grainW * 0.5f + u * grainW;
            const float hann = 0.5f * (1.0f - std::cos(u * 6.28318f));
            const float gy = midY - hann * maxAmp * 0.82f;
            grainEnv.lineTo(gx, gy);
        }
        grainEnv.lineTo(cursorX + grainW * 0.5f, midY);
        grainEnv.closeSubPath();
        g.setColour(juce::Colour(0xff84cc16).withAlpha(0.20f));
        g.fillPath(grainEnv);
        g.setColour(juce::Colour(0xff84cc16).withAlpha(0.85f));
        g.strokePath(grainEnv, juce::PathStrokeType(1.6f));

        const float scanDashes[2] = { 3.0f, 3.0f };
        for (int h = -1; h <= 1; ++h) {
            const float hx = cursorX + static_cast<float>(h) * (grainW * 0.28f);
            if (hx >= canvasRect_.getX() + 4.0f && hx <= canvasRect_.getRight() - 4.0f) {
                g.setColour(juce::Colour(0xff84cc16).withAlpha(h == 0 ? 0.75f : 0.35f));
                g.drawDashedLine(juce::Line<float>(hx, midY - maxAmp * 0.70f, hx, midY),
                                 scanDashes, 2, 1.0f);
            }
        }
    }

    // High-Resolution Bipolar Oscillating Waveform
    juce::Path wavePath;
    juce::Path waveFill;
    constexpr int kWavePts = 240;
    waveFill.startNewSubPath(canvasRect_.getX() + 6.0f, midY);

    for (int i = 0; i <= kWavePts; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(kWavePts);
        const float x = canvasRect_.getX() + 6.0f + u * (canvasRect_.getWidth() - 12.0f);
        const float waveShape = calculateOscSample(slotP, u * 3.0f);
        const float y = midY - waveShape * (maxAmp * 0.88f * std::clamp(slotP.level * 1.25f, 0.35f, 1.0f));
        if (i == 0) wavePath.startNewSubPath(x, y);
        else        wavePath.lineTo(x, y);
        waveFill.lineTo(x, y);
    }
    waveFill.lineTo(canvasRect_.getRight() - 6.0f, midY);
    waveFill.closeSubPath();

    // Zero line
    g.setColour(juce::Colour(0xff334155).withAlpha(0.35f));
    g.drawDashedLine(juce::Line<float>(canvasRect_.getX() + 6.0f, midY, canvasRect_.getRight() - 6.0f, midY), nullptr, 0, 1.0f);

    const auto layerCol = getLayerColour(selectedOsc_);
    juce::ColourGradient waveGrad(layerCol.withAlpha(0.32f), canvasRect_.getX(), midY - maxAmp,
                                  layerCol.withAlpha(0.04f), canvasRect_.getX(), midY + maxAmp, false);
    g.setGradientFill(waveGrad);
    g.fillPath(waveFill);

    g.setColour(layerCol.withAlpha(0.35f));
    g.strokePath(wavePath, juce::PathStrokeType(4.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(layerCol);
    g.strokePath(wavePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.strokePath(wavePath, juce::PathStrokeType(0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Playhead & Morph Node Handle
    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.drawVerticalLine(static_cast<int>(cursorX), canvasRect_.getY() + 4.0f, canvasRect_.getBottom() - 4.0f);

    g.setColour(layerCol);
    g.fillEllipse(cursorX - 5.0f, cursorY - 5.0f, 10.0f, 10.0f);
    g.setColour(juce::Colours::white);
    g.drawEllipse(cursorX - 5.0f, cursorY - 5.0f, 10.0f, 10.0f, 1.6f);
}

void JornaCenterStageColumn::drawStageFooter(juce::Graphics& g, const juce::Rectangle<float>& stageRect, const nova::dsp::WorshipSynthEngine::Parameters& params) {
    const auto& slotP = params.oscSlots[static_cast<std::size_t>(selectedOsc_)];
    const int pillY = static_cast<int>(stageRect.getBottom() - 26);
    const int pillX = static_cast<int>(stageRect.getRight() - 238);

    if (centerViewMode_ == 0) {
        octPillRect_ = juce::Rectangle<int>(pillX, pillY, 54, 20);
        semPillRect_ = juce::Rectangle<int>(pillX + 58, pillY, 54, 20);
        const juce::Rectangle<int> finPillRect(pillX + 116, pillY, 54, 20);
        const juce::Rectangle<int> crsPillRect(pillX + 174, pillY, 54, 20);

        const auto drawPill = [&](const juce::Rectangle<int>& pr, const juce::String& text, bool isInteractive) {
            g.setColour(theme_.bgControl);
            g.fillRoundedRectangle(pr.toFloat(), 4.0f);
            g.setColour(isInteractive ? juce::Colour(0xff38bdf8).withAlpha(0.7f) : theme_.borderSubtle);
            g.drawRoundedRectangle(pr.toFloat(), 4.0f, 1.0f);
            g.setFont(theme_.getValueFont(12.5f));
            g.setColour(juce::Colour(0xff38bdf8));
            g.drawText(text, pr, juce::Justification::centred);
        };

        drawPill(octPillRect_, juce::String("OCT ") + (slotP.octaveOffset >= 0 ? "+" : "") + juce::String(slotP.octaveOffset), true);
        drawPill(semPillRect_, juce::String("SEM ") + (slotP.semitoneOffset >= 0 ? "+" : "") + juce::String(slotP.semitoneOffset), true);
        drawPill(finPillRect, "FIN +0", false);
        drawPill(crsPillRect, "CRS 1.00", false);

        g.setFont(theme_.getLabelFont(11.0f));
        g.setColour(theme_.textMuted);
        g.drawText("DRAG: X = MORPH | Y = DETUNE", static_cast<int>(stageRect.getX() + 14), pillY, 240, 20, juce::Justification::centredLeft);
    } else if (centerViewMode_ == 1) {
        const auto tel = proc_.getSynthTelemetry();
        g.setFont(theme_.getLabelFont(11.0f));
        g.setColour(theme_.textMuted);
        g.drawText("CLICK LAYER BADGE TO EDIT  |  DRAG: WARMTH & CUTOFF", static_cast<int>(stageRect.getX() + 14), pillY, 340, 20, juce::Justification::centredLeft);

        g.setFont(theme_.getValueFont(12.0f));
        g.setColour(juce::Colour(0xff2dd4bf));
        g.drawText(juce::String(tel.activeVoiceCount) + " VOICES ACTIVE", pillX + 90, pillY, 140, 20, juce::Justification::centredRight);
    } else if (centerViewMode_ == 2 || centerViewMode_ == 3) {
        g.setFont(theme_.getLabelFont(11.0f));
        g.setColour(theme_.textMuted);
        g.drawText("DRAG: X = CUTOFF | Y = RESONANCE", static_cast<int>(stageRect.getX() + 14), pillY, 260, 20, juce::Justification::centredLeft);
    } else if (centerViewMode_ == 4) {
        g.setFont(theme_.getLabelFont(11.0f));
        g.setColour(theme_.textMuted);
        g.drawText("DRAG: X = WARMTH | Y = AIR", static_cast<int>(stageRect.getX() + 14), pillY, 260, 20, juce::Justification::centredLeft);
    }
}

} // namespace nova::synth
