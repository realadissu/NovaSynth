#include "DroneHubPanel.h"
#include <algorithm>
#include <cmath>

namespace nova::synth::ui {

void PianoKeyButton::paintButton(juce::Graphics& g, bool isHighlighted, bool isDown) {
    juce::ignoreUnused(isDown);
    const auto bounds = getLocalBounds().toFloat();
    const bool active = getToggleState();

    if (isBlack_) {
        // 1. Physical 3D Drop Shadow onto the white keys below
        g.setColour(juce::Colours::black.withAlpha(0.48f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 2.5f).reduced(0.5f, 0.0f), 2.5f);

        // 2. Black Key Body with rounded bottom corners
        const auto keyRect = bounds.reduced(0.5f, 0.0f);
        juce::Path keyPath;
        keyPath.addRoundedRectangle(keyRect.getX(), keyRect.getY(), keyRect.getWidth(), keyRect.getHeight(),
                                    2.5f, 2.5f, false, false, true, true);

        if (active) {
            // Active Glowing Amber Ebony Key
            juce::ColourGradient grad(juce::Colour(0xfff59e0b), keyRect.getCentreX(), keyRect.getY(),
                                      juce::Colour(0xffb45309), keyRect.getCentreX(), keyRect.getBottom(), false);
            g.setGradientFill(grad);
            g.fillPath(keyPath);

            // Front beveled facet
            g.setColour(juce::Colour(0xff78350f));
            g.fillRoundedRectangle(keyRect.getX(), keyRect.getBottom() - 5.0f, keyRect.getWidth(), 5.0f, 1.5f);

            g.setColour(juce::Colour(0xfffef3c7));
            g.strokePath(keyPath, juce::PathStrokeType(1.2f));
        } else {
            // Classic Matte Ebony Black Key with 3D front facet
            const auto topColor = isHighlighted ? juce::Colour(0xff2d3748) : juce::Colour(0xff1e242f);
            const auto botColor = isHighlighted ? juce::Colour(0xff1a202c) : juce::Colour(0xff0d1117);
            juce::ColourGradient grad(topColor, keyRect.getCentreX(), keyRect.getY(),
                                      botColor, keyRect.getCentreX(), keyRect.getBottom(), false);
            g.setGradientFill(grad);
            g.fillPath(keyPath);

            // Front beveled slope
            g.setColour(juce::Colour(0xff080c12));
            g.fillRoundedRectangle(keyRect.getX(), keyRect.getBottom() - 5.0f, keyRect.getWidth(), 5.0f, 1.5f);

            // Subtle edge reflection
            g.setColour(juce::Colour(0xff475569).withAlpha(0.40f));
            g.strokePath(keyPath, juce::PathStrokeType(1.0f));
        }

        // Note Name label
        g.setFont(theme_ ? theme_->getLabelFont(10.0f).boldened() : juce::Font(juce::FontOptions{10.0f}.withStyle("Bold")));
        g.setColour(active ? juce::Colours::white : juce::Colour(0xff94a3b8));
        g.drawText(noteName_, bounds.withTrimmedBottom(6.0f), juce::Justification::centredBottom, false);
    } else {
        // White Key Body with authentic ivory contours
        const auto keyRect = bounds;
        juce::Path keyPath;
        // Realistic rounded bottom corners (radius 3.5px)
        keyPath.addRoundedRectangle(keyRect.getX(), keyRect.getY(), keyRect.getWidth(), keyRect.getHeight() - 1.0f,
                                    3.5f, 3.5f, false, false, true, true);

        if (active) {
            // Active Warm Ivory/Amber Glowing Key
            juce::ColourGradient grad(juce::Colour(0xfffef3c7), keyRect.getCentreX(), keyRect.getY(),
                                      juce::Colour(0xfff59e0b), keyRect.getCentreX(), keyRect.getBottom(), false);
            g.setGradientFill(grad);
            g.fillPath(keyPath);

            // Front bevel lip
            g.setColour(juce::Colour(0xffd97706));
            g.fillRoundedRectangle(keyRect.getX(), keyRect.getBottom() - 4.0f, keyRect.getWidth(), 3.0f, 1.5f);

            g.setColour(juce::Colour(0xffb45309));
            g.strokePath(keyPath, juce::PathStrokeType(1.2f));
        } else {
            // Realistic Ivory Key with subtle top-to-bottom shading
            const auto topColor = isHighlighted ? juce::Colour(0xffffffff) : juce::Colour(0xfff8fafc);
            const auto botColor = isHighlighted ? juce::Colour(0xfff1f5f9) : juce::Colour(0xffe2e8f0);
            juce::ColourGradient grad(topColor, keyRect.getCentreX(), keyRect.getY(),
                                      botColor, keyRect.getCentreX(), keyRect.getBottom(), false);
            g.setGradientFill(grad);
            g.fillPath(keyPath);

            // Front bevel lip
            g.setColour(juce::Colour(0xffcbd5e1));
            g.fillRoundedRectangle(keyRect.getX(), keyRect.getBottom() - 4.0f, keyRect.getWidth(), 3.0f, 1.5f);

            // Key slit / border
            g.setColour(juce::Colour(0xff94a3b8).withAlpha(0.45f));
            g.strokePath(keyPath, juce::PathStrokeType(1.0f));
        }

        // Note Name label in lower third of key (below black keys)
        const auto labelArea = bounds.withTrimmedTop(bounds.getHeight() * 0.70f).withTrimmedBottom(4.0f);
        g.setFont(theme_ ? theme_->getLabelFont(11.0f).boldened() : juce::Font(juce::FontOptions{11.0f}.withStyle("Bold")));
        g.setColour(active ? juce::Colour(0xff451a03) : juce::Colour(0xff1e293b));
        g.drawText(noteName_, labelArea, juce::Justification::centred, false);
    }
}

void DroneHubPanel::paint(juce::Graphics& g) {
    g.fillAll(theme_.bgSurface.darker(0.18f));

    drawMasterCard(g, cardMasterRect_.toFloat());
    drawKeySelectorCard(g, cardKeysRect_.toFloat());
    drawAuraCard(g, cardAuraRect_.toFloat());
}

void DroneHubPanel::drawMasterCard(juce::Graphics& g, juce::Rectangle<float> area) {
    g.setColour(theme_.bgSurface.darker(0.12f));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    // Header Title
    g.setFont(theme_.getSectionFont(12.0f));
    g.setColour(juce::Colour(0xfff59e0b)); // Warm Amber
    g.drawText("WORSHIP DRONE BED", juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 160.0f, 16.0f),
               juce::Justification::left);

