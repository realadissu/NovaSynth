#include "MixerSlideOverDrawer.h"
#include <algorithm>
#include <cmath>

namespace nova::synth::ui {

void MixerSlideOverDrawer::initMasterAndEffectsControls() {
    auto& apvts = proc_.getAPVTS();

    masterFader_.setLookAndFeel(&faderLookAndFeel_);
    masterFader_.setSliderStyle(juce::Slider::LinearVertical);
    masterFader_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 16);
    masterFader_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xfff59e0b).withAlpha(0.40f));
    masterFader_.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff060a12));
    masterFader_.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfffbbf24));
    masterFader_.setColour(juce::Slider::thumbColourId, juce::Colour(0xfff59e0b));

    masterFaderAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "masterGainDb", masterFader_);

    masterFader_.textFromValueFunction = [](double val) {
        if (val <= -23.5) return juce::String("-inf");
        return (val > 0.05 ? "+" : "") + juce::String(val, 1) + " dB";
    };
    masterFader_.updateText();
    addAndMakeVisible(masterFader_);

    delayReturn_.attachToParameter(apvts, "delayMix");
    reverbReturn_.attachToParameter(apvts, "reverbMix");
    addAndMakeVisible(delayReturn_);
    addAndMakeVisible(reverbReturn_);

    closeBtn_.onClick = [this]() { setDrawerOpen(false); };
    addAndMakeVisible(closeBtn_);

    clearSolosBtn_.setButtonText("SOLO RESET");
    clearSolosBtn_.onClick = [this]() {
        proc_.clearAllSolos();
        updateDimming();
        repaint();
    };
    addAndMakeVisible(clearSolosBtn_);

    updateDimming();
}

MixerSlideOverDrawer::~MixerSlideOverDrawer() {
    stopTimer();
    for (auto& strip : strips_) strip.fader.setLookAndFeel(nullptr);
    masterFader_.setLookAndFeel(nullptr);
}

void MixerSlideOverDrawer::setDrawerOpen(bool open) {
    if (isOpen_ == open) return;
    isOpen_ = open;
    if (isOpen_) {
        grabKeyboardFocus();
        updateDimming();
    }
    startTimerHz(60);
}

void MixerSlideOverDrawer::toggleDrawer() {
    setDrawerOpen(!isOpen_);
}

bool MixerSlideOverDrawer::hitTest(int x, int y) {
    if (y < topY_ || y > bottomY_) return false;
    if (isOpen_ || openProgress_ > 0.001f) return true;
    return handleRect_.expanded(0, 18).contains(x, y);
}

