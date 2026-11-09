#include "NovaSynthEditor.h"
#include "ui/SourceListManager.h"
#include "ui/DualVisualizerHub.h"
#include "ui/SourceDeck.h"
#include "ui/DualFilterDeck.h"
#include "ui/EnvelopeQuadDeck.h"
#include "ui/StandaloneKeyboardBar.h"
#include "ui/ModulationHubPanel.h"
#include "ui/DroneHubPanel.h"
#include "ui/ArpeggiatorHubPanel.h"
#include "ui/MixerHubPanel.h"
#include "ui/NovaSynthUiHelpers.h"
#include "ui/JornaSourcesColumn.h"
#include "ui/WorkbenchVisualizers.h"
#include "ui/SignalFlowPanel.h"
#include "ui/JornaLowerWorkbench.h"
#include "MacDockIcon.h"
#include "BinaryData.h"
#include <cmath>
#include <algorithm>
#include <array>
#include <numbers>

namespace nova::synth {

namespace {


static float calculateOscSample(const nova::dsp::MultiEngineOscillator::Parameters& slot, float phaseNorm) noexcept {
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

// ==============================================================================
// 2. CENTER COLUMN: JORNA Top "SPECTRUM OUTPUT" + Large "OSC WAVE / PARTICLE" Stage
// ==============================================================================
class JornaCenterStageColumn : public juce::Component {
public:
    std::function<void(int)> onSelectOsc;

    JornaCenterStageColumn(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc), theme_(theme) {
        historyWave_.fill(0.0f);
        spectrumBins_.fill(0.02f);
    }

    void setSelectedOscIndex(int idx) noexcept {
        selectedOsc_ = std::clamp(idx, 0, 7);
        repaint();
    }

    void pullAudioForVisualizers() {
        std::array<float, 256> incoming{};
        const auto popped = proc_.popVisualizerSamples(incoming.data(), incoming.size());
        if (popped > 0) {
            for (std::size_t i = 0; i < popped; ++i) {
                historyWave_[writeCursor_] = incoming[i];
                writeCursor_ = (writeCursor_ + 1) & (historyWave_.size() - 1);
            }
        }

        // Smoothly update 32-band pseudo-spectral analyzer from recent waveform energy & harmonic profile
        const auto params = proc_.getCurrentParameters();
        const auto tel = proc_.getSynthTelemetry();
        const float outEnergy = std::clamp((tel.outputPeakL + tel.outputPeakR) * 1.4f, 0.0f, 1.0f);
        const float cutoffNorm = std::clamp(std::log10(params.filterCutoffHz / 20.0f) / 3.0f, 0.1f, 1.0f);

        for (std::size_t b = 0; b < spectrumBins_.size(); ++b) {
            const float normFreq = static_cast<float>(b) / static_cast<float>(spectrumBins_.size() - 1);
            const float rolloff = (normFreq > cutoffNorm)
                ? std::exp(-6.0f * (normFreq - cutoffNorm))
                : 1.0f;
            const float ripple = 0.55f + 0.45f * std::sin(static_cast<float>(b) * 1.7f + animPhase_ * 2.2f);
            const float target = outEnergy * rolloff * ripple * (1.0f - 0.35f * normFreq);
            spectrumBins_[b] = spectrumBins_[b] * 0.78f + std::max(0.02f, target) * 0.22f;
        }

        animPhase_ += 0.09f;
    }

    void paint(juce::Graphics& g) override {
        pullAudioForVisualizers();

        const auto full = getLocalBounds();
        const auto specRect = full.withHeight(88).toFloat().reduced(1.0f);
        const auto stageRect = full.withTrimmedTop(96).toFloat().reduced(1.0f);
        const auto params = proc_.getCurrentParameters();
        const auto& slotP = params.oscSlots[static_cast<std::size_t>(selectedOsc_)];

        // ------------------------------------------------------------------
        // A. TOP PANEL: "SPECTRUM OUTPUT" (WAVE | SPEC)
        // ------------------------------------------------------------------
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

        // ------------------------------------------------------------------
        // B. MAIN CENTER STAGE: "OSC WAVE | MACROS | FILTER 1 | FILTER 2"
        // ------------------------------------------------------------------
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

        if (centerViewMode_ == 0) {
            // --- Single Selected OSC Waveform + Granular Grain Window ---
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

        } else if (centerViewMode_ == 1) {
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

        } else if (centerViewMode_ == 2 || centerViewMode_ == 3) {
            // FILTER 1 / FILTER 2 Frequency Response Curve
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

        } else if (centerViewMode_ == 4) {
            // MACRO 2D PAD (Warmth vs Air)
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

        // Bottom area inside stageRect
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

    void mouseDown(const juce::MouseEvent& event) override {
        const auto pos = event.getPosition();

        if (centerViewMode_ == 1) {
            for (const auto& b : allSourcesBadges_) {
                if (b.first.contains(pos)) {
                    selectedOsc_ = b.second;
                    centerViewMode_ = 0;
                    if (onSelectOsc) onSelectOsc(b.second);
                    repaint();
                    return;
                }
            }
        }

        if (centerViewMode_ == 0) {
            if (octPillRect_.contains(pos)) {
                const juce::String prefix = "osc" + juce::String(selectedOsc_ + 1) + "_";
                if (auto* p = proc_.getAPVTS().getParameter(prefix + "octave")) {
                    const int cur = static_cast<int>(std::round(p->convertFrom0to1(p->getValue())));
                    int next = event.mods.isPopupMenu() ? (cur - 1) : (cur + 1);
                    if (next > 2) next = -2;
                    if (next < -2) next = 2;
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(next)));
                }
                repaint();
                return;
            }

            if (semPillRect_.contains(pos)) {
                const juce::String prefix = "osc" + juce::String(selectedOsc_ + 1) + "_";
                if (auto* p = proc_.getAPVTS().getParameter(prefix + "semi")) {
                    const int cur = static_cast<int>(std::round(p->convertFrom0to1(p->getValue())));
                    int next = event.mods.isPopupMenu() ? (cur - 1) : (cur + 1);
                    if (next > 12) next = -12;
                    if (next < -12) next = 12;
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(next)));
                }
                repaint();
                return;
            }
        }

        if (specModeToggleBounds_.contains(pos)) {
            showSpectrumMode_ = !showSpectrumMode_;
            repaint();
            return;
        }

        for (int i = 0; i < static_cast<int>(centerTabRects_.size()); ++i) {
            if (centerTabRects_[static_cast<std::size_t>(i)].contains(pos)) {
                centerViewMode_ = i;
                repaint();
                return;
            }
        }

        if (holdBtnRect_.contains(pos)) {
            if (auto* p = proc_.getAPVTS().getParameter("droneEnabled")) {
                p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
            }
            repaint();
            return;
        }

        if (canvasRect_.contains(pos.toFloat())) {
            dragCanvasAt(pos.toFloat());
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override {
        if (canvasRect_.contains(event.getMouseDownPosition().toFloat())) {
            dragCanvasAt(event.position);
        }
    }

private:
    void dragCanvasAt(juce::Point<float> pt) {
        const float nx = std::clamp((pt.x - canvasRect_.getX()) / std::max(1.0f, canvasRect_.getWidth()), 0.0f, 1.0f);
        const float ny = std::clamp(1.0f - (pt.y - canvasRect_.getY()) / std::max(1.0f, canvasRect_.getHeight()), 0.0f, 1.0f);

        if (centerViewMode_ == 0) {
            const juce::String prefix = "osc" + juce::String(selectedOsc_ + 1) + "_";
            if (auto* s = proc_.getAPVTS().getParameter(prefix + "shape"))  s->setValueNotifyingHost(nx);
            if (auto* d = proc_.getAPVTS().getParameter(prefix + "detune")) d->setValueNotifyingHost(ny);
        } else if (centerViewMode_ == 1) {
            if (auto* w = proc_.getAPVTS().getParameter("macroWarmth"))   w->setValueNotifyingHost(nx);
            if (auto* c = proc_.getAPVTS().getParameter("filterCutoffHz")) c->setValueNotifyingHost(ny);
        } else if (centerViewMode_ == 2) {
            if (auto* c = proc_.getAPVTS().getParameter("filterCutoffHz")) c->setValueNotifyingHost(nx);
            if (auto* r = proc_.getAPVTS().getParameter("filterResonance")) r->setValueNotifyingHost(ny);
        } else if (centerViewMode_ == 3) {
            if (auto* c = proc_.getAPVTS().getParameter("filter2CutoffHz")) c->setValueNotifyingHost(nx);
            if (auto* r = proc_.getAPVTS().getParameter("filter2Resonance")) r->setValueNotifyingHost(ny);
        } else if (centerViewMode_ == 4) {
            if (auto* w = proc_.getAPVTS().getParameter("macroWarmth")) w->setValueNotifyingHost(nx);
            if (auto* a = proc_.getAPVTS().getParameter("macroAir"))    a->setValueNotifyingHost(ny);
        }
        repaint();
    }

    [[maybe_unused]] NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int selectedOsc_ = 0;
    bool showSpectrumMode_ = true;
    int centerViewMode_ = 0;
    float animPhase_ = 0.0f;

    std::array<float, 512> historyWave_{};
    std::size_t writeCursor_ = 0;
    std::array<float, 32> spectrumBins_{};

    juce::Rectangle<int> specModeToggleBounds_;
    std::vector<juce::Rectangle<int>> centerTabRects_;
    std::vector<std::pair<juce::Rectangle<int>, int>> allSourcesBadges_;
    juce::Rectangle<float> canvasRect_;
    juce::Rectangle<int> holdBtnRect_;
    juce::Rectangle<int> octPillRect_;
    juce::Rectangle<int> semPillRect_;
};

// ==============================================================================
// 3. RIGHT COLUMN: JORNA Dual Filter + Interactive Tall Envelope Editor
// ==============================================================================
class JornaRightColumn : public juce::Component {
public:
    JornaRightColumn(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc),
          theme_(theme),
          cutoffKnob_("CUTOFF", "Hz", theme, false),
          resoKnob_("RESO", "", theme, false),
          carveKnob_("CARVE", "", theme, true),
          driveKnob_("DRIVE", "", theme, false),
          envAmtKnob_("KEYTRK", "", theme, false),
          atkKnob_("ATK", "s", theme, false),
          decKnob_("DEC", "s", theme, false),
          susKnob_("SUS", "", theme, false),
          relKnob_("REL", "s", theme, false) {
        auto& apvts = proc_.getAPVTS();
        cutoffKnob_.attachToParameter(apvts, "filterCutoffHz");
        resoKnob_.attachToParameter(apvts,   "filterResonance");
        carveKnob_.attachToParameter(apvts,  "vocalCarve");
        driveKnob_.attachToParameter(apvts,  "filterDrive");
        envAmtKnob_.attachToParameter(apvts, "filterEnvAmount");

        addAndMakeVisible(cutoffKnob_);
        addAndMakeVisible(resoKnob_);
        addAndMakeVisible(carveKnob_);
        addAndMakeVisible(driveKnob_);
        addAndMakeVisible(envAmtKnob_);

        addAndMakeVisible(atkKnob_);
        addAndMakeVisible(decKnob_);
        addAndMakeVisible(susKnob_);
        addAndMakeVisible(relKnob_);

        selectFilterTab(0);
        selectEnvelopeTab(0);
    }

    void selectFilterTab(int fltIdx) {
        activeFilterTab_ = std::clamp(fltIdx, 0, 1);
        auto& apvts = proc_.getAPVTS();
        if (activeFilterTab_ == 0) {
            cutoffKnob_.setLabel("CUTOFF");
            cutoffKnob_.setSuffix("Hz");
            cutoffKnob_.attachToParameter(apvts, "filterCutoffHz");

            resoKnob_.setLabel("RESO");
            resoKnob_.setSuffix("");
            resoKnob_.attachToParameter(apvts, "filterResonance");

            carveKnob_.setLabel("DRIVE");
            carveKnob_.setSuffix("");
            carveKnob_.attachToParameter(apvts, "filterDrive");

            driveKnob_.setLabel("ENV AMT");
            driveKnob_.setSuffix("");
            driveKnob_.attachToParameter(apvts, "filterEnvAmount");

            envAmtKnob_.setLabel("WARMTH");
            envAmtKnob_.setSuffix("");
            envAmtKnob_.attachToParameter(apvts, "macroWarmth");
        } else {
            cutoffKnob_.setLabel("CUTOFF");
            cutoffKnob_.setSuffix("Hz");
            cutoffKnob_.attachToParameter(apvts, "filter2CutoffHz");

            resoKnob_.setLabel("RESO");
            resoKnob_.setSuffix("");
            resoKnob_.attachToParameter(apvts, "filter2Resonance");

            carveKnob_.setLabel("DRIVE");
            carveKnob_.setSuffix("");
            carveKnob_.attachToParameter(apvts, "filter2Drive");

            driveKnob_.setLabel("ENV AMT");
            driveKnob_.setSuffix("");
            driveKnob_.attachToParameter(apvts, "filter2EnvAmount");

            envAmtKnob_.setLabel("CARVE");
            envAmtKnob_.setSuffix("");
            envAmtKnob_.attachToParameter(apvts, "vocalCarve");
        }
        repaint();
    }

    struct EnvParamIds {
        juce::String attack;
        juce::String decay;
        juce::String sustain;
        juce::String release;
    };

    EnvParamIds getActiveEnvParamIds() const {
        switch (activeEnvTab_) {
            case 1:  return { "fltAttack", "fltDecay", "fltSustain", "fltRelease" };
            case 2:  return { "modAttack", "modDecay", "modSustain", "modRelease" };
            case 3:  return { "auxAttack", "auxDecay", "auxSustain", "auxRelease" };
            case 0:
            default: return { "ampAttack", "ampDecay", "ampSustain", "ampRelease" };
        }
    }

    void selectEnvelopeTab(int envIdx) {
        activeEnvTab_ = std::clamp(envIdx, 0, 3);
        auto& apvts = proc_.getAPVTS();
        const auto ids = getActiveEnvParamIds();
        atkKnob_.attachToParameter(apvts, ids.attack);
        decKnob_.attachToParameter(apvts, ids.decay);
        susKnob_.attachToParameter(apvts, ids.sustain);
        relKnob_.attachToParameter(apvts, ids.release);
        repaint();
    }

    void paint(juce::Graphics& g) override {
        const auto full = getLocalBounds();
        const auto filterCard = full.withHeight(204).toFloat().reduced(1.0f);
        const auto envCard = full.withTrimmedTop(212).toFloat().reduced(1.0f);

        const auto params = proc_.getCurrentParameters();

        // ------------------------------------------------------------------
        // A. TOP RIGHT: [O FILTER 1]  [O FILTER 2]
        // ------------------------------------------------------------------
        g.setColour(theme_.bgSurface);
        g.fillRoundedRectangle(filterCard, theme_.cornerRadius);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(filterCard, theme_.cornerRadius, 1.0f);

        flt1TabRect_ = juce::Rectangle<int>(10, 8, ( getWidth() - 26 ) / 2, 22);
        flt2TabRect_ = juce::Rectangle<int>(flt1TabRect_.getRight() + 6, 8, flt1TabRect_.getWidth(), 22);

        const auto drawFilterTab = [&](juce::Rectangle<int> r, const juce::String& name, bool isSel, bool isEnabled) {
            g.setColour(isSel ? juce::Colour(0xff38bdf8).withAlpha(0.16f) : theme_.bgElevated);
            g.fillRoundedRectangle(r.toFloat(), 4.0f);
            g.setColour(isSel ? juce::Colour(0xff38bdf8) : theme_.borderSubtle);
            g.drawRoundedRectangle(r.toFloat(), 4.0f, 1.2f);

            const juce::Rectangle<float> ring(static_cast<float>(r.getX() + 7), static_cast<float>(r.getCentreY() - 4), 8.0f, 8.0f);
            g.setColour(isEnabled ? juce::Colour(0xff38bdf8) : theme_.textMuted);
            g.drawEllipse(ring, 1.6f);
            if (isEnabled) g.fillEllipse(ring.reduced(2.0f));

            g.setFont(theme_.getSectionFont(13.0f));
            g.setColour(isSel ? theme_.textPrimary : theme_.textSecondary);
            g.drawText(name, r.withTrimmedLeft(18), juce::Justification::centredLeft);
        };

        drawFilterTab(flt1TabRect_, "FILTER 1", activeFilterTab_ == 0, params.filter1Enabled);
        drawFilterTab(flt2TabRect_, "FILTER 2", activeFilterTab_ == 1, params.filter2Enabled);

        // Filter Type Selector Bar (< LOW 24 > or < VOCAL CARVE / FORMANT >)
        fltTypeRect_ = juce::Rectangle<int>(10, 34, getWidth() - 20, 20);
        g.setColour(theme_.bgControl);
        g.fillRoundedRectangle(fltTypeRect_.toFloat(), 4.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(fltTypeRect_.toFloat(), 4.0f, 1.0f);

        if (activeFilterTab_ == 0) {
            static constexpr std::array<const char*, 3> kFltNames = { "LOW 24", "BAND 12", "HIGH 24" };
            g.setFont(theme_.getSectionFont(13.0f));
            g.setColour(theme_.textPrimary);
            g.drawText("<  " + juce::String(kFltNames[static_cast<std::size_t>(std::clamp(params.filterType, 0, 2))]) + "  >",
                       fltTypeRect_, juce::Justification::centred);
        } else {
            static constexpr std::array<const char*, 4> kFlt2Names = { "LOW 24", "BAND 12", "HIGH 24", "CARVE BELL" };
            g.setFont(theme_.getSectionFont(13.0f));
            g.setColour(juce::Colour(0xff2dd4bf));
            g.drawText("<  " + juce::String(kFlt2Names[static_cast<std::size_t>(std::clamp(params.filter2Type, 0, 3))]) + "  >",
                       fltTypeRect_, juce::Justification::centred);
        }

        // ------------------------------------------------------------------
        // B. BOTTOM RIGHT: [ENV 1] [ENV 2] [ENV 3] [ENV 4] + Tall Curve Editor
        // ------------------------------------------------------------------
        g.setColour(theme_.bgSurface);
        g.fillRoundedRectangle(envCard, theme_.cornerRadius);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(envCard, theme_.cornerRadius, 1.0f);

        envTabRects_.clear();
        const int envTop = static_cast<int>(envCard.getY() + 8);
        const int envTabW = (getWidth() - 32) / 4;
        static constexpr std::array<const char*, 4> kEnvNames = {
            "ENV 1: AMP", "ENV 2: FILTER", "ENV 3: MOD", "ENV 4: AUX"
        };
        for (int i = 0; i < 4; ++i) {
            const juce::Rectangle<int> er(10 + i * (envTabW + 4), envTop, envTabW, 20);
            envTabRects_.push_back(er);
            const bool isAct = (activeEnvTab_ == i);
            g.setColour(isAct ? juce::Colour(0xff38bdf8).withAlpha(0.18f) : theme_.bgElevated);
            g.fillRoundedRectangle(er.toFloat(), 4.0f);
            g.setColour(isAct ? juce::Colour(0xff38bdf8) : theme_.borderSubtle);
            g.drawRoundedRectangle(er.toFloat(), 4.0f, 1.0f);
            g.setFont(theme_.getSectionFont(11.0f));
            g.setColour(isAct ? theme_.textPrimary : theme_.textSecondary);
            g.drawText(kEnvNames[static_cast<std::size_t>(i)], er, juce::Justification::centred);
        }

        // Tall Interactive Envelope Curve Graph (like JORNA's bottom-right cyan/orange curve!)
        envGraphRect_ = envCard.withTrimmedTop(106.0f).reduced(10.0f, 6.0f);
        g.setColour(juce::Colour(0xff0b0d12));
        g.fillRoundedRectangle(envGraphRect_, 5.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(envGraphRect_, 5.0f, 1.0f);

        auto& apvts = proc_.getAPVTS();
        const auto envIds = getActiveEnvParamIds();
        auto* pAtk = apvts.getParameter(envIds.attack);
        auto* pDec = apvts.getParameter(envIds.decay);
        auto* pSus = apvts.getParameter(envIds.sustain);
        auto* pRel = apvts.getParameter(envIds.release);

        const float uA = pAtk ? pAtk->getValue() : 0.2f;
        const float uD = pDec ? pDec->getValue() : 0.35f;
        const float uS = pSus ? pSus->getValue() : 0.8f;
        const float uR = pRel ? pRel->getValue() : 0.35f;

        const float gx0 = envGraphRect_.getX() + 10.0f;
        const float gw  = envGraphRect_.getWidth() - 20.0f;
        const float gyB = envGraphRect_.getBottom() - 20.0f;
        const float gh  = envGraphRect_.getHeight() - 32.0f;

        graphX0_ = gx0;
        graphW_  = gw;
        graphYB_ = gyB;
        graphH_  = gh;

        // Partition visual width into 4 distinct phases totaling exactly 1.0 (0.28 + 0.28 + 0.16 + 0.28 = 1.0)
        const float wA = 0.28f * gw;
        const float wD = 0.28f * gw;
        const float wS = 0.16f * gw;
        const float wR = 0.28f * gw;

        const juce::Point<float> pStart(gx0, gyB);
        const juce::Point<float> pPeak(gx0 + uA * wA, gyB - gh);
        const juce::Point<float> pSusStart(pPeak.x + uD * wD, gyB - uS * gh);
        const juce::Point<float> pSusEnd(pSusStart.x + wS, gyB - uS * gh);
        const juce::Point<float> pEnd(pSusEnd.x + uR * wR, gyB);

        pPeak_     = pPeak;
        pSusStart_ = pSusStart;
        pSusEnd_   = pSusEnd;
        pEnd_      = pEnd;

        // Subtle area fill underneath the curve
        juce::Path envFill;
        envFill.startNewSubPath(pStart);
        if (uA <= 0.005f) {
            envFill.lineTo(pPeak.x, pPeak.y);
        } else {
            envFill.quadraticTo(gx0 + (pPeak.x - gx0) * 0.25f, gyB - gh * 0.85f, pPeak.x, pPeak.y);
        }
        envFill.quadraticTo(pPeak.x + (pSusStart.x - pPeak.x) * 0.35f, pSusStart.y + (pPeak.y - pSusStart.y) * 0.30f, pSusStart.x, pSusStart.y);
        envFill.lineTo(pSusEnd.x, pSusEnd.y);
        if (uR <= 0.005f) {
            envFill.lineTo(pEnd.x, pEnd.y);
        } else {
            envFill.quadraticTo(pSusEnd.x + (pEnd.x - pSusEnd.x) * 0.35f, gyB - uS * gh * 0.25f, pEnd.x, pEnd.y);
        }
        envFill.closeSubPath();

        juce::ColourGradient envGrad(juce::Colour(0xff38bdf8).withAlpha(0.30f), gx0, gyB - gh,
                                     juce::Colour(0xff38bdf8).withAlpha(0.02f), gx0, gyB, false);
        g.setGradientFill(envGrad);
        g.fillPath(envFill);

        // Dashed baseline and vertical key-release boundary
        g.setColour(juce::Colour(0xff334155).withAlpha(0.35f));
        g.drawDashedLine(juce::Line<float>(gx0, gyB, gx0 + gw, gyB), nullptr, 0, 1.0f);
        const float dashes[2] = { 2.0f, 2.0f };
        g.drawDashedLine(juce::Line<float>(pSusEnd.x, gyB - gh, pSusEnd.x, gyB), dashes, 2, 0.8f);

        // Draw Cyan Attack/Decay/Sustain stroke
        juce::Path atkDecPath;
        atkDecPath.startNewSubPath(pStart);
        if (uA <= 0.005f) {
            atkDecPath.lineTo(pPeak.x, pPeak.y);
        } else {
            atkDecPath.quadraticTo(gx0 + (pPeak.x - gx0) * 0.25f, gyB - gh * 0.85f, pPeak.x, pPeak.y);
        }
        atkDecPath.quadraticTo(pPeak.x + (pSusStart.x - pPeak.x) * 0.35f, pSusStart.y + (pPeak.y - pSusStart.y) * 0.30f, pSusStart.x, pSusStart.y);
        atkDecPath.lineTo(pSusEnd.x, pSusEnd.y);
        g.setColour(juce::Colour(0xff38bdf8));
        g.strokePath(atkDecPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Draw Orange Release stroke
        juce::Path relPath;
        relPath.startNewSubPath(pSusEnd.x, pSusEnd.y);
        if (uR <= 0.005f) {
            relPath.lineTo(pEnd.x, pEnd.y);
        } else {
            relPath.quadraticTo(pSusEnd.x + (pEnd.x - pSusEnd.x) * 0.35f, gyB - uS * gh * 0.25f, pEnd.x, pEnd.y);
        }
        g.setColour(juce::Colour(0xfff59e0b));
        g.strokePath(relPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Interactive node handles with amber glow & cyan active highlight
        const auto drawHandle = [&](const juce::Point<float>& pt, DragNode nodeType) {
            const bool isHovered = (hoveredNode_ == nodeType);
            const bool isDragging = (activeDragNode_ == nodeType);

            if (isDragging || isHovered) {
                g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.35f));
                g.fillEllipse(pt.x - 9.0f, pt.y - 9.0f, 18.0f, 18.0f);
            }

            g.setColour(isDragging ? juce::Colours::white : juce::Colour(0xfff59e0b));
            g.fillEllipse(pt.x - 4.5f, pt.y - 4.5f, 9.0f, 9.0f);
            g.setColour(juce::Colour(0xff38bdf8));
            g.drawEllipse(pt.x - 4.5f, pt.y - 4.5f, 9.0f, 9.0f, 1.4f);
        };

        drawHandle(pPeak_, DragNode::Peak);
        drawHandle(pSusStart_, DragNode::SusStart);
        drawHandle(pSusEnd_, DragNode::SusEnd);
        drawHandle(pEnd_, DragNode::End);

        // Real-time floating value badge while dragging
        if (activeDragNode_ != DragNode::None) {
            juce::String tipText;
            juce::Point<float> tipPt;

            if (activeDragNode_ == DragNode::Peak && pAtk) {
                const float sec = pAtk->convertFrom0to1(uA);
                tipText = "ATK: " + ((sec < 1.0f) ? (juce::String(static_cast<int>(std::round(sec * 1000.0f))) + " ms")
                                                   : (juce::String(sec, 2) + " s"));
                tipPt = pPeak_;
            } else if (activeDragNode_ == DragNode::SusStart && pDec && pSus) {
                const float dSec = pDec->convertFrom0to1(uD);
                const int sPct = static_cast<int>(std::round(uS * 100.0f));
                const juce::String dStr = (dSec < 1.0f) ? (juce::String(static_cast<int>(std::round(dSec * 1000.0f))) + " ms")
                                                        : (juce::String(dSec, 2) + " s");
                tipText = "DEC: " + dStr + "  SUS: " + juce::String(sPct) + "%";
                tipPt = pSusStart_;
            } else if (activeDragNode_ == DragNode::SusEnd && pSus) {
                const int sPct = static_cast<int>(std::round(uS * 100.0f));
                tipText = "SUS: " + juce::String(sPct) + "%";
                tipPt = pSusEnd_;
            } else if (activeDragNode_ == DragNode::End && pRel) {
                const float rSec = pRel->convertFrom0to1(uR);
                tipText = "REL: " + ((rSec < 1.0f) ? (juce::String(static_cast<int>(std::round(rSec * 1000.0f))) + " ms")
                                                   : (juce::String(rSec, 2) + " s"));
                tipPt = pEnd_;
            }

            if (tipText.isNotEmpty()) {
                g.setFont(theme_.getValueFont(11.0f));
                juce::GlyphArrangement ga;
                ga.addLineOfText(g.getCurrentFont(), tipText, 0.0f, 0.0f);
                const float tipW = ga.getBoundingBox(0, -1, true).getWidth() + 14.0f;
                const float tipH = 18.0f;
                const float tipX = std::clamp(tipPt.x - tipW * 0.5f,
                                            envGraphRect_.getX() + 4.0f,
                                            envGraphRect_.getRight() - tipW - 4.0f);
                const float tipY = std::max(envGraphRect_.getY() + 4.0f,
                                          tipPt.y - tipH - 8.0f);

                const juce::Rectangle<float> tipR(tipX, tipY, tipW, tipH);
                g.setColour(juce::Colour(0xff0b0d12).withAlpha(0.92f));
                g.fillRoundedRectangle(tipR, 4.0f);
                g.setColour(juce::Colour(0xff38bdf8));
                g.drawRoundedRectangle(tipR, 4.0f, 1.0f);
                g.setColour(juce::Colours::white);
                g.drawText(tipText, tipR.toNearestInt(), juce::Justification::centred);
            }
        }

        // Stage labels along bottom of envelope graph
        g.setFont(theme_.getLabelFont(10.5f));
        g.setColour(theme_.textMuted);
        g.drawText("ATTACK", static_cast<int>(gx0), static_cast<int>(gyB + 4.0f), static_cast<int>(wA), 14, juce::Justification::centred);
        g.drawText("DECAY", static_cast<int>(gx0 + wA), static_cast<int>(gyB + 4.0f), static_cast<int>(wD), 14, juce::Justification::centred);
        g.drawText("SUSTAIN", static_cast<int>(gx0 + wA + wD), static_cast<int>(gyB + 4.0f), static_cast<int>(wS), 14, juce::Justification::centred);
        g.drawText("RELEASE", static_cast<int>(gx0 + wA + wD + wS), static_cast<int>(gyB + 4.0f), static_cast<int>(wR), 14, juce::Justification::centred);
    }

    void resized() override {
        // Filter knobs: 3 on top row, 2 on second row (generous large bounds matching OSC section)
        const int w3 = (getWidth() - 16) / 3;
        cutoffKnob_.setBounds(8,          56, w3, 72);
        resoKnob_.setBounds(8 + w3,       56, w3, 72);
        carveKnob_.setBounds(8 + w3 * 2,  56, w3, 72);

        const int w2 = (getWidth() - 32) / 2;
        driveKnob_.setBounds(16,          128, w2, 72);
        envAmtKnob_.setBounds(16 + w2,    128, w2, 72);

        // Envelope knobs row (ATK, DEC, SUS, REL): full 72px height matching OSC knobs!
        const int envKnobTop = 242;
        const int ew = (getWidth() - 16) / 4;
        atkKnob_.setBounds(8,          envKnobTop, ew, 72);
        decKnob_.setBounds(8 + ew,     envKnobTop, ew, 72);
        susKnob_.setBounds(8 + ew * 2, envKnobTop, ew, 72);
        relKnob_.setBounds(8 + ew * 3, envKnobTop, ew, 72);
    }

    void mouseMove(const juce::MouseEvent& event) override {
        const auto posF = event.position;
        DragNode prevHovered = hoveredNode_;
        hoveredNode_ = DragNode::None;

        float bestDist = 18.0f;
        if (posF.getDistanceFrom(pPeak_) <= bestDist) {
            bestDist = posF.getDistanceFrom(pPeak_);
            hoveredNode_ = DragNode::Peak;
        }
        if (posF.getDistanceFrom(pSusStart_) <= bestDist) {
            bestDist = posF.getDistanceFrom(pSusStart_);
            hoveredNode_ = DragNode::SusStart;
        }
        if (posF.getDistanceFrom(pSusEnd_) <= bestDist) {
            bestDist = posF.getDistanceFrom(pSusEnd_);
            hoveredNode_ = DragNode::SusEnd;
        }
        if (posF.getDistanceFrom(pEnd_) <= bestDist) {
            bestDist = posF.getDistanceFrom(pEnd_);
            hoveredNode_ = DragNode::End;
        }

        if (hoveredNode_ != DragNode::None) {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        } else {
            setMouseCursor(juce::MouseCursor::NormalCursor);
        }

        if (hoveredNode_ != prevHovered) {
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& event) override {
        const auto pos = event.getPosition();
        const auto posF = event.position;

        // 1. Check closest node handle within 18px grab radius
        float bestDist = 18.0f;
        DragNode bestNode = DragNode::None;

        if (posF.getDistanceFrom(pPeak_) <= bestDist) {
            bestDist = posF.getDistanceFrom(pPeak_);
            bestNode = DragNode::Peak;
        }
        if (posF.getDistanceFrom(pSusStart_) <= bestDist) {
            bestDist = posF.getDistanceFrom(pSusStart_);
            bestNode = DragNode::SusStart;
        }
        if (posF.getDistanceFrom(pSusEnd_) <= bestDist) {
            bestDist = posF.getDistanceFrom(pSusEnd_);
            bestNode = DragNode::SusEnd;
        }
        if (posF.getDistanceFrom(pEnd_) <= bestDist) {
            bestDist = posF.getDistanceFrom(pEnd_);
            bestNode = DragNode::End;
        }

        if (bestNode != DragNode::None) {
            activeDragNode_ = bestNode;
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            repaint();
            return;
        }

        // 2. Click anywhere inside envelope graph area grabs the corresponding phase
        if (envGraphRect_.contains(posF)) {
            if (posF.x <= (pPeak_.x + pSusStart_.x) * 0.5f) {
                activeDragNode_ = DragNode::Peak;
            } else if (posF.x <= (pSusStart_.x + pSusEnd_.x) * 0.5f) {
                activeDragNode_ = DragNode::SusStart;
            } else if (posF.x <= (pSusEnd_.x + pEnd_.x) * 0.5f) {
                activeDragNode_ = DragNode::SusEnd;
            } else {
                activeDragNode_ = DragNode::End;
            }
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            mouseDrag(event);
            return;
        }

        if (flt1TabRect_.contains(pos)) {
            if (pos.x < flt1TabRect_.getX() + 22) {
                if (auto* p = proc_.getAPVTS().getParameter("filter1Enabled")) {
                    p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
                }
            }
            selectFilterTab(0);
            return;
        }
        if (flt2TabRect_.contains(pos)) {
            if (pos.x < flt2TabRect_.getX() + 22) {
                if (auto* p = proc_.getAPVTS().getParameter("filter2Enabled")) {
                    p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
                }
            }
            selectFilterTab(1);
            return;
        }
        if (fltTypeRect_.contains(pos)) {
            // Left arrow click: Cycle previous
            if (pos.x < fltTypeRect_.getX() + 24) {
                if (activeFilterTab_ == 0) {
                    const int cur = proc_.getCurrentParameters().filterType;
                    const int prev = (cur - 1 + 3) % 3;
                    if (auto* p = proc_.getAPVTS().getParameter("filterType")) {
                        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(prev)));
                    }
                } else {
                    const int cur = proc_.getCurrentParameters().filter2Type;
                    const int prev = (cur - 1 + 4) % 4;
                    if (auto* p = proc_.getAPVTS().getParameter("filter2Type")) {
                        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(prev)));
                    }
                }
                repaint();
                return;
            }

            // Right arrow click: Cycle next
            if (pos.x > fltTypeRect_.getRight() - 24) {
                if (activeFilterTab_ == 0) {
                    const int cur = proc_.getCurrentParameters().filterType;
                    if (auto* p = proc_.getAPVTS().getParameter("filterType")) {
                        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>((cur + 1) % 3)));
                    }
                } else {
                    const int cur = proc_.getCurrentParameters().filter2Type;
                    if (auto* p = proc_.getAPVTS().getParameter("filter2Type")) {
                        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>((cur + 1) % 4)));
                    }
                }
                repaint();
                return;
            }

