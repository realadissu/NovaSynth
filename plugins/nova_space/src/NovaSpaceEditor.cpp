#include "NovaSpaceEditor.h"
#include <cmath>
#include <algorithm>
#include <array>

namespace nova::space {

namespace {

/**
 * @brief Interactive 60-FPS Reverb Space, Early Reflections & 16-Channel FDN Tank Visualizer.
 * Supports dragging horizontally to adjust Decay Time and vertically to adjust Room Size.
 */
class ReverbSpaceVisualizer : public juce::Component {
public:
    ReverbSpaceVisualizer(NovaSpaceProcessor& proc, const nova::ui::NovaTheme& theme)
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

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(theme_.bgSurface);
        g.fillRoundedRectangle(bounds, theme_.cornerRadius);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(bounds, theme_.cornerRadius, 1.0f);

        const auto params = proc_.getCurrentParameters();
        const auto tel = proc_.getReverbTelemetry();
        const bool isLush = (params.engineMode == nova::dsp::DualEngineReverb::EngineMode::LushCloud);
        const auto activeAccent = isLush ? theme_.accentSecondary : theme_.accentPrimary;

        // Update toggle states visually
        engineBtns_[0].setToggleState(!isLush, juce::dontSendNotification);
        engineBtns_[1].setToggleState(isLush, juce::dontSendNotification);
        for (int i = 0; i < 4; ++i) {
            spaceBtns_[static_cast<std::size_t>(i)].setToggleState(
                static_cast<int>(params.spaceType) == i, juce::dontSendNotification);
        }

        // Graph plotting area
        const auto graphArea = bounds.withTrimmedTop(44.0f).withTrimmedBottom(38.0f).reduced(14.0f, 6.0f);
        g.setColour(theme_.bgControl);
        g.fillRoundedRectangle(graphArea, 6.0f);
        g.setColour(theme_.borderSubtle.withAlpha(0.7f));
        g.drawRoundedRectangle(graphArea, 6.0f, 1.0f);

        // Subtle grid lines
        g.setColour(theme_.borderSubtle.withAlpha(0.35f));
        for (int i = 1; i < 4; ++i) {
            const float gx = graphArea.getX() + graphArea.getWidth() * (static_cast<float>(i) * 0.25f);
            g.drawVerticalLine(static_cast<int>(gx), graphArea.getY(), graphArea.getBottom());
        }

        // 1. Pre-delay offset + 12 Early Reflection vertical stems
        const float preNorm = std::clamp(params.preDelayMs / 250.0f, 0.0f, 1.0f) * 0.18f;
        const float erStartX = graphArea.getX() + 12.0f + preNorm * graphArea.getWidth();
        const float erWidth = graphArea.getWidth() * 0.22f * std::clamp(params.size, 0.3f, 1.3f);

        g.setColour(theme_.accentPrimary.withAlpha(0.55f));
        for (int t = 0; t < 8; ++t) {
            const float tx = erStartX + (static_cast<float>(t) / 8.0f) * erWidth;
            const float th = graphArea.getHeight() * (0.68f - static_cast<float>(t) * 0.06f) * params.erLevel;
            g.drawLine(tx, graphArea.getBottom() - 4.0f, tx, graphArea.getBottom() - 4.0f - th, 1.8f);
        }

        // 2. Late FDN Decay Envelope Curve
        const float decayNorm = std::clamp(std::log10(params.decaySeconds + 0.8f) / std::log10(30.8f), 0.08f, 1.0f);
        const float tailStartX = erStartX + erWidth * 0.35f;
        const float tailEndX = tailStartX + (graphArea.getRight() - tailStartX - 8.0f) * decayNorm;

        juce::Path envPath;
        envPath.startNewSubPath(tailStartX, graphArea.getBottom() - 4.0f);
        constexpr int kCurvePts = 48;
        for (int i = 0; i <= kCurvePts; ++i) {
            const float u = static_cast<float>(i) / static_cast<float>(kCurvePts);
            const float x = tailStartX + u * (tailEndX - tailStartX);
            const float env = params.freeze
                ? (0.78f + 0.06f * std::sin(u * 6.28f + animPhase_))
                : std::exp(-3.4f * u * (1.0f + params.highDamping * 0.45f));
            const float shimmerRipple = isLush
                ? (0.04f * params.shimmerOctave * std::sin(u * 18.0f - animPhase_ * 2.5f))
                : 0.0f;
            const float y = graphArea.getBottom() - 6.0f -
                            std::clamp(env + shimmerRipple, 0.0f, 0.95f) * (graphArea.getHeight() - 18.0f);
            envPath.lineTo(x, y);
        }
        envPath.lineTo(tailEndX, graphArea.getBottom() - 4.0f);
        envPath.closeSubPath();

