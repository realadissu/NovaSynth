#include "JornaRightColumn.h"
#include "../NovaSynthProcessor.h"
#include <cmath>
#include <algorithm>

namespace nova::synth {

void JornaRightColumn::mouseMove(const juce::MouseEvent& event) {
    const auto posF = event.position;
    DragNode prevHovered = hoveredNode_;
    hoveredNode_ = DragNode::None;

    float bestDist = 18.0f;
    if (posF.getDistanceFrom(pPeak_) <= bestDist) {
        bestDist = posF.getDistanceFrom(pPeak_);
        hoveredNode_ = DragNode::Peak;
    }
    if (posF.getDistanceFrom(pSusStart_) <= bestDist) {
        bestDist = posF.getDistanceFrom(pSusStart_);
        hoveredNode_ = DragNode::SusStart;
    }
    if (posF.getDistanceFrom(pSusEnd_) <= bestDist) {
        bestDist = posF.getDistanceFrom(pSusEnd_);
        hoveredNode_ = DragNode::SusEnd;
    }
    if (posF.getDistanceFrom(pEnd_) <= bestDist) {
        bestDist = posF.getDistanceFrom(pEnd_);
        hoveredNode_ = DragNode::End;
    }

    if (hoveredNode_ != DragNode::None) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    } else {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    if (hoveredNode_ != prevHovered) {
        repaint();
    }
}

void JornaRightColumn::mouseDown(const juce::MouseEvent& event) {
    const auto pos = event.getPosition();
    const auto posF = event.position;

    // 1. Check closest node handle within 18px grab radius
    float bestDist = 18.0f;
    DragNode bestNode = DragNode::None;

    if (posF.getDistanceFrom(pPeak_) <= bestDist) {
        bestDist = posF.getDistanceFrom(pPeak_);
        bestNode = DragNode::Peak;
    }
    if (posF.getDistanceFrom(pSusStart_) <= bestDist) {
        bestDist = posF.getDistanceFrom(pSusStart_);
        bestNode = DragNode::SusStart;
    }
    if (posF.getDistanceFrom(pSusEnd_) <= bestDist) {
        bestDist = posF.getDistanceFrom(pSusEnd_);
        bestNode = DragNode::SusEnd;
    }
    if (posF.getDistanceFrom(pEnd_) <= bestDist) {
        bestDist = posF.getDistanceFrom(pEnd_);
        bestNode = DragNode::End;
    }

    if (bestNode != DragNode::None) {
        activeDragNode_ = bestNode;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        repaint();
        return;
    }

    // 2. Click anywhere inside envelope graph area grabs the corresponding phase
    if (envGraphRect_.contains(posF)) {
        if (posF.x <= (pPeak_.x + pSusStart_.x) * 0.5f) {
            activeDragNode_ = DragNode::Peak;
        } else if (posF.x <= (pSusStart_.x + pSusEnd_.x) * 0.5f) {
            activeDragNode_ = DragNode::SusStart;
        } else if (posF.x <= (pSusEnd_.x + pEnd_.x) * 0.5f) {
            activeDragNode_ = DragNode::SusEnd;
        } else {
            activeDragNode_ = DragNode::End;
        }
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        mouseDrag(event);
        return;
    }

    if (flt1TabRect_.contains(pos)) {
        if (pos.x < flt1TabRect_.getX() + 22) {
            if (auto* p = proc_.getAPVTS().getParameter("filter1Enabled")) {
                p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
            }
        }
        selectFilterTab(0);
        return;
    }
    if (flt2TabRect_.contains(pos)) {
        if (pos.x < flt2TabRect_.getX() + 22) {
            if (auto* p = proc_.getAPVTS().getParameter("filter2Enabled")) {
                p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
            }
        }
        selectFilterTab(1);
        return;
    }
    if (fltTypeRect_.contains(pos)) {
        // Left arrow click: Cycle previous
        if (pos.x < fltTypeRect_.getX() + 24) {
            if (activeFilterTab_ == 0) {
                const int cur = proc_.getCurrentParameters().filterType;
                const int prev = (cur - 1 + 3) % 3;
                if (auto* p = proc_.getAPVTS().getParameter("filterType")) {
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(prev)));
                }
            } else {
                const int cur = proc_.getCurrentParameters().filter2Type;
                const int prev = (cur - 1 + 4) % 4;
                if (auto* p = proc_.getAPVTS().getParameter("filter2Type")) {
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(prev)));
                }
            }
            repaint();
            return;
        }

        // Right arrow click: Cycle next
        if (pos.x > fltTypeRect_.getRight() - 24) {
            if (activeFilterTab_ == 0) {
                const int cur = proc_.getCurrentParameters().filterType;
                if (auto* p = proc_.getAPVTS().getParameter("filterType")) {
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>((cur + 1) % 3)));
                }
            } else {
                const int cur = proc_.getCurrentParameters().filter2Type;
                if (auto* p = proc_.getAPVTS().getParameter("filter2Type")) {
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>((cur + 1) % 4)));
                }
            }
            repaint();
            return;
        }

        // Center click: Open rich Dropdown Popup Menu
        juce::PopupMenu menu;
        if (activeFilterTab_ == 0) {
            static constexpr std::array<const char*, 3> kFltNames = {
                "LOW 24 (Warm Analog Ladder / SVF)",
                "BAND 12 (Vocal Formant Bandpass)",
                "HIGH 24 (Air Highpass)"
            };
            const int cur = proc_.getCurrentParameters().filterType;
            for (int i = 0; i < 3; ++i) {
                menu.addItem(i + 1, kFltNames[static_cast<std::size_t>(i)], true, i == cur);
            }
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(fltTypeRect_)).withStandardItemHeight(28),
                [this](int res) {
                    if (res >= 1 && res <= 3) {
                        if (auto* p = proc_.getAPVTS().getParameter("filterType")) {
                            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(res - 1)));
                        }
                        repaint();
                    }
                });
        } else {
            static constexpr std::array<const char*, 4> kFlt2Names = {
                "LOW 24 (Secondary Warm Ladder)",
                "BAND 12 (Vocal Formant Bandpass)",
                "HIGH 24 (Air Highpass)",
                "CARVE BELL (Notch / Vocal Pocket)"
            };
            const int cur = proc_.getCurrentParameters().filter2Type;
            for (int i = 0; i < 4; ++i) {
                menu.addItem(i + 1, kFlt2Names[static_cast<std::size_t>(i)], true, i == cur);
            }
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(fltTypeRect_)).withStandardItemHeight(28),
                [this](int res) {
                    if (res >= 1 && res <= 4) {
                        if (auto* p = proc_.getAPVTS().getParameter("filter2Type")) {
                            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(res - 1)));
                        }
                        repaint();
                    }
                });
        }
        return;
    }

    for (int i = 0; i < static_cast<int>(envTabRects_.size()); ++i) {
        if (envTabRects_[static_cast<std::size_t>(i)].contains(pos)) {
            selectEnvelopeTab(i);
            return;
        }
    }
}

