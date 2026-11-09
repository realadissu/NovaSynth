#include "ModulationHubPanel.h"
#include <algorithm>
#include <cmath>

namespace nova::synth::ui {

juce::String getOscShapeParamName(const nova::dsp::MultiEngineOscillator::Parameters& slot) {
    using ET = nova::dsp::MultiEngineOscillator::EngineType;
    if (slot.sourceType == 4 || slot.engine == ET::SamplePlayback) return "Sample Start";
    if (slot.sourceType == 1 || slot.engine == ET::SubOscillator) return "Drive";
    if (slot.sourceType == 2 || slot.engine == ET::PhysicalPluckModal) return "Brightness";
    if (slot.sourceType == 3 || slot.engine == ET::NoiseTransient) return "Color";
    if (slot.engine == ET::EvolvingWavetable) return "WT Pos";
    if (slot.engine == ET::OrganicTexture) return "Texture Pos";
    return "PWM / Shape";
}

juce::String getDynamicDestinationName(nova::dsp::ModMatrix::Destination dst,
                                      const nova::dsp::WorshipSynthEngine::Parameters& params) {
    using D = nova::dsp::ModMatrix::Destination;
    if (dst == D::Osc1Shape) return "OSC 1 " + getOscShapeParamName(params.oscSlots[0]);
    if (dst == D::Osc2Shape) return "OSC 2 " + getOscShapeParamName(params.oscSlots[1]);
    if (dst == D::Osc3Shape) return "OSC 3 " + getOscShapeParamName(params.oscSlots[2]);
    if (dst == D::Osc4Shape) return "OSC 4 " + getOscShapeParamName(params.oscSlots[3]);
    if (dst == D::Osc5Shape) return "OSC 5 " + getOscShapeParamName(params.oscSlots[4]);
    if (dst == D::Osc6Shape) return "OSC 6 " + getOscShapeParamName(params.oscSlots[5]);
    if (dst == D::Osc7Shape) return "OSC 7 " + getOscShapeParamName(params.oscSlots[6]);
    if (dst == D::Osc8Shape) return "OSC 8 " + getOscShapeParamName(params.oscSlots[7]);
    return nova::dsp::ModMatrix::getDestinationName(dst);
}

void ModulationHubPanel::drawMatrixCard(juce::Graphics& g, juce::Rectangle<float> area) {
    g.setColour(theme_.bgSurface.darker(0.12f));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    g.setFont(theme_.getSectionFont(12.0f));
    g.setColour(theme_.accentPrimary);
    g.drawText("MODULATION MATRIX", juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 150.0f, 16.0f), juce::Justification::left);

    const auto& mm = proc_.getModMatrix();
    int activeCount = 0;
    for (std::size_t i = 0; i < nova::dsp::ModMatrix::kMaxSlots; ++i) {
        const auto s = mm.getSlot(i);
        if (s.source != nova::dsp::ModMatrix::Source::None && s.destination != nova::dsp::ModMatrix::Destination::None) {
            ++activeCount;
        }
    }

    const juce::Rectangle<float> badgeRect(area.getX() + 165.0f, area.getY() + 6.0f, 80.0f, 16.0f);
    g.setColour(juce::Colour(0xff2dd4bf).withAlpha(0.18f));
    g.fillRoundedRectangle(badgeRect, 3.0f);
    g.setFont(theme_.getLabelFont(10.0f));
    g.setColour(theme_.accentPrimary);
    g.drawText(juce::String(activeCount) + " / 32 ACTIVE", badgeRect, juce::Justification::centred);

    // Add Route Button
    const auto addBtnF = addRouteBtnRect_.toFloat();
    g.setColour(juce::Colour(0xff2dd4bf).withAlpha(0.22f));
    g.fillRoundedRectangle(addBtnF, 4.0f);
    g.setColour(theme_.accentPrimary);
    g.drawRoundedRectangle(addBtnF, 4.0f, 1.0f);
    g.setFont(theme_.getSectionFont(11.0f));
    g.drawText("+ Add Route", addBtnF, juce::Justification::centred);