        juce::ColourGradient grad(activeAccent.withAlpha(0.32f), tailStartX, graphArea.getY(),
                                  activeAccent.withAlpha(0.03f), tailStartX, graphArea.getBottom(), false);
        g.setGradientFill(grad);
        g.fillPath(envPath);

        g.setColour(activeAccent);
        g.strokePath(envPath, juce::PathStrokeType(2.0f));

        // 3. Live 16-Channel FDN Tank Energy Bars along bottom of visualizer
        const float barRegionW = graphArea.getWidth() * 0.42f;
        const float barStartX = graphArea.getRight() - barRegionW - 10.0f;
        const float barStep = barRegionW / 16.0f;
        for (int ch = 0; ch < 16; ++ch) {
            const float e = std::clamp(tel.channelEnergies[static_cast<std::size_t>(ch)] * 3.5f, 0.02f, 1.0f);
            const float bh = e * (graphArea.getHeight() * 0.55f);
            const float bx = barStartX + static_cast<float>(ch) * barStep;
            g.setColour(activeAccent.withAlpha(0.45f + 0.45f * e));
            g.fillRoundedRectangle(bx, graphArea.getBottom() - 6.0f - bh, barStep - 2.0f, bh, 1.5f);
        }

        // Overlay readout text (Decay / Size / Ducking GR)
        g.setFont(theme_.getValueFont(13.0f));
        g.setColour(theme_.textPrimary);
        const juce::String readout = juce::String("RT60: ") + juce::String(params.decaySeconds, 2) + " s   |   "
                                   + "Size: " + juce::String(static_cast<int>(params.size * 100.0f)) + "%   |   "
                                   + "Pre-Delay: " + juce::String(params.preDelayMs, 1) + " ms";
        g.drawText(readout, graphArea.toNearestInt().reduced(12, 8), juce::Justification::topLeft, false);

        if (tel.duckingGainReductionDb < -0.2f) {
            g.setColour(theme_.accentWarm);
            g.drawText("Ducking: " + juce::String(tel.duckingGainReductionDb, 1) + " dB",
                       graphArea.toNearestInt().reduced(12, 8), juce::Justification::topRight, false);
        }

        animPhase_ += 0.08f;
    }