void MixerSlideOverDrawer::resized() {
    const int totalW = getWidth();
    const int totalH = getHeight();
    if (totalW <= 0 || totalH <= 0) return;

    topY_ = 58;
    bottomY_ = std::max(topY_ + 380, totalH - 74);
    const int mixerH = bottomY_ - topY_;

    const int drawerW = std::clamp(static_cast<int>(static_cast<float>(totalW) * 0.70f), 680, 980);
    const int closedX = totalW;
    const int openedX = totalW - drawerW;
    const int currentX = static_cast<int>(std::round(static_cast<float>(closedX) - static_cast<float>(closedX - openedX) * openProgress_));

    drawerRect_ = juce::Rectangle<int>(currentX, topY_, totalW - currentX, mixerH);

    constexpr int hW = 28;
    constexpr int hH = 98;
    const int hY = topY_ + (mixerH - hH) / 2;
    handleRect_ = juce::Rectangle<int>(drawerRect_.getX() - hW, hY, hW, hH);

    auto dArea = drawerRect_.reduced(10, 8);
    auto headerArea = dArea.removeFromTop(36);
    closeBtn_.setBounds(headerArea.removeFromRight(26).withSizeKeepingCentre(22, 22));
    headerArea.removeFromRight(8);

    const bool hasSolo = proc_.hasAnySolo();
    clearSolosBtn_.setVisible(hasSolo);
    if (hasSolo) clearSolosBtn_.setBounds(headerArea.removeFromRight(88).reduced(0, 4));

    dArea.removeFromTop(6);

    constexpr int gap = 8;
    const int availW = dArea.getWidth() - gap;
    const int wChans = static_cast<int>(static_cast<float>(availW) * 0.81f);

    channelsCardRect_ = dArea.removeFromLeft(wChans);
    dArea.removeFromLeft(gap);
    masterCardRect_ = dArea;

    const int colW = (channelsCardRect_.getWidth() - 7 * 4) / 8;
    const int colH = channelsCardRect_.getHeight();

    for (int i = 0; i < 8; ++i) {
        const int colX = channelsCardRect_.getX() + i * (colW + 4);
        auto cCol = juce::Rectangle<int>(colX, channelsCardRect_.getY(), colW, colH);
        auto& strip = strips_[static_cast<std::size_t>(i)];

        cCol.removeFromTop(38);
        cCol.removeFromTop(6);
        if (strip.octStepper) strip.octStepper->setBounds(cCol.removeFromTop(24).reduced(2, 0));
        cCol.removeFromTop(6);
        strip.flt1Btn.setBounds(cCol.removeFromTop(19).reduced(4, 0));
        cCol.removeFromTop(3);
        strip.flt2Btn.setBounds(cCol.removeFromTop(19).reduced(4, 0));
        cCol.removeFromTop(8);
        if (strip.pan) strip.pan->setBounds(cCol.removeFromTop(64).reduced(2, 0));
        cCol.removeFromTop(8);

        auto smRow = cCol.removeFromTop(24).reduced(4, 0);
        const int btnW = (smRow.getWidth() - 4) / 2;
        strip.soloBtn.setBounds(smRow.removeFromLeft(btnW));
        strip.muteBtn.setBounds(smRow.removeFromRight(btnW));
        cCol.removeFromTop(12);

        constexpr int meterW = 7;
        cCol.removeFromRight(1);
        stripMeterRects_[static_cast<std::size_t>(i)] = cCol.removeFromRight(meterW).withTrimmedBottom(20);
        cCol.removeFromRight(2);
        strip.fader.setBounds(cCol);
    }

    auto mCol = masterCardRect_;
    mCol.removeFromTop(38);
    mCol.removeFromTop(8);
    delayReturn_.setBounds(mCol.removeFromTop(68).reduced(4, 0));
    mCol.removeFromTop(6);
    reverbReturn_.setBounds(mCol.removeFromTop(68).reduced(4, 0));
    mCol.removeFromTop(16);

    constexpr int mMeterW = 12;
    mCol.removeFromRight(6);
    masterMeterRect_ = mCol.removeFromRight(mMeterW).withTrimmedBottom(20);
    mCol.removeFromRight(6);
    masterFader_.setBounds(mCol);
}

void MixerSlideOverDrawer::mouseMove(const juce::MouseEvent& event) {
    const auto pos = event.getPosition();
    const bool hoverH = handleRect_.expanded(0, 18).contains(pos);
    if (hoverH != isHandleHovered_) {
        isHandleHovered_ = hoverH;
        setMouseCursor(isHandleHovered_ ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint(handleRect_.expanded(0, 18));
    }
}

void MixerSlideOverDrawer::mouseExit(const juce::MouseEvent&) {
    if (isHandleHovered_) {
        isHandleHovered_ = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint(handleRect_.expanded(0, 18));
    }
}

void MixerSlideOverDrawer::mouseDown(const juce::MouseEvent& event) {
    const auto pos = event.getPosition();
    if (handleRect_.expanded(0, 18).contains(pos)) {
        toggleDrawer();
        return;
    }
    if (isOpen_ && pos.x < drawerRect_.getX()) {
        setDrawerOpen(false);
        return;
    }
    if (isOpen_ && channelsCardRect_.contains(pos)) {
        const int colW = (channelsCardRect_.getWidth() - 7 * 4) / 8;
        for (int i = 0; i < 8; ++i) {
            const int colX = channelsCardRect_.getX() + i * (colW + 4);
            const juce::Rectangle<int> headerArea(colX, channelsCardRect_.getY(), colW, 38);
            if (headerArea.contains(pos)) {
                const juce::String paramId = "osc" + juce::String(i + 1) + "_enabled";
                if (auto* p = proc_.getAPVTS().getParameter(paramId)) {
                    const float cur = p->getValue();
                    p->setValueNotifyingHost(cur > 0.5f ? 0.0f : 1.0f);
                    updateDimming();
                    repaint();
                }
                return;
            }
        }
    }
}

bool MixerSlideOverDrawer::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey && isOpen_) {
        setDrawerOpen(false);
        return true;
    }
    return false;
}

