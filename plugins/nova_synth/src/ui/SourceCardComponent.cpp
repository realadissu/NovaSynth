#include "SourceCardComponent.h"
#include <algorithm>

namespace nova::synth {

    SourceCardComponent::SourceCardComponent(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme, int slotIndex)
        : proc_(proc),
          theme_(theme),
          slotIndex_(slotIndex),
          octKnob_("OCT", "", theme, false),
          semiKnob_("SEMI", "", theme, false),
          levelKnob_("LEVEL", "", theme, false),
          morphKnob_("MORPH", "", theme, false),
          unisonKnob_("UNISON", "", theme, true),
          panKnob_("PAN", "", theme, true),
          rateKnob_("RATE", "", theme, false),
          scatterKnob_("SCAT", "", theme, false),
          durKnob_("DUR", "", theme, false),
          warpKnob_("WARP", "", theme, false),
          loopStartKnob_("LOOP ST", "%", theme, false),
          loopEndKnob_("LOOP END", "%", theme, false),
          loopXfadeKnob_("XFADE", "%", theme, false),
          compactVolSlider_("VOL", theme),
          spreadSlider_("SPREAD", theme) {

        addAndMakeVisible(octKnob_);
        addAndMakeVisible(semiKnob_);
        addAndMakeVisible(levelKnob_);
        addAndMakeVisible(morphKnob_);
        addAndMakeVisible(unisonKnob_);
        addAndMakeVisible(panKnob_);
        addAndMakeVisible(rateKnob_);
        addAndMakeVisible(scatterKnob_);
        addAndMakeVisible(durKnob_);
        addAndMakeVisible(warpKnob_);
        addChildComponent(loopStartKnob_);
        addChildComponent(loopEndKnob_);
        addChildComponent(loopXfadeKnob_);

        const auto slotCol = getSlotColour(slotIndex_);
        compactVolSlider_.setAccentColour(slotCol);
        spreadSlider_.setAccentColour(slotCol);
        compactVolSlider_.setScrollWheelEnabled(false);
        spreadSlider_.setScrollWheelEnabled(false);

        octKnob_.getSlider().setScrollWheelEnabled(false);
        semiKnob_.getSlider().setScrollWheelEnabled(false);
        levelKnob_.getSlider().setScrollWheelEnabled(false);
        morphKnob_.getSlider().setScrollWheelEnabled(false);
        unisonKnob_.getSlider().setScrollWheelEnabled(false);
        panKnob_.getSlider().setScrollWheelEnabled(false);
        rateKnob_.getSlider().setScrollWheelEnabled(false);
        scatterKnob_.getSlider().setScrollWheelEnabled(false);
        durKnob_.getSlider().setScrollWheelEnabled(false);
        warpKnob_.getSlider().setScrollWheelEnabled(false);
        loopStartKnob_.getSlider().setScrollWheelEnabled(false);
        loopEndKnob_.getSlider().setScrollWheelEnabled(false);
        loopXfadeKnob_.getSlider().setScrollWheelEnabled(false);

        addAndMakeVisible(compactVolSlider_);
        addAndMakeVisible(spreadSlider_);

        bindParameters();
    }