void JornaRightColumn::mouseDrag(const juce::MouseEvent& event) {
    if (activeDragNode_ == DragNode::None || graphW_ <= 10.0f || graphH_ <= 10.0f) return;

    const auto pos = event.position;
    auto& apvts = proc_.getAPVTS();
    const auto envIds = getActiveEnvParamIds();
    const juce::String atkId = envIds.attack;
    const juce::String decId = envIds.decay;
    const juce::String susId = envIds.sustain;
    const juce::String relId = envIds.release;

    const float wA = 0.28f * graphW_;
    const float wD = 0.28f * graphW_;
    const float wS = 0.16f * graphW_;
    const float wR = 0.28f * graphW_;

    if (activeDragNode_ == DragNode::Peak) {
        const float new_uA = std::clamp((pos.x - graphX0_) / wA, 0.0f, 1.0f);
        if (auto* p = apvts.getParameter(atkId)) {
            p->setValueNotifyingHost(new_uA);
        }
    } else if (activeDragNode_ == DragNode::SusStart) {
        const float new_uD = std::clamp((pos.x - pPeak_.x) / wD, 0.0f, 1.0f);
        if (auto* p = apvts.getParameter(decId)) {
            p->setValueNotifyingHost(new_uD);
        }
        const float new_uS = std::clamp((graphYB_ - pos.y) / graphH_, 0.0f, 1.0f);
        if (auto* p = apvts.getParameter(susId)) {
            p->setValueNotifyingHost(new_uS);
        }
    } else if (activeDragNode_ == DragNode::SusEnd) {
        const float new_uD = std::clamp((pos.x - wS - pPeak_.x) / wD, 0.0f, 1.0f);
        if (auto* p = apvts.getParameter(decId)) {
            p->setValueNotifyingHost(new_uD);
        }
        const float new_uS = std::clamp((graphYB_ - pos.y) / graphH_, 0.0f, 1.0f);
        if (auto* p = apvts.getParameter(susId)) {
            p->setValueNotifyingHost(new_uS);
        }
    } else if (activeDragNode_ == DragNode::End) {
        const float new_uR = std::clamp((pos.x - pSusEnd_.x) / wR, 0.0f, 1.0f);
        if (auto* p = apvts.getParameter(relId)) {
            p->setValueNotifyingHost(new_uR);
        }
    }
    repaint();
}

void JornaRightColumn::mouseUp(const juce::MouseEvent&) {
    activeDragNode_ = DragNode::None;
    setMouseCursor(hoveredNode_ != DragNode::None ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

void JornaRightColumn::mouseExit(const juce::MouseEvent&) {
    if (activeDragNode_ == DragNode::None) {
        hoveredNode_ = DragNode::None;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }
}

} // namespace nova::synth
