#include "ModulationHubPanel.h"
#include <algorithm>
#include <cmath>

namespace nova::synth::ui {

void ModulationHubPanel::paint(juce::Graphics& g) {
    drawLfoCard(g, cardLfoRect_.toFloat());
    drawMacrosCard(g, cardMacrosRect_.toFloat());
    drawMatrixCard(g, cardMatrixRect_.toFloat());
}

void ModulationHubPanel::drawLfoCard(juce::Graphics& g, juce::Rectangle<float> area) {
    g.setColour(theme_.bgSurface.darker(0.12f));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    // Header Title
    g.setFont(theme_.getSectionFont(12.0f));
    g.setColour(theme_.accentPrimary);
    g.drawText("LFO & ANALOG DRIFT", juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 180.0f, 16.0f),
               juce::Justification::left);

    // Subtitle badge
    g.setFont(theme_.getLabelFont(10.0f));
    g.setColour(theme_.textMuted);
    g.drawText("4 DUAL-SHAPE GENERATORS", juce::Rectangle<float>(area.getX() + 160.0f, area.getY() + 7.0f, 180.0f, 14.0f),
               juce::Justification::left);

    static constexpr std::array<uint32_t, 4> kLfoColors = {
        0xff2dd4bf, // Cyan (LFO 1)
        0xfff59e0b, // Amber (LFO 2 - Drift)
        0xfff43f5e, // Rose (LFO 3)
        0xffa855f7  // Purple (LFO 4)
    };
    static constexpr std::array<const char*, 4> kLfoNames = {
        "LFO 1", "LFO 2 (DRIFT)", "LFO 3", "LFO 4"
    };

    for (int i = 0; i < 4; ++i) {
        const auto col = juce::Colour(kLfoColors[static_cast<std::size_t>(i)]);
        const auto sRect = lfoShapeRects_[static_cast<std::size_t>(i)].toFloat();

        // Waveform Box Background
        g.setColour(juce::Colour(0xff090d16));
        g.fillRoundedRectangle(sRect, 4.0f);
        g.setColour(col.withAlpha(0.35f));
        g.drawRoundedRectangle(sRect, 4.0f, 1.0f);

        // Fetch Shape from APVTS or Engine
        const juce::String paramId = "lfo" + juce::String(i + 1) + "Shape";
        int shapeInt = 0;
        if (auto* p = proc_.getAPVTS().getRawParameterValue(paramId)) {
            shapeInt = static_cast<int>(p->load());
        }
        const auto shape = static_cast<nova::dsp::LfoGenerator::Shape>(shapeInt);

        // Shape name label at bottom
        const auto labelRect = sRect.withTrimmedTop(sRect.getHeight() - 16.0f);
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.drawText(nova::dsp::LfoGenerator::getShapeName(shape), labelRect, juce::Justification::centred);

        // Wave preview area inside box
        const auto waveArea = sRect.withTrimmedBottom(18.0f).withTrimmedTop(4.0f).reduced(6.0f, 0.0f);
        drawMiniWaveform(g, waveArea, shape, col, i);

        // Top mini title
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(col);
        g.drawText(kLfoNames[static_cast<std::size_t>(i)],
                   juce::Rectangle<float>(sRect.getX(), sRect.getY() - 14.0f, sRect.getWidth(), 14.0f),
                   juce::Justification::centred);

        // Arrow hints < and > vertically centered in waveArea
        g.setFont(theme_.getLabelFont(11.0f));
        g.setColour(theme_.textMuted);
        g.drawText("<", juce::Rectangle<float>(sRect.getX() + 3.0f, waveArea.getCentreY() - 8.0f, 10.0f, 16.0f), juce::Justification::centred);
        g.drawText(">", juce::Rectangle<float>(sRect.getRight() - 13.0f, waveArea.getCentreY() - 8.0f, 10.0f, 16.0f), juce::Justification::centred);

        // Check if LFO is synced
        bool isSynced = false;
        const juce::String syncParamId = "lfo" + juce::String(i + 1) + "Sync";
        if (auto* p = proc_.getAPVTS().getRawParameterValue(syncParamId)) {
            isSynced = p->load() > 0.5f;
        }

        // Draw connecting line between Rate knob dial track and Sync button
        const auto btnR = syncBtnRects_[static_cast<std::size_t>(i)];
        const float lineY = static_cast<float>(btnR.getCentreY());

        const auto* rateComp = lfoKnobs_[static_cast<std::size_t>(i)].rate.get();
        const float rateDialCentreX = rateComp != nullptr
            ? static_cast<float>(rateComp->getX()) + static_cast<float>(rateComp->getWidth()) * 0.5f
            : static_cast<float>(btnR.getX() - 24);
        const float rateDialH = rateComp != nullptr ? static_cast<float>(rateComp->getHeight() - 26) : 32.0f;
        const float rateDialW = rateComp != nullptr ? static_cast<float>(rateComp->getWidth()) : 50.0f;
        const float dialRadius = std::min(rateDialW - 4.0f, rateDialH - 4.0f) * 0.5f;
        const float rateOuterRimX = rateDialCentreX + dialRadius * 0.95f;

        const float lineStartX = rateOuterRimX + 1.5f;
        const float lineEndX   = static_cast<float>(btnR.getX()) - 0.5f;

        if (lineEndX > lineStartX) {
            if (isSynced) {
                g.setColour(col.withAlpha(0.25f));
                g.drawLine(lineStartX, lineY, lineEndX, lineY, 3.2f);
                g.setColour(col.withAlpha(0.95f));
                g.drawLine(lineStartX, lineY, lineEndX, lineY, 1.5f);
                g.drawLine(lineStartX, lineY - 2.5f, lineStartX, lineY + 2.5f, 1.5f);
            } else {
                g.setColour(juce::Colour(0xff475569));
                g.drawLine(lineStartX, lineY, lineEndX, lineY, 1.2f);
                g.drawLine(lineStartX, lineY - 2.0f, lineStartX, lineY + 2.0f, 1.2f);
            }
        }

        const bool isHovered = (hoveredSyncBtn_ == i);
        drawSyncButton(g, btnR.toFloat(), isSynced, col, isHovered);
    }
}

