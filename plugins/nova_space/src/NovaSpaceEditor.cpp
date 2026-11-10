#include "NovaSpaceEditor.h"
#include <algorithm>
#include <cmath>

namespace nova::space {

// ==============================================================================
// ReverbSpaceVisualizer
// ==============================================================================

ReverbSpaceVisualizer::ReverbSpaceVisualizer(NovaSpaceProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc), theme_(theme) {
    for (int i = 0; i < 2; ++i) {
        engineBtns_[static_cast<std::size_t>(i)].setClickingTogglesState(false);
        addAndMakeVisible(engineBtns_[static_cast<std::size_t>(i)]);
    }
    engineBtns_[0].setButtonText("Acoustic Engine");
    engineBtns_[1].setButtonText("Lush Worship Cloud");
    engineBtns_[0].onClick = [this]() { setChoiceParam("engineMode", 0); };
    engineBtns_[1].onClick = [this]() { setChoiceParam("engineMode", 1); };

    const std::array<const char*, 4> spaceNames = { "Room", "Chamber", "Concert Hall", "Cathedral" };
    for (int i = 0; i < 4; ++i) {
        spaceBtns_[static_cast<std::size_t>(i)].setButtonText(spaceNames[static_cast<std::size_t>(i)]);
        spaceBtns_[static_cast<std::size_t>(i)].onClick = [this, i]() { setChoiceParam("spaceType", i); };
        addAndMakeVisible(spaceBtns_[static_cast<std::size_t>(i)]);
    }

    freezeBtn_.setButtonText("FREEZE CLOUD");
    freezeBtn_.setClickingTogglesState(true);
    freezeBtn_.getProperties().set("warmAccent", true);
    freezeAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc_.getAPVTS(), "freeze", freezeBtn_);
    addAndMakeVisible(freezeBtn_);
}

void ReverbSpaceVisualizer::resized() {
    auto area = getLocalBounds().reduced(12, 8);
    auto topRow = area.removeFromTop(28);
    engineBtns_[0].setBounds(topRow.removeFromLeft(140));
    topRow.removeFromLeft(6);
    engineBtns_[1].setBounds(topRow.removeFromLeft(155));
    freezeBtn_.setBounds(topRow.removeFromRight(118));

    auto bottomRow = area.removeFromBottom(24);
    const int spaceBtnW = 94;
    for (int i = 0; i < 4; ++i) {
        spaceBtns_[static_cast<std::size_t>(i)].setBounds(bottomRow.removeFromLeft(spaceBtnW));
        bottomRow.removeFromLeft(6);
    }
}

void ReverbSpaceVisualizer::mouseDrag(const juce::MouseEvent& event) {
    const auto bounds = getLocalBounds().toFloat().reduced(24.0f, 48.0f);
    if (bounds.getWidth() <= 10.0f || bounds.getHeight() <= 10.0f) return;

    const float normX = std::clamp((event.position.x - bounds.getX()) / bounds.getWidth(), 0.0f, 1.0f);
    const float normY = std::clamp(1.0f - (event.position.y - bounds.getY()) / bounds.getHeight(), 0.0f, 1.0f);

    if (auto* decayParam = proc_.getAPVTS().getParameter("decaySeconds")) {
        decayParam->setValueNotifyingHost(normX);
    }
    if (auto* sizeParam = proc_.getAPVTS().getParameter("size")) {
        sizeParam->setValueNotifyingHost(normY);
    }
}

void ReverbSpaceVisualizer::setChoiceParam(const juce::String& paramId, int choiceIndex) {
    if (auto* p = proc_.getAPVTS().getParameter(paramId)) {
        const float norm = p->convertTo0to1(static_cast<float>(choiceIndex));
        p->setValueNotifyingHost(norm);
    }
}

// ==============================================================================
// KnobRowPanel
// ==============================================================================

KnobRowPanel::KnobRowPanel(juce::AudioProcessorValueTreeState& apvts,
                           const nova::ui::NovaTheme& theme,
                           const std::vector<KnobSpec>& specs) {
    for (const auto& s : specs) {
        auto knob = std::make_unique<nova::ui::ArcKnob>(s.title, s.suffix, theme, s.secondaryAccent);
        knob->attachToParameter(apvts, s.paramId);
        addAndMakeVisible(knob.get());
        knobs_.push_back(std::move(knob));
    }
}

void KnobRowPanel::resized() {
    if (knobs_.empty()) return;
    auto area = getLocalBounds().reduced(6, 4);
    const int w = area.getWidth() / static_cast<int>(knobs_.size());
    for (auto& k : knobs_) {
        k->setBounds(area.removeFromLeft(w).reduced(4, 2));
    }
}

// ==============================================================================
// SpaceMainPage
// ==============================================================================

