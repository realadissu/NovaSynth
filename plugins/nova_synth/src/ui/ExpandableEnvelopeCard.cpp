#include "ExpandableEnvelopeCard.h"
#include <algorithm>
#include <cmath>

namespace nova::synth {

ExpandableEnvelopeCard::ExpandableEnvelopeCard(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme,
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
      susKnob_("SUS", "%", theme, false),
      relKnob_("REL", "s", theme, false)
{
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

void ExpandableEnvelopeCard::setExpanded(bool exp) {
    if (isExpanded_ == exp) return;
    isExpanded_ = exp;
    updateKnobVisibility();
    resized();
    repaint();
}

bool ExpandableEnvelopeCard::isExpanded() const noexcept { return isExpanded_; }

void ExpandableEnvelopeCard::setPinned(bool pin) {
    isPinned_ = pin;
    repaint();
}

bool ExpandableEnvelopeCard::isPinned() const noexcept { return isPinned_; }

juce::Point<float> ExpandableEnvelopeCard::getNodeDel() const noexcept { return nodeDel_; }
juce::Point<float> ExpandableEnvelopeCard::getNodeAtk() const noexcept { return nodeAtk_; }
juce::Point<float> ExpandableEnvelopeCard::getNodeHold() const noexcept { return nodeHold_; }
juce::Point<float> ExpandableEnvelopeCard::getNodeDec() const noexcept { return nodeDec_; }
juce::Point<float> ExpandableEnvelopeCard::getNodeSusEnd() const noexcept { return nodeSusEnd_; }
juce::Point<float> ExpandableEnvelopeCard::getNodeRel() const noexcept { return nodeRel_; }
const nova::ui::ArcKnob& ExpandableEnvelopeCard::getAtkKnob() const noexcept { return atkKnob_; }
const nova::ui::ArcKnob& ExpandableEnvelopeCard::getDecKnob() const noexcept { return decKnob_; }
const nova::ui::ArcKnob& ExpandableEnvelopeCard::getSusKnob() const noexcept { return susKnob_; }
const nova::ui::ArcKnob& ExpandableEnvelopeCard::getRelKnob() const noexcept { return relKnob_; }

void ExpandableEnvelopeCard::updateKnobVisibility() {
    delKnob_.setVisible(isExpanded_);
    atkKnob_.setVisible(isExpanded_);
    holdKnob_.setVisible(isExpanded_);
    decKnob_.setVisible(isExpanded_);
    susKnob_.setVisible(isExpanded_);
    relKnob_.setVisible(isExpanded_);
    velSlider_.setVisible(isExpanded_);
}

void ExpandableEnvelopeCard::resized() {
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

ExpandableEnvelopeCard::DragNode ExpandableEnvelopeCard::getHitNode(juce::Point<float> pos) const noexcept {
    if (!curveBox_.contains(pos)) return DragNode::None;

    if (pos.getDistanceFrom(nodeDel_) <= 16.0f && cachedDelW_ > 3.0f) return DragNode::Delay;
    if (pos.getDistanceFrom(nodeHold_) <= 18.0f && pos.x >= nodeHold_.x) return DragNode::Hold;
    if (pos.getDistanceFrom(nodeAtk_) <= 18.0f) return DragNode::Attack;
    if (pos.getDistanceFrom(nodeHold_) <= 18.0f) return DragNode::Hold;
    if (pos.getDistanceFrom(nodeDec_) <= 18.0f) return DragNode::DecaySustain;
    const bool hasSustain = (susKnob_.getSlider().getValue() > 0.005);
    if (hasSustain && pos.getDistanceFrom(nodeRel_) <= 18.0f) return DragNode::Release;

    const float startX = curveBox_.getX() + 6.0f;
    const float relX = pos.x - startX;
    if (relX <= cachedDelW_ + 4.0f && cachedDelW_ > 3.0f) return DragNode::Delay;
    if (relX <= (nodeAtk_.x - startX) + 4.0f) return DragNode::Attack;
    if (relX <= (nodeHold_.x - startX) + 6.0f) return DragNode::Hold;
    if (relX <= (nodeSusEnd_.x - startX) + 4.0f) return DragNode::DecaySustain;
    return DragNode::Release;
}

void ExpandableEnvelopeCard::mouseMove(const juce::MouseEvent& e) {
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

void ExpandableEnvelopeCard::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    if (auto* vp = findParentComponentOfClass<juce::Viewport>()) {
        vp->mouseWheelMove(e, wheel);
    }
}

void ExpandableEnvelopeCard::mouseDown(const juce::MouseEvent& e) {
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

void ExpandableEnvelopeCard::mouseDrag(const juce::MouseEvent& e) {
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

void ExpandableEnvelopeCard::mouseUp(const juce::MouseEvent& /*e*/) {
    activeDrag_ = DragNode::None;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

} // namespace nova::synth