void MixerSlideOverDrawer::toggleFilterRouting(int slotIdx, int targetFilter) {
    const juce::String paramId = "osc" + juce::String(slotIdx + 1) + "_filterRouting";
    if (auto* p = proc_.getAPVTS().getParameter(paramId)) {
        const int curRoute = static_cast<int>(p->convertFrom0to1(p->getValue()) + 0.5f);
        bool flt1 = (curRoute == 0 || curRoute == 2);
        bool flt2 = (curRoute == 1 || curRoute == 2);
        if (targetFilter == 1) flt1 = !flt1;
        if (targetFilter == 2) flt2 = !flt2;
        int newRoute = 3;
        if (flt1 && flt2) newRoute = 2;
        else if (flt1) newRoute = 0;
        else if (flt2) newRoute = 1;
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(newRoute)));
        updateDimming();
        repaint();
    }
}

void MixerSlideOverDrawer::updateDimming() {
    for (int i = 0; i < 8; ++i) {
        auto& strip = strips_[static_cast<std::size_t>(i)];
        const juce::String enabledId = "osc" + juce::String(i + 1) + "_enabled";
        bool isEnabled = true;
        if (auto* p = proc_.getAPVTS().getRawParameterValue(enabledId)) isEnabled = p->load() > 0.5f;

        const auto col = getLayerColour(i);
        const bool isSolo = proc_.isSlotSoloed(i);
        const bool isMuted = proc_.isSlotMuted(i);

        const float a = isEnabled ? (isMuted ? 0.75f : 1.0f) : 0.45f;
        if (strip.pan) strip.pan->setAlpha(a);
        strip.fader.setAlpha(a);
        if (strip.octStepper) {
            strip.octStepper->setAlpha(a);
            const juce::String octId = "osc" + juce::String(i + 1) + "_octave";
            if (auto* pOct = proc_.getAPVTS().getRawParameterValue(octId)) {
                strip.octStepper->currentOctave = static_cast<int>(std::round(pOct->load()));
            }
        }

        const juce::String routeId = "osc" + juce::String(i + 1) + "_filterRouting";
        int routeVal = 0;
        if (auto* pR = proc_.getAPVTS().getRawParameterValue(routeId)) routeVal = static_cast<int>(pR->load());
        const bool hasFlt1 = (routeVal == 0 || routeVal == 2);
        const bool hasFlt2 = (routeVal == 1 || routeVal == 2);

        strip.flt1Btn.setColors(
            col.withAlpha(0.38f), col, juce::Colours::white,
            juce::Colour(0xff0d1522), juce::Colour(0xff1e293b), juce::Colour(0xff64748b));
        strip.flt1Btn.setActive(hasFlt1);

        strip.flt2Btn.setColors(
            col.withAlpha(0.38f), col, juce::Colours::white,
            juce::Colour(0xff0d1522), juce::Colour(0xff1e293b), juce::Colour(0xff64748b));
        strip.flt2Btn.setActive(hasFlt2);

        strip.soloBtn.setColors(
            juce::Colour(0xffeab308), juce::Colour(0xfffacc15), juce::Colour(0xff090d16),
            juce::Colour(0xff0d1522), juce::Colour(0xff1e293b), juce::Colour(0xff64748b));
        strip.soloBtn.setActive(isSolo);

        strip.muteBtn.setColors(
            juce::Colour(0xff7f1d1d), juce::Colour(0xffdc2626), juce::Colours::white,
            juce::Colour(0xff0d1522), juce::Colour(0xff1e293b), juce::Colour(0xff64748b));
        strip.muteBtn.setActive(isMuted);
    }
}