    void resized() override {
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

    void mouseDrag(const juce::MouseEvent& event) override {
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

private:
    void setChoiceParam(const juce::String& paramId, int choiceIndex) {
        if (auto* p = proc_.getAPVTS().getParameter(paramId)) {
            const float norm = p->convertTo0to1(static_cast<float>(choiceIndex));
            p->setValueNotifyingHost(norm);
        }
    }

    NovaSpaceProcessor& proc_;
    nova::ui::NovaTheme theme_;
    std::array<juce::TextButton, 2> engineBtns_;
    std::array<juce::TextButton, 4> spaceBtns_;
    juce::TextButton freezeBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttach_;
    float animPhase_ = 0.0f;
};

/**
 * @brief Horizontal row of 5 ArcKnobs used inside a JORNA SectionDeck slot.
 */
class KnobRowPanel : public juce::Component {
public:
    struct KnobSpec {
        juce::String paramId;
        juce::String title;
        juce::String suffix;
        bool secondaryAccent = false;
    };

    KnobRowPanel(juce::AudioProcessorValueTreeState& apvts,
                 const nova::ui::NovaTheme& theme,
                 const std::vector<KnobSpec>& specs) {
        for (const auto& s : specs) {
            auto knob = std::make_unique<nova::ui::ArcKnob>(s.title, s.suffix, theme, s.secondaryAccent);
            knob->attachToParameter(apvts, s.paramId);
            addAndMakeVisible(knob.get());
            knobs_.push_back(std::move(knob));
        }
    }

    void resized() override {
        if (knobs_.empty()) return;
        auto area = getLocalBounds().reduced(6, 4);
        const int w = area.getWidth() / static_cast<int>(knobs_.size());
        for (auto& k : knobs_) {
            k->setBounds(area.removeFromLeft(w).reduced(4, 2));
        }
    }

private:
    std::vector<std::unique_ptr<nova::ui::ArcKnob>> knobs_;
};

/**
 * @brief Main "SPACE" Workspace Page combining the Visualizer and JORNA multi-slot SectionDecks.
 */
class SpaceMainPage : public juce::Component {
public:
    SpaceMainPage(NovaSpaceProcessor& proc, const nova::ui::NovaTheme& theme)
        : visualizer_(proc, theme),
          engineDeck_("REVERB ENGINE", theme, false),
          toneOutputDeck_("SHAPING & MASTER", theme, false) {
        addAndMakeVisible(visualizer_);
        addAndMakeVisible(engineDeck_);
        addAndMakeVisible(toneOutputDeck_);

        auto& apvts = proc.getAPVTS();

        engineDeck_.addSlot("CORE SPACE", std::make_unique<KnobRowPanel>(
            apvts, theme, std::vector<KnobRowPanel::KnobSpec>{
                {"preDelayMs",   "Pre-Delay", "ms", false},
                {"decaySeconds", "Decay",     "s",  false},
                {"size",         "Room Size", "",   false},
                {"diffusion",    "Diffusion", "",   false},
                {"erLevel",      "Early Ref", "",   false}
            }), false);

        engineDeck_.addSlot("LUSH & SHIMMER", std::make_unique<KnobRowPanel>(
            apvts, theme, std::vector<KnobRowPanel::KnobSpec>{
                {"shimmerOctave", "Octave +12", "",   true},
                {"shimmerFifth",  "Fifth +7",   "",   true},
                {"modRateHz",     "Mod Rate",   "Hz", true},
                {"modDepth",      "Mod Depth",  "",   true},
                {"ducking",       "Ducking",    "",   true}
            }), true);

        engineDeck_.setSlotMiniPreviewValues(0, {0.95f, 0.65f, 0.42f, 0.25f, 0.12f, 0.04f});
        engineDeck_.setSlotMiniPreviewValues(1, {0.25f, 0.55f, 0.85f, 0.65f, 0.90f, 0.70f});

        toneOutputDeck_.addSlot("TONE & VOCAL CARVE", std::make_unique<KnobRowPanel>(
            apvts, theme, std::vector<KnobRowPanel::KnobSpec>{
                {"lowCutHz",       "Low Cut",     "Hz", false},
                {"highCutHz",      "High Cut",    "Hz", false},
                {"highDamping",    "HF Damping",  "",   false},
                {"bassMultiplier", "Bass Ratio",  "x",  false},
                {"vocalCarve",     "Vocal Carve", "",   true}
            }), false);

        toneOutputDeck_.addSlot("OUTPUT & STAGE", std::make_unique<KnobRowPanel>(
            apvts, theme, std::vector<KnobRowPanel::KnobSpec>{
                {"ducking",      "Auto Duck",    "",   true},
                {"stereoWidth",  "Stereo Width", "",   false},
                {"mix",          "Dry / Wet",    "",   false},
                {"outputGainDb", "Output Gain",  "dB", false}
            }), false);

        toneOutputDeck_.setSlotMiniPreviewValues(0, {0.20f, 0.85f, 0.80f, 0.45f, 0.78f, 0.30f});
        toneOutputDeck_.setSlotMiniPreviewValues(1, {0.40f, 0.75f, 0.75f, 0.75f, 0.75f});
    }

    void resized() override {
        auto area = getLocalBounds();
        visualizer_.setBounds(area.removeFromTop(215));
        area.removeFromTop(10);

        const int halfH = (area.getHeight() - 10) / 2;
        engineDeck_.setBounds(area.removeFromTop(halfH));
        area.removeFromTop(10);
        toneOutputDeck_.setBounds(area);
    }

private:
    ReverbSpaceVisualizer visualizer_;
    nova::ui::SectionDeck engineDeck_;
    nova::ui::SectionDeck toneOutputDeck_;
};

/**
 * @brief Secondary "TONE & EQ" Page showing interactive Vocal-Carve & Damping EQ response curve.
 */
class ToneEqPage : public juce::Component {
public:
    ToneEqPage(NovaSpaceProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc), theme_(theme),
          eqControlsDeck_("EQ, DAMPING & DUCKING", theme, false) {
        addAndMakeVisible(eqControlsDeck_);

        auto& apvts = proc_.getAPVTS();
        eqControlsDeck_.addSlot("FREQUENCY SCULPTING", std::make_unique<KnobRowPanel>(
            apvts, theme, std::vector<KnobRowPanel::KnobSpec>{
                {"lowCutHz",       "Low Cut",     "Hz", false},
                {"vocalCarve",     "Vocal Carve", "",   true},
                {"highCutHz",      "High Cut",    "Hz", false},
                {"highDamping",    "HF Damping",  "",   false},
                {"bassMultiplier", "Bass RT60",   "x",  false},
                {"ducking",        "Ducking",     "",   true},
                {"mix",            "Dry / Wet",   "",   false}
            }), false);
    }

    void paint(juce::Graphics& g) override {
        auto eqBox = getLocalBounds().removeFromTop(245).toFloat().reduced(1.0f);
        g.setColour(theme_.bgSurface);
        g.fillRoundedRectangle(eqBox, theme_.cornerRadius);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(eqBox, theme_.cornerRadius, 1.0f);

        const auto plot = eqBox.reduced(18.0f, 22.0f);
        g.setColour(theme_.bgControl);
        g.fillRoundedRectangle(plot, 6.0f);

        const auto p = proc_.getCurrentParameters();
        nova::dsp::StateVariableFilter hp, lp, carve;
        hp.prepare(48000.0);
        lp.prepare(48000.0);
        carve.prepare(48000.0);
        hp.setParameters(nova::dsp::StateVariableFilter::Type::HighPass, p.lowCutHz, 0.707f);
        lp.setParameters(nova::dsp::StateVariableFilter::Type::LowPass, p.highCutHz, 0.707f);
        carve.setParameters(nova::dsp::StateVariableFilter::Type::Bell, 2200.0f, 0.85f, -9.5f * p.vocalCarve);

        juce::Path curve;
        constexpr int kSteps = 96;
        for (int i = 0; i <= kSteps; ++i) {
            const float normX = static_cast<float>(i) / static_cast<float>(kSteps);
            const float freq = 20.0f * std::pow(1000.0f, normX); // 20 Hz .. 20 kHz
            const float mag = hp.getMagnitudeAtFrequency(freq)
                            * lp.getMagnitudeAtFrequency(freq)
                            * carve.getMagnitudeAtFrequency(freq);
            const float db = 20.0f * std::log10(std::max(0.05f, mag));
            const float normY = std::clamp(0.32f - (db / 28.0f), 0.05f, 0.95f);

            const float px = plot.getX() + normX * plot.getWidth();
            const float py = plot.getY() + normY * plot.getHeight();
            if (i == 0) curve.startNewSubPath(px, py);
            else        curve.lineTo(px, py);
        }

        g.setColour(theme_.accentPrimary);
        g.strokePath(curve, juce::PathStrokeType(2.2f));

        g.setFont(theme_.getSectionFont(13.0f));
        g.setColour(theme_.textSecondary);
        g.drawText("VOCAL CARVE & DAMPING SPECTRUM (20 Hz - 20 kHz)",
                   plot.toNearestInt().reduced(12, 8), juce::Justification::topLeft, false);
    }

    void resized() override {
        auto area = getLocalBounds();
        area.removeFromTop(255);
        eqControlsDeck_.setBounds(area);
    }

private:
    NovaSpaceProcessor& proc_;
    nova::ui::NovaTheme theme_;
    nova::ui::SectionDeck eqControlsDeck_;
};

} // namespace

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

