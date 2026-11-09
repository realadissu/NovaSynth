#include "SignalFlowPanel.h"
#include "../NovaSynthProcessor.h"
#include <cmath>
#include <algorithm>

namespace nova::synth {

void SignalFlowPanel::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) {
    if (cardSourcesRect_.contains(event.getPosition())) {
        constexpr int totalSources = 8;
        constexpr int visibleCount = 6;
        constexpr int maxScroll = totalSources - visibleCount;
        wheelAccumulator_ += wheel.deltaY;
        if (wheelAccumulator_ <= -0.04f) {
            if (sourceScrollOffset_ < maxScroll) {
                sourceScrollOffset_ = std::min(maxScroll, sourceScrollOffset_ + 1);
                repaint();
            }
            wheelAccumulator_ = 0.0f;
        } else if (wheelAccumulator_ >= 0.04f) {
            if (sourceScrollOffset_ > 0) {
                sourceScrollOffset_ = std::max(0, sourceScrollOffset_ - 1);
                repaint();
            }
            wheelAccumulator_ = 0.0f;
        }
    }
}

void SignalFlowPanel::mouseDown(const juce::MouseEvent& event) {
    handleFlowMouseDown(event.getPosition());
}

void SignalFlowPanel::mouseDrag(const juce::MouseEvent& event) {
    handleFlowMouseDrag(event);
}

void SignalFlowPanel::mouseUp(const juce::MouseEvent& event) {
    handleFlowMouseUp(event);
}