void MixerSlideOverDrawer::timerCallback() {
    const float target = isOpen_ ? 1.0f : 0.0f;
    const float diff = target - openProgress_;

    if (std::abs(diff) > 0.002f) {
        openProgress_ += diff * 0.28f;
        resized();
        repaint();
    } else {
        if (std::abs(openProgress_ - target) > 0.0001f) {
            openProgress_ = target;
            resized();
            repaint();
        }
        if (!isOpen_) {
            stopTimer();
            return;
        }
    }

    if (isOpen_) {
        const bool hasSolo = proc_.hasAnySolo();
        if (clearSolosBtn_.isVisible() != hasSolo) clearSolosBtn_.setVisible(hasSolo);
        updateDimming();
        repaint();
    }
}

juce::Path MixerSlideOverDrawer::buildDrawerFillPath(juce::Rectangle<float> dRect, juce::Rectangle<float> hArea, float winRight) {
    juce::Path p;
    const float dX = dRect.getX();
    const float dY = dRect.getY();
    const float dB = dRect.getBottom();
    const float hTop = hArea.getY();
    const float hBottom = hArea.getBottom();
    const float hLeft = hArea.getX();
    constexpr float cr = 10.0f;
    constexpr float filletH = 18.0f;
    constexpr float kappa = 0.55228475f;

    p.startNewSubPath(winRight, dY);
    p.lineTo(dX + cr, dY);
    p.cubicTo(dX + cr * (1.0f - kappa), dY, dX, dY + cr * (1.0f - kappa), dX, dY + cr);
    p.lineTo(dX, hTop - filletH);
    p.cubicTo(dX, hTop, hLeft, hTop, hLeft, hTop + filletH);
    p.lineTo(hLeft, hBottom - filletH);
    p.cubicTo(hLeft, hBottom, dX, hBottom, dX, hBottom + filletH);
    p.lineTo(dX, dB - cr);
    p.cubicTo(dX, dB - cr * (1.0f - kappa), dX + cr * (1.0f - kappa), dB, dX + cr, dB);
    p.lineTo(winRight, dB);
    p.lineTo(winRight, dY);
    p.closeSubPath();
    return p;
}

juce::Path MixerSlideOverDrawer::buildDrawerStrokePath(juce::Rectangle<float> dRect, juce::Rectangle<float> hArea, float winRight) {
    juce::Path p;
    const float dX = dRect.getX();
    const float dY = dRect.getY();
    const float dB = dRect.getBottom();
    const float hTop = hArea.getY();
    const float hBottom = hArea.getBottom();
    const float hLeft = hArea.getX();
    constexpr float cr = 10.0f;
    constexpr float filletH = 18.0f;
    constexpr float kappa = 0.55228475f;

    p.startNewSubPath(winRight, dY);
    p.lineTo(dX + cr, dY);
    p.cubicTo(dX + cr * (1.0f - kappa), dY, dX, dY + cr * (1.0f - kappa), dX, dY + cr);
    p.lineTo(dX, hTop - filletH);
    p.cubicTo(dX, hTop, hLeft, hTop, hLeft, hTop + filletH);
    p.lineTo(hLeft, hBottom - filletH);
    p.cubicTo(hLeft, hBottom, dX, hBottom, dX, hBottom + filletH);
    p.lineTo(dX, dB - cr);
    p.cubicTo(dX, dB - cr * (1.0f - kappa), dX + cr * (1.0f - kappa), dB, dX + cr, dB);
    p.lineTo(winRight, dB);
    return p;
}

} // namespace nova::synth::ui

