#pragma once

#include "nova_ui/NovaTheme.h"
#include "nova_ui/ArcKnob.h"
#include "HorizontalViewport.h"
#include "../NovaSynthProcessor.h"
#include "BinaryData.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>
#include <unordered_map>

namespace nova::synth {

// ==============================================================================
// ExpandableEnvelopeCard: Akkordeon-Karte für DAHDSR + Velocity
// - Kompakt (~130px): Volle Kurve mit interaktiven Griffpunkten, Wertzeile unten
// - Expandiert (~390px): 6 Knobs (DEL, ATK, HOLD, DEC, SUS, REL) oben,
//                        breite interaktive Kurve + vertikaler VEL-Slider rechts
// ==============================================================================
class ExpandableEnvelopeCard : public juce::Component {
public:
    enum class DragNode {
        None,
        Delay,
        Attack,
        Hold,
        DecaySustain,
        Release,
        Velocity
    };

    std::function<void(int, bool)> onExpandRequested;
    std::function<void(int, bool)> onPinToggled;

    ExpandableEnvelopeCard(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme,
                           int envIndex, const juce::String& title, juce::Colour accentCol,
                           const juce::String& prefix)
        : proc_(proc),
          theme_(theme),
          envIndex_(envIndex),
          title_(title),
          accentCol_(accentCol),
          prefix_(prefix),
          delKnob_("DEL", "s", theme, false),
          atkKnob_("ATK", "s", theme, false),
          holdKnob_("HOLD", "s", theme, false),
          decKnob_("DEC", "s", theme, false),
          susKnob_("SUS", "", theme, false),
          relKnob_("REL", "s", theme, false) {

        addAndMakeVisible(delKnob_);
        addAndMakeVisible(atkKnob_);
        addAndMakeVisible(holdKnob_);
        addAndMakeVisible(decKnob_);
        addAndMakeVisible(susKnob_);
        addAndMakeVisible(relKnob_);

        velSlider_.setSliderStyle(juce::Slider::LinearVertical);
        velSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        velSlider_.setScrollWheelEnabled(false);
        velSlider_.setColour(juce::Slider::trackColourId, accentCol_);
        velSlider_.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff18202c));
        velSlider_.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        velSlider_.setTooltip("Velocity Sensitivity (0% = fixed envelope volume, 100% = key velocity dynamically scales envelope amplitude)");
        velSlider_.onValueChange = [this] { repaint(); };
        addAndMakeVisible(velSlider_);

        auto& apvts = proc_.getAPVTS();
        delKnob_.attachToParameter(apvts, prefix + "Delay");
        atkKnob_.attachToParameter(apvts, prefix + "Attack");
        holdKnob_.attachToParameter(apvts, prefix + "Hold");
        decKnob_.attachToParameter(apvts, prefix + "Decay");
        susKnob_.attachToParameter(apvts, prefix + "Sustain");
        relKnob_.attachToParameter(apvts, prefix + "Release");

        delKnob_.getSlider().setScrollWheelEnabled(false);
        atkKnob_.getSlider().setScrollWheelEnabled(false);
        holdKnob_.getSlider().setScrollWheelEnabled(false);
        decKnob_.getSlider().setScrollWheelEnabled(false);
        susKnob_.getSlider().setScrollWheelEnabled(false);
        relKnob_.getSlider().setScrollWheelEnabled(false);