void ModulationHubPanel::drawSyncButton(juce::Graphics& g, juce::Rectangle<float> btnRect, bool isSynced, juce::Colour col, bool isHovered) {
    constexpr float radius = 3.5f;

    if (isSynced) {
        g.setColour(col.withAlpha(0.22f));
        g.fillRoundedRectangle(btnRect.expanded(1.5f), radius + 1.0f);
        const auto grad = juce::ColourGradient::vertical(
            col.withAlpha(0.38f), btnRect.getY(),
            col.withAlpha(0.20f), btnRect.getBottom());
        g.setGradientFill(grad);
        g.fillRoundedRectangle(btnRect, radius);
        g.setColour(col.withAlpha(0.95f));
        g.drawRoundedRectangle(btnRect, radius, 1.3f);
    } else {
        const auto grad = juce::ColourGradient::vertical(
            isHovered ? juce::Colour(0xff273549) : juce::Colour(0xff1e293b), btnRect.getY(),
            isHovered ? juce::Colour(0xff162030) : juce::Colour(0xff0f172a), btnRect.getBottom());
        g.setGradientFill(grad);
        g.fillRoundedRectangle(btnRect, radius);
        g.setColour(isHovered ? theme_.borderFocus : juce::Colour(0xff3b4f68));
        g.drawRoundedRectangle(btnRect, radius, 1.2f);
    }

    const auto iconCol = isSynced ? juce::Colours::white : (isHovered ? juce::Colour(0xfff1f5f9) : juce::Colour(0xffcbd5e1));
    drawSyncIcon(g, btnRect.reduced(2.5f), iconCol);
}

