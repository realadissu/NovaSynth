#include "JornaSourcesColumn.h"
#include "NovaSynthUiHelpers.h"
#include "../NovaSynthProcessor.h"
#include "nova_dsp/oscillators/MultiEngineOscillator.h"
#include "nova_dsp/synth/WorshipSynthEngine.h"
#include <cmath>
#include <algorithm>
#include <array>

namespace nova::synth {

JornaSourcesColumn::JornaSourcesColumn(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc),
      theme_(theme),
      levelKnob_("LEVEL", "", theme, false),
      posKnob_("MORPH", "", theme, false),
      sprayKnob_("UNISON", "", theme, true),
      panKnob_("PAN", "", theme, true),
      pitchKnob_("OCTAVE", "", theme, false),
      semiKnob_("SEMI", "", theme, false),
      driveKnob_("DRIVE", "", theme, false),
      carveKnob_("CARVE", "", theme, true) {
    addAndMakeVisible(levelKnob_);
    addAndMakeVisible(posKnob_);
    addAndMakeVisible(sprayKnob_);
    addAndMakeVisible(panKnob_);
    addAndMakeVisible(pitchKnob_);
    addAndMakeVisible(semiKnob_);
    addAndMakeVisible(driveKnob_);
    addAndMakeVisible(carveKnob_);

    rebindSelectedOscillator(0);
}

void JornaSourcesColumn::rebindSelectedOscillator(int newIndex) {
    selectedOsc_ = std::clamp(newIndex, 0, 7);
    const auto params = proc_.getCurrentParameters();
    const auto& slot = params.oscSlots[static_cast<std::size_t>(selectedOsc_)];
    const juce::String prefix = "osc" + juce::String(selectedOsc_ + 1) + "_";

    auto& apvts = proc_.getAPVTS();
    levelKnob_.attachToParameter(apvts, prefix + "level");
    panKnob_.attachToParameter(apvts,   prefix + "pan");
    pitchKnob_.attachToParameter(apvts, prefix + "octave");
    semiKnob_.attachToParameter(apvts,  prefix + "semi");

    using ET = nova::dsp::MultiEngineOscillator::EngineType;
    if (slot.sourceType == 1 || slot.engine == ET::SubOscillator) {
        // SUB BASS CONTROLS
        posKnob_.setLabel("DRIVE");
        posKnob_.setSuffix("");
        posKnob_.attachToParameter(apvts, prefix + "shape");

        sprayKnob_.setLabel("WAVE");
        sprayKnob_.setSuffix("");
        sprayKnob_.attachToParameter(apvts, prefix + "model");

        driveKnob_.setLabel("SUB PUNCH");
        driveKnob_.setSuffix("");
        driveKnob_.attachToParameter(apvts, prefix + "detune");

        carveKnob_.setLabel("ROUTING");
        carveKnob_.setSuffix("");
        carveKnob_.attachToParameter(apvts, prefix + "filterRouting");
    } else if (slot.sourceType == 2 || slot.engine == ET::PhysicalPluckModal) {
        // PLUCK / MODAL CONTROLS
        posKnob_.setLabel("BRIGHT");
        posKnob_.setSuffix("");
        posKnob_.attachToParameter(apvts, prefix + "shape");

        sprayKnob_.setLabel("SPREAD");
        sprayKnob_.setSuffix("");
        sprayKnob_.attachToParameter(apvts, prefix + "detune");

        driveKnob_.setLabel("MATERIAL");
        driveKnob_.setSuffix("");
        driveKnob_.attachToParameter(apvts, prefix + "model");

        carveKnob_.setLabel("ROUTING");
        carveKnob_.setSuffix("");
        carveKnob_.attachToParameter(apvts, prefix + "filterRouting");
    } else if (slot.sourceType == 3 || slot.engine == ET::NoiseTransient) {
        // NOISE / ATMOSPHERE CONTROLS
        posKnob_.setLabel("COLOR");
        posKnob_.setSuffix("");
        posKnob_.attachToParameter(apvts, prefix + "shape");

        sprayKnob_.setLabel("WIDTH");
        sprayKnob_.setSuffix("");
        sprayKnob_.attachToParameter(apvts, prefix + "detune");

        driveKnob_.setLabel("FLAVOR");
        driveKnob_.setSuffix("");
        driveKnob_.attachToParameter(apvts, prefix + "model");

        carveKnob_.setLabel("ROUTING");
        carveKnob_.setSuffix("");
        carveKnob_.attachToParameter(apvts, prefix + "filterRouting");
    } else {
        // STANDARD HYBRID OSC CONTROLS
        posKnob_.setLabel("MORPH");
        posKnob_.setSuffix("");
        posKnob_.attachToParameter(apvts, prefix + "shape");

        sprayKnob_.setLabel("UNISON");
        sprayKnob_.setSuffix("");
        sprayKnob_.attachToParameter(apvts, prefix + "detune");

        driveKnob_.setLabel("WARMTH");
        driveKnob_.setSuffix("");
        driveKnob_.attachToParameter(apvts, "macroWarmth");

        carveKnob_.setLabel("ROUTING");
        carveKnob_.setSuffix("");
        carveKnob_.attachToParameter(apvts, prefix + "filterRouting");
    }

    if (onSelectedOscChanged) {
        onSelectedOscChanged(selectedOsc_);
    }
    repaint();
}

void JornaSourcesColumn::resized() {
    std::array<nova::ui::ArcKnob*, 4> row1 = { &levelKnob_, &posKnob_, &sprayKnob_, &panKnob_ };
    std::array<nova::ui::ArcKnob*, 4> row2 = { &pitchKnob_, &semiKnob_, &driveKnob_, &carveKnob_ };

    if (getHeight() < 300) {
        for (auto* k : row1) k->setVisible(false);
        for (auto* k : row2) k->setVisible(false);
        return;
    }

    for (auto* k : row1) k->setVisible(true);
    for (auto* k : row2) k->setVisible(true);

    const int knobsTop = 236;
    const int knobsHeight = std::max(40, getHeight() - knobsTop - 36);
    const int rowH = knobsHeight / 2;
    const int colW = (getWidth() - 16) / 4;

    for (int c = 0; c < 4; ++c) {
        row1[static_cast<std::size_t>(c)]->setBounds(8 + c * colW, knobsTop, colW, rowH);
        row2[static_cast<std::size_t>(c)]->setBounds(8 + c * colW, knobsTop + rowH, colW, rowH);
    }
}

void JornaSourcesColumn::mouseDown(const juce::MouseEvent& event) {
    const auto pos = event.getPosition();
    const int curCount = std::clamp(proc_.getCurrentParameters().activeOscCount, 1, 8);

    // [+] Add Layer
    if (addLayerBtnBounds_.contains(pos) && curCount < 8) {
        if (auto* p = proc_.getAPVTS().getParameter("activeOscCount")) {
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(curCount + 1)));
        }
        const juce::String nextPrefix = "osc" + juce::String(curCount + 1) + "_";
        if (auto* en = proc_.getAPVTS().getParameter(nextPrefix + "enabled")) {
            en->setValueNotifyingHost(1.0f);
        }
        rebindSelectedOscillator(curCount);
        return;
    }

    // [-] Remove Layer
    if (remLayerBtnBounds_.contains(pos) && curCount > 1) {
        if (auto* p = proc_.getAPVTS().getParameter("activeOscCount")) {
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(curCount - 1)));
        }
        rebindSelectedOscillator(std::min(selectedOsc_, curCount - 2));
        return;
    }

    // Check layer list clicks (Toggle Ring, Name Pill, Engine Dropdown, Filter Routing, or Row Selection)
    for (int i = 0; i < static_cast<int>(rowRects_.size()); ++i) {
        const auto idx = static_cast<std::size_t>(i);
        const juce::String prefix = "osc" + juce::String(i + 1) + "_";

        if (idx < toggleRingRects_.size() && toggleRingRects_[idx].expanded(4).contains(pos)) {
            if (auto* en = proc_.getAPVTS().getParameter(prefix + "enabled")) {
                const bool cur = en->getValue() > 0.5f;
                en->setValueNotifyingHost(cur ? 0.0f : 1.0f);
            }
            repaint();
            return;
        }

        if (idx < namePillRects_.size() && namePillRects_[idx].contains(pos)) {
            rebindSelectedOscillator(i);
            openSourceTypeMenuForSlot(i, namePillRects_[idx]);
            return;
        }

        if (idx < engineDropRects_.size() && engineDropRects_[idx].contains(pos)) {
            rebindSelectedOscillator(i);
            openEngineMenuForSlot(i, engineDropRects_[idx]);
            return;
        }

        if (idx < filterRoutingRects_.size() && filterRoutingRects_[idx].contains(pos)) {
            rebindSelectedOscillator(i);
            openFilterRoutingMenuForSlot(i, filterRoutingRects_[idx]);
            return;
        }

        if (rowRects_[idx].contains(pos)) {
            rebindSelectedOscillator(i);
            return;
        }
    }

    // Cycle / Select specific waveform/model for active engine
    if (waveCycleRect_.contains(pos)) {
        openModelMenuForSlot(selectedOsc_, waveCycleRect_, pos);
        return;
    }

    // Dragging Spread Bar
    if (spreadBarRect_.expanded(0, 6).contains(pos)) {
        updateSpreadFromMouse(static_cast<float>(pos.x));
    }
}