    void SourceCardComponent::bindParameters() {
        const juce::String prefix = "osc" + juce::String(slotIndex_ + 1) + "_";
        auto& apvts = proc_.getAPVTS();

        octKnob_.attachToParameter(apvts, prefix + "octave");
        semiKnob_.attachToParameter(apvts, prefix + "semi");
        levelKnob_.attachToParameter(apvts, prefix + "level");
        panKnob_.attachToParameter(apvts,   prefix + "pan");
        morphKnob_.attachToParameter(apvts, prefix + "shape");
        unisonKnob_.attachToParameter(apvts, prefix + "detune");
        rateKnob_.attachToParameter(apvts,  prefix + "rate");
        scatterKnob_.attachToParameter(apvts, prefix + "scatter");
        durKnob_.attachToParameter(apvts,   prefix + "duration");
        warpKnob_.attachToParameter(apvts,  prefix + "warp");
        loopStartKnob_.attachToParameter(apvts, prefix + "loopStart");
        loopEndKnob_.attachToParameter(apvts,   prefix + "loopEnd");
        loopXfadeKnob_.attachToParameter(apvts, prefix + "loopXfade");

        // Repaint visualizer live when parameters change
        octKnob_.getSlider().onValueChange = [this]() { repaint(); };
        semiKnob_.getSlider().onValueChange = [this]() { repaint(); };
        levelKnob_.getSlider().onValueChange = [this]() { repaint(); };
        panKnob_.getSlider().onValueChange = [this]() { repaint(); };
        morphKnob_.getSlider().onValueChange = [this]() { repaint(); };
        unisonKnob_.getSlider().onValueChange = [this]() { repaint(); };
        scatterKnob_.getSlider().onValueChange = [this]() { repaint(); };
        loopStartKnob_.getSlider().onValueChange = [this]() { repaint(); };
        loopEndKnob_.getSlider().onValueChange = [this]() { repaint(); };
        loopXfadeKnob_.getSlider().onValueChange = [this]() { repaint(); };

        compactVolAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "level", compactVolSlider_);
        spreadAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "spread", spreadSlider_);

        updateDynamicLabels();
    }

    void SourceCardComponent::setExpanded(bool expanded) {
        if (isExpanded_ != expanded) {
            isExpanded_ = expanded;
            updateControlVisibility();
            resized();
            repaint();
        }
    }

    void SourceCardComponent::setPinned(bool pinned) {
        if (isPinned_ != pinned) {
            isPinned_ = pinned;
            repaint();
        }
    }

    void SourceCardComponent::updateDynamicLabels() {
        const auto params = proc_.getCurrentParameters();
        const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex_)];
        using ET = nova::dsp::MultiEngineOscillator::EngineType;

        octKnob_.setLabel("OCT");
        semiKnob_.setLabel("SEMI");
        levelKnob_.setLabel("LEVEL");
        panKnob_.setLabel("PAN");

        if (slot.sourceType == 1 || slot.engine == ET::SubOscillator) {
            morphKnob_.setLabel("DRIVE");
            unisonKnob_.setLabel("PUNCH");
        } else if (slot.sourceType == 2 || slot.engine == ET::PhysicalPluckModal) {
            morphKnob_.setLabel("BRIGHT");
            unisonKnob_.setLabel("DAMP");
            rateKnob_.setLabel("PICK");
            scatterKnob_.setLabel("BODY");
        } else if (slot.sourceType == 3 || slot.engine == ET::NoiseTransient) {
            morphKnob_.setLabel("COLOR");
            unisonKnob_.setLabel("CRACKLE");
            scatterKnob_.setLabel("WIDTH");
        } else if (slot.sourceType == 4 || slot.engine == ET::SamplePlayback) {
            morphKnob_.setLabel("START");
            morphKnob_.setSuffix("%");
            loopStartKnob_.setLabel("LOOP ST");
            loopStartKnob_.setSuffix("%");
            loopEndKnob_.setLabel("LOOP END");
            loopEndKnob_.setSuffix("%");
            loopXfadeKnob_.setLabel("XFADE");
            loopXfadeKnob_.setSuffix("%");
            unisonKnob_.setLabel("SPREAD");
            unisonKnob_.setSuffix("%");
            scatterKnob_.setLabel("DETUNE");
            scatterKnob_.setSuffix("%");
        } else if (slot.engine == ET::EvolvingWavetable) {
            morphKnob_.setLabel("WT POS");
            warpKnob_.setLabel("WARP");
            unisonKnob_.setLabel("UNISON");
            scatterKnob_.setLabel("DETUNE");
        } else if (slot.engine == ET::OrganicTexture) {
            morphKnob_.setLabel("POS");
            unisonKnob_.setLabel("SPRAY");
            scatterKnob_.setLabel("SCAT");
            rateKnob_.setLabel("RATE");
            durKnob_.setLabel("DUR");
            warpKnob_.setLabel("SHAPE");
        } else {
            // Warm Analog
            morphKnob_.setLabel("PWM");
            unisonKnob_.setLabel("UNISON");
            scatterKnob_.setLabel("DETUNE");
        }
    }

    void SourceCardComponent::updateControlVisibility() {
        const bool exp = isExpanded_;
        compactVolSlider_.setVisible(!exp);

        if (!exp) {
            octKnob_.setVisible(false);
            semiKnob_.setVisible(false);
            levelKnob_.setVisible(false);
            morphKnob_.setVisible(false);
            unisonKnob_.setVisible(false);
            panKnob_.setVisible(false);
            rateKnob_.setVisible(false);
            scatterKnob_.setVisible(false);
            durKnob_.setVisible(false);
            warpKnob_.setVisible(false);
            loopStartKnob_.setVisible(false);
            loopEndKnob_.setVisible(false);
            loopXfadeKnob_.setVisible(false);
            spreadSlider_.setVisible(false);
            return;
        }

        const auto params = proc_.getCurrentParameters();
        const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex_)];
        using ET = nova::dsp::MultiEngineOscillator::EngineType;
        const bool isGranular = (slot.engine == ET::OrganicTexture);
        const bool isWT       = (slot.engine == ET::EvolvingWavetable);
        const bool isAnalog   = (slot.engine == ET::WarmAnalog && slot.sourceType == 0);
        const bool isSub      = (slot.sourceType == 1 || slot.engine == ET::SubOscillator);
        const bool isPluck    = (slot.sourceType == 2 || slot.engine == ET::PhysicalPluckModal);
        const bool isNoise    = (slot.sourceType == 3 || slot.engine == ET::NoiseTransient);
        const bool isSample   = (slot.sourceType == 4 || slot.engine == ET::SamplePlayback);

        octKnob_.setVisible(true);
        semiKnob_.setVisible(!isSub && !isNoise);
        levelKnob_.setVisible(true);
        panKnob_.setVisible(true);
        morphKnob_.setVisible(true);
        unisonKnob_.setVisible(true);

        loopStartKnob_.setVisible(isSample);
        loopEndKnob_.setVisible(isSample);
        loopXfadeKnob_.setVisible(isSample);

        rateKnob_.setVisible(isGranular || isPluck);
        scatterKnob_.setVisible(isGranular || isWT || isAnalog || isPluck || isNoise || isSample);
        durKnob_.setVisible(isGranular);
        warpKnob_.setVisible(isGranular || isWT);
        spreadSlider_.setVisible(isGranular);
    }

    void SourceCardComponent::openEngineMenu() {
        const auto params = proc_.getCurrentParameters();
        const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex_)];
        juce::PopupMenu m;
        m.addItem(1, "Analog", true, slot.engine == nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog && slot.sourceType == 0);
        m.addItem(2, "Wavetable", true, slot.engine == nova::dsp::MultiEngineOscillator::EngineType::EvolvingWavetable && slot.sourceType == 0);
        m.addItem(3, "Granular", true, slot.engine == nova::dsp::MultiEngineOscillator::EngineType::OrganicTexture && slot.sourceType == 0);
        m.addItem(4, "Sample", true, slot.sourceType == 4 || slot.engine == nova::dsp::MultiEngineOscillator::EngineType::SamplePlayback);
        m.addItem(5, "Sub", true, slot.sourceType == 1);
        m.addItem(6, "Pluck", true, slot.sourceType == 2);
        m.addItem(7, "Noise", true, slot.sourceType == 3);

        const juce::String prefix = "osc" + juce::String(slotIndex_ + 1) + "_";
        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(engineRect_)).withStandardItemHeight(28),
            [this, prefix](int res) {
                if (res >= 1 && res <= 3) {
                    if (auto* p = proc_.getAPVTS().getParameter(prefix + "sourceType")) p->setValueNotifyingHost(0.0f);
                    if (auto* pe = proc_.getAPVTS().getParameter(prefix + "engine")) {
                        pe->setValueNotifyingHost(pe->convertTo0to1(static_cast<float>(res - 1)));
                    }
                } else if (res == 4) {
                    if (auto* p = proc_.getAPVTS().getParameter(prefix + "sourceType")) p->setValueNotifyingHost(p->convertTo0to1(4.0f));
                    if (auto* pe = proc_.getAPVTS().getParameter(prefix + "engine")) {
                        pe->setValueNotifyingHost(pe->convertTo0to1(3.0f));
                    }
                } else if (res == 5) {
                    if (auto* p = proc_.getAPVTS().getParameter(prefix + "sourceType")) p->setValueNotifyingHost(p->convertTo0to1(1.0f));
                } else if (res == 6) {
                    if (auto* p = proc_.getAPVTS().getParameter(prefix + "sourceType")) p->setValueNotifyingHost(p->convertTo0to1(2.0f));
                } else if (res == 7) {
                    if (auto* p = proc_.getAPVTS().getParameter(prefix + "sourceType")) p->setValueNotifyingHost(p->convertTo0to1(3.0f));
                }

                if (auto* pm = proc_.getAPVTS().getParameter(prefix + "model")) {
                    pm->setValueNotifyingHost(0.0f);
                }

                proc_.syncParametersFromAPVTS();

                updateDynamicLabels();
                updateControlVisibility();
                resized();
                repaint();
                if (onCardStateChanged) onCardStateChanged(slotIndex_);
            });
    }

    void SourceCardComponent::openModelMenu() {
        const auto params = proc_.getCurrentParameters();
        const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex_)];
        const auto models = nova::dsp::MultiEngineOscillator::getModelsForEngine(slot.engine, slot.sourceType);
        if (models.empty()) return;

        juce::PopupMenu m;
        for (int i = 0; i < static_cast<int>(models.size()); ++i) {
            m.addItem(i + 1, models[static_cast<std::size_t>(i)], true, i == slot.modelIndex);
        }

        const juce::String prefix = "osc" + juce::String(slotIndex_ + 1) + "_";
        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(modelRect_)).withStandardItemHeight(28),
            [this, prefix, count = static_cast<int>(models.size())](int res) {
                if (res >= 1 && res <= count) {
                    if (auto* p = proc_.getAPVTS().getParameter(prefix + "model")) {
                        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(res - 1)));
                        proc_.syncParametersFromAPVTS();
                    }
                    repaint();
                }
            });
    }

    juce::Colour SourceCardComponent::getSlotColour(int index) noexcept {
        static constexpr std::array<uint32_t, 8> kColours = {
            0xff38bdf8, // 1: Cyan Sky (Analog A)
            0xff2dd4bf, // 2: Mint Teal (Wavetable B)
            0xfff43f5e, // 3: Bright Rose (Granular C)
            0xff8b5cf6, // 4: Rich Violet (Sub Bass)
            0xffa3e635, // 5: Electric Lime (Pluck / Text)
            0xfffb923c, // 6: Warm Amber (Noise / Air)
            0xff38bdf8, // 7: Cyan
            0xffec4899  // 8: Fuchsia
        };
        return juce::Colour(kColours[static_cast<std::size_t>(index % 8)]);
    }

    juce::String SourceCardComponent::getSlotShortName(int index) {
        static const char* kNames[8] = { "OSC A", "OSC B", "OSC C", "OSC D", "OSC E", "OSC F", "OSC G", "OSC H" };
        return kNames[index % 8];
    }

    juce::String SourceCardComponent::getEngineDisplayName(const nova::dsp::MultiEngineOscillator::Parameters& slot) {
        if (slot.sourceType == 1) return "Sub";
        if (slot.sourceType == 2) return "Pluck";
        if (slot.sourceType == 3) return "Noise";
        if (slot.sourceType == 4 || slot.engine == nova::dsp::MultiEngineOscillator::EngineType::SamplePlayback) return "Sample";
        switch (slot.engine) {
            case nova::dsp::MultiEngineOscillator::EngineType::WarmAnalog: return "Analog";
            case nova::dsp::MultiEngineOscillator::EngineType::EvolvingWavetable: return "Wavetable";
            case nova::dsp::MultiEngineOscillator::EngineType::OrganicTexture: return "Granular";
            case nova::dsp::MultiEngineOscillator::EngineType::PhysicalPluckModal: return "Pluck";
            case nova::dsp::MultiEngineOscillator::EngineType::SubOscillator: return "Sub";
            case nova::dsp::MultiEngineOscillator::EngineType::NoiseTransient: return "Noise";
            case nova::dsp::MultiEngineOscillator::EngineType::SamplePlayback: return "Sample";
        }
        return "Analog";
    }

    juce::String SourceCardComponent::getModelDisplayName(const nova::dsp::WorshipSynthEngine::Parameters& params, int slotIdx) {
        if (slotIdx < 0 || slotIdx >= 8) return "Classic Saw";
        const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIdx)];
        return nova::dsp::MultiEngineOscillator::getModelName(slot.engine, slot.sourceType, slot.modelIndex);
    }

    juce::String SourceCardComponent::getFilterRoutingDisplayName(int filterRouting) {
        switch (filterRouting) {
            case 0: return "FLT 1";
            case 1: return "FLT 2";
            case 2: return "FLT 1+2";
            case 3: return "DIRECT";
            default: return "FLT 1";
        }
    }

    juce::String SourceCardComponent::getAmpEnvDisplayName(int ampEnvSource) {
        switch (ampEnvSource) {
            case 1:  return "ENV: FLT";
            case 2:  return "ENV: MOD 3";
            case 3:  return "ENV: AUX 4";
            default: return "ENV: AMP 1";
        }
    }

} // namespace nova::synth
