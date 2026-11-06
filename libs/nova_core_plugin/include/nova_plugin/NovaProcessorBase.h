#pragma once

#include "nova_plugin/PresetManager.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

namespace nova::plugin {

/**
 * @brief Base AudioProcessor class shared across Nova Space, Nova Synth, and Nova Piano.
 * Provides APVTS management, UndoManager, PresetManager, A/B comparison, and state serialization.
 */
class NovaProcessorBase : public juce::AudioProcessor {
public:
    NovaProcessorBase(const BusesProperties& ioLayouts,
                      juce::AudioProcessorValueTreeState::ParameterLayout parameterLayout,
                      const juce::String& pluginName);
    ~NovaProcessorBase() override = default;

    // Standard AudioProcessor overrides
    const juce::String getName() const override { return pluginName_; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    virtual void onPresetLoaded() {}

    [[nodiscard]] juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts_; }
    [[nodiscard]] const juce::AudioProcessorValueTreeState& getAPVTS() const noexcept { return apvts_; }
    [[nodiscard]] juce::UndoManager& getUndoManager() noexcept { return undoManager_; }
    [[nodiscard]] PresetManager& getPresetManager() noexcept { return presetManager_; }
    [[nodiscard]] const PresetManager& getPresetManager() const noexcept { return presetManager_; }

    [[nodiscard]] float getInputPeakLevel() const noexcept { return inputPeak_.load(std::memory_order_relaxed); }
    [[nodiscard]] float getOutputPeakLevel() const noexcept { return outputPeak_.load(std::memory_order_relaxed); }
    [[nodiscard]] float getCpuLoadPercent() const noexcept { return cpuLoadPercent_.load(std::memory_order_relaxed); }

    [[nodiscard]] double getBpm() const noexcept { return bpm_.load(std::memory_order_relaxed); }
    [[nodiscard]] bool isHostBpmSynced() const noexcept { return hostBpmSynced_.load(std::memory_order_relaxed); }
    void setBpm(double bpm, bool syncedToHost = false) noexcept {
        bpm_.store(std::clamp(bpm, 20.0, 300.0), std::memory_order_relaxed);
        hostBpmSynced_.store(syncedToHost, std::memory_order_relaxed);
    }

    virtual void triggerPanic() noexcept {}

protected:
    void updateMetersAndCpu(float inPeak, float outPeak, double blockDurationSec, double elapsedSec) noexcept;

    juce::String pluginName_;
    juce::UndoManager undoManager_;
    juce::AudioProcessorValueTreeState apvts_;
    PresetManager presetManager_;

    std::atomic<float> inputPeak_{0.0f};
    std::atomic<float> outputPeak_{0.0f};
    std::atomic<float> cpuLoadPercent_{1.2f};
    std::atomic<double> bpm_{120.0};
    std::atomic<bool> hostBpmSynced_{false};
};

} // namespace nova::plugin