void JornaSourcesColumn::mouseDrag(const juce::MouseEvent& event) {
    if (spreadBarRect_.expanded(0, 12).contains(event.getMouseDownPosition())) {
        updateSpreadFromMouse(event.position.x);
    }
}

void JornaSourcesColumn::openSourceTypeMenuForSlot(int slotIdx, juce::Rectangle<int> targetRect) {
    const auto params = proc_.getCurrentParameters();
    const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIdx)];
    const int curType = slot.sourceType;

    juce::PopupMenu m;
    m.addItem(1, "OSC (Hybrid Oscillator: Wavetable / Granular / Analog)", true, curType == 0);
    m.addItem(2, "SUB (Dedicated Sub Bass)", true, curType == 1);
    m.addItem(3, "PLUCK (Karplus-Strong Physical Pluck)", true, curType == 2);
    m.addItem(4, "NOISE (Transient / Pink & Air Noise)", true, curType == 3);
    m.addItem(5, "SAMPLE (Acoustic Sample Attack & Layer)", true, curType == 4);

    const juce::String prefix = "osc" + juce::String(slotIdx + 1) + "_";
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(targetRect)).withStandardItemHeight(28),
                    [this, prefix, slotIdx](int res) {
                        if (res >= 1 && res <= 5) {
                            if (auto* p = proc_.getAPVTS().getParameter(prefix + "sourceType")) {
                                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(res - 1)));
                            }
                            if (auto* pm = proc_.getAPVTS().getParameter(prefix + "model")) {
                                pm->setValueNotifyingHost(0.0f);
                            }
                            rebindSelectedOscillator(slotIdx);
                            repaint();
                        }
                    });
}

