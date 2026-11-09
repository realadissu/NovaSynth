#include "FxRackPanel.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace nova::synth {

void FxRackPanel::drawCardBackground(juce::Graphics& g, const juce::Rectangle<int>& rect, juce::Colour bg, juce::Colour border) {
    const auto r = rect.toFloat();
    g.setColour(bg);
    g.fillRoundedRectangle(r, 6.0f);
    g.setColour(border);
    g.drawRoundedRectangle(r, 6.0f, 1.0f);
}

void FxRackPanel::drawToggleSwitch(juce::Graphics& g, const juce::Rectangle<int>& rect, bool isOn, juce::Colour onColor) {
    const auto r = rect.toFloat();
    g.setColour(isOn ? onColor.withAlpha(0.28f) : juce::Colour(0xff1e293b));
    g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
    g.setColour(isOn ? onColor.withAlpha(0.75f) : juce::Colour(0xff334155));
    g.drawRoundedRectangle(r, r.getHeight() * 0.5f, 1.0f);

    const float thumbRadius = (r.getHeight() - 4.0f) * 0.5f;
    const float thumbX = isOn ? (r.getRight() - thumbRadius - 2.0f) : (r.getX() + thumbRadius + 2.0f);
    const float thumbY = r.getCentreY();

    if (isOn) {
        g.setColour(onColor.withAlpha(0.35f));
        g.fillEllipse(thumbX - thumbRadius - 2.0f, thumbY - thumbRadius - 2.0f, (thumbRadius + 2.0f) * 2.0f, (thumbRadius + 2.0f) * 2.0f);
        g.setColour(onColor);
    } else {
        g.setColour(juce::Colour(0xff64748b));
    }
    g.fillEllipse(thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);
}

void FxRackPanel::drawDelayHeader(juce::Graphics& g, const juce::Rectangle<int>& r, const nova::dsp::WorshipSynthEngine::Parameters& params) {
    g.setFont(theme_.getSectionFont(13.0f));
    g.setColour(params.delayEnabled ? juce::Colour(0xff2dd4bf) : theme_.textMuted);
    g.drawText("STEREO TAPE DELAY", r.getX() + 8, r.getY() + 5, r.getWidth() - 130, 18, juce::Justification::centredLeft);

    // Sync Division Dropdown Pill
    static constexpr std::array<const char*, 8> kDivNames = {
        "1/2", "1/4", "1/4d", "1/8", "1/8d", "1/8t", "1/16", "1/16d"
    };
    const auto sb = delaySyncBtnRect_.toFloat();
    g.setColour(juce::Colour(0xff0d161c));
    g.fillRoundedRectangle(sb, 5.0f);
    g.setColour(params.delaySync ? juce::Colour(0xff2dd4bf).withAlpha(0.85f) : theme_.borderSubtle);
    g.drawRoundedRectangle(sb, 5.0f, 1.2f);

    // Solid filled downward triangle in bright cyan
    const float arrowX = sb.getRight() - 11.0f;
    const float arrowY = sb.getCentreY();
    juce::Path arrow;
    arrow.startNewSubPath(arrowX - 4.0f, arrowY - 2.5f);
    arrow.lineTo(arrowX + 4.0f, arrowY - 2.5f);
    arrow.lineTo(arrowX, arrowY + 3.0f);
    arrow.closeSubPath();
    g.setColour(params.delaySync ? juce::Colour(0xff2dd4bf) : theme_.textMuted);
    g.fillPath(arrow);

    g.setFont(theme_.getValueFont(11.5f));
    g.setColour(params.delaySync ? juce::Colour(0xff5eead4) : theme_.textMuted);
    const juce::String divText = params.delaySync ? juce::String(kDivNames[static_cast<std::size_t>(std::clamp(params.delaySyncDiv, 0, 7))]) : "FREE";
    g.drawText(divText, delaySyncBtnRect_.withTrimmedRight(14), juce::Justification::centred);

    drawToggleSwitch(g, delayBypRect_, params.delayEnabled, juce::Colour(0xff2dd4bf));
}

