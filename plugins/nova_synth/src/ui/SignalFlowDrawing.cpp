#include "SignalFlowPanel.h"
#include "../NovaSynthProcessor.h"
#include "NovaSynthUiHelpers.h"
#include <cmath>
#include <algorithm>

namespace nova::synth {

void SignalFlowPanel::drawToggleSwitch(juce::Graphics& g, const juce::Rectangle<int>& rect, bool isOn, juce::Colour onColor) {
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
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawEllipse(thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f, 0.8f);
}

void SignalFlowPanel::drawJackPin(juce::Graphics& g, juce::Point<float> pt, juce::Colour col, bool isConnected, bool isHovered) {
    if (isHovered) {
        g.setColour(col.withAlpha(0.55f));
        g.fillEllipse(pt.x - 8.0f, pt.y - 8.0f, 16.0f, 16.0f);
        g.setColour(col);
        g.drawEllipse(pt.x - 9.0f, pt.y - 9.0f, 18.0f, 18.0f, 1.4f);
    } else if (isConnected) {
        g.setColour(col.withAlpha(0.30f));
        g.fillEllipse(pt.x - 6.5f, pt.y - 6.5f, 13.0f, 13.0f);
    }

    g.setColour(juce::Colour(0xff334155));
    g.fillEllipse(pt.x - 5.5f, pt.y - 5.5f, 11.0f, 11.0f);
    g.setColour(juce::Colour(0xff64748b));
    g.drawEllipse(pt.x - 5.5f, pt.y - 5.5f, 11.0f, 11.0f, 1.0f);

    g.setColour(juce::Colour(0xff090d14));
    g.fillEllipse(pt.x - 2.5f, pt.y - 2.5f, 5.0f, 5.0f);

    if (isConnected) {
        g.setColour(col);
        g.drawEllipse(pt.x - 2.5f, pt.y - 2.5f, 5.0f, 5.0f, 1.2f);
        g.fillEllipse(pt.x - 1.2f, pt.y - 1.2f, 2.4f, 2.4f);
    }
}

void SignalFlowPanel::drawPatchCable(juce::Graphics& g, juce::Point<float> p1, juce::Point<float> p2,
                                     juce::Colour col, bool isBypass, float bypassY, bool isDimmed) {
    juce::Path p;
    p.startNewSubPath(p1);

    if (isBypass) {
        const float dropX1 = p1.x + 24.0f;
        const float dropX2 = p1.x + 40.0f;
        p.cubicTo(dropX1, p1.y, dropX1, bypassY, dropX2, bypassY);
        const float riseX1 = p2.x - 30.0f;
        const float riseX2 = p2.x - 12.0f;
        p.lineTo(riseX1, bypassY);
        p.cubicTo(riseX2, bypassY, riseX2, p2.y, p2.x, p2.y);
    } else {
        const float dx = std::max(20.0f, std::abs(p2.x - p1.x) * 0.48f);
        p.cubicTo(p1.x + dx, p1.y, p2.x - dx, p2.y, p2.x, p2.y);
    }

    if (isDimmed) {
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        juce::Path shadow = p;
        shadow.applyTransform(juce::AffineTransform::translation(0.0f, 1.5f));
        g.strokePath(shadow, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(juce::Colour(0xff475569).withAlpha(0.65f));
        g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }

    // 1. Drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    juce::Path shadow = p;
    shadow.applyTransform(juce::AffineTransform::translation(0.0f, 2.0f));
    g.strokePath(shadow, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 2. Outer atmospheric glow
    g.setColour(col.withAlpha(0.24f));
    g.strokePath(p, juce::PathStrokeType(5.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 3. Core solid cable
    g.setColour(col);
    g.strokePath(p, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 4. Subtle top light specular highlight
    g.setColour(col.brighter(0.5f).withAlpha(0.75f));
    g.strokePath(p, juce::PathStrokeType(0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void SignalFlowPanel::drawSourcesCard(juce::Graphics& g, const juce::Rectangle<float>& cardSources) {
    const auto params = proc_.getCurrentParameters();
    g.setColour(juce::Colour(0xff0d1522));
    g.fillRoundedRectangle(cardSources, 8.0f);
    g.setColour(juce::Colour(0xff1e3a5f).withAlpha(0.7f));
    g.drawRoundedRectangle(cardSources, 8.0f, 1.0f);

    g.setFont(theme_.getTitleFont(15.0f));
    g.setColour(juce::Colour(0xff38bdf8));
    g.drawText("SOURCES", static_cast<int>(cardSources.getX() + 10), static_cast<int>(cardSources.getY() + 7), 100, 18, juce::Justification::centredLeft);

    toggleSourcesRect_ = juce::Rectangle<int>(static_cast<int>(cardSources.getRight() - 36), static_cast<int>(cardSources.getY() + 8), 28, 15);
    bool anySourcesOn = false;
    for (int i = 0; i < std::clamp(params.activeOscCount, 1, 8); ++i) {
        if (params.oscSlots[static_cast<std::size_t>(i)].enabled) anySourcesOn = true;
    }
    drawToggleSwitch(g, toggleSourcesRect_, anySourcesOn, juce::Colour(0xff38bdf8));

    constexpr int totalSources = 8;
    constexpr int visibleCount = 6;
    constexpr int maxScroll = totalSources - visibleCount;
    sourceScrollOffset_ = std::clamp(sourceScrollOffset_, 0, maxScroll);

    const float scrollBarW = 10.0f;
    const float pillW = cardSources.getWidth() - 20.0f - scrollBarW;
    const float pillH = 23.0f;
    const float pillGap = 3.5f;

    for (int v = 0; v < visibleCount; ++v) {
        const int i = v + sourceScrollOffset_;
        const float py = cardSources.getY() + 32.0f + static_cast<float>(v) * (pillH + pillGap);
        const juce::Rectangle<float> pillR(cardSources.getX() + 10.0f, py, pillW, pillH);
        flowOscPillRects_.push_back(pillR.toNearestInt());

        const auto col = getLayerColour(i);
        const bool isEnabled = params.oscSlots[static_cast<std::size_t>(i)].enabled;
        const bool isSel = (selectedOscIndex_ == i);

        g.setColour(juce::Colour(0xff131c2a));
        g.fillRoundedRectangle(pillR, 5.0f);

        if (isSel) {
            g.setColour(juce::Colour(0xff38bdf8));
            g.drawRoundedRectangle(pillR, 5.0f, 1.8f);
        } else {
            g.setColour(isEnabled ? col.withAlpha(0.5f) : juce::Colour(0xff1e293b));
            g.drawRoundedRectangle(pillR, 5.0f, 1.0f);
        }

        const juce::Rectangle<int> ringR(static_cast<int>(pillR.getX() + 5), static_cast<int>(pillR.getCentreY() - 6.0f), 12, 12);
        flowOscRingRects_.push_back(ringR);
        g.setColour(isEnabled ? col : juce::Colour(0xff475569));
        g.drawEllipse(ringR.toFloat().reduced(0.8f), 1.5f);
        if (isEnabled) {
            g.fillEllipse(ringR.toFloat().reduced(3.8f));
        }

        g.setFont(theme_.getSectionFont(12.5f));
        g.setColour(isEnabled ? (isSel ? juce::Colours::white : col) : theme_.textMuted);
        g.drawText(getLayerShortName(params, i), static_cast<int>(pillR.getX() + 22), static_cast<int>(pillR.getY()), 65, static_cast<int>(pillR.getHeight()), juce::Justification::centredLeft);

        g.setFont(theme_.getValueFont(10.5f));
        g.setColour(theme_.textMuted);
        g.drawText(getEngineDisplayName(params, i), static_cast<int>(pillR.getX() + 84), static_cast<int>(pillR.getY()), static_cast<int>(pillR.getWidth() - 98), static_cast<int>(pillR.getHeight()), juce::Justification::centredLeft);

        const juce::Point<float> outPin(pillR.getRight() - 2.0f, pillR.getCentreY());
        oscOutPins_.push_back(outPin);
        const juce::Rectangle<int> jackHit(static_cast<int>(outPin.x - 7.0f), static_cast<int>(outPin.y - 7.0f), 14, 14);
        flowOscJackRects_.push_back(jackHit);

        const bool isDraggingThis = (isDraggingCable_ && dragSourceOscIndex_ == i);
        drawJackPin(g, outPin, col, isEnabled, isDraggingThis);
    }

    const float trackX = cardSources.getRight() - 14.0f;
    const float trackY = cardSources.getY() + 32.0f;
    const float trackH = static_cast<float>(visibleCount) * (pillH + pillGap) - pillGap;
    scrollTrackRect_ = juce::Rectangle<int>(static_cast<int>(trackX - 2.0f), static_cast<int>(trackY), 9, static_cast<int>(trackH));

    g.setColour(juce::Colour(0xff1e293b).withAlpha(0.6f));
    g.fillRoundedRectangle(trackX, trackY, 5.0f, trackH, 2.5f);

    const float scrollThumbH = trackH * (static_cast<float>(visibleCount) / static_cast<float>(totalSources));
    const float scrollMaxTravel = trackH - scrollThumbH;
    const float scrollThumbY = trackY + (static_cast<float>(sourceScrollOffset_) / static_cast<float>(maxScroll)) * scrollMaxTravel;
    g.setColour(theme_.accentPrimary.withAlpha(0.85f));
    g.fillRoundedRectangle(trackX, scrollThumbY, 5.0f, scrollThumbH, 2.5f);

    scrollUpRect_   = juce::Rectangle<int>(static_cast<int>(trackX - 4.0f), static_cast<int>(trackY - 14.0f), 13, 11);
    scrollDownRect_ = juce::Rectangle<int>(static_cast<int>(trackX - 4.0f), static_cast<int>(trackY + trackH + 3.0f), 13, 11);

    if (sourceScrollOffset_ > 0) {
        g.setColour(juce::Colour(0xff38bdf8));
        juce::Path arr;
        arr.startNewSubPath(static_cast<float>(scrollUpRect_.getX()) + 2.0f, static_cast<float>(scrollUpRect_.getBottom()));
        arr.lineTo(static_cast<float>(scrollUpRect_.getCentreX()), static_cast<float>(scrollUpRect_.getY()) + 2.0f);
        arr.lineTo(static_cast<float>(scrollUpRect_.getRight()) - 2.0f, static_cast<float>(scrollUpRect_.getBottom()));
        g.strokePath(arr, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    } else {
        g.setColour(theme_.textMuted.withAlpha(0.25f));
        juce::Path arr;
        arr.startNewSubPath(static_cast<float>(scrollUpRect_.getX()) + 2.0f, static_cast<float>(scrollUpRect_.getBottom()));
        arr.lineTo(static_cast<float>(scrollUpRect_.getCentreX()), static_cast<float>(scrollUpRect_.getY()) + 2.0f);
        arr.lineTo(static_cast<float>(scrollUpRect_.getRight()) - 2.0f, static_cast<float>(scrollUpRect_.getBottom()));
        g.strokePath(arr, juce::PathStrokeType(1.0f));
    }

    if (sourceScrollOffset_ < maxScroll) {
        g.setColour(juce::Colour(0xff38bdf8));
        juce::Path arr;
        arr.startNewSubPath(static_cast<float>(scrollDownRect_.getX()) + 2.0f, static_cast<float>(scrollDownRect_.getY()));
        arr.lineTo(static_cast<float>(scrollDownRect_.getCentreX()), static_cast<float>(scrollDownRect_.getBottom()) - 2.0f);
        arr.lineTo(static_cast<float>(scrollDownRect_.getRight()) - 2.0f, static_cast<float>(scrollDownRect_.getY()));
        g.strokePath(arr, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    } else {
        g.setColour(theme_.textMuted.withAlpha(0.25f));
        juce::Path arr;
        arr.startNewSubPath(static_cast<float>(scrollDownRect_.getX()) + 2.0f, static_cast<float>(scrollDownRect_.getY()));
        arr.lineTo(static_cast<float>(scrollDownRect_.getCentreX()), static_cast<float>(scrollDownRect_.getBottom()) - 2.0f);
        arr.lineTo(static_cast<float>(scrollDownRect_.getRight()) - 2.0f, static_cast<float>(scrollDownRect_.getY()));
        g.strokePath(arr, juce::PathStrokeType(1.0f));
    }
}

void SignalFlowPanel::drawFiltersCard(juce::Graphics& g, const juce::Rectangle<float>& cardFilters) {
    const auto params = proc_.getCurrentParameters();
    g.setColour(juce::Colour(0xff091a20));
    g.fillRoundedRectangle(cardFilters, 8.0f);
    g.setColour(juce::Colour(0xff134e4a).withAlpha(0.7f));
    g.drawRoundedRectangle(cardFilters, 8.0f, 1.0f);

    g.setFont(theme_.getTitleFont(15.0f));
    g.setColour(juce::Colour(0xff2dd4bf));
    g.drawText("FILTERS", static_cast<int>(cardFilters.getX() + 10), static_cast<int>(cardFilters.getY() + 7), 80, 18, juce::Justification::centredLeft);

    const juce::Rectangle<float> fltModBadge(cardFilters.getX() + 84.0f, cardFilters.getY() + 7.5f, 86.0f, 17.0f);
    g.setColour(juce::Colour(0xff134e4a).withAlpha(0.5f));
    g.fillRoundedRectangle(fltModBadge, 4.0f);
    g.setColour(juce::Colour(0xff2dd4bf).withAlpha(0.6f));
    g.drawRoundedRectangle(fltModBadge, 4.0f, 1.0f);
    g.setFont(theme_.getValueFont(10.5f));
    g.setColour(juce::Colour(0xff2dd4bf));
    g.drawText("MOD: ENV 2", fltModBadge, juce::Justification::centred);

    toggleFiltersRect_ = juce::Rectangle<int>(static_cast<int>(cardFilters.getRight() - 36), static_cast<int>(cardFilters.getY() + 8), 28, 15);
    const bool filtersOn = params.filter1Enabled || params.filter2Enabled;
    drawToggleSwitch(g, toggleFiltersRect_, filtersOn, juce::Colour(0xfff59e0b));

    const float fltW = cardFilters.getWidth() - 20.0f;
    const float fltH = 34.0f;
    const juce::Rectangle<float> flt1R(cardFilters.getX() + 10.0f, cardFilters.getY() + 32.0f, fltW, fltH);
    flt1PillRect_ = flt1R.toNearestInt();
    g.setColour(juce::Colour(0xff10242b));
    g.fillRoundedRectangle(flt1R, 6.0f);
    const bool flt1Hov = (isDraggingCable_ && (hoveredTarget_ == 0 || hoveredTarget_ == 2));
    g.setColour(flt1Hov ? juce::Colour(0xff2dd4bf) : (params.filter1Enabled ? juce::Colour(0xff2dd4bf).withAlpha(0.7f) : theme_.borderSubtle));
    g.drawRoundedRectangle(flt1R, 6.0f, flt1Hov ? 2.0f : 1.0f);

    flt1ToggleRect_ = juce::Rectangle<int>(static_cast<int>(flt1R.getRight() - 36), static_cast<int>(flt1R.getCentreY() - 6.5f), 22, 13);
    drawToggleSwitch(g, flt1ToggleRect_, params.filter1Enabled, juce::Colour(0xff2dd4bf));

    flt1InPin_  = { flt1R.getX() + 4.0f, flt1R.getCentreY() };
    drawJackPin(g, flt1InPin_, juce::Colour(0xff2dd4bf), params.filter1Enabled, flt1Hov);

    g.setFont(theme_.getSectionFont(13.0f));
    g.setColour(params.filter1Enabled ? juce::Colour(0xff2dd4bf) : theme_.textMuted);
    g.drawText("FLT 1", static_cast<int>(flt1R.getX() + 18), static_cast<int>(flt1R.getY() + 2), 45, 16, juce::Justification::centredLeft);
    g.setFont(theme_.getValueFont(11.5f));
    g.setColour(theme_.textSecondary);
    g.drawText(juce::String(static_cast<int>(params.filterCutoffHz)) + " Hz", static_cast<int>(flt1R.getX() + 18), static_cast<int>(flt1R.getY() + 17), 75, 14, juce::Justification::centredLeft);

    flt1OutPin_ = { flt1R.getRight() - 6.0f, flt1R.getCentreY() };
    drawJackPin(g, flt1OutPin_, juce::Colour(0xff2dd4bf), params.filter1Enabled, false);

    dualTargetRect_ = juce::Rectangle<int>(static_cast<int>(cardFilters.getX() + 12), static_cast<int>(flt1R.getBottom() + 3), static_cast<int>(cardFilters.getWidth() - 24), 20);
    const bool dualHov = (isDraggingCable_ && hoveredTarget_ == 2);
    if (dualHov) {
        g.setColour(juce::Colour(0xff2dd4bf).withAlpha(0.25f));
        g.fillRoundedRectangle(dualTargetRect_.toFloat(), 4.0f);
        g.setColour(juce::Colour(0xff2dd4bf));
        g.drawRoundedRectangle(dualTargetRect_.toFloat(), 4.0f, 1.4f);
    } else {
        g.setColour(juce::Colour(0xff134e4a).withAlpha(0.4f));
        g.drawRoundedRectangle(dualTargetRect_.toFloat(), 3.0f, 0.8f);
    }
    g.setFont(theme_.getValueFont(11.0f));
    g.setColour(dualHov ? juce::Colour(0xff2dd4bf) : theme_.textSecondary);
    const bool isParallel = (params.filterRoutingMode == 1);
    const juce::String routingLabel = dualHov ? "CONNECT DUAL (FLT 1 + 2)" : (isParallel ? "PARALLEL (1 + 2)" : "SERIAL (1 -> 2)");
    g.drawText(routingLabel, dualTargetRect_, juce::Justification::centred);

    const juce::Rectangle<float> flt2R(cardFilters.getX() + 10.0f, static_cast<float>(dualTargetRect_.getBottom() + 3), fltW, fltH);
    flt2PillRect_ = flt2R.toNearestInt();
    g.setColour(juce::Colour(0xff10242b));
    g.fillRoundedRectangle(flt2R, 6.0f);
    const bool flt2Hov = (isDraggingCable_ && (hoveredTarget_ == 1 || hoveredTarget_ == 2));
    g.setColour(flt2Hov ? juce::Colour(0xff2dd4bf) : (params.filter2Enabled ? juce::Colour(0xff2dd4bf).withAlpha(0.7f) : theme_.borderSubtle));
    g.drawRoundedRectangle(flt2R, 6.0f, flt2Hov ? 2.0f : 1.0f);

    flt2ToggleRect_ = juce::Rectangle<int>(static_cast<int>(flt2R.getRight() - 36), static_cast<int>(flt2R.getCentreY() - 6.5f), 22, 13);
    drawToggleSwitch(g, flt2ToggleRect_, params.filter2Enabled, juce::Colour(0xff2dd4bf));

    flt2InPin_  = { flt2R.getX() + 4.0f, flt2R.getCentreY() };
    drawJackPin(g, flt2InPin_, juce::Colour(0xff2dd4bf), params.filter2Enabled, flt2Hov);

    g.setFont(theme_.getSectionFont(13.0f));
    g.setColour(params.filter2Enabled ? juce::Colour(0xff2dd4bf) : theme_.textMuted);
    g.drawText("FLT 2", static_cast<int>(flt2R.getX() + 18), static_cast<int>(flt2R.getY() + 2), 45, 16, juce::Justification::centredLeft);
    g.setFont(theme_.getValueFont(11.5f));
    g.setColour(theme_.textSecondary);
    g.drawText("Carve: " + juce::String(static_cast<int>(params.vocalCarve * 100.0f)) + "%", static_cast<int>(flt2R.getX() + 18), static_cast<int>(flt2R.getY() + 17), 80, 14, juce::Justification::centredLeft);

    flt2OutPin_ = { flt2R.getRight() - 6.0f, flt2R.getCentreY() };
    drawJackPin(g, flt2OutPin_, juce::Colour(0xff2dd4bf), params.filter2Enabled, false);

    directGutterY_ = cardFilters.getBottom() - 16.0f;
    directGutterRect_ = juce::Rectangle<int>(static_cast<int>(cardFilters.getX() + 8), static_cast<int>(directGutterY_ - 11.0f), static_cast<int>(cardFilters.getWidth() - 16), 22);
    const bool gutterHov = (isDraggingCable_ && hoveredTarget_ == 3);

    if (gutterHov) {
        g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.2f));
        g.fillRoundedRectangle(directGutterRect_.toFloat(), 4.0f);
        g.setColour(juce::Colour(0xff38bdf8));
        g.drawRoundedRectangle(directGutterRect_.toFloat(), 4.0f, 1.4f);
    } else {
        g.setColour(juce::Colour(0xff334155).withAlpha(0.35f));
        g.drawDashedLine(juce::Line<float>(cardFilters.getX() + 14.0f, directGutterY_, cardFilters.getRight() - 14.0f, directGutterY_), nullptr, 0, 1.0f);
    }
    g.setFont(theme_.getValueFont(11.0f));
    g.setColour(gutterHov ? juce::Colour(0xff38bdf8) : theme_.textMuted);
    g.drawText("DIRECT (BYPASS FILTERS)", directGutterRect_, juce::Justification::centred);
}

void SignalFlowPanel::drawPatchCablesStage(juce::Graphics& g) {
    const auto params = proc_.getCurrentParameters();
    const bool revOn = (params.reverbMix > 0.01f);
    constexpr int visibleCount = 6;

    for (int v = 0; v < visibleCount; ++v) {
        const int i = v + sourceScrollOffset_;
        if (static_cast<std::size_t>(v) >= oscOutPins_.size()) break;
        const auto srcPt = oscOutPins_[static_cast<std::size_t>(v)];
        const auto col = getLayerColour(i);
        const bool isEnabled = params.oscSlots[static_cast<std::size_t>(i)].enabled;
        const int routing = params.oscSlots[static_cast<std::size_t>(i)].filterRouting;

        if (isDraggingCable_ && dragSourceOscIndex_ == i) {
            drawPatchCable(g, srcPt, dragCurrentPos_, col, false);
            g.setColour(col);
            g.fillEllipse(dragCurrentPos_.x - 4.5f, dragCurrentPos_.y - 4.5f, 9.0f, 9.0f);
            g.setColour(juce::Colours::white);
            g.drawEllipse(dragCurrentPos_.x - 5.5f, dragCurrentPos_.y - 5.5f, 11.0f, 11.0f, 1.4f);
        } else if (isEnabled) {
            if (routing == 0) {
                drawPatchCable(g, srcPt, flt1InPin_, col, false);
            } else if (routing == 1) {
                drawPatchCable(g, srcPt, flt2InPin_, col, false);
            } else if (routing == 2) {
                drawPatchCable(g, srcPt, flt1InPin_, col, false);
                drawPatchCable(g, srcPt, flt2InPin_, col, false);
            } else {
                drawPatchCable(g, srcPt, ampDirectInPin_, col, true, directGutterY_);
            }
        }
    }

    drawPatchCable(g, flt1OutPin_, ampFltInPin_, juce::Colour(0xff2dd4bf), false, 0.0f, !params.filter1Enabled);
    drawPatchCable(g, flt2OutPin_, ampFltInPin_, juce::Colour(0xff2dd4bf), false, 0.0f, !params.filter2Enabled);
    drawPatchCable(g, ampOutPin_, delayInPin_, juce::Colour(0xff38bdf8), false, 0.0f, false);
    drawPatchCable(g, delayOutPin_, reverbInPin_, juce::Colour(0xffc084fc), false, 0.0f, !params.delayEnabled);
    drawPatchCable(g, reverbOutPin_, outSummingJack_, juce::Colour(0xffe879f9), false, 0.0f, !revOn);
}

} // namespace nova::synth