SpaceMainPage::SpaceMainPage(NovaSpaceProcessor& proc, const nova::ui::NovaTheme& theme)
    : visualizer_(proc, theme),
      engineDeck_("REVERB ENGINE", theme, false),
      toneOutputDeck_("SHAPING & MASTER", theme, false) {
    addAndMakeVisible(visualizer_);
    addAndMakeVisible(engineDeck_);
    addAndMakeVisible(toneOutputDeck_);

    auto& apvts = proc.getAPVTS();

    engineDeck_.addSlot("CORE SPACE", std::make_unique<KnobRowPanel>(
        apvts, theme, std::vector<KnobRowPanel::KnobSpec>{
            {"preDelayMs", "Pre-Delay", "ms", false}, {"decaySeconds", "Decay", "s", false},
            {"size", "Room Size", "", false}, {"diffusion", "Diffusion", "", false},
            {"erLevel", "Early Ref", "", false}
        }), false);

    engineDeck_.addSlot("LUSH & SHIMMER", std::make_unique<KnobRowPanel>(
        apvts, theme, std::vector<KnobRowPanel::KnobSpec>{
            {"shimmerOctave", "Octave +12", "", true}, {"shimmerFifth", "Fifth +7", "", true},
            {"modRateHz", "Mod Rate", "Hz", true}, {"modDepth", "Mod Depth", "", true},
            {"ducking", "Ducking", "", true}
        }), true);

    engineDeck_.setSlotMiniPreviewValues(0, {0.95f, 0.65f, 0.42f, 0.25f, 0.12f, 0.04f});
    engineDeck_.setSlotMiniPreviewValues(1, {0.25f, 0.55f, 0.85f, 0.65f, 0.90f, 0.70f});

    toneOutputDeck_.addSlot("TONE & VOCAL CARVE", std::make_unique<KnobRowPanel>(
        apvts, theme, std::vector<KnobRowPanel::KnobSpec>{
            {"lowCutHz", "Low Cut", "Hz", false}, {"highCutHz", "High Cut", "Hz", false},
            {"highDamping", "HF Damping", "", false}, {"bassMultiplier", "Bass Ratio", "x", false},
            {"vocalCarve", "Vocal Carve", "", true}
        }), false);

    toneOutputDeck_.addSlot("OUTPUT & STAGE", std::make_unique<KnobRowPanel>(
        apvts, theme, std::vector<KnobRowPanel::KnobSpec>{
            {"ducking", "Auto Duck", "", true}, {"stereoWidth", "Stereo Width", "", false},
            {"mix", "Dry / Wet", "", false}, {"outputGainDb", "Output Gain", "dB", false}
        }), false);

    toneOutputDeck_.setSlotMiniPreviewValues(0, {0.20f, 0.85f, 0.80f, 0.45f, 0.78f, 0.30f});
    toneOutputDeck_.setSlotMiniPreviewValues(1, {0.40f, 0.75f, 0.75f, 0.75f, 0.75f});
}

void SpaceMainPage::resized() {
    auto area = getLocalBounds();
    visualizer_.setBounds(area.removeFromTop(215));
    area.removeFromTop(10);
    const int halfH = (area.getHeight() - 10) / 2;
    engineDeck_.setBounds(area.removeFromTop(halfH));
    area.removeFromTop(10);
    toneOutputDeck_.setBounds(area);
}

// ==============================================================================
// ToneEqPage
// ==============================================================================

ToneEqPage::ToneEqPage(NovaSpaceProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc), theme_(theme), eqControlsDeck_("EQ, DAMPING & DUCKING", theme, false) {
    addAndMakeVisible(eqControlsDeck_);
    auto& apvts = proc_.getAPVTS();
    eqControlsDeck_.addSlot("FREQUENCY SCULPTING", std::make_unique<KnobRowPanel>(
        apvts, theme, std::vector<KnobRowPanel::KnobSpec>{
            {"lowCutHz", "Low Cut", "Hz", false}, {"vocalCarve", "Vocal Carve", "", true},
            {"highCutHz", "High Cut", "Hz", false}, {"highDamping", "HF Damping", "", false},
            {"bassMultiplier", "Bass RT60", "x", false}, {"ducking", "Ducking", "", true},
            {"mix", "Dry / Wet", "", false}
        }), false);
}

void ToneEqPage::resized() {
    auto area = getLocalBounds();
    area.removeFromTop(255);
    eqControlsDeck_.setBounds(area);
}

// ==============================================================================
// NovaSpaceEditor
// ==============================================================================

