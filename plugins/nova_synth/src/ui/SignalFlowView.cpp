#include "SignalFlowPanel.h"
#include "../NovaSynthProcessor.h"
#include <cmath>
#include <algorithm>

namespace nova::synth {

SignalFlowPanel::SignalFlowPanel(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc), theme_(theme) {}

void SignalFlowPanel::setSelectedOscIndex(int idx) {
    selectedOscIndex_ = idx;
    repaint();
}

void SignalFlowPanel::paint(juce::Graphics& g) {
    drawSignalFlowView(g, getLocalBounds().toFloat());
}

void SignalFlowPanel::resized() {}

void SignalFlowPanel::drawAmpCard(juce::Graphics& g, const juce::Rectangle<float>& cardAmp, float commonFxY) {
    g.setColour(juce::Colour(0xff1d140e));
    g.fillRoundedRectangle(cardAmp, 8.0f);
    g.setColour(juce::Colour(0xff78350f).withAlpha(0.7f));
    g.drawRoundedRectangle(cardAmp, 8.0f, 1.0f);

    g.setFont(theme_.getTitleFont(15.0f));
    g.setColour(juce::Colour(0xfffb923c));
    g.drawText("AMP VCA", static_cast<int>(cardAmp.getX() + 10), static_cast<int>(cardAmp.getY() + 7), 80, 18, juce::Justification::centredLeft);

    const juce::Rectangle<float> ampBadge(cardAmp.getX() + 86.0f, cardAmp.getY() + 7.5f, 54.0f, 17.0f);
    g.setColour(juce::Colour(0xff78350f).withAlpha(0.5f));
    g.fillRoundedRectangle(ampBadge, 4.0f);
    g.setColour(juce::Colour(0xfffb923c).withAlpha(0.6f));
    g.drawRoundedRectangle(ampBadge, 4.0f, 1.0f);
    g.setFont(theme_.getValueFont(10.5f));
    g.setColour(juce::Colour(0xfffb923c));
    g.drawText("ENV 1", ampBadge, juce::Justification::centred);

    const juce::Rectangle<float> ampFltR(cardAmp.getX() + 8.0f, cardAmp.getY() + 32.0f, cardAmp.getWidth() * 0.52f, 26.0f);
    g.setColour(juce::Colour(0xff291811));
    g.fillRoundedRectangle(ampFltR, 5.0f);
    g.setColour(juce::Colour(0xff78350f).withAlpha(0.5f));
    g.drawRoundedRectangle(ampFltR, 5.0f, 1.0f);

    ampFltInPin_ = { ampFltR.getX() + 4.0f, ampFltR.getCentreY() };
    drawJackPin(g, ampFltInPin_, juce::Colour(0xfffb923c), true, false);
    g.setFont(theme_.getSectionFont(11.5f));
    g.setColour(juce::Colour(0xfffb923c));
    g.drawText("FLT IN", static_cast<int>(ampFltR.getX() + 14), static_cast<int>(ampFltR.getY()), static_cast<int>(ampFltR.getWidth() - 16), static_cast<int>(ampFltR.getHeight()), juce::Justification::centredLeft);

    const juce::Rectangle<float> ampDirectR(cardAmp.getX() + 8.0f, cardAmp.getY() + 62.0f, cardAmp.getWidth() * 0.52f, 26.0f);
    const bool ampDirectHov = (isDraggingCable_ && hoveredTarget_ == 3);
    g.setColour(juce::Colour(0xff291811));
    g.fillRoundedRectangle(ampDirectR, 5.0f);
    g.setColour(ampDirectHov ? juce::Colour(0xff38bdf8) : juce::Colour(0xff78350f).withAlpha(0.5f));
    g.drawRoundedRectangle(ampDirectR, 5.0f, ampDirectHov ? 1.8f : 1.0f);

    ampDirectInPin_ = { ampDirectR.getX() + 4.0f, ampDirectR.getCentreY() };
    drawJackPin(g, ampDirectInPin_, juce::Colour(0xff38bdf8), true, ampDirectHov);
    g.setFont(theme_.getSectionFont(11.5f));
    g.setColour(ampDirectHov ? juce::Colour(0xff38bdf8) : theme_.textSecondary);
    g.drawText("DIRECT IN", static_cast<int>(ampDirectR.getX() + 14), static_cast<int>(ampDirectR.getY()), static_cast<int>(ampDirectR.getWidth() - 16), static_cast<int>(ampDirectR.getHeight()), juce::Justification::centredLeft);

    const juce::Rectangle<float> ampOutR(cardAmp.getRight() - cardAmp.getWidth() * 0.38f, commonFxY - 13.0f, cardAmp.getWidth() * 0.38f - 8.0f, 26.0f);
    g.setColour(juce::Colour(0xff291811));
    g.fillRoundedRectangle(ampOutR, 5.0f);
    g.setColour(juce::Colour(0xfffb923c).withAlpha(0.6f));
    g.drawRoundedRectangle(ampOutR, 5.0f, 1.0f);

    ampOutPin_ = { ampOutR.getRight() - 4.0f, commonFxY };
    drawJackPin(g, ampOutPin_, juce::Colour(0xfffb923c), true, false);
    g.setFont(theme_.getSectionFont(11.5f));
    g.setColour(juce::Colour(0xfffb923c));
    g.drawText("OUT", static_cast<int>(ampOutR.getX() + 6), static_cast<int>(ampOutR.getY()), static_cast<int>(ampOutR.getWidth() - 16), static_cast<int>(ampOutR.getHeight()), juce::Justification::centredLeft);

    const juce::Rectangle<float> adsrBox(cardAmp.getX() + 8.0f, cardAmp.getY() + 94.0f, cardAmp.getWidth() - 16.0f, cardAmp.getBottom() - (cardAmp.getY() + 94.0f) - 8.0f);
    g.setColour(juce::Colour(0xff120d09));
    g.fillRoundedRectangle(adsrBox, 5.0f);
    g.setColour(juce::Colour(0xff451a03).withAlpha(0.5f));
    g.drawRoundedRectangle(adsrBox, 5.0f, 0.8f);

    auto& apvts = proc_.getAPVTS();
    auto* pAmpAtk = apvts.getParameter("ampAttack");
    auto* pAmpDec = apvts.getParameter("ampDecay");
    auto* pAmpSus = apvts.getParameter("ampSustain");
    auto* pAmpRel = apvts.getParameter("ampRelease");

    const float uA = pAmpAtk ? pAmpAtk->getValue() : 0.2f;
    const float uD = pAmpDec ? pAmpDec->getValue() : 0.35f;
    const float uS = pAmpSus ? pAmpSus->getValue() : 0.8f;
    const float uR = pAmpRel ? pAmpRel->getValue() : 0.35f;

    const float plotX = adsrBox.getX() + 6.0f;
    const float plotW = adsrBox.getWidth() - 12.0f;
    const float plotY = adsrBox.getY() + 14.0f;
    const float plotH = adsrBox.getHeight() - 28.0f;

    const float wA = 0.28f * plotW;
    const float wD = 0.28f * plotW;
    const float wS = 0.16f * plotW;
    const float wR = 0.28f * plotW;

    const juce::Point<float> pStart(plotX, plotY + plotH);
    const juce::Point<float> pPeak(plotX + uA * wA, plotY);
    const juce::Point<float> pSusStart(pPeak.x + uD * wD, plotY + plotH * (1.0f - uS));
    const juce::Point<float> pSusEnd(pSusStart.x + wS, pSusStart.y);
    const juce::Point<float> pEnd(pSusEnd.x + uR * wR, plotY + plotH);

    juce::Path adsrFill;
    adsrFill.startNewSubPath(pStart);
    if (uA <= 0.005f) {
        adsrFill.lineTo(pPeak);
    } else {
        adsrFill.quadraticTo(plotX + (pPeak.x - plotX) * 0.25f, plotY + plotH * 0.15f, pPeak.x, pPeak.y);
    }
    adsrFill.quadraticTo(pPeak.x + (pSusStart.x - pPeak.x) * 0.35f, pSusStart.y + (pPeak.y - pSusStart.y) * 0.30f, pSusStart.x, pSusStart.y);
    adsrFill.lineTo(pSusEnd);
    if (uR <= 0.005f) {
        adsrFill.lineTo(pEnd);
    } else {
        adsrFill.quadraticTo(pSusEnd.x + (pEnd.x - pSusEnd.x) * 0.35f, plotY + plotH * (1.0f - uS * 0.25f), pEnd.x, pEnd.y);
    }
    adsrFill.closeSubPath();

    juce::ColourGradient grad(juce::Colour(0xff38bdf8).withAlpha(0.28f), plotX, plotY,
                              juce::Colour(0xff38bdf8).withAlpha(0.02f), plotX, plotY + plotH, false);
    g.setGradientFill(grad);
    g.fillPath(adsrFill);

    juce::Path atkDec;
    atkDec.startNewSubPath(pStart);
    if (uA <= 0.005f) {
        atkDec.lineTo(pPeak);
    } else {
        atkDec.quadraticTo(plotX + (pPeak.x - plotX) * 0.25f, plotY + plotH * 0.15f, pPeak.x, pPeak.y);
    }
    atkDec.quadraticTo(pPeak.x + (pSusStart.x - pPeak.x) * 0.35f, pSusStart.y + (pPeak.y - pSusStart.y) * 0.30f, pSusStart.x, pSusStart.y);
    atkDec.lineTo(pSusEnd);
    g.setColour(juce::Colour(0xff38bdf8));
    g.strokePath(atkDec, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path rel;
    rel.startNewSubPath(pSusEnd);
    if (uR <= 0.005f) {
        rel.lineTo(pEnd);
    } else {
        rel.quadraticTo(pSusEnd.x + (pEnd.x - pSusEnd.x) * 0.35f, plotY + plotH * (1.0f - uS * 0.25f), pEnd.x, pEnd.y);
    }
    g.setColour(juce::Colour(0xfff59e0b));
    g.strokePath(rel, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (const auto& pt : { pPeak, pSusStart, pEnd }) {
        g.setColour(juce::Colour(0xfff59e0b));
        g.fillEllipse(pt.x - 2.5f, pt.y - 2.5f, 5.0f, 5.0f);
    }
}

void SignalFlowPanel::drawFxCard(juce::Graphics& g, const juce::Rectangle<float>& cardFx, float H, float commonFxY) {
    const auto params = proc_.getCurrentParameters();
    g.setColour(juce::Colour(0xff1a1022));
    g.fillRoundedRectangle(cardFx, 8.0f);
    g.setColour(juce::Colour(0xff581c87).withAlpha(0.7f));
    g.drawRoundedRectangle(cardFx, 8.0f, 1.0f);

    g.setFont(theme_.getTitleFont(15.0f));
    g.setColour(juce::Colour(0xffc084fc));
    g.drawText("FX", static_cast<int>(cardFx.getX() + 10), static_cast<int>(cardFx.getY() + 7), 80, 18, juce::Justification::centredLeft);

    toggleFxRect_ = juce::Rectangle<int>(static_cast<int>(cardFx.getRight() - 36), static_cast<int>(cardFx.getY() + 8), 28, 15);
    const bool anyFxActive = params.delayEnabled || (params.reverbMix > 0.01f);
    drawToggleSwitch(g, toggleFxRect_, anyFxActive, juce::Colour(0xffc084fc));

    const float slotW = (cardFx.getWidth() - 24.0f) * 0.5f;
    const juce::Rectangle<float> delaySlotR(cardFx.getX() + 8.0f, cardFx.getY() + 30.0f, slotW, H - 38.0f);
    const juce::Rectangle<float> reverbSlotR(delaySlotR.getRight() + 8.0f, cardFx.getY() + 30.0f, slotW, H - 38.0f);
    delaySlotRect_  = delaySlotR.toNearestInt();
    reverbSlotRect_ = reverbSlotR.toNearestInt();

    g.setColour(juce::Colour(0xff161226));
    g.fillRoundedRectangle(delaySlotR, 6.0f);
    g.setColour(params.delayEnabled ? juce::Colour(0xffc084fc).withAlpha(0.75f) : theme_.borderSubtle);
    g.drawRoundedRectangle(delaySlotR, 6.0f, 1.0f);

    g.setFont(theme_.getSectionFont(12.5f));
    g.setColour(params.delayEnabled ? juce::Colour(0xffc084fc) : theme_.textMuted);
    g.drawText("DELAY", static_cast<int>(delaySlotR.getX() + 8), static_cast<int>(delaySlotR.getY() + 6), 50, 16, juce::Justification::centredLeft);

    delayToggleRect_ = juce::Rectangle<int>(static_cast<int>(delaySlotR.getRight() - 24), static_cast<int>(delaySlotR.getY() + 8), 18, 11);
    drawToggleSwitch(g, delayToggleRect_, params.delayEnabled, juce::Colour(0xffc084fc));

    delayInPin_  = { delaySlotR.getX(), commonFxY };
    delayOutPin_ = { delaySlotR.getRight(), commonFxY };
    drawJackPin(g, delayInPin_, juce::Colour(0xff38bdf8), true, false);
    drawJackPin(g, delayOutPin_, juce::Colour(0xffc084fc), params.delayEnabled, false);

    g.setFont(theme_.getValueFont(10.5f));
    g.setColour(params.delayEnabled ? theme_.textPrimary : theme_.textMuted);
    g.drawText("Time: " + juce::String(static_cast<int>(params.delayTimeSec * 1000.0f)) + " ms", static_cast<int>(delaySlotR.getX() + 8), static_cast<int>(delaySlotR.getY() + 52), static_cast<int>(slotW - 16), 14, juce::Justification::centredLeft);
    g.setColour(params.delayEnabled ? theme_.textSecondary : theme_.textMuted);
    g.drawText("Feedback: " + juce::String(static_cast<int>(params.delayFeedback * 100.0f)) + "%", static_cast<int>(delaySlotR.getX() + 8), static_cast<int>(delaySlotR.getY() + 70), static_cast<int>(slotW - 16), 14, juce::Justification::centredLeft);
    g.drawText("Mix: " + juce::String(static_cast<int>(params.delayMix * 100.0f)) + "%", static_cast<int>(delaySlotR.getX() + 8), static_cast<int>(delaySlotR.getY() + 88), static_cast<int>(slotW - 16), 14, juce::Justification::centredLeft);
    g.drawText("Ping-Pong", static_cast<int>(delaySlotR.getX() + 8), static_cast<int>(delaySlotR.getBottom() - 20), static_cast<int>(slotW - 16), 14, juce::Justification::centredLeft);

    const bool revOn = (params.reverbMix > 0.01f);
    g.setColour(juce::Colour(0xff201026));
    g.fillRoundedRectangle(reverbSlotR, 6.0f);
    g.setColour(revOn ? juce::Colour(0xffe879f9).withAlpha(0.75f) : theme_.borderSubtle);
    g.drawRoundedRectangle(reverbSlotR, 6.0f, 1.0f);

    g.setFont(theme_.getSectionFont(12.5f));
    g.setColour(revOn ? juce::Colour(0xffe879f9) : theme_.textMuted);
    g.drawText("REVERB", static_cast<int>(reverbSlotR.getX() + 8), static_cast<int>(reverbSlotR.getY() + 6), 55, 16, juce::Justification::centredLeft);

    reverbToggleRect_ = juce::Rectangle<int>(static_cast<int>(reverbSlotR.getRight() - 24), static_cast<int>(reverbSlotR.getY() + 8), 18, 11);
    drawToggleSwitch(g, reverbToggleRect_, revOn, juce::Colour(0xffe879f9));

    reverbInPin_  = { reverbSlotR.getX(), commonFxY };
    reverbOutPin_ = { reverbSlotR.getRight(), commonFxY };
    drawJackPin(g, reverbInPin_, juce::Colour(0xffc084fc), params.delayEnabled, false);
    drawJackPin(g, reverbOutPin_, juce::Colour(0xffe879f9), revOn, false);

    g.setFont(theme_.getValueFont(10.5f));
    g.setColour(revOn ? theme_.textPrimary : theme_.textMuted);
    g.drawText("Decay: " + juce::String(params.reverbDecay, 1) + " s", static_cast<int>(reverbSlotR.getX() + 8), static_cast<int>(reverbSlotR.getY() + 52), static_cast<int>(slotW - 16), 14, juce::Justification::centredLeft);
    g.setColour(revOn ? theme_.textSecondary : theme_.textMuted);
    g.drawText("Shimmer: " + juce::String(static_cast<int>(params.reverbShimmer * 100.0f)) + "%", static_cast<int>(reverbSlotR.getX() + 8), static_cast<int>(reverbSlotR.getY() + 70), static_cast<int>(slotW - 16), 14, juce::Justification::centredLeft);
    g.drawText("Mix: " + juce::String(static_cast<int>(params.reverbMix * 100.0f)) + "%", static_cast<int>(reverbSlotR.getX() + 8), static_cast<int>(reverbSlotR.getY() + 88), static_cast<int>(slotW - 16), 14, juce::Justification::centredLeft);
    g.drawText("Lush Cloud", static_cast<int>(reverbSlotR.getX() + 8), static_cast<int>(reverbSlotR.getBottom() - 20), static_cast<int>(slotW - 16), 14, juce::Justification::centredLeft);
}

void SignalFlowPanel::drawOutputCard(juce::Graphics& g, const juce::Rectangle<float>& cardOutput, float commonFxY) {
    const auto params = proc_.getCurrentParameters();
    const auto tel = proc_.getSynthTelemetry();

    g.setColour(juce::Colour(0xff10151d));
    g.fillRoundedRectangle(cardOutput, 8.0f);
    g.setColour(juce::Colour(0xff334155).withAlpha(0.7f));
    g.drawRoundedRectangle(cardOutput, 8.0f, 1.0f);

    g.setFont(theme_.getTitleFont(15.0f));
    g.setColour(juce::Colour(0xff94a3b8));
    g.drawText("OUTPUT", static_cast<int>(cardOutput.getX() + 10), static_cast<int>(cardOutput.getY() + 7), 80, 18, juce::Justification::centredLeft);

    outSummingJack_ = { cardOutput.getX() + 14.0f, commonFxY };
    drawJackPin(g, outSummingJack_, juce::Colour(0xff38bdf8), true, false);
    g.setFont(theme_.getValueFont(10.0f));
    g.setColour(theme_.textMuted);
    g.drawText("BUS", static_cast<int>(outSummingJack_.x - 14.0f), static_cast<int>(outSummingJack_.y + 8.0f), 28, 12, juce::Justification::centred);

    const float faderX = cardOutput.getX() + 50.0f;
    const float faderY = cardOutput.getY() + 32.0f;
    const float faderH = 78.0f;
    masterFaderTrackRect_ = juce::Rectangle<int>(static_cast<int>(faderX - 3.0f), static_cast<int>(faderY), 6, static_cast<int>(faderH));

    g.setColour(juce::Colour(0xff090d14));
    g.fillRoundedRectangle(masterFaderTrackRect_.toFloat(), 3.0f);
    g.setColour(juce::Colour(0xff334155));
    g.drawRoundedRectangle(masterFaderTrackRect_.toFloat(), 3.0f, 1.0f);

    const float normGain = std::clamp((params.masterGainDb - (-24.0f)) / 36.0f, 0.0f, 1.0f);
    const float thumbY = faderY + (1.0f - normGain) * faderH;
    masterFaderThumbRect_ = juce::Rectangle<int>(static_cast<int>(faderX - 12.0f), static_cast<int>(thumbY - 6.0f), 24, 12);

    g.setColour(juce::Colour(0xff475569));
    g.fillRoundedRectangle(masterFaderThumbRect_.toFloat(), 3.0f);
    g.setColour(juce::Colour(0xff94a3b8));
    g.drawRoundedRectangle(masterFaderThumbRect_.toFloat(), 3.0f, 1.0f);
    g.setColour(juce::Colour(0xff38bdf8));
    g.fillRect(static_cast<float>(masterFaderThumbRect_.getX() + 3), thumbY - 0.5f, static_cast<float>(masterFaderThumbRect_.getWidth() - 6), 1.5f);

    g.setFont(theme_.getValueFont(11.0f));
    g.setColour(theme_.textPrimary);
    g.drawText(juce::String(params.masterGainDb, 1) + " dB", static_cast<int>(faderX - 24.0f), static_cast<int>(faderY + faderH + 4.0f), 48, 14, juce::Justification::centred);
    g.setFont(theme_.getValueFont(10.5f));
    g.setColour(theme_.textSecondary);
    g.drawText("Master", static_cast<int>(faderX - 20.0f), static_cast<int>(faderY + faderH + 18.0f), 40, 14, juce::Justification::centred);

    const float meterX = cardOutput.getX() + 90.0f;
    const float meterY = cardOutput.getY() + 32.0f;
    const float meterH = 78.0f;
    const float barW = 5.0f;

    const auto drawMeter = [&](float mx, float val) {
        const juce::Rectangle<float> bg(mx, meterY, barW, meterH);
        g.setColour(juce::Colour(0xff090d14));
        g.fillRoundedRectangle(bg, 2.0f);
        g.setColour(juce::Colour(0xff1e293b));
        g.drawRoundedRectangle(bg, 2.0f, 0.8f);

        const float norm = std::clamp(val, 0.0f, 1.0f);
        const float fillH = norm * meterH;
        const juce::Rectangle<float> fill(mx, meterY + (meterH - fillH), barW, fillH);
        const auto mCol = norm > 0.85f ? juce::Colour(0xfff43f5e) : (norm > 0.65f ? juce::Colour(0xfff59e0b) : juce::Colour(0xff2dd4bf));
        g.setColour(mCol);
        g.fillRoundedRectangle(fill, 2.0f);
    };
    drawMeter(meterX, tel.outputPeakL);
    drawMeter(meterX + 8.0f, tel.outputPeakR);

    g.setFont(theme_.getValueFont(10.5f));
    g.setColour(theme_.textSecondary);
    g.drawText("Stereo", static_cast<int>(meterX - 8.0f), static_cast<int>(meterY + meterH + 4.0f), 34, 14, juce::Justification::centred);

    const juce::Rectangle<int> clipBadge(static_cast<int>(cardOutput.getCentreX() - 25.0f), static_cast<int>(cardOutput.getBottom() - 24.0f), 50, 16);
    g.setColour(juce::Colour(0xff22c55e).withAlpha(0.16f));
    g.fillRoundedRectangle(clipBadge.toFloat(), 3.0f);
    g.setColour(juce::Colour(0xff22c55e).withAlpha(0.7f));
    g.drawRoundedRectangle(clipBadge.toFloat(), 3.0f, 1.0f);
    g.setColour(juce::Colour(0xff22c55e));
    g.fillEllipse(static_cast<float>(clipBadge.getX() + 5), static_cast<float>(clipBadge.getCentreY()) - 2.5f, 5.0f, 5.0f);
    g.setFont(theme_.getValueFont(10.0f));
    g.drawText("CLIP", clipBadge.withTrimmedLeft(12), juce::Justification::centred);
}

void SignalFlowPanel::drawSignalFlowView(juce::Graphics& g, juce::Rectangle<float> area) {
    flowOscPillRects_.clear();
    flowOscRingRects_.clear();
    flowOscJackRects_.clear();
    oscOutPins_.clear();

    const float gap = 8.0f;
    const float wOutput = 126.0f;
    const float availW = area.getWidth() - wOutput - gap * 4.0f;
    const float wSources = std::round(availW * 0.23f);
    const float wFilters = std::round(availW * 0.28f);
    const float wAmp     = std::round(availW * 0.24f);
    const float wFx      = availW - (wSources + wFilters + wAmp);
    const float H = area.getHeight();

    const juce::Rectangle<float> cardSources(area.getX(), area.getY(), wSources, H);
    const juce::Rectangle<float> cardFilters(cardSources.getRight() + gap, area.getY(), wFilters, H);
    const juce::Rectangle<float> cardAmp    (cardFilters.getRight() + gap, area.getY(), wAmp, H);
    const juce::Rectangle<float> cardFx     (cardAmp.getRight() + gap, area.getY(), wFx, H);
    const juce::Rectangle<float> cardOutput (cardFx.getRight() + gap, area.getY(), wOutput, H);

    cardSourcesRect_ = cardSources.toNearestInt();
    cardAmpRect_     = cardAmp.toNearestInt();
    const float commonFxY = cardFx.getY() + 66.0f;

    drawSourcesCard(g, cardSources);
    drawFiltersCard(g, cardFilters);
    drawAmpCard(g, cardAmp, commonFxY);
    drawFxCard(g, cardFx, H, commonFxY);
    drawOutputCard(g, cardOutput, commonFxY);
    drawPatchCablesStage(g);
}

} // namespace nova::synth
