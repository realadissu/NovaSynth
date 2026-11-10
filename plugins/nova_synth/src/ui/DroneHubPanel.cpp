#include "DroneHubPanel.h"
#include <algorithm>

namespace nova::synth::ui {

PianoKeyButton::PianoKeyButton() : juce::Button("Key") {
    setClickingTogglesState(false);
}

void PianoKeyButton::setup(const juce::String& noteName, bool isBlack, const nova::ui::NovaTheme* theme) {
    noteName_ = noteName;
    isBlack_ = isBlack;
    theme_ = theme;
    setButtonText(noteName);
    setTooltip("Drone Root Note: " + noteName);
}

DroneHubPanel::DroneHubPanel(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc),
      theme_(theme),
      droneLevel_("LEVEL", "%", theme, false),
      droneTone_("WARMTH", "%", theme, true),
      droneGlide_("GLIDE", "s", theme, false) {

    auto& apvts = proc_.getAPVTS();

    // 1. Master Pad Hold Toggle Button
    droneHoldBtn_.setButtonText("DRONE PAD HOLD");
    droneHoldBtn_.setClickingTogglesState(true);
    droneHoldBtn_.getProperties().set("warmAccent", true);
    droneHoldAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "droneEnabled", droneHoldBtn_);
    addAndMakeVisible(droneHoldBtn_);

    // 2. Space Freeze Button
    freezeBtn_.setButtonText("FREEZE SPACE");
    freezeBtn_.setClickingTogglesState(true);
    freezeBtn_.getProperties().set("warmAccent", true);
    freezeAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "reverbFreeze", freezeBtn_);
    addAndMakeVisible(freezeBtn_);

    // 3. Knobs
    droneLevel_.attachToParameter(apvts, "droneLevel");
    droneTone_.attachToParameter(apvts,  "droneTone");
    droneGlide_.attachToParameter(apvts, "droneGlide");
    addAndMakeVisible(droneLevel_);
    addAndMakeVisible(droneTone_);
    addAndMakeVisible(droneGlide_);

    // 4. 12-Key Chromatic Selector (Authentic Piano Keybed)
    static constexpr std::array<const char*, 12> kKeyNames = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    static constexpr std::array<bool, 12> kIsBlack = {
        false, true, false, true, false, false, true, false, true, false, true, false
    };
    for (int k = 0; k < 12; ++k) {
        keyBtns_[static_cast<std::size_t>(k)].setup(
            kKeyNames[static_cast<std::size_t>(k)],
            kIsBlack[static_cast<std::size_t>(k)],
            &theme_);
        keyBtns_[static_cast<std::size_t>(k)].onClick = [this, k]() {
            if (auto* p = proc_.getAPVTS().getParameter("droneRootKey")) {
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(k)));
            }
            proc_.triggerDrone(true);
            repaint();
        };
        addAndMakeVisible(keyBtns_[static_cast<std::size_t>(k)]);
    }

    // 5. Voicing Mode Buttons
    static constexpr std::array<const char*, 4> kVoicingLabels = {
        "LUSH 5TH", "DEEP SUB", "AMBIENT 9TH", "CELESTIAL 8VE"
    };
    for (int v = 0; v < 4; ++v) {
        voicingBtns_[static_cast<std::size_t>(v)].setButtonText(kVoicingLabels[static_cast<std::size_t>(v)]);
        voicingBtns_[static_cast<std::size_t>(v)].setClickingTogglesState(false);
        voicingBtns_[static_cast<std::size_t>(v)].onClick = [this, v]() {
            if (auto* p = proc_.getAPVTS().getParameter("droneVoicing")) {
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(v)));
                repaint();
            }
        };
        addAndMakeVisible(voicingBtns_[static_cast<std::size_t>(v)]);
    }

    // 6. Octave Steppers
    octDownBtn_.setButtonText("-");
    octDownBtn_.setTooltip("Transpose Drone Down One Octave (-2 to +2)");
    octDownBtn_.onClick = [this]() {
        if (auto* p = proc_.getAPVTS().getParameter("droneOctave")) {
            const int cur = std::clamp(static_cast<int>(p->convertFrom0to1(p->getValue())), -2, 2);
            if (cur > -2) {
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(cur - 1)));
                repaint();
            }
        }
    };
    addAndMakeVisible(octDownBtn_);

    octUpBtn_.setButtonText("+");
    octUpBtn_.setTooltip("Transpose Drone Up One Octave (-2 to +2)");
    octUpBtn_.onClick = [this]() {
        if (auto* p = proc_.getAPVTS().getParameter("droneOctave")) {
            const int cur = std::clamp(static_cast<int>(p->convertFrom0to1(p->getValue())), -2, 2);
            if (cur < 2) {
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(cur + 1)));
                repaint();
            }
        }
    };
    addAndMakeVisible(octUpBtn_);

    startTimerHz(30);
}

DroneHubPanel::~DroneHubPanel() {
    stopTimer();
}

juce::String DroneHubPanel::getTooltip() {
    return "Continuous Worship Drone sound bed for seamless musical transitions and background ambient pad.";
}

