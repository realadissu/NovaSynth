#include "MixerSlideOverDrawer.h"
#include <BinaryData.h>
#include <algorithm>

namespace nova::synth::ui {

void MixerSlideOverDrawer::paint(juce::Graphics& g) {
    if (openProgress_ <= 0.001f) {
        drawClosedTab(g, getWidth(), handleRect_.toFloat());
        return;
    }

    const auto scrimArea = juce::Rectangle<int>(0, topY_, drawerRect_.getX(), bottomY_ - topY_);
    if (!scrimArea.isEmpty()) {
        g.setColour(juce::Colours::black.withAlpha(0.60f * openProgress_));
        g.fillRect(scrimArea);
    }

    const auto drwF = drawerRect_.toFloat();
    const auto hF   = handleRect_.toFloat();
    const float winRight = static_cast<float>(getWidth());
    const auto fillPath   = buildDrawerFillPath(drwF, hF, winRight);
    const auto strokePath = buildDrawerStrokePath(drwF, hF, winRight);

    const auto shadowGrad = juce::ColourGradient::horizontal(
        juce::Colours::black.withAlpha(0.65f * openProgress_), hF.getX() - 16.0f,
        juce::Colours::transparentBlack, hF.getX() + 6.0f);
    g.setGradientFill(shadowGrad);
    g.fillRect(juce::Rectangle<float>(hF.getX() - 16.0f, drwF.getY(), 16.0f, drwF.getHeight()));

    g.setColour(juce::Colour(0xff080c14));
    g.fillPath(fillPath);

    const float strokeW = isHandleHovered_ ? 2.4f : 2.0f;
    g.setColour(isHandleHovered_ ? theme_.accentPrimary : juce::Colour(0xff4b6584));
    g.strokePath(strokePath, juce::PathStrokeType(strokeW, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

    g.setColour(juce::Colours::white.withAlpha(0.08f * openProgress_));
    g.drawHorizontalLine(drawerRect_.getY() + 1, drwF.getX() + 10.0f, winRight);

    drawTabDetails(g, hF);
    drawHeader(g);
    drawChannelStrips(g);
    drawMasterSection(g);
}

void MixerSlideOverDrawer::drawClosedTab(juce::Graphics& g, int totalW, juce::Rectangle<float> hArea) {
    juce::Path pFill;
    const float winRight = static_cast<float>(totalW);
    const float hTop = hArea.getY();
    const float hBottom = hArea.getBottom();
    const float hLeft = hArea.getX();
    constexpr float filletH = 18.0f;

    pFill.startNewSubPath(winRight, hTop - filletH);
    pFill.cubicTo(winRight, hTop, hLeft, hTop, hLeft, hTop + filletH);
    pFill.lineTo(hLeft, hBottom - filletH);
    pFill.cubicTo(hLeft, hBottom, winRight, hBottom, winRight, hBottom + filletH);
    pFill.closeSubPath();

    const auto bgGrad = juce::ColourGradient::horizontal(
        isHandleHovered_ ? juce::Colour(0xff223249) : juce::Colour(0xff111a28), hLeft,
        theme_.bgSurface.darker(0.16f), winRight);
    g.setGradientFill(bgGrad);
    g.fillPath(pFill);

    juce::Path pStroke;
    pStroke.startNewSubPath(winRight, hTop - filletH);
    pStroke.cubicTo(winRight, hTop, hLeft, hTop, hLeft, hTop + filletH);
    pStroke.lineTo(hLeft, hBottom - filletH);
    pStroke.cubicTo(hLeft, hBottom, winRight, hBottom, winRight, hBottom + filletH);

    const float strokeW = isHandleHovered_ ? 2.4f : 2.0f;
    g.setColour(isHandleHovered_ ? theme_.accentPrimary : juce::Colour(0xff4b6584));
    g.strokePath(pStroke, juce::PathStrokeType(strokeW, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

    drawTabDetails(g, hArea);
}

void MixerSlideOverDrawer::drawTabDetails(juce::Graphics& g, juce::Rectangle<float> hArea) {
    const float hLeft = hArea.getX();
    const float hCenterY = hArea.getCentreY();

    g.setColour(isHandleHovered_ ? theme_.accentPrimary.withAlpha(0.85f) : juce::Colour(0xff64748b));
    const float dotX1 = hLeft + 5.5f;
    const float dotX2 = hLeft + 9.0f;
    for (int row = -2; row <= 2; ++row) {
        const float dotY = hCenterY + static_cast<float>(row) * 5.0f;
        g.fillEllipse(dotX1 - 1.0f, dotY - 1.0f, 2.0f, 2.0f);
        g.fillEllipse(dotX2 - 1.0f, dotY - 1.0f, 2.0f, 2.0f);
    }

    g.setFont(theme_.getSectionFont(11.0f));
    static constexpr std::array<const char*, 5> kLetters = { "M", "I", "X", "E", "R" };
    const float letterH = (hArea.getHeight() - 18.0f) / 5.0f;
    const float textX = hLeft + 12.0f;
    const float textW = hArea.getRight() - textX;

    for (std::size_t i = 0; i < 5; ++i) {
        const auto lRect = juce::Rectangle<float>(textX, hArea.getY() + 9.0f + static_cast<float>(i) * letterH, textW, letterH);
        g.setColour(isHandleHovered_ ? theme_.accentPrimary : juce::Colour(0xffcbd5e1));
        g.drawText(kLetters[i], lRect, juce::Justification::centred);
    }
}

void MixerSlideOverDrawer::drawHeader(juce::Graphics& g) {
    const auto hArea = drawerRect_.withHeight(40).reduced(12, 6).toFloat();

    g.setFont(theme_.getSectionFont(14.0f));
    g.setColour(theme_.accentPrimary);
    g.drawText("CONSOLE MIXER", hArea.withWidth(160.0f), juce::Justification::centredLeft);

    g.setFont(theme_.getLabelFont(10.0f));
    g.setColour(theme_.textMuted);
    g.drawText("8 SOUND ENGINES + MASTER BUS", hArea.withTrimmedLeft(165.0f).withWidth(210.0f), juce::Justification::centredLeft);

    if (clearSolosBtn_.isVisible()) {
        clearSolosBtn_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff59e0b).withAlpha(0.20f));
        clearSolosBtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff59e0b));
    }
}

void MixerSlideOverDrawer::drawChannelStrips(juce::Graphics& g) {
    g.setColour(juce::Colour(0xff0b101a));
    g.fillRoundedRectangle(channelsCardRect_.toFloat(), 6.0f);
    g.setColour(juce::Colour(0xff223249));
    g.drawRoundedRectangle(channelsCardRect_.toFloat(), 6.0f, 1.2f);

    const auto curParams = proc_.getCurrentParameters();

    for (int i = 0; i < 8; ++i) {
        auto& strip = strips_[static_cast<std::size_t>(i)];
        const auto colW = (channelsCardRect_.getWidth() - 7 * 4) / 8;
        const int colX = channelsCardRect_.getX() + i * (colW + 4);
        const auto laneR = juce::Rectangle<float>(static_cast<float>(colX), static_cast<float>(channelsCardRect_.getY()), static_cast<float>(colW), static_cast<float>(channelsCardRect_.getHeight()));

        if (i > 0) {
            g.setColour(juce::Colour(0xff182436));
            g.drawVerticalLine(colX - 2, laneR.getY() + 4.0f, laneR.getBottom() - 4.0f);
        }

        const juce::String enabledId = "osc" + juce::String(i + 1) + "_enabled";
        bool isEnabled = true;
        if (auto* p = proc_.getAPVTS().getRawParameterValue(enabledId)) isEnabled = p->load() > 0.5f;

        const auto headerR = laneR.withHeight(38.0f).reduced(2.0f, 2.0f);
        const auto col = getLayerColour(i);
        const juce::String name = getLayerShortName(curParams, i);
        const juce::String eng = getEngineShortName(curParams, i);

        g.setColour(isEnabled ? col : col.withAlpha(0.40f));
        g.fillRoundedRectangle(headerR.withHeight(3.0f), 1.5f);

        g.setFont(theme_.getSectionFont(12.0f));
        g.setColour(isEnabled ? juce::Colours::white : juce::Colour(0xff94a3b8));
        g.drawText(name, headerR.withTrimmedTop(5.0f).withHeight(15.0f), juce::Justification::centred);

        g.setFont(theme_.getLabelFont(9.0f));
        g.setColour(isEnabled ? col : col.withAlpha(0.50f));
        g.drawText(eng, headerR.withTrimmedTop(20.0f).withHeight(13.0f), juce::Justification::centred);

        drawChannelFaderTicks(g, strip.fader, col);

        const auto mRect = stripMeterRects_[static_cast<std::size_t>(i)].toFloat();
        const float pk = isEnabled ? proc_.getSourcePeak(i) : 0.0f;
        drawMeterBar(g, mRect, pk);
    }
}

void MixerSlideOverDrawer::drawMasterSection(juce::Graphics& g) {
    g.setColour(juce::Colour(0xff090e17));
    g.fillRoundedRectangle(masterCardRect_.toFloat(), 6.0f);
    g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.65f));
    g.drawRoundedRectangle(masterCardRect_.toFloat(), 6.0f, 1.2f);

    const auto hR = masterCardRect_.toFloat().withHeight(38.0f).reduced(6.0f, 2.0f);
    g.setColour(juce::Colour(0xfff59e0b));
    g.fillRoundedRectangle(hR.withHeight(3.0f), 1.5f);

    g.setFont(theme_.getSectionFont(12.0f));
    g.setColour(juce::Colour(0xfff59e0b));
    g.drawText("MASTER", hR.withTrimmedTop(5.0f).withHeight(15.0f), juce::Justification::centred);

    g.setFont(theme_.getLabelFont(9.0f));
    g.setColour(juce::Colour(0xffcbd5e1));
    g.drawText("OUT BUS", hR.withTrimmedTop(20.0f).withHeight(13.0f), juce::Justification::centred);

    drawMasterFaderTicks(g, masterFader_);

    const auto mRect = masterMeterRect_.toFloat();
    const float pkL = proc_.getMasterPeakL();
    const float pkR = proc_.getMasterPeakR();
    const float halfW = (mRect.getWidth() - 2.0f) * 0.5f;
    drawMeterBar(g, mRect.withWidth(halfW), pkL);
    drawMeterBar(g, mRect.withTrimmedLeft(halfW + 2.0f), pkR);
}

