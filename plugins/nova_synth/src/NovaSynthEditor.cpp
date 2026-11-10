#include "NovaSynthEditor.h"
#include "ui/NovaSynthWorkspacePage.h"
#include "MacDockIcon.h"
#include "BinaryData.h"
#include <algorithm>

namespace nova::synth {

NovaSynthEditor::NovaSynthEditor(NovaSynthProcessor& proc)
    : AudioProcessorEditor(&proc),
      processor_(proc),
      theme_(nova::ui::NovaTheme::create(nova::ui::NovaTheme::AccentPreset::SpaceCyan)),
      lookAndFeel_(theme_),
      shell_(proc, "NOVA SYNTH", "", theme_) {
    setLookAndFeel(&lookAndFeel_);
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);
    addAndMakeVisible(shell_);
    addAndMakeVisible(mixerDrawer_);

    auto appLogo = juce::ImageFileFormat::loadFrom(
        NovaResources::NovaSynth_Logo_png, static_cast<std::size_t>(NovaResources::NovaSynth_Logo_pngSize));
    if (!appLogo.isValid()) {
        appLogo = juce::ImageFileFormat::loadFrom(
            NovaResources::NovaSynth_Icon_256_png, static_cast<std::size_t>(NovaResources::NovaSynth_Icon_256_pngSize));
    }
    if (appLogo.isValid()) {
        shell_.setAppIcon(appLogo);
    }

#if JUCE_MAC
    if (juce::JUCEApplicationBase::isStandaloneApp()) {
        nova::mac::setApplicationDockIcon();
    }
#endif

    shell_.setTextScaleCallback([this](nova::ui::NovaTheme::TextScale scale) {
        theme_.setTextScale(scale);
        lookAndFeel_.setTheme(theme_);
        repaint();
    });

    auto wsPage = std::make_unique<NovaSynthWorkspacePage>(processor_, theme_);
    auto* wsPagePtr = wsPage.get();
    wsPage->onOpenMixerDrawer = [this]() { mixerDrawer_.setDrawerOpen(true); };
    onSetWorkbenchTab_ = [wsPagePtr](int tabIdx) {
        wsPagePtr->setInitialWorkbenchTab(tabIdx);
    };
    onUpdateTelemetry_ = [wsPagePtr](const nova::dsp::WorshipSynthEngine::Telemetry& tel) {
        wsPagePtr->updateModulationTelemetry(tel);
    };
    onSyncUI_ = [wsPagePtr]() {
        wsPagePtr->syncSourceStates();
    };

    const auto initParams = processor_.getCurrentParameters();
    for (int i = 0; i < 8; ++i) {
        lastOscEnabled_[static_cast<std::size_t>(i)] = initParams.oscSlots[static_cast<std::size_t>(i)].enabled;
    }
    lastFlt1Enabled_ = initParams.filter1Enabled;
    lastFlt2Enabled_ = initParams.filter2Enabled;

    shell_.addPage("WORKSPACE", std::move(wsPage));

    shell_.setMixerCallback([this]() {
        mixerDrawer_.toggleDrawer();
    });

    shell_.setPatchesCallback([]() {
        // Reserved for future left slide-over patch browser
    });

    constexpr int kMinWidth = 1000;
    constexpr int kMinHeight = 720;
    constexpr int kMaxWidth = 16384;
    constexpr int kMaxHeight = 16384;

    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* primary = displays.getPrimaryDisplay();
    const int screenW = (primary != nullptr) ? primary->userArea.getWidth() : 1440;
    const int screenH = (primary != nullptr) ? primary->userArea.getHeight() : 900;

    const int defaultW = std::clamp(1220, kMinWidth, std::max(kMinWidth, screenW - 30));
    const int defaultH = std::clamp(screenH < 880 ? screenH - 40 : 840, kMinHeight, std::max(kMinHeight, screenH - 40));