void JornaSourcesColumn::openEngineMenuForSlot(int slotIdx, juce::Rectangle<int> targetRect) {
    const auto params = proc_.getCurrentParameters();
    const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIdx)];
    const juce::String prefix = "osc" + juce::String(slotIdx + 1) + "_";

    if (slot.sourceType != 0) {
        openSourceTypeMenuForSlot(slotIdx, targetRect);
        return;
    }

    const int curEng = static_cast<int>(slot.engine);
    juce::PopupMenu m;
    m.addItem(1, "Warm Analog (Saw / Pulse / Unison)", true, curEng == 0);
    m.addItem(2, "Evolving Wavetable (8 Harmonic Frames)", true, curEng == 1);
    m.addItem(3, "Granular Texture (Choir & Air)", true, curEng == 2);
    m.addItem(4, "Sample Playback (Acoustic Layers)", true, curEng == 6);

    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(targetRect)).withStandardItemHeight(28),
                    [this, prefix](int res) {
                        if (res >= 1 && res <= 4) {
                            if (auto* p = proc_.getAPVTS().getParameter(prefix + "engine")) {
                                const float engVal = (res == 4) ? 3.0f : static_cast<float>(res - 1);
                                p->setValueNotifyingHost(p->convertTo0to1(engVal));
                            }
                            if (auto* pm = proc_.getAPVTS().getParameter(prefix + "model")) {
                                pm->setValueNotifyingHost(0.0f);
                            }
                            repaint();
                        }
                    });
}

