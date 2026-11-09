#include "WorkbenchVisualizers.h"
#include <cmath>
#include <algorithm>
#include <array>

namespace nova::synth {

void drawReverbVisualizer(juce::Graphics& g,
                          const juce::Rectangle<int>& rect,
                          const nova::dsp::WorshipSynthEngine::Parameters& params,
                          const nova::ui::NovaTheme& theme,
                          float phase) {
    const auto r = rect.toFloat();
    g.setColour(juce::Colour(0xff0c0617));
    g.fillRoundedRectangle(r, 4.0f);
    const bool revOn = (params.reverbMix > 0.01f);
    g.setColour(revOn ? juce::Colour(0xffc084fc).withAlpha(0.25f) : theme.borderSubtle);
    g.drawRoundedRectangle(r, 4.0f, 1.0f);

    if (!revOn) {
        g.setFont(theme.getValueFont(11.0f));
        g.setColour(theme.textMuted);
        g.drawText("REVERB BYPASSED", rect, juce::Justification::centred);
        return;
    }

    const float startX = r.getX() + 6.0f;
    const float endX = r.getRight() - 6.0f;
    const float baseW = endX - startX;
    const float curveH = r.getHeight() - 8.0f;
    const float bottomY = r.getBottom() - 4.0f;
    const float tau = 0.25f + (params.reverbDecay / 28.0f) * 0.75f;

    juce::Path decayPath, decayFill;
    decayPath.startNewSubPath(startX, bottomY - curveH * 0.88f);
    decayFill.startNewSubPath(startX, bottomY);
    decayFill.lineTo(startX, bottomY - curveH * 0.88f);

    for (float step = 1.0f; step <= 36.0f; step += 1.0f) {
        const float u = step / 36.0f;
        const float px = startX + u * baseW;
        float v = 0.0f;
        if (params.reverbFreeze) {
            v = 0.82f + 0.05f * std::sin(u * 14.0f + phase * 2.5f);
        } else {
            v = std::exp(-u / tau) * (0.85f + 0.04f * std::sin(u * 16.0f + phase * 1.5f));
        }
        const float py = bottomY - std::clamp(v, 0.04f, 0.94f) * curveH;
        decayPath.lineTo(px, py);
        decayFill.lineTo(px, py);
    }
    decayFill.lineTo(endX, bottomY);
    decayFill.closeSubPath();

    juce::ColourGradient fillGrad(
        params.reverbFreeze ? juce::Colour(0xff38bdf8).withAlpha(0.35f) : juce::Colour(0xffc084fc).withAlpha(0.28f),
        startX, r.getY(),
        juce::Colour(0xff0c0617).withAlpha(0.02f),
        startX, bottomY, false);
    g.setGradientFill(fillGrad);
    g.fillPath(decayFill);

    g.setColour(params.reverbFreeze ? juce::Colour(0xff67e8f9) : juce::Colour(0xffd8b4fe));
    g.strokePath(decayPath, juce::PathStrokeType(1.4f));

    if (params.reverbShimmer > 0.05f) {
        const int numStars = static_cast<int>(3 + params.reverbShimmer * 5.0f);
        for (int s = 0; s < numStars; ++s) {
            const float su = std::fmod(static_cast<float>(s) * 0.23f + phase * 0.04f, 0.95f);
            const float sv = std::fmod(static_cast<float>(s) * 0.31f + phase * 0.06f, 0.85f);
            const float sx = startX + su * baseW;
            const float sy = bottomY - sv * curveH;
            juce::Path star;
            const float sz = 3.2f + params.reverbShimmer * 2.0f;
            star.startNewSubPath(sx, sy - sz);
            star.lineTo(sx + sz * 0.45f, sy);
            star.lineTo(sx, sy + sz);
            star.lineTo(sx - sz * 0.45f, sy);
            star.closeSubPath();
            g.setColour(juce::Colour(0xfffde047).withAlpha(0.75f * params.reverbShimmer));
            g.fillPath(star);
        }
    }

    g.setFont(theme.getValueFont(9.5f));
    if (params.reverbFreeze) {
        g.setColour(juce::Colour(0xff38bdf8));
        g.drawText("HOLD INFINITE", rect.reduced(6, 2), juce::Justification::bottomRight);
    } else {
        g.setColour(juce::Colour(0xffc084fc).withAlpha(0.85f));
        const int dryPct = static_cast<int>(std::round(params.reverbDry * 100.0f));
        const int wetPct = static_cast<int>(std::round(params.reverbMix * 100.0f));
        g.drawText(juce::String(params.reverbDecay, 1) + " s | D: " + juce::String(dryPct) + "% W: " + juce::String(wetPct) + "%",
                   rect.reduced(6, 2), juce::Justification::bottomRight);
    }
}

void drawMacrosVisualizer(juce::Graphics& g,
                          const juce::Rectangle<int>& rect,
                          const nova::dsp::WorshipSynthEngine::Parameters& params,
                          const nova::ui::NovaTheme& theme,
                          float phase) {
    const auto r = rect.toFloat();
    g.setColour(juce::Colour(0xff140d04));
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(juce::Colour(0xfffbbf24).withAlpha(0.25f));
    g.drawRoundedRectangle(r, 4.0f, 1.0f);

    const float startX = r.getX() + 6.0f;
    const float endX = r.getRight() - 6.0f;
    const float baseW = endX - startX;
    const float curveH = r.getHeight() - 8.0f;
    const float bottomY = r.getBottom() - 4.0f;

    static constexpr std::array<float, 3> kFreqs = { 0.22f, 0.54f, 0.84f };
    static constexpr std::array<const char*, 3> kLabels = { "100", "1k", "10k" };
    g.setFont(theme.getValueFont(8.0f));
    for (std::size_t fi = 0; fi < 3; ++fi) {
        const float fx = startX + kFreqs[fi] * baseW;
        g.setColour(juce::Colour(0xfffbbf24).withAlpha(0.12f));
        g.drawVerticalLine(static_cast<int>(fx), r.getY() + 4.0f, r.getBottom() - 4.0f);
        g.setColour(theme.textMuted.withAlpha(0.5f));
        g.drawText(kLabels[fi], static_cast<int>(fx - 12), static_cast<int>(r.getBottom() - 14), 24, 10, juce::Justification::centred);
    }

    juce::Path curvePath, fillPath;
    auto getGainAt = [&](float u) {
        const float gw = params.macroWarmth * std::exp(-u * 4.0f) * 0.38f;
        const float gm = params.macroMotion * 0.12f * std::sin(u * 16.0f - phase * 2.5f) * std::sin(u * 3.14159f);
        const float diff = (u - 0.70f) / 0.14f;
        const float gs = params.macroShimmer * 0.28f * std::exp(-diff * diff);
        const float ga = params.macroAir * 0.30f * (1.0f / (1.0f + std::exp(-(u - 0.76f) * 10.0f)));
        return 0.28f + gw + gm + gs + ga;
    };

    curvePath.startNewSubPath(startX, bottomY - std::clamp(getGainAt(0.0f), 0.06f, 0.94f) * curveH);
    fillPath.startNewSubPath(startX, bottomY);
    fillPath.lineTo(startX, bottomY - std::clamp(getGainAt(0.0f), 0.06f, 0.94f) * curveH);

    for (float step = 1.0f; step <= 36.0f; step += 1.0f) {
        const float u = step / 36.0f;
        const float px = startX + u * baseW;
        const float py = bottomY - std::clamp(getGainAt(u), 0.06f, 0.94f) * curveH;
        curvePath.lineTo(px, py);
        fillPath.lineTo(px, py);
    }
    fillPath.lineTo(endX, bottomY);
    fillPath.closeSubPath();

    juce::ColourGradient fillGrad(juce::Colour(0xfffbbf24).withAlpha(0.24f), startX, r.getY(),
                                 juce::Colour(0xff140d04).withAlpha(0.02f), startX, bottomY, false);
    g.setGradientFill(fillGrad);
    g.fillPath(fillPath);

    g.setColour(juce::Colour(0xfffbbf24));
    g.strokePath(curvePath, juce::PathStrokeType(1.5f));

    static constexpr std::array<std::pair<float, const char*>, 4> kNodes = {{
        { 0.12f, "W" }, { 0.40f, "M" }, { 0.70f, "S" }, { 0.90f, "A" }
    }};
    g.setFont(theme.getValueFont(9.0f));
    for (const auto& [nu, nlabel] : kNodes) {
        const float nx = startX + nu * baseW;
        const float ny = bottomY - std::clamp(getGainAt(nu), 0.06f, 0.94f) * curveH;
        g.setColour(juce::Colour(0xfffbbf24));
        g.fillEllipse(nx - 2.5f, ny - 2.5f, 5.0f, 5.0f);
        g.setColour(juce::Colour(0xfffef08a));
        g.drawText(nlabel, static_cast<int>(nx - 8), static_cast<int>(ny - 14), 16, 12, juce::Justification::centred);
    }
}

void drawMasterVisualizer(juce::Graphics& g,
                          const juce::Rectangle<int>& rect,
                          const nova::ui::NovaTheme& theme,
                          float smoothPeakL,
                          float smoothPeakR,
                          float peakHoldL,
                          float peakHoldR) {
    const auto r = rect.toFloat();
    g.setColour(juce::Colour(0xff06130b));
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(juce::Colour(0xff22c55e).withAlpha(0.25f));
    g.drawRoundedRectangle(r, 4.0f, 1.0f);

    const bool isClipping = (smoothPeakL > 0.98f || smoothPeakR > 0.98f);
    const float ledX = r.getRight() - 12.0f;
    const float ledY = r.getY() + 7.0f;
    g.setColour(isClipping ? juce::Colour(0xffef4444) : juce::Colour(0xff1f2937));
    g.fillEllipse(ledX - 3.0f, ledY - 3.0f, 6.0f, 6.0f);
    if (isClipping) {
        g.setColour(juce::Colour(0xffef4444).withAlpha(0.35f));
        g.fillEllipse(ledX - 5.0f, ledY - 5.0f, 10.0f, 10.0f);
    }
    g.setFont(theme.getValueFont(8.5f));
    g.setColour(isClipping ? juce::Colour(0xffef4444) : theme.textMuted);
    g.drawText("CLIP", static_cast<int>(ledX - 28), static_cast<int>(ledY - 5), 24, 10, juce::Justification::centredRight);

    const float barLeft = r.getX() + 18.0f;
    const float barRight = r.getRight() - 8.0f;
    const float barW = barRight - barLeft;
    const float lY = r.getY() + 22.0f;
    const float rY = r.getY() + 36.0f;
    const float barH = 8.0f;

    g.setFont(theme.getValueFont(9.5f));
    g.setColour(juce::Colour(0xff22c55e).withAlpha(0.65f));
    g.drawText("L", static_cast<int>(r.getX() + 4), static_cast<int>(lY - 1), 12, 10, juce::Justification::centredLeft);
    g.drawText("R", static_cast<int>(r.getX() + 4), static_cast<int>(rY - 1), 12, 10, juce::Justification::centredLeft);

    auto drawBar = [&](float y, float peakVal, float holdVal) {
        const juce::Rectangle<float> bgBar(barLeft, y, barW, barH);
        g.setColour(juce::Colour(0xff102017));
        g.fillRoundedRectangle(bgBar, 2.0f);

        const float pNorm = std::clamp(std::sqrt(peakVal), 0.0f, 1.0f);
        if (pNorm > 0.01f) {
            const juce::Rectangle<float> actBar(barLeft, y, barW * pNorm, barH);
            juce::Colour barCol = juce::Colour(0xff22c55e);
            if (pNorm > 0.92f) barCol = juce::Colour(0xffef4444);
            else if (pNorm > 0.70f) barCol = juce::Colour(0xfff59e0b);
            g.setColour(barCol);
            g.fillRoundedRectangle(actBar, 2.0f);
        }

        const float hNorm = std::clamp(std::sqrt(holdVal), 0.0f, 1.0f);
        if (hNorm > 0.02f) {
            const float hx = barLeft + hNorm * barW;
            g.setColour(juce::Colours::white);
            g.drawVerticalLine(static_cast<int>(hx), y, y + barH);
        }
    };

    drawBar(lY, smoothPeakL, peakHoldL);
    drawBar(rY, smoothPeakR, peakHoldR);

    // Scale calibration tick markings
    static constexpr std::array<std::pair<float, const char*>, 4> kTicks = {{
        { 0.25f, "-24" }, { 0.50f, "-12" }, { 0.71f, "-6" }, { 1.00f, "0" }
    }};
    g.setFont(theme.getValueFont(8.0f));
    g.setColour(theme.textMuted.withAlpha(0.6f));
    for (const auto& [frac, tlabel] : kTicks) {
        const float tx = barLeft + frac * barW;
        g.drawVerticalLine(static_cast<int>(tx), rY + barH + 1.0f, rY + barH + 4.0f);
        g.drawText(tlabel, static_cast<int>(tx - 10.0f), static_cast<int>(rY + barH + 4.0f), 20, 9, juce::Justification::centred);
    }

    const float maxPeak = std::max(smoothPeakL, smoothPeakR);
    const float dbVal = (maxPeak > 1e-4f) ? (20.0f * std::log10(maxPeak)) : -60.0f;
    g.setFont(theme.getValueFont(9.5f));
    g.setColour(juce::Colour(0xff22c55e).withAlpha(0.9f));
    g.drawText((dbVal > -59.0f ? juce::String(dbVal, 1) + " dB" : "-INF"),
               rect.reduced(6, 3), juce::Justification::bottomLeft);
}

} // namespace nova::synth