    setResizable(true, true);
    setResizeLimits(kMinWidth, kMinHeight, kMaxWidth, kMaxHeight);
    setSize(defaultW, defaultH);
    startTimerHz(30);
}

NovaSynthEditor::~NovaSynthEditor() {
    stopTimer();
    onUpdateTelemetry_ = nullptr;
    onSetWorkbenchTab_ = nullptr;
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    if (parentDocWindow_ != nullptr) {
        parentDocWindow_->setLookAndFeel(nullptr);
    }
    setLookAndFeel(nullptr);
}

void NovaSynthEditor::paint(juce::Graphics& g) {
    g.fillAll(theme_.bgCanvas);
}

void NovaSynthEditor::resized() {
    shell_.setBounds(getLocalBounds());
    mixerDrawer_.setBounds(getLocalBounds());
}

void NovaSynthEditor::setInitialWorkbenchTab(int tabIdx) {
    if (onSetWorkbenchTab_) {
        onSetWorkbenchTab_(tabIdx);
    }
}

void NovaSynthEditor::parentHierarchyChanged() {
    configureParentWindow();
}

void NovaSynthEditor::visibilityChanged() {
    if (isVisible()) {
        configureParentWindow();
        startTimerHz(30);
    } else {
        stopTimer();
    }
}

void NovaSynthEditor::configureParentWindow() {
    if (auto* docWin = findParentComponentOfClass<juce::DocumentWindow>()) {
        parentDocWindow_ = docWin;

        constexpr int kMinWidth = 1000;
        constexpr int kMinHeight = 720;
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

        // 3. Screen bounds guard on initial open: Ensure window opens with generous workstation proportions
        const auto& displays = juce::Desktop::getInstance().getDisplays();
        if (const auto* primary = displays.getPrimaryDisplay()) {
            const auto screenArea = primary->userArea;
            const auto winBounds = docWin->getBounds();

            const int defaultW = std::clamp(1220, kMinWidth, std::max(kMinWidth, screenArea.getWidth() - 30));
            const int defaultH = std::clamp(screenArea.getHeight() < 880 ? screenArea.getHeight() - 40 : 840,
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

                    shell_.setAudioSettingsCallback([docWin, btn]() {
                        if (btn != nullptr && docWin != nullptr) {
                            btn->setBounds(0, 0, 1, 1);
                            btn->triggerClick();
                            // Directly invoke Item 1 (Audio/MIDI Settings...) without requiring a secondary click
                            juce::Timer::callAfterDelay(10, []() {
                                if (auto* modal = juce::Component::getCurrentlyModalComponent()) {
                                    modal->keyPressed(juce::KeyPress(juce::KeyPress::downKey));
                                    modal->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
                                }
                            });
                        }
                    });
                    break;
                }
            }
        }
    }
}

void NovaSynthEditor::timerCallback() {
    shell_.setMixerActive(mixerDrawer_.isDrawerOpen());
    if (onUpdateTelemetry_) {
        const auto tel = processor_.getSynthTelemetry();
        onUpdateTelemetry_(tel);
    }

    const auto params = processor_.getCurrentParameters();
    bool anyChanged = false;
    for (int i = 0; i < 8; ++i) {
        const bool curEn = params.oscSlots[static_cast<std::size_t>(i)].enabled;
        if (curEn != lastOscEnabled_[static_cast<std::size_t>(i)]) {
            lastOscEnabled_[static_cast<std::size_t>(i)] = curEn;
            anyChanged = true;
        }
    }
    if (params.filter1Enabled != lastFlt1Enabled_ || params.filter2Enabled != lastFlt2Enabled_) {
        lastFlt1Enabled_ = params.filter1Enabled;
        lastFlt2Enabled_ = params.filter2Enabled;
        anyChanged = true;
    }
    if (anyChanged && onSyncUI_) {
        onSyncUI_();
    }
}

} // namespace nova::synth