void SignalFlowPanel::handleFlowMouseDown(juce::Point<int> pos) {
    auto& apvts = proc_.getAPVTS();

    // 1. Toggle switches in headers (with state preservation)
    if (toggleSourcesRect_.expanded(4).contains(pos)) {
        const auto params = proc_.getCurrentParameters();
        bool anyOn = false;
        for (int i = 0; i < params.activeOscCount; ++i) {
            if (params.oscSlots[static_cast<std::size_t>(i)].enabled) anyOn = true;
        }

        if (anyOn) {
            for (int i = 0; i < 8; ++i) {
                const juce::String prefix = "osc" + juce::String(i + 1) + "_";
                if (auto* p = apvts.getParameter(prefix + "enabled")) {
                    cachedOscEnabledState_[static_cast<std::size_t>(i)] = (p->getValue() > 0.5f);
                    p->setValueNotifyingHost(0.0f);
                }
            }
        } else {
            bool restoredAny = false;
            for (int i = 0; i < 8; ++i) {
                const juce::String prefix = "osc" + juce::String(i + 1) + "_";
                if (auto* p = apvts.getParameter(prefix + "enabled")) {
                    const bool shouldEnable = cachedOscEnabledState_[static_cast<std::size_t>(i)];
                    if (shouldEnable) {
                        p->setValueNotifyingHost(1.0f);
                        restoredAny = true;
                    } else {
                        p->setValueNotifyingHost(0.0f);
                    }
                }
            }
            if (!restoredAny) {
                if (auto* p = apvts.getParameter("osc1_enabled")) p->setValueNotifyingHost(1.0f);
            }
        }
        if (onSourceStateChanged) onSourceStateChanged();
        repaint();
        return;
    }

    if (toggleFiltersRect_.expanded(4).contains(pos)) {
        const auto params = proc_.getCurrentParameters();
        const bool anyFltOn = params.filter1Enabled || params.filter2Enabled;
        if (anyFltOn) {
            cachedFlt1Enabled_ = params.filter1Enabled;
            cachedFlt2Enabled_ = params.filter2Enabled;
            if (auto* p = apvts.getParameter("filter1Enabled")) p->setValueNotifyingHost(0.0f);
            if (auto* p = apvts.getParameter("filter2Enabled")) p->setValueNotifyingHost(0.0f);
        } else {
            if (auto* p = apvts.getParameter("filter1Enabled")) p->setValueNotifyingHost(cachedFlt1Enabled_ ? 1.0f : 0.0f);
            if (auto* p = apvts.getParameter("filter2Enabled")) p->setValueNotifyingHost(cachedFlt2Enabled_ ? 1.0f : 0.0f);
            if (!cachedFlt1Enabled_ && !cachedFlt2Enabled_) {
                if (auto* p = apvts.getParameter("filter1Enabled")) p->setValueNotifyingHost(1.0f);
            }
        }
        if (onFilterStateChanged) onFilterStateChanged();
        repaint();
        return;
    }

    // Dual Filter Routing Mode Toggle (SERIAL / PARALLEL)
    if (dualTargetRect_.contains(pos)) {
        if (auto* p = apvts.getParameter("filterRoutingMode")) {
            const float cur = p->convertFrom0to1(p->getValue());
            p->setValueNotifyingHost(cur > 0.5f ? 0.0f : 1.0f);
        }
        if (onFilterStateChanged) onFilterStateChanged();
        repaint();
        return;
    }

    // Individual Filter Bypass Toggles
    if (flt1ToggleRect_.expanded(4).contains(pos)) {
        if (auto* p = apvts.getParameter("filter1Enabled")) {
            p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
        }
        if (onFilterStateChanged) onFilterStateChanged();
        repaint();
        return;
    }

    if (flt2ToggleRect_.expanded(4).contains(pos)) {
        if (auto* p = apvts.getParameter("filter2Enabled")) {
            p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
        }
        if (onFilterStateChanged) onFilterStateChanged();
        repaint();
        return;
    }

    // Individual Delay & Reverb Bypass Toggles
    if (delayToggleRect_.expanded(4).contains(pos)) {
        if (auto* p = apvts.getParameter("delayEnabled")) {
            p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
        }
        repaint();
        return;
    }

    if (reverbToggleRect_.expanded(4).contains(pos)) {
        if (auto* p = apvts.getParameter("reverbMix")) {
            const float cur = p->convertFrom0to1(p->getValue());
            if (cur > 0.01f) {
                cachedReverbMix_ = cur;
                p->setValueNotifyingHost(0.0f);
            } else {
                const float target = cachedReverbMix_ > 0.05f ? cachedReverbMix_ : 0.42f;
                p->setValueNotifyingHost(p->convertTo0to1(target));
            }
        }
        repaint();
        return;
    }

    if (toggleFxRect_.expanded(4).contains(pos)) {
        const auto curP = proc_.getCurrentParameters();
        const bool anyOn = curP.delayEnabled || (curP.reverbMix > 0.01f);
        if (anyOn) {
            cachedDelayEnabled_ = curP.delayEnabled;
            if (curP.reverbMix > 0.01f) cachedReverbMix_ = curP.reverbMix;
            if (auto* p = apvts.getParameter("delayEnabled")) p->setValueNotifyingHost(0.0f);
            if (auto* p = apvts.getParameter("reverbMix"))    p->setValueNotifyingHost(0.0f);
        } else {
            if (auto* p = apvts.getParameter("delayEnabled")) p->setValueNotifyingHost(cachedDelayEnabled_ ? 1.0f : 0.0f);
            if (auto* p = apvts.getParameter("reverbMix")) {
                const float target = cachedReverbMix_ > 0.05f ? cachedReverbMix_ : 0.42f;
                p->setValueNotifyingHost(p->convertTo0to1(target));
            }
        }
        repaint();
        return;
    }

    // Scroll chevron clicks and scroll track clicks
    if (scrollUpRect_.contains(pos) && sourceScrollOffset_ > 0) {
        --sourceScrollOffset_;
        repaint();
        return;
    }
    if (scrollDownRect_.contains(pos) && sourceScrollOffset_ < 2) {
        ++sourceScrollOffset_;
        repaint();
        return;
    }
    if (scrollTrackRect_.contains(pos)) {
        const float clickY = static_cast<float>(pos.y);
        const float trackY = static_cast<float>(scrollTrackRect_.getY());
        const float trackH = std::max(1.0f, static_cast<float>(scrollTrackRect_.getHeight()));
        const float ratio = std::clamp((clickY - trackY) / trackH, 0.0f, 1.0f);
        sourceScrollOffset_ = std::clamp(static_cast<int>(std::round(ratio * 2.0f)), 0, 2);
        repaint();
        return;
    }

    // 2. OSC Concentric Ring Toggle (O)
    for (std::size_t v = 0; v < flowOscRingRects_.size(); ++v) {
        if (flowOscRingRects_[v].expanded(4).contains(pos)) {
            const int realIdx = static_cast<int>(v) + sourceScrollOffset_;
            const juce::String prefix = "osc" + juce::String(realIdx + 1) + "_";
            if (auto* p = apvts.getParameter(prefix + "enabled")) {
                p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
            }
            if (onSourceStateChanged) onSourceStateChanged();
            repaint();
            return;
        }
    }

    // 3. OSC Jack Pin -> START DRAGGING CABLE!
    for (std::size_t v = 0; v < flowOscJackRects_.size(); ++v) {
        if (flowOscJackRects_[v].expanded(6).contains(pos)) {
            const int realIdx = static_cast<int>(v) + sourceScrollOffset_;
            isDraggingCable_ = true;
            dragSourceOscIndex_ = realIdx;
            dragStartPos_ = oscOutPins_[v];
            dragCurrentPos_ = pos.toFloat();
            hoveredTarget_ = -1;
            repaint();
            return;
        }
    }

    // 4. OSC Pill Body Click
    for (std::size_t v = 0; v < flowOscPillRects_.size(); ++v) {
        if (flowOscPillRects_[v].contains(pos)) {
            const int realIdx = static_cast<int>(v) + sourceScrollOffset_;
            if (pos.x > flowOscPillRects_[v].getRight() - 28) {
                isDraggingCable_ = true;
                dragSourceOscIndex_ = realIdx;
                dragStartPos_ = oscOutPins_[v];
                dragCurrentPos_ = pos.toFloat();
                hoveredTarget_ = -1;
            } else {
                selectedOscIndex_ = realIdx;
                if (onSelectOsc) onSelectOsc(realIdx);
            }
            repaint();
            return;
        }
    }

    // 5. Master Fader Interaction
    if (masterFaderTrackRect_.expanded(10).contains(pos) || masterFaderThumbRect_.expanded(4).contains(pos)) {
        isDraggingMasterFader_ = true;
        const float norm = 1.0f - std::clamp(static_cast<float>(pos.y - masterFaderTrackRect_.getY()) / static_cast<float>(masterFaderTrackRect_.getHeight()), 0.0f, 1.0f);
        const float dbVal = -24.0f + norm * 36.0f;
        if (auto* p = apvts.getParameter("masterGainDb")) {
            p->setValueNotifyingHost(p->convertTo0to1(dbVal));
        }
        repaint();
        return;
    }
}