    // Subtitle badge
    g.setFont(theme_.getLabelFont(10.0f));
    g.setColour(theme_.textMuted);
    g.drawText("INFINITE TONAL PAD", juce::Rectangle<float>(area.getRight() - 110.0f, area.getY() + 7.0f, 100.0f, 14.0f),
               juce::Justification::right);
}

void DroneHubPanel::drawKeySelectorCard(juce::Graphics& g, juce::Rectangle<float> area) {
    g.setColour(theme_.bgSurface.darker(0.12f));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    // Header Title
    g.setFont(theme_.getSectionFont(12.0f));
    g.setColour(theme_.accentPrimary);
    g.drawText("ROOT KEY & HARMONIC VOICING", juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 200.0f, 16.0f),
               juce::Justification::left);

    // Current Active Key
    int curKey = 0;
    if (auto* p = proc_.getAPVTS().getRawParameterValue("droneRootKey")) {
        curKey = std::clamp(static_cast<int>(p->load()), 0, 11);
    }

    // Current Octave display between stepper buttons
    int curOct = 0;
    if (auto* p = proc_.getAPVTS().getRawParameterValue("droneOctave")) {
        curOct = std::clamp(static_cast<int>(p->load()), -2, 2);
    }
    const juce::String octStr = "OCT " + (curOct > 0 ? "+" + juce::String(curOct) : juce::String(curOct));
    g.setFont(theme_.getLabelFont(10.0f));
    g.setColour(curOct != 0 ? juce::Colour(0xfff59e0b) : theme_.textSecondary);
    g.drawText(octStr, juce::Rectangle<float>(area.getRight() - 68.0f, area.getY() + 6.0f, 42.0f, 16.0f),
               juce::Justification::centred);

    // Highlight active Key button state
    for (int k = 0; k < 12; ++k) {
        keyBtns_[static_cast<std::size_t>(k)].setToggleState(k == curKey, juce::dontSendNotification);
    }

    // Highlight active Voicing button
    int curVoicing = 0;
    if (auto* pV = proc_.getAPVTS().getRawParameterValue("droneVoicing")) {
        curVoicing = std::clamp(static_cast<int>(pV->load()), 0, 3);
    }
    for (int v = 0; v < 4; ++v) {
        voicingBtns_[static_cast<std::size_t>(v)].setToggleState(v == curVoicing, juce::dontSendNotification);
    }

    // Red velvet acoustic felt strip along the top rail of piano keys
    if (!pianoArea_.isEmpty()) {
        g.setColour(juce::Colour(0xff881337)); // Deep burgundy felt
        g.fillRect(pianoArea_.getX(), pianoArea_.getY(), pianoArea_.getWidth(), 3);
        g.setColour(juce::Colours::black.withAlpha(0.60f));
        g.drawHorizontalLine(pianoArea_.getY(), static_cast<float>(pianoArea_.getX()), static_cast<float>(pianoArea_.getRight()));
    }
}

