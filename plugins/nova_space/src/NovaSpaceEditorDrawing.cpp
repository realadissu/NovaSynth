#include "NovaSpaceEditor.h"
#include "nova_dsp/filters/StateVariableFilter.h"
#include <algorithm>
#include <cmath>

namespace nova::space {

// ==============================================================================
// ReverbSpaceVisualizer - Paint & Render Helpers
// ==============================================================================

void ReverbSpaceVisualizer::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(theme_.bgSurface);
    g.fillRoundedRectangle(bounds, theme_.cornerRadius);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(bounds, theme_.cornerRadius, 1.0f);

    const auto params = proc_.getCurrentParameters();
    const auto tel = proc_.getReverbTelemetry();
    const bool isLush = (params.engineMode == nova::dsp::DualEngineReverb::EngineMode::LushCloud);
    const auto activeAccent = isLush ? theme_.accentSecondary : theme_.accentPrimary;

    // Update toggle states visually
    engineBtns_[0].setToggleState(!isLush, juce::dontSendNotification);
    engineBtns_[1].setToggleState(isLush, juce::dontSendNotification);
    for (int i = 0; i < 4; ++i) {
        spaceBtns_[static_cast<std::size_t>(i)].setToggleState(
            static_cast<int>(params.spaceType) == i, juce::dontSendNotification);
    }

    // Graph plotting area
    const auto graphArea = bounds.withTrimmedTop(44.0f).withTrimmedBottom(38.0f).reduced(14.0f, 6.0f);
    g.setColour(theme_.bgControl);
    g.fillRoundedRectangle(graphArea, 6.0f);
    g.setColour(theme_.borderSubtle.withAlpha(0.7f));
    g.drawRoundedRectangle(graphArea, 6.0f, 1.0f);

    // Subtle grid lines
    g.setColour(theme_.borderSubtle.withAlpha(0.35f));
    for (int i = 1; i < 4; ++i) {
        const float gx = graphArea.getX() + graphArea.getWidth() * (static_cast<float>(i) * 0.25f);
        g.drawVerticalLine(static_cast<int>(gx), graphArea.getY(), graphArea.getBottom());
    }

    drawEarlyReflections(g, graphArea, params);
    drawDecayEnvelope(g, graphArea, params, isLush, activeAccent);
    drawEnergyBars(g, graphArea, tel, activeAccent);
    drawReadoutText(g, graphArea, params, tel);

    animPhase_ += 0.08f;
}

void ReverbSpaceVisualizer::drawEarlyReflections(juce::Graphics& g, juce::Rectangle<float> graphArea,
                                                const nova::dsp::DualEngineReverb::Parameters& params) {
    const float preNorm = std::clamp(params.preDelayMs / 250.0f, 0.0f, 1.0f) * 0.18f;
    const float erStartX = graphArea.getX() + 12.0f + preNorm * graphArea.getWidth();
    const float erWidth = graphArea.getWidth() * 0.22f * std::clamp(params.size, 0.3f, 1.3f);

    g.setColour(theme_.accentPrimary.withAlpha(0.55f));
    for (int t = 0; t < 8; ++t) {
        const float tx = erStartX + (static_cast<float>(t) / 8.0f) * erWidth;
        const float th = graphArea.getHeight() * (0.68f - static_cast<float>(t) * 0.06f) * params.erLevel;
        g.drawLine(tx, graphArea.getBottom() - 4.0f, tx, graphArea.getBottom() - 4.0f - th, 1.8f);
    }
}

void ReverbSpaceVisualizer::drawDecayEnvelope(juce::Graphics& g, juce::Rectangle<float> graphArea,
                                             const nova::dsp::DualEngineReverb::Parameters& params,
                                             bool isLush, juce::Colour activeAccent) {
    const float preNorm = std::clamp(params.preDelayMs / 250.0f, 0.0f, 1.0f) * 0.18f;
    const float erStartX = graphArea.getX() + 12.0f + preNorm * graphArea.getWidth();
    const float erWidth = graphArea.getWidth() * 0.22f * std::clamp(params.size, 0.3f, 1.3f);

    const float decayNorm = std::clamp(std::log10(params.decaySeconds + 0.8f) / std::log10(30.8f), 0.08f, 1.0f);
    const float tailStartX = erStartX + erWidth * 0.35f;
    const float tailEndX = tailStartX + (graphArea.getRight() - tailStartX - 8.0f) * decayNorm;

    juce::Path envPath;
    envPath.startNewSubPath(tailStartX, graphArea.getBottom() - 4.0f);
    constexpr int kCurvePts = 48;
    for (int i = 0; i <= kCurvePts; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(kCurvePts);
        const float x = tailStartX + u * (tailEndX - tailStartX);
        const float env = params.freeze
            ? (0.78f + 0.06f * std::sin(u * 6.28f + animPhase_))
            : std::exp(-3.4f * u * (1.0f + params.highDamping * 0.45f));
        const float shimmerRipple = isLush
            ? (0.04f * params.shimmerOctave * std::sin(u * 18.0f - animPhase_ * 2.5f))
            : 0.0f;
        const float y = graphArea.getBottom() - 6.0f -
                        std::clamp(env + shimmerRipple, 0.0f, 0.95f) * (graphArea.getHeight() - 18.0f);
        envPath.lineTo(x, y);
    }
    envPath.lineTo(tailEndX, graphArea.getBottom() - 4.0f);
    envPath.closeSubPath();

    juce::ColourGradient grad(activeAccent.withAlpha(0.32f), tailStartX, graphArea.getY(),
                              activeAccent.withAlpha(0.03f), tailStartX, graphArea.getBottom(), false);
    g.setGradientFill(grad);
    g.fillPath(envPath);

    g.setColour(activeAccent);
    g.strokePath(envPath, juce::PathStrokeType(2.0f));
}