void ModulationHubPanel::drawSyncIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour col) {
    const float s = std::min(area.getWidth(), area.getHeight());
    if (s <= 1.0f) return;

    const float cx = area.getCentreX();
    const float cy = area.getCentreY();
    const float loopCenterX = 0.26f * s;
    const float loopRadius  = 0.16f * s;
    const float legLen      = 0.12f * s;
    const float loopStroke  = std::max(1.3f, 0.13f * s);
    const float barHalfLen  = 0.19f * s;
    const float barStroke   = std::max(1.3f, 0.12f * s);

    const auto strokeType = juce::PathStrokeType(loopStroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    const auto barStrokeType = juce::PathStrokeType(barStroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    const auto xform = juce::AffineTransform::rotation(-juce::MathConstants<float>::pi * 0.25f).translated(cx, cy);

    // 1. Center link bar
    juce::Path bar;
    bar.startNewSubPath(-barHalfLen, 0.0f);
    bar.lineTo(barHalfLen, 0.0f);
    bar.applyTransform(xform);

    // 2. Right loop
    juce::Path rightLoop;
    rightLoop.startNewSubPath(loopCenterX - legLen, loopRadius);
    rightLoop.lineTo(loopCenterX, loopRadius);
    rightLoop.addCentredArc(loopCenterX, 0.0f, loopRadius, loopRadius, 0.0f, juce::MathConstants<float>::pi, 0.0f, false);
    rightLoop.lineTo(loopCenterX - legLen, -loopRadius);
    rightLoop.applyTransform(xform);

    // 3. Left loop
    juce::Path leftLoop;
    leftLoop.startNewSubPath(-loopCenterX + legLen, -loopRadius);
    leftLoop.lineTo(-loopCenterX, -loopRadius);
    leftLoop.addCentredArc(-loopCenterX, 0.0f, loopRadius, loopRadius, 0.0f, 0.0f, -juce::MathConstants<float>::pi, false);
    leftLoop.lineTo(-loopCenterX + legLen, loopRadius);
    leftLoop.applyTransform(xform);

    g.setColour(col);
    g.strokePath(bar, barStrokeType);
    g.strokePath(rightLoop, strokeType);
    g.strokePath(leftLoop, strokeType);
}

void ModulationHubPanel::drawMiniWaveform(juce::Graphics& g, juce::Rectangle<float> area,
                                          nova::dsp::LfoGenerator::Shape shape, juce::Colour col, int lfoIdx) {
    const float midY = area.getCentreY();
    g.setColour(theme_.borderSubtle.withAlpha(0.25f));
    g.drawHorizontalLine(static_cast<int>(std::round(midY)), area.getX(), area.getRight());

    float depth = 0.5f;
    const juce::String depthParam = "lfo" + juce::String(lfoIdx + 1) + "Depth";
    if (auto* pDepth = proc_.getAPVTS().getRawParameterValue(depthParam)) {
        depth = pDepth->load();
    }
    const float h = area.getHeight() * 0.42f * (0.08f + 0.92f * std::clamp(depth, 0.0f, 1.0f));
    const int steps = 40;

    auto evalShape = [shape, midY, h](float t) noexcept -> float {
        switch (shape) {
            case nova::dsp::LfoGenerator::Shape::Sine:
                return midY - std::sin(t * 6.283185f) * h;
            case nova::dsp::LfoGenerator::Shape::Triangle:
                return midY - (1.0f - 4.0f * std::abs(t - 0.5f)) * h;
            case nova::dsp::LfoGenerator::Shape::SawUp:
                return midY - (2.0f * t - 1.0f) * h;
            case nova::dsp::LfoGenerator::Shape::SmoothDrift: {
                const float s1 = std::sin(t * 6.283f);
                const float s2 = std::sin(t * 12.566f + 1.2f) * 0.4f;
                return midY - (s1 + s2) * 0.7f * h;
            }
            case nova::dsp::LfoGenerator::Shape::PulseGate:
                return midY - ((t < 0.5f) ? 1.0f : -1.0f) * h;
        }
        return midY;
    };

    juce::Path p;
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float x = area.getX() + t * area.getWidth();
        const float y = evalShape(t);

        if (i == 0) p.startNewSubPath(x, y);
        else p.lineTo(x, y);
    }

    g.setColour(col.withAlpha(0.85f));
    g.strokePath(p, juce::PathStrokeType(1.5f));

    const float cyclePhase = lfoPhases_[static_cast<std::size_t>(std::clamp(lfoIdx, 0, 3))];
    const float dotX = area.getX() + cyclePhase * area.getWidth();
    const float dotY = evalShape(cyclePhase);

    g.setColour(col.withAlpha(0.40f));
    g.fillEllipse(dotX - 5.0f, dotY - 5.0f, 10.0f, 10.0f);
    g.setColour(juce::Colours::white);
    g.fillEllipse(dotX - 2.5f, dotY - 2.5f, 5.0f, 5.0f);
}

void ModulationHubPanel::drawMacrosCard(juce::Graphics& g, juce::Rectangle<float> area) {
    g.setColour(theme_.bgSurface.darker(0.12f));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    g.setFont(theme_.getSectionFont(12.0f));
    g.setColour(juce::Colour(0xfff59e0b)); // Warm Amber
    g.drawText("WORSHIP MACROS", juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 140.0f, 16.0f),
               juce::Justification::left);

    g.setFont(theme_.getLabelFont(10.0f));
    g.setColour(theme_.textMuted);
    g.drawText("PERFORMANCE", juce::Rectangle<float>(area.getRight() - 90.0f, area.getY() + 7.0f, 80.0f, 14.0f),
               juce::Justification::right);

    const auto barArea = area.withTrimmedTop(area.getHeight() - 24.0f).reduced(10.0f, 2.0f);
    const float itemW = (barArea.getWidth() - 15.0f) / 4.0f;

    static constexpr std::array<const char*, 4> kPerfLabels = { "WHL", "VEL", "A.T.", "PBD" };
    static constexpr std::array<uint32_t, 4> kPerfCols = { 0xff3b82f6, 0xff2dd4bf, 0xfff43f5e, 0xffa855f7 };
    const std::array<float, 4> kPerfVals = { whlVisual_, velVisual_, atVisual_, pbdVisual_ };

    for (int i = 0; i < 4; ++i) {
        const juce::Rectangle<float> itemRect(
            barArea.getX() + static_cast<float>(i) * (itemW + 5.0f),
            barArea.getY(), itemW, barArea.getHeight());

        g.setFont(theme_.getLabelFont(9.0f));
        g.setColour(theme_.textMuted);
        g.drawText(kPerfLabels[static_cast<std::size_t>(i)],
                   juce::Rectangle<float>(itemRect.getX(), itemRect.getY(), itemRect.getWidth(), 9.0f),
                   juce::Justification::centred);

        const auto meterRect = itemRect.withTrimmedTop(10.0f).reduced(1.0f, 1.0f);
        g.setColour(juce::Colour(0xff090d16));
        g.fillRoundedRectangle(meterRect, 2.0f);

        const float val = std::clamp(kPerfVals[static_cast<std::size_t>(i)], 0.0f, 1.0f);
        if (val > 0.02f) {
            const auto fillRect = meterRect.withWidth(meterRect.getWidth() * val);
            g.setColour(juce::Colour(kPerfCols[static_cast<std::size_t>(i)]).withAlpha(0.75f));
            g.fillRoundedRectangle(fillRect, 2.0f);
        }
    }
}

} // namespace nova::synth::ui