float MixerSlideOverDrawer::getFaderThumbLeft(const juce::Slider& slider) noexcept {
    return ConsoleFaderLookAndFeel::calculateThumbLeft(
        static_cast<float>(slider.getX()), static_cast<float>(slider.getWidth()));
}

void MixerSlideOverDrawer::drawChannelFaderTicks(juce::Graphics& g, const juce::Slider& slider, juce::Colour /*accentCol*/) {
    const float thumbL = getFaderThumbLeft(slider);
    const float sliderY = static_cast<float>(slider.getY());

    struct TickDef {
        float val;
        const char* label;
        bool isMajor;
        bool isUnity;
    };

    static constexpr std::array<TickDef, 7> kTicks = {{
        { 1.0000f, "0",    true,  true  },
        { 0.7079f, "-3",   true,  false },
        { 0.5012f, "-6",   true,  false },
        { 0.2512f, "-12",  true,  false },
        { 0.1259f, "",     false, false },
        { 0.0631f, "",     false, false },
        { 0.0000f, "-inf", true,  false }
    }};

    for (const auto& t : kTicks) {
        const float y = sliderY + slider.getPositionOfValue(t.val);
        const float tickLen = t.isUnity ? 6.0f : (t.isMajor ? 4.5f : 3.0f);
        const float strokeW = t.isUnity ? 1.6f : 1.0f;

        if (t.isUnity) g.setColour(juce::Colours::white);
        else if (t.isMajor) g.setColour(juce::Colour(0xff64748b));
        else g.setColour(juce::Colour(0xff334155));

        g.drawLine(thumbL - 1.5f - tickLen, y, thumbL - 1.5f, y, strokeW);

        if (t.label != nullptr && t.label[0] != '\0') {
            if (t.isUnity) {
                g.setFont(theme_.getValueFont(9.5f));
                g.setColour(juce::Colours::white);
            } else {
                g.setFont(theme_.getValueFont(9.0f));
                g.setColour(juce::Colour(0xff94a3b8));
            }
            const auto textRect = juce::Rectangle<float>(thumbL - 25.0f, y - 6.0f, 16.0f, 12.0f);
            g.drawText(t.label, textRect, juce::Justification::centredRight, false);
        }
    }
}

