#include "ExpandableEnvelopeCard.h"
#include "BinaryData.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace nova::synth {

void ExpandableEnvelopeCard::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);

    g.setColour(theme_.bgControl.withAlpha(0.35f));
    g.fillRoundedRectangle(bounds, theme_.cornerRadius);

    if (isExpanded_) {
        g.setColour(accentCol_.withAlpha(0.70f));
        g.drawRoundedRectangle(bounds, theme_.cornerRadius, 1.2f);
    } else {
        g.setColour(theme_.borderSubtle.withAlpha(0.7f));
        g.drawRoundedRectangle(bounds, theme_.cornerRadius, 0.8f);
    }

    // Header Zeile (24px)
    auto header = getLocalBounds().removeFromTop(24).reduced(6, 2);

    // Pin Button ganz rechts (nur im expandierten Zustand sichtbar)
    if (isExpanded_) {
        pinRect_ = header.removeFromRight(20);
        drawPinIcon(g, pinRect_.toFloat(), accentCol_, isPinned_);
        header.removeFromRight(4);
    } else {
        pinRect_ = {};
    }

    // LED Statuspunkt
    const float ledR = 3.0f;
    const float ledX = static_cast<float>(header.getX()) + 4.0f;
    const float ledY = static_cast<float>(header.getCentreY());
    g.setColour(accentCol_.withAlpha(0.4f));
    g.fillEllipse(ledX - 4.0f, ledY - 4.0f, 8.0f, 8.0f);
    g.setColour(accentCol_);
    g.fillEllipse(ledX - ledR, ledY - ledR, ledR * 2.0f, ledR * 2.0f);

    header.removeFromLeft(12);

    // Title Pill
    g.setFont(theme_.getSectionFont(10.5f));
    g.setColour(accentCol_);
    g.drawFittedText(title_, header, juce::Justification::centredLeft, 1, 0.85f);

    // Visualizer Box für ADSR Kurve
    g.setColour(theme_.bgSurface.darker(0.35f));
    g.fillRoundedRectangle(curveBox_, 4.0f);
    g.setColour(theme_.borderSubtle.withAlpha(0.6f));
    g.drawRoundedRectangle(curveBox_, 4.0f, 0.8f);

    // Zeichne Kurve
    drawAdsrCurve(g, curveBox_);

    // Im expandierten Zustand: VEL SENS Pill & Beschriftung
    if (isExpanded_ && velBox_.getWidth() > 0) {
        const auto vBoxF = velBox_.toFloat();
        g.setColour(juce::Colour(0xff090f18));
        g.fillRoundedRectangle(vBoxF, 4.0f);
        g.setColour(theme_.borderSubtle.withAlpha(0.7f));
        g.drawRoundedRectangle(vBoxF, 4.0f, 0.8f);

        g.setFont(theme_.getLabelFont(8.0f));
        g.setColour(theme_.accentPrimary);
        g.drawText("VEL", velBox_.getX(), velBox_.getY() + 3, velBox_.getWidth(), 11, juce::Justification::centred, false);

        const int pct = static_cast<int>(std::round(velSlider_.getValue() * 100.0));
        g.setFont(theme_.getValueFont(8.5f));
        g.setColour(pct > 0 ? theme_.textPrimary : theme_.textMuted);
        g.drawText(juce::String(pct) + "%", velBox_.getX(), velBox_.getBottom() - 14, velBox_.getWidth(), 12, juce::Justification::centred, false);
    }
}