void FxRackPanel::drawReverbHeader(juce::Graphics& g, const juce::Rectangle<int>& r, const nova::dsp::WorshipSynthEngine::Parameters& params) {
    const bool revOn = (params.reverbMix > 0.01f);
    g.setFont(theme_.getSectionFont(13.0f));
    g.setColour(revOn ? juce::Colour(0xffc084fc) : theme_.textMuted);
    g.drawText("NOVA SPACE REVERB", r.getX() + 8, r.getY() + 5, r.getWidth() - 180, 18, juce::Justification::centredLeft);

    // Shimmer pitch mode selector dropdown pill
    const auto sp = shimmerPitchBtnRect_.toFloat();
    g.setColour(juce::Colour(0xff1e1b2e));
    g.fillRoundedRectangle(sp, 5.0f);
    g.setColour(params.reverbShimmer > 0.05f ? juce::Colour(0xffc084fc).withAlpha(0.85f) : theme_.borderSubtle);
    g.drawRoundedRectangle(sp, 5.0f, 1.2f);

    // Solid filled downward triangle in purple/lavender
    const float arrowX = sp.getRight() - 11.0f;
    const float arrowY = sp.getCentreY();
    juce::Path arrow;
    arrow.startNewSubPath(arrowX - 4.0f, arrowY - 2.5f);
    arrow.lineTo(arrowX + 4.0f, arrowY - 2.5f);
    arrow.lineTo(arrowX, arrowY + 3.0f);
    arrow.closeSubPath();
    g.setColour(params.reverbShimmer > 0.05f ? juce::Colour(0xffc084fc) : theme_.textMuted);
    g.fillPath(arrow);

    g.setFont(theme_.getValueFont(11.5f));
    g.setColour(params.reverbShimmer > 0.05f ? juce::Colour(0xffe9d5ff) : theme_.textMuted);

    juce::String pitchText = "+1 OCT";
    switch (params.reverbShimmerPitch) {
        case 0: pitchText = "+1 OCT"; break;
        case 1: pitchText = "+2 OCT"; break;
        case 2: pitchText = "+1+2 OCT"; break;
        case 3: pitchText = "-1 OCT"; break;
        default: break;
    }
    g.drawText(pitchText, shimmerPitchBtnRect_.withTrimmedRight(14), juce::Justification::centred);

    const auto fb = freezeBtnRect_.toFloat();
    if (params.reverbFreeze) {
        g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.28f));
        g.fillRoundedRectangle(fb, 4.0f);
        g.setColour(juce::Colour(0xff38bdf8));
        g.drawRoundedRectangle(fb, 4.0f, 1.2f);
        g.setFont(theme_.getValueFont(10.0f));
        g.setColour(juce::Colour(0xff38bdf8));
    } else {
        g.setColour(juce::Colour(0xff1e1b2e));
        g.fillRoundedRectangle(fb, 4.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(fb, 4.0f, 1.0f);
        g.setFont(theme_.getValueFont(10.0f));
        g.setColour(theme_.textMuted);
    }
    g.drawText("FREEZE", freezeBtnRect_, juce::Justification::centred);

    drawToggleSwitch(g, reverbBypRect_, revOn, juce::Colour(0xffc084fc));
}

void FxRackPanel::drawMacrosHeader(juce::Graphics& g, const juce::Rectangle<int>& r, const nova::dsp::WorshipSynthEngine::Parameters& /*params*/) {
    g.setFont(theme_.getSectionFont(13.0f));
    g.setColour(juce::Colour(0xfffbbf24));
    g.drawText("PERFORMANCE MACROS", r.getX() + 8, r.getY() + 5, r.getWidth() - 16, 18, juce::Justification::centredLeft);
}

void FxRackPanel::drawMasterHeader(juce::Graphics& g, const juce::Rectangle<int>& r, const nova::dsp::WorshipSynthEngine::Parameters& /*params*/) {
    g.setFont(theme_.getSectionFont(13.0f));
    g.setColour(juce::Colour(0xff22c55e));
    g.drawText("MASTER", r.getX() + 8, r.getY() + 5, r.getWidth() - 16, 18, juce::Justification::centredLeft);
}