void JornaSourcesColumn::openModelMenuForSlot(int slotIdx, juce::Rectangle<int> targetRect, juce::Point<int> clickPos) {
    const auto params = proc_.getCurrentParameters();
    const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIdx)];
    const auto models = nova::dsp::MultiEngineOscillator::getModelsForEngine(slot.engine, slot.sourceType);
    if (models.empty()) return;

    const juce::String prefix = "osc" + juce::String(slotIdx + 1) + "_";
    auto* param = proc_.getAPVTS().getParameter(prefix + "model");
    const int curModel = slot.modelIndex;
    const int totalModels = static_cast<int>(models.size());

    // Left arrow click: Cycle previous
    if (clickPos.x < targetRect.getX() + 24) {
        int prevModel = curModel - 1;
        if (prevModel < 0) prevModel = totalModels - 1;
        if (param != nullptr) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(prevModel)));
            proc_.syncParametersFromAPVTS();
        }
        repaint();
        return;
    }

    // Right arrow click: Cycle next
    if (clickPos.x > targetRect.getRight() - 24) {
        int nextModel = (curModel + 1) % totalModels;
        if (param != nullptr) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(nextModel)));
            proc_.syncParametersFromAPVTS();
        }
        repaint();
        return;
    }

    // Middle click: Open popup menu
    juce::PopupMenu m;
    for (int i = 0; i < totalModels; ++i) {
        m.addItem(i + 1, models[static_cast<std::size_t>(i)], true, i == curModel);
    }

    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(targetRect)).withStandardItemHeight(28),
                    [this, prefix, totalModels](int res) {
                        if (res >= 1 && res <= totalModels) {
                            if (auto* p = proc_.getAPVTS().getParameter(prefix + "model")) {
                                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(res - 1)));
                                proc_.syncParametersFromAPVTS();
                            }
                            repaint();
                        }
                    });
}

void JornaSourcesColumn::openFilterRoutingMenuForSlot(int slotIdx, juce::Rectangle<int> targetRect) {
    const auto params = proc_.getCurrentParameters();
    const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIdx)];
    const int curRoute = slot.filterRouting;

    juce::PopupMenu m;
    m.addItem(1, "FLT 1 (Filter 1: Warm Ladder / SVF)", true, curRoute == 0);
    m.addItem(2, "FLT 2 (Filter 2: Vocal Carve Formant)", true, curRoute == 1);
    m.addItem(3, "FLT 1+2 (Filter 1 -> Filter 2 in series)", true, curRoute == 2);
    m.addItem(4, "DIRECT (Bypass Filters directly to Amp)", true, curRoute == 3);

    const juce::String prefix = "osc" + juce::String(slotIdx + 1) + "_";
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(targetRect)).withStandardItemHeight(28),
                    [this, prefix](int res) {
                        if (res >= 1 && res <= 4) {
                            if (auto* p = proc_.getAPVTS().getParameter(prefix + "filterRouting")) {
                                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(res - 1)));
                            }
                            repaint();
                        }
                    });
}

void JornaSourcesColumn::updateSpreadFromMouse(float mouseX) {
    const float norm = std::clamp(
        (mouseX - static_cast<float>(spreadBarRect_.getX())) /
        std::max(1.0f, static_cast<float>(spreadBarRect_.getWidth())),
        0.0f, 1.0f);
    const juce::String prefix = "osc" + juce::String(selectedOsc_ + 1) + "_";
    if (auto* p = proc_.getAPVTS().getParameter(prefix + "detune")) {
        p->setValueNotifyingHost(norm);
    }
    repaint();
}

} // namespace nova::synth