void ExpandableEnvelopeCard::drawAdsrCurve(juce::Graphics& g, juce::Rectangle<float> area) {
    const float del = static_cast<float>(delKnob_.getSlider().getValue());
    const float atk = static_cast<float>(atkKnob_.getSlider().getValue());
    const float hld = static_cast<float>(holdKnob_.getSlider().getValue());
    const float dec = static_cast<float>(decKnob_.getSlider().getValue());
    const float sus = static_cast<float>(susKnob_.getSlider().getValue());
    const float rel = static_cast<float>(relKnob_.getSlider().getValue());

    const float w = area.getWidth() - 12.0f;
    const float startX = area.getX() + 6.0f;
    const float bottomY = area.getBottom() - 8.0f;
    const float topY = area.getY() + 8.0f;
    const float h = bottomY - topY;

    // 1. DEL segment: strictly depends on DEL
    const float delNorm = std::pow(std::clamp(del / 2.0f, 0.0f, 1.0f), 0.45f);
    const float delW = (del > 0.005f) ? (w * 0.03f + delNorm * (w * 0.10f)) : 0.0f;

    // 2. ATK segment: Strictly depends on ATK (0s = instant vertical leap)
    const float atkNorm = std::pow(std::clamp(atk / 6.0f, 0.0f, 1.0f), 0.38f);
    const float atkW = (atk > 0.005f) ? (3.0f + atkNorm * (w * 0.20f)) : 0.0f;

    // 3. HOLD segment: extends from nodeAtk_
    const float hldNorm = std::pow(std::clamp(hld / 3.0f, 0.0f, 1.0f), 0.45f);
    const float hldW = (hld > 0.005f) ? (4.0f + hldNorm * (w * 0.12f)) : 0.0f;

    // 4. DECAY segment: strictly depends on DEC
    const float decNorm = std::pow(std::clamp(dec / 8.0f, 0.0f, 1.0f), 0.38f);
    const float decW = (dec > 0.005f) ? (4.0f + decNorm * (w * 0.22f)) : 2.0f;

    // 5. SUSTAIN plateau: visual representation of the held key state
    const bool hasSustain = (sus > 0.005f);
    const float susW = hasSustain ? std::clamp(w * 0.14f, 16.0f, 32.0f) : 0.0f;
    const float susY = bottomY - std::clamp(sus, 0.0f, 1.0f) * h;

    // 6. RELEASE segment: strictly depends on REL
    const float relNorm = std::pow(std::clamp(rel / 8.0f, 0.0f, 1.0f), 0.38f);
    const float relW = hasSustain ? (4.0f + relNorm * (w * 0.22f)) : 0.0f;

    // Total required width: if sum exceeds available width w, scale down proportionally
    const float totalReq = delW + atkW + hldW + decW + susW + relW;
    const float scale = (totalReq > w) ? (w / totalReq) : 1.0f;

    const float finalDelW = delW * scale;
    const float finalAtkW = atkW * scale;
    const float finalHldW = hldW * scale;
    const float finalDecW = decW * scale;
    const float finalSusW = susW * scale;
    const float finalRelW = relW * scale;

    nodeDel_    = { startX + finalDelW, bottomY };
    nodeAtk_    = { nodeDel_.x + finalAtkW, topY };
    nodeHold_   = { nodeAtk_.x + finalHldW, topY };
    nodeDec_    = { nodeHold_.x + finalDecW, susY };
    nodeSusEnd_ = { nodeDec_.x + finalSusW, susY };
    nodeRel_    = { nodeSusEnd_.x + finalRelW, bottomY };

    cachedDelW_ = finalDelW;
    cachedAtkW_ = finalAtkW;
    cachedHoldW_ = finalHldW;
    cachedDecW_ = finalDecW;
    cachedSusW_ = finalSusW;

    // Path construction
    juce::Path p;
    juce::Path fillP;

    p.startNewSubPath(startX, bottomY);
    fillP.startNewSubPath(startX, bottomY);

    if (finalDelW > 1.0f) {
        p.lineTo(nodeDel_.x, nodeDel_.y);
        fillP.lineTo(nodeDel_.x, nodeDel_.y);
    }

    // Curved Attack (convex rise to peak)
    if (finalAtkW > 1.0f) {
        p.quadraticTo(nodeDel_.x + (nodeAtk_.x - nodeDel_.x) * 0.25f, topY, nodeAtk_.x, nodeAtk_.y);
        fillP.quadraticTo(nodeDel_.x + (nodeAtk_.x - nodeDel_.x) * 0.25f, topY, nodeAtk_.x, nodeAtk_.y);
    } else {
        p.lineTo(nodeAtk_.x, nodeAtk_.y);
        fillP.lineTo(nodeAtk_.x, nodeAtk_.y);
    }

    if (finalHldW > 1.0f) {
        p.lineTo(nodeHold_.x, nodeHold_.y);
        fillP.lineTo(nodeHold_.x, nodeHold_.y);
    }

    // Curved Decay (concave fall to sustain level)
    p.quadraticTo(nodeHold_.x + (nodeDec_.x - nodeHold_.x) * 0.70f, susY, nodeDec_.x, nodeDec_.y);
    fillP.quadraticTo(nodeHold_.x + (nodeDec_.x - nodeHold_.x) * 0.70f, susY, nodeDec_.x, nodeDec_.y);

    if (hasSustain) {
        // Flat Sustain section
        p.lineTo(nodeSusEnd_.x, nodeSusEnd_.y);
        fillP.lineTo(nodeSusEnd_.x, nodeSusEnd_.y);

        // Curved Release (concave fall to bottom)
        p.quadraticTo(nodeSusEnd_.x + (nodeRel_.x - nodeSusEnd_.x) * 0.65f, bottomY, nodeRel_.x, nodeRel_.y);
        fillP.quadraticTo(nodeSusEnd_.x + (nodeRel_.x - nodeSusEnd_.x) * 0.65f, bottomY, nodeRel_.x, nodeRel_.y);
    }

    // Baseline continuation to the right edge if release/decay finished early
    const float endX = hasSustain ? nodeRel_.x : nodeDec_.x;
    if (endX < startX + w) {
        p.lineTo(startX + w, bottomY);
        fillP.lineTo(startX + w, bottomY);
    }

    fillP.closeSubPath();

    // Subtle gradient under waveform
    g.setGradientFill(juce::ColourGradient(accentCol_.withAlpha(0.24f), startX, topY,
                                          accentCol_.withAlpha(0.03f), startX, bottomY, false));
    g.fillPath(fillP);

    // Vertical guideline indicating Note-Off / Key Release Gate (only if sustain is active)
    if (hasSustain) {
        g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.55f));
        g.drawVerticalLine(static_cast<int>(nodeSusEnd_.x), topY - 2.0f, bottomY + 2.0f);
    }

    // Leuchtende Kurven-Kontur
    g.setColour(accentCol_.withAlpha(0.25f));
    g.strokePath(p, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(accentCol_);
    g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Node Handles
    const bool delActive = (activeDrag_ == DragNode::Delay) || (hoveredNode_ == DragNode::Delay);
    const bool atkActive = (activeDrag_ == DragNode::Attack) || (hoveredNode_ == DragNode::Attack);
    const bool hldActive = (activeDrag_ == DragNode::Hold) || (hoveredNode_ == DragNode::Hold);
    const bool decActive = (activeDrag_ == DragNode::DecaySustain) || (hoveredNode_ == DragNode::DecaySustain);
    const bool relActive = (activeDrag_ == DragNode::Release) || (hoveredNode_ == DragNode::Release);

    if (finalDelW > 3.0f) drawHandleDot(g, nodeDel_.x, nodeDel_.y, delActive ? 4.8f : 3.4f, delActive);
    drawHandleDot(g, nodeAtk_.x, nodeAtk_.y, atkActive ? 4.8f : 3.4f, atkActive);
    drawHandleDot(g, nodeHold_.x, nodeHold_.y, hldActive ? 4.8f : 3.4f, hldActive);
    drawHandleDot(g, nodeDec_.x, nodeDec_.y, decActive ? 5.2f : 3.8f, decActive);
    if (hasSustain) {
        drawHandleDot(g, nodeRel_.x, nodeRel_.y, relActive ? 4.8f : 3.4f, relActive);
    }
}

