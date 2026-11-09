#include "BinaryData.h"
#include "SourceCardComponent.h"
#include <numbers>
#include <cmath>

namespace nova::synth {

    void SourceCardComponent::paint(juce::Graphics& g) {
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

    void SourceCardComponent::drawPillWithArrow(juce::Graphics& g, juce::Rectangle<int> rect, const juce::String& text,
                                  juce::Colour bg, juce::Colour border, juce::Colour textCol,
                                  const juce::Font& font) {
        g.setColour(bg);
        g.fillRoundedRectangle(rect.toFloat(), 3.0f);
        g.setColour(border);
        g.drawRoundedRectangle(rect.toFloat(), 3.0f, 0.8f);

        auto r = rect.reduced(3, 1);
        const float triW = 4.0f;
        const float triH = 3.0f;
        auto triRect = r.removeFromRight(static_cast<int>(triW + 3)).toFloat()
                        .withSizeKeepingCentre(triW, triH);
        juce::Path tri;
        tri.addTriangle(triRect.getX(), triRect.getY(),
                        triRect.getRight(), triRect.getY(),
                        triRect.getCentreX(), triRect.getBottom());
        g.setColour(textCol.withAlpha(0.80f));
        g.fillPath(tri);

        g.setFont(font);
        g.setColour(textCol);
        g.drawFittedText(text, r, juce::Justification::centred, 1, 0.85f);
    }

    void SourceCardComponent::paintCompact(juce::Graphics& g, juce::Colour slotCol,
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

        // Envelope Pill (directly visible & clickable in compact card!)
        if (ampEnvRect_.getWidth() >= 24) {
            const bool isCustomEnv = (slot.ampEnvSource != 0);
            const juce::String envShort = (slot.ampEnvSource == 1) ? "FLT"
                                        : ((slot.ampEnvSource == 2) ? "M3"
                                        : ((slot.ampEnvSource == 3) ? "AUX"
                                        : "AMP"));
            drawPillWithArrow(g, ampEnvRect_, envShort, theme_.bgElevated,
                              isCustomEnv ? theme_.accentWarm : theme_.borderSubtle,
                              isCustomEnv ? theme_.accentWarm : theme_.textMuted,
                              theme_.getValueFont(8.5f));
        }

        // Visualizer Screen
        drawEngineVisualizer(g, visualizerRect_, slot, slotCol, false);
    }

    void SourceCardComponent::paintExpanded(juce::Graphics& g, juce::Colour slotCol,
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
        drawPillWithArrow(g, engineRect_, getEngineDisplayName(slot), theme_.bgElevated,
                          theme_.borderSubtle, slotCol, theme_.getValueFont(9.0f));

        // Visualizer Screen (Waveform / 3D Wavetable)
        drawEngineVisualizer(g, visualizerRect_, slot, slotCol, true);

        // Filter Routing Pill (links unten über Model)
        drawPillWithArrow(g, routingRect_, getFilterRoutingDisplayName(slot.filterRouting),
                          theme_.bgElevated, theme_.borderFocus, theme_.accentPrimary,
                          theme_.getValueFont(9.0f));

        // Amp Env Pill (rechts neben Routing)
        const bool isCustomEnv = (slot.ampEnvSource != 0);
        drawPillWithArrow(g, ampEnvRect_, getAmpEnvDisplayName(slot.ampEnvSource),
                          theme_.bgElevated, isCustomEnv ? theme_.accentWarm : theme_.borderSubtle,
                          isCustomEnv ? theme_.accentWarm : theme_.textSecondary,
                          theme_.getValueFont(9.0f));

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
        const bool isSample   = (slot.sourceType == 4 || slot.engine == ET::SamplePlayback);

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

            // Lowest Note Only Voicing Toggle (Sub)
            const bool lowOn = slot.lowestNoteOnly;
            g.setColour(lowOn ? juce::Colour(0xff22c55e).withAlpha(0.25f) : theme_.bgElevated);
            g.fillRoundedRectangle(lowestNoteRect_.toFloat(), 3.0f);
            g.setColour(lowOn ? juce::Colour(0xff22c55e) : theme_.borderSubtle);
            g.drawRoundedRectangle(lowestNoteRect_.toFloat(), 3.0f, 1.0f);
            g.setFont(theme_.getValueFont(8.5f));
            g.setColour(lowOn ? juce::Colour(0xff4ade80) : theme_.textMuted);
            g.drawText(lowOn ? "VOICING: LOW NOTE (MONO)" : "VOICING: POLYPHONIC", lowestNoteRect_, juce::Justification::centred, false);

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
        } else if (isSample) {
            static const char* kLoopPillLabels[] = { "ONE-SHOT", "FORWARD", "PING-PONG", "REVERSE" };
            for (int i = 0; i < 4; ++i) {
                const auto r = wavePillRects_[static_cast<std::size_t>(i)];
                if (r.isEmpty()) continue;
                const bool isActive = (slot.sampleLoopMode == i);
                const bool isHov = (hoveredWavePill_ == i);

                g.setColour(isActive ? slotCol.withAlpha(0.24f) : (isHov ? theme_.bgControl.brighter(0.12f) : theme_.bgElevated));
                g.fillRoundedRectangle(r.toFloat(), 3.0f);
                g.setColour(isActive ? slotCol : (isHov ? theme_.borderFocus : theme_.borderSubtle));
                g.drawRoundedRectangle(r.toFloat(), 3.0f, isActive ? 1.2f : 0.8f);

                g.setFont(theme_.getValueFont(8.5f));
                g.setColour(isActive ? juce::Colours::white : (isHov ? theme_.textPrimary : theme_.textMuted));
                g.drawText(kLoopPillLabels[i], r, juce::Justification::centred, false);
            }
        }

        // Floating HUD Tooltip Badge
        if (hoveredWavePill_ >= 0) {
            static const char* kLoopTooltips[] = { "ONE-SHOT", "FORWARD", "PING-PONG", "REVERSE" };
            const int maxPills = isAnalog ? 6 : ((isSub || isNoise || isSample) ? 4 : 0);
            if (hoveredWavePill_ < maxPills) {
                const auto targetRect = wavePillRects_[static_cast<std::size_t>(hoveredWavePill_)];
                const auto tooltipName = isSample
                    ? juce::String(kLoopTooltips[hoveredWavePill_])
                    : juce::String(nova::dsp::MultiEngineOscillator::getModelName(slot.engine, slot.sourceType, hoveredWavePill_));

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

    void SourceCardComponent::drawPinIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour oscCol, bool isPinned) {
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

} // namespace nova::synth