void MixerSlideOverDrawer::drawMasterFaderTicks(juce::Graphics& g, const juce::Slider& slider) {
    const float thumbL = getFaderThumbLeft(slider);
    const float sliderY = static_cast<float>(slider.getY());

    struct TickDef {
        float val;
        const char* label;
        bool isMajor;
        bool isUnity;
    };

    static constexpr std::array<TickDef, 13> kTicks = {{
        {  12.0f, "+12", true,  false },
        {   9.0f, "",    false, false },
        {   6.0f, "+6",  true,  false },
        {   3.0f, "",    false, false },
        {   0.0f, "0",   true,  true  },
        {  -3.0f, "",    false, false },
        {  -6.0f, "-6",  true,  false },
        {  -9.0f, "",    false, false },
        { -12.0f, "-12", true,  false },
        { -15.0f, "",    false, false },
        { -18.0f, "-18", true,  false },
        { -21.0f, "",    false, false },
        { -24.0f, "-24", true,  false }
    }};

    for (const auto& t : kTicks) {
        const float y = sliderY + slider.getPositionOfValue(t.val);
        const float tickLen = t.isUnity ? 7.0f : (t.isMajor ? 4.5f : 3.0f);
        const float strokeW = t.isUnity ? 1.8f : 1.0f;

        if (t.isUnity) g.setColour(juce::Colour(0xfff59e0b));
        else if (t.val > 0.0f) g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.65f));
        else if (t.isMajor) g.setColour(juce::Colour(0xff64748b));
        else g.setColour(juce::Colour(0xff334155));

        g.drawLine(thumbL - 1.5f - tickLen, y, thumbL - 1.5f, y, strokeW);

        if (t.label != nullptr && t.label[0] != '\0') {
            if (t.isUnity) {
                g.setFont(theme_.getValueFont(10.0f));
                g.setColour(juce::Colour(0xfffbbf24));
            } else if (t.val > 0.0f) {
                g.setFont(theme_.getValueFont(9.0f));
                g.setColour(juce::Colour(0xfff59e0b));
            } else {
                g.setFont(theme_.getValueFont(9.0f));
                g.setColour(juce::Colour(0xffcbd5e1));
            }
            const auto textRect = juce::Rectangle<float>(thumbL - 27.0f, y - 6.0f, 18.0f, 12.0f);
            g.drawText(t.label, textRect, juce::Justification::centredRight, false);
        }
    }
}

