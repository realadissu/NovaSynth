#include "NovaSynthEditor.h"
#include "ui/SourceListManager.h"
#include "ui/DualVisualizerHub.h"
#include "ui/SourceDeck.h"
#include "ui/DualFilterDeck.h"
#include "ui/EnvelopeQuadDeck.h"
#include "ui/StandaloneKeyboardBar.h"
#include "ui/ModulationHubPanel.h"
#include "ui/DroneHubPanel.h"
#include "ui/ArpeggiatorHubPanel.h"
#include "ui/MixerHubPanel.h"
#include "MacDockIcon.h"
#include "BinaryData.h"
#include <cmath>
#include <algorithm>
#include <array>
#include <numbers>

namespace nova::synth {

namespace {

// Signature JORNA layer colors for the 8 Oscillator Sources
juce::Colour getLayerColour(int index) noexcept {
    static constexpr std::array<uint32_t, 8> kColours = {
        0xff38bdf8, // OSC A: Cyan
        0xff2dd4bf, // OSC B: Teal
        0xffec4899, // OSC C: Pink / Magenta
        0xffa855f7, // OSC D: Purple
        0xff84cc16, // OSC E: Lime Green
        0xfff59e0b, // OSC F: Amber Gold
        0xfff43f5e, // OSC G: Coral Red
        0xff60a5fa  // OSC H: Sky Blue
    };
    return juce::Colour(kColours[static_cast<std::size_t>(std::clamp(index, 0, 7))]);
}

juce::String getLayerShortName(const nova::dsp::WorshipSynthEngine::Parameters& params, int slotIndex) {
    const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex)];
    if (slot.sourceType == 1) return "SUB";
    if (slot.sourceType == 2) return "PLUCK";
    if (slot.sourceType == 3) return "NOISE";
    if (slot.sourceType == 4) return "SAMPLE";

    int oscIdx = 0;
    for (int i = 0; i < slotIndex; ++i) {
        if (params.oscSlots[static_cast<std::size_t>(i)].sourceType == 0) {
            oscIdx++;
        }
    }
    const char letter = static_cast<char>('A' + std::clamp(oscIdx, 0, 25));
    return juce::String("OSC ") + juce::String::charToString(letter);
}

juce::String getEngineDisplayName(const nova::dsp::WorshipSynthEngine::Parameters& params, int slotIndex) {
    const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex)];
    if (slot.sourceType == 1) return "SUB OSC";
    if (slot.sourceType == 2) return "KARPLUS PLUCK";
    if (slot.sourceType == 3) return "PINK / AIR";
    if (slot.sourceType == 4) return "SAMPLE LAYER";

    using ET = nova::dsp::MultiEngineOscillator::EngineType;
    switch (slot.engine) {
        case ET::WarmAnalog:        return "WARM ANALOG";
        case ET::EvolvingWavetable: return "WAVETABLE";
        case ET::OrganicTexture:    return "GRANULAR";
        case ET::PhysicalPluckModal:return "KARPLUS PLUCK";
        case ET::SubOscillator:     return "SUB OSC";
        case ET::NoiseTransient:    return "PINK / AIR";
        case ET::SamplePlayback:    return "SAMPLE LAYER";
    }
    return "WAVETABLE";
}

juce::String getModelDisplayName(const nova::dsp::WorshipSynthEngine::Parameters& params, int slotIndex) {
    if (slotIndex < 0 || slotIndex >= params.activeOscCount) return "Classic Saw";
    const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex)];
    return nova::dsp::MultiEngineOscillator::getModelName(slot.engine, slot.sourceType, slot.modelIndex);
}

juce::String getFilterRoutingDisplayName(int filterRouting) {
    switch (filterRouting) {
        case 0: return "FLT 1";
        case 1: return "FLT 2";
        case 2: return "FLT 1+2";
        case 3: return "DIRECT";
        default: return "FLT 1";
    }
}

// ==============================================================================
// 1. LEFT COLUMN: JORNA "SOURCES" Multi-Layer List + Selected Source Inspector
// ==============================================================================
class JornaSourcesColumn : public juce::Component {
public:
    std::function<void(int)> onSelectedOscChanged;