void DroneHubPanel::timerCallback() {
    visualPhase_ += 0.045f;
    if (visualPhase_ > 628.3185f) visualPhase_ -= 628.3185f;
    repaint(cardAuraRect_);
}

void DroneHubPanel::resized() {
    const auto area = getLocalBounds().reduced(8, 6);
    const int totalW = area.getWidth();
    const int totalH = area.getHeight();
    constexpr int gap = 8;
    const int availW = totalW - gap * 2;

    // Split: Master (30%), Key Selector (44%), Harmonic Aura (26%)
    const int wMaster = static_cast<int>(static_cast<float>(availW) * 0.30f);
    const int wKeys   = static_cast<int>(static_cast<float>(availW) * 0.44f);
    const int wAura   = availW - wMaster - wKeys;

    cardMasterRect_ = juce::Rectangle<int>(area.getX(), area.getY(), wMaster, totalH);
    cardKeysRect_   = juce::Rectangle<int>(cardMasterRect_.getRight() + gap, area.getY(), wKeys, totalH);
    cardAuraRect_   = juce::Rectangle<int>(cardKeysRect_.getRight() + gap, area.getY(), wAura, totalH);

    // Layout Master Controls
    auto mArea = cardMasterRect_.withTrimmedTop(28).reduced(8, 6);
    auto topBtns = mArea.removeFromTop(28);
    const int halfBtnW = (topBtns.getWidth() - 6) / 2;
    droneHoldBtn_.setBounds(topBtns.removeFromLeft(halfBtnW));
    freezeBtn_.setBounds(topBtns.removeFromRight(halfBtnW));

    mArea.removeFromTop(8);
    const int knobW = (mArea.getWidth() - 8) / 3;
    droneLevel_.setBounds(mArea.removeFromLeft(knobW));
    mArea.removeFromLeft(4);
    droneTone_.setBounds(mArea.removeFromLeft(knobW));
    mArea.removeFromLeft(4);
    droneGlide_.setBounds(mArea);

    // Layout Key Selector & Voicings
    auto kArea = cardKeysRect_.withTrimmedTop(28).reduced(8, 6);

    // Voicing Selector at top of keys card
    auto vRow = kArea.removeFromTop(24);
    const int vBtnW = (vRow.getWidth() - 9) / 4;
    for (int v = 0; v < 4; ++v) {
        voicingBtns_[static_cast<std::size_t>(v)].setBounds(vRow.removeFromLeft(vBtnW));
        vRow.removeFromLeft(3);
    }

    kArea.removeFromTop(10);
    // Piano Keys: 7 white keys at bottom with 1px slits, 5 black keys raised on top with 3D shadows
    pianoArea_ = kArea;
    const int pianoW = pianoArea_.getWidth();
    const int whiteW = pianoW / 7;
    const int whiteH = pianoArea_.getHeight() - 3; // 3px for red velvet felt strip at keybed rail
    const int blackW = static_cast<int>(static_cast<float>(whiteW) * 0.64f);
    const int blackH = static_cast<int>(static_cast<float>(whiteH) * 0.60f);

    // White keys: C(0), D(2), E(4), F(5), G(7), A(9), B(11)
    static constexpr std::array<int, 7> kWhites = { 0, 2, 4, 5, 7, 9, 11 };
    for (int i = 0; i < 7; ++i) {
        const int keyIdx = kWhites[static_cast<std::size_t>(i)];
        const int kx = pianoArea_.getX() + i * whiteW;
        const int kw = (i == 6) ? (pianoArea_.getRight() - kx) : (whiteW - 1);
        keyBtns_[static_cast<std::size_t>(keyIdx)].setBounds(
            kx, pianoArea_.getY() + 3, kw, whiteH);
    }

    // Black keys: C#(1), D#(3), F#(6), G#(8), A#(10)
    // Positioned perfectly over the 1px seams between white keys
    static constexpr std::array<int, 5> kBlacks = { 1, 3, 6, 8, 10 };
    static constexpr std::array<int, 5> kBlackWhiteSlots = { 0, 1, 3, 4, 5 };
    for (int i = 0; i < 5; ++i) {
        const int keyIdx = kBlacks[static_cast<std::size_t>(i)];
        const int slot = kBlackWhiteSlots[static_cast<std::size_t>(i)];
        const int seamX = pianoArea_.getX() + (slot + 1) * whiteW - 1;
        keyBtns_[static_cast<std::size_t>(keyIdx)].setBounds(
            seamX - blackW / 2, pianoArea_.getY() + 3, blackW, blackH);
        keyBtns_[static_cast<std::size_t>(keyIdx)].toFront(true);
    }

    // Octave Stepper buttons in Header
    const int octY = cardKeysRect_.getY() + 5;
    const int octRight = cardKeysRect_.getRight() - 8;
    octUpBtn_.setBounds(octRight - 18, octY, 18, 18);
    octDownBtn_.setBounds(octRight - 18 - 44 - 18, octY, 18, 18);
}

} // namespace nova::synth::ui

