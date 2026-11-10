#pragma once

#include "nova_ui/NovaTheme.h"
#include "nova_dsp/synth/WorshipSynthEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include <array>

namespace nova::synth {

class NovaSynthProcessor;

class JornaCenterStageColumn : public juce::Component {
public:
    std::function<void(int)> onSelectOsc;

    JornaCenterStageColumn(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~JornaCenterStageColumn() override = default;

    void setSelectedOscIndex(int idx) noexcept;
    void pullAudioForVisualizers();

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

    static float calculateOscSample(const nova::dsp::MultiEngineOscillator::Parameters& slot, float phaseNorm) noexcept;

private:
    void dragCanvasAt(juce::Point<float> pt);

    void drawSpectrumOutput(juce::Graphics& g, const juce::Rectangle<float>& specRect);
    void drawCenterStageHeader(juce::Graphics& g, const juce::Rectangle<float>& stageRect, const nova::dsp::WorshipSynthEngine::Parameters& params);
    void drawOscWaveStage(juce::Graphics& g, const nova::dsp::WorshipSynthEngine::Parameters& params);
    void drawAllSourcesStage(juce::Graphics& g, const nova::dsp::WorshipSynthEngine::Parameters& params);
    void drawFilterStage(juce::Graphics& g, const nova::dsp::WorshipSynthEngine::Parameters& params);
    void drawMacroStage(juce::Graphics& g, const nova::dsp::WorshipSynthEngine::Parameters& params);
    void drawStageFooter(juce::Graphics& g, const juce::Rectangle<float>& stageRect, const nova::dsp::WorshipSynthEngine::Parameters& params);

    [[maybe_unused]] NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    int selectedOsc_ = 0;
    bool showSpectrumMode_ = true;
    int centerViewMode_ = 0;
    float animPhase_ = 0.0f;

    std::array<float, 512> historyWave_{};
    std::size_t writeCursor_ = 0;
    std::array<float, 32> spectrumBins_{};

    juce::Rectangle<int> specModeToggleBounds_;
    std::vector<juce::Rectangle<int>> centerTabRects_;
    std::vector<std::pair<juce::Rectangle<int>, int>> allSourcesBadges_;
    juce::Rectangle<float> canvasRect_;
    juce::Rectangle<int> holdBtnRect_;
    juce::Rectangle<int> octPillRect_;
    juce::Rectangle<int> semPillRect_;
};

} // namespace nova::synth