void MixerSlideOverDrawer::drawMeterBar(juce::Graphics& g, juce::Rectangle<float> area, float level) {
    g.setColour(juce::Colour(0xff090d16));
    g.fillRoundedRectangle(area, 2.0f);
    g.setColour(juce::Colour(0xff1e293b).withAlpha(0.5f));
    g.drawRoundedRectangle(area, 2.0f, 0.8f);

    if (level <= 0.001f) return;

    const float fillH = std::clamp(level, 0.0f, 1.0f) * area.getHeight();
    const auto fillRect = area.withTrimmedTop(area.getHeight() - fillH);

    const auto grad = juce::ColourGradient::vertical(
        level > 0.95f ? juce::Colour(0xffef4444) : (level > 0.70f ? juce::Colour(0xfff59e0b) : juce::Colour(0xff2dd4bf)),
        fillRect.getY(),
        juce::Colour(0xff10b981), fillRect.getBottom());
    g.setGradientFill(grad);
    g.fillRoundedRectangle(fillRect, 2.0f);
}

juce::Image MixerSlideOverDrawer::loadIcon(const char* resourceName, const juce::String& fallbackFilename) {
    int dataSize = 0;
    const char* data = NovaResources::getNamedResource(resourceName, dataSize);
    if (data != nullptr && dataSize > 0) {
        auto img = juce::ImageFileFormat::loadFrom(data, static_cast<std::size_t>(dataSize));
        if (img.isValid()) return img;
    }
    const auto fallbackFile = juce::File("/Users/wolfgangmeincke/Nova Plugins/Resources/Icons").getChildFile(fallbackFilename);
    if (fallbackFile.existsAsFile()) {
        auto img = juce::ImageFileFormat::loadFrom(fallbackFile);
        if (img.isValid()) return img;
    }
    return {};
}

juce::Colour MixerSlideOverDrawer::getLayerColour(int index) noexcept {
    static constexpr std::array<uint32_t, 8> kColours = {
        0xff38bdf8, 0xff2dd4bf, 0xffec4899, 0xffa855f7,
        0xff84cc16, 0xfff59e0b, 0xff06b6d4, 0xff8b5cf6
    };
    return juce::Colour(kColours[static_cast<std::size_t>(index % 8)]);
}

juce::String MixerSlideOverDrawer::getLayerShortName(const nova::dsp::WorshipSynthEngine::Parameters& p, int i) {
    const auto& s = p.oscSlots[static_cast<std::size_t>(i)];
    if (s.sourceType == 1) return "SUB";
    if (s.sourceType == 2) return "PLUCK";
    if (s.sourceType == 3) return "NOISE";
    int oscIdx = 0;
    for (int j = 0; j < i; ++j) {
        if (p.oscSlots[static_cast<std::size_t>(j)].sourceType == 0) oscIdx++;
    }
    return juce::String("OSC ") + juce::String::charToString(static_cast<char>('A' + oscIdx));
}

juce::String MixerSlideOverDrawer::getEngineShortName(const nova::dsp::WorshipSynthEngine::Parameters& p, int i) {
    const auto& s = p.oscSlots[static_cast<std::size_t>(i)];
    if (s.sourceType == 1) return "SUB OSC";
    if (s.sourceType == 2) return "PLUCK";
    if (s.sourceType == 3) return "NOISE";
    if (s.sourceType == 4) return "SAMPLE";
    using ET = nova::dsp::MultiEngineOscillator::EngineType;
    switch (s.engine) {
        case ET::WarmAnalog:        return "ANALOG";
        case ET::EvolvingWavetable: return "WAVETABLE";
        case ET::OrganicTexture:    return "TEXTURE";
        case ET::PhysicalPluckModal:return "PLUCK";
        case ET::SubOscillator:     return "SUB";
        case ET::NoiseTransient:    return "NOISE";
        case ET::SamplePlayback:    return "SAMPLE";
    }
    return "ANALOG";
}

} // namespace nova::synth::ui