void ReverbSpaceVisualizer::drawEnergyBars(juce::Graphics& g, juce::Rectangle<float> graphArea,
                                          const nova::dsp::DualEngineReverb::Telemetry& tel,
                                          juce::Colour activeAccent) {
    const float barRegionW = graphArea.getWidth() * 0.42f;
    const float barStartX = graphArea.getRight() - barRegionW - 10.0f;
    const float barStep = barRegionW / 16.0f;
    for (int ch = 0; ch < 16; ++ch) {
        const float e = std::clamp(tel.channelEnergies[static_cast<std::size_t>(ch)] * 3.5f, 0.02f, 1.0f);
        const float bh = e * (graphArea.getHeight() * 0.55f);
        const float bx = barStartX + static_cast<float>(ch) * barStep;
        g.setColour(activeAccent.withAlpha(0.45f + 0.45f * e));
        g.fillRoundedRectangle(bx, graphArea.getBottom() - 6.0f - bh, barStep - 2.0f, bh, 1.5f);
    }
}

void ReverbSpaceVisualizer::drawReadoutText(juce::Graphics& g, juce::Rectangle<float> graphArea,
                                           const nova::dsp::DualEngineReverb::Parameters& params,
                                           const nova::dsp::DualEngineReverb::Telemetry& tel) {
    g.setFont(theme_.getValueFont(13.0f));
    g.setColour(theme_.textPrimary);
    const juce::String readout = juce::String("RT60: ") + juce::String(params.decaySeconds, 2) + " s   |   "
                               + "Size: " + juce::String(static_cast<int>(params.size * 100.0f)) + "%   |   "
                               + "Pre-Delay: " + juce::String(params.preDelayMs, 1) + " ms";
    g.drawText(readout, graphArea.toNearestInt().reduced(12, 8), juce::Justification::topLeft, false);

    if (tel.duckingGainReductionDb < -0.2f) {
        g.setColour(theme_.accentWarm);
        g.drawText("Ducking: " + juce::String(tel.duckingGainReductionDb, 1) + " dB",
                   graphArea.toNearestInt().reduced(12, 8), juce::Justification::topRight, false);
    }
}

// ==============================================================================
// ToneEqPage - Paint
// ==============================================================================

void ToneEqPage::paint(juce::Graphics& g) {
    auto eqBox = getLocalBounds().removeFromTop(245).toFloat().reduced(1.0f);
    g.setColour(theme_.bgSurface);
    g.fillRoundedRectangle(eqBox, theme_.cornerRadius);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(eqBox, theme_.cornerRadius, 1.0f);

    const auto plot = eqBox.reduced(18.0f, 22.0f);
    g.setColour(theme_.bgControl);
    g.fillRoundedRectangle(plot, 6.0f);

    const auto p = proc_.getCurrentParameters();
    nova::dsp::StateVariableFilter hp, lp, carve;
    hp.prepare(48000.0);
    lp.prepare(48000.0);
    carve.prepare(48000.0);
    hp.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, p.lowCutHz, 0.707f);
    lp.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, p.highCutHz, 0.707f);
    carve.setParameters(nova::dsp::StateVariableFilter::Type::Bell, 2200.0f, 0.85f, -9.5f * p.vocalCarve);

    juce::Path curve;
    constexpr int kSteps = 96;
    for (int i = 0; i <= kSteps; ++i) {
        const float normX = static_cast<float>(i) / static_cast<float>(kSteps);
        const float freq = 20.0f * std::pow(1000.0f, normX); // 20 Hz .. 20 kHz
        const float mag = hp.getMagnitudeAtFrequency(freq)
                        * lp.getMagnitudeAtFrequency(freq)
                        * carve.getMagnitudeAtFrequency(freq);
        const float db = 20.0f * std::log10(std::max(0.05f, mag));
        const float normY = std::clamp(0.32f - (db / 28.0f), 0.05f, 0.95f);

        const float px = plot.getX() + normX * plot.getWidth();
        const float py = plot.getY() + normY * plot.getHeight();
        if (i == 0) curve.startNewSubPath(px, py);
        else        curve.lineTo(px, py);
    }

    g.setColour(theme_.accentPrimary);
    g.strokePath(curve, juce::PathStrokeType(2.2f));

    g.setFont(theme_.getSectionFont(13.0f));
    g.setColour(theme_.textSecondary);
    g.drawText("VOCAL CARVE & DAMPING SPECTRUM (20 Hz - 20 kHz)",
               plot.toNearestInt().reduced(12, 8), juce::Justification::topLeft, false);
}

// ==============================================================================
// NovaSpaceEditor - Paint
// ==============================================================================

void NovaSpaceEditor::paint(juce::Graphics& g) {
    g.fillAll(theme_.bgCanvas);
}

} // namespace nova::space