    // Route Rows Area
    const auto params = proc_.getCurrentParameters();
    const auto& rows = matrixRowRects_;
    std::vector<std::size_t> activeSlots;
    for (std::size_t i = 0; i < nova::dsp::ModMatrix::kMaxSlots; ++i) {
        const auto s = mm.getSlot(i);
        if (s.source != nova::dsp::ModMatrix::Source::None && s.destination != nova::dsp::ModMatrix::Destination::None) {
            activeSlots.push_back(i);
        }
    }

    if (activeSlots.empty()) {
        g.setFont(theme_.getLabelFont(11.0f));
        g.setColour(theme_.textMuted);
        g.drawText("No active modulations. Click '+ Add Route' to map LFOs, Envelopes or Macros.",
                   area.withTrimmedTop(35.0f).reduced(12.0f), juce::Justification::centred);
        return;
    }

    auto drawChevron = [&g](float rightX, float y, float h, juce::Colour c, float a) {
        juce::Path ch;
        const float cx = rightX - 8.0f;
        const float cy = y + h * 0.5f;
        ch.startNewSubPath(cx - 3.5f, cy - 1.5f);
        ch.lineTo(cx, cy + 2.0f);
        ch.lineTo(cx + 3.5f, cy - 1.5f);
        g.setColour(c.withAlpha(a));
        g.strokePath(ch, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    auto drawArrow = [&g](juce::Rectangle<int> r, juce::Colour c, float a) {
        juce::Path arr;
        const float cx = static_cast<float>(r.getCentreX());
        const float cy = static_cast<float>(r.getCentreY());
        arr.startNewSubPath(cx - 4.5f, cy);
        arr.lineTo(cx + 4.0f, cy);
        arr.startNewSubPath(cx + 1.0f, cy - 3.0f);
        arr.lineTo(cx + 4.5f, cy);
        arr.lineTo(cx + 1.0f, cy + 3.0f);
        g.setColour(c.withAlpha(a));
        g.strokePath(arr, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    for (int r = 0; r < static_cast<int>(rows.size()); ++r) {
        const int slotIdxIdx = r + matrixScrollOffset_;
        if (slotIdxIdx < 0 || slotIdxIdx >= static_cast<int>(activeSlots.size())) break;

        const std::size_t slotIdx = activeSlots[static_cast<std::size_t>(slotIdxIdx)];
        const auto slot = mm.getSlot(slotIdx);
        const auto& row = rows[static_cast<std::size_t>(r)];
        const bool isBypassed = !slot.enabled;
        const float alpha = isBypassed ? 0.38f : 1.0f;

        // Slot # indicator
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(theme_.textMuted.withAlpha(alpha));
        g.drawText("#" + juce::String(static_cast<int>(slotIdx) + 1), row.srcRect.getX() - 26, row.srcRect.getY(), 22, row.srcRect.getHeight(), juce::Justification::centredRight);

        // Source Selector Button
        const auto srcF = row.srcRect.toFloat();
        g.setColour(juce::Colour(0xff090d16).withAlpha(alpha));
        g.fillRoundedRectangle(srcF, 3.0f);
        g.setColour(theme_.borderSubtle.withAlpha(alpha));
        g.drawRoundedRectangle(srcF, 3.0f, 1.0f);
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(theme_.textPrimary.withAlpha(alpha));
        g.drawText(nova::dsp::ModMatrix::getSourceName(slot.source), srcF.reduced(4.0f, 0.0f), juce::Justification::left);
        drawChevron(srcF.getRight(), srcF.getY(), srcF.getHeight(), theme_.accentPrimary, alpha);

        drawArrow(row.arrowRect, theme_.textMuted, alpha);

        // Destination Selector Button
        const auto dstF = row.dstRect.toFloat();
        g.setColour(juce::Colour(0xff090d16).withAlpha(alpha));
        g.fillRoundedRectangle(dstF, 3.0f);
        g.setColour(theme_.borderSubtle.withAlpha(alpha));
        g.drawRoundedRectangle(dstF, 3.0f, 1.0f);
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(theme_.textPrimary.withAlpha(alpha));
        g.drawText(getDynamicDestinationName(slot.destination, params), dstF.reduced(4.0f, 0.0f), juce::Justification::left);
        drawChevron(dstF.getRight(), dstF.getY(), dstF.getHeight(), theme_.accentPrimary, alpha);

        // Bipolar Amount Slider Bar
        const auto amtF = row.amtRect.toFloat();
        g.setColour(juce::Colour(0xff090d16));
        g.fillRoundedRectangle(amtF, 3.0f);
        g.setColour(theme_.borderSubtle.withAlpha(alpha));
        g.drawRoundedRectangle(amtF, 3.0f, 1.0f);

        const float midX = amtF.getCentreX();
        g.setColour(theme_.textMuted.withAlpha(0.4f));
        g.drawVerticalLine(static_cast<int>(midX), amtF.getY() + 1.0f, amtF.getBottom() - 1.0f);

        const float amtVal = std::clamp(slot.amount, -1.0f, 1.0f);
        if (!isBypassed && std::abs(amtVal) > 0.005f) {
            const float barW = (amtF.getWidth() * 0.5f - 2.0f) * std::abs(amtVal);
            juce::Rectangle<float> fillBar = amtVal > 0.0f
                ? juce::Rectangle<float>(midX, amtF.getY() + 2.0f, barW, amtF.getHeight() - 4.0f)
                : juce::Rectangle<float>(midX - barW, amtF.getY() + 2.0f, barW, amtF.getHeight() - 4.0f);
            g.setColour(juce::Colour(amtVal > 0.0f ? 0xff2dd4bf : 0xfff59e0b).withAlpha(0.80f));
            g.fillRoundedRectangle(fillBar, 2.0f);
        }

        g.setFont(theme_.getLabelFont(9.0f));
        g.setColour(isBypassed ? theme_.textMuted : juce::Colours::white);
        const int pct = static_cast<int>(std::round(amtVal * 100.0f));
        g.drawText((pct > 0 ? "+" : "") + juce::String(pct) + "%", amtF, juce::Justification::centred);

        // Bypass Toggle Dot
        const auto bypF = row.bypassRect.toFloat();
        g.setColour(isBypassed ? theme_.textMuted.withAlpha(0.35f) : juce::Colour(0xff2dd4bf));
        g.fillEllipse(bypF.getCentreX() - 4.0f, bypF.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.setColour(theme_.borderSubtle);
        g.drawEllipse(bypF.getCentreX() - 4.0f, bypF.getCentreY() - 4.0f, 8.0f, 8.0f, 1.0f);

        // Delete Button (x)
        const auto delF = row.delRect.toFloat();
        g.setFont(theme_.getLabelFont(11.0f));
        g.setColour(theme_.textMuted);
        g.drawText("x", delF, juce::Justification::centred);
    }
}

void ModulationHubPanel::cycleLfoShape(int lfoIdx, bool forward) {
    const juce::String paramId = "lfo" + juce::String(lfoIdx + 1) + "Shape";
    if (auto* p = proc_.getAPVTS().getParameter(paramId)) {
        constexpr int numShapes = 5;
        const int cur = static_cast<int>(p->convertFrom0to1(p->getValue()));
        const int next = forward ? ((cur + 1) % numShapes) : ((cur + numShapes - 1) % numShapes);
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(next)));
        repaint();
    }
}

void ModulationHubPanel::addNewMatrixRoute() {
    auto& mm = proc_.getModMatrix();
    for (std::size_t i = 0; i < nova::dsp::ModMatrix::kMaxSlots; ++i) {
        if (!mm.getSlot(i).enabled && mm.getSlot(i).source == nova::dsp::ModMatrix::Source::None) {
            mm.setSlot(i, nova::dsp::ModMatrix::Source::Lfo1, nova::dsp::ModMatrix::Destination::FilterCutoff, 0.40f, true);
            repaint();
            return;
        }
    }
}

void ModulationHubPanel::deleteMatrixRoute(std::size_t slotIdx) {
    if (slotIdx < nova::dsp::ModMatrix::kMaxSlots) {
        proc_.getModMatrix().setSlot(slotIdx, nova::dsp::ModMatrix::Source::None, nova::dsp::ModMatrix::Destination::None, 0.0f, false);
        repaint();
    }
}

void ModulationHubPanel::showSourceMenu(std::size_t slotIdx, juce::Rectangle<int> targetRect) {
    juce::PopupMenu m;
    m.addItem(1, "LFO 1"); m.addItem(2, "LFO 2 (Drift)"); m.addItem(3, "LFO 3"); m.addItem(4, "LFO 4");
    m.addSeparator();
    m.addItem(5, "Amp Envelope"); m.addItem(6, "Filter Envelope"); m.addItem(7, "Mod Envelope 3"); m.addItem(8, "Aux Envelope 4");
    m.addSeparator();
    m.addItem(9, "Velocity"); m.addItem(10, "Mod Wheel (CC1)");
    m.addSeparator();
    m.addItem(11, "Macro Warmth"); m.addItem(12, "Macro Motion"); m.addItem(13, "Macro Shimmer"); m.addItem(14, "Macro Air");

    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(targetRect)),
        [this, slotIdx](int result) {
            if (result <= 0) return;
            nova::dsp::ModMatrix::Source src = nova::dsp::ModMatrix::Source::None;
            switch (result) {
                case 1:  src = nova::dsp::ModMatrix::Source::Lfo1; break;
                case 2:  src = nova::dsp::ModMatrix::Source::Lfo2; break;
                case 3:  src = nova::dsp::ModMatrix::Source::Lfo3; break;
                case 4:  src = nova::dsp::ModMatrix::Source::Lfo4; break;
                case 5:  src = nova::dsp::ModMatrix::Source::AmpEnv; break;
                case 6:  src = nova::dsp::ModMatrix::Source::FilterEnv; break;
                case 7:  src = nova::dsp::ModMatrix::Source::ModEnv3; break;
                case 8:  src = nova::dsp::ModMatrix::Source::AuxEnv4; break;
                case 9:  src = nova::dsp::ModMatrix::Source::Velocity; break;
                case 10: src = nova::dsp::ModMatrix::Source::ModWheel; break;
                case 11: src = nova::dsp::ModMatrix::Source::MacroWarmth; break;
                case 12: src = nova::dsp::ModMatrix::Source::MacroMotion; break;
                case 13: src = nova::dsp::ModMatrix::Source::MacroShimmer; break;
                case 14: src = nova::dsp::ModMatrix::Source::MacroAir; break;
            }
            auto& mm = proc_.getModMatrix();
            const auto cur = mm.getSlot(slotIdx);
            mm.setSlot(slotIdx, src, cur.destination, cur.amount, cur.enabled);
            repaint();
        });
}

void ModulationHubPanel::showDestinationMenu(std::size_t slotIdx, juce::Rectangle<int> targetRect) {
    juce::PopupMenu m;
    const auto params = proc_.getCurrentParameters();

    juce::PopupMenu fltMenu;
    fltMenu.addItem(1, "Filter 1 Cutoff"); fltMenu.addItem(2, "Filter 1 Resonance");
    fltMenu.addItem(3, "Filter 2 Cutoff"); fltMenu.addItem(4, "Filter 2 Resonance");
    fltMenu.addItem(5, "Vocal Carve (Mid Scoop)");
    m.addSubMenu("Filters", fltMenu);

    juce::PopupMenu levelMenu;
    levelMenu.addItem(10, "OSC 1 Level (Swell/Vol)"); levelMenu.addItem(11, "OSC 2 Level (Swell/Vol)");
    levelMenu.addItem(12, "OSC 3 Level (Swell/Pad)"); levelMenu.addItem(13, "OSC 4 Level (Swell/Sub)");
    levelMenu.addItem(14, "OSC 5 Level (Swell)"); levelMenu.addItem(15, "OSC 6 Level (Swell)");
    levelMenu.addItem(16, "OSC 7 Level (Swell)"); levelMenu.addItem(17, "OSC 8 Level (Swell)");
    m.addSubMenu("OSC Volume / Swell", levelMenu);

    juce::PopupMenu shapeMenu;
    for (int i = 0; i < 8; ++i) {
        shapeMenu.addItem(20 + i, "OSC " + juce::String(i + 1) + ": " + getOscShapeParamName(params.oscSlots[static_cast<std::size_t>(i)]));
    }
    m.addSubMenu("OSC Shape / Morph", shapeMenu);

    juce::PopupMenu pitchMenu;
    pitchMenu.addItem(30, "OSC 1 Pitch"); pitchMenu.addItem(31, "OSC 2 Pitch");
    pitchMenu.addItem(32, "OSC 3 Pitch"); pitchMenu.addItem(33, "OSC 4 Pitch");
    pitchMenu.addItem(34, "OSC 5 Pitch"); pitchMenu.addItem(35, "OSC 6 Pitch");
    pitchMenu.addItem(36, "OSC 7 Pitch"); pitchMenu.addItem(37, "OSC 8 Pitch");
    pitchMenu.addSeparator();
    pitchMenu.addItem(38, "Pitch All (Master Vibrato)");
    m.addSubMenu("OSC Pitch", pitchMenu);

    juce::PopupMenu envMenu;
    envMenu.addItem(50, "Amp Env Attack"); envMenu.addItem(51, "Amp Env Decay"); envMenu.addItem(52, "Amp Env Release");
    envMenu.addSeparator();
    envMenu.addItem(53, "Filter Env Attack"); envMenu.addItem(54, "Filter Env Decay"); envMenu.addItem(55, "Filter Env Release");
    envMenu.addSeparator();
    envMenu.addItem(56, "Mod Env 3 Attack"); envMenu.addItem(57, "Mod Env 3 Decay"); envMenu.addItem(58, "Mod Env 3 Release");
    envMenu.addSeparator();
    envMenu.addItem(70, "Aux Env 4 Attack"); envMenu.addItem(71, "Aux Env 4 Decay"); envMenu.addItem(72, "Aux Env Release");
    m.addSubMenu("Envelopes (ADSR Times)", envMenu);

    juce::PopupMenu lfoMenu;
    lfoMenu.addItem(60, "LFO 1 Rate / Speed"); lfoMenu.addItem(61, "LFO 2 Rate / Speed");
    lfoMenu.addItem(62, "LFO 3 Rate / Speed"); lfoMenu.addItem(63, "LFO 4 Rate / Speed");
    m.addSubMenu("LFO Speed", lfoMenu);

    juce::PopupMenu fxMenu;
    fxMenu.addItem(40, "Reverb Cloud Mix"); fxMenu.addItem(41, "Shimmer Amount");
    fxMenu.addItem(42, "Delay Mix"); fxMenu.addItem(43, "Delay Feedback");
    fxMenu.addItem(44, "Stereo Pan Spread");
    m.addSubMenu("Effects & Space", fxMenu);

    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(targetRect)),
        [this, slotIdx](int result) {
            if (result <= 0) return;
            nova::dsp::ModMatrix::Destination dst = nova::dsp::ModMatrix::Destination::None;
            switch (result) {
                case 1:  dst = nova::dsp::ModMatrix::Destination::FilterCutoff; break;
                case 2:  dst = nova::dsp::ModMatrix::Destination::FilterResonance; break;
                case 3:  dst = nova::dsp::ModMatrix::Destination::Filter2Cutoff; break;
                case 4:  dst = nova::dsp::ModMatrix::Destination::Filter2Resonance; break;
                case 5:  dst = nova::dsp::ModMatrix::Destination::VocalCarve; break;

                case 10: dst = nova::dsp::ModMatrix::Destination::Osc1Level; break;
                case 11: dst = nova::dsp::ModMatrix::Destination::Osc2Level; break;
                case 12: dst = nova::dsp::ModMatrix::Destination::Osc3Level; break;
                case 13: dst = nova::dsp::ModMatrix::Destination::Osc4Level; break;
                case 14: dst = nova::dsp::ModMatrix::Destination::Osc5Level; break;
                case 15: dst = nova::dsp::ModMatrix::Destination::Osc6Level; break;
                case 16: dst = nova::dsp::ModMatrix::Destination::Osc7Level; break;
                case 17: dst = nova::dsp::ModMatrix::Destination::Osc8Level; break;

                case 20: dst = nova::dsp::ModMatrix::Destination::Osc1Shape; break;
                case 21: dst = nova::dsp::ModMatrix::Destination::Osc2Shape; break;
                case 22: dst = nova::dsp::ModMatrix::Destination::Osc3Shape; break;
                case 23: dst = nova::dsp::ModMatrix::Destination::Osc4Shape; break;
                case 24: dst = nova::dsp::ModMatrix::Destination::Osc5Shape; break;
                case 25: dst = nova::dsp::ModMatrix::Destination::Osc6Shape; break;
                case 26: dst = nova::dsp::ModMatrix::Destination::Osc7Shape; break;
                case 27: dst = nova::dsp::ModMatrix::Destination::Osc8Shape; break;

                case 30: dst = nova::dsp::ModMatrix::Destination::Osc1Pitch; break;
                case 31: dst = nova::dsp::ModMatrix::Destination::Osc2Pitch; break;
                case 32: dst = nova::dsp::ModMatrix::Destination::Osc3Pitch; break;
                case 33: dst = nova::dsp::ModMatrix::Destination::Osc4Pitch; break;
                case 34: dst = nova::dsp::ModMatrix::Destination::Osc5Pitch; break;
                case 35: dst = nova::dsp::ModMatrix::Destination::Osc6Pitch; break;
                case 36: dst = nova::dsp::ModMatrix::Destination::Osc7Pitch; break;
                case 37: dst = nova::dsp::ModMatrix::Destination::Osc8Pitch; break;
                case 38: dst = nova::dsp::ModMatrix::Destination::OscPitchAll; break;

                case 50: dst = nova::dsp::ModMatrix::Destination::AmpAttack; break;
                case 51: dst = nova::dsp::ModMatrix::Destination::AmpDecay; break;
                case 52: dst = nova::dsp::ModMatrix::Destination::AmpRelease; break;
                case 53: dst = nova::dsp::ModMatrix::Destination::FilterAttack; break;
                case 54: dst = nova::dsp::ModMatrix::Destination::FilterDecay; break;
                case 55: dst = nova::dsp::ModMatrix::Destination::FilterRelease; break;
                case 56: dst = nova::dsp::ModMatrix::Destination::ModEnv3Attack; break;
                case 57: dst = nova::dsp::ModMatrix::Destination::ModEnv3Decay; break;
                case 58: dst = nova::dsp::ModMatrix::Destination::ModEnv3Release; break;
                case 70: dst = nova::dsp::ModMatrix::Destination::AuxEnv4Attack; break;
                case 71: dst = nova::dsp::ModMatrix::Destination::AuxEnv4Decay; break;
                case 72: dst = nova::dsp::ModMatrix::Destination::AuxEnv4Release; break;

                case 60: dst = nova::dsp::ModMatrix::Destination::Lfo1Rate; break;
                case 61: dst = nova::dsp::ModMatrix::Destination::Lfo2Rate; break;
                case 62: dst = nova::dsp::ModMatrix::Destination::Lfo3Rate; break;
                case 63: dst = nova::dsp::ModMatrix::Destination::Lfo4Rate; break;

                case 40: dst = nova::dsp::ModMatrix::Destination::ReverbMix; break;
                case 41: dst = nova::dsp::ModMatrix::Destination::ShimmerAmount; break;
                case 42: dst = nova::dsp::ModMatrix::Destination::DelayMix; break;
                case 43: dst = nova::dsp::ModMatrix::Destination::DelayFeedback; break;
                case 44: dst = nova::dsp::ModMatrix::Destination::PanSpread; break;
                default: break;
            }
            auto& mm = proc_.getModMatrix();
            const auto cur = mm.getSlot(slotIdx);
            mm.setSlot(slotIdx, cur.source, dst, cur.amount, cur.enabled);
            repaint();
        });
}

} // namespace nova::synth::ui

