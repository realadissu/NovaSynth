#pragma once

#include "nova_ui/NovaTheme.h"
#include "nova_ui/ArcKnob.h"
#include "HorizontalViewport.h"
#include "../NovaSynthProcessor.h"
#include "BinaryData.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>
#include <numbers>
#include <unordered_map>

namespace nova::synth {

// ==============================================================================
// SourceCardComponent: Accordion-fähige Oszillatorkarte
// - Kompakt-Modus: Exakt QUADRATISCH (Breite = Höhe ~144px), große Wellenform, Volume-Slider, Solo/Mute
// - Expandiert-Modus (~390px): Kategorie-spezifische Profi-Bedienelemente
//   * Granular: 10 Knobs (2x5), Sample-Selector, KeyTrk & horizontaler SPREAD-Bar
//   * Wavetable: 3D Isometric Terrain mit WT-Scrubber, 8 Knobs (WT POS, WARP, UNISON, DETUNE...)
//   * Warm Analog: Waveform-Pills [SINE, TRI, SAW, PULSE], 7 Knobs (PWM, UNISON, DETUNE...)
//   * Sub Bass: Waveform-Pills [SINE, TRI, 808, SQR], Lowest Note Only Toggle, Octave Steppers & Knobs
//   * Pluck: Model-Selector, 8 Knobs (BRIGHT, DAMPING, PICK POS, BODY...)
//   * Noise: Type-Pills [WHITE, PINK, VINYL, AIR], Knobs (COLOR, CRACKLE, WIDTH...)
// ==============================================================================
// ==============================================================================
// SleekHorizontalSlider: Eleganter, moderner Horizontal-Slider im Nova Theme
// ==============================================================================
class SleekHorizontalSlider : public juce::Slider {
public:
    SleekHorizontalSlider(const juce::String& label, const nova::ui::NovaTheme& theme)
        : label_(label), theme_(theme) {
        setSliderStyle(juce::Slider::LinearHorizontal);
        setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        setScrollWheelEnabled(false);
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        if (auto* vp = findParentComponentOfClass<juce::Viewport>()) {
            vp->mouseWheelMove(e, wheel);
        }
    }

    void setAccentColour(juce::Colour c) noexcept {
        accentCol_ = c;
        repaint();
    }

    void setCustomLabel(const juce::String& l) noexcept {
        label_ = l;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        const auto trackY = bounds.getCentreY();
        const float trackH = 4.0f;

        // Label auf der linken Seite
        float leftPad = 0.0f;
        if (label_.isNotEmpty()) {
            leftPad = 48.0f;
            auto lblRect = juce::Rectangle<float>(0.0f, 0.0f, leftPad - 5.0f, bounds.getHeight());
            g.setFont(theme_.getLabelFont(8.5f));
            g.setColour(theme_.textMuted);
            g.drawText(label_, lblRect, juce::Justification::centredRight, false);
        }

        // Value auf der rechten Seite
        const float rightPad = 38.0f;
        auto valRect = juce::Rectangle<float>(bounds.getRight() - rightPad + 4.0f, 0.0f, rightPad - 4.0f, bounds.getHeight());
        g.setFont(theme_.getValueFont(8.5f));
        g.setColour(juce::Colours::white.withAlpha(0.9f));

        juce::String valStr;
        const double val = getValue();
        if (getMaximum() <= 1.05 && getMinimum() >= 0.0) {
            valStr = juce::String(static_cast<int>(std::round(val * 100.0))) + "%";
        } else {
            valStr = juce::String(val, 2);
        }
        g.drawText(valStr, valRect, juce::Justification::centredLeft, false);

        // Track-Bereich
        const float trackX = leftPad;
        const float trackW = std::max(10.0f, bounds.getRight() - rightPad - trackX);
        const auto trackRect = juce::Rectangle<float>(trackX, trackY - trackH * 0.5f, trackW, trackH);

        // Track Background Trough
        g.setColour(theme_.bgControl.brighter(0.06f));
        g.fillRoundedRectangle(trackRect, trackH * 0.5f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(trackRect, trackH * 0.5f, 0.7f);

        // Filled Active Track
        const double normVal = (getValue() - getMinimum()) / std::max(0.0001, getMaximum() - getMinimum());
        const float fillW = static_cast<float>(std::clamp(normVal, 0.0, 1.0)) * trackW;
        const auto col = accentCol_.isTransparent() ? theme_.accentPrimary : accentCol_;
        if (fillW > 1.0f) {
            const auto fillRect = juce::Rectangle<float>(trackX, trackY - trackH * 0.5f, fillW, trackH);
            g.setColour(col.withAlpha(0.85f));
            g.fillRoundedRectangle(fillRect, trackH * 0.5f);
        }

        // Thumb Capsule Handle
        const float thumbX = trackX + fillW;
        const float thumbW = 7.0f;
        const float thumbH = 12.0f;
        const auto thumbRect = juce::Rectangle<float>(thumbX - thumbW * 0.5f, trackY - thumbH * 0.5f, thumbW, thumbH);

        if (isMouseOverOrDragging()) {
            g.setColour(col.withAlpha(0.35f));
            g.fillRoundedRectangle(thumbRect.expanded(1.5f), 3.5f);
        }
        g.setColour(theme_.bgElevated.brighter(0.2f));
        g.fillRoundedRectangle(thumbRect, 2.5f);
        g.setColour(col);
        g.drawRoundedRectangle(thumbRect, 2.5f, 1.0f);
    }

private:
    juce::String label_;
    const nova::ui::NovaTheme& theme_;
    juce::Colour accentCol_{};
};

class SourceCardComponent : public juce::Component, public juce::TooltipClient {
public:
    std::function<void(int)> onCardStateChanged;
    std::function<void(int, bool)> onExpandRequested;
    std::function<void(int, bool)> onPinToggled;

    SourceCardComponent(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme, int slotIndex)
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

        addAndMakeVisible(compactVolSlider_);
        addAndMakeVisible(spreadSlider_);

        bindParameters();
    }

    void bindParameters() {
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

        compactVolAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "level", compactVolSlider_);
        spreadAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "spread", spreadSlider_);

