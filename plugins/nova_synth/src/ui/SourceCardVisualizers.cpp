#include "SourceCardComponent.h"
#include "nova_dsp/oscillators/SampleBank.h"
#include <numbers>
#include <cmath>
#include <algorithm>

namespace nova::synth {

    void SourceCardComponent::drawEngineVisualizer(juce::Graphics& g, juce::Rectangle<int> area,
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
        } else if (slot.sourceType == 4 || slot.engine == ET::SamplePlayback) {
            // High-resolution sampled audio waveform & dynamic START offset indicator
            const auto& smp = nova::dsp::SampleBank::getSample(slot.modelIndex);
            if (smp.zones != nullptr && smp.numZones > 0 && smp.zones[0].pcmL != nullptr && smp.zones[0].numSamples > 0) {
                const auto& z = smp.zones[0];
                constexpr int numPts = 64;
                const int step = std::max(1, z.numSamples / numPts);
                juce::Path waveTop, waveBot;
                const float currentMorph = static_cast<float>(morphKnob_.getSlider().getValue());
                const float startOffsetNorm = std::clamp(currentMorph, 0.0f, 1.0f);
                const float startLineX = startX + startOffsetNorm * w;

                const float loopStartNorm = std::clamp(static_cast<float>(loopStartKnob_.getSlider().getValue()), 0.0f, 0.98f);
                const float loopEndNorm = std::clamp(static_cast<float>(loopEndKnob_.getSlider().getValue()), 0.02f, 1.0f);
                const float loopStartX = startX + loopStartNorm * w;
                const float loopEndX = startX + loopEndNorm * w;

                for (int i = 0; i <= numPts; ++i) {
                    const float normX = static_cast<float>(i) / static_cast<float>(numPts);
                    const float x = startX + normX * w;
                    const int sIdx = std::clamp(static_cast<int>(normX * static_cast<float>(z.numSamples - 1)), 0, z.numSamples - 1);
                    float maxV = 0.0f;
                    for (int k = 0; k < step && (sIdx + k) < z.numSamples; k += 4) {
                        maxV = std::max(maxV, std::abs(static_cast<float>(z.pcmL[sIdx + k]) * (1.0f / 32768.0f)));
                    }
                    const float yTop = midY - maxV * (bF.getHeight() * 0.38f);
                    const float yBot = midY + maxV * (bF.getHeight() * 0.38f);
                    if (i == 0) {
                        waveTop.startNewSubPath(x, yTop);
                        waveBot.startNewSubPath(x, yBot);
                    } else {
                        waveTop.lineTo(x, yTop);
                        waveBot.lineTo(x, yBot);
                    }
                }

                // Sample Fill
                juce::Path fillP = waveTop;
                fillP.lineTo(startX + w, midY);
                fillP.lineTo(startX, midY);
                fillP.closeSubPath();
                g.setColour(slotCol.withAlpha(0.18f));
                g.fillPath(fillP);

                // Waveform contour
                g.setColour(slotCol.withAlpha(0.85f));
                g.strokePath(waveTop, juce::PathStrokeType(1.4f));
                g.strokePath(waveBot, juce::PathStrokeType(1.1f));

                // Shaded Loop Region & Direction Indicators (when loopMode > 0)
                if (slot.sampleLoopMode > 0 && loopEndX > loopStartX) {
                    const auto loopBox = juce::Rectangle<float>(loopStartX, bF.getY() + 3.0f, loopEndX - loopStartX, bF.getHeight() - 6.0f);
                    g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.12f));
                    g.fillRoundedRectangle(loopBox, 2.0f);
                    g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.35f));
                    g.drawRoundedRectangle(loopBox, 2.0f, 0.8f);

                    // Procedural vector loop direction arrow
                    const float arrowMidX = loopBox.getCentreX();
                    const float arrowY = bF.getBottom() - 14.0f;
                    juce::Path arrowP;
                    if (slot.sampleLoopMode == 1) {
                        // Forward ->
                        arrowP.startNewSubPath(arrowMidX - 6.0f, arrowY);
                        arrowP.lineTo(arrowMidX + 4.0f, arrowY);
                        arrowP.startNewSubPath(arrowMidX + 1.0f, arrowY - 3.5f);
                        arrowP.lineTo(arrowMidX + 5.0f, arrowY);
                        arrowP.lineTo(arrowMidX + 1.0f, arrowY + 3.5f);
                    } else if (slot.sampleLoopMode == 2) {
                        // Ping-Pong <->
                        arrowP.startNewSubPath(arrowMidX - 7.0f, arrowY);
                        arrowP.lineTo(arrowMidX + 7.0f, arrowY);
                        // Left head
                        arrowP.startNewSubPath(arrowMidX - 4.0f, arrowY - 3.0f);
                        arrowP.lineTo(arrowMidX - 8.0f, arrowY);
                        arrowP.lineTo(arrowMidX - 4.0f, arrowY + 3.0f);
                        // Right head
                        arrowP.startNewSubPath(arrowMidX + 4.0f, arrowY - 3.0f);
                        arrowP.lineTo(arrowMidX + 8.0f, arrowY);
                        arrowP.lineTo(arrowMidX + 4.0f, arrowY + 3.0f);
                    } else if (slot.sampleLoopMode == 3) {
                        // Reverse <-
                        arrowP.startNewSubPath(arrowMidX + 6.0f, arrowY);
                        arrowP.lineTo(arrowMidX - 4.0f, arrowY);
                        arrowP.startNewSubPath(arrowMidX - 1.0f, arrowY - 3.5f);
                        arrowP.lineTo(arrowMidX - 5.0f, arrowY);
                        arrowP.lineTo(arrowMidX - 1.0f, arrowY + 3.5f);
                    }
                    g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.75f));
                    g.strokePath(arrowP, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                    // LOOP START laser marker with handle
                    const float clampedLoopStartX = std::clamp(loopStartX, startX, startX + w);
                    g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.85f));
                    g.drawVerticalLine(static_cast<int>(clampedLoopStartX), bF.getY() + 4.0f, bF.getBottom() - 4.0f);

                    juce::Path lsHandle;
                    lsHandle.startNewSubPath(clampedLoopStartX - 3.5f, bF.getY() + 4.0f);
                    lsHandle.lineTo(clampedLoopStartX + 3.5f, bF.getY() + 4.0f);
                    lsHandle.lineTo(clampedLoopStartX, bF.getY() + 9.0f);
                    lsHandle.closeSubPath();
                    g.fillPath(lsHandle);

                    g.setFont(theme_.getValueFont(8.0f));
                    g.setColour(juce::Colour(0xff38bdf8));
                    const int lsTextX = (clampedLoopStartX + 32.0f < startX + w) ? static_cast<int>(clampedLoopStartX + 3.0f) : static_cast<int>(clampedLoopStartX - 30.0f);
                    g.drawText("L1", lsTextX, static_cast<int>(bF.getY() + 4.0f), 28, 10, juce::Justification::centredLeft);

                    // LOOP END laser marker with handle
                    const float clampedLoopEndX = std::clamp(loopEndX, startX, startX + w);
                    g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.85f));
                    g.drawVerticalLine(static_cast<int>(clampedLoopEndX), bF.getY() + 4.0f, bF.getBottom() - 4.0f);

                    juce::Path leHandle;
                    leHandle.startNewSubPath(clampedLoopEndX - 3.5f, bF.getY() + 4.0f);
                    leHandle.lineTo(clampedLoopEndX + 3.5f, bF.getY() + 4.0f);
                    leHandle.lineTo(clampedLoopEndX, bF.getY() + 9.0f);
                    leHandle.closeSubPath();
                    g.fillPath(leHandle);

                    const int leTextX = (clampedLoopEndX - 30.0f > startX) ? static_cast<int>(clampedLoopEndX - 30.0f) : static_cast<int>(clampedLoopEndX + 3.0f);
                    g.drawText("L2", leTextX, static_cast<int>(bF.getY() + 4.0f), 28, 10, juce::Justification::centredLeft);
                }

                // Sample START offset laser marker with handle
                const float clampedLineX = std::clamp(startLineX, startX, startX + w);
                g.setColour(juce::Colours::white.withAlpha(0.95f));
                g.drawVerticalLine(static_cast<int>(clampedLineX), bF.getY() + 4.0f, bF.getBottom() - 4.0f);

                juce::Path markerHandle;
                markerHandle.startNewSubPath(clampedLineX - 4.0f, bF.getY() + 4.0f);
                markerHandle.lineTo(clampedLineX + 4.0f, bF.getY() + 4.0f);
                markerHandle.lineTo(clampedLineX, bF.getY() + 10.0f);
                markerHandle.closeSubPath();
                g.setColour(juce::Colours::white);
                g.fillPath(markerHandle);

                g.setFont(theme_.getValueFont(8.5f));
                g.setColour(juce::Colours::white);
                const int textX = (clampedLineX + 44.0f < startX + w) ? static_cast<int>(clampedLineX + 3.0f) : static_cast<int>(clampedLineX - 42.0f);
                g.drawText("START", textX, static_cast<int>(bF.getY() + 4.0f), 40, 11, juce::Justification::centredLeft);
            }
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

    void SourceCardComponent::drawVisualizerBadge(juce::Graphics& g, juce::Rectangle<int> area,
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

} // namespace nova::synth
