#include "SourceCardComponent.h"
#include "nova_dsp/oscillators/SampleBank.h"
#include <numbers>
#include <cmath>
#include <algorithm>

namespace nova::synth {

    void SourceCardComponent::drawWaveformIcon(juce::Graphics& g, juce::Rectangle<float> area,
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
            const auto& smp = nova::dsp::SampleBank::getSample(modelIndex);
            if (smp.zones != nullptr && smp.numZones > 0 && smp.zones[0].pcmL != nullptr && smp.zones[0].numSamples > 0) {
                const auto& z = smp.zones[0];
                constexpr int numPts = 32;
                const int step = std::max(1, z.numSamples / numPts);
                juce::Path p;
                for (int i = 0; i <= numPts; ++i) {
                    const float normX = static_cast<float>(i) / static_cast<float>(numPts);
                    const float x = l + normX * r.getWidth();
                    const int sIdx = std::clamp(static_cast<int>(normX * static_cast<float>(z.numSamples - 1)), 0, z.numSamples - 1);
                    float maxV = 0.0f;
                    for (int k = 0; k < step && (sIdx + k) < z.numSamples; k += 8) {
                        maxV = std::max(maxV, std::abs(static_cast<float>(z.pcmL[sIdx + k]) * (1.0f / 32768.0f)));
                    }
                    const float y = midY - maxV * (r.getHeight() * 0.42f) * ((i % 2 == 0) ? 1.0f : -0.8f);
                    if (i == 0) p.startNewSubPath(x, midY);
                    else p.lineTo(x, y);
                }
                g.setColour(strokeCol);
                g.strokePath(p, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            } else {
                juce::Path p;
                p.startNewSubPath(l, midY);
                p.lineTo(l + r.getWidth() * 0.20f, t + 2.0f);
                p.lineTo(l + r.getWidth() * 0.40f, b - 2.0f);
                p.lineTo(right, midY);
                g.setColour(strokeCol);
                g.strokePath(p, juce::PathStrokeType(1.2f));
            }
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

    void SourceCardComponent::draw3DWavetableTerrain(juce::Graphics& g, juce::Rectangle<float> bF,
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

} // namespace nova::synth
