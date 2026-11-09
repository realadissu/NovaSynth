#include "SourceCardComponent.h"
#include <algorithm>

namespace nova::synth {

    [[nodiscard]] float SourceCardComponent::getPreferredExpandedWidth() const noexcept {
        const auto params = proc_.getCurrentParameters();
        const auto& slot = params.oscSlots[static_cast<std::size_t>(slotIndex_)];
        if (slot.engine == nova::dsp::MultiEngineOscillator::EngineType::OrganicTexture && slot.sourceType == 0) {
            return 520.0f;
        }
        if (slot.sourceType == 4 || slot.engine == nova::dsp::MultiEngineOscillator::EngineType::SamplePlayback) {
            return 470.0f;
        }
        return 390.0f;
    }

    void SourceCardComponent::resized() {
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
            header.removeFromLeft(48); // Badge
            header.removeFromLeft(3);  // Gap
            header.removeFromRight(3); // Gap before solo
            ampEnvRect_ = header;      // Directly accessible Envelope Routing Pill!
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
            const int routingPillW = (leftW - 6 - 4) / 2;
            routingRect_    = juce::Rectangle<int>(6, getHeight() - 46, routingPillW, 18);
            ampEnvRect_     = juce::Rectangle<int>(6 + routingPillW + 4, getHeight() - 46, routingPillW, 18);
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
            const bool isSample   = (slot.sourceType == 4 || slot.engine == ET::SamplePlayback);
            juce::ignoreUnused(isNoise, isSample);

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

                const int octW = 28;
                for (int i = 0; i < 3; ++i) {
                    subOctPillRects_[static_cast<std::size_t>(i)] = juce::Rectangle<int>(rightX + rightW - (3 - i) * (octW + 3), 50, octW, 20);
                }
                const int octTotalW = 3 * (octW + 3);
                lowestNoteRect_ = juce::Rectangle<int>(rightX, 50, std::max(140, rightW - octTotalW - 6), 20);

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
            } else if (isSample) {
                const int pillCount = 4;
                const int gap = 4;
                const int pillW = (rightW - (pillCount - 1) * gap) / pillCount;
                for (int i = 0; i < pillCount; ++i) {
                    wavePillRects_[static_cast<std::size_t>(i)] = juce::Rectangle<int>(rightX + i * (pillW + gap), 26, pillW, 20);
                }
                wavePillRects_[4] = {};
                wavePillRects_[5] = {};

                const int yRow1 = 50;
                const int kh = (getHeight() - 56) / 2;
                const int yRow2 = 50 + kh + 2;

                // Row 1: Tuning & Output (4 equal columns: OCT, SEMI, LEVEL, PAN)
                const int kw4 = (rightW - 12) / 4;
                octKnob_.setBounds(rightX, yRow1, kw4, kh);
                semiKnob_.setBounds(rightX + kw4 + 4, yRow1, kw4, kh);
                levelKnob_.setBounds(rightX + (kw4 + 4) * 2, yRow1, kw4, kh);
                panKnob_.setBounds(rightX + (kw4 + 4) * 3, yRow1, kw4, kh);

                // Row 2: Sample Shaping & Looping (6 equal columns: START, LOOP ST, LOOP END, XFADE, SPREAD, DETUNE)
                const int kw6 = (rightW - 20) / 6;
                morphKnob_.setBounds(rightX, yRow2, kw6, kh);
                loopStartKnob_.setBounds(rightX + kw6 + 4, yRow2, kw6, kh);
                loopEndKnob_.setBounds(rightX + (kw6 + 4) * 2, yRow2, kw6, kh);
                loopXfadeKnob_.setBounds(rightX + (kw6 + 4) * 3, yRow2, kw6, kh);
                unisonKnob_.setBounds(rightX + (kw6 + 4) * 4, yRow2, kw6, kh);
                scatterKnob_.setBounds(rightX + (kw6 + 4) * 5, yRow2, kw6, kh);
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

} // namespace nova::synth