void ExpandableEnvelopeCard::drawHandleDot(juce::Graphics& g, float x, float y, float radius, bool isHighlighted) {
    if (isHighlighted) {
        g.setColour(accentCol_.withAlpha(0.40f));
        g.fillEllipse(x - radius * 2.0f, y - radius * 2.0f, radius * 4.0f, radius * 4.0f);
    }
    g.setColour(juce::Colours::white);
    g.fillEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(accentCol_);
    g.drawEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f, 1.2f);
}

void ExpandableEnvelopeCard::drawPinIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour oscCol, bool isPinned) {
    static std::unordered_map<uint64_t, juce::Image> pinCache;
    const uint64_t key = (static_cast<uint64_t>(oscCol.getARGB()) << 1) | (isPinned ? 1ULL : 0ULL);

    auto it = pinCache.find(key);
    if (it == pinCache.end()) {
        const char* srcData = isPinned ? NovaResources::pin_filled_png : NovaResources::pin_outline_png;
        const int srcSize = isPinned ? NovaResources::pin_filled_pngSize : NovaResources::pin_outline_pngSize;
        auto rawImg = juce::ImageFileFormat::loadFrom(srcData, static_cast<size_t>(srcSize));

        if (rawImg.isValid()) {
            const int w = rawImg.getWidth();
            const int h = rawImg.getHeight();
            juce::Image tinted(juce::Image::ARGB, w, h, true);
            juce::Image::BitmapData srcBm(rawImg, juce::Image::BitmapData::readOnly);
            juce::Image::BitmapData dstBm(tinted, juce::Image::BitmapData::writeOnly);

            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    const auto p = srcBm.getPixelColour(x, y);
                    const float a = p.getFloatAlpha();
                    if (a > 0.001f) {
                        dstBm.setPixelColour(x, y, oscCol.withAlpha(a));
                    }
                }
            }
            it = pinCache.emplace(key, std::move(tinted)).first;
        }
    }

    if (it != pinCache.end() && it->second.isValid()) {
        g.drawImage(it->second, area, juce::RectanglePlacement::centred);
    }
}

} // namespace nova::synth