        if (apvts.getParameter(prefix + "Velocity") != nullptr) {
            velAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                apvts, prefix + "Velocity", velSlider_);
        }

        delKnob_.getSlider().onValueChange = [this] { repaint(); };
        atkKnob_.getSlider().onValueChange = [this] { repaint(); };
        holdKnob_.getSlider().onValueChange = [this] { repaint(); };
        decKnob_.getSlider().onValueChange = [this] { repaint(); };
        susKnob_.getSlider().onValueChange = [this] { repaint(); };
        relKnob_.getSlider().onValueChange = [this] { repaint(); };

        updateKnobVisibility();
    }

    void setExpanded(bool exp) {
        if (isExpanded_ == exp) return;
        isExpanded_ = exp;
        updateKnobVisibility();
        resized();
        repaint();
    }

    [[nodiscard]] bool isExpanded() const noexcept { return isExpanded_; }

    void setPinned(bool pin) {
        isPinned_ = pin;
        repaint();
    }

    [[nodiscard]] bool isPinned() const noexcept { return isPinned_; }

    void updateKnobVisibility() {
        delKnob_.setVisible(isExpanded_);
        atkKnob_.setVisible(isExpanded_);
        holdKnob_.setVisible(isExpanded_);
        decKnob_.setVisible(isExpanded_);
        susKnob_.setVisible(isExpanded_);
        relKnob_.setVisible(isExpanded_);
        velSlider_.setVisible(isExpanded_);
    }

    void paint(juce::Graphics& g) override {
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

        // Im expandierten Zustand: VEL SENS Beschriftung und Live-Prozentanzeige
        if (isExpanded_ && velBox_.getWidth() > 0) {
            g.setFont(theme_.getLabelFont(8.0f));
            g.setColour(theme_.accentPrimary);
            g.drawText("VEL SENS", velBox_.getX() - 6, velBox_.getY(), velBox_.getWidth() + 12, 12, juce::Justification::centred, false);

            const int pct = static_cast<int>(std::round(velSlider_.getValue() * 100.0));
            g.setFont(theme_.getValueFont(8.5f));
            g.setColour(theme_.textPrimary);
            g.drawText(juce::String(pct) + "%", velBox_.getX() - 4, velBox_.getBottom() - 13, velBox_.getWidth() + 8, 12, juce::Justification::centred, false);
        }
    }

    void resized() override {
        updateKnobVisibility();

        if (!isExpanded_) {
            // Kompakt: Kurve füllt die gesamte Höhe sauber aus ohne enge Sekundenleiste
            curveBox_ = juce::Rectangle<float>(6.0f, 24.0f, static_cast<float>(getWidth() - 12), static_cast<float>(getHeight() - 30));
            velBox_ = {};
        } else {
            auto area = getLocalBounds().reduced(6, 4);
            area.removeFromTop(22); // Header

            // 6 Knobs Reihe
            auto knobRow = area.removeFromTop(56);
            const int kw = knobRow.getWidth() / 6;
            delKnob_.setBounds(knobRow.removeFromLeft(kw).reduced(2, 0));
            atkKnob_.setBounds(knobRow.removeFromLeft(kw).reduced(2, 0));
            holdKnob_.setBounds(knobRow.removeFromLeft(kw).reduced(2, 0));
            decKnob_.setBounds(knobRow.removeFromLeft(kw).reduced(2, 0));
            susKnob_.setBounds(knobRow.removeFromLeft(kw).reduced(2, 0));
            relKnob_.setBounds(knobRow.reduced(2, 0));

            area.removeFromTop(4);

            // Unterer Bereich: Kurve (links) + VEL Slider (rechts)
            velBox_ = area.removeFromRight(34);
            auto vArea = velBox_;
            vArea.removeFromTop(13);
            vArea.removeFromBottom(15);
            velSlider_.setBounds(vArea.reduced(5, 2));

            area.removeFromRight(6);
            curveBox_ = area.toFloat();
        }
    }

    [[nodiscard]] DragNode getHitNode(juce::Point<float> pos) const noexcept {
        if (!curveBox_.contains(pos)) return DragNode::None;

        if (pos.getDistanceFrom(nodeDel_) <= 16.0f && cachedDelW_ > 3.0f) return DragNode::Delay;
        if (pos.getDistanceFrom(nodeAtk_) <= 18.0f) return DragNode::Attack;
        if (pos.getDistanceFrom(nodeHold_) <= 16.0f && cachedHoldW_ > 3.0f) return DragNode::Hold;
        if (pos.getDistanceFrom(nodeDec_) <= 18.0f) return DragNode::DecaySustain;
        if (pos.getDistanceFrom(nodeRel_) <= 18.0f) return DragNode::Release;

        const float startX = curveBox_.getX() + 6.0f;
        const float relX = pos.x - startX;
        if (relX <= cachedDelW_ + 4.0f && cachedDelW_ > 3.0f) return DragNode::Delay;
        if (relX <= (nodeAtk_.x - startX) + 4.0f) return DragNode::Attack;
        if (relX <= (nodeHold_.x - startX) + 4.0f && cachedHoldW_ > 3.0f) return DragNode::Hold;
        if (relX <= (nodeSusEnd_.x - startX) + 4.0f) return DragNode::DecaySustain;
        return DragNode::Release;
    }

    void mouseMove(const juce::MouseEvent& e) override {
        if (e.eventComponent != this) return;

        if (pinRect_.contains(e.getPosition())) {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
            if (hoveredNode_ != DragNode::None) {
                hoveredNode_ = DragNode::None;
                repaint();
            }
            return;
        }

        // Header hover: Hand Cursor
        if (e.getPosition().y <= 24) {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
            if (hoveredNode_ != DragNode::None) {
                hoveredNode_ = DragNode::None;
                repaint();
            }
            return;
        }

        const auto hit = getHitNode(e.position);
        if (hit != hoveredNode_) {
            hoveredNode_ = hit;
            repaint();
        }

        if (hoveredNode_ == DragNode::Attack || hoveredNode_ == DragNode::Release ||
            hoveredNode_ == DragNode::Delay || hoveredNode_ == DragNode::Hold) {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        } else if (hoveredNode_ == DragNode::DecaySustain) {
            setMouseCursor(juce::MouseCursor::UpDownLeftRightResizeCursor);
        } else if (curveBox_.contains(e.position)) {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        } else {
            setMouseCursor(juce::MouseCursor::NormalCursor);
        }
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        if (auto* vp = findParentComponentOfClass<juce::Viewport>()) {
            vp->mouseWheelMove(e, wheel);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (e.eventComponent != this) return;
        const auto pos = e.getPosition();

        // 1. Pin Button (nur wenn expandiert)
        if (isExpanded_ && pinRect_.contains(pos)) {
            isPinned_ = !isPinned_;
            if (onPinToggled) onPinToggled(envIndex_, isPinned_);
            if (!isPinned_) {
                setExpanded(false);
            }
            repaint();
            return;
        }

        // 2. Header-Klick (y <= 24, z.B. auf Titel "ENV 1: AMP"):
        // - Wenn kompakt: expandieren!
        // - Wenn expandiert: einklappen (außer wenn gepinnt)!
        if (pos.y <= 24) {
            if (!isExpanded_) {
                if (onExpandRequested) onExpandRequested(envIndex_, true);
            } else if (!isPinned_) {
                if (onExpandRequested) onExpandRequested(envIndex_, false);
            }
            return;
        }

        // 3. ADSR Kurve Klick & Drag (sowohl im kompakten als auch im expandierten Zustand):
        // Ermöglicht direktes Bearbeiten der Hüllkurve mit der Maus in der Kompaktansicht,
        // OHNE dass die Karte unaufgefordert aufspringt!
        if (curveBox_.contains(e.position)) {
            // Optional: Doppel-Klick in Kompaktansicht expandiert
            if (!isExpanded_ && e.getNumberOfClicks() >= 2) {
                if (onExpandRequested) onExpandRequested(envIndex_, true);
                return;
            }

            activeDrag_ = getHitNode(e.position);
            dragStartPos_ = e.position;
            dragStartDel_ = static_cast<float>(delKnob_.getSlider().getValue());
            dragStartAtk_ = static_cast<float>(atkKnob_.getSlider().getValue());
            dragStartHold_ = static_cast<float>(holdKnob_.getSlider().getValue());
            dragStartDec_ = static_cast<float>(decKnob_.getSlider().getValue());
            dragStartSus_ = static_cast<float>(susKnob_.getSlider().getValue());
            dragStartRel_ = static_cast<float>(relKnob_.getSlider().getValue());
            repaint();
            return;
        }

        // 4. Sonstiger Klick auf freie Fläche der kompakten Karte: expandieren
        if (!isExpanded_) {
            if (onExpandRequested) onExpandRequested(envIndex_, true);
            return;
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (activeDrag_ == DragNode::None) return;

        const auto delta = e.position - dragStartPos_;
        const float w = curveBox_.getWidth() - 12.0f;
        const float h = (curveBox_.getBottom() - 8.0f) - (curveBox_.getY() + 8.0f);

        if (activeDrag_ == DragNode::Delay) {
            const float deltaSec = (delta.x / (w * 0.20f)) * 1.5f;
            const float newDel = std::clamp(dragStartDel_ + deltaSec, 0.0f, 2.0f);
            delKnob_.getSlider().setValue(newDel, juce::sendNotificationSync);
        } else if (activeDrag_ == DragNode::Attack) {
            const float deltaSec = (delta.x / (w * 0.30f)) * 4.0f;
            const float newAtk = std::clamp(dragStartAtk_ + deltaSec, 0.002f, 8.0f);
            atkKnob_.getSlider().setValue(newAtk, juce::sendNotificationSync);
        } else if (activeDrag_ == DragNode::Hold) {
            const float deltaSec = (delta.x / (w * 0.20f)) * 2.0f;
            const float newHold = std::clamp(dragStartHold_ + deltaSec, 0.0f, 3.0f);
            holdKnob_.getSlider().setValue(newHold, juce::sendNotificationSync);
        } else if (activeDrag_ == DragNode::DecaySustain) {
            const float deltaDecSec = (delta.x / (w * 0.30f)) * 5.0f;
            const float newDec = std::clamp(dragStartDec_ + deltaDecSec, 0.02f, 10.0f);
            decKnob_.getSlider().setValue(newDec, juce::sendNotificationSync);

            const float deltaSus = -delta.y / h;
            const float newSus = std::clamp(dragStartSus_ + deltaSus, 0.0f, 1.0f);
            susKnob_.getSlider().setValue(newSus, juce::sendNotificationSync);
        } else if (activeDrag_ == DragNode::Release) {
            const float deltaRelSec = (delta.x / (w * 0.30f)) * 5.0f;
            const float newRel = std::clamp(dragStartRel_ + deltaRelSec, 0.01f, 10.0f);
            relKnob_.getSlider().setValue(newRel, juce::sendNotificationSync);
        }
        repaint();
    }

    void mouseUp(const juce::MouseEvent& /*e*/) override {
        activeDrag_ = DragNode::None;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }

    float currentWidth_ = 135.0f;

private:
    void drawAdsrCurve(juce::Graphics& g, juce::Rectangle<float> area) {
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

        // 2. ATK segment: Strictly depends on ATK
        const float atkNorm = std::pow(std::clamp(atk / 6.0f, 0.0f, 1.0f), 0.38f);
        const float atkW = 6.0f + atkNorm * (w * 0.20f);

        // 3. HOLD segment: extends from nodeAtk_
        const float hldNorm = std::pow(std::clamp(hld / 3.0f, 0.0f, 1.0f), 0.45f);
        const float hldW = (hld > 0.005f) ? (4.0f + hldNorm * (w * 0.12f)) : 0.0f;

        // 4. DECAY segment: strictly depends on DEC
        const float decNorm = std::pow(std::clamp(dec / 8.0f, 0.0f, 1.0f), 0.38f);
        const float decW = 6.0f + decNorm * (w * 0.22f);

        // 5. SUSTAIN plateau: visual representation of the held key state (fixed proportion)
        const float susW = std::clamp(w * 0.14f, 18.0f, 32.0f);
        const float susY = bottomY - std::clamp(sus, 0.05f, 1.0f) * h;

        // 6. RELEASE segment: strictly depends on REL
        const float relNorm = std::pow(std::clamp(rel / 8.0f, 0.0f, 1.0f), 0.38f);
        const float relW = 6.0f + relNorm * (w * 0.25f);

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
        p.quadraticTo(nodeDel_.x + (nodeAtk_.x - nodeDel_.x) * 0.25f, topY, nodeAtk_.x, nodeAtk_.y);
        fillP.quadraticTo(nodeDel_.x + (nodeAtk_.x - nodeDel_.x) * 0.25f, topY, nodeAtk_.x, nodeAtk_.y);

        if (finalHldW > 1.0f) {
            p.lineTo(nodeHold_.x, nodeHold_.y);
            fillP.lineTo(nodeHold_.x, nodeHold_.y);
        }

        // Curved Decay (concave fall to sustain level)
        p.quadraticTo(nodeHold_.x + (nodeDec_.x - nodeHold_.x) * 0.70f, susY, nodeDec_.x, nodeDec_.y);
        fillP.quadraticTo(nodeHold_.x + (nodeDec_.x - nodeHold_.x) * 0.70f, susY, nodeDec_.x, nodeDec_.y);

        // Flat Sustain section
        p.lineTo(nodeSusEnd_.x, nodeSusEnd_.y);
        fillP.lineTo(nodeSusEnd_.x, nodeSusEnd_.y);

        // Curved Release (concave fall to bottom)
        p.quadraticTo(nodeSusEnd_.x + (nodeRel_.x - nodeSusEnd_.x) * 0.65f, bottomY, nodeRel_.x, nodeRel_.y);
        fillP.quadraticTo(nodeSusEnd_.x + (nodeRel_.x - nodeSusEnd_.x) * 0.65f, bottomY, nodeRel_.x, nodeRel_.y);

        // Baseline continuation to the right edge if release finished early
        if (nodeRel_.x < startX + w) {
            p.lineTo(startX + w, bottomY);
            fillP.lineTo(startX + w, bottomY);
        }

        fillP.closeSubPath();

        // Subtle gradient under waveform
        g.setGradientFill(juce::ColourGradient(accentCol_.withAlpha(0.24f), startX, topY,
                                              accentCol_.withAlpha(0.03f), startX, bottomY, false));
        g.fillPath(fillP);

        // Vertical guideline indicating Note-Off / Key Release Gate
        g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.55f));
        g.drawVerticalLine(static_cast<int>(nodeSusEnd_.x), topY - 2.0f, bottomY + 2.0f);

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
        if (finalHldW > 3.0f) drawHandleDot(g, nodeHold_.x, nodeHold_.y, hldActive ? 4.8f : 3.4f, hldActive);
        drawHandleDot(g, nodeDec_.x, nodeDec_.y, decActive ? 5.2f : 3.8f, decActive);
        drawHandleDot(g, nodeRel_.x, nodeRel_.y, relActive ? 4.8f : 3.4f, relActive);
    }

    void drawHandleDot(juce::Graphics& g, float x, float y, float radius, bool isHighlighted) {
        if (isHighlighted) {
            g.setColour(accentCol_.withAlpha(0.40f));
            g.fillEllipse(x - radius * 2.0f, y - radius * 2.0f, radius * 4.0f, radius * 4.0f);
        }
        g.setColour(juce::Colours::white);
        g.fillEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(accentCol_);
        g.drawEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f, 1.2f);
    }

    static void drawPinIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour oscCol, bool isPinned) {
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

    [[maybe_unused]] NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int envIndex_ = 0;
    juce::String title_;
    juce::Colour accentCol_;
    juce::String prefix_;

    bool isExpanded_ = false;
    bool isPinned_ = false;

    nova::ui::ArcKnob delKnob_;
    nova::ui::ArcKnob atkKnob_;
    nova::ui::ArcKnob holdKnob_;
    nova::ui::ArcKnob decKnob_;
    nova::ui::ArcKnob susKnob_;
    nova::ui::ArcKnob relKnob_;

    juce::Slider velSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velAttachment_;

    juce::Rectangle<int> pinRect_;
    juce::Rectangle<int> velBox_;
    juce::Rectangle<float> curveBox_;

    float cachedDelW_ = 0.0f;
    float cachedAtkW_ = 20.0f;
    float cachedHoldW_ = 0.0f;
    float cachedDecW_ = 20.0f;
    float cachedSusW_ = 14.0f;

    DragNode activeDrag_ = DragNode::None;
    DragNode hoveredNode_ = DragNode::None;
    juce::Point<float> dragStartPos_;
    float dragStartDel_ = 0.0f;
    float dragStartAtk_ = 0.45f;
    float dragStartHold_ = 0.0f;
    float dragStartDec_ = 1.40f;
    float dragStartSus_ = 0.82f;
    float dragStartRel_ = 2.20f;
    juce::Point<float> nodeDel_;
    juce::Point<float> nodeAtk_;
    juce::Point<float> nodeHold_;
    juce::Point<float> nodeDec_;
    juce::Point<float> nodeSusEnd_;
    juce::Point<float> nodeRel_;
};

