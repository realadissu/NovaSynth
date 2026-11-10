#include "JornaRightColumn.h"
#include "../NovaSynthProcessor.h"
#include <cmath>

namespace nova::synth {

void JornaRightColumn::paint(juce::Graphics& g) {
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

} // namespace nova::synth