void DroneHubPanel::drawAuraCard(juce::Graphics& g, juce::Rectangle<float> area) {
    g.setColour(theme_.bgSurface.darker(0.12f));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    // Header Title
    g.setFont(theme_.getSectionFont(12.0f));
    g.setColour(juce::Colour(0xfff59e0b));
    g.drawText("HARMONIC AURA", juce::Rectangle<float>(area.getX() + 10.0f, area.getY() + 6.0f, 120.0f, 16.0f),
               juce::Justification::left);

    bool isDroneActive = false;
    bool isDroneEnabled = false;
    if (auto* p = proc_.getAPVTS().getRawParameterValue("droneEnabled")) {
        isDroneEnabled = p->load() > 0.5f;
        isDroneActive = isDroneEnabled && proc_.isDroneTriggered();
    }

    // Subtitle badge
    g.setFont(theme_.getLabelFont(10.0f));
    g.setColour(isDroneActive ? juce::Colour(0xfff59e0b) : (isDroneEnabled ? juce::Colour(0xff38bdf8) : theme_.textMuted));
    g.drawText(isDroneActive ? "ACTIVE" : (isDroneEnabled ? "ARMED (KEY)" : "OFF"),
               juce::Rectangle<float>(area.getRight() - 95.0f, area.getY() + 7.0f, 85.0f, 14.0f),
               juce::Justification::right);

    // Real-time Glowing Harmonic Resonance Aura
    const auto plotArea = area.withTrimmedTop(28.0f).reduced(8.0f, 6.0f);
    g.setColour(juce::Colour(0xff090d16));
    g.fillRoundedRectangle(plotArea, 4.0f);
    g.setColour(theme_.borderSubtle.withAlpha(0.40f));
    g.drawRoundedRectangle(plotArea, 4.0f, 1.0f);

    const float cx = plotArea.getCentreX();
    const float cy = plotArea.getCentreY();
    const float maxR = std::min(plotArea.getWidth(), plotArea.getHeight()) * 0.45f;
    float droneLevel = 0.5f;
    if (auto* pL = proc_.getAPVTS().getRawParameterValue("droneLevel")) {
        droneLevel = pL->load();
    }

    const float intensity = isDroneActive ? (0.25f + 0.75f * std::clamp(droneLevel, 0.0f, 1.0f)) : 0.10f;
    const auto auraCol = isDroneActive ? juce::Colour(0xfff59e0b) : theme_.textMuted;

    // Draw concentric glowing harmonic rings
    for (int ring = 1; ring <= 4; ++ring) {
        const float rBase = maxR * (static_cast<float>(ring) / 4.0f);
        const float modulation = std::sin(visualPhase_ * 1.5f + static_cast<float>(ring) * 0.8f) * 3.5f * intensity;
        const float r = std::max(2.0f, rBase + modulation);

        juce::Path ringP;
        ringP.addCentredArc(cx, cy, r, r, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);

        const float ringAlpha = (1.0f - static_cast<float>(ring) * 0.18f) * intensity;
        g.setColour(auraCol.withAlpha(ringAlpha * 0.35f));
        g.strokePath(ringP, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(auraCol.withAlpha(ringAlpha * 0.85f));
        g.strokePath(ringP, juce::PathStrokeType(1.2f));
    }

    // Center Pulsing Core Dot
    const float coreR = 4.0f + 3.0f * intensity * std::abs(std::sin(visualPhase_ * 2.0f));
    g.setColour(auraCol.withAlpha(intensity * 0.40f));
    g.fillEllipse(cx - coreR - 3.0f, cy - coreR - 3.0f, (coreR + 3.0f) * 2.0f, (coreR + 3.0f) * 2.0f);
    g.setColour(isDroneActive ? juce::Colours::white : theme_.textMuted);
    g.fillEllipse(cx - coreR, cy - coreR, coreR * 2.0f, coreR * 2.0f);
}

} // namespace nova::synth::ui