        updateDynamicLabels();
    }

    [[nodiscard]] int getSlotIndex() const noexcept { return slotIndex_; }
    [[nodiscard]] bool isExpanded() const noexcept { return isExpanded_; }
    [[nodiscard]] bool isPinned() const noexcept { return isPinned_; }
    [[nodiscard]] float getPreferredExpandedWidth() const noexcept {
        const auto params = proc_.getCurrentParameters();
        const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex_)];
        if (slot.engine == nova::dsp::MultiEngineOscillator::EngineType::OrganicTexture && slot.sourceType == 0) {
            return 520.0f;
        }
        return 390.0f;
    }

    void setExpanded(bool expanded) {
        if (isExpanded_ != expanded) {
            isExpanded_ = expanded;
            updateControlVisibility();
            resized();
            repaint();
        }
    }

    void setPinned(bool pinned) {
        if (isPinned_ != pinned) {
            isPinned_ = pinned;
            repaint();
        }
    }

    void updateDynamicLabels() {
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
            morphKnob_.setLabel("ATTACK");
            unisonKnob_.setLabel("SPREAD");
            scatterKnob_.setLabel("DETUNE");
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

    void updateControlVisibility() {
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

        rateKnob_.setVisible(isGranular || isPluck);
        scatterKnob_.setVisible(isGranular || isWT || isAnalog || isPluck || isNoise || isSample);
        durKnob_.setVisible(isGranular);
        warpKnob_.setVisible(isGranular || isWT);
        spreadSlider_.setVisible(isGranular);
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        const auto slotCol = getSlotColour(slotIndex_);

        g.setColour(theme_.bgSurface);
        g.fillRoundedRectangle(bounds, theme_.cornerRadius);

        if (isExpanded_) {
            g.setColour(slotCol.withAlpha(0.65f));
            g.drawRoundedRectangle(bounds, theme_.cornerRadius, 1.2f);
        } else {
            g.setColour(theme_.borderSubtle);
            g.drawRoundedRectangle(bounds, theme_.cornerRadius, 0.8f);
        }

        const auto params = proc_.getCurrentParameters();
        const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex_)];

        if (!isExpanded_) {
            paintCompact(g, slotCol, slot);
        } else {
            paintExpanded(g, slotCol, slot, params);
        }
    }

    void resized() override {
        updateControlVisibility();

        if (!isExpanded_) {
            // Quadratischer Kompakt-Modus
            auto header = getLocalBounds().removeFromTop(24).reduced(5, 2);
            powerRect_ = header.removeFromRight(16);
            soloRect_  = header.removeFromRight(16);
            pinRect_   = {};
            visualizerRect_ = juce::Rectangle<int>(6, 26, getWidth() - 12, getHeight() - 56);
            compactVolSlider_.setBounds(8, getHeight() - 25, getWidth() - 16, 18);
            routingRect_ = {};
            ampEnvRect_ = {};
            modelRect_ = {};
            modelPrevRect_ = {};
            modelNextRect_ = {};
            keyTrkRect_ = {};
            lowestNoteRect_ = {};
            for (auto& r : wavePillRects_) r = {};
            for (auto& r : subOctPillRects_) r = {};
        } else {
            // Vollwertiger Expandierter Zustand (~390px breit)
            auto header = getLocalBounds().removeFromTop(24).reduced(6, 2);
            pinRect_   = header.removeFromRight(18);
            header.removeFromRight(4);
            powerRect_ = header.removeFromRight(16);
            header.removeFromRight(3);
            soloRect_  = header.removeFromRight(16);

            // Linke Hälfte: Visualizer & Routing & Model
            const int leftW = 135;
            visualizerRect_ = juce::Rectangle<int>(6, 26, leftW - 6, getHeight() - 74);
            const int pillW = (leftW - 6 - 4) / 2;
            routingRect_    = juce::Rectangle<int>(6, getHeight() - 46, pillW, 18);
            ampEnvRect_     = juce::Rectangle<int>(6 + pillW + 4, getHeight() - 46, pillW, 18);
            modelRect_      = juce::Rectangle<int>(6, getHeight() - 25, leftW - 6, 20);
            modelPrevRect_  = juce::Rectangle<int>(modelRect_.getX(), modelRect_.getY(), 18, modelRect_.getHeight());
            modelNextRect_  = juce::Rectangle<int>(modelRect_.getRight() - 18, modelRect_.getY(), 18, modelRect_.getHeight());

            // Rechte Hälfte: Kategorie-spezifische Anordnung
            const int rightX = leftW + 8;
            const int rightW = getWidth() - rightX - 6;

            const auto params = proc_.getCurrentParameters();
            const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex_)];
            using ET = nova::dsp::MultiEngineOscillator::EngineType;
            const bool isGranular = (slot.engine == ET::OrganicTexture);
            const bool isWT       = (slot.engine == ET::EvolvingWavetable);
            const bool isAnalog   = (slot.engine == ET::WarmAnalog && slot.sourceType == 0);
            const bool isSub      = (slot.sourceType == 1 || slot.engine == ET::SubOscillator);
            const bool isPluck    = (slot.sourceType == 2 || slot.engine == ET::PhysicalPluckModal);
            const bool isNoise    = (slot.sourceType == 3 || slot.engine == ET::NoiseTransient);
            juce::ignoreUnused(isNoise);

            for (auto& r : wavePillRects_) r = {};
            for (auto& r : subOctPillRects_) r = {};
            keyTrkRect_ = {};
            lowestNoteRect_ = {};

            if (isGranular) {
                // Granular: KeyTrk rechts im Header neben Solo
                keyTrkRect_ = header.removeFromRight(50).reduced(0, 1);

                // Knobs Grid: 2 Reihen à 5 Knobs – voll dimensioniert über die verfügbare Höhe
                const int kw = (rightW - 16) / 5;
                const int bottomMargin = 26;
                const int availH = getHeight() - 26 - bottomMargin;
                const int kh = std::max(48, (availH - 6) / 2);
                const int yRow1 = 26;
                const int yRow2 = 26 + kh + 4;

                levelKnob_.setBounds(rightX, yRow1, kw, kh);
                morphKnob_.setBounds(rightX + (kw + 4), yRow1, kw, kh);
                unisonKnob_.setBounds(rightX + (kw + 4) * 2, yRow1, kw, kh);
                scatterKnob_.setBounds(rightX + (kw + 4) * 3, yRow1, kw, kh);
                rateKnob_.setBounds(rightX + (kw + 4) * 4, yRow1, kw, kh);

                octKnob_.setBounds(rightX, yRow2, kw, kh);
                semiKnob_.setBounds(rightX + (kw + 4), yRow2, kw, kh);
                durKnob_.setBounds(rightX + (kw + 4) * 2, yRow2, kw, kh);
                warpKnob_.setBounds(rightX + (kw + 4) * 3, yRow2, kw, kh);
                panKnob_.setBounds(rightX + (kw + 4) * 4, yRow2, kw, kh);

                // Bottom Sleek Spread Slider
                spreadSlider_.setBounds(rightX, getHeight() - 23, rightW, 18);
            } else if (isWT) {
                const int kw = (rightW - 12) / 4;
                const int kh = (getHeight() - 56) / 2;
                const int yRow1 = 26;
                const int yRow2 = 26 + kh + 4;

                octKnob_.setBounds(rightX, yRow1, kw, kh);
                semiKnob_.setBounds(rightX + kw + 4, yRow1, kw, kh);
                levelKnob_.setBounds(rightX + (kw + 4) * 2, yRow1, kw, kh);
                panKnob_.setBounds(rightX + (kw + 4) * 3, yRow1, kw, kh);

                morphKnob_.setBounds(rightX, yRow2, kw, kh);
                warpKnob_.setBounds(rightX + kw + 4, yRow2, kw, kh);
                unisonKnob_.setBounds(rightX + (kw + 4) * 2, yRow2, kw, kh);
                scatterKnob_.setBounds(rightX + (kw + 4) * 3, yRow2, kw, kh);
            } else if (isAnalog) {
                const int pillCount = 6;
                const int gap = 3;
                const int pillW = (rightW - (pillCount - 1) * gap) / pillCount;
                for (int i = 0; i < pillCount; ++i) {
                    wavePillRects_[static_cast<std::size_t>(i)] = juce::Rectangle<int>(rightX + i * (pillW + gap), 26, pillW, 20);
                }

                const int kw4 = (rightW - 12) / 4;
                const int kh = (getHeight() - 56) / 2;
                octKnob_.setBounds(rightX, 50, kw4, kh);
                semiKnob_.setBounds(rightX + kw4 + 4, 50, kw4, kh);
                levelKnob_.setBounds(rightX + (kw4 + 4) * 2, 50, kw4, kh);
                panKnob_.setBounds(rightX + (kw4 + 4) * 3, 50, kw4, kh);

                const int kw3 = (rightW - 8) / 3;
                morphKnob_.setBounds(rightX, 50 + kh + 2, kw3, kh);
                unisonKnob_.setBounds(rightX + kw3 + 4, 50 + kh + 2, kw3, kh);
                scatterKnob_.setBounds(rightX + (kw3 + 4) * 2, 50 + kh + 2, kw3, kh);
            } else if (isSub) {
                const int pillCount = 4;
                const int gap = 3;
                const int pillW = (rightW - (pillCount - 1) * gap) / pillCount;
                for (int i = 0; i < pillCount; ++i) {
                    wavePillRects_[static_cast<std::size_t>(i)] = juce::Rectangle<int>(rightX + i * (pillW + gap), 26, pillW, 20);
                }
                wavePillRects_[4] = {};
                wavePillRects_[5] = {};

                lowestNoteRect_ = juce::Rectangle<int>(rightX, 50, 130, 20);
                const int octW = 28;
                for (int i = 0; i < 3; ++i) {
                    subOctPillRects_[static_cast<std::size_t>(i)] = juce::Rectangle<int>(rightX + rightW - (3 - i) * (octW + 3), 50, octW, 20);
                }

                const int kw5 = (rightW - 16) / 5;
                const int kh = getHeight() - 76;
                octKnob_.setBounds(rightX, 74, kw5, kh);
                levelKnob_.setBounds(rightX + (kw5 + 4), 74, kw5, kh);
                morphKnob_.setBounds(rightX + (kw5 + 4) * 2, 74, kw5, kh);
                unisonKnob_.setBounds(rightX + (kw5 + 4) * 3, 74, kw5, kh);
                panKnob_.setBounds(rightX + (kw5 + 4) * 4, 74, kw5, kh);
            } else if (isPluck) {
                const int kw = (rightW - 12) / 4;
                const int kh = (getHeight() - 56) / 2;
                octKnob_.setBounds(rightX, 26, kw, kh);
                semiKnob_.setBounds(rightX + kw + 4, 26, kw, kh);
                levelKnob_.setBounds(rightX + (kw + 4) * 2, 26, kw, kh);
                panKnob_.setBounds(rightX + (kw + 4) * 3, 26, kw, kh);

                morphKnob_.setBounds(rightX, 26 + kh + 4, kw, kh);
                unisonKnob_.setBounds(rightX + kw + 4, 26 + kh + 4, kw, kh);
                rateKnob_.setBounds(rightX + (kw + 4) * 2, 26 + kh + 4, kw, kh);
                scatterKnob_.setBounds(rightX + (kw + 4) * 3, 26 + kh + 4, kw, kh);
            } else {
                const int pillCount = 4;
                const int gap = 3;
                const int pillW = (rightW - (pillCount - 1) * gap) / pillCount;
                for (int i = 0; i < pillCount; ++i) {
                    wavePillRects_[static_cast<std::size_t>(i)] = juce::Rectangle<int>(rightX + i * (pillW + gap), 26, pillW, 20);
                }
                wavePillRects_[4] = {};
                wavePillRects_[5] = {};

                const int kw = (rightW - 8) / 3;
                const int kh = (getHeight() - 56) / 2;
                octKnob_.setBounds(rightX, 50, kw, kh);
                levelKnob_.setBounds(rightX + kw + 4, 50, kw, kh);
                panKnob_.setBounds(rightX + (kw + 4) * 2, 50, kw, kh);

                morphKnob_.setBounds(rightX, 50 + kh + 2, kw, kh);
                unisonKnob_.setBounds(rightX + kw + 4, 50 + kh + 2, kw, kh);
                scatterKnob_.setBounds(rightX + (kw + 4) * 2, 50 + kh + 2, kw, kh);
            }
        }
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        if (auto* vp = findParentComponentOfClass<juce::Viewport>()) {
            vp->mouseWheelMove(e, wheel);
        }
    }

    void mouseExit(const juce::MouseEvent&) override {
        if (hoveredWavePill_ != -1) {
            hoveredWavePill_ = -1;
            repaint();
        }
    }

    void mouseMove(const juce::MouseEvent& e) override {
        const auto pos = e.getPosition();

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

    void mouseDown(const juce::MouseEvent& e) override {
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

        // 11. Waveform Icon Pills (Analog, Sub, Noise)
        for (int i = 0; i < 6; ++i) {
            if (wavePillRects_[static_cast<std::size_t>(i)].contains(pos)) {
                const juce::String prefix = "osc" + juce::String(slotIndex_ + 1) + "_";
                if (auto* p = proc_.getAPVTS().getParameter(prefix + "model")) {
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(i)));
                    proc_.syncParametersFromAPVTS();
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

    void cycleModel(int delta) {
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

    float currentWidth_ = 144.0f;

private:
    void paintCompact(juce::Graphics& g, juce::Colour slotCol,
                      const nova::dsp::MultiEngineOscillator::Parameters& slot) {
        auto header = getLocalBounds().removeFromTop(24).reduced(5, 2);

        // Slot Badge
        auto badge = header.removeFromLeft(48);
        g.setColour(slotCol.withAlpha(0.22f));
        g.fillRoundedRectangle(badge.toFloat(), 3.0f);
        g.setColour(slotCol);
        g.drawRoundedRectangle(badge.toFloat(), 3.0f, 1.0f);
        g.setFont(theme_.getSectionFont(10.0f));
        g.setColour(juce::Colours::white);
        g.drawFittedText(getSlotShortName(slotIndex_), badge, juce::Justification::centred, 1, 0.85f);

        // Power Toggle
        powerRect_ = header.removeFromRight(16);
        const auto powCircle = powerRect_.withSizeKeepingCentre(10, 10).toFloat();
        g.setColour(slot.enabled ? slotCol : theme_.textMuted.withAlpha(0.4f));
        if (slot.enabled) g.fillEllipse(powCircle);
        else g.drawEllipse(powCircle, 1.3f);

        // Solo Button [S]
        soloRect_ = header.removeFromRight(16);
        const bool isSolo = proc_.isSlotSoloed(slotIndex_);
        g.setColour(isSolo ? juce::Colour(0xffeab308) : theme_.bgElevated);
        g.fillRoundedRectangle(soloRect_.toFloat(), 3.0f);
        g.setColour(isSolo ? juce::Colour(0xfffacc15) : theme_.borderSubtle);
        g.drawRoundedRectangle(soloRect_.toFloat(), 3.0f, 0.8f);
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(isSolo ? juce::Colour(0xff1e1b4b) : theme_.textMuted);
        g.drawText("S", soloRect_, juce::Justification::centred, false);

        // Visualizer Screen
        drawEngineVisualizer(g, visualizerRect_, slot, slotCol, false);
    }

    void paintExpanded(juce::Graphics& g, juce::Colour slotCol,
                       const nova::dsp::MultiEngineOscillator::Parameters& slot,
                       const nova::dsp::WorshipSynthEngine::Parameters& params) {
        auto header = getLocalBounds().removeFromTop(24).reduced(6, 2);

        // Trennlinie zwischen linker und rechter Hälfte
        const float divX = 143.0f;
        g.setColour(theme_.borderSubtle.withAlpha(0.6f));
        g.drawVerticalLine(static_cast<int>(divX), 6.0f, static_cast<float>(getHeight() - 6));

        // 1. PIN BUTTON: Ganz oben rechts in der Ecke!
        pinRect_ = header.removeFromRight(18);
        if (pinRect_.contains(getMouseXYRelative())) {
            g.setColour(theme_.bgElevated.withAlpha(0.6f));
            g.fillRoundedRectangle(pinRect_.toFloat(), 3.0f);
        }
        drawPinIcon(g, pinRect_.toFloat().reduced(1.0f), slotCol, isPinned_);
        header.removeFromRight(4);

        // 2. POWER SWITCH: Links neben Pin
        powerRect_ = header.removeFromRight(16);
        const auto powCircle = powerRect_.withSizeKeepingCentre(10, 10).toFloat();
        g.setColour(slot.enabled ? slotCol : theme_.textMuted.withAlpha(0.4f));
        if (slot.enabled) g.fillEllipse(powCircle);
        else g.drawEllipse(powCircle, 1.3f);

        // 3. SOLO BUTTON: Links neben Power
        header.removeFromRight(3);
        soloRect_ = header.removeFromRight(16);
        const bool isSolo = proc_.isSlotSoloed(slotIndex_);
        g.setColour(isSolo ? juce::Colour(0xffeab308) : theme_.bgElevated);
        g.fillRoundedRectangle(soloRect_.toFloat(), 3.0f);
        g.setColour(isSolo ? juce::Colour(0xfffacc15) : theme_.borderSubtle);
        g.drawRoundedRectangle(soloRect_.toFloat(), 3.0f, 0.8f);
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(isSolo ? juce::Colour(0xff1e1b4b) : theme_.textMuted);
        g.drawText("S", soloRect_, juce::Justification::centred, false);

        // Slot Badge
        auto badge = header.removeFromLeft(46);
        g.setColour(slotCol.withAlpha(0.22f));
        g.fillRoundedRectangle(badge.toFloat(), 3.0f);
        g.setColour(slotCol);
        g.drawRoundedRectangle(badge.toFloat(), 3.0f, 1.0f);
        g.setFont(theme_.getSectionFont(10.0f));
        g.setColour(juce::Colours::white);
        g.drawFittedText(getSlotShortName(slotIndex_), badge, juce::Justification::centred, 1, 0.85f);

        // Engine Dropdown Pill
        header.removeFromLeft(4);
        header.setWidth(std::min(header.getWidth(), 80));
        engineRect_ = header;
        g.setColour(theme_.bgElevated);
        g.fillRoundedRectangle(engineRect_.toFloat(), 3.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(engineRect_.toFloat(), 3.0f, 0.8f);
        g.setFont(theme_.getValueFont(9.0f));
        g.setColour(slotCol);
        g.drawFittedText(getEngineDisplayName(slot) + " \xe2\x96\xbe", engineRect_, juce::Justification::centred, 1, 0.85f);

        // Visualizer Screen (Waveform / 3D Wavetable)
        drawEngineVisualizer(g, visualizerRect_, slot, slotCol, true);

        // Filter Routing Pill (links unten über Model)
        g.setColour(theme_.bgElevated);
        g.fillRoundedRectangle(routingRect_.toFloat(), 3.0f);
        g.setColour(theme_.borderFocus);
        g.drawRoundedRectangle(routingRect_.toFloat(), 3.0f, 0.8f);
        g.setFont(theme_.getValueFont(9.0f));
        g.setColour(theme_.accentPrimary);
        g.drawFittedText(getFilterRoutingDisplayName(slot.filterRouting) + " \xe2\x96\xbe",
                         routingRect_, juce::Justification::centred, 1, 0.85f);

        // Amp Env Pill (rechts neben Routing)
        g.setColour(theme_.bgElevated);
        g.fillRoundedRectangle(ampEnvRect_.toFloat(), 3.0f);
        const bool isCustomEnv = (slot.ampEnvSource != 0);
        g.setColour(isCustomEnv ? theme_.accentWarm : theme_.borderSubtle);
        g.drawRoundedRectangle(ampEnvRect_.toFloat(), 3.0f, 0.8f);
        g.setFont(theme_.getValueFont(9.0f));
        g.setColour(isCustomEnv ? theme_.accentWarm : theme_.textSecondary);
        g.drawFittedText(getAmpEnvDisplayName(slot.ampEnvSource) + " \xe2\x96\xbe",
                         ampEnvRect_, juce::Justification::centred, 1, 0.85f);

        // Model Selector Box mit < und > Steppern
        g.setColour(theme_.bgElevated.brighter(0.08f));
        g.fillRoundedRectangle(modelRect_.toFloat(), 3.0f);
        g.setColour(slotCol.withAlpha(0.65f));
        g.drawRoundedRectangle(modelRect_.toFloat(), 3.0f, 1.0f);

        // < Stepper
        g.setFont(theme_.getValueFont(10.0f));
        g.setColour(theme_.textMuted);
        g.drawText("<", modelPrevRect_, juce::Justification::centred, false);

        // > Stepper
        g.drawText(">", modelNextRect_, juce::Justification::centred, false);

        // Model Name Text
        auto modelContent = modelRect_.toFloat().reduced(18.0f, 2.0f);
        g.setFont(theme_.getSectionFont(9.0f));
        g.setColour(juce::Colours::white);
        g.drawFittedText(getModelDisplayName(params, slotIndex_),
                         modelContent.toNearestInt(), juce::Justification::centred, 1, 0.85f);

        // Rechte Seite: Kategorie-spezifische Zeichnungen
        using ET = nova::dsp::MultiEngineOscillator::EngineType;
        const bool isGranular = (slot.engine == ET::OrganicTexture);
        const bool isAnalog   = (slot.engine == ET::WarmAnalog && slot.sourceType == 0);
        const bool isSub      = (slot.sourceType == 1 || slot.engine == ET::SubOscillator);
        const bool isNoise    = (slot.sourceType == 3 || slot.engine == ET::NoiseTransient);

        if (isGranular) {
            // KeyTrk Button
            const bool keyTrkActive = slot.lowestNoteOnly;
            g.setColour(keyTrkActive ? slotCol.withAlpha(0.35f) : theme_.bgElevated);
            g.fillRoundedRectangle(keyTrkRect_.toFloat(), 3.0f);
            g.setColour(keyTrkActive ? slotCol : theme_.borderSubtle);
            g.drawRoundedRectangle(keyTrkRect_.toFloat(), 3.0f, 0.8f);
            g.setFont(theme_.getValueFont(9.0f));
            g.setColour(keyTrkActive ? juce::Colours::white : theme_.textMuted);
            g.drawText("KeyTrk", keyTrkRect_, juce::Justification::centred, false);
        } else if (isAnalog) {
            for (int i = 0; i < 6; ++i) {
                const auto r = wavePillRects_[static_cast<std::size_t>(i)];
                const bool isActive = (slot.modelIndex == i);
                const bool isHov = (hoveredWavePill_ == i);

                g.setColour(isActive ? slotCol.withAlpha(0.24f) : (isHov ? theme_.bgControl.brighter(0.12f) : theme_.bgElevated));
                g.fillRoundedRectangle(r.toFloat(), 3.0f);
                g.setColour(isActive ? slotCol : (isHov ? theme_.borderFocus : theme_.borderSubtle));
                g.drawRoundedRectangle(r.toFloat(), 3.0f, isActive ? 1.2f : 0.8f);

                const auto iconCol = isActive ? slotCol.brighter(0.35f) : (isHov ? juce::Colours::white : theme_.textMuted);
                drawWaveformIcon(g, r.toFloat(), slot.engine, slot.sourceType, i, iconCol, isActive);
            }
        } else if (isSub) {
            for (int i = 0; i < 4; ++i) {
                const auto r = wavePillRects_[static_cast<std::size_t>(i)];
                const bool isActive = (slot.modelIndex == i);
                const bool isHov = (hoveredWavePill_ == i);

                g.setColour(isActive ? slotCol.withAlpha(0.24f) : (isHov ? theme_.bgControl.brighter(0.12f) : theme_.bgElevated));
                g.fillRoundedRectangle(r.toFloat(), 3.0f);
                g.setColour(isActive ? slotCol : (isHov ? theme_.borderFocus : theme_.borderSubtle));
                g.drawRoundedRectangle(r.toFloat(), 3.0f, isActive ? 1.2f : 0.8f);

                const auto iconCol = isActive ? slotCol.brighter(0.35f) : (isHov ? juce::Colours::white : theme_.textMuted);
                drawWaveformIcon(g, r.toFloat(), slot.engine, slot.sourceType, i, iconCol, isActive);
            }

            // Lowest Note Only Toggle
            const bool lowOn = slot.lowestNoteOnly;
            g.setColour(lowOn ? juce::Colour(0xff22c55e).withAlpha(0.25f) : theme_.bgElevated);
            g.fillRoundedRectangle(lowestNoteRect_.toFloat(), 3.0f);
            g.setColour(lowOn ? juce::Colour(0xff22c55e) : theme_.borderSubtle);
            g.drawRoundedRectangle(lowestNoteRect_.toFloat(), 3.0f, 1.0f);
            g.setFont(theme_.getValueFont(8.5f));
            g.setColour(lowOn ? juce::Colour(0xff4ade80) : theme_.textMuted);
            g.drawText(lowOn ? "LOW NOTE ONLY: ON" : "LOWEST NOTE ONLY", lowestNoteRect_, juce::Justification::centred, false);

            // Octave Steppers [-1, -2, -3]
            static const char* kOctPills[] = { "-1", "-2", "-3" };
            for (int i = 0; i < 3; ++i) {
                const auto r = subOctPillRects_[static_cast<std::size_t>(i)];
                const bool isActive = (slot.octaveOffset == (-1 - i));
                g.setColour(isActive ? slotCol : theme_.bgElevated);
                g.fillRoundedRectangle(r.toFloat(), 3.0f);
                g.setColour(isActive ? slotCol.brighter(0.3f) : theme_.borderSubtle);
                g.drawRoundedRectangle(r.toFloat(), 3.0f, 0.8f);
                g.setFont(theme_.getValueFont(9.0f));
                g.setColour(isActive ? juce::Colours::white : theme_.textMuted);
                g.drawText(kOctPills[i], r, juce::Justification::centred, false);
            }
        } else if (isNoise) {
            for (int i = 0; i < 4; ++i) {
                const auto r = wavePillRects_[static_cast<std::size_t>(i)];
                const bool isActive = (slot.modelIndex == i);
                const bool isHov = (hoveredWavePill_ == i);

                g.setColour(isActive ? slotCol.withAlpha(0.24f) : (isHov ? theme_.bgControl.brighter(0.12f) : theme_.bgElevated));
                g.fillRoundedRectangle(r.toFloat(), 3.0f);
                g.setColour(isActive ? slotCol : (isHov ? theme_.borderFocus : theme_.borderSubtle));
                g.drawRoundedRectangle(r.toFloat(), 3.0f, isActive ? 1.2f : 0.8f);

                const auto iconCol = isActive ? slotCol.brighter(0.35f) : (isHov ? juce::Colours::white : theme_.textMuted);
                drawWaveformIcon(g, r.toFloat(), slot.engine, slot.sourceType, i, iconCol, isActive);
            }
        }

        // Floating HUD Tooltip Badge
        if (hoveredWavePill_ >= 0) {
            const int maxPills = isAnalog ? 6 : ((isSub || isNoise) ? 4 : 0);
            if (hoveredWavePill_ < maxPills) {
                const auto targetRect = wavePillRects_[static_cast<std::size_t>(hoveredWavePill_)];
                const auto tooltipName = juce::String(nova::dsp::MultiEngineOscillator::getModelName(slot.engine, slot.sourceType, hoveredWavePill_));

                const auto font = theme_.getValueFont(8.5f);
                juce::GlyphArrangement ga;
                ga.addLineOfText(font, tooltipName, 0.0f, 0.0f);
                const int textW = static_cast<int>(ga.getBoundingBox(0, -1, true).getWidth());
                const int tipW = textW + 12;
                const int tipH = 15;
                const int rightX = 143;
                const int minX = rightX;
                const int maxX = getWidth() - 6 - tipW;
                const int tipX = std::clamp(targetRect.getCentreX() - tipW / 2, minX, std::max(minX, maxX));
                const int tipY = 5;

                juce::Rectangle<float> tipRect(static_cast<float>(tipX), static_cast<float>(tipY),
                                               static_cast<float>(tipW), static_cast<float>(tipH));

                g.setColour(theme_.bgSurface.darker(0.35f).withAlpha(0.95f));
                g.fillRoundedRectangle(tipRect, 3.0f);
                g.setColour(slotCol.withAlpha(0.85f));
                g.drawRoundedRectangle(tipRect, 3.0f, 0.9f);

                g.setFont(font);
                g.setColour(juce::Colours::white);
                g.drawFittedText(tooltipName, tipRect.toNearestInt(), juce::Justification::centred, 1, 0.85f);
            }
        }
    }

    static void drawWaveformIcon(juce::Graphics& g, juce::Rectangle<float> area,
                                 nova::dsp::MultiEngineOscillator::EngineType engine,
                                 int sourceType, int modelIndex,
                                 juce::Colour strokeCol, bool isActive) {
        const auto r = area.reduced(5.0f, 4.0f);
        const float l = r.getX();
        const float right = r.getRight();
        const float t = r.getY();
        const float b = r.getBottom();
        const float midY = r.getCentreY();
        const float midX = r.getCentreX();
        constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;

        using ET = nova::dsp::MultiEngineOscillator::EngineType;

        if (sourceType == 1 || engine == ET::SubOscillator) {
            juce::Path p;
            if (modelIndex == 0) {
                p.startNewSubPath(l, midY);
                p.quadraticTo(l + r.getWidth() * 0.25f, t, midX, midY);
                p.quadraticTo(l + r.getWidth() * 0.75f, b, right, midY);
            } else if (modelIndex == 1) {
                p.startNewSubPath(l, b);
                p.lineTo(midX, t);
                p.lineTo(right, b);
            } else if (modelIndex == 2) {
                for (int i = 0; i <= 24; ++i) {
                    const float u = static_cast<float>(i) / 24.0f;
                    const float x = l + u * r.getWidth();
                    const float s = std::tanh(std::sin(twoPi * u) * 2.2f) * 0.9f;
                    const float y = midY - s * (r.getHeight() * 0.45f);
                    if (i == 0) p.startNewSubPath(x, y);
                    else p.lineTo(x, y);
                }
            } else {
                p.startNewSubPath(l, t);
                p.lineTo(midX, t);
                p.lineTo(midX, b);
                p.lineTo(right, b);
            }
            g.setColour(strokeCol);
            g.strokePath(p, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        } else if (sourceType == 3 || engine == ET::NoiseTransient) {
            juce::Path p;
            if (modelIndex == 0) {
                static constexpr float kPink[] = { 0.5f, 0.2f, 0.7f, 0.35f, 0.8f, 0.4f, 0.6f, 0.25f, 0.5f };
                for (int i = 0; i < 9; ++i) {
                    const float x = l + (static_cast<float>(i) / 8.0f) * r.getWidth();
                    const float y = b - kPink[i] * r.getHeight();
                    if (i == 0) p.startNewSubPath(x, y);
                    else p.lineTo(x, y);
                }
            } else if (modelIndex == 1) {
                p.startNewSubPath(l, midY);
                p.lineTo(l + r.getWidth() * 0.30f, midY);
                p.lineTo(l + r.getWidth() * 0.35f, t);
                p.lineTo(l + r.getWidth() * 0.40f, b);
                p.lineTo(l + r.getWidth() * 0.45f, midY);
                p.lineTo(l + r.getWidth() * 0.70f, midY);
                p.lineTo(l + r.getWidth() * 0.75f, t + 2.0f);
                p.lineTo(l + r.getWidth() * 0.80f, midY);
                p.lineTo(right, midY);
            } else if (modelIndex == 2) {
                static constexpr float kWhite[] = { 0.5f, 0.9f, 0.1f, 0.8f, 0.2f, 0.75f, 0.3f, 0.85f, 0.15f, 0.5f };
                for (int i = 0; i < 10; ++i) {
                    const float x = l + (static_cast<float>(i) / 9.0f) * r.getWidth();
                    const float y = b - kWhite[i] * r.getHeight();
                    if (i == 0) p.startNewSubPath(x, y);
                    else p.lineTo(x, y);
                }
            } else {
                p.startNewSubPath(l, midY);
                p.lineTo(l + r.getWidth() * 0.15f, t);
                p.lineTo(l + r.getWidth() * 0.30f, b);
                p.lineTo(l + r.getWidth() * 0.45f, midY - 2.0f);
                p.lineTo(l + r.getWidth() * 0.60f, midY + 1.0f);
                p.lineTo(right, midY);
            }
            g.setColour(strokeCol);
            g.strokePath(p, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        } else if (sourceType == 4 || engine == ET::SamplePlayback) {
            juce::Path p;
            p.startNewSubPath(l, midY);
            p.lineTo(l + r.getWidth() * 0.12f, t + 1.0f);
            p.lineTo(l + r.getWidth() * 0.25f, b - 1.0f);
            p.lineTo(l + r.getWidth() * 0.40f, midY - r.getHeight() * 0.35f);
            p.lineTo(l + r.getWidth() * 0.60f, midY + r.getHeight() * 0.20f);
            p.lineTo(l + r.getWidth() * 0.80f, midY - r.getHeight() * 0.10f);
            p.lineTo(right, midY);
            g.setColour(strokeCol);
            g.strokePath(p, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        } else {
            switch (modelIndex) {
                case 0: { // Classic Saw
                    juce::Path p;
                    p.startNewSubPath(l, b);
                    p.lineTo(right, t);
                    p.lineTo(right, b);
                    g.setColour(strokeCol);
                    g.strokePath(p, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    break;
                }
                case 1: { // Pulse / Square
                    juce::Path p;
                    p.startNewSubPath(l, t);
                    p.lineTo(midX, t);
                    p.lineTo(midX, b);
                    p.lineTo(right, b);
                    g.setColour(strokeCol);
                    g.strokePath(p, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    break;
                }
                case 2: { // Warm Triangle
                    juce::Path p;
                    p.startNewSubPath(l, b);
                    p.lineTo(midX, t);
                    p.lineTo(right, b);
                    g.setColour(strokeCol);
                    g.strokePath(p, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    break;
                }
                case 3: { // Mellow Sine
                    juce::Path p;
                    p.startNewSubPath(l, midY);
                    p.quadraticTo(l + r.getWidth() * 0.25f, t, midX, midY);
                    p.quadraticTo(l + r.getWidth() * 0.75f, b, right, midY);
                    g.setColour(strokeCol);
                    g.strokePath(p, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    break;
                }
                case 4: { // Super Saw Multi
                    juce::Path p1;
                    p1.startNewSubPath(l, b - 1.0f);
                    p1.lineTo(l + r.getWidth() * 0.42f, t + 2.5f);
                    p1.lineTo(l + r.getWidth() * 0.42f, b - 1.0f);
                    g.setColour(strokeCol.withAlpha(isActive ? 0.6f : 0.4f));
                    g.strokePath(p1, juce::PathStrokeType(1.0f));

                    juce::Path p3;
                    p3.startNewSubPath(l + r.getWidth() * 0.58f, b - 1.0f);
                    p3.lineTo(right, t + 2.5f);
                    p3.lineTo(right, b - 1.0f);
                    g.setColour(strokeCol.withAlpha(isActive ? 0.6f : 0.4f));
                    g.strokePath(p3, juce::PathStrokeType(1.0f));

                    juce::Path p2;
                    p2.startNewSubPath(l + r.getWidth() * 0.25f, b);
                    p2.lineTo(l + r.getWidth() * 0.75f, t);
                    p2.lineTo(l + r.getWidth() * 0.75f, b);
                    g.setColour(strokeCol);
                    g.strokePath(p2, juce::PathStrokeType(1.4f));
                    break;
                }
                case 5: { // Hard Sync Lead
                    juce::Path p;
                    p.startNewSubPath(l, b);
                    p.lineTo(midX, t);
                    p.lineTo(midX, b);
                    p.lineTo(right, t);
                    p.lineTo(right, b);
                    g.setColour(strokeCol);
                    g.strokePath(p, juce::PathStrokeType(1.3f));
                    break;
                }
                default: break;
            }
        }
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

    // =========================================================================
    // Visualizer Renderer (mit echtem 3D Isometric Wavetable Ribbon Terrain)
    // =========================================================================
    void drawEngineVisualizer(juce::Graphics& g, juce::Rectangle<int> area,
                             const nova::dsp::MultiEngineOscillator::Parameters& slot,
                             juce::Colour slotCol, bool isExpandedView) {
        const auto bF = area.toFloat();
        g.setColour(theme_.bgControl.withAlpha(0.75f));
        g.fillRoundedRectangle(bF, 4.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(bF, 4.0f, 0.8f);

        const float midY = bF.getCentreY();
        const float w = bF.getWidth() - 8.0f;
        const float startX = bF.getX() + 4.0f;
        constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
        using ET = nova::dsp::MultiEngineOscillator::EngineType;

        if (slot.sourceType == 1 || slot.engine == ET::SubOscillator) {
            juce::Path p;
            for (int i = 0; i <= 48; ++i) {
                const float normX = static_cast<float>(i) / 48.0f;
                const float x = startX + normX * w;
                const float ph = std::fmod(normX * 2.0f, 1.0f);
                float wave = 0.0f;
                switch (slot.modelIndex) {
                    case 0: wave = std::sin(twoPi * ph); break;
                    case 1: wave = 2.0f * std::abs(2.0f * (ph - 0.5f)) - 1.0f; break;
                    case 2: wave = std::tanh(std::sin(twoPi * ph) * 2.4f) * 0.95f; break;
                    case 3: wave = (ph < 0.5f ? 0.85f : -0.85f); break;
                    default: wave = std::sin(twoPi * ph); break;
                }
                const float y = midY - wave * (bF.getHeight() * 0.32f);
                if (i == 0) p.startNewSubPath(x, y);
                else p.lineTo(x, y);
            }
            g.setColour(juce::Colour(0xff22c55e).withAlpha(0.35f));
            g.strokePath(p, juce::PathStrokeType(2.5f));
            g.setColour(juce::Colour(0xff4ade80));
            g.strokePath(p, juce::PathStrokeType(1.3f));
        } else if (slot.engine == ET::EvolvingWavetable) {
            draw3DWavetableTerrain(g, bF, slot, slotCol);
        } else if (slot.engine == ET::WarmAnalog) {
            juce::Path p;
            const int model = slot.modelIndex;
            if (model == 4) {
                for (int saw = 0; saw < 3; ++saw) {
                    const float sawOffset = static_cast<float>(saw) * 0.15f * (0.3f + slot.shapeMorph * 0.7f);
                    const float sAlpha = (saw == 0) ? 0.9f : 0.45f;
                    juce::Path sp;
                    for (int i = 0; i <= 48; ++i) {
                        const float normX = static_cast<float>(i) / 48.0f;
                        const float x = startX + normX * w;
                        const float sawCycle = std::fmod(normX * 2.5f + sawOffset, 1.0f);
                        const float y = midY - (sawCycle * 2.0f - 1.0f) * (bF.getHeight() * 0.30f);
                        if (i == 0) sp.startNewSubPath(x, y);
                        else sp.lineTo(x, y);
                    }
                    g.setColour(slotCol.withAlpha(sAlpha));
                    g.strokePath(sp, juce::PathStrokeType(saw == 0 ? 1.4f : 1.0f));
                }
            } else {
                for (int i = 0; i <= 48; ++i) {
                    const float normX = static_cast<float>(i) / 48.0f;
                    const float x = startX + normX * w;
                    const float ph = std::fmod(normX * 2.5f, 1.0f);
                    float wave = 0.0f;
                    switch (model) {
                        case 0: wave = (2.0f * ph - 1.0f); break;
                        case 1: {
                            const float pw = 0.5f - 0.38f * slot.shapeMorph;
                            wave = (ph < pw ? 0.9f : -0.9f);
                            break;
                        }
                        case 2: wave = -(2.0f * std::abs(2.0f * (ph - 0.5f)) - 1.0f); break;
                        case 3: {
                            const float s = std::sin(twoPi * ph);
                            wave = (slot.shapeMorph > 0.05f) ? std::sin(s * (1.0f + slot.shapeMorph * 2.8f) * (0.5f * std::numbers::pi_v<float>)) : s;
                            break;
                        }
                        case 5: {
                            const float syncFactor = 1.0f + 2.0f * slot.shapeMorph;
                            wave = (2.0f * std::fmod(ph * syncFactor, 1.0f) - 1.0f) * 0.85f;
                            break;
                        }
                        default: wave = (2.0f * ph - 1.0f); break;
                    }
                    const float y = midY - wave * (bF.getHeight() * 0.32f);
                    if (i == 0) p.startNewSubPath(x, y);
                    else p.lineTo(x, y);
                }
                g.setColour(slotCol.withAlpha(0.35f));
                g.strokePath(p, juce::PathStrokeType(2.5f));
                g.setColour(slotCol.brighter(0.4f));
                g.strokePath(p, juce::PathStrokeType(1.3f));
            }
        } else if (slot.sourceType == 2 || slot.engine == ET::PhysicalPluckModal) {
            juce::Path p;
            for (int i = 0; i <= 48; ++i) {
                const float normX = static_cast<float>(i) / 48.0f;
                const float x = startX + normX * w;
                const float decay = std::exp(-normX * (2.2f + (1.0f - slot.shapeMorph) * 2.5f));
                const float y = midY - std::sin(normX * twoPi * 6.0f) * decay * (bF.getHeight() * 0.35f);
                if (i == 0) p.startNewSubPath(x, y);
                else p.lineTo(x, y);
            }
            g.setColour(juce::Colour(0xffec4899).withAlpha(0.4f));
            g.strokePath(p, juce::PathStrokeType(2.2f));
            g.setColour(juce::Colour(0xfff472b6));
            g.strokePath(p, juce::PathStrokeType(1.2f));
        } else if (slot.engine == ET::OrganicTexture) {
            juce::Path p;
            for (int i = 0; i <= 48; ++i) {
                const float normX = static_cast<float>(i) / 48.0f;
                const float x = startX + normX * w;
                const float grainEnv = std::sin(normX * std::numbers::pi_v<float>);
                const float val = std::sin(normX * twoPi * (4.0f + slot.shapeMorph * 6.0f)) * grainEnv;
                const float y = midY - val * (bF.getHeight() * 0.32f);
                if (i == 0) p.startNewSubPath(x, y);
                else p.lineTo(x, y);
            }
            g.setColour(slotCol.withAlpha(0.35f));
            g.strokePath(p, juce::PathStrokeType(2.2f));
            g.setColour(slotCol.brighter(0.4f));
            g.strokePath(p, juce::PathStrokeType(1.2f));
        } else {
            juce::Path p;
            juce::Random rnd(slotIndex_ * 77 + slot.modelIndex * 13);
            for (int i = 0; i <= 48; ++i) {
                const float normX = static_cast<float>(i) / 48.0f;
                const float x = startX + normX * w;
                float r = (rnd.nextFloat() * 2.0f - 1.0f);
                if (slot.modelIndex == 3) r *= std::exp(-normX * 12.0f);
                const float y = midY + r * (bF.getHeight() * 0.28f);
                if (i == 0) p.startNewSubPath(x, y);
                else p.lineTo(x, y);
            }
            g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.65f));
            g.strokePath(p, juce::PathStrokeType(1.1f));
        }

        drawVisualizerBadge(g, area, slot, slotCol, isExpandedView);
    }

    void draw3DWavetableTerrain(juce::Graphics& g, juce::Rectangle<float> bF,
                                const nova::dsp::MultiEngineOscillator::Parameters& slot,
                                juce::Colour slotCol) {
        constexpr int numSlices = 10;
        constexpr int numSamples = 48;
        constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
        const int model = std::clamp(slot.modelIndex, 0, 7);

        const float totalW = bF.getWidth() - 14.0f;
        const float activeMorph = std::clamp(slot.shapeMorph, 0.0f, 1.0f);

        // Speichere Slice-Punkte für die longitudinalen 3D-Gitterlinien (Mesh-Rippen)
        std::array<std::array<juce::Point<float>, numSlices>, 5> gridRibs;
        constexpr std::array<int, 5> kRibIndices = { 0, 12, 24, 36, 48 };

        // 1. Berechne und zeichne Slices von hinten nach vorne (slice 0 = Ferne, slice numSlices-1 = Vordergrund)
        for (int slice = 0; slice < numSlices; ++slice) {
            const float zNorm = static_cast<float>(slice) / static_cast<float>(numSlices - 1); // 0 = Ferne, 1 = Nah
            const float invZ = 1.0f - zNorm;

            // Tiefe isometrische Projektion
            const float xOffset = invZ * 18.0f;
            const float yOffset = -invZ * 26.0f;
            const float sliceW = totalW - 20.0f - invZ * 12.0f;
            const float sliceX = bF.getX() + 6.0f + xOffset;
            const float baselineY = bF.getCentreY() + yOffset + 12.0f;

            juce::Path wavePath;
            juce::Path fillPath;

            for (int i = 0; i <= numSamples; ++i) {
                const float normX = static_cast<float>(i) / static_cast<float>(numSamples);
                const float x = sliceX + normX * sliceW;
                const float ph = normX * twoPi * 2.0f;

                const float zMorph = zNorm;
                float val = 0.0f;
                switch (model) {
                    case 0: val = 0.65f * std::sin(ph) + 0.35f * std::sin(ph * (1.0f + zMorph * 3.0f)); break;
                    case 1: val = 0.5f * std::sin(ph) + 0.3f * std::sin(ph * 3.0f) + 0.2f * std::sin(ph * (4.0f + zMorph * 4.0f)); break;
                    case 2: val = 0.55f * std::sin(ph) + 0.35f * std::sin(ph * 2.0f) + 0.25f * std::sin(ph * 4.0f * (1.0f + zMorph)); break;
                    case 3: val = 0.45f * std::sin(ph) + 0.38f * std::sin(ph * 3.0f) + 0.25f * std::sin(ph * (5.0f + zMorph * 3.0f)); break;
                    case 4: val = 0.5f * std::sin(ph) + 0.35f * std::sin(ph * 2.0f) + 0.25f * std::sin(ph * (2.0f + zMorph * 6.0f)); break;
                    case 5: val = 0.45f * std::sin(ph) + 0.35f * std::sin(ph * (3.0f + zMorph * 4.0f)); break;
                    case 6: val = 0.6f * std::sin(ph) + 0.3f * std::sin(ph * 2.0f) + 0.2f * std::sin(ph * 3.0f); break;
                    case 7: val = 0.4f * std::sin(ph) + 0.3f * std::sin(ph * (2.0f + zMorph * 6.0f)); break;
                    default: val = std::sin(ph); break;
                }

                // Deutlich prägnantere 3D-Höhe
                const float amp = (12.0f + zNorm * 3.5f);
                const float y = baselineY - val * amp;

                if (i == 0) {
                    wavePath.startNewSubPath(x, y);
                    fillPath.startNewSubPath(x, baselineY + 16.0f);
                    fillPath.lineTo(x, y);
                } else {
                    wavePath.lineTo(x, y);
                    fillPath.lineTo(x, y);
                }

                for (size_t r = 0; r < kRibIndices.size(); ++r) {
                    if (i == kRibIndices[r]) {
                        gridRibs[r][static_cast<size_t>(slice)] = juce::Point<float>(x, y);
                    }
                }
            }

            fillPath.lineTo(sliceX + sliceW, baselineY + 16.0f);
            fillPath.closeSubPath();

            // 3D Occlusion Fill
            g.setColour(theme_.bgControl.darker(0.35f).withAlpha(0.90f));
            g.fillPath(fillPath);

            const float distToMorph = std::abs(activeMorph - zNorm);
            const bool isNearActive = (distToMorph < 0.08f);

            if (isNearActive) {
                // Aktive Morph-Position strahlt als intensiver Neon-Laser
                g.setColour(slotCol.withAlpha(0.35f));
                g.strokePath(wavePath, juce::PathStrokeType(4.5f));
                g.setColour(slotCol.brighter(0.6f));
                g.strokePath(wavePath, juce::PathStrokeType(2.0f));
            } else {
                // Atmosphärische Tiefenstaffelung (hinten subtiler, vorne brillant)
                const float alpha = 0.16f + zNorm * 0.50f;
                g.setColour(slotCol.withAlpha(alpha));
                g.strokePath(wavePath, juce::PathStrokeType(1.1f));
            }
        }

        // 2. Zeichne longitudinale 3D-Gitterlinien (Mesh-Rippen verbinden die Slices zu echtem 3D Terrain)
        for (const auto& rib : gridRibs) {
            juce::Path ribPath;
            ribPath.startNewSubPath(rib[0]);
            for (size_t s = 1; s < numSlices; ++s) {
                ribPath.lineTo(rib[s]);
            }
            g.setColour(slotCol.withAlpha(0.20f));
            g.strokePath(ribPath, juce::PathStrokeType(0.8f));
        }

        // 3. 3D Laser-Cursor an der aktuellen WT POS
        const float cursorZNorm = activeMorph;
        const float cursorInvZ = 1.0f - cursorZNorm;
        const float cursorXOffset = cursorInvZ * 18.0f;
        const float cursorYOffset = -cursorInvZ * 26.0f;
        const float cursorSliceW = totalW - 20.0f - cursorInvZ * 12.0f;
        const float cursorX = bF.getX() + 6.0f + cursorXOffset + activeMorph * cursorSliceW;
        const float cursorBaselineY = bF.getCentreY() + cursorYOffset + 12.0f;

        // Vertikaler 3D Scanning-Laser-Strahl
        g.setColour(slotCol.withAlpha(0.25f));
        g.drawVerticalLine(static_cast<int>(cursorX), cursorBaselineY - 18.0f, cursorBaselineY + 12.0f);

        g.setColour(juce::Colours::white);
        g.fillEllipse(cursorX - 2.5f, cursorBaselineY - 2.5f, 5.0f, 5.0f);
        g.setColour(slotCol.brighter(0.4f));
        g.drawEllipse(cursorX - 5.0f, cursorBaselineY - 5.0f, 10.0f, 10.0f, 1.4f);
    }

    void drawVisualizerBadge(juce::Graphics& g, juce::Rectangle<int> area,
                            const nova::dsp::MultiEngineOscillator::Parameters& slot,
                            juce::Colour slotCol, bool isExpandedView) {
        juce::String badgeText;
        using ET = nova::dsp::MultiEngineOscillator::EngineType;

        if (slot.sourceType == 1 || slot.engine == ET::SubOscillator) {
            badgeText = juce::String(nova::dsp::MultiEngineOscillator::getModelName(slot.engine, 1, slot.modelIndex)).toUpperCase();
        } else if (slot.sourceType == 4 || slot.engine == ET::SamplePlayback) {
            badgeText = "SMP: " + juce::String(nova::dsp::MultiEngineOscillator::getModelName(slot.engine, slot.sourceType, slot.modelIndex)).toUpperCase();
        } else if (slot.engine == ET::WarmAnalog) {
            switch (slot.modelIndex) {
                case 0: badgeText = "CLASSIC SAW"; break;
                case 1: badgeText = (slot.shapeMorph > 0.05f) ? ("PULSE PWM " + juce::String(juce::roundToInt(slot.shapeMorph * 100.0f)) + "%") : "PULSE / SQUARE"; break;
                case 2: badgeText = "WARM TRIANGLE"; break;
                case 3: badgeText = "MELLOW SINE"; break;
                case 4: badgeText = "SUPER SAW"; break;
                case 5: badgeText = "HARD SYNC LEAD"; break;
                default: badgeText = "CLASSIC SAW"; break;
            }
        } else if (slot.engine == ET::EvolvingWavetable) {
            if (isExpandedView) {
                badgeText = "WT: " + juce::String(nova::dsp::MultiEngineOscillator::getModelName(slot.engine, slot.sourceType, slot.modelIndex)).toUpperCase();
            } else {
                badgeText = "3D WAVETABLE";
            }
        } else {
            const char* name = nova::dsp::MultiEngineOscillator::getModelName(slot.engine, slot.sourceType, slot.modelIndex);
            badgeText = juce::String(name).toUpperCase();
        }

        const auto font = theme_.getValueFont(8.0f);
        juce::GlyphArrangement ga;
        ga.addLineOfText(font, badgeText, 0.0f, 0.0f);
        const int textW = static_cast<int>(ga.getBoundingBox(0, -1, true).getWidth());
        const int badgeW = std::clamp(textW + 8, 40, area.getWidth() - 8);
        const auto badgeRect = juce::Rectangle<float>(static_cast<float>(area.getX() + 3),
                                                      static_cast<float>(area.getY() + 3),
                                                      static_cast<float>(badgeW), 12.0f);

        g.setColour(theme_.bgSurface.darker(0.3f).withAlpha(0.92f));
        g.fillRoundedRectangle(badgeRect, 2.5f);
        g.setColour(slotCol.withAlpha(0.70f));
        g.drawRoundedRectangle(badgeRect, 2.5f, 0.8f);

        g.setFont(font);
        g.setColour(juce::Colours::white);
        g.drawFittedText(badgeText, badgeRect.toNearestInt(), juce::Justification::centred, 1, 0.85f);
    }

    void openEngineMenu() {
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

    void openModelMenu() {
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

    void openRoutingMenu() {
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

    void openAmpEnvMenu() {
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

    static juce::Colour getSlotColour(int index) noexcept {
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

    static juce::String getSlotShortName(int index) {
        static const char* kNames[8] = { "OSC A", "OSC B", "OSC C", "OSC D", "OSC E", "OSC F", "OSC G", "OSC H" };
        return kNames[index % 8];
    }

    static juce::String getEngineDisplayName(const nova::dsp::MultiEngineOscillator::Parameters& slot) {
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

    static juce::String getModelDisplayName(const nova::dsp::WorshipSynthEngine::Parameters& params, int slotIdx) {
        if (slotIdx < 0 || slotIdx >= 8) return "Classic Saw";
        const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIdx)];
        return nova::dsp::MultiEngineOscillator::getModelName(slot.engine, slot.sourceType, slot.modelIndex);
    }

    static juce::String getFilterRoutingDisplayName(int filterRouting) {
        switch (filterRouting) {
            case 0: return "FLT 1";
            case 1: return "FLT 2";
            case 2: return "FLT 1+2";
            case 3: return "DIRECT";
            default: return "FLT 1";
        }
    }

    static juce::String getAmpEnvDisplayName(int ampEnvSource) {
        switch (ampEnvSource) {
            case 1:  return "ENV: FLT";
            case 2:  return "ENV: MOD 3";
            case 3:  return "ENV: AUX 4";
            default: return "ENV: AMP 1";
        }
    }

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int slotIndex_ = 0;
    bool isExpanded_ = false;
    bool isPinned_ = false;

    // Vollständige ArcKnobs für alle Kategorien
    nova::ui::ArcKnob octKnob_;
    nova::ui::ArcKnob semiKnob_;
    nova::ui::ArcKnob levelKnob_;
    nova::ui::ArcKnob morphKnob_;
    nova::ui::ArcKnob unisonKnob_;
    nova::ui::ArcKnob panKnob_;
    nova::ui::ArcKnob rateKnob_;
    nova::ui::ArcKnob scatterKnob_;
    nova::ui::ArcKnob durKnob_;
    nova::ui::ArcKnob warpKnob_;

    SleekHorizontalSlider compactVolSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compactVolAttachment_;

    SleekHorizontalSlider spreadSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spreadAttachment_;

    // Klick- und Trefferflächen
    juce::Rectangle<int> powerRect_;
    juce::Rectangle<int> soloRect_;
    juce::Rectangle<int> pinRect_;
    juce::Rectangle<int> engineRect_;
    juce::Rectangle<int> visualizerRect_;
    juce::Rectangle<int> modelRect_;
    juce::Rectangle<int> modelPrevRect_;
    juce::Rectangle<int> modelNextRect_;
    juce::Rectangle<int> routingRect_;
    juce::Rectangle<int> ampEnvRect_;
    juce::Rectangle<int> keyTrkRect_;
    juce::Rectangle<int> lowestNoteRect_;
    std::array<juce::Rectangle<int>, 6> wavePillRects_;
    std::array<juce::Rectangle<int>, 3> subOctPillRects_;
    int hoveredWavePill_ = -1;

    juce::String getTooltip() override {
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
};

// ==============================================================================
// AddSourceCardComponent: Eigenständige [+ Add Source] Karte
// ==============================================================================
class AddSourceCardComponent : public juce::Component {
public:
    std::function<void()> onAddClicked;

    AddSourceCardComponent(const nova::ui::NovaTheme& theme) : theme_(theme) {}

    void setCount(int activeCount) {
        if (activeCount_ != activeCount) {
            activeCount_ = activeCount;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        // Hintergrund mit dezentem Hover-Highlight
        g.setColour(isMouseOver() ? theme_.bgControl.brighter(0.08f) : theme_.bgControl.withAlpha(0.35f));
        g.fillRoundedRectangle(bounds, theme_.cornerRadius);

        // Gestrichelter Rand
        g.setColour(isMouseOver() ? theme_.accentPrimary : theme_.accentPrimary.withAlpha(0.45f));
        g.drawRoundedRectangle(bounds, theme_.cornerRadius, 1.2f);

        // Plus Icon
        g.setFont(theme_.getTitleFont(26.0f));
        g.setColour(isMouseOver() ? juce::Colours::white : theme_.accentPrimary);
        g.drawText("+", getLocalBounds().reduced(0, 30), juce::Justification::centred, false);

        // Text
        g.setFont(theme_.getValueFont(9.5f));
        g.setColour(isMouseOver() ? theme_.textPrimary : theme_.textMuted);
        g.drawFittedText("Add Source (" + juce::String(activeCount_) + "/8)",
                         getLocalBounds().removeFromBottom(28), juce::Justification::centred, 1, 0.85f);
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        if (auto* vp = findParentComponentOfClass<juce::Viewport>()) {
            vp->mouseWheelMove(e, wheel);
        }
    }

    void mouseEnter(const juce::MouseEvent&) override { setMouseCursor(juce::MouseCursor::PointingHandCursor); repaint(); }
    void mouseExit(const juce::MouseEvent&) override { setMouseCursor(juce::MouseCursor::NormalCursor); repaint(); }
    void mouseDown(const juce::MouseEvent&) override {
        if (onAddClicked) onAddClicked();
    }

private:
    const nova::ui::NovaTheme& theme_;
    int activeCount_ = 1;
};

// ==============================================================================
// SourceDeck: Horizontal scrollbarer Viewport mit weichem Accordion-Effekt
// ==============================================================================
class SourceDeck : public juce::Component, public juce::Timer {
public:
    std::function<void()> onSourceStateChanged;

    SourceDeck(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc), theme_(theme), addCard_(theme) {

        viewport_.setViewedComponent(&deckContent_, false);
        addAndMakeVisible(viewport_);

        for (int i = 0; i < 8; ++i) {
            auto card = std::make_unique<SourceCardComponent>(proc_, theme_, i);
            card->onCardStateChanged = [this](int /*slot*/) {
                refreshCards();
            };
            card->onExpandRequested = [this](int slotIdx, bool shouldExpand) {
                handleCardExpand(slotIdx, shouldExpand);
            };
            card->onPinToggled = [this](int slotIdx, bool isPinned) {
                handleCardPin(slotIdx, isPinned);
            };
            deckContent_.addChildComponent(*card);
            cards_.push_back(std::move(card));
        }

        addCard_.onAddClicked = [this]() {
            auto& apvts = proc_.getAPVTS();
            const auto params = proc_.getCurrentParameters();
            for (int i = 0; i < 8; ++i) {
                if (!params.oscSlots[static_cast<std::size_t>(i)].enabled) {
                    const juce::String prefix = "osc" + juce::String(i + 1) + "_";
                    if (auto* p = apvts.getParameter(prefix + "enabled")) {
                        p->setValueNotifyingHost(1.0f);
                    }
                    if (auto* pCount = apvts.getParameter("activeOscCount")) {
                        const int nextCount = std::max(proc_.getCurrentParameters().activeOscCount, i + 1);
                        pCount->setValueNotifyingHost(pCount->convertTo0to1(static_cast<float>(nextCount)));
                    }
                    proc_.syncParametersFromAPVTS();
                    refreshCards();
                    handleCardExpand(i, true);
                    return;
                }
            }
        };
        deckContent_.addChildComponent(addCard_);

        // All sources start collapsed in sleek square compact overview mode on launch
        for (auto& card : cards_) {
            card->setExpanded(false);
        }

        refreshCards();
        startTimerHz(60);
    }

    ~SourceDeck() override {
        stopTimer();
    }

    const std::vector<std::unique_ptr<SourceCardComponent>>& getCardList() const noexcept {
        return cards_;
    }

    void handleCardExpand(int slotIdx, bool shouldExpand) {
        for (int i = 0; i < 8; ++i) {
            if (i == slotIdx) {
                cards_[static_cast<std::size_t>(i)]->setExpanded(shouldExpand);
            } else if (!cards_[static_cast<std::size_t>(i)]->isPinned()) {
                cards_[static_cast<std::size_t>(i)]->setExpanded(false);
            }
        }
        autoScrollToCard(slotIdx);
    }

    void handleCardPin(int slotIdx, bool isPinned) {
        cards_[static_cast<std::size_t>(slotIdx)]->setPinned(isPinned);
        if (isPinned) {
            cards_[static_cast<std::size_t>(slotIdx)]->setExpanded(true);
            autoScrollToCard(slotIdx);
        } else {
            cards_[static_cast<std::size_t>(slotIdx)]->setExpanded(false);
        }
    }

    void collapseUnpinnedCards() {
        for (auto& card : cards_) {
            if (card->isVisible() && !card->isPinned()) {
                card->setExpanded(false);
            }
        }
    }

    void scrollToSlot(int slotIndex) {
        if (slotIndex >= 0 && slotIndex < 8) {
            handleCardExpand(slotIndex, true);
        }
    }

    void refreshCards() {
        const auto params = proc_.getCurrentParameters();
        for (int i = 0; i < 8; ++i) {
            const bool enabled = params.oscSlots[static_cast<std::size_t>(i)].enabled;
            cards_[static_cast<std::size_t>(i)]->setVisible(enabled);
            if (enabled) {
                cards_[static_cast<std::size_t>(i)]->updateDynamicLabels();
                cards_[static_cast<std::size_t>(i)]->updateControlVisibility();
                cards_[static_cast<std::size_t>(i)]->resized();
                cards_[static_cast<std::size_t>(i)]->repaint();
            }
        }
        updateDeckLayout();
        if (onSourceStateChanged) onSourceStateChanged();
    }

    void timerCallback() override {
        const auto params = proc_.getCurrentParameters();
        int activeCount = 0;
        uint64_t stateHash = 0;
        for (int i = 0; i < 8; ++i) {
            const auto& s = params.oscSlots[static_cast<std::size_t>(i)];
            if (s.enabled) {
                activeCount++;
                stateHash |= (1ULL << i);
            }
            stateHash ^= (static_cast<uint64_t>(s.engine) << (i * 6));
            stateHash ^= (static_cast<uint64_t>(s.sourceType) << (i * 6 + 3));
            stateHash ^= (static_cast<uint64_t>(s.modelIndex) << (i * 6 + 5));
        }

        if (stateHash != lastStateHash_) {
            lastStateHash_ = stateHash;
            lastActiveCount_ = activeCount;
            refreshCards();
        }

        // 60Hz Tweening der Karten-Breiten
        const int cardH = getHeight() - 14;
        const float compactTarget = static_cast<float>(cardH); // Quadratische Karten im kompakten Zustand

        bool needsLayout = false;
        for (auto& card : cards_) {
            if (card->isVisible()) {
                const float target = card->isExpanded() ? card->getPreferredExpandedWidth() : compactTarget;
                const float diff = target - card->currentWidth_;
                if (std::abs(diff) > 0.5f) {
                    card->currentWidth_ += diff * 0.28f;
                    needsLayout = true;
                } else {
                    card->currentWidth_ = target;
                }
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

    void autoScrollToCard(int slotIdx) {
        if (slotIdx >= 0 && slotIdx < 8 && cards_[static_cast<std::size_t>(slotIdx)]->isVisible()) {
            const int cardX = cards_[static_cast<std::size_t>(slotIdx)]->getX();
            const int targetW = static_cast<int>(cards_[static_cast<std::size_t>(slotIdx)]->isExpanded()
                ? cards_[static_cast<std::size_t>(slotIdx)]->getPreferredExpandedWidth()
                : static_cast<float>(getHeight() - 14));
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

private:
    void updateDeckLayout() {
        const auto params = proc_.getCurrentParameters();
        int activeCount = 0;
        for (int i = 0; i < 8; ++i) {
            if (params.oscSlots[static_cast<std::size_t>(i)].enabled) activeCount++;
        }
        lastActiveCount_ = activeCount;
        addCard_.setCount(activeCount);

        const int cardH = getHeight() - 14;
        int currentX = 4;

        for (int i = 0; i < 8; ++i) {
            if (params.oscSlots[static_cast<std::size_t>(i)].enabled) {
                const int w = static_cast<int>(std::round(cards_[static_cast<std::size_t>(i)]->currentWidth_));
                cards_[static_cast<std::size_t>(i)]->setBounds(currentX, 2, w, cardH);
                currentX += w + 6;
            }
        }

        // "+ Add Source" Karte
        if (activeCount < 8) {
            addCard_.setVisible(true);
            addCard_.setBounds(currentX, 2, 105, cardH);
            currentX += 105 + 6;
        } else {
            addCard_.setVisible(false);
        }

        const int targetW = std::max(getWidth(), currentX + 10);
        const int targetH = getHeight() - 6;
        if (deckContent_.getWidth() != targetW || deckContent_.getHeight() != targetH) {
            deckContent_.setSize(targetW, targetH);
        }
        deckContent_.repaint();
    }

    // =========================================================================
    // Innerer Container im Viewport
    // =========================================================================
    class DeckContentComponent : public juce::Component {
    public:
        DeckContentComponent(SourceDeck& owner) : owner_(owner) {}

        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
            owner_.viewport_.mouseWheelMove(e, wheel);
        }

        void mouseDown(const juce::MouseEvent& e) override {
            if (e.eventComponent == this) {
                for (auto& card : owner_.cards_) {
                    if (card->isVisible() && !card->isPinned()) {
                        card->setExpanded(false);
                    }
                }
            }
        }

    private:
        SourceDeck& owner_;
    };

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    HorizontalViewport viewport_;
    DeckContentComponent deckContent_{*this};

    std::vector<std::unique_ptr<SourceCardComponent>> cards_;
    AddSourceCardComponent addCard_;

    int lastActiveCount_ = 1;
    uint64_t lastStateHash_ = 0;
};

} // namespace nova::synth