// ==============================================================================
// EnvelopeQuadDeck: Horizontal scrollbarer Viewport mit weichem Accordion-Effekt
// ==============================================================================
class EnvelopeQuadDeck : public juce::Component, public juce::Timer {
public:
    EnvelopeQuadDeck(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc), theme_(theme),
          deckContent_(*this),
          ampEnv_(proc, theme, 0, "ENV 1: AMP", juce::Colour(0xff38bdf8), "amp"),
          fltEnv_(proc, theme, 1, "ENV 2: FILTER", juce::Colour(0xffa855f7), "flt"),
          modEnv_(proc, theme, 2, "ENV 3: MOD", juce::Colour(0xff2dd4bf), "mod"),
          auxEnv_(proc, theme, 3, "ENV 4: AUX", juce::Colour(0xfff59e0b), "aux") {

        viewport_.setViewedComponent(&deckContent_, false);
        addAndMakeVisible(viewport_);

        deckContent_.addAndMakeVisible(ampEnv_);
        deckContent_.addAndMakeVisible(fltEnv_);
        deckContent_.addAndMakeVisible(modEnv_);
        deckContent_.addAndMakeVisible(auxEnv_);

        setupCallbacks(ampEnv_);
        setupCallbacks(fltEnv_);
        setupCallbacks(modEnv_);
        setupCallbacks(auxEnv_);

        // All envelopes start collapsed in sleek compact overview mode on launch
        ampEnv_.setExpanded(false);
        fltEnv_.setExpanded(false);
        modEnv_.setExpanded(false);
        auxEnv_.setExpanded(false);

        startTimerHz(60);
    }