NovaSpaceEditor::NovaSpaceEditor(NovaSpaceProcessor& proc)
    : AudioProcessorEditor(&proc),
      processor_(proc),
      theme_(nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan)),
      lookAndFeel_(theme_),
      shell_(proc, "NOVA SPACE", "Dual-Engine Reverb", theme_) {
    setLookAndFeel(&lookAndFeel_);
    addAndMakeVisible(shell_);

    shell_.addPage("SPACE", std::make_unique<SpaceMainPage>(processor_, theme_));
    shell_.addPage("TONE & EQ", std::make_unique<ToneEqPage>(processor_, theme_));

    constexpr int kMinWidth = 780, kMinHeight = 540, kMaxWidth = 16384, kMaxHeight = 16384;

    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* primary = displays.getPrimaryDisplay();
    const int screenW = (primary != nullptr) ? primary->userArea.getWidth() : 1440;
    const int screenH = (primary != nullptr) ? primary->userArea.getHeight() : 900;
    const int defaultW = std::clamp(920, kMinWidth, std::max(kMinWidth, screenW - 40));
    const int defaultH = std::clamp(screenH < 720 ? screenH - 50 : 620, kMinHeight, std::max(kMinHeight, screenH - 50));

    setResizable(true, true);
    setResizeLimits(kMinWidth, kMinHeight, kMaxWidth, kMaxHeight);
    setSize(defaultW, defaultH);
    startTimerHz(30);
}

NovaSpaceEditor::~NovaSpaceEditor() {
    stopTimer();
    if (parentDocWindow_ != nullptr) {
        parentDocWindow_->setLookAndFeel(nullptr);
    }
    setLookAndFeel(nullptr);
}

void NovaSpaceEditor::resized() {
    shell_.setBounds(getLocalBounds());
}

void NovaSpaceEditor::parentHierarchyChanged() {
    configureParentWindow();
}

void NovaSpaceEditor::visibilityChanged() {
    if (isVisible()) {
        configureParentWindow();
    }
}

void NovaSpaceEditor::configureParentWindow() {
    if (auto* docWin = findParentComponentOfClass<juce::DocumentWindow>()) {
        parentDocWindow_ = docWin;
        constexpr int kMinWidth = 780, kMinHeight = 540, kMaxWidth = 16384, kMaxHeight = 16384;

        if (!docWin->isUsingNativeTitleBar()) {
            docWin->setUsingNativeTitleBar(true);
            docWin->setTitleBarButtonsRequired(juce::DocumentWindow::allButtons, true);
        }

        docWin->setLookAndFeel(&lookAndFeel_);

        if (auto* c = docWin->getConstrainer()) {
            c->setMinimumSize(kMinWidth, kMinHeight);
            c->setMaximumSize(kMaxWidth, kMaxHeight);
        }
        if (auto* peer = docWin->getPeer()) {
            peer->setConstrainer(docWin->getConstrainer());
        }

        const auto& displays = juce::Desktop::getInstance().getDisplays();
        if (const auto* primary = displays.getPrimaryDisplay()) {
            const auto screenArea = primary->userArea;
            const auto winBounds = docWin->getBounds();
            const int defaultW = std::clamp(1020, kMinWidth, std::max(kMinWidth, screenArea.getWidth() - 30));
            const int defaultH = std::clamp(screenArea.getHeight() < 800 ? screenArea.getHeight() - 40 : 700,
                                            kMinHeight, std::max(kMinHeight, screenArea.getHeight() - 40));

            if (winBounds.getWidth() < kMinWidth || winBounds.getHeight() < kMinHeight) {
                const int targetW = std::max(winBounds.getWidth(), defaultW);
                const int targetH = std::max(winBounds.getHeight(), defaultH);
                const int targetX = screenArea.getX() + std::max(0, (screenArea.getWidth() - targetW) / 2);
                const int targetY = screenArea.getY() + std::max(0, (screenArea.getHeight() - targetH) / 2);
                docWin->setBounds(targetX, targetY, targetW, targetH);
            }
        }

        for (int i = 0; i < docWin->getNumChildComponents(); ++i) {
            auto* child = docWin->getChildComponent(i);
            if (auto* btn = dynamic_cast<juce::Button*>(child)) {
                if (btn->getButtonText().containsIgnoreCase("Options")) {
                    standaloneOptionsButton_ = btn;
                    btn->setVisible(false);
                    btn->setBounds(0, 0, 0, 0);

                    shell_.setAudioSettingsCallback([this, docWin, btn]() {
                        if (btn != nullptr && docWin != nullptr) {
                            auto gearBounds = shell_.getSettingsButtonScreenBounds();
                            auto localInDoc = docWin->getLocalArea(nullptr, gearBounds);
                            btn->setBounds(localInDoc);
                            btn->triggerClick();
                        }
                    });
                    break;
                }
            }
        }
    }
}

void NovaSpaceEditor::timerCallback() {
    repaint();
}

} // namespace nova::space