            // Center click: Open rich Dropdown Popup Menu
            juce::PopupMenu menu;
            if (activeFilterTab_ == 0) {
                static constexpr std::array<const char*, 3> kFltNames = {
                    "LOW 24 (Warm Analog Ladder / SVF)",
                    "BAND 12 (Vocal Formant Bandpass)",
                    "HIGH 24 (Air Highpass)"
                };
                const int cur = proc_.getCurrentParameters().filterType;
                for (int i = 0; i < 3; ++i) {
                    menu.addItem(i + 1, kFltNames[static_cast<std::size_t>(i)], true, i == cur);
                }
                menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(fltTypeRect_)).withStandardItemHeight(28),
                    [this](int res) {
                        if (res >= 1 && res <= 3) {
                            if (auto* p = proc_.getAPVTS().getParameter("filterType")) {
                                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(res - 1)));
                            }
                            repaint();
                        }
                    });
            } else {
                static constexpr std::array<const char*, 4> kFlt2Names = {
                    "LOW 24 (Secondary Warm Ladder)",
                    "BAND 12 (Vocal Formant Bandpass)",
                    "HIGH 24 (Air Highpass)",
                    "CARVE BELL (Notch / Vocal Pocket)"
                };
                const int cur = proc_.getCurrentParameters().filter2Type;
                for (int i = 0; i < 4; ++i) {
                    menu.addItem(i + 1, kFlt2Names[static_cast<std::size_t>(i)], true, i == cur);
                }
                menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(fltTypeRect_)).withStandardItemHeight(28),
                    [this](int res) {
                        if (res >= 1 && res <= 4) {
                            if (auto* p = proc_.getAPVTS().getParameter("filter2Type")) {
                                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(res - 1)));
                            }
                            repaint();
                        }
                    });
            }
            return;
        }

        for (int i = 0; i < static_cast<int>(envTabRects_.size()); ++i) {
            if (envTabRects_[static_cast<std::size_t>(i)].contains(pos)) {
                selectEnvelopeTab(i);
                return;
            }
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override {
        if (activeDragNode_ == DragNode::None || graphW_ <= 10.0f || graphH_ <= 10.0f) return;

        const auto pos = event.position;
        auto& apvts = proc_.getAPVTS();
        const auto envIds = getActiveEnvParamIds();
        const juce::String atkId = envIds.attack;
        const juce::String decId = envIds.decay;
        const juce::String susId = envIds.sustain;
        const juce::String relId = envIds.release;

        const float wA = 0.28f * graphW_;
        const float wD = 0.28f * graphW_;
        const float wS = 0.16f * graphW_;
        const float wR = 0.28f * graphW_;

        if (activeDragNode_ == DragNode::Peak) {
            const float new_uA = std::clamp((pos.x - graphX0_) / wA, 0.0f, 1.0f);
            if (auto* p = apvts.getParameter(atkId)) {
                p->setValueNotifyingHost(new_uA);
            }
        } else if (activeDragNode_ == DragNode::SusStart) {
            const float new_uD = std::clamp((pos.x - pPeak_.x) / wD, 0.0f, 1.0f);
            if (auto* p = apvts.getParameter(decId)) {
                p->setValueNotifyingHost(new_uD);
            }
            const float new_uS = std::clamp((graphYB_ - pos.y) / graphH_, 0.0f, 1.0f);
            if (auto* p = apvts.getParameter(susId)) {
                p->setValueNotifyingHost(new_uS);
            }
        } else if (activeDragNode_ == DragNode::SusEnd) {
            const float new_uD = std::clamp((pos.x - wS - pPeak_.x) / wD, 0.0f, 1.0f);
            if (auto* p = apvts.getParameter(decId)) {
                p->setValueNotifyingHost(new_uD);
            }
            const float new_uS = std::clamp((graphYB_ - pos.y) / graphH_, 0.0f, 1.0f);
            if (auto* p = apvts.getParameter(susId)) {
                p->setValueNotifyingHost(new_uS);
            }
        } else if (activeDragNode_ == DragNode::End) {
            const float new_uR = std::clamp((pos.x - pSusEnd_.x) / wR, 0.0f, 1.0f);
            if (auto* p = apvts.getParameter(relId)) {
                p->setValueNotifyingHost(new_uR);
            }
        }
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override {
        activeDragNode_ = DragNode::None;
        setMouseCursor(hoveredNode_ != DragNode::None ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseExit(const juce::MouseEvent&) override {
        if (activeDragNode_ == DragNode::None) {
            hoveredNode_ = DragNode::None;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

private:
    [[maybe_unused]] NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int activeFilterTab_ = 0;
    int activeEnvTab_ = 0;

    juce::Rectangle<int> flt1TabRect_;
    juce::Rectangle<int> flt2TabRect_;
    juce::Rectangle<int> fltTypeRect_;
    std::vector<juce::Rectangle<int>> envTabRects_;
    juce::Rectangle<float> envGraphRect_;

    enum class DragNode { None, Peak, SusStart, SusEnd, End };
    DragNode activeDragNode_ = DragNode::None;
    DragNode hoveredNode_ = DragNode::None;

    juce::Point<float> pPeak_{};
    juce::Point<float> pSusStart_{};
    juce::Point<float> pSusEnd_{};
    juce::Point<float> pEnd_{};
    float graphX0_ = 0.0f;
    float graphW_ = 0.0f;
    float graphYB_ = 0.0f;
    float graphH_ = 0.0f;

    nova::ui::ArcKnob cutoffKnob_;
    nova::ui::ArcKnob resoKnob_;
    nova::ui::ArcKnob carveKnob_;
    nova::ui::ArcKnob driveKnob_;
    nova::ui::ArcKnob envAmtKnob_;

    nova::ui::ArcKnob atkKnob_;
    nova::ui::ArcKnob decKnob_;
    nova::ui::ArcKnob susKnob_;
    nova::ui::ArcKnob relKnob_;
};


// ==============================================================================
// 5. BOTTOM FOOTER BAR: JORNA Mod Pills + Compact Live Chord Audition Pills
// ==============================================================================
class JornaBottomFooterBar : public juce::Component {
public:
    struct AuditionDef {
        const char* name;
        int count;
        std::array<int, 4> notes;
    };

    static constexpr std::array<AuditionDef, 7> kAuditionPills = {{
        { "C3",     1, { 60, 0,  0,  0  } },
        { "C Maj9", 4, { 48, 60, 64, 74 } },
        { "F Maj7", 4, { 41, 57, 60, 64 } },
        { "Am7",    4, { 45, 57, 60, 67 } },
        { "G6 Sus", 4, { 43, 55, 62, 64 } },
        { "Dm9",    4, { 50, 60, 65, 76 } },
        { "Em7",    4, { 40, 55, 59, 62 } }
    }};

    JornaBottomFooterBar(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc), theme_(theme) {
        for (std::size_t i = 0; i < kAuditionPills.size(); ++i) {
            chordBtns_[i].setButtonText(kAuditionPills[i].name);
            if (i == 0) {
                // Highlight single note audition button with cyan text accent
                chordBtns_[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xff38bdf8));
                chordBtns_[i].setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            }
            chordBtns_[i].onStateChange = [this, i]() {
                const bool isDown = chordBtns_[i].isDown();
                if (isDown && !chordHeld_[i]) {
                    chordHeld_[i] = true;
                    for (int n = 0; n < kAuditionPills[i].count; ++n) {
                        proc_.triggerAuditionNoteOn(kAuditionPills[i].notes[static_cast<std::size_t>(n)], 0.82f);
                    }
                } else if (!isDown && chordHeld_[i]) {
                    chordHeld_[i] = false;
                    for (int n = 0; n < kAuditionPills[i].count; ++n) {
                        proc_.triggerAuditionNoteOff(kAuditionPills[i].notes[static_cast<std::size_t>(n)]);
                    }
                }
            };
            addAndMakeVisible(chordBtns_[i]);
        }
    }

    void paint(juce::Graphics& g) override {
        g.setFont(theme_.getSectionFont(10.5f));

        int x = 6;
        const auto drawBadgeGroup = [&](const juce::String& groupTitle,
                                        juce::Colour titleCol,
                                        const std::vector<const char*>& pills) {
            g.setColour(titleCol);
            g.drawText(groupTitle, x, 0, 32, getHeight(), juce::Justification::centredLeft);
            x += 30;
            for (const char* p : pills) {
                const juce::Rectangle<float> pr(static_cast<float>(x), 2.0f, 20.0f, static_cast<float>(getHeight() - 4));
                g.setColour(theme_.bgElevated);
                g.fillRoundedRectangle(pr, 3.0f);
                g.setColour(theme_.textSecondary);
                g.drawText(p, pr.toNearestInt(), juce::Justification::centred);
                x += 23;
            }
            x += 10;
        };

        drawBadgeGroup("ENV",  juce::Colour(0xff38bdf8), { "E1", "E2", "E3", "E4" });
        drawBadgeGroup("LFO",  juce::Colour(0xfff59e0b), { "L1", "L2", "L3", "L4" });
        drawBadgeGroup("PLAY", juce::Colour(0xffeab308), { "VEL", "KEY", "MW" });
        drawBadgeGroup("MAC",  juce::Colour(0xffa855f7), { "1", "2", "3", "4" });
    }

    void resized() override {
        auto rightArea = getLocalBounds().withTrimmedLeft(getWidth() - 410);
        const int bw = (rightArea.getWidth() - 28) / 7;
        for (auto& b : chordBtns_) {
            b.setBounds(rightArea.removeFromLeft(bw).reduced(0, 1));
            rightArea.removeFromLeft(4);
        }
    }

private:
    [[maybe_unused]] NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    std::array<juce::TextButton, 7> chordBtns_;
    std::array<bool, 7> chordHeld_{};
};

// ==============================================================================
// 6. NEW 8-REGION WORKSPACE PAGE (NOVA SYNTH NEXT)
// ==============================================================================
class NovaSynthWorkspacePage : public juce::Component {
public:
    std::function<void()> onOpenMixerDrawer;

    NovaSynthWorkspacePage(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc),
          sourceList_(proc, theme),
          visualizerHub_(proc, theme),
          sourceDeck_(proc, theme),
          filterDeck_(proc, theme),
          envDeck_(proc, theme),
          workbench_(proc, theme),
          keyboardBar_(proc, theme) {
        addAndMakeVisible(sourceList_);
        addAndMakeVisible(visualizerHub_);
        addAndMakeVisible(sourceDeck_);
        addAndMakeVisible(filterDeck_);
        addAndMakeVisible(envDeck_);
        addAndMakeVisible(workbench_);
        addAndMakeVisible(keyboardBar_);

        workbench_.onOpenMixerDrawer = [this]() {
            if (onOpenMixerDrawer) {
                onOpenMixerDrawer();
            }
        };

        sourceList_.onSourceSelected = [this](int slotIdx) {
            const auto params = proc_.getCurrentParameters();
            if (slotIdx >= 0 && slotIdx < 8 && !params.oscSlots[static_cast<std::size_t>(slotIdx)].enabled) {
                const juce::String prefix = "osc" + juce::String(slotIdx + 1) + "_";
                if (auto* p = proc_.getAPVTS().getParameter(prefix + "enabled")) {
                    p->setValueNotifyingHost(1.0f);
                }
                sourceDeck_.refreshCards();
            }
            sourceDeck_.scrollToSlot(slotIdx);
            workbench_.setSelectedOscIndex(slotIdx);
        };
        auto notifyAllSourceViews = [this]() {
            if (isSyncingSourceViews_) return;
            isSyncingSourceViews_ = true;
            sourceList_.repaint();
            sourceDeck_.refreshCards();
            workbench_.repaint();
            isSyncingSourceViews_ = false;
        };

        sourceList_.onSourceStateChanged = notifyAllSourceViews;
        sourceDeck_.onSourceStateChanged = notifyAllSourceViews;
        workbench_.onSourceStateChanged  = notifyAllSourceViews;

        filterDeck_.onFilterStateChanged = [this]() {
            workbench_.repaint();
        };
        workbench_.onFilterStateChanged = [this]() {
            filterDeck_.repaint();
        };
    }

    void syncSourceStates() {
        if (isSyncingSourceViews_) return;
        isSyncingSourceViews_ = true;
        sourceList_.repaint();
        sourceDeck_.refreshCards();
        workbench_.repaint();
        filterDeck_.repaint();
        isSyncingSourceViews_ = false;
    }

    void setInitialWorkbenchTab(int tabIdx) {
        workbench_.selectWorkbenchTab(tabIdx);
    }

    void updateModulationTelemetry(const nova::dsp::WorshipSynthEngine::Telemetry& tel) {
        filterDeck_.updateModulationTelemetry(tel);
        keyboardBar_.syncFromTelemetry(tel.modWheelValue, tel.pitchBendValue);
    }

    void resized() override {
        auto area = getLocalBounds().reduced(8, 6);

        // Zone 8: Bottom Standalone Keyboard (62px)
        keyboardBar_.setBounds(area.removeFromBottom(62));
        area.removeFromBottom(6);

        // Zone 7: Modular Workbench (220px) - Displays 6 sources in Flow before scrolling
        workbench_.setBounds(area.removeFromBottom(220));
        area.removeFromBottom(6);

        // Zone 5 & 6: Dual Filter Deck & Envelope Quad Deck (168px)
        auto midArea = area.removeFromBottom(168);
        area.removeFromBottom(6);

        const int filterW = static_cast<int>(static_cast<float>(midArea.getWidth()) * 0.44f);
        filterDeck_.setBounds(midArea.removeFromLeft(filterW));
        midArea.removeFromLeft(6);
        envDeck_.setBounds(midArea);

        // Zone 2 (left column 160px) & Zone 3 (visualizer hub) + Zone 4 (source deck)
        sourceList_.setBounds(area.removeFromLeft(160));
        area.removeFromLeft(6);

        auto upperRight = area;
        const int visualizerH = std::clamp(static_cast<int>(static_cast<float>(upperRight.getHeight()) * 0.36f), 108, 122);
        visualizerHub_.setBounds(upperRight.removeFromTop(visualizerH));
        upperRight.removeFromTop(6);
        sourceDeck_.setBounds(upperRight);
    }

private:
    [[maybe_unused]] NovaSynthProcessor& proc_;
    SourceListManager sourceList_;
    DualVisualizerHub visualizerHub_;
    SourceDeck sourceDeck_;
    DualFilterDeck filterDeck_;
    EnvelopeQuadDeck envDeck_;
    JornaLowerWorkbench workbench_;
    StandaloneKeyboardBar keyboardBar_;
    bool isSyncingSourceViews_ = false;
};

} // namespace

NovaSynthEditor::NovaSynthEditor(NovaSynthProcessor& proc)
    : AudioProcessorEditor(&proc),
      processor_(proc),
      theme_(nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan)),
      lookAndFeel_(theme_),
      shell_(proc, "NOVA SYNTH", "", theme_) {
    setLookAndFeel(&lookAndFeel_);
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);
    addAndMakeVisible(shell_);
    addAndMakeVisible(mixerDrawer_);

    auto appLogo = juce::ImageFileFormat::loadFrom(
        NovaResources::NovaSynth_Logo_png, static_cast<std::size_t>(NovaResources::NovaSynth_Logo_pngSize));
    if (!appLogo.isValid()) {
        appLogo = juce::ImageFileFormat::loadFrom(
            NovaResources::NovaSynth_Icon_256_png, static_cast<std::size_t>(NovaResources::NovaSynth_Icon_256_pngSize));
    }
    if (appLogo.isValid()) {
        shell_.setAppIcon(appLogo);
    }

#if JUCE_MAC
    if (juce::JUCEApplicationBase::isStandaloneApp()) {
        nova::mac::setApplicationDockIcon();
    }
#endif

    shell_.setTextScaleCallback([this](nova::ui::NovaTheme::TextScale scale) {
        theme_.setTextScale(scale);
        lookAndFeel_.setTheme(theme_);
        repaint();
    });

    auto wsPage = std::make_unique<NovaSynthWorkspacePage>(processor_, theme_);
    auto* wsPagePtr = wsPage.get();
    wsPage->onOpenMixerDrawer = [this]() { mixerDrawer_.setDrawerOpen(true); };
    onSetWorkbenchTab_ = [wsPagePtr](int tabIdx) {
        wsPagePtr->setInitialWorkbenchTab(tabIdx);
    };
    onUpdateTelemetry_ = [wsPagePtr](const nova::dsp::WorshipSynthEngine::Telemetry& tel) {
        wsPagePtr->updateModulationTelemetry(tel);
    };
    onSyncUI_ = [wsPagePtr]() {
        wsPagePtr->syncSourceStates();
    };

    const auto initParams = processor_.getCurrentParameters();
    for (int i = 0; i < 8; ++i) {
        lastOscEnabled_[static_cast<std::size_t>(i)] = initParams.oscSlots[static_cast<std::size_t>(i)].enabled;
    }
    lastFlt1Enabled_ = initParams.filter1Enabled;
    lastFlt2Enabled_ = initParams.filter2Enabled;

    shell_.addPage("WORKSPACE", std::move(wsPage));

    shell_.setMixerCallback([this]() {
        mixerDrawer_.toggleDrawer();
    });

    shell_.setPatchesCallback([]() {
        // Reserved for future left slide-over patch browser
    });

    constexpr int kMinWidth = 1000;
    constexpr int kMinHeight = 720;
    constexpr int kMaxWidth = 16384;
    constexpr int kMaxHeight = 16384;

    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* primary = displays.getPrimaryDisplay();
    const int screenW = (primary != nullptr) ? primary->userArea.getWidth() : 1440;
    const int screenH = (primary != nullptr) ? primary->userArea.getHeight() : 900;

    const int defaultW = std::clamp(1220, kMinWidth, std::max(kMinWidth, screenW - 30));
    const int defaultH = std::clamp(screenH < 880 ? screenH - 40 : 840, kMinHeight, std::max(kMinHeight, screenH - 40));

    setResizable(true, true);
    setResizeLimits(kMinWidth, kMinHeight, kMaxWidth, kMaxHeight);
    setSize(defaultW, defaultH);
    startTimerHz(30);
}

NovaSynthEditor::~NovaSynthEditor() {
    stopTimer();
    onUpdateTelemetry_ = nullptr;
    onSetWorkbenchTab_ = nullptr;
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    if (parentDocWindow_ != nullptr) {
        parentDocWindow_->setLookAndFeel(nullptr);
    }
    setLookAndFeel(nullptr);
}

void NovaSynthEditor::paint(juce::Graphics& g) {
    g.fillAll(theme_.bgCanvas);
}

void NovaSynthEditor::resized() {
    shell_.setBounds(getLocalBounds());
    mixerDrawer_.setBounds(getLocalBounds());
}

void NovaSynthEditor::setInitialWorkbenchTab(int tabIdx) {
    if (onSetWorkbenchTab_) {
        onSetWorkbenchTab_(tabIdx);
    }
}

void NovaSynthEditor::parentHierarchyChanged() {
    configureParentWindow();
}

void NovaSynthEditor::visibilityChanged() {
    if (isVisible()) {
        configureParentWindow();
        startTimerHz(30);
    } else {
        stopTimer();
    }
}

void NovaSynthEditor::configureParentWindow() {
    if (auto* docWin = findParentComponentOfClass<juce::DocumentWindow>()) {
        parentDocWindow_ = docWin;

        constexpr int kMinWidth = 1000;
        constexpr int kMinHeight = 720;
        constexpr int kMaxWidth = 16384;
        constexpr int kMaxHeight = 16384;

        // 1. Enable Native OS Title Bar (Traffic lights on macOS, DWM on Windows, WM decorations on Linux)
        if (!docWin->isUsingNativeTitleBar()) {
            docWin->setUsingNativeTitleBar(true);
            docWin->setTitleBarButtonsRequired(juce::DocumentWindow::allButtons, true);
        }

        // Apply our sleek dark LookAndFeel to parent window popups as well
        docWin->setLookAndFeel(&lookAndFeel_);

        // 2. Minimum size enforcement with unrestricted maximum size across Standalone parent window and native peer
        if (auto* c = docWin->getConstrainer()) {
            c->setMinimumSize(kMinWidth, kMinHeight);
            c->setMaximumSize(kMaxWidth, kMaxHeight);
        }
        if (auto* peer = docWin->getPeer()) {
            peer->setConstrainer(docWin->getConstrainer());
        }

        // 3. Screen bounds guard on initial open: Ensure window opens with generous workstation proportions
        const auto& displays = juce::Desktop::getInstance().getDisplays();
        if (const auto* primary = displays.getPrimaryDisplay()) {
            const auto screenArea = primary->userArea;
            const auto winBounds = docWin->getBounds();

            const int defaultW = std::clamp(1220, kMinWidth, std::max(kMinWidth, screenArea.getWidth() - 30));
            const int defaultH = std::clamp(screenArea.getHeight() < 880 ? screenArea.getHeight() - 40 : 840,
                                            kMinHeight, std::max(kMinHeight, screenArea.getHeight() - 40));

            if (winBounds.getWidth() < kMinWidth || winBounds.getHeight() < kMinHeight) {
                const int targetW = std::max(winBounds.getWidth(), defaultW);
                const int targetH = std::max(winBounds.getHeight(), defaultH);
                const int targetX = screenArea.getX() + std::max(0, (screenArea.getWidth() - targetW) / 2);
                const int targetY = screenArea.getY() + std::max(0, (screenArea.getHeight() - targetH) / 2);
                docWin->setBounds(targetX, targetY, targetW, targetH);
            }
        }

        // 4. Hide standalone JUCE "Options" button and route into PluginShell's settings gear
        for (int i = 0; i < docWin->getNumChildComponents(); ++i) {
            auto* child = docWin->getChildComponent(i);
            if (auto* btn = dynamic_cast<juce::Button*>(child)) {
                if (btn->getButtonText().containsIgnoreCase("Options")) {
                    standaloneOptionsButton_ = btn;
                    btn->setVisible(false);
                    btn->setBounds(0, 0, 0, 0);

                    shell_.setAudioSettingsCallback([docWin, btn]() {
                        if (btn != nullptr && docWin != nullptr) {
                            btn->setBounds(0, 0, 1, 1);
                            btn->triggerClick();
                            // Directly invoke Item 1 (Audio/MIDI Settings...) without requiring a secondary click
                            juce::Timer::callAfterDelay(10, []() {
                                if (auto* modal = juce::Component::getCurrentlyModalComponent()) {
                                    modal->keyPressed(juce::KeyPress(juce::KeyPress::downKey));
                                    modal->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
                                }
                            });
                        }
                    });
                    break;
                }
            }
        }
    }
}

void NovaSynthEditor::timerCallback() {
    shell_.setMixerActive(mixerDrawer_.isDrawerOpen());
    if (onUpdateTelemetry_) {
        const auto tel = processor_.getSynthTelemetry();
        onUpdateTelemetry_(tel);
    }

    const auto params = processor_.getCurrentParameters();
    bool anyChanged = false;
    for (int i = 0; i < 8; ++i) {
        const bool curEn = params.oscSlots[static_cast<std::size_t>(i)].enabled;
        if (curEn != lastOscEnabled_[static_cast<std::size_t>(i)]) {
            lastOscEnabled_[static_cast<std::size_t>(i)] = curEn;
            anyChanged = true;
        }
    }
    if (params.filter1Enabled != lastFlt1Enabled_ || params.filter2Enabled != lastFlt2Enabled_) {
        lastFlt1Enabled_ = params.filter1Enabled;
        lastFlt2Enabled_ = params.filter2Enabled;
        anyChanged = true;
    }
    if (anyChanged && onSyncUI_) {
        onSyncUI_();
    }
}

} // namespace nova::synth