    std::array<ExpandableEnvelopeCard*, 4> getCardList() noexcept {
        return { &ampEnv_, &fltEnv_, &modEnv_, &auxEnv_ };
    }

    ~EnvelopeQuadDeck() override {
        stopTimer();
    }

    void setupCallbacks(ExpandableEnvelopeCard& card) {
        card.onExpandRequested = [this](int envIdx, bool exp) {
            handleCardExpand(envIdx, exp);
        };
        card.onPinToggled = [this](int envIdx, bool pin) {
            handleCardPin(envIdx, pin);
        };
    }

    void handleCardExpand(int envIdx, bool shouldExpand) {
        auto cards = getCardList();
        for (std::size_t i = 0; i < 4; ++i) {
            if (static_cast<int>(i) == envIdx) {
                cards[i]->setExpanded(shouldExpand);
            } else if (!cards[i]->isPinned()) {
                cards[i]->setExpanded(false);
            }
        }
        if (shouldExpand) {
            autoScrollToCard(envIdx);
        }
    }

    void handleCardPin(int envIdx, bool isPinned) {
        auto cards = getCardList();
        if (envIdx >= 0 && envIdx < 4) {
            cards[static_cast<std::size_t>(envIdx)]->setPinned(isPinned);
            if (isPinned) {
                cards[static_cast<std::size_t>(envIdx)]->setExpanded(true);
                autoScrollToCard(envIdx);
            } else {
                cards[static_cast<std::size_t>(envIdx)]->setExpanded(false);
            }
        }
    }