    constexpr int kMinWidth = 780;
    constexpr int kMinHeight = 540;
    constexpr int kMaxWidth = 16384;
    constexpr int kMaxHeight = 16384;

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

void NovaSpaceEditor::paint(juce::Graphics& g) {
    g.fillAll(theme_.bgCanvas);
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

        constexpr int kMinWidth = 780;
        constexpr int kMinHeight = 540;
        constexpr int kMaxWidth = 16384;
        constexpr int kMaxHeight = 16384;

        // 1. Enable Native OS Title Bar (Traffic lights on macOS, DWM on Windows, WM decorations on Linux)
        if (!docWin->isUsingNativeTitleBar()) {
            docWin->setUsingNativeTitleBar(true);
            docWin->setTitleBarButtonsRequired(juce::DocumentWindow::allButtons, true);
        }

        // Apply our sleek dark LookAndFeel to parent window popups as well
        docWin->setLookAndFeel(&lookAndFeel_);

        // 2. Minimum size enforcement with unrestricted maximum size across Standalone parent window and native peer
        if (auto* c = docWin->getConstrainer()) {
            c->setMinimumSize(kMinWidth, kMinHeight);
            c->setMaximumSize(kMaxWidth, kMaxHeight);
        }
        if (auto* peer = docWin->getPeer()) {
            peer->setConstrainer(docWin->getConstrainer());
        }

        // 3. Screen bounds guard on initial open: Ensure window opens with generous proportions
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

        // 4. Hide standalone JUCE "Options" button and route into PluginShell's settings gear
        for (int i = 0; i < docWin->getNumChildComponents(); ++i) {
            auto* child = docWin->getChildComponent(i);
            if (auto* btn = dynamic_cast<juce::Button*>(child)) {
                if (btn->getButtonText().containsIgnoreCase("Options")) {
                    standaloneOptionsButton_ = btn;
                    btn->setVisible(false);
                    btn->setBounds(0, 0, 0, 0);

                    shell_.setAudioSettingsCallback([this, docWin, btn]() {
                        if (btn != nullptr && docWin != nullptr) {
                            // Anchor JUCE standalone popup directly under our sleek settings gear icon
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