    JornaSourcesColumn(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
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

    [[nodiscard]] int getSelectedOscIndex() const noexcept { return selectedOsc_; }

    void rebindSelectedOscillator(int newIndex) {
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

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(theme_.bgSurface);
        g.fillRoundedRectangle(bounds, theme_.cornerRadius);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(bounds, theme_.cornerRadius, 1.0f);

        const auto params = proc_.getCurrentParameters();
        const int activeCount = std::clamp(params.activeOscCount, 1, 8);
        if (selectedOsc_ >= activeCount) {
            selectedOsc_ = activeCount - 1;
        }

        // --- Header: SOURCES  N LAYERS   [+] [-]   OUT ---
        g.setFont(theme_.getSectionFont(14.5f));
        g.setColour(juce::Colour(0xff38bdf8));
        g.drawText("SOURCES", 12, 8, 70, 18, juce::Justification::centredLeft);

        g.setFont(theme_.getLabelFont(12.0f));
        g.setColour(theme_.textMuted);
        g.drawText(juce::String(activeCount) + " LAYERS", 82, 8, 65, 18, juce::Justification::centredLeft);

        // Add (+) and Remove (-) Layer pills in header
        addLayerBtnBounds_ = juce::Rectangle<int>(getWidth() - 96, 7, 24, 18);
        remLayerBtnBounds_ = juce::Rectangle<int>(getWidth() - 68, 7, 24, 18);

        g.setColour(theme_.bgElevated);
        g.fillRoundedRectangle(addLayerBtnBounds_.toFloat(), 4.0f);
        g.fillRoundedRectangle(remLayerBtnBounds_.toFloat(), 4.0f);
        g.setColour(theme_.borderFocus);
        g.drawRoundedRectangle(addLayerBtnBounds_.toFloat(), 4.0f, 1.0f);
        g.drawRoundedRectangle(remLayerBtnBounds_.toFloat(), 4.0f, 1.0f);

        g.setFont(theme_.getSectionFont(13.5f));
        g.setColour(activeCount < 8 ? juce::Colour(0xff38bdf8) : theme_.textMuted);
        g.drawText("+", addLayerBtnBounds_, juce::Justification::centred);
        g.setColour(activeCount > 1 ? juce::Colour(0xfff43f5e) : theme_.textMuted);
        g.drawText("-", remLayerBtnBounds_, juce::Justification::centred);

        g.setFont(theme_.getLabelFont(12.0f));
        g.setColour(theme_.textMuted);
        g.drawText("OUT", getWidth() - 40, 8, 30, 18, juce::Justification::centredRight);

        // --- Vertical Layer List (up to 8 layers) ---
        const int listTop = 30;
        const int listBottom = (getHeight() >= 300) ? 186 : (getHeight() - 6);
        const int rowH = std::clamp((listBottom - listTop) / std::max(1, activeCount), 16, 26);

        rowRects_.clear();
        toggleRingRects_.clear();
        namePillRects_.clear();
        engineDropRects_.clear();
        filterRoutingRects_.clear();

        for (int i = 0; i < activeCount; ++i) {
            const int ry = listTop + i * rowH;
            const juce::Rectangle<int> rowR(8, ry, getWidth() - 16, rowH - 2);
            rowRects_.push_back(rowR);

            const auto layerCol = getLayerColour(i);
            const bool isSel = (i == selectedOsc_);
            const juce::String prefix = "osc" + juce::String(i + 1) + "_";
            const bool isOn = proc_.getAPVTS().getRawParameterValue(prefix + "enabled")->load() > 0.5f;
            const auto& slotP = params.oscSlots[static_cast<std::size_t>(i)];

            // Row background + colored selection outline (just like JORNA's OSC C row!)
            g.setColour(isSel ? layerCol.withAlpha(0.13f) : theme_.bgControl.withAlpha(0.75f));
            g.fillRoundedRectangle(rowR.toFloat(), 4.0f);
            if (isSel) {
                g.setColour(layerCol.withAlpha(0.85f));
                g.drawRoundedRectangle(rowR.toFloat(), 4.0f, 1.3f);
            }

            // Left On/Off Ring Button (O)
            const juce::Rectangle<int> ringR(rowR.getX() + 6, rowR.getCentreY() - 5, 10, 10);
            toggleRingRects_.push_back(ringR);
            g.setColour(isOn ? layerCol : theme_.textMuted.withAlpha(0.35f));
            g.drawEllipse(ringR.toFloat(), 2.0f);
            if (isOn) {
                g.fillEllipse(ringR.toFloat().reduced(2.5f));
            }

            // Dynamically proportioned pills to prevent overlap on any width
            const int totalW = rowR.getWidth();
            const int nameW = std::clamp(totalW * 28 / 100, 44, 58);
            const int fltW = 44;
            const int engW = std::max(38, totalW - (20 + nameW + fltW + 10));

            // Layer Name Pill (e.g. OSC A, SUB, PLUCK, NOISE)
            const juce::Rectangle<int> nameR(rowR.getX() + 18, rowR.getY() + 1, nameW, rowR.getHeight() - 2);
            namePillRects_.push_back(nameR);
            g.setFont(theme_.getSectionFont(12.5f));
            g.setColour(isOn ? layerCol : theme_.textMuted);
            g.drawText(getLayerShortName(params, i), nameR, juce::Justification::centredLeft);

            // Engine Dropdown Pill
            const juce::Rectangle<int> engR(rowR.getX() + 18 + nameW + 4, rowR.getY() + 2, engW, rowR.getHeight() - 4);
            engineDropRects_.push_back(engR);
            g.setColour(theme_.bgElevated);
            g.fillRoundedRectangle(engR.toFloat(), 3.0f);
            g.setFont(theme_.getLabelFont(11.0f));
            g.setColour(isOn ? theme_.textPrimary : theme_.textMuted);
            g.drawText(getEngineDisplayName(params, i) + " v", engR.reduced(3, 0),
                       juce::Justification::centredLeft);

            // Filter Target Pill (FLT 1, FLT 2, FLT 1+2, DIRECT)
            const juce::Rectangle<int> fltR(rowR.getRight() - fltW, rowR.getY() + 2, fltW, rowR.getHeight() - 4);
            filterRoutingRects_.push_back(fltR);
            g.setColour(theme_.bgElevated);
            g.fillRoundedRectangle(fltR.toFloat(), 3.0f);
            g.setColour(isOn ? juce::Colour(0xff38bdf8) : theme_.textMuted);
            g.setFont(theme_.getLabelFont(10.5f));
            g.drawText(getFilterRoutingDisplayName(slotP.filterRouting), fltR, juce::Justification::centred);
        }

        if (getHeight() >= 300) {
            // --- Divider & Selected Oscillator Detail Header ---
            const int detailTop = 192;
            g.setColour(theme_.borderSubtle);
            g.drawHorizontalLine(detailTop - 3, 8.0f, static_cast<float>(getWidth() - 8));

            const auto selCol = getLayerColour(selectedOsc_);
            const auto& selSlot = params.oscSlots[static_cast<std::size_t>(selectedOsc_)];

            g.setFont(theme_.getSectionFont(14.5f));
            g.setColour(selCol);
            g.drawText(getLayerShortName(params, selectedOsc_), 12, detailTop, 60, 18, juce::Justification::centredLeft);

            g.setFont(theme_.getLabelFont(11.5f));
            g.setColour(theme_.textMuted);
            juce::String subDesc = "WAVETABLE / ANALOG / GRANULAR / PLUCK";
            using ET = nova::dsp::MultiEngineOscillator::EngineType;
            if (selSlot.sourceType == 1 || selSlot.engine == ET::SubOscillator) {
                subDesc = "DEDICATED SUB BASS GENERATOR";
            } else if (selSlot.sourceType == 2 || selSlot.engine == ET::PhysicalPluckModal) {
                subDesc = "KARPLUS-STRONG PHYSICAL PLUCK";
            } else if (selSlot.sourceType == 3 || selSlot.engine == ET::NoiseTransient) {
                subDesc = "ATMOSPHERIC TEXTURE & VINYL DUST";
            }
            g.drawText(subDesc, 68, detailTop, getWidth() - 78, 18, juce::Justification::centredRight);

            // Waveform / Preset Sub-Bar (< WARM ANALOG / WAVETABLE > across full width)
            waveCycleRect_ = juce::Rectangle<int>(10, detailTop + 20, getWidth() - 20, 22);
            g.setColour(theme_.bgControl);
            g.fillRoundedRectangle(waveCycleRect_.toFloat(), 4.0f);
            g.setColour(theme_.borderSubtle);
            g.drawRoundedRectangle(waveCycleRect_.toFloat(), 4.0f, 1.0f);
            g.setFont(theme_.getSectionFont(12.5f));
            g.setColour(theme_.textPrimary);
            g.drawText("<  " + getModelDisplayName(params, selectedOsc_).toUpperCase() + "  >", waveCycleRect_,
                       juce::Justification::centred);

            // --- Bottom SPREAD 100% Bar (JORNA green horizontal bar) ---
            spreadBarRect_ = juce::Rectangle<int>(12, getHeight() - 24, getWidth() - 24, 14);
            const float spreadNorm = std::clamp(selSlot.detune, 0.0f, 1.0f);

            g.setFont(theme_.getLabelFont(11.5f));
            g.setColour(theme_.textSecondary);
            g.drawText("SPREAD", spreadBarRect_.getX(), spreadBarRect_.getY() - 12, 60, 12,
                       juce::Justification::centredLeft);
            g.setColour(juce::Colour(0xff22c55e));
            g.drawText(juce::String(static_cast<int>(spreadNorm * 100.0f)) + "%",
                       spreadBarRect_.getRight() - 45, spreadBarRect_.getY() - 12, 45, 12,
                       juce::Justification::centredRight);

            const auto barTrack = spreadBarRect_.withTrimmedTop(6).withHeight(5).toFloat();
            g.setColour(theme_.bgControl);
            g.fillRoundedRectangle(barTrack, 2.5f);
            g.setColour(juce::Colour(0xff22c55e));
            g.fillRoundedRectangle(barTrack.withWidth(barTrack.getWidth() * spreadNorm), 2.5f);
        } else {
            waveCycleRect_ = {};
            spreadBarRect_ = {};
        }
    }

    void resized() override {
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

    void mouseDown(const juce::MouseEvent& event) override {
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

    void mouseDrag(const juce::MouseEvent& event) override {
        if (spreadBarRect_.expanded(0, 12).contains(event.getMouseDownPosition())) {
            updateSpreadFromMouse(event.position.x);
        }
    }

private:
    void openSourceTypeMenuForSlot(int slotIdx, juce::Rectangle<int> targetRect) {
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

    void openEngineMenuForSlot(int slotIdx, juce::Rectangle<int> targetRect) {
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

    void openModelMenuForSlot(int slotIdx, juce::Rectangle<int> targetRect, juce::Point<int> clickPos) {
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

    void openFilterRoutingMenuForSlot(int slotIdx, juce::Rectangle<int> targetRect) {
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

    void updateSpreadFromMouse(float mouseX) {
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

    [[maybe_unused]] NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int selectedOsc_ = 0;

    std::vector<juce::Rectangle<int>> rowRects_;
    std::vector<juce::Rectangle<int>> toggleRingRects_;
    std::vector<juce::Rectangle<int>> namePillRects_;
    std::vector<juce::Rectangle<int>> engineDropRects_;
    std::vector<juce::Rectangle<int>> filterRoutingRects_;
    juce::Rectangle<int> addLayerBtnBounds_;
    juce::Rectangle<int> remLayerBtnBounds_;
    juce::Rectangle<int> waveCycleRect_;
    juce::Rectangle<int> spreadBarRect_;

    nova::ui::ArcKnob levelKnob_;
    nova::ui::ArcKnob posKnob_;
    nova::ui::ArcKnob sprayKnob_;
    nova::ui::ArcKnob panKnob_;
    nova::ui::ArcKnob pitchKnob_;
    nova::ui::ArcKnob semiKnob_;
    nova::ui::ArcKnob driveKnob_;
    nova::ui::ArcKnob carveKnob_;
};

static float calculateOscSample(const nova::dsp::MultiEngineOscillator::Parameters& slot, float phaseNorm) noexcept {
    using ET = nova::dsp::MultiEngineOscillator::EngineType;
    const float cyc = std::fmod(std::abs(phaseNorm), 1.0f);
    const float morph = slot.shapeMorph;

    if (slot.sourceType == 1 || slot.engine == ET::SubOscillator) {
        const float sinWave = std::sin(cyc * 6.283185f);
        const float triWave = 2.0f * std::abs(2.0f * (cyc - 0.5f)) - 1.0f;
        const float sqrWave = (cyc < 0.5f ? 0.9f : -0.9f);
        switch (slot.modelIndex) {
            case 0: return sinWave;
            case 1: return (1.0f - morph * 0.5f) * triWave + (morph * 0.5f) * sinWave;
            case 2: return std::tanh(sinWave * (1.8f + morph * 2.0f)) * 0.9f;
            case 3: return sqrWave;
            default: return sinWave;
        }
    }
    if (slot.sourceType == 2 || slot.engine == ET::PhysicalPluckModal) {
        return std::exp(-cyc * (2.4f + morph * 3.0f)) * std::sin(cyc * 6.283185f * (1.0f + morph * 2.0f));
    }
    if (slot.sourceType == 3 || slot.engine == ET::NoiseTransient) {
        return 0.75f * std::sin(cyc * 37.0f) * std::cos(cyc * 21.0f) * std::exp(-cyc * 3.5f);
    }
    if (slot.engine == ET::WarmAnalog) {
        const float saw = 1.0f - 2.0f * cyc;
        const float pulse = (cyc < (0.25f + 0.5f * morph)) ? 0.9f : -0.9f;
        const float tri = 2.0f * std::abs(2.0f * (cyc - 0.5f)) - 1.0f;
        const float sinW = std::sin(cyc * 6.283185f);
        switch (slot.modelIndex) {
            case 0: return saw;
            case 1: return pulse;
            case 2: return tri;
            case 3: return sinW;
            case 4: return 0.65f * saw + 0.35f * (1.0f - 2.0f * std::fmod(cyc * 2.0f, 1.0f));
            case 5: return 0.85f * (1.0f - 2.0f * std::fmod(cyc * (1.5f + morph * 2.0f), 1.0f));
            default: return (1.0f - morph) * saw + morph * pulse;
        }
    }

    // Wavetable / Organic Texture
    const float harm = 1.0f + static_cast<float>(slot.modelIndex);
    const float w1 = std::sin(cyc * 6.283185f);
    const float w2 = 0.45f * std::sin(cyc * 6.283185f * harm + morph * 3.14159f);
    const float w3 = 0.25f * std::sin(cyc * 18.84955f);
    return (w1 + w2 + w3) / 1.70f;
}

// ==============================================================================
// 2. CENTER COLUMN: JORNA Top "SPECTRUM OUTPUT" + Large "OSC WAVE / PARTICLE" Stage
// ==============================================================================
class JornaCenterStageColumn : public juce::Component {
public:
    std::function<void(int)> onSelectOsc;

    JornaCenterStageColumn(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc), theme_(theme) {
        historyWave_.fill(0.0f);
        spectrumBins_.fill(0.02f);
    }

    void setSelectedOscIndex(int idx) noexcept {
        selectedOsc_ = std::clamp(idx, 0, 7);
        repaint();
    }

    void pullAudioForVisualizers() {
        std::array<float, 256> incoming{};
        const auto popped = proc_.popVisualizerSamples(incoming.data(), incoming.size());
        if (popped > 0) {
            for (std::size_t i = 0; i < popped; ++i) {
                historyWave_[writeCursor_] = incoming[i];
                writeCursor_ = (writeCursor_ + 1) & (historyWave_.size() - 1);
            }
        }

        // Smoothly update 32-band pseudo-spectral analyzer from recent waveform energy & harmonic profile
        const auto params = proc_.getCurrentParameters();
        const auto tel = proc_.getSynthTelemetry();
        const float outEnergy = std::clamp((tel.outputPeakL + tel.outputPeakR) * 1.4f, 0.0f, 1.0f);
        const float cutoffNorm = std::clamp(std::log10(params.filterCutoffHz / 20.0f) / 3.0f, 0.1f, 1.0f);

        for (std::size_t b = 0; b < spectrumBins_.size(); ++b) {
            const float normFreq = static_cast<float>(b) / static_cast<float>(spectrumBins_.size() - 1);
            const float rolloff = (normFreq > cutoffNorm)
                ? std::exp(-6.0f * (normFreq - cutoffNorm))
                : 1.0f;
            const float ripple = 0.55f + 0.45f * std::sin(static_cast<float>(b) * 1.7f + animPhase_ * 2.2f);
            const float target = outEnergy * rolloff * ripple * (1.0f - 0.35f * normFreq);
            spectrumBins_[b] = spectrumBins_[b] * 0.78f + std::max(0.02f, target) * 0.22f;
        }

        animPhase_ += 0.09f;
    }

    void paint(juce::Graphics& g) override {
        pullAudioForVisualizers();

        const auto full = getLocalBounds();
        const auto specRect = full.withHeight(88).toFloat().reduced(1.0f);
        const auto stageRect = full.withTrimmedTop(96).toFloat().reduced(1.0f);
        const auto params = proc_.getCurrentParameters();
        const auto& slotP = params.oscSlots[static_cast<std::size_t>(selectedOsc_)];

        // ------------------------------------------------------------------
        // A. TOP PANEL: "SPECTRUM OUTPUT" (WAVE | SPEC)
        // ------------------------------------------------------------------
        g.setColour(theme_.bgSurface);
        g.fillRoundedRectangle(specRect, theme_.cornerRadius);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(specRect, theme_.cornerRadius, 1.0f);

        g.setFont(theme_.getSectionFont(13.0f));
        g.setColour(juce::Colour(0xff38bdf8));
        g.drawText("SPECTRUM", static_cast<int>(specRect.getX() + 12), static_cast<int>(specRect.getY() + 6),
                   72, 16, juce::Justification::centredLeft);
        g.setFont(theme_.getLabelFont(11.0f));
        g.setColour(theme_.textMuted);
        g.drawText("OUTPUT", static_cast<int>(specRect.getX() + 84), static_cast<int>(specRect.getY() + 6),
                   55, 16, juce::Justification::centredLeft);

        // WAVE | SPEC toggle text
        specModeToggleBounds_ = juce::Rectangle<int>(
            static_cast<int>(specRect.getCentreX() + 20.0f), static_cast<int>(specRect.getY() + 6.0f), 80, 16);
        g.setFont(theme_.getSectionFont(11.0f));
        g.setColour(!showSpectrumMode_ ? juce::Colour(0xff38bdf8) : theme_.textMuted);
        g.drawText("WAVE", specModeToggleBounds_.withWidth(38), juce::Justification::centred);
        g.setColour(showSpectrumMode_ ? juce::Colour(0xff38bdf8) : theme_.textMuted);
        g.drawText("SPEC", specModeToggleBounds_.withTrimmedLeft(40), juce::Justification::centred);

        // Frequency axis labels: 20  100  500  1k  5k  10k  20k
        static constexpr std::array<const char*, 7> kFreqLabels = { "20", "100", "500", "1k", "5k", "10k", "20k" };
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(theme_.textMuted);
        const float plotLeft = specRect.getX() + 14.0f;
        const float plotRight = specRect.getRight() - 14.0f;
        for (int i = 0; i < 7; ++i) {
            const float fx = plotLeft + (static_cast<float>(i) / 6.0f) * (plotRight - plotLeft);
            g.drawText(kFreqLabels[static_cast<std::size_t>(i)],
                       static_cast<int>(fx - 15.0f), static_cast<int>(specRect.getY() + 22), 30, 12,
                       juce::Justification::centred);
        }

        // Draw Blue Filled Spectrum Mountain (or Oscilloscope Wave)
        const auto specPlot = specRect.withTrimmedTop(34.0f).reduced(12.0f, 4.0f);
        juce::Path specPath;
        specPath.startNewSubPath(specPlot.getX(), specPlot.getBottom());

        if (showSpectrumMode_) {
            for (std::size_t i = 0; i < spectrumBins_.size(); ++i) {
                const float nx = static_cast<float>(i) / static_cast<float>(spectrumBins_.size() - 1);
                const float ny = std::clamp(spectrumBins_[i], 0.02f, 0.96f);
                const float px = specPlot.getX() + nx * specPlot.getWidth();
                const float py = specPlot.getBottom() - ny * specPlot.getHeight();
                specPath.lineTo(px, py);
            }
            specPath.lineTo(specPlot.getRight(), specPlot.getBottom());
            specPath.closeSubPath();

            g.setColour(juce::Colour(0xff2563eb).withAlpha(0.65f));
            g.fillPath(specPath);
            g.setColour(juce::Colour(0xff3b82f6));
            g.strokePath(specPath, juce::PathStrokeType(1.5f));
        } else {
            juce::Path waveOut;
            for (std::size_t i = 0; i < 128; ++i) {
                const float nx = static_cast<float>(i) / 127.0f;
                const float s = historyWave_[(writeCursor_ + i * 2) & (historyWave_.size() - 1)];
                const float px = specPlot.getX() + nx * specPlot.getWidth();
                const float py = specPlot.getCentreY() - std::clamp(s * 1.6f, -0.9f, 0.9f) * (specPlot.getHeight() * 0.45f);
                if (i == 0) waveOut.startNewSubPath(px, py);
                else        waveOut.lineTo(px, py);
            }
            g.setColour(juce::Colour(0xff38bdf8));
            g.strokePath(waveOut, juce::PathStrokeType(1.8f));
        }

        // ------------------------------------------------------------------
        // B. MAIN CENTER STAGE: "OSC WAVE | MACROS | FILTER 1 | FILTER 2"
        // ------------------------------------------------------------------
        g.setColour(theme_.bgSurface);
        g.fillRoundedRectangle(stageRect, theme_.cornerRadius);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(stageRect, theme_.cornerRadius, 1.0f);

        // Top Mode Pills: [OSC WAVE] [ALL SOURCES] [FILTER 1] [FILTER 2] [MACROS]
        static constexpr std::array<const char*, 5> kCenterTabs = {
            "OSC WAVE", "ALL SOURCES", "FILTER 1", "FILTER 2", "MACROS"
        };
        static constexpr std::array<int, 5> kTabWidths = { 76, 92, 68, 68, 64 };
        centerTabRects_.clear();
        int tx = static_cast<int>(stageRect.getX() + 10);
        const int ty = static_cast<int>(stageRect.getY() + 8);
        for (int i = 0; i < 5; ++i) {
            const int tw = kTabWidths[static_cast<std::size_t>(i)];
            const juce::Rectangle<int> tr(tx, ty, tw, 22);
            centerTabRects_.push_back(tr);
            const bool isAct = (centerViewMode_ == i);
            g.setColour(isAct ? juce::Colour(0xff38bdf8).withAlpha(0.18f) : theme_.bgElevated);
            g.fillRoundedRectangle(tr.toFloat(), 4.0f);
            g.setColour(isAct ? juce::Colour(0xff38bdf8) : theme_.borderSubtle);
            g.drawRoundedRectangle(tr.toFloat(), 4.0f, 1.0f);
            g.setFont(theme_.getSectionFont(12.5f));
            g.setColour(isAct ? theme_.textPrimary : theme_.textSecondary);
            g.drawText(kCenterTabs[static_cast<std::size_t>(i)], tr, juce::Justification::centred);
            tx += tw + 4;
        }

        // Top-right dynamic status label
        if (centerViewMode_ == 1) {
            int activeCount = 0;
            for (int i = 0; i < 8; ++i) {
                if (params.oscSlots[static_cast<std::size_t>(i)].enabled)
                    activeCount++;
            }
            g.setFont(theme_.getSectionFont(13.0f));
            g.setColour(juce::Colour(0xff2dd4bf));
            g.drawText(juce::String(activeCount) + " ACTIVE SOURCES",
                       static_cast<int>(stageRect.getRight() - 145), ty, 135, 22,
                       juce::Justification::centredRight);
        } else if (centerViewMode_ == 2) {
            g.setFont(theme_.getSectionFont(13.0f));
            g.setColour(juce::Colour(0xff38bdf8));
            g.drawText(params.filterType == 0 ? "FLT 1: LADDER" : (params.filterType == 1 ? "FLT 1: BP" : "FLT 1: HP"),
                       static_cast<int>(stageRect.getRight() - 130), ty, 120, 22,
                       juce::Justification::centredRight);
        } else if (centerViewMode_ == 3) {
            g.setFont(theme_.getSectionFont(13.0f));
            g.setColour(juce::Colour(0xff2dd4bf));
            static constexpr std::array<const char*, 4> kFlt2Types = { "FLT 2: LADDER", "FLT 2: BP", "FLT 2: HP", "FLT 2: CARVE BELL" };
            g.drawText(kFlt2Types[static_cast<std::size_t>(std::clamp(params.filter2Type, 0, 3))],
                       static_cast<int>(stageRect.getRight() - 145), ty, 135, 22,
                       juce::Justification::centredRight);
        } else if (centerViewMode_ == 4) {
            g.setFont(theme_.getSectionFont(13.0f));
            g.setColour(juce::Colour(0xfff59e0b));
            g.drawText("MACRO 2D PAD",
                       static_cast<int>(stageRect.getRight() - 120), ty, 110, 22,
                       juce::Justification::centredRight);
        } else {
            const auto layerCol = getLayerColour(selectedOsc_);
            g.setFont(theme_.getSectionFont(14.0f));
            g.setColour(layerCol);
            g.drawText(getLayerShortName(params, selectedOsc_),
                       static_cast<int>(stageRect.getRight() - 65), ty, 55, 22,
                       juce::Justification::centredRight);
        }

        // Tall Canvas inside Center Stage
        canvasRect_ = stageRect.withTrimmedTop(36.0f).withTrimmedBottom(32.0f).reduced(10.0f, 2.0f);
        g.setColour(juce::Colour(0xff0b0d12));
        g.fillRoundedRectangle(canvasRect_, 6.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(canvasRect_, 6.0f, 1.0f);

        // Green [HOLD] Drone Button in top-right of canvas
        holdBtnRect_ = juce::Rectangle<int>(
            static_cast<int>(canvasRect_.getRight() - 58), static_cast<int>(canvasRect_.getY() + 8), 48, 20);
        const bool isHold = params.droneEnabled;
        g.setColour(isHold ? juce::Colour(0xff22c55e).withAlpha(0.28f) : theme_.bgElevated);
        g.fillRoundedRectangle(holdBtnRect_.toFloat(), 4.0f);
        g.setColour(juce::Colour(0xff22c55e));
        g.drawRoundedRectangle(holdBtnRect_.toFloat(), 4.0f, 1.2f);
        g.setFont(theme_.getSectionFont(12.0f));
        g.setColour(isHold ? juce::Colours::white : juce::Colour(0xff22c55e));
        g.drawText("HOLD", holdBtnRect_, juce::Justification::centred);

        if (centerViewMode_ == 0) {
            // --- Single Selected OSC Waveform + Granular Grain Window ---
            const float morph = slotP.shapeMorph;
            const float spray = slotP.detune;
            const float midY = canvasRect_.getCentreY() + 10.0f;
            const float maxAmp = canvasRect_.getHeight() * 0.40f;
            const float cursorX = canvasRect_.getX() + 24.0f + morph * (canvasRect_.getWidth() - 48.0f);
            const float cursorY = canvasRect_.getBottom() - 24.0f - spray * (canvasRect_.getHeight() - 56.0f);

            const bool isGranular = (slotP.engine == nova::dsp::MultiEngineOscillator::EngineType::OrganicTexture);
            if (isGranular) {
                const float grainW = 32.0f + morph * 96.0f;
                juce::Path grainEnv;
                grainEnv.startNewSubPath(cursorX - grainW * 0.5f, midY);
                for (int i = 0; i <= 32; ++i) {
                    const float u = static_cast<float>(i) / 32.0f;
                    const float gx = cursorX - grainW * 0.5f + u * grainW;
                    const float hann = 0.5f * (1.0f - std::cos(u * 6.28318f));
                    const float gy = midY - hann * maxAmp * 0.82f;
                    grainEnv.lineTo(gx, gy);
                }
                grainEnv.lineTo(cursorX + grainW * 0.5f, midY);
                grainEnv.closeSubPath();
                g.setColour(juce::Colour(0xff84cc16).withAlpha(0.20f));
                g.fillPath(grainEnv);
                g.setColour(juce::Colour(0xff84cc16).withAlpha(0.85f));
                g.strokePath(grainEnv, juce::PathStrokeType(1.6f));

                const float scanDashes[2] = { 3.0f, 3.0f };
                for (int h = -1; h <= 1; ++h) {
                    const float hx = cursorX + static_cast<float>(h) * (grainW * 0.28f);
                    if (hx >= canvasRect_.getX() + 4.0f && hx <= canvasRect_.getRight() - 4.0f) {
                        g.setColour(juce::Colour(0xff84cc16).withAlpha(h == 0 ? 0.75f : 0.35f));
                        g.drawDashedLine(juce::Line<float>(hx, midY - maxAmp * 0.70f, hx, midY),
                                         scanDashes, 2, 1.0f);
                    }
                }
            }

            // High-Resolution Bipolar Oscillating Waveform
            juce::Path wavePath;
            juce::Path waveFill;
            constexpr int kWavePts = 240;
            waveFill.startNewSubPath(canvasRect_.getX() + 6.0f, midY);

            for (int i = 0; i <= kWavePts; ++i) {
                const float u = static_cast<float>(i) / static_cast<float>(kWavePts);
                const float x = canvasRect_.getX() + 6.0f + u * (canvasRect_.getWidth() - 12.0f);
                const float waveShape = calculateOscSample(slotP, u * 3.0f);
                const float y = midY - waveShape * (maxAmp * 0.88f * std::clamp(slotP.level * 1.25f, 0.35f, 1.0f));
                if (i == 0) wavePath.startNewSubPath(x, y);
                else        wavePath.lineTo(x, y);
                waveFill.lineTo(x, y);
            }
            waveFill.lineTo(canvasRect_.getRight() - 6.0f, midY);
            waveFill.closeSubPath();

            // Zero line
            g.setColour(juce::Colour(0xff334155).withAlpha(0.35f));
            g.drawDashedLine(juce::Line<float>(canvasRect_.getX() + 6.0f, midY, canvasRect_.getRight() - 6.0f, midY), nullptr, 0, 1.0f);

            const auto layerCol = getLayerColour(selectedOsc_);
            juce::ColourGradient waveGrad(layerCol.withAlpha(0.32f), canvasRect_.getX(), midY - maxAmp,
                                          layerCol.withAlpha(0.04f), canvasRect_.getX(), midY + maxAmp, false);
            g.setGradientFill(waveGrad);
            g.fillPath(waveFill);

            g.setColour(layerCol.withAlpha(0.35f));
            g.strokePath(wavePath, juce::PathStrokeType(4.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(layerCol);
            g.strokePath(wavePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(juce::Colours::white.withAlpha(0.75f));
            g.strokePath(wavePath, juce::PathStrokeType(0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Playhead & Morph Node Handle
            g.setColour(juce::Colours::white.withAlpha(0.65f));
            g.drawVerticalLine(static_cast<int>(cursorX), canvasRect_.getY() + 4.0f, canvasRect_.getBottom() - 4.0f);

            g.setColour(layerCol);
            g.fillEllipse(cursorX - 5.0f, cursorY - 5.0f, 10.0f, 10.0f);
            g.setColour(juce::Colours::white);
            g.drawEllipse(cursorX - 5.0f, cursorY - 5.0f, 10.0f, 10.0f, 1.6f);

        } else if (centerViewMode_ == 1) {
            // --- ALL SOURCES: Live Stabilized Scope When Playing, or Composite Sum + Layer Ghosts When Idle ---
            const float midY = canvasRect_.getCentreY() + 6.0f;
            const float maxAmp = canvasRect_.getHeight() * 0.38f;
            const auto tel = proc_.getSynthTelemetry();
            const float outEnergy = (tel.outputPeakL + tel.outputPeakR) * 0.5f;
            const bool isAudioActive = (tel.activeVoiceCount > 0 || outEnergy > 0.005f);

            // Zero line
            g.setColour(juce::Colour(0xff334155).withAlpha(0.35f));
            g.drawDashedLine(juce::Line<float>(canvasRect_.getX() + 6.0f, midY, canvasRect_.getRight() - 6.0f, midY), nullptr, 0, 1.0f);

            if (isAudioActive) {
                // Stabilized Zero-Crossing Oscilloscope from live output audio
                std::size_t trigIdx = (writeCursor_ + historyWave_.size() - 192) & (historyWave_.size() - 1);
                for (int off = 240; off >= 32; --off) {
                    const std::size_t uOff = static_cast<std::size_t>(off);
                    const std::size_t i0 = (writeCursor_ + historyWave_.size() - uOff - 1) & (historyWave_.size() - 1);
                    const std::size_t i1 = (writeCursor_ + historyWave_.size() - uOff) & (historyWave_.size() - 1);
                    if (historyWave_[i0] <= 0.0f && historyWave_[i1] > 0.0f) {
                        trigIdx = i1;
                        break;
                    }
                }

                constexpr int kScopePts = 192;
                juce::Path livePath;
                juce::Path liveFill;
                liveFill.startNewSubPath(canvasRect_.getX() + 6.0f, midY);

                for (int pt = 0; pt <= kScopePts; ++pt) {
                    const float u = static_cast<float>(pt) / static_cast<float>(kScopePts);
                    const float x = canvasRect_.getX() + 6.0f + u * (canvasRect_.getWidth() - 12.0f);
                    const std::size_t sIdx = (trigIdx + static_cast<std::size_t>(pt)) & (historyWave_.size() - 1);
                    const float s = std::clamp(historyWave_[sIdx] * 1.5f, -1.0f, 1.0f);
                    const float y = midY - s * (maxAmp * 0.90f);

                    if (pt == 0) livePath.startNewSubPath(x, y);
                    else         livePath.lineTo(x, y);
                    liveFill.lineTo(x, y);
                }
                liveFill.lineTo(canvasRect_.getRight() - 6.0f, midY);
                liveFill.closeSubPath();

                juce::ColourGradient liveGrad(juce::Colour(0xff2dd4bf).withAlpha(0.30f), canvasRect_.getX(), midY - maxAmp,
                                              juce::Colour(0xff2dd4bf).withAlpha(0.03f), canvasRect_.getX(), midY + maxAmp, false);
                g.setGradientFill(liveGrad);
                g.fillPath(liveFill);

                g.setColour(juce::Colour(0xff2dd4bf).withAlpha(0.40f));
                g.strokePath(livePath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour(juce::Colour(0xff2dd4bf));
                g.strokePath(livePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour(juce::Colours::white.withAlpha(0.90f));
                g.strokePath(livePath, juce::PathStrokeType(0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                // Pulsing Green Live Scope Indicator
                g.setColour(juce::Colour(0xff22c55e));
                g.fillEllipse(canvasRect_.getX() + 12.0f, canvasRect_.getY() + 11.0f, 6.0f, 6.0f);
                g.setFont(theme_.getSectionFont(11.5f));
                g.setColour(juce::Colour(0xff2dd4bf));
                g.drawText("LIVE SCOPE", static_cast<int>(canvasRect_.getX() + 24), static_cast<int>(canvasRect_.getY() + 7), 100, 14, juce::Justification::centredLeft);
            } else {
                // Idle / Theoretical Sum of all active layers
                constexpr int kPts = 240;

                // 1. Subtle ghost curves for each active layer in its own color
                for (int i = 0; i < 8; ++i) {
                    const auto& slot = params.oscSlots[static_cast<std::size_t>(i)];
                    if (!slot.enabled) continue;

                    juce::Path layerGhost;
                    for (int pt = 0; pt <= kPts; ++pt) {
                        const float u = static_cast<float>(pt) / static_cast<float>(kPts);
                        const float x = canvasRect_.getX() + 6.0f + u * (canvasRect_.getWidth() - 12.0f);
                        const float s = calculateOscSample(slot, u * 3.0f);
                        const float y = midY - s * (maxAmp * 0.40f * std::clamp(slot.level, 0.25f, 1.0f));
                        if (pt == 0) layerGhost.startNewSubPath(x, y);
                        else         layerGhost.lineTo(x, y);
                    }
                    g.setColour(getLayerColour(i).withAlpha(0.32f));
                    g.strokePath(layerGhost, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }

                // 2. Composite Sum Path + Gradient Fill
                int activeSources = 0;
                for (int i = 0; i < 8; ++i) {
                    if (params.oscSlots[static_cast<std::size_t>(i)].enabled) activeSources++;
                }
                const float normFactor = activeSources > 0 ? (1.0f / std::sqrt(static_cast<float>(activeSources))) : 1.0f;

                juce::Path sumPath;
                juce::Path sumFill;
                sumFill.startNewSubPath(canvasRect_.getX() + 6.0f, midY);

                for (int pt = 0; pt <= kPts; ++pt) {
                    const float u = static_cast<float>(pt) / static_cast<float>(kPts);
                    const float x = canvasRect_.getX() + 6.0f + u * (canvasRect_.getWidth() - 12.0f);
                    float sumVal = 0.0f;
                    for (int i = 0; i < 8; ++i) {
                        const auto& slot = params.oscSlots[static_cast<std::size_t>(i)];
                        if (slot.enabled) {
                            sumVal += calculateOscSample(slot, u * 3.0f) * slot.level;
                        }
                    }
                    const float comp = std::tanh(sumVal * normFactor * 0.95f);
                    const float y = midY - comp * (maxAmp * 0.85f);
                    if (pt == 0) sumPath.startNewSubPath(x, y);
                    else         sumPath.lineTo(x, y);
                    sumFill.lineTo(x, y);
                }
                sumFill.lineTo(canvasRect_.getRight() - 6.0f, midY);
                sumFill.closeSubPath();

                juce::ColourGradient sumGrad(juce::Colour(0xff2dd4bf).withAlpha(0.28f), canvasRect_.getX(), midY - maxAmp,
                                              juce::Colour(0xff2dd4bf).withAlpha(0.03f), canvasRect_.getX(), midY + maxAmp, false);
                g.setGradientFill(sumGrad);
                g.fillPath(sumFill);

                g.setColour(juce::Colour(0xff2dd4bf).withAlpha(0.38f));
                g.strokePath(sumPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour(juce::Colour(0xff2dd4bf));
                g.strokePath(sumPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour(juce::Colours::white.withAlpha(0.85f));
                g.strokePath(sumPath, juce::PathStrokeType(0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                g.setFont(theme_.getSectionFont(11.5f));
                g.setColour(juce::Colour(0xff2dd4bf));
                g.drawText("COMPOSITE HARMONIC SUM", static_cast<int>(canvasRect_.getX() + 12), static_cast<int>(canvasRect_.getY() + 7), 180, 14, juce::Justification::centredLeft);
            }

            // Clickable layer pills along bottom of canvas
            allSourcesBadges_.clear();
            int bX = static_cast<int>(canvasRect_.getX() + 10.0f);
            const int bY = static_cast<int>(canvasRect_.getBottom() - 24.0f);
            for (int i = 0; i < 8; ++i) {
                if (params.oscSlots[static_cast<std::size_t>(i)].enabled) {
                    const juce::String name = getLayerShortName(params, i);
                    const int bw = 54;
                    const juce::Rectangle<int> bRect(bX, bY, bw, 18);
                    allSourcesBadges_.push_back({ bRect, i });

                    const auto c = getLayerColour(i);
                    g.setColour(c.withAlpha(0.18f));
                    g.fillRoundedRectangle(bRect.toFloat(), 3.0f);
                    g.setColour(c.withAlpha(0.85f));
                    g.drawRoundedRectangle(bRect.toFloat(), 3.0f, 1.0f);
                    g.setFont(theme_.getLabelFont(10.5f));
                    g.setColour(c);
                    g.drawText(name, bRect, juce::Justification::centred);

                    bX += bw + 6;
                }
            }

        } else if (centerViewMode_ == 2 || centerViewMode_ == 3) {
            // FILTER 1 / FILTER 2 Frequency Response Curve
            nova::dsp::StateVariableFilter svf1, svf2;
            svf1.prepare(48000.0);
            svf2.prepare(48000.0);
            auto svf1Type = nova::dsp::StateVariableFilter::Type::LowPass;
            if (params.filterType == 1) svf1Type = nova::dsp::StateVariableFilter::Type::BandPass;
            if (params.filterType == 2) svf1Type = nova::dsp::StateVariableFilter::Type::HighPass;
            svf1.setParameters(svf1Type, params.filterCutoffHz, params.filterResonance);

            auto svf2Type = nova::dsp::StateVariableFilter::Type::LowPass;
            if (params.filter2Type == 1) svf2Type = nova::dsp::StateVariableFilter::Type::BandPass;
            else if (params.filter2Type == 2) svf2Type = nova::dsp::StateVariableFilter::Type::HighPass;
            else if (params.filter2Type == 3) svf2Type = nova::dsp::StateVariableFilter::Type::Bell;
            svf2.setParameters(svf2Type, params.filter2CutoffHz, params.filter2Resonance);

            juce::Path fPath;
            fPath.startNewSubPath(canvasRect_.getX(), canvasRect_.getBottom());
            for (int i = 0; i <= 96; ++i) {
                const float nx = static_cast<float>(i) / 96.0f;
                const float freq = 20.0f * std::pow(1000.0f, nx);
                float mag = 1.0f;
                if (centerViewMode_ == 2) {
                    mag = svf1.getMagnitudeAtFrequency(freq);
                } else {
                    mag = svf2.getMagnitudeAtFrequency(freq);
                }
                const float db = 20.0f * std::log10(std::max(0.04f, mag));
                const float ny = std::clamp(0.40f - (db / 35.0f), 0.06f, 0.94f);
                fPath.lineTo(canvasRect_.getX() + nx * canvasRect_.getWidth(),
                             canvasRect_.getY() + ny * canvasRect_.getHeight());
            }
            fPath.lineTo(canvasRect_.getRight(), canvasRect_.getBottom());
            fPath.closeSubPath();

            juce::Colour filterCol = (centerViewMode_ == 2) ? juce::Colour(0xff38bdf8) : juce::Colour(0xff2dd4bf);
            g.setColour(filterCol.withAlpha(0.25f));
            g.fillPath(fPath);
            g.setColour(filterCol);
            g.strokePath(fPath, juce::PathStrokeType(2.0f));

            g.setFont(theme_.getSectionFont(11.5f));
            g.setColour(filterCol);
            g.drawText(centerViewMode_ == 2 ? "FILTER 1 RESPONSE (LADDER / SVF)" : "FILTER 2 RESPONSE (VOCAL CARVE)",
                       static_cast<int>(canvasRect_.getX() + 12), static_cast<int>(canvasRect_.getY() + 7), 240, 14,
                       juce::Justification::centredLeft);

        } else if (centerViewMode_ == 4) {
            // MACRO 2D PAD (Warmth vs Air)
            g.setColour(juce::Colour(0xff334155).withAlpha(0.25f));
            for (int gx = 1; gx < 4; ++gx) {
                const float lx = canvasRect_.getX() + static_cast<float>(gx) * (canvasRect_.getWidth() / 4.0f);
                g.drawDashedLine(juce::Line<float>(lx, canvasRect_.getY() + 4.0f, lx, canvasRect_.getBottom() - 4.0f), nullptr, 0, 1.0f);
            }
            for (int gy = 1; gy < 4; ++gy) {
                const float ly = canvasRect_.getY() + static_cast<float>(gy) * (canvasRect_.getHeight() / 4.0f);
                g.drawDashedLine(juce::Line<float>(canvasRect_.getX() + 4.0f, ly, canvasRect_.getRight() - 4.0f, ly), nullptr, 0, 1.0f);
            }

            const float padX = canvasRect_.getX() + 20.0f + params.macroWarmth * (canvasRect_.getWidth() - 40.0f);
            const float padY = canvasRect_.getBottom() - 20.0f - params.macroAir * (canvasRect_.getHeight() - 40.0f);

            g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.20f));
            g.fillEllipse(padX - 20.0f, padY - 20.0f, 40.0f, 40.0f);
            g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.60f));
            g.drawEllipse(padX - 20.0f, padY - 20.0f, 40.0f, 40.0f, 1.2f);
            g.setColour(juce::Colour(0xfff59e0b));
            g.fillEllipse(padX - 5.0f, padY - 5.0f, 10.0f, 10.0f);
            g.setColour(juce::Colours::white);
            g.drawEllipse(padX - 5.0f, padY - 5.0f, 10.0f, 10.0f, 1.5f);

            g.setFont(theme_.getSectionFont(11.5f));
            g.setColour(juce::Colour(0xfff59e0b));
            g.drawText("MACRO 2D PAD: WARMTH (X) & AIR (Y)",
                       static_cast<int>(canvasRect_.getX() + 12), static_cast<int>(canvasRect_.getY() + 7), 260, 14,
                       juce::Justification::centredLeft);
        }

        // Bottom area inside stageRect
        const int pillY = static_cast<int>(stageRect.getBottom() - 26);
        const int pillX = static_cast<int>(stageRect.getRight() - 238);

        if (centerViewMode_ == 0) {
            octPillRect_ = juce::Rectangle<int>(pillX, pillY, 54, 20);
            semPillRect_ = juce::Rectangle<int>(pillX + 58, pillY, 54, 20);
            const juce::Rectangle<int> finPillRect(pillX + 116, pillY, 54, 20);
            const juce::Rectangle<int> crsPillRect(pillX + 174, pillY, 54, 20);

            const auto drawPill = [&](const juce::Rectangle<int>& pr, const juce::String& text, bool isInteractive) {
                g.setColour(theme_.bgControl);
                g.fillRoundedRectangle(pr.toFloat(), 4.0f);
                g.setColour(isInteractive ? juce::Colour(0xff38bdf8).withAlpha(0.7f) : theme_.borderSubtle);
                g.drawRoundedRectangle(pr.toFloat(), 4.0f, 1.0f);
                g.setFont(theme_.getValueFont(12.5f));
                g.setColour(juce::Colour(0xff38bdf8));
                g.drawText(text, pr, juce::Justification::centred);
            };

            drawPill(octPillRect_, juce::String("OCT ") + (slotP.octaveOffset >= 0 ? "+" : "") + juce::String(slotP.octaveOffset), true);
            drawPill(semPillRect_, juce::String("SEM ") + (slotP.semitoneOffset >= 0 ? "+" : "") + juce::String(slotP.semitoneOffset), true);
            drawPill(finPillRect, "FIN +0", false);
            drawPill(crsPillRect, "CRS 1.00", false);

            g.setFont(theme_.getLabelFont(11.0f));
            g.setColour(theme_.textMuted);
            g.drawText("DRAG: X = MORPH | Y = DETUNE", static_cast<int>(stageRect.getX() + 14), pillY, 240, 20, juce::Justification::centredLeft);
        } else if (centerViewMode_ == 1) {
            const auto tel = proc_.getSynthTelemetry();
            g.setFont(theme_.getLabelFont(11.0f));
            g.setColour(theme_.textMuted);
            g.drawText("CLICK LAYER BADGE TO EDIT  •  DRAG: WARMTH & CUTOFF", static_cast<int>(stageRect.getX() + 14), pillY, 340, 20, juce::Justification::centredLeft);

            g.setFont(theme_.getValueFont(12.0f));
            g.setColour(juce::Colour(0xff2dd4bf));
            g.drawText(juce::String(tel.activeVoiceCount) + " VOICES ACTIVE", pillX + 90, pillY, 140, 20, juce::Justification::centredRight);
        } else if (centerViewMode_ == 2 || centerViewMode_ == 3) {
            g.setFont(theme_.getLabelFont(11.0f));
            g.setColour(theme_.textMuted);
            g.drawText("DRAG: X = CUTOFF | Y = RESONANCE", static_cast<int>(stageRect.getX() + 14), pillY, 260, 20, juce::Justification::centredLeft);
        } else if (centerViewMode_ == 4) {
            g.setFont(theme_.getLabelFont(11.0f));
            g.setColour(theme_.textMuted);
            g.drawText("DRAG: X = WARMTH | Y = AIR", static_cast<int>(stageRect.getX() + 14), pillY, 260, 20, juce::Justification::centredLeft);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override {
        const auto pos = event.getPosition();

        if (centerViewMode_ == 1) {
            for (const auto& b : allSourcesBadges_) {
                if (b.first.contains(pos)) {
                    selectedOsc_ = b.second;
                    centerViewMode_ = 0;
                    if (onSelectOsc) onSelectOsc(b.second);
                    repaint();
                    return;
                }
            }
        }

        if (centerViewMode_ == 0) {
            if (octPillRect_.contains(pos)) {
                const juce::String prefix = "osc" + juce::String(selectedOsc_ + 1) + "_";
                if (auto* p = proc_.getAPVTS().getParameter(prefix + "octave")) {
                    const int cur = static_cast<int>(std::round(p->convertFrom0to1(p->getValue())));
                    int next = event.mods.isPopupMenu() ? (cur - 1) : (cur + 1);
                    if (next > 2) next = -2;
                    if (next < -2) next = 2;
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(next)));
                }
                repaint();
                return;
            }

            if (semPillRect_.contains(pos)) {
                const juce::String prefix = "osc" + juce::String(selectedOsc_ + 1) + "_";
                if (auto* p = proc_.getAPVTS().getParameter(prefix + "semi")) {
                    const int cur = static_cast<int>(std::round(p->convertFrom0to1(p->getValue())));
                    int next = event.mods.isPopupMenu() ? (cur - 1) : (cur + 1);
                    if (next > 12) next = -12;
                    if (next < -12) next = 12;
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(next)));
                }
                repaint();
                return;
            }
        }

        if (specModeToggleBounds_.contains(pos)) {
            showSpectrumMode_ = !showSpectrumMode_;
            repaint();
            return;
        }

        for (int i = 0; i < static_cast<int>(centerTabRects_.size()); ++i) {
            if (centerTabRects_[static_cast<std::size_t>(i)].contains(pos)) {
                centerViewMode_ = i;
                repaint();
                return;
            }
        }

        if (holdBtnRect_.contains(pos)) {
            if (auto* p = proc_.getAPVTS().getParameter("droneEnabled")) {
                p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
            }
            repaint();
            return;
        }

        if (canvasRect_.contains(pos.toFloat())) {
            dragCanvasAt(pos.toFloat());
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override {
        if (canvasRect_.contains(event.getMouseDownPosition().toFloat())) {
            dragCanvasAt(event.position);
        }
    }

private:
    void dragCanvasAt(juce::Point<float> pt) {
        const float nx = std::clamp((pt.x - canvasRect_.getX()) / std::max(1.0f, canvasRect_.getWidth()), 0.0f, 1.0f);
        const float ny = std::clamp(1.0f - (pt.y - canvasRect_.getY()) / std::max(1.0f, canvasRect_.getHeight()), 0.0f, 1.0f);

        if (centerViewMode_ == 0) {
            const juce::String prefix = "osc" + juce::String(selectedOsc_ + 1) + "_";
            if (auto* s = proc_.getAPVTS().getParameter(prefix + "shape"))  s->setValueNotifyingHost(nx);
            if (auto* d = proc_.getAPVTS().getParameter(prefix + "detune")) d->setValueNotifyingHost(ny);
        } else if (centerViewMode_ == 1) {
            if (auto* w = proc_.getAPVTS().getParameter("macroWarmth"))   w->setValueNotifyingHost(nx);
            if (auto* c = proc_.getAPVTS().getParameter("filterCutoffHz")) c->setValueNotifyingHost(ny);
        } else if (centerViewMode_ == 2) {
            if (auto* c = proc_.getAPVTS().getParameter("filterCutoffHz")) c->setValueNotifyingHost(nx);
            if (auto* r = proc_.getAPVTS().getParameter("filterResonance")) r->setValueNotifyingHost(ny);
        } else if (centerViewMode_ == 3) {
            if (auto* c = proc_.getAPVTS().getParameter("filter2CutoffHz")) c->setValueNotifyingHost(nx);
            if (auto* r = proc_.getAPVTS().getParameter("filter2Resonance")) r->setValueNotifyingHost(ny);
        } else if (centerViewMode_ == 4) {
            if (auto* w = proc_.getAPVTS().getParameter("macroWarmth")) w->setValueNotifyingHost(nx);
            if (auto* a = proc_.getAPVTS().getParameter("macroAir"))    a->setValueNotifyingHost(ny);
        }
        repaint();
    }

    [[maybe_unused]] NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int selectedOsc_ = 0;
    bool showSpectrumMode_ = true;
    int centerViewMode_ = 0;
    float animPhase_ = 0.0f;

    std::array<float, 512> historyWave_{};
    std::size_t writeCursor_ = 0;
    std::array<float, 32> spectrumBins_{};

    juce::Rectangle<int> specModeToggleBounds_;
    std::vector<juce::Rectangle<int>> centerTabRects_;
    std::vector<std::pair<juce::Rectangle<int>, int>> allSourcesBadges_;
    juce::Rectangle<float> canvasRect_;
    juce::Rectangle<int> holdBtnRect_;
    juce::Rectangle<int> octPillRect_;
    juce::Rectangle<int> semPillRect_;
};

// ==============================================================================
// 3. RIGHT COLUMN: JORNA Dual Filter + Interactive Tall Envelope Editor
// ==============================================================================
class JornaRightColumn : public juce::Component {
public:
    JornaRightColumn(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc),
          theme_(theme),
          cutoffKnob_("CUTOFF", "Hz", theme, false),
          resoKnob_("RESO", "", theme, false),
          carveKnob_("CARVE", "", theme, true),
          driveKnob_("DRIVE", "", theme, false),
          envAmtKnob_("KEYTRK", "", theme, false),
          atkKnob_("ATK", "s", theme, false),
          decKnob_("DEC", "s", theme, false),
          susKnob_("SUS", "", theme, false),
          relKnob_("REL", "s", theme, false) {
        auto& apvts = proc_.getAPVTS();
        cutoffKnob_.attachToParameter(apvts, "filterCutoffHz");
        resoKnob_.attachToParameter(apvts,   "filterResonance");
        carveKnob_.attachToParameter(apvts,  "vocalCarve");
        driveKnob_.attachToParameter(apvts,  "filterDrive");
        envAmtKnob_.attachToParameter(apvts, "filterEnvAmount");

        addAndMakeVisible(cutoffKnob_);
        addAndMakeVisible(resoKnob_);
        addAndMakeVisible(carveKnob_);
        addAndMakeVisible(driveKnob_);
        addAndMakeVisible(envAmtKnob_);

        addAndMakeVisible(atkKnob_);
        addAndMakeVisible(decKnob_);
        addAndMakeVisible(susKnob_);
        addAndMakeVisible(relKnob_);

        selectFilterTab(0);
        selectEnvelopeTab(0);
    }

    void selectFilterTab(int fltIdx) {
        activeFilterTab_ = std::clamp(fltIdx, 0, 1);
        auto& apvts = proc_.getAPVTS();
        if (activeFilterTab_ == 0) {
            cutoffKnob_.setLabel("CUTOFF");
            cutoffKnob_.setSuffix("Hz");
            cutoffKnob_.attachToParameter(apvts, "filterCutoffHz");

            resoKnob_.setLabel("RESO");
            resoKnob_.setSuffix("");
            resoKnob_.attachToParameter(apvts, "filterResonance");

            carveKnob_.setLabel("DRIVE");
            carveKnob_.setSuffix("");
            carveKnob_.attachToParameter(apvts, "filterDrive");

            driveKnob_.setLabel("ENV AMT");
            driveKnob_.setSuffix("");
            driveKnob_.attachToParameter(apvts, "filterEnvAmount");

            envAmtKnob_.setLabel("WARMTH");
            envAmtKnob_.setSuffix("");
            envAmtKnob_.attachToParameter(apvts, "macroWarmth");
        } else {
            cutoffKnob_.setLabel("CUTOFF");
            cutoffKnob_.setSuffix("Hz");
            cutoffKnob_.attachToParameter(apvts, "filter2CutoffHz");

            resoKnob_.setLabel("RESO");
            resoKnob_.setSuffix("");
            resoKnob_.attachToParameter(apvts, "filter2Resonance");

            carveKnob_.setLabel("DRIVE");
            carveKnob_.setSuffix("");
            carveKnob_.attachToParameter(apvts, "filter2Drive");

            driveKnob_.setLabel("ENV AMT");
            driveKnob_.setSuffix("");
            driveKnob_.attachToParameter(apvts, "filter2EnvAmount");

            envAmtKnob_.setLabel("CARVE");
            envAmtKnob_.setSuffix("");
            envAmtKnob_.attachToParameter(apvts, "vocalCarve");
        }
        repaint();
    }

    struct EnvParamIds {
        juce::String attack;
        juce::String decay;
        juce::String sustain;
        juce::String release;
    };

    EnvParamIds getActiveEnvParamIds() const {
        switch (activeEnvTab_) {
            case 1:  return { "fltAttack", "fltDecay", "fltSustain", "fltRelease" };
            case 2:  return { "modAttack", "modDecay", "modSustain", "modRelease" };
            case 3:  return { "auxAttack", "auxDecay", "auxSustain", "auxRelease" };
            case 0:
            default: return { "ampAttack", "ampDecay", "ampSustain", "ampRelease" };
        }
    }

    void selectEnvelopeTab(int envIdx) {
        activeEnvTab_ = std::clamp(envIdx, 0, 3);
        auto& apvts = proc_.getAPVTS();
        const auto ids = getActiveEnvParamIds();
        atkKnob_.attachToParameter(apvts, ids.attack);
        decKnob_.attachToParameter(apvts, ids.decay);
        susKnob_.attachToParameter(apvts, ids.sustain);
        relKnob_.attachToParameter(apvts, ids.release);
        repaint();
    }

    void paint(juce::Graphics& g) override {
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

    void resized() override {
        // Filter knobs: 3 on top row, 2 on second row (generous large bounds matching OSC section)
        const int w3 = (getWidth() - 16) / 3;
        cutoffKnob_.setBounds(8,          56, w3, 72);
        resoKnob_.setBounds(8 + w3,       56, w3, 72);
        carveKnob_.setBounds(8 + w3 * 2,  56, w3, 72);

        const int w2 = (getWidth() - 32) / 2;
        driveKnob_.setBounds(16,          128, w2, 72);
        envAmtKnob_.setBounds(16 + w2,    128, w2, 72);

        // Envelope knobs row (ATK, DEC, SUS, REL): full 72px height matching OSC knobs!
        const int envKnobTop = 242;
        const int ew = (getWidth() - 16) / 4;
        atkKnob_.setBounds(8,          envKnobTop, ew, 72);
        decKnob_.setBounds(8 + ew,     envKnobTop, ew, 72);
        susKnob_.setBounds(8 + ew * 2, envKnobTop, ew, 72);
        relKnob_.setBounds(8 + ew * 3, envKnobTop, ew, 72);
    }

    void mouseMove(const juce::MouseEvent& event) override {
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

    void mouseDown(const juce::MouseEvent& event) override {
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

    void mouseDrag(const juce::MouseEvent& event) override {
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

    void mouseUp(const juce::MouseEvent&) override {
        activeDragNode_ = DragNode::None;
        setMouseCursor(hoveredNode_ != DragNode::None ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseExit(const juce::MouseEvent&) override {
        if (activeDragNode_ == DragNode::None) {
            hoveredNode_ = DragNode::None;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

private:
    [[maybe_unused]] NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int activeFilterTab_ = 0;
    int activeEnvTab_ = 0;

    juce::Rectangle<int> flt1TabRect_;
    juce::Rectangle<int> flt2TabRect_;
    juce::Rectangle<int> fltTypeRect_;
    std::vector<juce::Rectangle<int>> envTabRects_;
    juce::Rectangle<float> envGraphRect_;

    enum class DragNode { None, Peak, SusStart, SusEnd, End };
    DragNode activeDragNode_ = DragNode::None;
    DragNode hoveredNode_ = DragNode::None;

    juce::Point<float> pPeak_{};
    juce::Point<float> pSusStart_{};
    juce::Point<float> pSusEnd_{};
    juce::Point<float> pEnd_{};
    float graphX0_ = 0.0f;
    float graphW_ = 0.0f;
    float graphYB_ = 0.0f;
    float graphH_ = 0.0f;

    nova::ui::ArcKnob cutoffKnob_;
    nova::ui::ArcKnob resoKnob_;
    nova::ui::ArcKnob carveKnob_;
    nova::ui::ArcKnob driveKnob_;
    nova::ui::ArcKnob envAmtKnob_;

    nova::ui::ArcKnob atkKnob_;
    nova::ui::ArcKnob decKnob_;
    nova::ui::ArcKnob susKnob_;
    nova::ui::ArcKnob relKnob_;
};

// ==============================================================================
// 4. LOWER FULL-WIDTH WORKBENCH:
//    FLOW | MODULATION | FX RACK | DRONE & ARP | MIXER | SOURCES
// ==============================================================================
class JornaLowerWorkbench : public juce::Component {
public:
    std::function<void(int)> onSelectOsc;
    std::function<void()> onOpenMixerDrawer;

    JornaLowerWorkbench(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc),
          theme_(theme),
          modulationHubPanel_(proc, theme),
          fxRackPanel_(proc, theme),
          droneHubPanel_(proc, theme),
          arpeggiatorHubPanel_(proc, theme),
          mixerHubPanel_(proc, theme) {
        addChildComponent(modulationHubPanel_);
        addChildComponent(fxRackPanel_);
        addChildComponent(droneHubPanel_);
        addChildComponent(arpeggiatorHubPanel_);
        addChildComponent(mixerHubPanel_);
        selectWorkbenchTab(0); // Default to FLOW signal architecture view!
    }

    void selectWorkbenchTab(int idx) {
        activeTab_ = std::clamp(idx, 0, 5);
        modulationHubPanel_.setVisible(activeTab_ == 1);
        fxRackPanel_.setVisible(activeTab_ == 2);
        droneHubPanel_.setVisible(activeTab_ == 3);
        arpeggiatorHubPanel_.setVisible(activeTab_ == 4);
        mixerHubPanel_.setVisible(activeTab_ == 5);
        repaint();
    }

    void setSelectedOscIndex(int idx) {
        selectedOscIndex_ = idx;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(theme_.bgSurface);
        g.fillRoundedRectangle(bounds, theme_.cornerRadius);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(bounds, theme_.cornerRadius, 1.0f);

        // Top Workbench Tab Bar: FLOW | MODULATION | FX RACK | DRONE | ARPEGGIATOR | MIXER
        static constexpr std::array<const char*, 6> kTabs = {
            "FLOW", "MODULATION", "FX RACK", "DRONE", "ARPEGGIATOR", "MIXER"
        };
        tabRects_.clear();
        const int tabW = (getWidth() - 40) / 6;
        for (int i = 0; i < 6; ++i) {
            const juce::Rectangle<int> tr(20 + i * tabW, 6, tabW - 8, 22);
            tabRects_.push_back(tr);
            const bool isAct = (activeTab_ == i);
            if (isAct) {
                g.setColour(juce::Colour(0xff2dd4bf).withAlpha(0.22f));
                g.fillRoundedRectangle(tr.toFloat(), 4.0f);
                g.setColour(juce::Colour(0xff2dd4bf));
                g.drawRoundedRectangle(tr.toFloat(), 4.0f, 1.2f);
            }
            g.setFont(theme_.getSectionFont(13.0f));
            g.setColour(isAct ? juce::Colour(0xff2dd4bf) : theme_.textSecondary);
            g.drawText(kTabs[static_cast<std::size_t>(i)], tr, juce::Justification::centred);
        }

        if (activeTab_ == 0) {
            drawSignalFlowView(g, bounds.withTrimmedTop(32.0f).reduced(10.0f, 6.0f));
        }

        phase_ += 0.06f;
    }

    void resized() override {
        const auto content = getLocalBounds().withTrimmedTop(32).reduced(10, 4);
        modulationHubPanel_.setBounds(content);
        fxRackPanel_.setBounds(content);
        droneHubPanel_.setBounds(content);
        arpeggiatorHubPanel_.setBounds(content);
        mixerHubPanel_.setBounds(content);
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override {
        if (activeTab_ == 0 && cardSourcesRect_.contains(event.getPosition())) {
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

    void mouseDown(const juce::MouseEvent& event) override {
        const auto pos = event.getPosition();
        for (int i = 0; i < static_cast<int>(tabRects_.size()); ++i) {
            if (tabRects_[static_cast<std::size_t>(i)].contains(pos)) {
                if (i == 5) {
                    if (onOpenMixerDrawer) {
                        onOpenMixerDrawer();
                    }
                    return;
                }
                selectWorkbenchTab(i);
                return;
            }
        }

        if (activeTab_ == 0) {
            handleFlowMouseDown(pos);
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override {
        if (activeTab_ == 0) {
            handleFlowMouseDrag(event);
        }
    }

    void mouseUp(const juce::MouseEvent& event) override {
        if (activeTab_ == 0) {
            handleFlowMouseUp(event);
        }
    }

private:
    class FxRackPanel : public juce::Component, private juce::Timer {
    public:
        FxRackPanel(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
            : proc_(proc),
              theme_(theme),
              delaySwing_("SWING", "%", theme, false),
              delayFdbk_("FDBK", "%", theme, true),
              delayMix_("MIX", "%", theme, false),
              revDry_("DRY", "%", theme, false),
              revMix_("WET", "%", theme, false),
              revDecay_("DECAY", "s", theme, false),
              revShimmer_("SHIMMER", "%", theme, true),
              revDucking_("DUCKING", "%", theme, true),
              macroWarmth_("WARMTH", "", theme, false),
              macroMotion_("MOTION", "", theme, true),
              macroShimmer_("SHIMMER", "", theme, true),
              macroAir_("AIR", "", theme, false),
              masterGain_("OUTPUT", "dB", theme, false) {
            auto& apvts = proc_.getAPVTS();
            delaySwing_.attachToParameter(apvts, "delaySwing");
            delayFdbk_.attachToParameter(apvts, "delayFeedback");
            delayMix_.attachToParameter(apvts, "delayMix");

            revDry_.attachToParameter(apvts, "reverbDry");
            revMix_.attachToParameter(apvts, "reverbMix");
            revDecay_.attachToParameter(apvts, "reverbDecay");
            revShimmer_.attachToParameter(apvts, "reverbShimmer");
            revDucking_.attachToParameter(apvts, "reverbDucking");

            macroWarmth_.attachToParameter(apvts, "macroWarmth");
            macroMotion_.attachToParameter(apvts, "macroMotion");
            macroShimmer_.attachToParameter(apvts, "macroShimmer");
            macroAir_.attachToParameter(apvts, "macroAir");

            masterGain_.attachToParameter(apvts, "masterGainDb");

            addAndMakeVisible(delaySwing_);
            addAndMakeVisible(delayFdbk_);
            addAndMakeVisible(delayMix_);

            addAndMakeVisible(revDry_);
            addAndMakeVisible(revMix_);
            addAndMakeVisible(revDecay_);
            addAndMakeVisible(revShimmer_);
            addAndMakeVisible(revDucking_);

            addAndMakeVisible(macroWarmth_);
            addAndMakeVisible(macroMotion_);
            addAndMakeVisible(macroShimmer_);
            addAndMakeVisible(macroAir_);

            addAndMakeVisible(masterGain_);

            startTimerHz(30);
        }

        ~FxRackPanel() override {
            stopTimer();
        }

        void timerCallback() override {
            phase_ += 0.08f;
            if (phase_ > 628.3f) phase_ -= 628.3f;

            const auto tel = proc_.getSynthTelemetry();
            const float decay = 0.88f;
            smoothPeakL_ = std::max(tel.outputPeakL, smoothPeakL_ * decay);
            smoothPeakR_ = std::max(tel.outputPeakR, smoothPeakR_ * decay);

            if (tel.outputPeakL > peakHoldL_) {
                peakHoldL_ = tel.outputPeakL;
                holdCountL_ = 30;
            } else if (holdCountL_ > 0) {
                --holdCountL_;
            } else {
                peakHoldL_ *= 0.94f;
            }

            if (tel.outputPeakR > peakHoldR_) {
                peakHoldR_ = tel.outputPeakR;
                holdCountR_ = 30;
            } else if (holdCountR_ > 0) {
                --holdCountR_;
            } else {
                peakHoldR_ *= 0.94f;
            }

            const auto& mm = proc_.getModMatrix();
            const float dMixDepth = mm.getTotalDepth(nova::dsp::ModMatrix::Destination::DelayMix);
            const float dMixLive = (dMixDepth != 0.0f)
                ? std::clamp(mm.getModulationOffset(nova::dsp::ModMatrix::Destination::DelayMix) / std::max(0.01f, std::abs(dMixDepth)), -1.0f, 1.0f)
                : 0.0f;
            delayMix_.setModulation(dMixDepth, dMixLive);

            const float dFbDepth = mm.getTotalDepth(nova::dsp::ModMatrix::Destination::DelayFeedback);
            const float dFbLive = (dFbDepth != 0.0f)
                ? std::clamp(mm.getModulationOffset(nova::dsp::ModMatrix::Destination::DelayFeedback) / std::max(0.01f, std::abs(dFbDepth)), -1.0f, 1.0f)
                : 0.0f;
            delayFdbk_.setModulation(dFbDepth, dFbLive);

            const float rMixDepth = mm.getTotalDepth(nova::dsp::ModMatrix::Destination::ReverbMix);
            const float rMixLive = (rMixDepth != 0.0f)
                ? std::clamp(mm.getModulationOffset(nova::dsp::ModMatrix::Destination::ReverbMix) / std::max(0.01f, std::abs(rMixDepth)), -1.0f, 1.0f)
                : 0.0f;
            revMix_.setModulation(rMixDepth, rMixLive);

            const float rShimDepth = mm.getTotalDepth(nova::dsp::ModMatrix::Destination::ShimmerAmount);
            const float rShimLive = (rShimDepth != 0.0f)
                ? std::clamp(mm.getModulationOffset(nova::dsp::ModMatrix::Destination::ShimmerAmount) / std::max(0.01f, std::abs(rShimDepth)), -1.0f, 1.0f)
                : 0.0f;
            revShimmer_.setModulation(rShimDepth, rShimLive);

            repaint();
        }

        void resized() override {
            auto totalArea = getLocalBounds();
            const int totalW = totalArea.getWidth();
            const int totalH = totalArea.getHeight();
            const int gap = 8;
            const int availW = totalW - gap * 3;

            // Width allocation: Delay 24%, Reverb 32%, Macros 28%, Master 16%
            const int wDelay = static_cast<int>(static_cast<float>(availW) * 0.24f);
            const int wReverb = static_cast<int>(static_cast<float>(availW) * 0.32f);
            const int wMacros = static_cast<int>(static_cast<float>(availW) * 0.28f);
            const int wMaster = availW - wDelay - wReverb - wMacros;

            cardDelayRect_ = juce::Rectangle<int>(totalArea.getX(), totalArea.getY(), wDelay, totalH);
            cardReverbRect_ = juce::Rectangle<int>(cardDelayRect_.getRight() + gap, totalArea.getY(), wReverb, totalH);
            cardMacrosRect_ = juce::Rectangle<int>(cardReverbRect_.getRight() + gap, totalArea.getY(), wMacros, totalH);
            cardMasterRect_ = juce::Rectangle<int>(cardMacrosRect_.getRight() + gap, totalArea.getY(), wMaster, totalH);

            const int visH = 74; // Significantly expanded visualizer height for rich graphical feedback

            // Card 1: Delay (3 Knobs)
            {
                auto r = cardDelayRect_;
                delayBypRect_ = juce::Rectangle<int>(r.getRight() - 32, r.getY() + 7, 24, 14);
                delaySyncBtnRect_ = juce::Rectangle<int>(delayBypRect_.getX() - 76, r.getY() + 4, 70, 20);
                delayVisRect_ = juce::Rectangle<int>(r.getX() + 6, r.getY() + 27, r.getWidth() - 12, visH);
                const int knobTop = delayVisRect_.getBottom() + 6;
                const int knobH = r.getBottom() - knobTop - 4;
                const int kw = (r.getWidth() - 8) / 3;
                delaySwing_.setBounds(r.getX() + 4 + 0 * kw, knobTop, kw, knobH);
                delayFdbk_.setBounds(r.getX() + 4 + 1 * kw, knobTop, kw, knobH);
                delayMix_.setBounds(r.getX() + 4 + 2 * kw, knobTop, kw, knobH);
            }

            // Card 2: Reverb (5 Knobs)
            {
                auto r = cardReverbRect_;
                reverbBypRect_ = juce::Rectangle<int>(r.getRight() - 32, r.getY() + 7, 24, 14);
                freezeBtnRect_ = juce::Rectangle<int>(reverbBypRect_.getX() - 54, r.getY() + 5, 48, 18);
                shimmerPitchBtnRect_ = juce::Rectangle<int>(freezeBtnRect_.getX() - 76, r.getY() + 4, 70, 20);
                reverbVisRect_ = juce::Rectangle<int>(r.getX() + 6, r.getY() + 27, r.getWidth() - 12, visH);
                const int knobTop = reverbVisRect_.getBottom() + 6;
                const int knobH = r.getBottom() - knobTop - 4;
                const int kw = (r.getWidth() - 8) / 5;
                revDry_.setBounds(r.getX() + 4 + 0 * kw, knobTop, kw, knobH);
                revMix_.setBounds(r.getX() + 4 + 1 * kw, knobTop, kw, knobH);
                revDecay_.setBounds(r.getX() + 4 + 2 * kw, knobTop, kw, knobH);
                revShimmer_.setBounds(r.getX() + 4 + 3 * kw, knobTop, kw, knobH);
                revDucking_.setBounds(r.getX() + 4 + 4 * kw, knobTop, kw, knobH);
            }

            // Card 3: Macros (4 Knobs)
            {
                auto r = cardMacrosRect_;
                macroVisRect_ = juce::Rectangle<int>(r.getX() + 6, r.getY() + 27, r.getWidth() - 12, visH);
                const int knobTop = macroVisRect_.getBottom() + 6;
                const int knobH = r.getBottom() - knobTop - 4;
                const int kw = (r.getWidth() - 8) / 4;
                macroWarmth_.setBounds(r.getX() + 4 + 0 * kw, knobTop, kw, knobH);
                macroMotion_.setBounds(r.getX() + 4 + 1 * kw, knobTop, kw, knobH);
                macroShimmer_.setBounds(r.getX() + 4 + 2 * kw, knobTop, kw, knobH);
                macroAir_.setBounds(r.getX() + 4 + 3 * kw, knobTop, kw, knobH);
            }

            // Card 4: Master (1 Knob)
            {
                auto r = cardMasterRect_;
                masterVisRect_ = juce::Rectangle<int>(r.getX() + 6, r.getY() + 27, r.getWidth() - 12, visH);
                const int knobTop = masterVisRect_.getBottom() + 6;
                const int knobH = r.getBottom() - knobTop - 4;
                const int kw = std::min(r.getWidth() - 8, knobH);
                const int kx = r.getX() + (r.getWidth() - kw) / 2;
                masterGain_.setBounds(kx, knobTop, kw, knobH);
            }
        }

        void mouseDown(const juce::MouseEvent& event) override {
            const auto pos = event.getPosition();
            auto& apvts = proc_.getAPVTS();

            if (delayBypRect_.expanded(4).contains(pos)) {
                if (auto* p = apvts.getParameter("delayEnabled")) {
                    p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
                }
                repaint();
                return;
            }

            if (delaySyncBtnRect_.expanded(4).contains(pos)) {
                juce::PopupMenu menu;
                static constexpr std::array<const char*, 8> kDivLabels = {
                    "1/2 (Half Note)",
                    "1/4 (Quarter Note)",
                    "1/4d (Dotted Quarter)",
                    "1/8 (Eighth Note)",
                    "1/8d (Dotted 8th - Signature Worship)",
                    "1/8t (Eighth Triplet)",
                    "1/16 (Sixteenth Note)",
                    "1/16d (Dotted 16th)"
                };
                auto* pDiv = apvts.getParameter("delaySyncDiv");
                const int cur = (pDiv != nullptr) ? static_cast<int>(pDiv->convertFrom0to1(pDiv->getValue()) + 0.5f) : 4;
                for (int i = 0; i < 8; ++i) {
                    menu.addItem(i + 1, kDivLabels[static_cast<std::size_t>(i)], true, i == cur);
                }

                menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(delaySyncBtnRect_)).withStandardItemHeight(28),
                    [pDiv, this](int res) {
                        if (res >= 1 && res <= 8 && pDiv != nullptr) {
                            pDiv->setValueNotifyingHost(pDiv->convertTo0to1(static_cast<float>(res - 1)));
                            repaint();
                        }
                    });
                return;
            }

            if (reverbBypRect_.expanded(4).contains(pos)) {
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

            if (freezeBtnRect_.expanded(4).contains(pos)) {
                if (auto* p = apvts.getParameter("reverbFreeze")) {
                    p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
                }
                repaint();
                return;
            }

            if (shimmerPitchBtnRect_.expanded(4).contains(pos)) {
                juce::PopupMenu menu;
                static constexpr std::array<const char*, 4> kPitchLabels = {
                    "+1 OCT (+12 st Octave Shimmer)",
                    "+2 OCT (+24 st Celestial Air)",
                    "+1+2 OCT (Dual Octave Valhalla Halo)",
                    "-1 OCT (-12 st Sub-Octave Ambient)"
                };
                auto* pPitch = apvts.getParameter("reverbShimmerPitch");
                const int curIdx = (pPitch != nullptr) ? static_cast<int>(pPitch->convertFrom0to1(pPitch->getValue()) + 0.5f) : 0;
                for (int i = 0; i < 4; ++i) {
                    menu.addItem(i + 1, kPitchLabels[static_cast<std::size_t>(i)], true, i == curIdx);
                }

                menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(shimmerPitchBtnRect_)).withStandardItemHeight(28),
                    [pPitch, this](int res) {
                        if (res >= 1 && res <= 4 && pPitch != nullptr) {
                            pPitch->setValueNotifyingHost(pPitch->convertTo0to1(static_cast<float>(res - 1)));
                            repaint();
                        }
                    });
                return;
            }
        }

        void paint(juce::Graphics& g) override {
            const auto params = proc_.getCurrentParameters();

            // Module 1: STEREO TAPE DELAY (Teal 0xff2dd4bf)
            drawCardBackground(g, cardDelayRect_, juce::Colour(0xff0d161c), juce::Colour(0xff2dd4bf).withAlpha(0.35f));
            drawDelayHeader(g, cardDelayRect_, params);
            drawDelayVisualizer(g, delayVisRect_, params);

            // Module 2: NOVA SPACE REVERB (Purple 0xffc084fc)
            drawCardBackground(g, cardReverbRect_, juce::Colour(0xff150d22), juce::Colour(0xffc084fc).withAlpha(0.35f));
            drawReverbHeader(g, cardReverbRect_, params);
            drawReverbVisualizer(g, reverbVisRect_, params);

            // Module 3: PERFORMANCE MACROS (Amber 0xfffbbf24)
            drawCardBackground(g, cardMacrosRect_, juce::Colour(0xff1c160c), juce::Colour(0xfffbbf24).withAlpha(0.35f));
            drawMacrosHeader(g, cardMacrosRect_, params);
            drawMacrosVisualizer(g, macroVisRect_, params);

            // Module 4: MASTER OUTPUT (Green 0xff22c55e)
            drawCardBackground(g, cardMasterRect_, juce::Colour(0xff0d1a14), juce::Colour(0xff22c55e).withAlpha(0.35f));
            drawMasterHeader(g, cardMasterRect_, params);
            drawMasterVisualizer(g, masterVisRect_, params);
        }

    private:
        void drawCardBackground(juce::Graphics& g, const juce::Rectangle<int>& rect, juce::Colour bg, juce::Colour border) {
            const auto r = rect.toFloat();
            g.setColour(bg);
            g.fillRoundedRectangle(r, 6.0f);
            g.setColour(border);
            g.drawRoundedRectangle(r, 6.0f, 1.0f);
        }

        void drawToggleSwitch(juce::Graphics& g, const juce::Rectangle<int>& rect, bool isOn, juce::Colour onColor) {
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
        }

        void drawDelayHeader(juce::Graphics& g, const juce::Rectangle<int>& r, const nova::dsp::WorshipSynthEngine::Parameters& params) {
            g.setFont(theme_.getSectionFont(13.0f));
            g.setColour(params.delayEnabled ? juce::Colour(0xff2dd4bf) : theme_.textMuted);
            g.drawText("STEREO TAPE DELAY", r.getX() + 8, r.getY() + 5, r.getWidth() - 130, 18, juce::Justification::centredLeft);

            // Sync Division Dropdown Pill
            static constexpr std::array<const char*, 8> kDivNames = {
                "1/2", "1/4", "1/4d", "1/8", "1/8d", "1/8t", "1/16", "1/16d"
            };
            const auto sb = delaySyncBtnRect_.toFloat();
            g.setColour(juce::Colour(0xff0d161c));
            g.fillRoundedRectangle(sb, 5.0f);
            g.setColour(params.delaySync ? juce::Colour(0xff2dd4bf).withAlpha(0.85f) : theme_.borderSubtle);
            g.drawRoundedRectangle(sb, 5.0f, 1.2f);

            // Solid filled downward triangle ▼ in bright cyan
            const float arrowX = sb.getRight() - 11.0f;
            const float arrowY = sb.getCentreY();
            juce::Path arrow;
            arrow.startNewSubPath(arrowX - 4.0f, arrowY - 2.5f);
            arrow.lineTo(arrowX + 4.0f, arrowY - 2.5f);
            arrow.lineTo(arrowX, arrowY + 3.0f);
            arrow.closeSubPath();
            g.setColour(params.delaySync ? juce::Colour(0xff2dd4bf) : theme_.textMuted);
            g.fillPath(arrow);

            g.setFont(theme_.getValueFont(11.5f));
            g.setColour(params.delaySync ? juce::Colour(0xff5eead4) : theme_.textMuted);
            const juce::String divText = params.delaySync ? juce::String(kDivNames[static_cast<std::size_t>(std::clamp(params.delaySyncDiv, 0, 7))]) : "FREE";
            g.drawText(divText, delaySyncBtnRect_.withTrimmedRight(14), juce::Justification::centred);

            drawToggleSwitch(g, delayBypRect_, params.delayEnabled, juce::Colour(0xff2dd4bf));
        }

        void drawReverbHeader(juce::Graphics& g, const juce::Rectangle<int>& r, const nova::dsp::WorshipSynthEngine::Parameters& params) {
            const bool revOn = (params.reverbMix > 0.01f);
            g.setFont(theme_.getSectionFont(13.0f));
            g.setColour(revOn ? juce::Colour(0xffc084fc) : theme_.textMuted);
            g.drawText("NOVA SPACE REVERB", r.getX() + 8, r.getY() + 5, r.getWidth() - 180, 18, juce::Justification::centredLeft);

            // Shimmer pitch mode selector dropdown pill
            const auto sp = shimmerPitchBtnRect_.toFloat();
            g.setColour(juce::Colour(0xff1e1b2e));
            g.fillRoundedRectangle(sp, 5.0f);
            g.setColour(params.reverbShimmer > 0.05f ? juce::Colour(0xffc084fc).withAlpha(0.85f) : theme_.borderSubtle);
            g.drawRoundedRectangle(sp, 5.0f, 1.2f);

            // Solid filled downward triangle ▼ in purple/lavender
            const float arrowX = sp.getRight() - 11.0f;
            const float arrowY = sp.getCentreY();
            juce::Path arrow;
            arrow.startNewSubPath(arrowX - 4.0f, arrowY - 2.5f);
            arrow.lineTo(arrowX + 4.0f, arrowY - 2.5f);
            arrow.lineTo(arrowX, arrowY + 3.0f);
            arrow.closeSubPath();
            g.setColour(params.reverbShimmer > 0.05f ? juce::Colour(0xffc084fc) : theme_.textMuted);
            g.fillPath(arrow);

            g.setFont(theme_.getValueFont(11.5f));
            g.setColour(params.reverbShimmer > 0.05f ? juce::Colour(0xffe9d5ff) : theme_.textMuted);

            juce::String pitchText = "+1 OCT";
            switch (params.reverbShimmerPitch) {
                case 0: pitchText = "+1 OCT"; break;
                case 1: pitchText = "+2 OCT"; break;
                case 2: pitchText = "+1+2 OCT"; break;
                case 3: pitchText = "-1 OCT"; break;
                default: break;
            }
            g.drawText(pitchText, shimmerPitchBtnRect_.withTrimmedRight(14), juce::Justification::centred);

            const auto fb = freezeBtnRect_.toFloat();
            if (params.reverbFreeze) {
                g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.28f));
                g.fillRoundedRectangle(fb, 4.0f);
                g.setColour(juce::Colour(0xff38bdf8));
                g.drawRoundedRectangle(fb, 4.0f, 1.2f);
                g.setFont(theme_.getValueFont(10.0f));
                g.setColour(juce::Colour(0xff38bdf8));
            } else {
                g.setColour(juce::Colour(0xff1e1b2e));
                g.fillRoundedRectangle(fb, 4.0f);
                g.setColour(theme_.borderSubtle);
                g.drawRoundedRectangle(fb, 4.0f, 1.0f);
                g.setFont(theme_.getValueFont(10.0f));
                g.setColour(theme_.textMuted);
            }
            g.drawText("FREEZE", freezeBtnRect_, juce::Justification::centred);

            drawToggleSwitch(g, reverbBypRect_, revOn, juce::Colour(0xffc084fc));
        }

        void drawMacrosHeader(juce::Graphics& g, const juce::Rectangle<int>& r, const nova::dsp::WorshipSynthEngine::Parameters& /*params*/) {
            g.setFont(theme_.getSectionFont(13.0f));
            g.setColour(juce::Colour(0xfffbbf24));
            g.drawText("PERFORMANCE MACROS", r.getX() + 8, r.getY() + 5, r.getWidth() - 16, 18, juce::Justification::centredLeft);
        }

        void drawMasterHeader(juce::Graphics& g, const juce::Rectangle<int>& r, const nova::dsp::WorshipSynthEngine::Parameters& /*params*/) {
            g.setFont(theme_.getSectionFont(13.0f));
            g.setColour(juce::Colour(0xff22c55e));
            g.drawText("MASTER", r.getX() + 8, r.getY() + 5, r.getWidth() - 16, 18, juce::Justification::centredLeft);
        }

        void drawDelayVisualizer(juce::Graphics& g, const juce::Rectangle<int>& rect, const nova::dsp::WorshipSynthEngine::Parameters& params) {
            const auto r = rect.toFloat();
            g.setColour(juce::Colour(0xff070d13));
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(params.delayEnabled ? juce::Colour(0xff2dd4bf).withAlpha(0.25f) : theme_.borderSubtle);
            g.drawRoundedRectangle(r, 4.0f, 1.0f);

            if (!params.delayEnabled) {
                g.setFont(theme_.getValueFont(11.0f));
                g.setColour(theme_.textMuted);
                g.drawText("DELAY BYPASSED", rect, juce::Justification::centred);
                return;
            }

            const float midY = r.getCentreY();
            g.setColour(juce::Colour(0xff162530));
            g.drawHorizontalLine(static_cast<int>(midY), r.getX() + 4.0f, r.getRight() - 4.0f);

            g.setFont(theme_.getValueFont(11.0f));
            g.setColour(juce::Colour(0xff2dd4bf).withAlpha(0.70f));
            g.drawText("L", static_cast<int>(r.getX() + 6), static_cast<int>(r.getY() + 3), 14, 14, juce::Justification::centredLeft);
            g.drawText("R", static_cast<int>(r.getX() + 6), static_cast<int>(midY + 4), 14, 14, juce::Justification::centredLeft);

            const float effSec = params.delaySync
                ? nova::dsp::WorshipSynthEngine::getDelayTimeForDivision(params.bpm, params.delaySyncDiv)
                : params.delayTimeSec;
            const float normT = std::clamp((effSec - 0.05f) / 1.45f, 0.05f, 1.0f);
            const float dx = 20.0f + normT * 24.0f;
            const float fdbk = std::clamp(params.delayFeedback, 0.1f, 0.92f);
            const int pulseTap = static_cast<int>(std::fmod(phase_ * (1.6f / std::max(0.12f, effSec)), 6.0f));
            const float swing = params.delaySwing; // 0.0 to 1.0

            for (int i = 0; i < 6; ++i) {
                const bool isL = (i % 2 == 0);
                const float swingOffset = (!isL) ? (swing * dx * 0.333f) : 0.0f;
                const float px = r.getX() + 26.0f + static_cast<float>(i) * dx + swingOffset;
                if (px > r.getRight() - 14.0f) break;

                const float amp = std::pow(fdbk, static_cast<float>(i) * 0.65f);
                const float barH = std::max(4.0f, (r.getHeight() * 0.38f) * amp);
                const float barY = isL ? (midY - barH) : midY;
                const bool isPulse = (i == pulseTap);

                const juce::Rectangle<float> tapBar(px, barY, 5.0f, barH);
                g.setColour(isPulse ? juce::Colour(0xff67e8f9) : juce::Colour(0xff2dd4bf).withAlpha(0.60f * amp + 0.25f));
                g.fillRoundedRectangle(tapBar, 2.0f);
                if (isPulse) {
                    g.setColour(juce::Colour(0xff67e8f9).withAlpha(0.35f));
                    g.drawRoundedRectangle(tapBar.expanded(2.0f, 1.0f), 3.0f, 1.0f);
                }
            }

            g.setFont(theme_.getValueFont(11.0f));
            g.setColour(juce::Colour(0xff2dd4bf));
            static constexpr std::array<const char*, 8> kDivs = { "1/2", "1/4", "1/4d", "1/8", "1/8d", "1/8t", "1/16", "1/16d" };
            const juce::String timeLabel = params.delaySync
                ? (juce::String(kDivs[static_cast<std::size_t>(std::clamp(params.delaySyncDiv, 0, 7))]) + " (" + juce::String(static_cast<int>(effSec * 1000.0f)) + " ms)")
                : (juce::String(static_cast<int>(params.delayTimeSec * 1000.0f)) + " ms");
            const juce::String swingLabel = (params.delaySwing > 0.01f)
                ? (" | Swing: " + juce::String(static_cast<int>(params.delaySwing * 100.0f)) + "%")
                : " | Straight";
            g.drawText(timeLabel + swingLabel + " | " + juce::String(static_cast<int>(params.delayMix * 100.0f)) + "%",
                       rect.reduced(6, 2), juce::Justification::bottomRight);
        }

        void drawReverbVisualizer(juce::Graphics& g, const juce::Rectangle<int>& rect, const nova::dsp::WorshipSynthEngine::Parameters& params) {
            const auto r = rect.toFloat();
            g.setColour(juce::Colour(0xff0c0617));
            g.fillRoundedRectangle(r, 4.0f);
            const bool revOn = (params.reverbMix > 0.01f);
            g.setColour(revOn ? juce::Colour(0xffc084fc).withAlpha(0.25f) : theme_.borderSubtle);
            g.drawRoundedRectangle(r, 4.0f, 1.0f);

            if (!revOn) {
                g.setFont(theme_.getValueFont(11.0f));
                g.setColour(theme_.textMuted);
                g.drawText("REVERB BYPASSED", rect, juce::Justification::centred);
                return;
            }

            const float startX = r.getX() + 6.0f;
            const float endX = r.getRight() - 6.0f;
            const float baseW = endX - startX;
            const float curveH = r.getHeight() - 8.0f;
            const float bottomY = r.getBottom() - 4.0f;
            const float tau = 0.25f + (params.reverbDecay / 28.0f) * 0.75f;

            juce::Path decayPath, decayFill;
            decayPath.startNewSubPath(startX, bottomY - curveH * 0.88f);
            decayFill.startNewSubPath(startX, bottomY);
            decayFill.lineTo(startX, bottomY - curveH * 0.88f);

            for (float step = 1.0f; step <= 36.0f; step += 1.0f) {
                const float u = step / 36.0f;
                const float px = startX + u * baseW;
                float v = 0.0f;
                if (params.reverbFreeze) {
                    v = 0.82f + 0.05f * std::sin(u * 14.0f + phase_ * 2.5f);
                } else {
                    v = std::exp(-u / tau) * (0.85f + 0.04f * std::sin(u * 16.0f + phase_ * 1.5f));
                }
                const float py = bottomY - std::clamp(v, 0.04f, 0.94f) * curveH;
                decayPath.lineTo(px, py);
                decayFill.lineTo(px, py);
            }
            decayFill.lineTo(endX, bottomY);
            decayFill.closeSubPath();

            juce::ColourGradient fillGrad(
                params.reverbFreeze ? juce::Colour(0xff38bdf8).withAlpha(0.35f) : juce::Colour(0xffc084fc).withAlpha(0.28f),
                startX, r.getY(),
                juce::Colour(0xff0c0617).withAlpha(0.02f),
                startX, bottomY, false);
            g.setGradientFill(fillGrad);
            g.fillPath(decayFill);

            g.setColour(params.reverbFreeze ? juce::Colour(0xff67e8f9) : juce::Colour(0xffd8b4fe));
            g.strokePath(decayPath, juce::PathStrokeType(1.4f));

            if (params.reverbShimmer > 0.05f) {
                const int numStars = static_cast<int>(3 + params.reverbShimmer * 5.0f);
                for (int s = 0; s < numStars; ++s) {
                    const float su = std::fmod(static_cast<float>(s) * 0.23f + phase_ * 0.04f, 0.95f);
                    const float sv = std::fmod(static_cast<float>(s) * 0.31f + phase_ * 0.06f, 0.85f);
                    const float sx = startX + su * baseW;
                    const float sy = bottomY - sv * curveH;
                    juce::Path star;
                    const float sz = 3.2f + params.reverbShimmer * 2.0f;
                    star.startNewSubPath(sx, sy - sz);
                    star.lineTo(sx + sz * 0.45f, sy);
                    star.lineTo(sx, sy + sz);
                    star.lineTo(sx - sz * 0.45f, sy);
                    star.closeSubPath();
                    g.setColour(juce::Colour(0xfffde047).withAlpha(0.75f * params.reverbShimmer));
                    g.fillPath(star);
                }
            }

            g.setFont(theme_.getValueFont(9.5f));
            if (params.reverbFreeze) {
                g.setColour(juce::Colour(0xff38bdf8));
                g.drawText("HOLD INFINITE", rect.reduced(6, 2), juce::Justification::bottomRight);
            } else {
                g.setColour(juce::Colour(0xffc084fc).withAlpha(0.85f));
                const int dryPct = static_cast<int>(std::round(params.reverbDry * 100.0f));
                const int wetPct = static_cast<int>(std::round(params.reverbMix * 100.0f));
                g.drawText(juce::String(params.reverbDecay, 1) + " s | D: " + juce::String(dryPct) + "% W: " + juce::String(wetPct) + "%",
                           rect.reduced(6, 2), juce::Justification::bottomRight);
            }
        }

        void drawMacrosVisualizer(juce::Graphics& g, const juce::Rectangle<int>& rect, const nova::dsp::WorshipSynthEngine::Parameters& params) {
            const auto r = rect.toFloat();
            g.setColour(juce::Colour(0xff140d04));
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(juce::Colour(0xfffbbf24).withAlpha(0.25f));
            g.drawRoundedRectangle(r, 4.0f, 1.0f);

            const float startX = r.getX() + 6.0f;
            const float endX = r.getRight() - 6.0f;
            const float baseW = endX - startX;
            const float curveH = r.getHeight() - 8.0f;
            const float bottomY = r.getBottom() - 4.0f;

            static constexpr std::array<float, 3> kFreqs = { 0.22f, 0.54f, 0.84f };
            static constexpr std::array<const char*, 3> kLabels = { "100", "1k", "10k" };
            g.setFont(theme_.getValueFont(8.0f));
            for (std::size_t fi = 0; fi < 3; ++fi) {
                const float fx = startX + kFreqs[fi] * baseW;
                g.setColour(juce::Colour(0xfffbbf24).withAlpha(0.12f));
                g.drawVerticalLine(static_cast<int>(fx), r.getY() + 4.0f, r.getBottom() - 4.0f);
                g.setColour(theme_.textMuted.withAlpha(0.5f));
                g.drawText(kLabels[fi], static_cast<int>(fx - 12), static_cast<int>(r.getBottom() - 14), 24, 10, juce::Justification::centred);
            }

            juce::Path curvePath, fillPath;
            auto getGainAt = [&](float u) {
                const float gw = params.macroWarmth * std::exp(-u * 4.0f) * 0.38f;
                const float gm = params.macroMotion * 0.12f * std::sin(u * 16.0f - phase_ * 2.5f) * std::sin(u * 3.14159f);
                const float diff = (u - 0.70f) / 0.14f;
                const float gs = params.macroShimmer * 0.28f * std::exp(-diff * diff);
                const float ga = params.macroAir * 0.30f * (1.0f / (1.0f + std::exp(-(u - 0.76f) * 10.0f)));
                return 0.28f + gw + gm + gs + ga;
            };

            curvePath.startNewSubPath(startX, bottomY - std::clamp(getGainAt(0.0f), 0.06f, 0.94f) * curveH);
            fillPath.startNewSubPath(startX, bottomY);
            fillPath.lineTo(startX, bottomY - std::clamp(getGainAt(0.0f), 0.06f, 0.94f) * curveH);

            for (float step = 1.0f; step <= 36.0f; step += 1.0f) {
                const float u = step / 36.0f;
                const float px = startX + u * baseW;
                const float py = bottomY - std::clamp(getGainAt(u), 0.06f, 0.94f) * curveH;
                curvePath.lineTo(px, py);
                fillPath.lineTo(px, py);
            }
            fillPath.lineTo(endX, bottomY);
            fillPath.closeSubPath();

            juce::ColourGradient fillGrad(juce::Colour(0xfffbbf24).withAlpha(0.24f), startX, r.getY(),
                                         juce::Colour(0xff140d04).withAlpha(0.02f), startX, bottomY, false);
            g.setGradientFill(fillGrad);
            g.fillPath(fillPath);

            g.setColour(juce::Colour(0xfffbbf24));
            g.strokePath(curvePath, juce::PathStrokeType(1.5f));

            static constexpr std::array<std::pair<float, const char*>, 4> kNodes = {{
                { 0.12f, "W" }, { 0.40f, "M" }, { 0.70f, "S" }, { 0.90f, "A" }
            }};
            g.setFont(theme_.getValueFont(9.0f));
            for (const auto& [nu, nlabel] : kNodes) {
                const float nx = startX + nu * baseW;
                const float ny = bottomY - std::clamp(getGainAt(nu), 0.06f, 0.94f) * curveH;
                g.setColour(juce::Colour(0xfffbbf24));
                g.fillEllipse(nx - 2.5f, ny - 2.5f, 5.0f, 5.0f);
                g.setColour(juce::Colour(0xfffef08a));
                g.drawText(nlabel, static_cast<int>(nx - 8), static_cast<int>(ny - 14), 16, 12, juce::Justification::centred);
            }
        }

        void drawMasterVisualizer(juce::Graphics& g, const juce::Rectangle<int>& rect, const nova::dsp::WorshipSynthEngine::Parameters& /*params*/) {
            const auto r = rect.toFloat();
            g.setColour(juce::Colour(0xff06130b));
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(juce::Colour(0xff22c55e).withAlpha(0.25f));
            g.drawRoundedRectangle(r, 4.0f, 1.0f);

            const bool isClipping = (smoothPeakL_ > 0.98f || smoothPeakR_ > 0.98f);
            const float ledX = r.getRight() - 12.0f;
            const float ledY = r.getY() + 7.0f;
            g.setColour(isClipping ? juce::Colour(0xffef4444) : juce::Colour(0xff1f2937));
            g.fillEllipse(ledX - 3.0f, ledY - 3.0f, 6.0f, 6.0f);
            if (isClipping) {
                g.setColour(juce::Colour(0xffef4444).withAlpha(0.35f));
                g.fillEllipse(ledX - 5.0f, ledY - 5.0f, 10.0f, 10.0f);
            }
            g.setFont(theme_.getValueFont(8.5f));
            g.setColour(isClipping ? juce::Colour(0xffef4444) : theme_.textMuted);
            g.drawText("CLIP", static_cast<int>(ledX - 28), static_cast<int>(ledY - 5), 24, 10, juce::Justification::centredRight);

            const float barLeft = r.getX() + 18.0f;
            const float barRight = r.getRight() - 8.0f;
            const float barW = barRight - barLeft;
            const float lY = r.getY() + 22.0f;
            const float rY = r.getY() + 36.0f;
            const float barH = 8.0f;

            g.setFont(theme_.getValueFont(9.5f));
            g.setColour(juce::Colour(0xff22c55e).withAlpha(0.65f));
            g.drawText("L", static_cast<int>(r.getX() + 4), static_cast<int>(lY - 1), 12, 10, juce::Justification::centredLeft);
            g.drawText("R", static_cast<int>(r.getX() + 4), static_cast<int>(rY - 1), 12, 10, juce::Justification::centredLeft);

            auto drawBar = [&](float y, float peakVal, float holdVal) {
                const juce::Rectangle<float> bgBar(barLeft, y, barW, barH);
                g.setColour(juce::Colour(0xff102017));
                g.fillRoundedRectangle(bgBar, 2.0f);

                const float pNorm = std::clamp(std::sqrt(peakVal), 0.0f, 1.0f);
                if (pNorm > 0.01f) {
                    const juce::Rectangle<float> actBar(barLeft, y, barW * pNorm, barH);
                    juce::Colour barCol = juce::Colour(0xff22c55e);
                    if (pNorm > 0.92f) barCol = juce::Colour(0xffef4444);
                    else if (pNorm > 0.70f) barCol = juce::Colour(0xfff59e0b);
                    g.setColour(barCol);
                    g.fillRoundedRectangle(actBar, 2.0f);
                }

                const float hNorm = std::clamp(std::sqrt(holdVal), 0.0f, 1.0f);
                if (hNorm > 0.02f) {
                    const float hx = barLeft + hNorm * barW;
                    g.setColour(juce::Colours::white);
                    g.drawVerticalLine(static_cast<int>(hx), y, y + barH);
                }
            };

            drawBar(lY, smoothPeakL_, peakHoldL_);
            drawBar(rY, smoothPeakR_, peakHoldR_);

            // Scale calibration tick markings
            static constexpr std::array<std::pair<float, const char*>, 4> kTicks = {{
                { 0.25f, "-24" }, { 0.50f, "-12" }, { 0.71f, "-6" }, { 1.00f, "0" }
            }};
            g.setFont(theme_.getValueFont(8.0f));
            g.setColour(theme_.textMuted.withAlpha(0.6f));
            for (const auto& [frac, tlabel] : kTicks) {
                const float tx = barLeft + frac * barW;
                g.drawVerticalLine(static_cast<int>(tx), rY + barH + 1.0f, rY + barH + 4.0f);
                g.drawText(tlabel, static_cast<int>(tx - 10.0f), static_cast<int>(rY + barH + 4.0f), 20, 9, juce::Justification::centred);
            }

            const float maxPeak = std::max(smoothPeakL_, smoothPeakR_);
            const float dbVal = (maxPeak > 1e-4f) ? (20.0f * std::log10(maxPeak)) : -60.0f;
            g.setFont(theme_.getValueFont(9.5f));
            g.setColour(juce::Colour(0xff22c55e).withAlpha(0.9f));
            g.drawText((dbVal > -59.0f ? juce::String(dbVal, 1) + " dB" : "-INF"),
                       rect.reduced(6, 3), juce::Justification::bottomLeft);
        }

        NovaSynthProcessor& proc_;
        const nova::ui::NovaTheme& theme_;

        nova::ui::ArcKnob delaySwing_;
        nova::ui::ArcKnob delayFdbk_;
        nova::ui::ArcKnob delayMix_;

        nova::ui::ArcKnob revDry_;
        nova::ui::ArcKnob revMix_;
        nova::ui::ArcKnob revDecay_;
        nova::ui::ArcKnob revShimmer_;
        nova::ui::ArcKnob revDucking_;

        nova::ui::ArcKnob macroWarmth_;
        nova::ui::ArcKnob macroMotion_;
        nova::ui::ArcKnob macroShimmer_;
        nova::ui::ArcKnob macroAir_;

        nova::ui::ArcKnob masterGain_;

        juce::Rectangle<int> cardDelayRect_;
        juce::Rectangle<int> cardReverbRect_;
        juce::Rectangle<int> cardMacrosRect_;
        juce::Rectangle<int> cardMasterRect_;

        juce::Rectangle<int> delayBypRect_;
        juce::Rectangle<int> delaySyncBtnRect_;
        juce::Rectangle<int> reverbBypRect_;
        juce::Rectangle<int> freezeBtnRect_;
        juce::Rectangle<int> shimmerPitchBtnRect_;

        juce::Rectangle<int> delayVisRect_;
        juce::Rectangle<int> reverbVisRect_;
        juce::Rectangle<int> macroVisRect_;
        juce::Rectangle<int> masterVisRect_;

        float phase_ = 0.0f;
        float cachedReverbMix_ = 0.42f;
        float smoothPeakL_ = 0.0f;
        float smoothPeakR_ = 0.0f;
        float peakHoldL_ = 0.0f;
        float peakHoldR_ = 0.0f;
        int holdCountL_ = 0;
        int holdCountR_ = 0;
    };

    void drawToggleSwitch(juce::Graphics& g, const juce::Rectangle<int>& rect, bool isOn, juce::Colour onColor) {
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

    void drawJackPin(juce::Graphics& g, juce::Point<float> pt, juce::Colour col, bool isConnected, bool isHovered) {
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

    void drawPatchCable(juce::Graphics& g, juce::Point<float> p1, juce::Point<float> p2, juce::Colour col, bool isBypass = false, float bypassY = 0.0f, bool isDimmed = false) {
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
            // Muted, non-glowing pass-through cable in dark slate (shows untouched audio pass-through)
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

    void drawSignalFlowView(juce::Graphics& g, juce::Rectangle<float> area) {
        flowOscPillRects_.clear();
        flowOscRingRects_.clear();
        flowOscJackRects_.clear();
        oscOutPins_.clear();

        const auto params = proc_.getCurrentParameters();
        const auto tel = proc_.getSynthTelemetry();

        const float gap = 8.0f;
        const float wOutput = 126.0f;
        const float availW = area.getWidth() - wOutput - gap * 4.0f;
        const float wSources = std::round(availW * 0.23f);
        const float wFilters = std::round(availW * 0.28f);
        const float wAmp     = std::round(availW * 0.24f);
        const float wFx      = availW - (wSources + wFilters + wAmp);
        const float H = area.getHeight();

        const juce::Rectangle<float> cardSources(area.getX(), area.getY(), wSources, H);
        const juce::Rectangle<float> cardFilters(cardSources.getRight() + gap, area.getY(), wFilters, H);
        const juce::Rectangle<float> cardAmp    (cardFilters.getRight() + gap, area.getY(), wAmp, H);
        const juce::Rectangle<float> cardFx     (cardAmp.getRight() + gap, area.getY(), wFx, H);
        const juce::Rectangle<float> cardOutput (cardFx.getRight() + gap, area.getY(), wOutput, H);

        cardSourcesRect_ = cardSources.toNearestInt();
        cardAmpRect_     = cardAmp.toNearestInt();

        // =====================================================================
        // 1. SOURCES CARD (Navy Midnight)
        // =====================================================================
        g.setColour(juce::Colour(0xff0d1522));
        g.fillRoundedRectangle(cardSources, 8.0f);
        g.setColour(juce::Colour(0xff1e3a5f).withAlpha(0.7f));
        g.drawRoundedRectangle(cardSources, 8.0f, 1.0f);

        // Header Title: 15.0px Bold
        g.setFont(theme_.getTitleFont(15.0f));
        g.setColour(juce::Colour(0xff38bdf8));
        g.drawText("SOURCES", static_cast<int>(cardSources.getX() + 10), static_cast<int>(cardSources.getY() + 7), 100, 18, juce::Justification::centredLeft);

        // Header Toggle Switch
        toggleSourcesRect_ = juce::Rectangle<int>(static_cast<int>(cardSources.getRight() - 36), static_cast<int>(cardSources.getY() + 8), 28, 15);
        bool anySourcesOn = false;
        for (int i = 0; i < std::clamp(params.activeOscCount, 1, 8); ++i) {
            if (params.oscSlots[static_cast<std::size_t>(i)].enabled) anySourcesOn = true;
        }
        drawToggleSwitch(g, toggleSourcesRect_, anySourcesOn, juce::Colour(0xff38bdf8));

        // Display up to 6 OSCs simultaneously, scrollable if > 6 (total 8 slots)
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

            // Pill background
            g.setColour(juce::Colour(0xff131c2a));
            g.fillRoundedRectangle(pillR, 5.0f);

            // Glowing border if selected or active
            if (isSel) {
                g.setColour(juce::Colour(0xff38bdf8));
                g.drawRoundedRectangle(pillR, 5.0f, 1.8f);
            } else {
                g.setColour(isEnabled ? col.withAlpha(0.5f) : juce::Colour(0xff1e293b));
                g.drawRoundedRectangle(pillR, 5.0f, 1.0f);
            }

            // Concentric Ring on left (O)
            const juce::Rectangle<int> ringR(static_cast<int>(pillR.getX() + 5), static_cast<int>(pillR.getCentreY() - 6.0f), 12, 12);
            flowOscRingRects_.push_back(ringR);
            g.setColour(isEnabled ? col : juce::Colour(0xff475569));
            g.drawEllipse(ringR.toFloat().reduced(0.8f), 1.5f);
            if (isEnabled) {
                g.fillEllipse(ringR.toFloat().reduced(3.8f));
            }

            // Layer Label: bold 12.5px
            g.setFont(theme_.getSectionFont(12.5f));
            g.setColour(isEnabled ? (isSel ? juce::Colours::white : col) : theme_.textMuted);
            g.drawText(getLayerShortName(params, i), static_cast<int>(pillR.getX() + 22), static_cast<int>(pillR.getY()), 65, static_cast<int>(pillR.getHeight()), juce::Justification::centredLeft);

            // Engine type badge
            g.setFont(theme_.getValueFont(10.5f));
            g.setColour(theme_.textMuted);
            g.drawText(getEngineDisplayName(params, i), static_cast<int>(pillR.getX() + 84), static_cast<int>(pillR.getY()), static_cast<int>(pillR.getWidth() - 98), static_cast<int>(pillR.getHeight()), juce::Justification::centredLeft);

            // Right-hand Output Jack pin
            const juce::Point<float> outPin(pillR.getRight() - 2.0f, pillR.getCentreY());
            oscOutPins_.push_back(outPin);
            const juce::Rectangle<int> jackHit(static_cast<int>(outPin.x - 7.0f), static_cast<int>(outPin.y - 7.0f), 14, 14);
            flowOscJackRects_.push_back(jackHit);

            const bool isDraggingThis = (isDraggingCable_ && dragSourceOscIndex_ == i);
            drawJackPin(g, outPin, col, isEnabled, isDraggingThis);
        }

        // Dedicated sleek scroll track and thumb
        const float trackX = cardSources.getRight() - 14.0f;
        const float trackY = cardSources.getY() + 32.0f;
        const float trackH = static_cast<float>(visibleCount) * (pillH + pillGap) - pillGap;
        scrollTrackRect_ = juce::Rectangle<int>(static_cast<int>(trackX - 2.0f), static_cast<int>(trackY), 9, static_cast<int>(trackH));

        // Background track
        g.setColour(juce::Colour(0xff1e293b).withAlpha(0.6f));
        g.fillRoundedRectangle(trackX, trackY, 5.0f, trackH, 2.5f);

        // Thumb
        const float scrollThumbH = trackH * (static_cast<float>(visibleCount) / static_cast<float>(totalSources));
        const float scrollMaxTravel = trackH - scrollThumbH;
        const float scrollThumbY = trackY + (static_cast<float>(sourceScrollOffset_) / static_cast<float>(maxScroll)) * scrollMaxTravel;
        g.setColour(theme_.accentPrimary.withAlpha(0.85f));
        g.fillRoundedRectangle(trackX, scrollThumbY, 5.0f, scrollThumbH, 2.5f);

        // Scroll chevron buttons at top and bottom of track
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

        // =====================================================================
        // 2. FILTERS CARD (Dark Teal)
        // =====================================================================
        g.setColour(juce::Colour(0xff091a20));
        g.fillRoundedRectangle(cardFilters, 8.0f);
        g.setColour(juce::Colour(0xff134e4a).withAlpha(0.7f));
        g.drawRoundedRectangle(cardFilters, 8.0f, 1.0f);

        // Header Title: 15.0px Bold
        g.setFont(theme_.getTitleFont(15.0f));
        g.setColour(juce::Colour(0xff2dd4bf));
        g.drawText("FILTERS", static_cast<int>(cardFilters.getX() + 10), static_cast<int>(cardFilters.getY() + 7), 80, 18, juce::Justification::centredLeft);

        // Header Mod Badge: [MOD: ENV 2]
        const juce::Rectangle<float> fltModBadge(cardFilters.getX() + 84.0f, cardFilters.getY() + 7.5f, 86.0f, 17.0f);
        g.setColour(juce::Colour(0xff134e4a).withAlpha(0.5f));
        g.fillRoundedRectangle(fltModBadge, 4.0f);
        g.setColour(juce::Colour(0xff2dd4bf).withAlpha(0.6f));
        g.drawRoundedRectangle(fltModBadge, 4.0f, 1.0f);
        g.setFont(theme_.getValueFont(10.5f));
        g.setColour(juce::Colour(0xff2dd4bf));
        g.drawText("MOD: ENV 2", fltModBadge, juce::Justification::centred);

        // Area Global Toggle Switch
        toggleFiltersRect_ = juce::Rectangle<int>(static_cast<int>(cardFilters.getRight() - 36), static_cast<int>(cardFilters.getY() + 8), 28, 15);
        const bool filtersOn = params.filter1Enabled || params.filter2Enabled;
        drawToggleSwitch(g, toggleFiltersRect_, filtersOn, juce::Colour(0xfff59e0b));

        // FLT 1 Pill
        const float fltW = cardFilters.getWidth() - 20.0f;
        const float fltH = 34.0f;
        const juce::Rectangle<float> flt1R(cardFilters.getX() + 10.0f, cardFilters.getY() + 32.0f, fltW, fltH);
        flt1PillRect_ = flt1R.toNearestInt();
        g.setColour(juce::Colour(0xff10242b));
        g.fillRoundedRectangle(flt1R, 6.0f);
        const bool flt1Hov = (isDraggingCable_ && (hoveredTarget_ == 0 || hoveredTarget_ == 2));
        g.setColour(flt1Hov ? juce::Colour(0xff2dd4bf) : (params.filter1Enabled ? juce::Colour(0xff2dd4bf).withAlpha(0.7f) : theme_.borderSubtle));
        g.drawRoundedRectangle(flt1R, 6.0f, flt1Hov ? 2.0f : 1.0f);

        // Individual FLT 1 Bypass Toggle (RIGHT, directly left of Connection Point!)
        flt1ToggleRect_ = juce::Rectangle<int>(static_cast<int>(flt1R.getRight() - 36), static_cast<int>(flt1R.getCentreY() - 6.5f), 22, 13);
        drawToggleSwitch(g, flt1ToggleRect_, params.filter1Enabled, juce::Colour(0xff2dd4bf));

        // FLT 1 In Pin (far left)
        flt1InPin_  = { flt1R.getX() + 4.0f, flt1R.getCentreY() };
        drawJackPin(g, flt1InPin_, juce::Colour(0xff2dd4bf), params.filter1Enabled, flt1Hov);

        // FLT 1 Labels
        g.setFont(theme_.getSectionFont(13.0f));
        g.setColour(params.filter1Enabled ? juce::Colour(0xff2dd4bf) : theme_.textMuted);
        g.drawText("FLT 1", static_cast<int>(flt1R.getX() + 18), static_cast<int>(flt1R.getY() + 2), 45, 16, juce::Justification::centredLeft);
        g.setFont(theme_.getValueFont(11.5f));
        g.setColour(theme_.textSecondary);
        g.drawText(juce::String(static_cast<int>(params.filterCutoffHz)) + " Hz", static_cast<int>(flt1R.getX() + 18), static_cast<int>(flt1R.getY() + 17), 75, 14, juce::Justification::centredLeft);

        // FLT 1 Out Pin (far right)
        flt1OutPin_ = { flt1R.getRight() - 6.0f, flt1R.getCentreY() };
        drawJackPin(g, flt1OutPin_, juce::Colour(0xff2dd4bf), params.filter1Enabled, false);

        // DUAL (FLT 1+2) Bridge Zone
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
        g.setColour(dualHov ? juce::Colour(0xff2dd4bf) : theme_.textMuted);
        g.drawText("DUAL (FLT 1 + 2)", dualTargetRect_, juce::Justification::centred);

        // FLT 2 Pill
        const juce::Rectangle<float> flt2R(cardFilters.getX() + 10.0f, static_cast<float>(dualTargetRect_.getBottom() + 3), fltW, fltH);
        flt2PillRect_ = flt2R.toNearestInt();
        g.setColour(juce::Colour(0xff10242b));
        g.fillRoundedRectangle(flt2R, 6.0f);
        const bool flt2Hov = (isDraggingCable_ && (hoveredTarget_ == 1 || hoveredTarget_ == 2));
        g.setColour(flt2Hov ? juce::Colour(0xff2dd4bf) : (params.filter2Enabled ? juce::Colour(0xff2dd4bf).withAlpha(0.7f) : theme_.borderSubtle));
        g.drawRoundedRectangle(flt2R, 6.0f, flt2Hov ? 2.0f : 1.0f);

        // Individual FLT 2 Bypass Toggle (RIGHT, directly left of Connection Point!)
        flt2ToggleRect_ = juce::Rectangle<int>(static_cast<int>(flt2R.getRight() - 36), static_cast<int>(flt2R.getCentreY() - 6.5f), 22, 13);
        drawToggleSwitch(g, flt2ToggleRect_, params.filter2Enabled, juce::Colour(0xff2dd4bf));

        // FLT 2 In Pin (far left)
        flt2InPin_  = { flt2R.getX() + 4.0f, flt2R.getCentreY() };
        drawJackPin(g, flt2InPin_, juce::Colour(0xff2dd4bf), params.filter2Enabled, flt2Hov);

        // FLT 2 Labels
        g.setFont(theme_.getSectionFont(13.0f));
        g.setColour(params.filter2Enabled ? juce::Colour(0xff2dd4bf) : theme_.textMuted);
        g.drawText("FLT 2", static_cast<int>(flt2R.getX() + 18), static_cast<int>(flt2R.getY() + 2), 45, 16, juce::Justification::centredLeft);
        g.setFont(theme_.getValueFont(11.5f));
        g.setColour(theme_.textSecondary);
        g.drawText("Carve: " + juce::String(static_cast<int>(params.vocalCarve * 100.0f)) + "%", static_cast<int>(flt2R.getX() + 18), static_cast<int>(flt2R.getY() + 17), 80, 14, juce::Justification::centredLeft);

        // FLT 2 Out Pin (far right)
        flt2OutPin_ = { flt2R.getRight() - 6.0f, flt2R.getCentreY() };
        drawJackPin(g, flt2OutPin_, juce::Colour(0xff2dd4bf), params.filter2Enabled, false);

        // Lower Dedicated Bypass Gutter (Under FLT 2)
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

        // =====================================================================
        // 3. AMP VCA CARD (Warm Amber) - Shaped by ENV 1 (Amp Envelope)
        // =====================================================================
        g.setColour(juce::Colour(0xff1d140e));
        g.fillRoundedRectangle(cardAmp, 8.0f);
        g.setColour(juce::Colour(0xff78350f).withAlpha(0.7f));
        g.drawRoundedRectangle(cardAmp, 8.0f, 1.0f);

        // Common straight horizontal axis for AMP -> DELAY -> REVERB -> OUTPUT
        const float commonFxY = cardFx.getY() + 66.0f;

        // Header Title: 15.0px Bold
        g.setFont(theme_.getTitleFont(15.0f));
        g.setColour(juce::Colour(0xfffb923c));
        g.drawText("AMP VCA", static_cast<int>(cardAmp.getX() + 10), static_cast<int>(cardAmp.getY() + 7), 80, 18, juce::Justification::centredLeft);

        // Header Badge: [ENV 1]
        const juce::Rectangle<float> ampBadge(cardAmp.getX() + 86.0f, cardAmp.getY() + 7.5f, 54.0f, 17.0f);
        g.setColour(juce::Colour(0xff78350f).withAlpha(0.5f));
        g.fillRoundedRectangle(ampBadge, 4.0f);
        g.setColour(juce::Colour(0xfffb923c).withAlpha(0.6f));
        g.drawRoundedRectangle(ampBadge, 4.0f, 1.0f);
        g.setFont(theme_.getValueFont(10.5f));
        g.setColour(juce::Colour(0xfffb923c));
        g.drawText("ENV 1", ampBadge, juce::Justification::centred);

        // Input 1: Filter Sum Audio In
        const juce::Rectangle<float> ampFltR(cardAmp.getX() + 8.0f, cardAmp.getY() + 32.0f, cardAmp.getWidth() * 0.52f, 26.0f);
        g.setColour(juce::Colour(0xff291811));
        g.fillRoundedRectangle(ampFltR, 5.0f);
        g.setColour(juce::Colour(0xff78350f).withAlpha(0.5f));
        g.drawRoundedRectangle(ampFltR, 5.0f, 1.0f);

        ampFltInPin_ = { ampFltR.getX() + 4.0f, ampFltR.getCentreY() };
        drawJackPin(g, ampFltInPin_, juce::Colour(0xfffb923c), true, false);
        g.setFont(theme_.getSectionFont(11.5f));
        g.setColour(juce::Colour(0xfffb923c));
        g.drawText("FLT IN", static_cast<int>(ampFltR.getX() + 14), static_cast<int>(ampFltR.getY()), static_cast<int>(ampFltR.getWidth() - 16), static_cast<int>(ampFltR.getHeight()), juce::Justification::centredLeft);

        // Input 2: Direct Audio In (Bypasses Filters directly into AMP)
        const juce::Rectangle<float> ampDirectR(cardAmp.getX() + 8.0f, cardAmp.getY() + 62.0f, cardAmp.getWidth() * 0.52f, 26.0f);
        const bool ampDirectHov = (isDraggingCable_ && hoveredTarget_ == 3);
        g.setColour(juce::Colour(0xff291811));
        g.fillRoundedRectangle(ampDirectR, 5.0f);
        g.setColour(ampDirectHov ? juce::Colour(0xff38bdf8) : juce::Colour(0xff78350f).withAlpha(0.5f));
        g.drawRoundedRectangle(ampDirectR, 5.0f, ampDirectHov ? 1.8f : 1.0f);

        ampDirectInPin_ = { ampDirectR.getX() + 4.0f, ampDirectR.getCentreY() };
        drawJackPin(g, ampDirectInPin_, juce::Colour(0xff38bdf8), true, ampDirectHov);
        g.setFont(theme_.getSectionFont(11.5f));
        g.setColour(ampDirectHov ? juce::Colour(0xff38bdf8) : theme_.textSecondary);
        g.drawText("DIRECT IN", static_cast<int>(ampDirectR.getX() + 14), static_cast<int>(ampDirectR.getY()), static_cast<int>(ampDirectR.getWidth() - 16), static_cast<int>(ampDirectR.getHeight()), juce::Justification::centredLeft);

        // Main Audio Output Pin from AMP VCA into FX (aligned to common horizontal axis!)
        const juce::Rectangle<float> ampOutR(cardAmp.getRight() - cardAmp.getWidth() * 0.38f, commonFxY - 13.0f, cardAmp.getWidth() * 0.38f - 8.0f, 26.0f);
        g.setColour(juce::Colour(0xff291811));
        g.fillRoundedRectangle(ampOutR, 5.0f);
        g.setColour(juce::Colour(0xfffb923c).withAlpha(0.6f));
        g.drawRoundedRectangle(ampOutR, 5.0f, 1.0f);

        ampOutPin_ = { ampOutR.getRight() - 4.0f, commonFxY };
        drawJackPin(g, ampOutPin_, juce::Colour(0xfffb923c), true, false);
        g.setFont(theme_.getSectionFont(11.5f));
        g.setColour(juce::Colour(0xfffb923c));
        g.drawText("OUT", static_cast<int>(ampOutR.getX() + 6), static_cast<int>(ampOutR.getY()), static_cast<int>(ampOutR.getWidth() - 16), static_cast<int>(ampOutR.getHeight()), juce::Justification::centredLeft);

        // Miniature ADSR Curve Preview Box (1:1 identical Bézier curves as upper ENV 1 editor)
        const juce::Rectangle<float> adsrBox(cardAmp.getX() + 8.0f, cardAmp.getY() + 94.0f, cardAmp.getWidth() - 16.0f, cardAmp.getBottom() - (cardAmp.getY() + 94.0f) - 8.0f);
        g.setColour(juce::Colour(0xff120d09));
        g.fillRoundedRectangle(adsrBox, 5.0f);
        g.setColour(juce::Colour(0xff451a03).withAlpha(0.5f));
        g.drawRoundedRectangle(adsrBox, 5.0f, 0.8f);

        // Plot ADSR Curve with exact Bézier mathematics matching the upper editor
        auto& apvts = proc_.getAPVTS();
        auto* pAmpAtk = apvts.getParameter("ampAttack");
        auto* pAmpDec = apvts.getParameter("ampDecay");
        auto* pAmpSus = apvts.getParameter("ampSustain");
        auto* pAmpRel = apvts.getParameter("ampRelease");

        const float uA = pAmpAtk ? pAmpAtk->getValue() : 0.2f;
        const float uD = pAmpDec ? pAmpDec->getValue() : 0.35f;
        const float uS = pAmpSus ? pAmpSus->getValue() : 0.8f;
        const float uR = pAmpRel ? pAmpRel->getValue() : 0.35f;

        const float plotX = adsrBox.getX() + 6.0f;
        const float plotW = adsrBox.getWidth() - 12.0f;
        const float plotY = adsrBox.getY() + 14.0f;
        const float plotH = adsrBox.getHeight() - 28.0f;

        const float wA = 0.28f * plotW;
        const float wD = 0.28f * plotW;
        const float wS = 0.16f * plotW;
        const float wR = 0.28f * plotW;

        const juce::Point<float> pStart(plotX, plotY + plotH);
        const juce::Point<float> pPeak(plotX + uA * wA, plotY);
        const juce::Point<float> pSusStart(pPeak.x + uD * wD, plotY + plotH * (1.0f - uS));
        const juce::Point<float> pSusEnd(pSusStart.x + wS, pSusStart.y);
        const juce::Point<float> pEnd(pSusEnd.x + uR * wR, plotY + plotH);

        juce::Path adsrFill;
        adsrFill.startNewSubPath(pStart);
        if (uA <= 0.005f) {
            adsrFill.lineTo(pPeak);
        } else {
            adsrFill.quadraticTo(plotX + (pPeak.x - plotX) * 0.25f, plotY + plotH * 0.15f, pPeak.x, pPeak.y);
        }
        adsrFill.quadraticTo(pPeak.x + (pSusStart.x - pPeak.x) * 0.35f, pSusStart.y + (pPeak.y - pSusStart.y) * 0.30f, pSusStart.x, pSusStart.y);
        adsrFill.lineTo(pSusEnd);
        if (uR <= 0.005f) {
            adsrFill.lineTo(pEnd);
        } else {
            adsrFill.quadraticTo(pSusEnd.x + (pEnd.x - pSusEnd.x) * 0.35f, plotY + plotH * (1.0f - uS * 0.25f), pEnd.x, pEnd.y);
        }
        adsrFill.closeSubPath();

        juce::ColourGradient grad(juce::Colour(0xff38bdf8).withAlpha(0.28f), plotX, plotY,
                                  juce::Colour(0xff38bdf8).withAlpha(0.02f), plotX, plotY + plotH, false);
        g.setGradientFill(grad);
        g.fillPath(adsrFill);

        // Cyan Attack/Decay/Sustain stroke
        juce::Path atkDec;
        atkDec.startNewSubPath(pStart);
        if (uA <= 0.005f) {
            atkDec.lineTo(pPeak);
        } else {
            atkDec.quadraticTo(plotX + (pPeak.x - plotX) * 0.25f, plotY + plotH * 0.15f, pPeak.x, pPeak.y);
        }
        atkDec.quadraticTo(pPeak.x + (pSusStart.x - pPeak.x) * 0.35f, pSusStart.y + (pPeak.y - pSusStart.y) * 0.30f, pSusStart.x, pSusStart.y);
        atkDec.lineTo(pSusEnd);
        g.setColour(juce::Colour(0xff38bdf8));
        g.strokePath(atkDec, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Orange Release stroke
        juce::Path rel;
        rel.startNewSubPath(pSusEnd);
        if (uR <= 0.005f) {
            rel.lineTo(pEnd);
        } else {
            rel.quadraticTo(pSusEnd.x + (pEnd.x - pSusEnd.x) * 0.35f, plotY + plotH * (1.0f - uS * 0.25f), pEnd.x, pEnd.y);
        }
        g.setColour(juce::Colour(0xfff59e0b));
        g.strokePath(rel, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Amber Node Handle Dots
        for (const auto& pt : { pPeak, pSusStart, pSusEnd, pEnd }) {
            g.setColour(juce::Colour(0xfff59e0b));
            g.fillEllipse(pt.x - 2.5f, pt.y - 2.5f, 5.0f, 5.0f);
        }

        // ADSR Values text
        g.setFont(theme_.getValueFont(10.0f));
        g.setColour(theme_.textMuted);
        const juce::String adsrStr = "A:" + juce::String(params.ampEnv.attackSec, 2) + "s  D:" +
                                     juce::String(params.ampEnv.decaySec, 1) + "s  S:" +
                                     juce::String(static_cast<int>(params.ampEnv.sustainLvl * 100.0f)) + "%  R:" +
                                     juce::String(params.ampEnv.releaseSec, 1) + "s";
        g.drawText(adsrStr, static_cast<int>(adsrBox.getX()), static_cast<int>(adsrBox.getBottom() - 15), static_cast<int>(adsrBox.getWidth()), 12, juce::Justification::centred);

        // =====================================================================
        // 4. FX CARD (Horizontal Pedalboard / Series Chain)
        // =====================================================================
        g.setColour(juce::Colour(0xff1a1022));
        g.fillRoundedRectangle(cardFx, 8.0f);
        g.setColour(juce::Colour(0xff581c87).withAlpha(0.7f));
        g.drawRoundedRectangle(cardFx, 8.0f, 1.0f);

        // Header Title: 15.0px Bold
        g.setFont(theme_.getTitleFont(15.0f));
        g.setColour(juce::Colour(0xffc084fc));
        g.drawText("FX", static_cast<int>(cardFx.getX() + 10), static_cast<int>(cardFx.getY() + 7), 80, 18, juce::Justification::centredLeft);

        // Area Global Toggle Switch
        toggleFxRect_ = juce::Rectangle<int>(static_cast<int>(cardFx.getRight() - 36), static_cast<int>(cardFx.getY() + 8), 28, 15);
        const bool anyFxActive = params.delayEnabled || (params.reverbMix > 0.01f);
        drawToggleSwitch(g, toggleFxRect_, anyFxActive, juce::Colour(0xffc084fc));

        // Two side-by-side vertical pedal modules: DELAY (left) and REVERB (right)
        const float slotW = (cardFx.getWidth() - 24.0f) * 0.5f;
        const juce::Rectangle<float> delaySlotR(cardFx.getX() + 8.0f, cardFx.getY() + 30.0f, slotW, H - 38.0f);
        const juce::Rectangle<float> reverbSlotR(delaySlotR.getRight() + 8.0f, cardFx.getY() + 30.0f, slotW, H - 38.0f);
        delaySlotRect_  = delaySlotR.toNearestInt();
        reverbSlotRect_ = reverbSlotR.toNearestInt();

        // 4A. DELAY MODULE (Slot 1)
        g.setColour(juce::Colour(0xff161226));
        g.fillRoundedRectangle(delaySlotR, 6.0f);
        g.setColour(params.delayEnabled ? juce::Colour(0xffc084fc).withAlpha(0.75f) : theme_.borderSubtle);
        g.drawRoundedRectangle(delaySlotR, 6.0f, 1.0f);

        // Delay Header & Individual Bypass
        g.setFont(theme_.getSectionFont(12.5f));
        g.setColour(params.delayEnabled ? juce::Colour(0xffc084fc) : theme_.textMuted);
        g.drawText("DELAY", static_cast<int>(delaySlotR.getX() + 8), static_cast<int>(delaySlotR.getY() + 6), 50, 16, juce::Justification::centredLeft);

        delayToggleRect_ = juce::Rectangle<int>(static_cast<int>(delaySlotR.getRight() - 24), static_cast<int>(delaySlotR.getY() + 8), 18, 11);
        drawToggleSwitch(g, delayToggleRect_, params.delayEnabled, juce::Colour(0xffc084fc));

        // Delay Pins aligned to straight horizontal axis!
        delayInPin_  = { delaySlotR.getX(), commonFxY };
        delayOutPin_ = { delaySlotR.getRight(), commonFxY };
        drawJackPin(g, delayInPin_, juce::Colour(0xff38bdf8), true, false);
        drawJackPin(g, delayOutPin_, juce::Colour(0xffc084fc), params.delayEnabled, false);

        // Delay Parameter Readouts
        g.setFont(theme_.getValueFont(10.5f));
        g.setColour(params.delayEnabled ? theme_.textPrimary : theme_.textMuted);
        g.drawText("Time: " + juce::String(static_cast<int>(params.delayTimeSec * 1000.0f)) + " ms", static_cast<int>(delaySlotR.getX() + 8), static_cast<int>(delaySlotR.getY() + 52), static_cast<int>(slotW - 16), 14, juce::Justification::centredLeft);
        g.setColour(params.delayEnabled ? theme_.textSecondary : theme_.textMuted);
        g.drawText("Fdbk: " + juce::String(static_cast<int>(params.delayFeedback * 100.0f)) + "%", static_cast<int>(delaySlotR.getX() + 8), static_cast<int>(delaySlotR.getY() + 70), static_cast<int>(slotW - 16), 14, juce::Justification::centredLeft);
        g.drawText("Mix: " + juce::String(static_cast<int>(params.delayMix * 100.0f)) + "%", static_cast<int>(delaySlotR.getX() + 8), static_cast<int>(delaySlotR.getY() + 88), static_cast<int>(slotW - 16), 14, juce::Justification::centredLeft);
        g.drawText("Ping-Pong", static_cast<int>(delaySlotR.getX() + 8), static_cast<int>(delaySlotR.getBottom() - 20), static_cast<int>(slotW - 16), 14, juce::Justification::centredLeft);

        // 4B. REVERB MODULE (Slot 2)
        const bool revOn = (params.reverbMix > 0.01f);
        g.setColour(juce::Colour(0xff201026));
        g.fillRoundedRectangle(reverbSlotR, 6.0f);
        g.setColour(revOn ? juce::Colour(0xffe879f9).withAlpha(0.75f) : theme_.borderSubtle);
        g.drawRoundedRectangle(reverbSlotR, 6.0f, 1.0f);

        // Reverb Header & Individual Bypass
        g.setFont(theme_.getSectionFont(12.5f));
        g.setColour(revOn ? juce::Colour(0xffe879f9) : theme_.textMuted);
        g.drawText("REVERB", static_cast<int>(reverbSlotR.getX() + 8), static_cast<int>(reverbSlotR.getY() + 6), 55, 16, juce::Justification::centredLeft);

        reverbToggleRect_ = juce::Rectangle<int>(static_cast<int>(reverbSlotR.getRight() - 24), static_cast<int>(reverbSlotR.getY() + 8), 18, 11);
        drawToggleSwitch(g, reverbToggleRect_, revOn, juce::Colour(0xffe879f9));

        // Reverb Pins aligned to straight horizontal axis!
        reverbInPin_  = { reverbSlotR.getX(), commonFxY };
        reverbOutPin_ = { reverbSlotR.getRight(), commonFxY };
        drawJackPin(g, reverbInPin_, juce::Colour(0xffc084fc), params.delayEnabled, false);
        drawJackPin(g, reverbOutPin_, juce::Colour(0xffe879f9), revOn, false);

        // Reverb Parameter Readouts
        g.setFont(theme_.getValueFont(10.5f));
        g.setColour(revOn ? theme_.textPrimary : theme_.textMuted);
        g.drawText("Decay: " + juce::String(params.reverbDecay, 1) + " s", static_cast<int>(reverbSlotR.getX() + 8), static_cast<int>(reverbSlotR.getY() + 52), static_cast<int>(slotW - 16), 14, juce::Justification::centredLeft);
        g.setColour(revOn ? theme_.textSecondary : theme_.textMuted);
        g.drawText("Shim: " + juce::String(static_cast<int>(params.reverbShimmer * 100.0f)) + "%", static_cast<int>(reverbSlotR.getX() + 8), static_cast<int>(reverbSlotR.getY() + 70), static_cast<int>(slotW - 16), 14, juce::Justification::centredLeft);
        g.drawText("Mix: " + juce::String(static_cast<int>(params.reverbMix * 100.0f)) + "%", static_cast<int>(reverbSlotR.getX() + 8), static_cast<int>(reverbSlotR.getY() + 88), static_cast<int>(slotW - 16), 14, juce::Justification::centredLeft);
        g.drawText("Lush Cloud", static_cast<int>(reverbSlotR.getX() + 8), static_cast<int>(reverbSlotR.getBottom() - 20), static_cast<int>(slotW - 16), 14, juce::Justification::centredLeft);

        // =====================================================================
        // 5. OUTPUT CARD (Slim 126px Slate) - Master Bus, Fader, Meters & LED
        // =====================================================================
        g.setColour(juce::Colour(0xff10151d));
        g.fillRoundedRectangle(cardOutput, 8.0f);
        g.setColour(juce::Colour(0xff334155).withAlpha(0.7f));
        g.drawRoundedRectangle(cardOutput, 8.0f, 1.0f);

        // Header Title: 15.0px Bold
        g.setFont(theme_.getTitleFont(15.0f));
        g.setColour(juce::Colour(0xff94a3b8));
        g.drawText("OUTPUT", static_cast<int>(cardOutput.getX() + 10), static_cast<int>(cardOutput.getY() + 7), 80, 18, juce::Justification::centredLeft);

        // Stereo Summing Terminal ("BUS" aligned to straight horizontal axis!)
        outSummingJack_ = { cardOutput.getX() + 14.0f, commonFxY };
        drawJackPin(g, outSummingJack_, juce::Colour(0xff38bdf8), true, false);
        g.setFont(theme_.getValueFont(10.0f));
        g.setColour(theme_.textMuted);
        g.drawText("BUS", static_cast<int>(outSummingJack_.x - 14.0f), static_cast<int>(outSummingJack_.y + 8.0f), 28, 12, juce::Justification::centred);

        // Vertical Master Volume Fader
        const float faderX = cardOutput.getX() + 50.0f;
        const float faderY = cardOutput.getY() + 32.0f;
        const float faderH = 78.0f;
        masterFaderTrackRect_ = juce::Rectangle<int>(static_cast<int>(faderX - 3.0f), static_cast<int>(faderY), 6, static_cast<int>(faderH));

        // Track slot
        g.setColour(juce::Colour(0xff090d14));
        g.fillRoundedRectangle(masterFaderTrackRect_.toFloat(), 3.0f);
        g.setColour(juce::Colour(0xff334155));
        g.drawRoundedRectangle(masterFaderTrackRect_.toFloat(), 3.0f, 1.0f);

        // Fader Thumb
        const float normGain = std::clamp((params.masterGainDb - (-24.0f)) / 36.0f, 0.0f, 1.0f);
        const float thumbY = faderY + (1.0f - normGain) * faderH;
        masterFaderThumbRect_ = juce::Rectangle<int>(static_cast<int>(faderX - 12.0f), static_cast<int>(thumbY - 6.0f), 24, 12);

        g.setColour(juce::Colour(0xff475569));
        g.fillRoundedRectangle(masterFaderThumbRect_.toFloat(), 3.0f);
        g.setColour(juce::Colour(0xff94a3b8));
        g.drawRoundedRectangle(masterFaderThumbRect_.toFloat(), 3.0f, 1.0f);
        g.setColour(juce::Colour(0xff38bdf8));
        g.fillRect(static_cast<float>(masterFaderThumbRect_.getX() + 3), thumbY - 0.5f, static_cast<float>(masterFaderThumbRect_.getWidth() - 6), 1.5f);

        // Fader Value & Label below
        g.setFont(theme_.getValueFont(11.0f));
        g.setColour(theme_.textPrimary);
        g.drawText(juce::String(params.masterGainDb, 1) + " dB", static_cast<int>(faderX - 24.0f), static_cast<int>(faderY + faderH + 4.0f), 48, 14, juce::Justification::centred);
        g.setFont(theme_.getValueFont(10.5f));
        g.setColour(theme_.textSecondary);
        g.drawText("Master", static_cast<int>(faderX - 20.0f), static_cast<int>(faderY + faderH + 18.0f), 40, 14, juce::Justification::centred);

        // Stereo Peak Meters
        const float meterX = cardOutput.getX() + 90.0f;
        const float meterY = cardOutput.getY() + 32.0f;
        const float meterH = 78.0f;
        const float barW = 5.0f;

        const auto drawMeter = [&](float mx, float val) {
            const juce::Rectangle<float> bg(mx, meterY, barW, meterH);
            g.setColour(juce::Colour(0xff090d14));
            g.fillRoundedRectangle(bg, 2.0f);
            g.setColour(juce::Colour(0xff1e293b));
            g.drawRoundedRectangle(bg, 2.0f, 0.8f);

            const float norm = std::clamp(val, 0.0f, 1.0f);
            const float fillH = norm * meterH;
            const juce::Rectangle<float> fill(mx, meterY + (meterH - fillH), barW, fillH);
            const auto mCol = norm > 0.85f ? juce::Colour(0xfff43f5e) : (norm > 0.65f ? juce::Colour(0xfff59e0b) : juce::Colour(0xff2dd4bf));
            g.setColour(mCol);
            g.fillRoundedRectangle(fill, 2.0f);
        };
        drawMeter(meterX, tel.outputPeakL);
        drawMeter(meterX + 8.0f, tel.outputPeakR);

        g.setFont(theme_.getValueFont(10.5f));
        g.setColour(theme_.textSecondary);
        g.drawText("Stereo", static_cast<int>(meterX - 8.0f), static_cast<int>(meterY + meterH + 4.0f), 34, 14, juce::Justification::centred);

        // Soft Clipper compact LED badge placed underneath!
        const juce::Rectangle<int> clipBadge(static_cast<int>(cardOutput.getCentreX() - 25.0f), static_cast<int>(cardOutput.getBottom() - 24.0f), 50, 16);
        g.setColour(juce::Colour(0xff22c55e).withAlpha(0.16f));
        g.fillRoundedRectangle(clipBadge.toFloat(), 3.0f);
        g.setColour(juce::Colour(0xff22c55e).withAlpha(0.7f));
        g.drawRoundedRectangle(clipBadge.toFloat(), 3.0f, 1.0f);
        // LED dot
        g.setColour(juce::Colour(0xff22c55e));
        g.fillEllipse(static_cast<float>(clipBadge.getX() + 5), static_cast<float>(clipBadge.getCentreY()) - 2.5f, 5.0f, 5.0f);
        g.setFont(theme_.getValueFont(10.0f));
        g.drawText("CLIP", clipBadge.withTrimmedLeft(12), juce::Justification::centred);

        // =====================================================================
        // 6. PATCH CABLES DRAWING
        // =====================================================================
        // Stage 1 -> 2 & 3: Sources to Filters OR Directly to AMP VCA
        for (int v = 0; v < visibleCount; ++v) {
            const int i = v + sourceScrollOffset_;
            if (static_cast<std::size_t>(v) >= oscOutPins_.size()) break;
            const auto srcPt = oscOutPins_[static_cast<std::size_t>(v)];
            const auto col = getLayerColour(i);
            const bool isEnabled = params.oscSlots[static_cast<std::size_t>(i)].enabled;
            const int routing = params.oscSlots[static_cast<std::size_t>(i)].filterRouting;

            if (isDraggingCable_ && dragSourceOscIndex_ == i) {
                // Live dragging rubberband cable to mouse cursor!
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
                    // DUAL: Two glowing patch cables to both FLT 1 and FLT 2!
                    drawPatchCable(g, srcPt, flt1InPin_, col, false);
                    drawPatchCable(g, srcPt, flt2InPin_, col, false);
                } else {
                    // DIRECT: Cable routes directly to AMP VCA Direct In!
                    drawPatchCable(g, srcPt, ampDirectInPin_, col, true, directGutterY_);
                }
            }
        }

        // Stage 2 -> 3: Filters to AMP VCA
        // If a filter is bypassed, gray it out (isDimmed = true) so the user sees signal passing through untouched!
        drawPatchCable(g, flt1OutPin_, ampFltInPin_, juce::Colour(0xff2dd4bf), false, 0.0f, !params.filter1Enabled);
        drawPatchCable(g, flt2OutPin_, ampFltInPin_, juce::Colour(0xff2dd4bf), false, 0.0f, !params.filter2Enabled);

        // Stage 3 -> 4: AMP VCA to DELAY (Straight Horizontal Line!)
        drawPatchCable(g, ampOutPin_, delayInPin_, juce::Colour(0xff38bdf8), false, 0.0f, false);

        // DELAY to REVERB Jumper Cable (Straight Horizontal Jumper! Dimmed if delay bypassed)
        drawPatchCable(g, delayOutPin_, reverbInPin_, juce::Colour(0xffc084fc), false, 0.0f, !params.delayEnabled);

        // REVERB to OUTPUT BUS Cable (Straight Horizontal Cable! Dimmed if reverb bypassed)
        drawPatchCable(g, reverbOutPin_, outSummingJack_, juce::Colour(0xffe879f9), false, 0.0f, !revOn);
    }

    void handleFlowMouseDown(juce::Point<int> pos) {
        auto& apvts = proc_.getAPVTS();

        // 1. Toggle switches in headers (with state preservation)
        if (toggleSourcesRect_.expanded(4).contains(pos)) {
            const auto params = proc_.getCurrentParameters();
            bool anyOn = false;
            for (int i = 0; i < params.activeOscCount; ++i) {
                if (params.oscSlots[static_cast<std::size_t>(i)].enabled) anyOn = true;
            }

            if (anyOn) {
                // User wants to BYPASS Sources: Save each enabled state and disable
                for (int i = 0; i < 8; ++i) {
                    const juce::String prefix = "osc" + juce::String(i + 1) + "_";
                    if (auto* p = apvts.getParameter(prefix + "enabled")) {
                        cachedOscEnabledState_[static_cast<std::size_t>(i)] = (p->getValue() > 0.5f);
                        p->setValueNotifyingHost(0.0f);
                    }
                }
            } else {
                // User wants to UN-BYPASS Sources: Restore only previously active OSCs
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
            repaint();
            return;
        }

        // Individual Filter Bypass Toggles
        if (flt1ToggleRect_.expanded(4).contains(pos)) {
            if (auto* p = apvts.getParameter("filter1Enabled")) {
                p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
            }
            repaint();
            return;
        }

        if (flt2ToggleRect_.expanded(4).contains(pos)) {
            if (auto* p = apvts.getParameter("filter2Enabled")) {
                p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
            }
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

    void handleFlowMouseDrag(const juce::MouseEvent& event) {
        if (isDraggingCable_) {
            dragCurrentPos_ = event.getPosition().toFloat();

            // Hit test destination targets
            // Current routing of the dragged oscillator:
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
                // If already at FLT 2, dragging to FLT 1 offers DUAL!
                hoveredTarget_ = (curRouting == 1) ? 2 : 0;
            } else if (flt2PillRect_.expanded(8).contains(event.getPosition()) || dragCurrentPos_.getDistanceFrom(flt2InPin_) < 28.0f) {
                // If already at FLT 1, dragging to FLT 2 offers DUAL!
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

    void handleFlowMouseUp(const juce::MouseEvent& event) {
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

    [[maybe_unused]] NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int activeTab_ = 0;
    int selectedOscIndex_ = 0;
    int sourceScrollOffset_ = 0;
    float phase_ = 0.0f;
    std::vector<juce::Rectangle<int>> tabRects_;

    // FLOW tab elements & state:
    juce::Rectangle<int> cardSourcesRect_;
    std::vector<juce::Rectangle<int>> flowOscPillRects_;
    std::vector<juce::Rectangle<int>> flowOscRingRects_;
    std::vector<juce::Rectangle<int>> flowOscJackRects_;
    std::vector<juce::Point<float>> oscOutPins_;

    juce::Rectangle<int> scrollUpRect_;
    juce::Rectangle<int> scrollDownRect_;
    juce::Rectangle<int> scrollTrackRect_;
    float wheelAccumulator_ = 0.0f;

    juce::Rectangle<int> toggleSourcesRect_;
    juce::Rectangle<int> toggleFiltersRect_;
    juce::Rectangle<int> toggleFxRect_;

    std::array<bool, 8> cachedOscEnabledState_ = { true, true, true, false, false, false, false, false };
    bool cachedFlt1Enabled_ = true;
    bool cachedFlt2Enabled_ = true;

    juce::Rectangle<int> flt1PillRect_;
    juce::Rectangle<int> flt2PillRect_;
    juce::Rectangle<int> flt1ToggleRect_;
    juce::Rectangle<int> flt2ToggleRect_;
    juce::Rectangle<int> dualTargetRect_;
    juce::Rectangle<int> directGutterRect_;
    juce::Point<float> flt1InPin_;
    juce::Point<float> flt1OutPin_;
    juce::Point<float> flt2InPin_;
    juce::Point<float> flt2OutPin_;
    float directGutterY_ = 0.0f;

    juce::Rectangle<int> cardAmpRect_;
    juce::Point<float> ampFltInPin_;
    juce::Point<float> ampDirectInPin_;
    juce::Point<float> ampOutPin_;

    juce::Rectangle<int> delaySlotRect_;
    juce::Rectangle<int> reverbSlotRect_;
    juce::Rectangle<int> delayToggleRect_;
    juce::Rectangle<int> reverbToggleRect_;
    bool cachedDelayEnabled_ = true;
    float cachedReverbMix_ = 0.42f;
    juce::Point<float> delayInPin_;
    juce::Point<float> delayOutPin_;
    juce::Point<float> reverbInPin_;
    juce::Point<float> reverbOutPin_;

    juce::Point<float> outSummingJack_;
    juce::Rectangle<int> masterFaderTrackRect_;
    juce::Rectangle<int> masterFaderThumbRect_;

    // Interactive Drag'n'Drop State:
    bool isDraggingCable_ = false;
    int dragSourceOscIndex_ = -1;
    juce::Point<float> dragStartPos_;
    juce::Point<float> dragCurrentPos_;
    int hoveredTarget_ = -1;
    bool isDraggingMasterFader_ = false;

    ui::ModulationHubPanel modulationHubPanel_;
    FxRackPanel fxRackPanel_;
    ui::DroneHubPanel droneHubPanel_;
    ui::ArpeggiatorHubPanel arpeggiatorHubPanel_;
    ui::MixerHubPanel mixerHubPanel_;
};

// ==============================================================================
// 5. BOTTOM FOOTER BAR: JORNA Mod Pills + Compact Live Chord Audition Pills
// ==============================================================================
class JornaBottomFooterBar : public juce::Component {
public:
    struct AuditionDef {
        const char* name;
        int count;
        std::array<int, 4> notes;
    };

    static constexpr std::array<AuditionDef, 7> kAuditionPills = {{
        { "C3",     1, { 60, 0,  0,  0  } },
        { "C Maj9", 4, { 48, 60, 64, 74 } },
        { "F Maj7", 4, { 41, 57, 60, 64 } },
        { "Am7",    4, { 45, 57, 60, 67 } },
        { "G6 Sus", 4, { 43, 55, 62, 64 } },
        { "Dm9",    4, { 50, 60, 65, 76 } },
        { "Em7",    4, { 40, 55, 59, 62 } }
    }};

    JornaBottomFooterBar(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc), theme_(theme) {
        for (std::size_t i = 0; i < kAuditionPills.size(); ++i) {
            chordBtns_[i].setButtonText(kAuditionPills[i].name);
            if (i == 0) {
                // Highlight single note audition button with cyan text accent
                chordBtns_[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xff38bdf8));
                chordBtns_[i].setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            }
            chordBtns_[i].onStateChange = [this, i]() {
                const bool isDown = chordBtns_[i].isDown();
                if (isDown && !chordHeld_[i]) {
                    chordHeld_[i] = true;
                    for (int n = 0; n < kAuditionPills[i].count; ++n) {
                        proc_.triggerAuditionNoteOn(kAuditionPills[i].notes[static_cast<std::size_t>(n)], 0.82f);
                    }
                } else if (!isDown && chordHeld_[i]) {
                    chordHeld_[i] = false;
                    for (int n = 0; n < kAuditionPills[i].count; ++n) {
                        proc_.triggerAuditionNoteOff(kAuditionPills[i].notes[static_cast<std::size_t>(n)]);
                    }
                }
            };
            addAndMakeVisible(chordBtns_[i]);
        }
    }

    void paint(juce::Graphics& g) override {
        g.setFont(theme_.getSectionFont(10.5f));

        int x = 6;
        const auto drawBadgeGroup = [&](const juce::String& groupTitle,
                                        juce::Colour titleCol,
                                        const std::vector<const char*>& pills) {
            g.setColour(titleCol);
            g.drawText(groupTitle, x, 0, 32, getHeight(), juce::Justification::centredLeft);
            x += 30;
            for (const char* p : pills) {
                const juce::Rectangle<float> pr(static_cast<float>(x), 2.0f, 20.0f, static_cast<float>(getHeight() - 4));
                g.setColour(theme_.bgElevated);
                g.fillRoundedRectangle(pr, 3.0f);
                g.setColour(theme_.textSecondary);
                g.drawText(p, pr.toNearestInt(), juce::Justification::centred);
                x += 23;
            }
            x += 10;
        };

        drawBadgeGroup("ENV",  juce::Colour(0xff38bdf8), { "E1", "E2", "E3", "E4" });
        drawBadgeGroup("LFO",  juce::Colour(0xfff59e0b), { "L1", "L2", "L3", "L4" });
        drawBadgeGroup("PLAY", juce::Colour(0xffeab308), { "VEL", "KEY", "MW" });
        drawBadgeGroup("MAC",  juce::Colour(0xffa855f7), { "1", "2", "3", "4" });
    }

    void resized() override {
        auto rightArea = getLocalBounds().withTrimmedLeft(getWidth() - 410);
        const int bw = (rightArea.getWidth() - 28) / 7;
        for (auto& b : chordBtns_) {
            b.setBounds(rightArea.removeFromLeft(bw).reduced(0, 1));
            rightArea.removeFromLeft(4);
        }
    }

private:
    [[maybe_unused]] NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    std::array<juce::TextButton, 7> chordBtns_;
    std::array<bool, 7> chordHeld_{};
};

// ==============================================================================
// 6. NEW 8-REGION WORKSPACE PAGE (NOVA SYNTH NEXT)
// ==============================================================================
class NovaSynthWorkspacePage : public juce::Component {
public:
    std::function<void()> onOpenMixerDrawer;

    NovaSynthWorkspacePage(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc),
          sourceList_(proc, theme),
          visualizerHub_(proc, theme),
          sourceDeck_(proc, theme),
          filterDeck_(proc, theme),
          envDeck_(proc, theme),
          workbench_(proc, theme),
          keyboardBar_(proc, theme) {
        addAndMakeVisible(sourceList_);
        addAndMakeVisible(visualizerHub_);
        addAndMakeVisible(sourceDeck_);
        addAndMakeVisible(filterDeck_);
        addAndMakeVisible(envDeck_);
        addAndMakeVisible(workbench_);
        addAndMakeVisible(keyboardBar_);

        workbench_.onOpenMixerDrawer = [this]() {
            if (onOpenMixerDrawer) {
                onOpenMixerDrawer();
            }
        };

        sourceList_.onSourceSelected = [this](int slotIdx) {
            const auto params = proc_.getCurrentParameters();
            if (slotIdx >= 0 && slotIdx < 8 && !params.oscSlots[static_cast<std::size_t>(slotIdx)].enabled) {
                const juce::String prefix = "osc" + juce::String(slotIdx + 1) + "_";
                if (auto* p = proc_.getAPVTS().getParameter(prefix + "enabled")) {
                    p->setValueNotifyingHost(1.0f);
                }
                sourceDeck_.refreshCards();
            }
            sourceDeck_.scrollToSlot(slotIdx);
            workbench_.setSelectedOscIndex(slotIdx);
        };
        sourceList_.onSourceStateChanged = [this]() {
            sourceDeck_.refreshCards();
        };
        sourceDeck_.onSourceStateChanged = [this]() {
            sourceList_.repaint();
        };
    }

    void setInitialWorkbenchTab(int tabIdx) {
        workbench_.selectWorkbenchTab(tabIdx);
    }

    void updateModulationTelemetry(const nova::dsp::WorshipSynthEngine::Telemetry& tel) {
        filterDeck_.updateModulationTelemetry(tel);
    }

    void resized() override {
        auto area = getLocalBounds().reduced(8, 6);

        // Zone 8: Bottom Standalone Keyboard (62px)
        keyboardBar_.setBounds(area.removeFromBottom(62));
        area.removeFromBottom(6);

        // Zone 7: Modular Workbench (220px) - Displays 6 sources in Flow before scrolling
        workbench_.setBounds(area.removeFromBottom(220));
        area.removeFromBottom(6);

        // Zone 5 & 6: Dual Filter Deck & Envelope Quad Deck (168px)
        auto midArea = area.removeFromBottom(168);
        area.removeFromBottom(6);

        const int filterW = static_cast<int>(static_cast<float>(midArea.getWidth()) * 0.44f);
        filterDeck_.setBounds(midArea.removeFromLeft(filterW));
        midArea.removeFromLeft(6);
        envDeck_.setBounds(midArea);

        // Zone 2 (left column 160px) & Zone 3 (visualizer hub) + Zone 4 (source deck)
        sourceList_.setBounds(area.removeFromLeft(160));
        area.removeFromLeft(6);

        auto upperRight = area;
        const int visualizerH = std::clamp(static_cast<int>(static_cast<float>(upperRight.getHeight()) * 0.36f), 108, 122);
        visualizerHub_.setBounds(upperRight.removeFromTop(visualizerH));
        upperRight.removeFromTop(6);
        sourceDeck_.setBounds(upperRight);
    }

private:
    NovaSynthProcessor& proc_;
    SourceListManager sourceList_;
    DualVisualizerHub visualizerHub_;
    SourceDeck sourceDeck_;
    DualFilterDeck filterDeck_;
    EnvelopeQuadDeck envDeck_;
    JornaLowerWorkbench workbench_;
    StandaloneKeyboardBar keyboardBar_;
};

} // namespace

NovaSynthEditor::NovaSynthEditor(NovaSynthProcessor& proc)
    : AudioProcessorEditor(&proc),
      processor_(proc),
      theme_(nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan)),
      lookAndFeel_(theme_),
      shell_(proc, "NOVA SYNTH", "", theme_) {
    setLookAndFeel(&lookAndFeel_);
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);
    addAndMakeVisible(shell_);
    addAndMakeVisible(mixerDrawer_);

    auto appLogo = juce::ImageFileFormat::loadFrom(
        NovaResources::NovaSynth_Logo_png, static_cast<std::size_t>(NovaResources::NovaSynth_Logo_pngSize));
    if (!appLogo.isValid()) {
        appLogo = juce::ImageFileFormat::loadFrom(
            NovaResources::NovaSynth_Icon_256_png, static_cast<std::size_t>(NovaResources::NovaSynth_Icon_256_pngSize));
    }
    if (appLogo.isValid()) {
        shell_.setAppIcon(appLogo);
    }

#if JUCE_MAC
    if (juce::JUCEApplicationBase::isStandaloneApp()) {
        nova::mac::setApplicationDockIcon();
    }
#endif

    shell_.setTextScaleCallback([this](nova::ui::NovaTheme::TextScale scale) {
        theme_.setTextScale(scale);
        lookAndFeel_.setTheme(theme_);
        repaint();
    });

    auto wsPage = std::make_unique<NovaSynthWorkspacePage>(processor_, theme_);
    auto* wsPagePtr = wsPage.get();
    wsPage->onOpenMixerDrawer = [this]() { mixerDrawer_.setDrawerOpen(true); };
    onSetWorkbenchTab_ = [wsPagePtr](int tabIdx) {
        wsPagePtr->setInitialWorkbenchTab(tabIdx);
    };
    onUpdateTelemetry_ = [wsPagePtr](const nova::dsp::WorshipSynthEngine::Telemetry& tel) {
        wsPagePtr->updateModulationTelemetry(tel);
    };

    shell_.addPage("WORKSPACE", std::move(wsPage));

    shell_.setMixerCallback([this]() {
        mixerDrawer_.toggleDrawer();
    });

    shell_.setPatchesCallback([]() {
        // Reserved for future left slide-over patch browser
    });

    constexpr int kMinWidth = 1000;
    constexpr int kMinHeight = 720;
    constexpr int kMaxWidth = 1680;
    constexpr int kMaxHeight = 1100;

    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* primary = displays.getPrimaryDisplay();
    const int screenW = (primary != nullptr) ? primary->userArea.getWidth() : 1440;
    const int screenH = (primary != nullptr) ? primary->userArea.getHeight() : 900;

    const int defaultW = std::clamp(1220, kMinWidth, std::max(kMinWidth, screenW - 30));
    const int defaultH = std::clamp(screenH < 880 ? screenH - 40 : 840, kMinHeight, kMaxHeight);

    setResizable(true, true);
    setResizeLimits(kMinWidth, kMinHeight, kMaxWidth, kMaxHeight);
    setSize(defaultW, defaultH);
    startTimerHz(30);
}

NovaSynthEditor::~NovaSynthEditor() {
    stopTimer();
    onUpdateTelemetry_ = nullptr;
    onSetWorkbenchTab_ = nullptr;
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    if (parentDocWindow_ != nullptr) {
        parentDocWindow_->setLookAndFeel(nullptr);
    }
    setLookAndFeel(nullptr);
}

void NovaSynthEditor::paint(juce::Graphics& g) {
    g.fillAll(theme_.bgCanvas);
}

void NovaSynthEditor::resized() {
    shell_.setBounds(getLocalBounds());
    mixerDrawer_.setBounds(getLocalBounds());
}

void NovaSynthEditor::setInitialWorkbenchTab(int tabIdx) {
    if (onSetWorkbenchTab_) {
        onSetWorkbenchTab_(tabIdx);
    }
}

void NovaSynthEditor::parentHierarchyChanged() {
    configureParentWindow();
}

void NovaSynthEditor::visibilityChanged() {
    if (isVisible()) {
        configureParentWindow();
        startTimerHz(30);
    } else {
        stopTimer();
    }
}

void NovaSynthEditor::configureParentWindow() {
    if (auto* docWin = findParentComponentOfClass<juce::DocumentWindow>()) {
        parentDocWindow_ = docWin;

        constexpr int kMinWidth = 1000;
        constexpr int kMinHeight = 720;
        constexpr int kMaxWidth = 1680;
        constexpr int kMaxHeight = 1100;

        // 1. Enable Native OS Title Bar (Traffic lights on macOS, DWM on Windows, WM decorations on Linux)
        if (!docWin->isUsingNativeTitleBar()) {
            docWin->setUsingNativeTitleBar(true);
            docWin->setTitleBarButtonsRequired(juce::DocumentWindow::allButtons, true);
        }

        // Apply our sleek dark LookAndFeel to parent window popups as well
        docWin->setLookAndFeel(&lookAndFeel_);

        // 2. Strict minimum and maximum size enforcement across Standalone parent window and native peer
        if (auto* c = docWin->getConstrainer()) {
            c->setMinimumSize(kMinWidth, kMinHeight);
            c->setMaximumSize(kMaxWidth, kMaxHeight);
        }
        if (auto* peer = docWin->getPeer()) {
            peer->setConstrainer(docWin->getConstrainer());
        }

        // 3. Screen bounds guard: Ensure window opens with generous, comfortable workstation proportions
        const auto& displays = juce::Desktop::getInstance().getDisplays();
        if (const auto* primary = displays.getPrimaryDisplay()) {
            const auto screenArea = primary->userArea;
            const auto winBounds = docWin->getBounds();

            const int defaultW = std::clamp(1220, kMinWidth, std::max(kMinWidth, screenArea.getWidth() - 30));
            const int defaultH = std::clamp(screenArea.getHeight() < 880 ? screenArea.getHeight() - 40 : 840, kMinHeight, kMaxHeight);

            // If the window bounds are uninitialized or smaller than default, open at full workstation size
            const int baseW = (winBounds.getWidth() >= defaultW) ? winBounds.getWidth() : defaultW;
            const int baseH = (winBounds.getHeight() >= defaultH) ? winBounds.getHeight() : defaultH;

            const int targetW = std::clamp(baseW, kMinWidth, std::max(kMinWidth, screenArea.getWidth() - 20));
            const int targetH = std::clamp(baseH, kMinHeight, std::max(kMinHeight, screenArea.getHeight() - 40));

            int targetX = winBounds.getX();
            int targetY = winBounds.getY();

            if (targetX < screenArea.getX() || targetX + targetW > screenArea.getRight() || targetX == 0) {
                targetX = screenArea.getX() + std::max(0, (screenArea.getWidth() - targetW) / 2);
            }
            if (targetY < screenArea.getY() || targetY + targetH > screenArea.getBottom() || targetY == 0) {
                targetY = screenArea.getY() + std::max(0, (screenArea.getHeight() - targetH) / 2);
            }

            if (targetW != winBounds.getWidth() || targetH != winBounds.getHeight() ||
                targetX != winBounds.getX() || targetY != winBounds.getY()) {
                docWin->setBounds(targetX, targetY, targetW, targetH);
            }
        }

        // 4. Hide standalone JUCE "Options" button and route into PluginShell's settings gear
        for (int i = 0; i < docWin->getNumChildComponents(); ++i) {
            auto* child = docWin->getChildComponent(i);
            if (auto* btn = dynamic_cast<juce::Button*>(child)) {
                if (btn->getButtonText().containsIgnoreCase("Options")) {
                    standaloneOptionsButton_ = btn;
                    btn->setVisible(false);
                    btn->setBounds(0, 0, 0, 0);

                    shell_.setAudioSettingsCallback([this, docWin, btn]() {
                        if (btn != nullptr && docWin != nullptr) {
                            btn->setBounds(0, 0, 1, 1);
                            btn->triggerClick();
                            // Directly invoke Item 1 (Audio/MIDI Settings...) without requiring a secondary click
                            juce::Timer::callAfterDelay(10, []() {
                                if (auto* modal = juce::Component::getCurrentlyModalComponent()) {
                                    modal->keyPressed(juce::KeyPress(juce::KeyPress::downKey));
                                    modal->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
                                }
                            });
                        }
                    });
                    break;
                }
            }
        }
    }
}

void NovaSynthEditor::timerCallback() {
    shell_.setMixerActive(mixerDrawer_.isDrawerOpen());
    if (onUpdateTelemetry_) {
        const auto tel = processor_.getSynthTelemetry();
        onUpdateTelemetry_(tel);
    }
}

} // namespace nova::synth