    void collapseUnpinnedCards() {
        auto cards = getCardList();
        for (auto* card : cards) {
            if (card->isVisible() && !card->isPinned()) {
                card->setExpanded(false);
            }
        }
    }

    void autoScrollToCard(int envIdx) {
        auto cards = getCardList();
        if (envIdx >= 0 && envIdx < 4) {
            const int cardX = cards[static_cast<std::size_t>(envIdx)]->getX();
            const int targetW = static_cast<int>(cards[static_cast<std::size_t>(envIdx)]->isExpanded() ? 390.0f : 130.0f);
            const int viewX = viewport_.getViewPositionX();
            const int viewW = viewport_.getViewWidth();

            if (cardX < viewX) {
                viewport_.setViewPosition(std::max(0, cardX - 10), 0);
            } else if (cardX + targetW > viewX + viewW) {
                const int maxX = std::max(0, deckContent_.getWidth() - viewW);
                viewport_.setViewPosition(std::clamp(cardX + targetW - viewW + 10, 0, maxX), 0);
            }
        }
    }

    void timerCallback() override {
        auto cards = getCardList();
        bool needsLayout = false;

        const int availableW = viewport_.getWidth() - 14;
        int numExpanded = 0;
        for (std::size_t i = 0; i < 4; ++i) {
            if (cards[i]->isExpanded()) numExpanded++;
        }

        const float expTarget = 390.0f;
        float compactTarget = 130.0f;

        if (numExpanded == 0) {
            compactTarget = std::max(130.0f, (static_cast<float>(availableW) - 18.0f) / 4.0f);
        } else {
            const int numCompact = 4 - numExpanded;
            if (numCompact > 0) {
                const float remW = static_cast<float>(availableW) - (static_cast<float>(numExpanded) * expTarget) - 18.0f;
                // Compact cards retain at least 130px, sliding out to the right instead of squishing!
                compactTarget = std::max(130.0f, remW / static_cast<float>(numCompact));
            }
        }

        for (std::size_t i = 0; i < 4; ++i) {
            const float target = cards[i]->isExpanded() ? expTarget : compactTarget;
            const float diff = target - cards[i]->currentWidth_;
            if (std::abs(diff) > 0.5f) {
                cards[i]->currentWidth_ += diff * 0.28f;
                needsLayout = true;
            } else {
                cards[i]->currentWidth_ = target;
            }
        }

        if (needsLayout) {
            updateDeckLayout();
        }
    }

