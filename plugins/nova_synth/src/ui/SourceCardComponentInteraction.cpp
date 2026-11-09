#include "SourceCardComponent.h"
#include <algorithm>

namespace nova::synth {

    void SourceCardComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
        if (auto* vp = findParentComponentOfClass<juce::Viewport>()) {
            vp->mouseWheelMove(e, wheel);
        }
    }

    void SourceCardComponent::mouseExit(const juce::MouseEvent&) {
        if (hoveredWavePill_ != -1) {
            hoveredWavePill_ = -1;
            repaint();
        }
    }

    void SourceCardComponent::mouseMove(const juce::MouseEvent& e) {
        const auto pos = e.getPosition();

        const auto params = proc_.getCurrentParameters();
        const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex_)];
        using ET = nova::dsp::MultiEngineOscillator::EngineType;
        const bool isSample = (slot.sourceType == 4 || slot.engine == ET::SamplePlayback);

        if (isExpanded_ && isSample && visualizerRect_.contains(pos)) {
            const float startX = static_cast<float>(visualizerRect_.getX()) + 4.0f;
            const float w = static_cast<float>(visualizerRect_.getWidth()) - 8.0f;
            if (w > 1.0f) {
                const float currentMorph = static_cast<float>(morphKnob_.getSlider().getValue());
                const float startXLine = startX + currentMorph * w;
                const float loopStartLine = startX + static_cast<float>(loopStartKnob_.getSlider().getValue()) * w;
                const float loopEndLine = startX + static_cast<float>(loopEndKnob_.getSlider().getValue()) * w;
                const float px = static_cast<float>(pos.x);
                if (std::abs(px - startXLine) <= 7.0f || std::abs(px - loopStartLine) <= 7.0f || std::abs(px - loopEndLine) <= 7.0f) {
                    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                    return;
                }
            }
        }

        int newHover = -1;
        for (int i = 0; i < 6; ++i) {
            if (wavePillRects_[static_cast<std::size_t>(i)].contains(pos)) {
                newHover = i;
                break;
            }
        }
        if (newHover != hoveredWavePill_) {
            hoveredWavePill_ = newHover;
            repaint();
        }

        if (pinRect_.contains(pos) || powerRect_.contains(pos) || soloRect_.contains(pos) ||
            engineRect_.contains(pos) || modelRect_.contains(pos) || routingRect_.contains(pos) ||
            ampEnvRect_.contains(pos) || keyTrkRect_.contains(pos) || lowestNoteRect_.contains(pos) || newHover >= 0) {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
            return;
        }
        for (const auto& r : subOctPillRects_) {
            if (r.contains(pos)) {
                setMouseCursor(juce::MouseCursor::PointingHandCursor);
                return;
            }
        }
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    void SourceCardComponent::mouseDrag(const juce::MouseEvent& e) {
        if (activeDragMarker_ == DragMarker::None) return;
        const float startX = static_cast<float>(visualizerRect_.getX()) + 4.0f;
        const float w = static_cast<float>(visualizerRect_.getWidth()) - 8.0f;
        if (w <= 1.0f) return;

        const float normX = std::clamp((static_cast<float>(e.x) - startX) / w, 0.0f, 1.0f);
        if (activeDragMarker_ == DragMarker::Start) {
            morphKnob_.getSlider().setValue(normX, juce::sendNotificationSync);
        } else if (activeDragMarker_ == DragMarker::LoopStart) {
            const double endVal = loopEndKnob_.getSlider().getValue();
            loopStartKnob_.getSlider().setValue(std::min(static_cast<double>(normX), endVal - 0.01), juce::sendNotificationSync);
        } else if (activeDragMarker_ == DragMarker::LoopEnd) {
            const double startVal = loopStartKnob_.getSlider().getValue();
            loopEndKnob_.getSlider().setValue(std::max(static_cast<double>(normX), startVal + 0.01), juce::sendNotificationSync);
        }
        repaint();
    }

    void SourceCardComponent::mouseUp(const juce::MouseEvent&) {
        activeDragMarker_ = DragMarker::None;
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    void SourceCardComponent::mouseDown(const juce::MouseEvent& e) {
        if (e.eventComponent != this) return;
        const auto pos = e.getPosition();

        // 1. Power Toggle (funktioniert kompakt & expandiert; expandiert/kollabiert NICHT!)
        if (powerRect_.contains(pos)) {
            const auto params = proc_.getCurrentParameters();
            const bool curState = params.oscSlots[static_cast<std::size_t>(slotIndex_)].enabled;
            const juce::String prefix = "osc" + juce::String(slotIndex_ + 1) + "_";
            if (auto* p = proc_.getAPVTS().getParameter(prefix + "enabled")) {
                p->setValueNotifyingHost(curState ? 0.0f : 1.0f);
            }
            if (onCardStateChanged) onCardStateChanged(slotIndex_);
            repaint();
            return;
        }

        // 2. Solo Button (funktioniert kompakt & expandiert; expandiert/kollabiert NICHT!)
        if (soloRect_.contains(pos)) {
            const bool curSolo = proc_.isSlotSoloed(slotIndex_);
            proc_.setSlotSolo(slotIndex_, !curSolo);
            if (onCardStateChanged) onCardStateChanged(slotIndex_);
            repaint();
            return;
        }

        // 2b. Envelope Pill (funktioniert sowohl im kompakten als auch im expandierten Modus!)
        if (ampEnvRect_.contains(pos)) {
            openAmpEnvMenu();
            return;
        }

        // 3. Kompakter Zustand: Klick auf Slot Badge oder Karte expandiert!
        if (!isExpanded_) {
            if (onExpandRequested) onExpandRequested(slotIndex_, true);
            return;
        }

        // 4. Pin-Button (nur wenn expandiert)
        if (pinRect_.contains(pos)) {
            isPinned_ = !isPinned_;
            if (onPinToggled) onPinToggled(slotIndex_, isPinned_);
            if (!isPinned_) {
                setExpanded(false);
            }
            repaint();
            return;
        }

        // 5. Header-Klick auf expandierte Karte (y <= 24, z.B. auf Slot Badge "OSC A" oder freien Header):
        // Wenn nicht gepinnt, einklappen!
        // Nicht auslösen bei Klick auf Engine Dropdown!
        if (pos.y <= 24 && !engineRect_.contains(pos)) {
            if (!isPinned_) {
                if (onExpandRequested) onExpandRequested(slotIndex_, false);
            }
            return;
        }

        // 6. Engine Dropdown
        if (engineRect_.contains(pos)) {
            openEngineMenu();
            return;
        }

        // 7. Routing Dropdown
        if (routingRect_.contains(pos)) {
            openRoutingMenu();
            return;
        }

        // 7b. Amp Env Dropdown
        if (ampEnvRect_.contains(pos)) {
            openAmpEnvMenu();
            return;
        }

        // 7c. Sample Waveform Visualizer Markers (START, LOOP START, LOOP END)
        const auto params = proc_.getCurrentParameters();
        const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex_)];
        using ET = nova::dsp::MultiEngineOscillator::EngineType;
        const bool isSample = (slot.sourceType == 4 || slot.engine == ET::SamplePlayback);

        if (isExpanded_ && isSample && visualizerRect_.contains(pos)) {
            const float startX = static_cast<float>(visualizerRect_.getX()) + 4.0f;
            const float w = static_cast<float>(visualizerRect_.getWidth()) - 8.0f;
            if (w > 1.0f) {
                const float currentMorph = static_cast<float>(morphKnob_.getSlider().getValue());
                const float startXLine = startX + currentMorph * w;
                const float loopStartLine = startX + static_cast<float>(loopStartKnob_.getSlider().getValue()) * w;
                const float loopEndLine = startX + static_cast<float>(loopEndKnob_.getSlider().getValue()) * w;
                const float px = static_cast<float>(pos.x);

                if (std::abs(px - startXLine) <= 8.0f) {
                    activeDragMarker_ = DragMarker::Start;
                } else if (std::abs(px - loopStartLine) <= 8.0f) {
                    activeDragMarker_ = DragMarker::LoopStart;
                } else if (std::abs(px - loopEndLine) <= 8.0f) {
                    activeDragMarker_ = DragMarker::LoopEnd;
                } else {
                    activeDragMarker_ = DragMarker::Start;
                    const float normX = std::clamp((px - startX) / w, 0.0f, 1.0f);
                    morphKnob_.getSlider().setValue(normX, juce::sendNotificationSync);
                }
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                repaint();
                return;
            }
        }

        // 8. Model Stepper < und >
        if (modelPrevRect_.contains(pos)) {
            cycleModel(-1);
            return;
        }
        if (modelNextRect_.contains(pos)) {
            cycleModel(1);
            return;
        }
        if (modelRect_.contains(pos)) {
            openModelMenu();
            return;
        }

        // 9. KeyTrk Toggle (Granular)
        if (keyTrkRect_.contains(pos)) {
            const juce::String prefix = "osc" + juce::String(slotIndex_ + 1) + "_";
            if (auto* p = proc_.getAPVTS().getParameter(prefix + "lowestNoteOnly")) {
                const bool cur = (p->getValue() > 0.5f);
                p->setValueNotifyingHost(cur ? 0.0f : 1.0f);
            }
            repaint();
            return;
        }

        // 10. Lowest Note Only Toggle (Sub)
        if (lowestNoteRect_.contains(pos)) {
            const juce::String prefix = "osc" + juce::String(slotIndex_ + 1) + "_";
            if (auto* p = proc_.getAPVTS().getParameter(prefix + "lowestNoteOnly")) {
                const bool cur = (p->getValue() > 0.5f);
                p->setValueNotifyingHost(cur ? 0.0f : 1.0f);
            }
            repaint();
            return;
        }

        // 11. Waveform Icon Pills (Analog, Sub, Noise) OR Loop Mode Pills (Sample)
        for (int i = 0; i < 6; ++i) {
            if (wavePillRects_[static_cast<std::size_t>(i)].contains(pos)) {
                const juce::String prefix = "osc" + juce::String(slotIndex_ + 1) + "_";
                if (isSample) {
                    if (auto* p = proc_.getAPVTS().getParameter(prefix + "loopMode")) {
                        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(i)));
                        proc_.syncParametersFromAPVTS();
                    }
                } else {
                    if (auto* p = proc_.getAPVTS().getParameter(prefix + "model")) {
                        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(i)));
                        proc_.syncParametersFromAPVTS();
                    }
                }
                repaint();
                return;
            }
        }

        // 12. Sub Octave Stepper Pills [-1, -2, -3]
        for (int i = 0; i < 3; ++i) {
            if (subOctPillRects_[static_cast<std::size_t>(i)].contains(pos)) {
                const float octVal = -1.0f - static_cast<float>(i);
                const juce::String prefix = "osc" + juce::String(slotIndex_ + 1) + "_";
                if (auto* p = proc_.getAPVTS().getParameter(prefix + "octave")) {
                    p->setValueNotifyingHost(p->convertTo0to1(octVal));
                }
                repaint();
                return;
            }
        }
    }

    void SourceCardComponent::cycleModel(int delta) {
        const auto params = proc_.getCurrentParameters();
        const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex_)];
        const auto models = nova::dsp::MultiEngineOscillator::getModelsForEngine(slot.engine, slot.sourceType);
        if (models.empty()) return;
        const int count = static_cast<int>(models.size());
        const int nextIdx = (slot.modelIndex + delta + count) % count;
        const juce::String prefix = "osc" + juce::String(slotIndex_ + 1) + "_";
        if (auto* p = proc_.getAPVTS().getParameter(prefix + "model")) {
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(nextIdx)));
            proc_.syncParametersFromAPVTS();
        }
        repaint();
    }

    void SourceCardComponent::openRoutingMenu() {
        const auto params = proc_.getCurrentParameters();
        const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex_)];
        juce::PopupMenu m;
        m.addItem(1, "FLT 1 (Filter 1)", true, slot.filterRouting == 0);
        m.addItem(2, "FLT 2 (Filter 2)", true, slot.filterRouting == 1);
        m.addItem(3, "FLT 1+2 (Dual Series)", true, slot.filterRouting == 2);
        m.addItem(4, "DIRECT (Bypass Filters)", true, slot.filterRouting == 3);

        const juce::String prefix = "osc" + juce::String(slotIndex_ + 1) + "_";
        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(routingRect_)).withStandardItemHeight(28),
            [this, prefix](int res) {
                if (res >= 1 && res <= 4) {
                    if (auto* p = proc_.getAPVTS().getParameter(prefix + "filterRouting")) {
                        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(res - 1)));
                    }
                    repaint();
                }
            });
    }

    void SourceCardComponent::openAmpEnvMenu() {
        const auto params = proc_.getCurrentParameters();
        const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex_)];
        juce::PopupMenu m;
        m.addItem(1, "Amp Env 1 (Main Voice)", true, slot.ampEnvSource == 0);
        m.addItem(2, "Filter Env 2",           true, slot.ampEnvSource == 1);
        m.addItem(3, "Mod Env 3 (Swell/Pad)",  true, slot.ampEnvSource == 2);
        m.addItem(4, "Aux Env 4 (Pitch/Fast)", true, slot.ampEnvSource == 3);

        const juce::String prefix = "osc" + juce::String(slotIndex_ + 1) + "_";
        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(ampEnvRect_)).withStandardItemHeight(28),
            [this, prefix](int res) {
                if (res >= 1 && res <= 4) {
                    if (auto* p = proc_.getAPVTS().getParameter(prefix + "ampEnvSource")) {
                        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(res - 1)));
                    }
                    proc_.syncParametersFromAPVTS();
                    repaint();
                }
            });
    }

    juce::String SourceCardComponent::getTooltip() {
        const auto mousePos = getMouseXYRelative();
        if (ampEnvRect_.contains(mousePos)) {
            return "Envelope Routing: Click to route slot to Amp Env 1, Filter Env 2, Mod Env 3, or Aux Env 4";
        }
        if (hoveredWavePill_ >= 0 && hoveredWavePill_ < 6) {
            const auto params = proc_.getCurrentParameters();
            const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex_)];
            const auto models = nova::dsp::MultiEngineOscillator::getModelsForEngine(slot.engine, slot.sourceType);
            const int count = static_cast<int>(models.size());
            if (hoveredWavePill_ < count) {
                return juce::String(models[static_cast<std::size_t>(hoveredWavePill_)]);
            }
        }
        return {};
    }

} // namespace nova::synth