void SignalFlowPanel::handleFlowMouseDrag(const juce::MouseEvent& event) {
    if (isDraggingCable_) {
        dragCurrentPos_ = event.getPosition().toFloat();

        int curRouting = 0;
        if (dragSourceOscIndex_ >= 0) {
            const juce::String prefix = "osc" + juce::String(dragSourceOscIndex_ + 1) + "_";
            if (auto* p = proc_.getAPVTS().getParameter(prefix + "filterRouting")) {
                curRouting = static_cast<int>(p->convertFrom0to1(p->getValue()));
            }
        }

        if (dualTargetRect_.expanded(6).contains(event.getPosition())) {
            hoveredTarget_ = 2; // DUAL (FLT 1 + 2)
        } else if (flt1PillRect_.expanded(8).contains(event.getPosition()) || dragCurrentPos_.getDistanceFrom(flt1InPin_) < 28.0f) {
            hoveredTarget_ = (curRouting == 1) ? 2 : 0;
        } else if (flt2PillRect_.expanded(8).contains(event.getPosition()) || dragCurrentPos_.getDistanceFrom(flt2InPin_) < 28.0f) {
            hoveredTarget_ = (curRouting == 0) ? 2 : 1;
        } else if (cardAmpRect_.expanded(6).contains(event.getPosition()) ||
                   directGutterRect_.expanded(12).contains(event.getPosition()) ||
                   dragCurrentPos_.getDistanceFrom(ampDirectInPin_) < 32.0f) {
            hoveredTarget_ = 3; // DIRECT (into AMP VCA)
        } else {
            hoveredTarget_ = -1;
        }
        repaint();
    } else if (isDraggingMasterFader_) {
        auto& apvts = proc_.getAPVTS();
        const float norm = 1.0f - std::clamp(static_cast<float>(event.y - masterFaderTrackRect_.getY()) / static_cast<float>(masterFaderTrackRect_.getHeight()), 0.0f, 1.0f);
        const float dbVal = -24.0f + norm * 36.0f;
        if (auto* p = apvts.getParameter("masterGainDb")) {
            p->setValueNotifyingHost(p->convertTo0to1(dbVal));
        }
        repaint();
    }
}

void SignalFlowPanel::handleFlowMouseUp(const juce::MouseEvent& event) {
    juce::ignoreUnused(event);
    if (isDraggingCable_) {
        if (dragSourceOscIndex_ >= 0 && hoveredTarget_ >= 0) {
            auto& apvts = proc_.getAPVTS();
            const juce::String prefix = "osc" + juce::String(dragSourceOscIndex_ + 1) + "_";
            if (auto* p = apvts.getParameter(prefix + "filterRouting")) {
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(hoveredTarget_)));
            }
        }
        isDraggingCable_ = false;
        dragSourceOscIndex_ = -1;
        hoveredTarget_ = -1;
        repaint();
    }

    if (isDraggingMasterFader_) {
        isDraggingMasterFader_ = false;
        repaint();
    }
}

} // namespace nova::synth