    void resized() override {
        viewport_.setBounds(getLocalBounds());
        updateDeckLayout();
    }

private:
    void updateDeckLayout() {
        auto cards = getCardList();
        const int cardH = getHeight() - 14;
        int currentX = 4;

        for (std::size_t i = 0; i < 4; ++i) {
            const int w = static_cast<int>(std::round(cards[i]->currentWidth_));
            cards[i]->setBounds(currentX, 2, w, cardH);
            currentX += w + 6;
        }

        const int targetW = std::max(getWidth(), currentX + 8);
        const int targetH = getHeight() - 6;
        if (deckContent_.getWidth() != targetW || deckContent_.getHeight() != targetH) {
            deckContent_.setSize(targetW, targetH);
        }
        deckContent_.repaint();
    }

    class DeckContentComponent : public juce::Component {
    public:
        DeckContentComponent(EnvelopeQuadDeck& owner) : owner_(owner) {}

        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
            owner_.viewport_.mouseWheelMove(e, wheel);
        }

        void mouseDown(const juce::MouseEvent& e) override {
            if (e.eventComponent == this) {
                auto cards = owner_.getCardList();
                for (auto* card : cards) {
                    if (card->isVisible() && !card->isPinned()) {
                        card->setExpanded(false);
                    }
                }
            }
        }

    private:
        EnvelopeQuadDeck& owner_;
    };

    [[maybe_unused]] NovaSynthProcessor& proc_;
    [[maybe_unused]] const nova::ui::NovaTheme& theme_;
    HorizontalViewport viewport_;
    DeckContentComponent deckContent_{*this};

    ExpandableEnvelopeCard ampEnv_;
    ExpandableEnvelopeCard fltEnv_;
    ExpandableEnvelopeCard modEnv_;
    ExpandableEnvelopeCard auxEnv_;
};

} // namespace nova::synth
