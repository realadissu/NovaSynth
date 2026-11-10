#include "JornaCenterStageColumn.h"
#include "../NovaSynthProcessor.h"
#include <cmath>
#include <algorithm>

namespace nova::synth {

JornaCenterStageColumn::JornaCenterStageColumn(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc), theme_(theme) {
    historyWave_.fill(0.0f);
    spectrumBins_.fill(0.02f);
}

void JornaCenterStageColumn::setSelectedOscIndex(int idx) noexcept {
    selectedOsc_ = std::clamp(idx, 0, 7);
    repaint();
}

void JornaCenterStageColumn::pullAudioForVisualizers() {
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

void JornaCenterStageColumn::paint(juce::Graphics& g) {
    pullAudioForVisualizers();

    const auto full = getLocalBounds();
    const auto specRect = full.withHeight(88).toFloat().reduced(1.0f);
    const auto stageRect = full.withTrimmedTop(96).toFloat().reduced(1.0f);
    const auto params = proc_.getCurrentParameters();

    drawSpectrumOutput(g, specRect);
    drawCenterStageHeader(g, stageRect, params);

    switch (centerViewMode_) {
        case 0: drawOscWaveStage(g, params); break;
        case 1: drawAllSourcesStage(g, params); break;
        case 2:
        case 3: drawFilterStage(g, params); break;
        case 4: drawMacroStage(g, params); break;
        default: break;
    }

    drawStageFooter(g, stageRect, params);
}

void JornaCenterStageColumn::mouseDown(const juce::MouseEvent& event) {
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

void JornaCenterStageColumn::mouseDrag(const juce::MouseEvent& event) {
    if (canvasRect_.contains(event.getMouseDownPosition().toFloat())) {
        dragCanvasAt(event.position);
    }
}

void JornaCenterStageColumn::dragCanvasAt(juce::Point<float> pt) {
    const float nx = std::clamp((pt.x - canvasRect_.getX()) / std::max(1.0f, canvasRect_.getWidth()), 0.0f, 1.0f);
    const float ny = std::clamp(1.0f - (pt.y - canvasRect_.getY()) / std::max(1.0f, canvasRect_.getHeight()), 0.0f, 1.0f);

    if (centerViewMode_ == 0) {
        const juce::String prefix = "osc" + juce::String(selectedOsc_ + 1) + "_";
        if (auto* s = proc_.getAPVTS().getParameter(prefix + "shape"))  s->setValueNotifyingHost(nx);
        if (auto* d = proc_.getAPVTS().getParameter(prefix + "detune")) d->setValueNotifyingHost(ny);
    } else if (centerViewMode_ == 1) {
        if (auto* w = proc_.getAPVTS().getParameter("macroWarmth"))    w->setValueNotifyingHost(nx);
        if (auto* c = proc_.getAPVTS().getParameter("filterCutoffHz"))  c->setValueNotifyingHost(ny);
    } else if (centerViewMode_ == 2) {
        if (auto* c = proc_.getAPVTS().getParameter("filterCutoffHz"))  c->setValueNotifyingHost(nx);
        if (auto* r = proc_.getAPVTS().getParameter("filterResonance")) r->setValueNotifyingHost(ny);
    } else if (centerViewMode_ == 3) {
        if (auto* c = proc_.getAPVTS().getParameter("filter2CutoffHz")) c->setValueNotifyingHost(nx);
        if (auto* r = proc_.getAPVTS().getParameter("filter2Resonance")) r->setValueNotifyingHost(ny);
    } else if (centerViewMode_ == 4) {
        if (auto* w = proc_.getAPVTS().getParameter("macroWarmth")) w->setValueNotifyingHost(nx);
        if (auto* a = proc_.getAPVTS().getParameter("macroAir"))     a->setValueNotifyingHost(ny);
    }
    repaint();
}

} // namespace nova::synth
