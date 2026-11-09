#include "ModulationHubPanel.h"
#include <algorithm>
#include <cmath>

namespace nova::synth::ui {

ModulationHubPanel::ModulationHubPanel(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc),
      theme_(theme),
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

        lfoKnobs_[static_cast<std::size_t>(i)].depth->attachToParameter(apvts, "lfo" + idxStr + "Depth");
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

ModulationHubPanel::~ModulationHubPanel() {
    stopTimer();
}

void ModulationHubPanel::updateLfoRateKnobAttachment(int lfoIdx, bool isSynced) {
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

void ModulationHubPanel::toggleLfoSync(int lfoIdx) {
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

bool ModulationHubPanel::isLfoSynced(int index) const noexcept {
    if (index < 0 || index >= 4) return false;
    if (auto* p = proc_.getAPVTS().getRawParameterValue("lfo" + juce::String(index + 1) + "Sync")) {
        return p->load() > 0.5f;
    }
    return false;
}

void ModulationHubPanel::timerCallback() {
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

    const auto tel = proc_.getSynthTelemetry();
    whlVisual_ = tel.modWheelValue;
    pbdVisual_ = std::clamp((tel.pitchBendValue / 2.0f) * 0.5f + 0.5f, 0.0f, 1.0f);
    velVisual_ = std::max(tel.activeVoiceCount > 0 ? tel.lastPlayedVelocity : 0.0f, velVisual_ * 0.92f);
    atVisual_  = std::max(std::max(tel.outputPeakL, tel.outputPeakR), atVisual_ * 0.92f);
    repaint();
}

void ModulationHubPanel::resized() {
    const auto area = getLocalBounds();
    const int totalW = area.getWidth();
    const int totalH = area.getHeight();
    constexpr int gap = 8;
    const int availW = totalW - gap * 2;

    const int wLfo = static_cast<int>(static_cast<float>(availW) * 0.47f);
    const int wMac = static_cast<int>(static_cast<float>(availW) * 0.19f);
    const int wMat = availW - wLfo - wMac;

    cardLfoRect_    = juce::Rectangle<int>(area.getX(), area.getY(), wLfo, totalH);
    cardMacrosRect_ = juce::Rectangle<int>(cardLfoRect_.getRight() + gap, area.getY(), wMac, totalH);
    cardMatrixRect_ = juce::Rectangle<int>(cardMacrosRect_.getRight() + gap, area.getY(), wMat, totalH);

    layoutLfoControls();
    layoutMacroControls();
    updateMatrixRowRects();
}

void ModulationHubPanel::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) {
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

juce::String ModulationHubPanel::getTooltip() {
    if (hoveredSyncBtn_ >= 0 && hoveredSyncBtn_ < 4) {
        const bool isSynced = isLfoSynced(hoveredSyncBtn_);
        return isSynced ? "LFO Tempo Sync (Active - Click to switch to Free Hz)"
                        : "LFO Tempo Sync (Inactive - Click to sync to BPM)";
    }
    return {};
}

void ModulationHubPanel::mouseMove(const juce::MouseEvent& event) {
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

void ModulationHubPanel::mouseExit(const juce::MouseEvent&) {
    if (hoveredSyncBtn_ >= 0) {
        hoveredSyncBtn_ = -1;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ModulationHubPanel::mouseDown(const juce::MouseEvent& event) {
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

    // 3. Check Add Route button
    if (addRouteBtnRect_.contains(pos)) {
        addNewMatrixRoute();
        return;
    }

    // 4. Check Matrix Row controls
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

        if (row.srcRect.contains(pos)) {
            showSourceMenu(slotIdx, row.srcRect);
            return;
        }
        if (row.dstRect.contains(pos)) {
            showDestinationMenu(slotIdx, row.dstRect);
            return;
        }
        if (row.bypassRect.contains(pos)) {
            auto& mutableMm = proc_.getModMatrix();
            const auto cur = mutableMm.getSlot(slotIdx);
            mutableMm.setSlot(slotIdx, cur.source, cur.destination, cur.amount, !cur.enabled);
            repaint();
            return;
        }
        if (row.delRect.contains(pos)) {
            deleteMatrixRoute(slotIdx);
            return;
        }
        if (row.amtRect.contains(pos)) {
            draggingSlotIdx_ = static_cast<int>(slotIdx);
            dragStartAmount_ = mm.getSlot(slotIdx).amount;
            dragStartX_ = static_cast<float>(pos.x);
            dragAmtRect_ = row.amtRect.toFloat();
            if (event.mods.isPopupMenu() || event.getNumberOfClicks() == 2) {
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

void ModulationHubPanel::mouseDrag(const juce::MouseEvent& event) {
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

void ModulationHubPanel::mouseUp(const juce::MouseEvent& /*event*/) {
    draggingSlotIdx_ = -1;
}

void ModulationHubPanel::layoutLfoControls() {
    const auto area = cardLfoRect_.withTrimmedTop(24).reduced(8, 6);
    const int colW = (area.getWidth() - 15) / 4;
    const int knobH = std::clamp(static_cast<int>(static_cast<float>(area.getHeight()) * 0.40f), 54, 62);
    const int knobY = area.getBottom() - knobH;

    for (int i = 0; i < 4; ++i) {
        const int colX = area.getX() + i * (colW + 5);
        const int shapeTop = area.getY() + 15;
        const int sH = std::max(40, knobY - shapeTop - 6);
        lfoShapeRects_[static_cast<std::size_t>(i)] = juce::Rectangle<int>(colX, shapeTop, colW, sH);

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

        const float dialCenterY = static_cast<float>(knobY) + 12.0f + static_cast<float>(knobH - 26) * 0.5f;
        constexpr int btnSize = 16;
        const int btnX = rateX + knobW + (gapBetweenKnobs - btnSize) / 2;
        const int btnY = static_cast<int>(std::round(dialCenterY - static_cast<float>(btnSize) * 0.5f));
        syncBtnRects_[static_cast<std::size_t>(i)] = juce::Rectangle<int>(btnX, btnY, btnSize, btnSize);
    }
}

void ModulationHubPanel::layoutMacroControls() {
    const auto area = cardMacrosRect_.withTrimmedTop(24).withTrimmedBottom(26).reduced(8, 4);
    const int halfW = (area.getWidth() - 6) / 2;
    const int halfH = (area.getHeight() - 6) / 2;
    macroWarmth_.setBounds(area.getX(), area.getY(), halfW, halfH);
    macroMotion_.setBounds(area.getX() + halfW + 6, area.getY(), halfW, halfH);
    macroShimmer_.setBounds(area.getX(), area.getY() + halfH + 6, halfW, halfH);
    macroAir_.setBounds(area.getX() + halfW + 6, area.getY() + halfH + 6, halfW, halfH);
}

void ModulationHubPanel::updateMatrixRowRects() {
    const auto area = cardMatrixRect_.withTrimmedTop(30).reduced(8, 4);
    addRouteBtnRect_ = juce::Rectangle<int>(cardMatrixRect_.getRight() - 95, cardMatrixRect_.getY() + 5, 85, 20);

    matrixRowRects_.clear();
    constexpr int rowH = 25;
    constexpr int rowGap = 4;
    const int numRows = std::min(5, area.getHeight() / (rowH + rowGap));

    for (int r = 0; r < numRows; ++r) {
        const int rowY = area.getY() + r * (rowH + rowGap);
        MatrixRowUI row;

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

} // namespace nova::synth::ui

