#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "nova_ui/NovaTheme.h"
#include "nova_ui/ArcKnob.h"
#include "nova_dsp/modulation/ModMatrix.h"
#include "nova_dsp/modulation/LfoGenerator.h"
#include "../NovaSynthProcessor.h"
#include <array>
#include <vector>
#include <memory>

namespace nova::synth::ui {

/**
 * @brief Integrated All-in-One Worship Modulation Hub (Proposal 1).
 * Features:
 *  1. 4 LFO cards with interactive waveform selection, animated phase tracking, Rate & Depth ArcKnobs.
 *  2. Worship Performance Macros (Warmth, Motion, Shimmer, Air) + Live Performance Meters (ModWheel, Vel, AT, PB).
 *  3. Quick Modulation Matrix with dynamic slot routing, bipolar amount sliders (-100%..+100%), bypass and delete.
 */
class ModulationHubPanel : public juce::Component, public juce::TooltipClient, private juce::Timer {
public:
    ModulationHubPanel(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc),
          theme_(theme),
          // 4 Worship Macros
          macroWarmth_("WARMTH", "", theme, false),
          macroMotion_("MOTION", "", theme, true),
          macroShimmer_("SHIMMER", "", theme, true),
          macroAir_("AIR", "", theme, false) {
        
        auto& apvts = proc_.getAPVTS();

        // 1. Initialize 4 LFO control pairs
        for (int i = 0; i < 4; ++i) {
            const juce::String idxStr = juce::String(i + 1);
            lfoKnobs_[static_cast<std::size_t>(i)].rate =
                std::make_unique<nova::ui::ArcKnob>("RATE", "Hz", theme, false);
            lfoKnobs_[static_cast<std::size_t>(i)].depth =
                std::make_unique<nova::ui::ArcKnob>("DEPTH", "%", theme, (i % 2 == 1));

            bool isSynced = false;
            if (auto* p = apvts.getRawParameterValue("lfo" + idxStr + "Sync")) {
                isSynced = p->load() > 0.5f;
            }
            updateLfoRateKnobAttachment(i, isSynced);

            lfoKnobs_[static_cast<std::size_t>(i)].depth->attachToParameter(
                apvts, "lfo" + idxStr + "Depth");

            addAndMakeVisible(*lfoKnobs_[static_cast<std::size_t>(i)].rate);
            addAndMakeVisible(*lfoKnobs_[static_cast<std::size_t>(i)].depth);
        }

        // 2. Initialize 4 Worship Performance Macros
        macroWarmth_.attachToParameter(apvts, "macroWarmth");
        macroMotion_.attachToParameter(apvts, "macroMotion");
        macroShimmer_.attachToParameter(apvts, "macroShimmer");
        macroAir_.attachToParameter(apvts, "macroAir");

        addAndMakeVisible(macroWarmth_);
        addAndMakeVisible(macroMotion_);
        addAndMakeVisible(macroShimmer_);
        addAndMakeVisible(macroAir_);

        startTimerHz(30);
    }

    ~ModulationHubPanel() override {
        stopTimer();
    }

    void updateLfoRateKnobAttachment(int lfoIdx, bool isSynced) {
        if (lfoIdx < 0 || lfoIdx >= 4) return;
        const juce::String idxStr = juce::String(lfoIdx + 1);
        auto& apvts = proc_.getAPVTS();
        auto& knob = lfoKnobs_[static_cast<std::size_t>(lfoIdx)].rate;
        if (!knob) return;

        if (isSynced) {
            knob->setLabel("SYNC");
            knob->setSuffix("");
            knob->attachToParameter(apvts, "lfo" + idxStr + "SyncDiv");
        } else {
            knob->setLabel("RATE");
            knob->setSuffix("Hz");
            knob->attachToParameter(apvts, "lfo" + idxStr + "RateHz");
        }
    }

    void toggleLfoSync(int lfoIdx) {
        if (lfoIdx < 0 || lfoIdx >= 4) return;
        const juce::String idxStr = juce::String(lfoIdx + 1);
        auto& apvts = proc_.getAPVTS();

        bool curSync = false;
        if (auto* p = apvts.getRawParameterValue("lfo" + idxStr + "Sync")) {
            curSync = p->load() > 0.5f;
        }
        const bool newSync = !curSync;
        if (auto* p = apvts.getParameter("lfo" + idxStr + "Sync")) {
            p->setValueNotifyingHost(newSync ? 1.0f : 0.0f);
        }

        updateLfoRateKnobAttachment(lfoIdx, newSync);
        repaint();
    }

    [[nodiscard]] int getLfoShapeHeight(int index = 0) const noexcept {
        return (index >= 0 && index < 4) ? lfoShapeRects_[static_cast<std::size_t>(index)].getHeight() : 0;
    }
    [[nodiscard]] int getLfoKnobHeight(int index = 0) const noexcept {
        return (index >= 0 && index < 4 && lfoKnobs_[static_cast<std::size_t>(index)].rate)
            ? lfoKnobs_[static_cast<std::size_t>(index)].rate->getHeight() : 0;
    }
    [[nodiscard]] const nova::ui::ArcKnob* getLfoRateKnob(int index = 0) const noexcept {
        return (index >= 0 && index < 4) ? lfoKnobs_[static_cast<std::size_t>(index)].rate.get() : nullptr;
    }
    [[nodiscard]] const nova::ui::ArcKnob* getLfoDepthKnob(int index = 0) const noexcept {
        return (index >= 0 && index < 4) ? lfoKnobs_[static_cast<std::size_t>(index)].depth.get() : nullptr;
    }
    [[nodiscard]] bool isLfoSynced(int index = 0) const noexcept {
        if (index < 0 || index >= 4) return false;
        if (auto* p = proc_.getAPVTS().getRawParameterValue("lfo" + juce::String(index + 1) + "Sync")) {
            return p->load() > 0.5f;
        }
        return false;
    }
    [[nodiscard]] juce::Rectangle<int> getSyncBtnRect(int index = 0) const noexcept {
        return (index >= 0 && index < 4) ? syncBtnRects_[static_cast<std::size_t>(index)] : juce::Rectangle<int>{};
    }

