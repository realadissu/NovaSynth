#include "FxRackPanel.h"
#include "WorkbenchVisualizers.h"
#include "../NovaSynthProcessor.h"
#include "nova_dsp/modulation/ModMatrix.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace nova::synth {

FxRackPanel::FxRackPanel(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
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

FxRackPanel::~FxRackPanel() {
    stopTimer();
}

void FxRackPanel::timerCallback() {
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

void FxRackPanel::resized() {
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

void FxRackPanel::mouseDown(const juce::MouseEvent& event) {
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

void FxRackPanel::paint(juce::Graphics& g) {
    const auto params = proc_.getCurrentParameters();

    // Module 1: STEREO TAPE DELAY (Teal 0xff2dd4bf)
    drawCardBackground(g, cardDelayRect_, juce::Colour(0xff0d161c), juce::Colour(0xff2dd4bf).withAlpha(0.35f));
    drawDelayHeader(g, cardDelayRect_, params);
    drawDelayVisualizer(g, delayVisRect_, params);

    // Module 2: NOVA SPACE REVERB (Purple 0xffc084fc)
    drawCardBackground(g, cardReverbRect_, juce::Colour(0xff150d22), juce::Colour(0xffc084fc).withAlpha(0.35f));
    drawReverbHeader(g, cardReverbRect_, params);
    drawReverbVisualizer(g, reverbVisRect_, params, theme_, phase_);

    // Module 3: PERFORMANCE MACROS (Amber 0xfffbbf24)
    drawCardBackground(g, cardMacrosRect_, juce::Colour(0xff1c160c), juce::Colour(0xfffbbf24).withAlpha(0.35f));
    drawMacrosHeader(g, cardMacrosRect_, params);
    drawMacrosVisualizer(g, macroVisRect_, params, theme_, phase_);

    // Module 4: MASTER OUTPUT (Green 0xff22c55e)
    drawCardBackground(g, cardMasterRect_, juce::Colour(0xff0d1a14), juce::Colour(0xff22c55e).withAlpha(0.35f));
    drawMasterHeader(g, cardMasterRect_, params);
    drawMasterVisualizer(g, masterVisRect_, theme_, smoothPeakL_, smoothPeakR_, peakHoldL_, peakHoldR_);
}

} // namespace nova::synth