void FxRackPanel::drawDelayVisualizer(juce::Graphics& g, const juce::Rectangle<int>& rect, const nova::dsp::WorshipSynthEngine::Parameters& params) {
    const auto r = rect.toFloat();
    g.setColour(juce::Colour(0xff070d13));
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(params.delayEnabled ? juce::Colour(0xff2dd4bf).withAlpha(0.25f) : theme_.borderSubtle);
    g.drawRoundedRectangle(r, 4.0f, 1.0f);

    if (!params.delayEnabled) {
        g.setFont(theme_.getValueFont(11.0f));
        g.setColour(theme_.textMuted);
        g.drawText("DELAY BYPASSED", rect, juce::Justification::centred);
        return;
    }

    const float midY = r.getCentreY();
    g.setColour(juce::Colour(0xff162530));
    g.drawHorizontalLine(static_cast<int>(midY), r.getX() + 4.0f, r.getRight() - 4.0f);

    g.setFont(theme_.getValueFont(11.0f));
    g.setColour(juce::Colour(0xff2dd4bf).withAlpha(0.70f));
    g.drawText("L", static_cast<int>(r.getX() + 6), static_cast<int>(r.getY() + 3), 14, 14, juce::Justification::centredLeft);
    g.drawText("R", static_cast<int>(r.getX() + 6), static_cast<int>(midY + 4), 14, 14, juce::Justification::centredLeft);

    const float effSec = params.delaySync
        ? nova::dsp::WorshipSynthEngine::getDelayTimeForDivision(params.bpm, params.delaySyncDiv)
        : params.delayTimeSec;
    const float normT = std::clamp((effSec - 0.05f) / 1.45f, 0.05f, 1.0f);
    const float dx = 20.0f + normT * 24.0f;
    const float fdbk = std::clamp(params.delayFeedback, 0.1f, 0.92f);
    const int pulseTap = static_cast<int>(std::fmod(phase_ * (1.6f / std::max(0.12f, effSec)), 6.0f));
    const float swing = params.delaySwing; // 0.0 to 1.0

    for (int i = 0; i < 6; ++i) {
        const bool isL = (i % 2 == 0);
        const float swingOffset = (!isL) ? (swing * dx * 0.333f) : 0.0f;
        const float px = r.getX() + 26.0f + static_cast<float>(i) * dx + swingOffset;
        if (px > r.getRight() - 14.0f) break;

        const float amp = std::pow(fdbk, static_cast<float>(i) * 0.65f);
        const float barH = std::max(4.0f, (r.getHeight() * 0.38f) * amp);
        const float barY = isL ? (midY - barH) : midY;
        const bool isPulse = (i == pulseTap);

        const juce::Rectangle<float> tapBar(px, barY, 5.0f, barH);
        g.setColour(isPulse ? juce::Colour(0xff67e8f9) : juce::Colour(0xff2dd4bf).withAlpha(0.60f * amp + 0.25f));
        g.fillRoundedRectangle(tapBar, 2.0f);
        if (isPulse) {
            g.setColour(juce::Colour(0xff67e8f9).withAlpha(0.35f));
            g.drawRoundedRectangle(tapBar.expanded(2.0f, 1.0f), 3.0f, 1.0f);
        }
    }

    g.setFont(theme_.getValueFont(11.0f));
    g.setColour(juce::Colour(0xff2dd4bf));
    static constexpr std::array<const char*, 8> kDivs = { "1/2", "1/4", "1/4d", "1/8", "1/8d", "1/8t", "1/16", "1/16d" };
    const juce::String timeLabel = params.delaySync
        ? (juce::String(kDivs[static_cast<std::size_t>(std::clamp(params.delaySyncDiv, 0, 7))]) + " (" + juce::String(static_cast<int>(effSec * 1000.0f)) + " ms)")
        : (juce::String(static_cast<int>(params.delayTimeSec * 1000.0f)) + " ms");
    const juce::String swingLabel = (params.delaySwing > 0.01f)
        ? (" | Swing: " + juce::String(static_cast<int>(params.delaySwing * 100.0f)) + "%")
        : " | Straight";
    g.drawText(timeLabel + swingLabel + " | " + juce::String(static_cast<int>(params.delayMix * 100.0f)) + "%",
               rect.reduced(6, 2), juce::Justification::bottomRight);
}

} // namespace nova::synth