    void timerCallback() override {
        constexpr float dt = 1.0f / 30.0f;
        const auto params = proc_.getCurrentParameters();
        for (int i = 0; i < 4; ++i) {
            float rateHz = 0.5f;
            if (i == 0) rateHz = params.lfo1Sync ? nova::dsp::WorshipSynthEngine::getLfoRateForDivision(params.bpm, params.lfo1SyncDiv) : params.lfo1RateHz;
            else if (i == 1) rateHz = params.lfo2Sync ? nova::dsp::WorshipSynthEngine::getLfoRateForDivision(params.bpm, params.lfo2SyncDiv) : params.lfo2RateHz;
            else if (i == 2) rateHz = params.lfo3Sync ? nova::dsp::WorshipSynthEngine::getLfoRateForDivision(params.bpm, params.lfo3SyncDiv) : params.lfo3RateHz;
            else if (i == 3) rateHz = params.lfo4Sync ? nova::dsp::WorshipSynthEngine::getLfoRateForDivision(params.bpm, params.lfo4SyncDiv) : params.lfo4RateHz;

            lfoPhases_[static_cast<std::size_t>(i)] += rateHz * dt;
            if (lfoPhases_[static_cast<std::size_t>(i)] >= 1.0f) {
                lfoPhases_[static_cast<std::size_t>(i)] -= std::floor(lfoPhases_[static_cast<std::size_t>(i)]);
            }
        }

        // Smoothly decay live performance visualizers
        const auto tel = proc_.getSynthTelemetry();
        whlVisual_ = std::max(tel.modWheelValue, whlVisual_ * 0.94f);
        velVisual_ = std::max(tel.activeVoiceCount > 0 ? tel.lastPlayedVelocity : 0.0f, velVisual_ * 0.92f);
        atVisual_  = std::max(std::max(tel.outputPeakL, tel.outputPeakR), atVisual_ * 0.92f);

        repaint();
    }

    void paint(juce::Graphics& g) override {
        // Draw the 3 modular workbench cards
        drawLfoCard(g, cardLfoRect_.toFloat());
        drawMacrosCard(g, cardMacrosRect_.toFloat());
        drawMatrixCard(g, cardMatrixRect_.toFloat());
    }

    void resized() override {
        const auto area = getLocalBounds();
        const int totalW = area.getWidth();
        const int totalH = area.getHeight();
        constexpr int gap = 8;
        const int availW = totalW - gap * 2;

        // Width split: LFOs (47%), Macros (19%), Quick Mod Matrix (34%)
        const int wLfo = static_cast<int>(static_cast<float>(availW) * 0.47f);
        const int wMac = static_cast<int>(static_cast<float>(availW) * 0.19f);
        const int wMat = availW - wLfo - wMac;

        cardLfoRect_    = juce::Rectangle<int>(area.getX(), area.getY(), wLfo, totalH);
        cardMacrosRect_ = juce::Rectangle<int>(cardLfoRect_.getRight() + gap, area.getY(), wMac, totalH);
        cardMatrixRect_ = juce::Rectangle<int>(cardMacrosRect_.getRight() + gap, area.getY(), wMat, totalH);

        // Layout LFO mini-cards
        layoutLfoControls();

        // Layout Macro Knobs
        layoutMacroControls();

        // Update Matrix rects
        updateMatrixRowRects();
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override {
        if (cardMatrixRect_.contains(event.getPosition())) {
            const auto& mm = proc_.getModMatrix();
            int activeCount = 0;
            for (std::size_t i = 0; i < nova::dsp::ModMatrix::kMaxSlots; ++i) {
                const auto s = mm.getSlot(i);
                if (s.source != nova::dsp::ModMatrix::Source::None && s.destination != nova::dsp::ModMatrix::Destination::None) {
                    ++activeCount;
                }
            }
            constexpr int visibleRows = 4;
            const int maxScroll = std::max(0, activeCount - visibleRows);
            if (wheel.deltaY < 0.0f && matrixScrollOffset_ < maxScroll) {
                ++matrixScrollOffset_;
                repaint();
            } else if (wheel.deltaY > 0.0f && matrixScrollOffset_ > 0) {
                --matrixScrollOffset_;
                repaint();
            }
        }
    }

    juce::String getTooltip() override {
        if (hoveredSyncBtn_ >= 0 && hoveredSyncBtn_ < 4) {
            const bool isSynced = isLfoSynced(hoveredSyncBtn_);
            return isSynced ? "LFO Tempo Sync (Active - Click to switch to Free Hz)"
                            : "LFO Tempo Sync (Inactive - Click to sync to BPM)";
        }
        return {};
    }

    void mouseMove(const juce::MouseEvent& event) override {
        const auto pos = event.getPosition();
        int newHoveredSync = -1;
        for (int i = 0; i < 4; ++i) {
            if (syncBtnRects_[static_cast<std::size_t>(i)].contains(pos)) {
                newHoveredSync = i;
                break;
            }
        }
        if (newHoveredSync != hoveredSyncBtn_) {
            hoveredSyncBtn_ = newHoveredSync;
            setMouseCursor(hoveredSyncBtn_ >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override {
        if (hoveredSyncBtn_ >= 0) {
            hoveredSyncBtn_ = -1;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& event) override {
        const auto pos = event.getPosition();

        // 1. Check LFO Sync buttons
        for (int i = 0; i < 4; ++i) {
            if (syncBtnRects_[static_cast<std::size_t>(i)].contains(pos)) {
                toggleLfoSync(i);
                return;
            }
        }

        // 2. Check LFO shape selectors
        for (int i = 0; i < 4; ++i) {
            if (lfoShapeRects_[static_cast<std::size_t>(i)].contains(pos)) {
                cycleLfoShape(i, pos.x > lfoShapeRects_[static_cast<std::size_t>(i)].getCentreX());
                return;
            }
        }

        // 2. Check Add Route button
        if (addRouteBtnRect_.contains(pos)) {
            addNewMatrixRoute();
            return;
        }

        // 3. Check Matrix Row controls
        const auto& mm = proc_.getModMatrix();
        std::vector<std::size_t> activeSlots;
        for (std::size_t i = 0; i < nova::dsp::ModMatrix::kMaxSlots; ++i) {
            const auto s = mm.getSlot(i);
            if (s.source != nova::dsp::ModMatrix::Source::None && s.destination != nova::dsp::ModMatrix::Destination::None) {
                activeSlots.push_back(i);
            }
        }

        for (int r = 0; r < static_cast<int>(matrixRowRects_.size()); ++r) {
            const int slotIdxIdx = r + matrixScrollOffset_;
            if (slotIdxIdx < 0 || slotIdxIdx >= static_cast<int>(activeSlots.size())) break;

            const std::size_t slotIdx = activeSlots[static_cast<std::size_t>(slotIdxIdx)];
            const auto& row = matrixRowRects_[static_cast<std::size_t>(r)];

            // Source menu
            if (row.srcRect.contains(pos)) {
                showSourceMenu(slotIdx, row.srcRect);
                return;
            }

            // Destination menu
            if (row.dstRect.contains(pos)) {
                showDestinationMenu(slotIdx, row.dstRect);
                return;
            }

            // Bypass toggle
            if (row.bypassRect.contains(pos)) {
                auto& mutableMm = proc_.getModMatrix();
                const auto cur = mutableMm.getSlot(slotIdx);
                mutableMm.setSlot(slotIdx, cur.source, cur.destination, cur.amount, !cur.enabled);
                repaint();
                return;
            }

            // Delete route
            if (row.delRect.contains(pos)) {
                deleteMatrixRoute(slotIdx);
                return;
            }

            // Amount drag start
            if (row.amtRect.contains(pos)) {
                draggingSlotIdx_ = static_cast<int>(slotIdx);
                dragStartAmount_ = mm.getSlot(slotIdx).amount;
                dragStartX_ = static_cast<float>(pos.x);
                dragAmtRect_ = row.amtRect.toFloat();
                if (event.mods.isPopupMenu() || event.getNumberOfClicks() == 2) {
                    // Double click or right click resets to 0%
                    proc_.getModMatrix().setSlot(slotIdx, mm.getSlot(slotIdx).source,
                                                 mm.getSlot(slotIdx).destination, 0.0f,
                                                 mm.getSlot(slotIdx).enabled);
                    draggingSlotIdx_ = -1;
                    repaint();
                }
                return;
            }
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override {
        if (draggingSlotIdx_ >= 0 && draggingSlotIdx_ < static_cast<int>(nova::dsp::ModMatrix::kMaxSlots)) {
            const float dx = static_cast<float>(event.getPosition().x) - dragStartX_;
            const float deltaNorm = dx / (dragAmtRect_.getWidth() * 0.5f);
            const float newAmt = std::clamp(dragStartAmount_ + deltaNorm, -1.0f, 1.0f);

            auto& mm = proc_.getModMatrix();
            const auto cur = mm.getSlot(static_cast<std::size_t>(draggingSlotIdx_));
            mm.setSlot(static_cast<std::size_t>(draggingSlotIdx_), cur.source, cur.destination, newAmt, cur.enabled);
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent& /*event*/) override {
        draggingSlotIdx_ = -1;
    }

private:
    struct LfoKnobPair {
        std::unique_ptr<nova::ui::ArcKnob> rate;
        std::unique_ptr<nova::ui::ArcKnob> depth;
    };

    struct MatrixRowUI {
        juce::Rectangle<int> srcRect;
        juce::Rectangle<int> arrowRect;
        juce::Rectangle<int> dstRect;
        juce::Rectangle<int> amtRect;
        juce::Rectangle<int> bypassRect;
        juce::Rectangle<int> delRect;
    };

    void drawLfoCard(juce::Graphics& g, juce::Rectangle<float> area) {
        // Card background & subtle border
        g.setColour(theme_.bgSurface.darker(0.12f));
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(area, 6.0f, 1.0f);

        // Header Title
        g.setFont(theme_.getSectionFont(12.0f));
        g.setColour(theme_.accentPrimary);
        g.drawText("LFO & ANALOG DRIFT", juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 180.0f, 16.0f),
                   juce::Justification::left);

        // Subtitle badge
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(theme_.textMuted);
        g.drawText("4 DUAL-SHAPE GENERATORS", juce::Rectangle<float>(area.getX() + 160.0f, area.getY() + 7.0f, 180.0f, 14.0f),
                   juce::Justification::left);

        // Draw the 4 mini LFO column separators and waveform selectors
        static constexpr std::array<uint32_t, 4> kLfoColors = {
            0xff2dd4bf, // Cyan (LFO 1)
            0xfff59e0b, // Amber (LFO 2 - Drift)
            0xfff43f5e, // Rose (LFO 3)
            0xffa855f7  // Purple (LFO 4)
        };
        static constexpr std::array<const char*, 4> kLfoNames = {
            "LFO 1", "LFO 2 (DRIFT)", "LFO 3", "LFO 4"
        };

        for (int i = 0; i < 4; ++i) {
            const auto col = juce::Colour(kLfoColors[static_cast<std::size_t>(i)]);
            const auto sRect = lfoShapeRects_[static_cast<std::size_t>(i)].toFloat();

            // Waveform Box Background
            g.setColour(juce::Colour(0xff090d16));
            g.fillRoundedRectangle(sRect, 4.0f);
            g.setColour(col.withAlpha(0.35f));
            g.drawRoundedRectangle(sRect, 4.0f, 1.0f);

            // Fetch Shape from APVTS or Engine
            const juce::String paramId = "lfo" + juce::String(i + 1) + "Shape";
            int shapeInt = 0;
            if (auto* p = proc_.getAPVTS().getRawParameterValue(paramId)) {
                shapeInt = static_cast<int>(p->load());
            }
            const auto shape = static_cast<nova::dsp::LfoGenerator::Shape>(shapeInt);

            // Shape name label at bottom
            const auto labelRect = sRect.withTrimmedTop(sRect.getHeight() - 16.0f);
            g.setFont(theme_.getLabelFont(10.0f));
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.drawText(nova::dsp::LfoGenerator::getShapeName(shape), labelRect, juce::Justification::centred);

            // Wave preview area inside box (leaving room for label at bottom)
            const auto waveArea = sRect.withTrimmedBottom(18.0f).withTrimmedTop(4.0f).reduced(6.0f, 0.0f);
            drawMiniWaveform(g, waveArea, shape, col, i);

            // Top mini title
            g.setFont(theme_.getLabelFont(10.0f));
            g.setColour(col);
            g.drawText(kLfoNames[static_cast<std::size_t>(i)],
                       juce::Rectangle<float>(sRect.getX(), sRect.getY() - 14.0f, sRect.getWidth(), 14.0f),
                       juce::Justification::centred);

            // Arrow hints < and > vertically centered in waveArea
            g.setFont(theme_.getLabelFont(11.0f));
            g.setColour(theme_.textMuted);
            g.drawText("<", juce::Rectangle<float>(sRect.getX() + 3.0f, waveArea.getCentreY() - 8.0f, 10.0f, 16.0f), juce::Justification::centred);
            g.drawText(">", juce::Rectangle<float>(sRect.getRight() - 13.0f, waveArea.getCentreY() - 8.0f, 10.0f, 16.0f), juce::Justification::centred);

            // Check if LFO is synced
            bool isSynced = false;
            const juce::String syncParamId = "lfo" + juce::String(i + 1) + "Sync";
            if (auto* p = proc_.getAPVTS().getRawParameterValue(syncParamId)) {
                isSynced = p->load() > 0.5f;
            }

            // Draw connecting line between Rate knob dial track and Sync button
            const auto btnR = syncBtnRects_[static_cast<std::size_t>(i)];
            const float lineY = static_cast<float>(btnR.getCentreY());

            // Center and outer rim of the Rate rotary dial
            const auto* rateComp = lfoKnobs_[static_cast<std::size_t>(i)].rate.get();
            const float rateDialCentreX = rateComp != nullptr
                ? static_cast<float>(rateComp->getX()) + static_cast<float>(rateComp->getWidth()) * 0.5f
                : static_cast<float>(btnR.getX() - 24);
            const float rateDialH = rateComp != nullptr
                ? static_cast<float>(rateComp->getHeight() - 26)
                : 32.0f;
            const float rateDialW = rateComp != nullptr
                ? static_cast<float>(rateComp->getWidth())
                : 50.0f;
            const float dialRadius = std::min(rateDialW - 4.0f, rateDialH - 4.0f) * 0.5f;
            const float rateOuterRimX = rateDialCentreX + dialRadius * 0.95f;

            const float lineStartX = rateOuterRimX + 1.5f;
            const float lineEndX   = static_cast<float>(btnR.getX()) - 0.5f;

            if (lineEndX > lineStartX) {
                if (isSynced) {
                    // Glowing accent line when Tempo Synced
                    g.setColour(col.withAlpha(0.25f));
                    g.drawLine(lineStartX, lineY, lineEndX, lineY, 3.2f);
                    g.setColour(col.withAlpha(0.95f));
                    g.drawLine(lineStartX, lineY, lineEndX, lineY, 1.5f);

                    // Clean junction tick mark at Rate dial rim
                    g.drawLine(lineStartX, lineY - 2.5f, lineStartX, lineY + 2.5f, 1.5f);
                } else {
                    // Subtle gray line when Inactive / Free run
                    g.setColour(juce::Colour(0xff475569));
                    g.drawLine(lineStartX, lineY, lineEndX, lineY, 1.2f);
                    g.drawLine(lineStartX, lineY - 2.0f, lineStartX, lineY + 2.0f, 1.2f);
                }
            }

            // Draw Sync Button with tactile highlight and hover state
            const bool isHovered = (hoveredSyncBtn_ == i);
            drawSyncButton(g, btnR.toFloat(), isSynced, col, isHovered);
        }
    }

    void drawSyncButton(juce::Graphics& g, juce::Rectangle<float> btnRect, bool isSynced, juce::Colour col, bool isHovered = false) {
        constexpr float radius = 3.5f;

        if (isSynced) {
            // Luminous outer accent glow
            g.setColour(col.withAlpha(0.22f));
            g.fillRoundedRectangle(btnRect.expanded(1.5f), radius + 1.0f);

            // Button body gradient
            const auto grad = juce::ColourGradient::vertical(
                col.withAlpha(0.38f), btnRect.getY(),
                col.withAlpha(0.20f), btnRect.getBottom());
            g.setGradientFill(grad);
            g.fillRoundedRectangle(btnRect, radius);

            // Vibrant border
            g.setColour(col.withAlpha(0.95f));
            g.drawRoundedRectangle(btnRect, radius, 1.3f);
        } else {
            // Tactile hardware button in prominent dark slate
            const auto grad = juce::ColourGradient::vertical(
                isHovered ? juce::Colour(0xff273549) : juce::Colour(0xff1e293b), btnRect.getY(),
                isHovered ? juce::Colour(0xff162030) : juce::Colour(0xff0f172a), btnRect.getBottom());
            g.setGradientFill(grad);
            g.fillRoundedRectangle(btnRect, radius);

            // Crisp defined border
            g.setColour(isHovered ? theme_.borderFocus : juce::Colour(0xff3b4f68));
            g.drawRoundedRectangle(btnRect, radius, 1.2f);
        }

        // Crisp Chain Link Icon (white when synced, bright silver/white when free)
        const auto iconCol = isSynced
            ? juce::Colours::white
            : (isHovered ? juce::Colour(0xfff1f5f9) : juce::Colour(0xffcbd5e1));
        drawSyncIcon(g, btnRect.reduced(2.5f), iconCol);
    }

    static void drawSyncIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour col) {
        const float s = std::min(area.getWidth(), area.getHeight());
        if (s <= 1.0f) return;

        const float cx = area.getCentreX();
        const float cy = area.getCentreY();

        // Exact vector geometry matching the chain-link icon (angled 45 degrees)
        const float loopCenterX = 0.26f * s;
        const float loopRadius  = 0.16f * s;
        const float legLen      = 0.12f * s;
        const float loopStroke  = std::max(1.3f, 0.13f * s);
        const float barHalfLen  = 0.19f * s;
        const float barStroke   = std::max(1.3f, 0.12f * s);

        const auto strokeType = juce::PathStrokeType(loopStroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        const auto barStrokeType = juce::PathStrokeType(barStroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        // Transformation: rotate -45 degrees (-pi/4) so chain links orient diagonally (bottom-left to top-right)
        const auto xform = juce::AffineTransform::rotation(-juce::MathConstants<float>::pi * 0.25f)
                                                 .translated(cx, cy);

        // 1. Center link bar
        juce::Path bar;
        bar.startNewSubPath(-barHalfLen, 0.0f);
        bar.lineTo(barHalfLen, 0.0f);
        bar.applyTransform(xform);

        // 2. Right loop (top-right when rotated)
        // Curves around through x = loopCenterX + loopRadius, ends on upper leg
        juce::Path rightLoop;
        rightLoop.startNewSubPath(loopCenterX - legLen, loopRadius);
        rightLoop.lineTo(loopCenterX, loopRadius);
        rightLoop.addCentredArc(loopCenterX, 0.0f, loopRadius, loopRadius,
                                0.0f,
                                juce::MathConstants<float>::pi,
                                0.0f,
                                false);
        rightLoop.lineTo(loopCenterX - legLen, -loopRadius);
        rightLoop.applyTransform(xform);

        // 3. Left loop (bottom-left when rotated)
        // Curves around through x = -loopCenterX - loopRadius, ends on lower leg
        juce::Path leftLoop;
        leftLoop.startNewSubPath(-loopCenterX + legLen, -loopRadius);
        leftLoop.lineTo(-loopCenterX, -loopRadius);
        leftLoop.addCentredArc(-loopCenterX, 0.0f, loopRadius, loopRadius,
                               0.0f,
                               0.0f,
                               -juce::MathConstants<float>::pi,
                               false);
        leftLoop.lineTo(-loopCenterX + legLen, loopRadius);
        leftLoop.applyTransform(xform);

        g.setColour(col);
        g.strokePath(bar, barStrokeType);
        g.strokePath(rightLoop, strokeType);
        g.strokePath(leftLoop, strokeType);
    }

    void drawMiniWaveform(juce::Graphics& g, juce::Rectangle<float> area,
                          nova::dsp::LfoGenerator::Shape shape, juce::Colour col, int lfoIdx) {
        const float midY = area.getCentreY();

        // Subtle zero reference line
        g.setColour(theme_.borderSubtle.withAlpha(0.25f));
        g.drawHorizontalLine(static_cast<int>(std::round(midY)), area.getX(), area.getRight());

        // Dynamic Depth scaling: amplitude grows or shrinks with Depth knob!
        float depth = 0.5f;
        const juce::String depthParam = "lfo" + juce::String(lfoIdx + 1) + "Depth";
        if (auto* pDepth = proc_.getAPVTS().getRawParameterValue(depthParam)) {
            depth = pDepth->load();
        }
        // At Depth 0%, wave is a subtle flat line; at Depth 100%, wave reaches full height
        const float h = area.getHeight() * 0.42f * (0.08f + 0.92f * std::clamp(depth, 0.0f, 1.0f));
        const int steps = 40;

        auto evalShape = [shape, midY, h](float t) noexcept -> float {
            switch (shape) {
                case nova::dsp::LfoGenerator::Shape::Sine:
                    return midY - std::sin(t * 6.283185f) * h;
                case nova::dsp::LfoGenerator::Shape::Triangle:
                    return midY - (1.0f - 4.0f * std::abs(t - 0.5f)) * h;
                case nova::dsp::LfoGenerator::Shape::SawUp:
                    return midY - (2.0f * t - 1.0f) * h;
                case nova::dsp::LfoGenerator::Shape::SmoothDrift: {
                    const float s1 = std::sin(t * 6.283f);
                    const float s2 = std::sin(t * 12.566f + 1.2f) * 0.4f;
                    return midY - (s1 + s2) * 0.7f * h;
                }
                case nova::dsp::LfoGenerator::Shape::PulseGate:
                    return midY - ((t < 0.5f) ? 1.0f : -1.0f) * h;
            }
            return midY;
        };

        juce::Path p;
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const float x = area.getX() + t * area.getWidth();
            const float y = evalShape(t);

            if (i == 0) p.startNewSubPath(x, y);
            else p.lineTo(x, y);
        }

        g.setColour(col.withAlpha(0.85f));
        g.strokePath(p, juce::PathStrokeType(1.5f));

        // Animated glowing dot tracking wave phase at the exact speed of the LFO's Rate!
        const float cyclePhase = lfoPhases_[static_cast<std::size_t>(std::clamp(lfoIdx, 0, 3))];
        const float dotX = area.getX() + cyclePhase * area.getWidth();
        const float dotY = evalShape(cyclePhase);

        g.setColour(col.withAlpha(0.40f));
        g.fillEllipse(dotX - 5.0f, dotY - 5.0f, 10.0f, 10.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(dotX - 2.5f, dotY - 2.5f, 5.0f, 5.0f);
    }

    void drawMacrosCard(juce::Graphics& g, juce::Rectangle<float> area) {
        g.setColour(theme_.bgSurface.darker(0.12f));
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(area, 6.0f, 1.0f);

        // Header Title
        g.setFont(theme_.getSectionFont(12.0f));
        g.setColour(juce::Colour(0xfff59e0b)); // Warm Amber
        g.drawText("WORSHIP MACROS", juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 140.0f, 16.0f),
                   juce::Justification::left);

        // Subtitle badge
        g.setFont(theme_.getLabelFont(10.0f));
        g.setColour(theme_.textMuted);
        g.drawText("PERFORMANCE", juce::Rectangle<float>(area.getRight() - 90.0f, area.getY() + 7.0f, 80.0f, 14.0f),
                   juce::Justification::right);

        // Draw Performance Meters strip at the bottom
        const auto barArea = area.withTrimmedTop(area.getHeight() - 24.0f).reduced(10.0f, 2.0f);
        const float itemW = (barArea.getWidth() - 15.0f) / 4.0f;

        static constexpr std::array<const char*, 4> kPerfLabels = { "WHL", "VEL", "A.T.", "PBD" };
        static constexpr std::array<uint32_t, 4> kPerfCols = { 0xff3b82f6, 0xff2dd4bf, 0xfff43f5e, 0xffa855f7 };
        const std::array<float, 4> kPerfVals = { whlVisual_, velVisual_, atVisual_, 0.5f };

        for (int i = 0; i < 4; ++i) {
            const juce::Rectangle<float> itemRect(
                barArea.getX() + static_cast<float>(i) * (itemW + 5.0f),
                barArea.getY(), itemW, barArea.getHeight());

            g.setFont(theme_.getLabelFont(9.0f));
            g.setColour(theme_.textMuted);
            g.drawText(kPerfLabels[static_cast<std::size_t>(i)],
                       juce::Rectangle<float>(itemRect.getX(), itemRect.getY(), itemRect.getWidth(), 9.0f),
                       juce::Justification::centred);

            const auto meterRect = itemRect.withTrimmedTop(10.0f).reduced(1.0f, 1.0f);
            g.setColour(juce::Colour(0xff090d16));
            g.fillRoundedRectangle(meterRect, 2.0f);

            const float val = std::clamp(kPerfVals[static_cast<std::size_t>(i)], 0.0f, 1.0f);
            if (val > 0.02f) {
                const auto fillRect = meterRect.withWidth(meterRect.getWidth() * val);
                g.setColour(juce::Colour(kPerfCols[static_cast<std::size_t>(i)]).withAlpha(0.75f));
                g.fillRoundedRectangle(fillRect, 2.0f);
            }
        }
    }

    void drawMatrixCard(juce::Graphics& g, juce::Rectangle<float> area) {
        g.setColour(theme_.bgSurface.darker(0.12f));
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(area, 6.0f, 1.0f);

        // Header Title
        g.setFont(theme_.getSectionFont(12.0f));
        g.setColour(theme_.accentPrimary);
        g.drawText("MODULATION MATRIX", juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 150.0f, 16.0f),
                   juce::Justification::left);

        // Active slots count badge
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
            g.drawText("#" + juce::String(static_cast<int>(slotIdx) + 1),
                       row.srcRect.getX() - 26, row.srcRect.getY(), 22, row.srcRect.getHeight(),
                       juce::Justification::centredRight);

            // Source Selector Button
            const auto srcF = row.srcRect.toFloat();
            g.setColour(juce::Colour(0xff090d16).withAlpha(alpha));
            g.fillRoundedRectangle(srcF, 3.0f);
            g.setColour(theme_.borderSubtle.withAlpha(alpha));
            g.drawRoundedRectangle(srcF, 3.0f, 1.0f);
            g.setFont(theme_.getLabelFont(10.0f));
            g.setColour(theme_.textPrimary.withAlpha(alpha));
            g.drawText(nova::dsp::ModMatrix::getSourceName(slot.source), srcF.reduced(4.0f, 0.0f),
                       juce::Justification::left);
            drawChevron(srcF.getRight(), srcF.getY(), srcF.getHeight(), theme_.accentPrimary, alpha);

            // Arrow
            drawArrow(row.arrowRect, theme_.textMuted, alpha);

            // Destination Selector Button
            const auto dstF = row.dstRect.toFloat();
            g.setColour(juce::Colour(0xff090d16).withAlpha(alpha));
            g.fillRoundedRectangle(dstF, 3.0f);
            g.setColour(theme_.borderSubtle.withAlpha(alpha));
            g.drawRoundedRectangle(dstF, 3.0f, 1.0f);
            g.setFont(theme_.getLabelFont(10.0f));
            g.setColour(theme_.textPrimary.withAlpha(alpha));
            g.drawText(nova::dsp::ModMatrix::getDestinationName(slot.destination), dstF.reduced(4.0f, 0.0f),
                       juce::Justification::left);
            drawChevron(dstF.getRight(), dstF.getY(), dstF.getHeight(), theme_.accentPrimary, alpha);

            // Bipolar Amount Slider Bar
            const auto amtF = row.amtRect.toFloat();
            g.setColour(juce::Colour(0xff090d16));
            g.fillRoundedRectangle(amtF, 3.0f);
            g.setColour(theme_.borderSubtle.withAlpha(alpha));
            g.drawRoundedRectangle(amtF, 3.0f, 1.0f);

            // Center Notch at 0%
            const float midX = amtF.getCentreX();
            g.setColour(theme_.textMuted.withAlpha(0.4f));
            g.drawVerticalLine(static_cast<int>(midX), amtF.getY() + 1.0f, amtF.getBottom() - 1.0f);

            // Amount bar fill (-100%..+100%)
            const float amtVal = std::clamp(slot.amount, -1.0f, 1.0f);
            if (!isBypassed && std::abs(amtVal) > 0.005f) {
                const float barW = (amtF.getWidth() * 0.5f - 2.0f) * std::abs(amtVal);
                juce::Rectangle<float> fillBar;
                if (amtVal > 0.0f) {
                    fillBar = juce::Rectangle<float>(midX, amtF.getY() + 2.0f, barW, amtF.getHeight() - 4.0f);
                    g.setColour(juce::Colour(0xff2dd4bf).withAlpha(0.80f));
                } else {
                    fillBar = juce::Rectangle<float>(midX - barW, amtF.getY() + 2.0f, barW, amtF.getHeight() - 4.0f);
                    g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.80f));
                }
                g.fillRoundedRectangle(fillBar, 2.0f);
            }

            // Numeric readout (+45%, -20%)
            g.setFont(theme_.getLabelFont(9.0f));
            g.setColour(isBypassed ? theme_.textMuted : juce::Colours::white);
            const int pct = static_cast<int>(std::round(amtVal * 100.0f));
            const juce::String pctStr = (pct > 0 ? "+" : "") + juce::String(pct) + "%";
            g.drawText(pctStr, amtF, juce::Justification::centred);

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
            g.drawText("×", delF, juce::Justification::centred);
        }
    }

    void layoutLfoControls() {
        const auto area = cardLfoRect_.withTrimmedTop(24).reduced(8, 6);
        const int colW = (area.getWidth() - 15) / 4;

        // Size knobs identically to Filter Deck (~56-62px height) and place at bottom
        const int knobH = std::clamp(static_cast<int>(static_cast<float>(area.getHeight()) * 0.40f), 54, 62);
        const int knobY = area.getBottom() - knobH;

        for (int i = 0; i < 4; ++i) {
            const int colX = area.getX() + i * (colW + 5);

            // Shape box gets all remaining vertical height above the knobs
            const int shapeTop = area.getY() + 15;
            const int sH = std::max(40, knobY - shapeTop - 6);
            lfoShapeRects_[static_cast<std::size_t>(i)] = juce::Rectangle<int>(colX, shapeTop, colW, sH);

            // Rate and Depth knobs side-by-side with 20px gap for Sync button
            constexpr int gapBetweenKnobs = 20;
            const int knobW = (colW - gapBetweenKnobs) / 2;
            const int rateX = colX;
            const int depthX = colX + knobW + gapBetweenKnobs;

            if (lfoKnobs_[static_cast<std::size_t>(i)].rate) {
                lfoKnobs_[static_cast<std::size_t>(i)].rate->setBounds(rateX, knobY, knobW, knobH);
            }
            if (lfoKnobs_[static_cast<std::size_t>(i)].depth) {
                lfoKnobs_[static_cast<std::size_t>(i)].depth->setBounds(depthX, knobY, knobW, knobH);
            }

            // Exact vertical center of the rotary dial circle of the knobs:
            // ArcKnob: topLabel = 12px, textBoxBelow = 14px, rotary dial height = knobH - 26
            const float dialCenterY = static_cast<float>(knobY) + 12.0f + static_cast<float>(knobH - 26) * 0.5f;

            // Sync Button centered in the gap, and vertically centered to match the knob dial center exactly
            constexpr int btnSize = 16;
            const int btnX = rateX + knobW + (gapBetweenKnobs - btnSize) / 2;
            const int btnY = static_cast<int>(std::round(dialCenterY - static_cast<float>(btnSize) * 0.5f));
            syncBtnRects_[static_cast<std::size_t>(i)] = juce::Rectangle<int>(btnX, btnY, btnSize, btnSize);
        }
    }

    void layoutMacroControls() {
        const auto area = cardMacrosRect_.withTrimmedTop(24).withTrimmedBottom(26).reduced(8, 4);
        const int halfW = (area.getWidth() - 6) / 2;
        const int halfH = (area.getHeight() - 6) / 2;

        macroWarmth_.setBounds(area.getX(), area.getY(), halfW, halfH);
        macroMotion_.setBounds(area.getX() + halfW + 6, area.getY(), halfW, halfH);
        macroShimmer_.setBounds(area.getX(), area.getY() + halfH + 6, halfW, halfH);
        macroAir_.setBounds(area.getX() + halfW + 6, area.getY() + halfH + 6, halfW, halfH);
    }

    void updateMatrixRowRects() {
        const auto area = cardMatrixRect_.withTrimmedTop(30).reduced(8, 4);
        addRouteBtnRect_ = juce::Rectangle<int>(cardMatrixRect_.getRight() - 95, cardMatrixRect_.getY() + 5, 85, 20);

        matrixRowRects_.clear();
        constexpr int rowH = 25;
        constexpr int rowGap = 4;
        const int numRows = std::min(5, area.getHeight() / (rowH + rowGap));

        for (int r = 0; r < numRows; ++r) {
            const int rowY = area.getY() + r * (rowH + rowGap);
            MatrixRowUI row;

            // Allocation: [Slot# 26px] [Src 82px] [-> 14px] [Dst 90px] [Amt 95px] [Bypass 18px] [Del 18px]
            int curX = area.getX() + 28;
            row.srcRect    = juce::Rectangle<int>(curX, rowY, 82, rowH); curX += 84;
            row.arrowRect  = juce::Rectangle<int>(curX, rowY, 14, rowH); curX += 16;
            row.dstRect    = juce::Rectangle<int>(curX, rowY, 90, rowH); curX += 92;
            row.amtRect    = juce::Rectangle<int>(curX, rowY, 96, rowH); curX += 100;
            row.bypassRect = juce::Rectangle<int>(curX, rowY, 18, rowH); curX += 20;
            row.delRect    = juce::Rectangle<int>(curX, rowY, 18, rowH);

            matrixRowRects_.push_back(row);
        }
    }

    void cycleLfoShape(int lfoIdx, bool forward) {
        const juce::String paramId = "lfo" + juce::String(lfoIdx + 1) + "Shape";
        if (auto* p = proc_.getAPVTS().getParameter(paramId)) {
            constexpr int numShapes = 5;
            const int cur = static_cast<int>(p->convertFrom0to1(p->getValue()));
            const int next = forward ? ((cur + 1) % numShapes) : ((cur + numShapes - 1) % numShapes);
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(next)));
            repaint();
        }
    }

    void addNewMatrixRoute() {
        auto& mm = proc_.getModMatrix();
        for (std::size_t i = 0; i < nova::dsp::ModMatrix::kMaxSlots; ++i) {
            if (!mm.getSlot(i).enabled && mm.getSlot(i).source == nova::dsp::ModMatrix::Source::None) {
                // Default: LFO 1 -> Filter Cutoff at +40%
                mm.setSlot(i, nova::dsp::ModMatrix::Source::Lfo1,
                           nova::dsp::ModMatrix::Destination::FilterCutoff, 0.40f, true);
                repaint();
                return;
            }
        }
    }

    void deleteMatrixRoute(std::size_t slotIdx) {
        if (slotIdx < nova::dsp::ModMatrix::kMaxSlots) {
            proc_.getModMatrix().setSlot(slotIdx, nova::dsp::ModMatrix::Source::None,
                                         nova::dsp::ModMatrix::Destination::None, 0.0f, false);
            repaint();
        }
    }

    void showSourceMenu(std::size_t slotIdx, juce::Rectangle<int> targetRect) {
        juce::PopupMenu m;
        m.addItem(1,  "LFO 1");
        m.addItem(2,  "LFO 2 (Drift)");
        m.addItem(3,  "LFO 3");
        m.addItem(4,  "LFO 4");
        m.addSeparator();
        m.addItem(5,  "Amp Envelope");
        m.addItem(6,  "Filter Envelope");
        m.addItem(7,  "Mod Envelope 3");
        m.addItem(8,  "Aux Envelope 4");
        m.addSeparator();
        m.addItem(9,  "Velocity");
        m.addItem(10, "Mod Wheel (CC1)");
        m.addSeparator();
        m.addItem(11, "Macro Warmth");
        m.addItem(12, "Macro Motion");
        m.addItem(13, "Macro Shimmer");
        m.addItem(14, "Macro Air");

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

    void showDestinationMenu(std::size_t slotIdx, juce::Rectangle<int> targetRect) {
        juce::PopupMenu m;

        juce::PopupMenu fltMenu;
        fltMenu.addItem(1, "Filter 1 Cutoff");
        fltMenu.addItem(2, "Filter 1 Resonance");
        fltMenu.addItem(3, "Filter 2 Cutoff");
        fltMenu.addItem(4, "Filter 2 Resonance");
        fltMenu.addItem(5, "Vocal Carve (Mid Scoop)");
        m.addSubMenu("Filters", fltMenu);

        juce::PopupMenu levelMenu;
        levelMenu.addItem(10, "OSC 1 Level (Swell/Vol)");
        levelMenu.addItem(11, "OSC 2 Level (Swell/Vol)");
        levelMenu.addItem(12, "OSC 3 Level (Swell/Pad)");
        levelMenu.addItem(13, "OSC 4 Level (Swell/Sub)");
        m.addSubMenu("OSC Volume / Swell", levelMenu);

        juce::PopupMenu shapeMenu;
        shapeMenu.addItem(20, "OSC 1 Shape / Morph");
        shapeMenu.addItem(21, "OSC 2 Shape / Morph");
        shapeMenu.addItem(22, "OSC 3 Shape / Morph");
        shapeMenu.addItem(23, "OSC 4 Shape / Morph");
        m.addSubMenu("OSC Shape / Morph", shapeMenu);

        juce::PopupMenu pitchMenu;
        pitchMenu.addItem(30, "OSC 1 Pitch");
        pitchMenu.addItem(31, "OSC 2 Pitch");
        pitchMenu.addItem(32, "OSC 3 Pitch");
        pitchMenu.addItem(33, "OSC 4 Pitch");
        pitchMenu.addItem(34, "Pitch All (Master Vibrato)");
        m.addSubMenu("OSC Pitch", pitchMenu);

        juce::PopupMenu fxMenu;
        fxMenu.addItem(40, "Reverb Cloud Mix");
        fxMenu.addItem(41, "Shimmer Amount");
        fxMenu.addItem(42, "Delay Mix");
        fxMenu.addItem(43, "Delay Feedback");
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

                    case 20: dst = nova::dsp::ModMatrix::Destination::Osc1Shape; break;
                    case 21: dst = nova::dsp::ModMatrix::Destination::Osc2Shape; break;
                    case 22: dst = nova::dsp::ModMatrix::Destination::Osc3Shape; break;
                    case 23: dst = nova::dsp::ModMatrix::Destination::Osc4Shape; break;

                    case 30: dst = nova::dsp::ModMatrix::Destination::Osc1Pitch; break;
                    case 31: dst = nova::dsp::ModMatrix::Destination::Osc2Pitch; break;
                    case 32: dst = nova::dsp::ModMatrix::Destination::Osc3Pitch; break;
                    case 33: dst = nova::dsp::ModMatrix::Destination::Osc4Pitch; break;
                    case 34: dst = nova::dsp::ModMatrix::Destination::OscPitchAll; break;

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

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;

    // Cards Rectangles
    juce::Rectangle<int> cardLfoRect_;
    juce::Rectangle<int> cardMacrosRect_;
    juce::Rectangle<int> cardMatrixRect_;

    // 1. LFO Controls
    std::array<LfoKnobPair, 4> lfoKnobs_;
    std::array<juce::Rectangle<int>, 4> lfoShapeRects_;
    std::array<juce::Rectangle<int>, 4> syncBtnRects_;
    int hoveredSyncBtn_ = -1;

    // 2. Worship Macros
    nova::ui::ArcKnob macroWarmth_;
    nova::ui::ArcKnob macroMotion_;
    nova::ui::ArcKnob macroShimmer_;
    nova::ui::ArcKnob macroAir_;

    // Performance Meters
    float whlVisual_ = 0.0f;
    float velVisual_ = 0.0f;
    float atVisual_  = 0.0f;

    // 3. Quick Mod Matrix
    juce::Rectangle<int> addRouteBtnRect_;
    std::vector<MatrixRowUI> matrixRowRects_;
    int matrixScrollOffset_ = 0;

    int draggingSlotIdx_ = -1;
    float dragStartAmount_ = 0.0f;
    float dragStartX_ = 0.0f;
    juce::Rectangle<float> dragAmtRect_;

    std::array<float, 4> lfoPhases_{ 0.0f, 0.25f, 0.50f, 0.75f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationHubPanel)
};

} // namespace nova::synth::ui
