#pragma once

#include "nova_plugin/NovaProcessorBase.h"
#include "nova_dsp/synth/WorshipSynthEngine.h"
#include "nova_dsp/math/LockFreeRingBuffer.h"

namespace nova::synth {

class NovaSynthProcessor : public nova::plugin::NovaProcessorBase {
public:
    NovaSynthProcessor();
    ~NovaSynthProcessor() override = default;

    bool acceptsMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 8.0; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    void onPresetLoaded() override {
        presetChangePending_.store(true, std::memory_order_release);
    }

    [[nodiscard]] nova::dsp::ModMatrix& getModMatrix() noexcept {
        return synthEngine_.getModMatrix();
    }
    [[nodiscard]] const nova::dsp::ModMatrix& getModMatrix() const noexcept {
        return synthEngine_.getModMatrix();
    }
    [[nodiscard]] nova::dsp::WorshipSynthEngine& getEngine() noexcept {
        return synthEngine_;
    }
    [[nodiscard]] const nova::dsp::WorshipSynthEngine& getEngine() const noexcept {
        return synthEngine_;
    }

    void triggerDrone(bool triggered = true) noexcept {
        synthEngine_.triggerDrone(triggered);
    }
    [[nodiscard]] bool isDroneTriggered() const noexcept {
        return synthEngine_.isDroneTriggered();
    }

    [[nodiscard]] nova::dsp::WorshipSynthEngine::Telemetry getSynthTelemetry() const noexcept {
        return synthEngine_.getTelemetry();
    }

    [[nodiscard]] nova::dsp::WorshipSynthEngine::Parameters getCurrentParameters() const noexcept {
        return currentParams_;
    }

    [[nodiscard]] double getCurrentBpm() const noexcept { return currentBpm_; }
    [[nodiscard]] bool isHostBpmSynced() const noexcept { return isHostBpmSynced_; }
    void setInternalBpm(double bpm) noexcept { currentBpm_ = std::clamp(bpm, 20.0, 300.0); }

    std::size_t popVisualizerSamples(float* dest, std::size_t maxCount) noexcept {
        return synthEngine_.popVisualizerSamples(dest, maxCount);
    }

    void triggerAuditionNoteOn(int midiNote, float velocity = 0.82f) noexcept;
    void triggerAuditionNoteOff(int midiNote) noexcept;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void syncParametersFromAPVTS() noexcept;

    void setSlotSolo(int slotIndex, bool isSoloed) noexcept {
        if (slotIndex < 0 || slotIndex >= 8) return;
        uint8_t cur = soloMask_.load(std::memory_order_relaxed);
        if (isSoloed) cur |= static_cast<uint8_t>(1 << slotIndex);
        else cur &= static_cast<uint8_t>(~(1 << slotIndex));
        soloMask_.store(cur, std::memory_order_release);
        syncParametersFromAPVTS();
    }
    [[nodiscard]] bool isSlotSoloed(int slotIndex) const noexcept {
        if (slotIndex < 0 || slotIndex >= 8) return false;
        return (soloMask_.load(std::memory_order_relaxed) & (1 << slotIndex)) != 0;
    }
    [[nodiscard]] bool hasAnySolo() const noexcept {
        return soloMask_.load(std::memory_order_relaxed) != 0;
    }
    [[nodiscard]] uint8_t getSoloMask() const noexcept {
        return soloMask_.load(std::memory_order_relaxed);
    }
    void clearAllSolos() noexcept {
        soloMask_.store(0, std::memory_order_release);
        syncParametersFromAPVTS();
    }

    // Mixer Audio Channel Mute (independent of oscillator enabled/power state)
    void setSlotMute(int slotIndex, bool isMuted) noexcept {
        if (slotIndex < 0 || slotIndex >= 8) return;
        uint8_t cur = muteMask_.load(std::memory_order_relaxed);
        if (isMuted) cur |= static_cast<uint8_t>(1 << slotIndex);
        else cur &= static_cast<uint8_t>(~(1 << slotIndex));
        muteMask_.store(cur, std::memory_order_release);
        syncParametersFromAPVTS();
    }
    [[nodiscard]] bool isSlotMuted(int slotIndex) const noexcept {
        if (slotIndex < 0 || slotIndex >= 8) return false;
        return (muteMask_.load(std::memory_order_relaxed) & (1 << slotIndex)) != 0;
    }
    [[nodiscard]] bool hasAnyMute() const noexcept {
        return muteMask_.load(std::memory_order_relaxed) != 0;
    }
    [[nodiscard]] uint8_t getMuteMask() const noexcept {
        return muteMask_.load(std::memory_order_relaxed);
    }
    void clearAllMutes() noexcept {
        muteMask_.store(0, std::memory_order_release);
        syncParametersFromAPVTS();
    }

    // Arpeggiator Step Sequencer Accessors & Presets
    struct ArpRhythmPreset {
        juce::String name;
        int length = 16;
        std::vector<bool> gates;
        std::vector<float> velocities;
        bool isUser = false;
    };

    [[nodiscard]] int getArpPatternLength() const noexcept;
    void setArpPatternLength(int length);
    void applyArpRhythmPattern(int length, const std::vector<bool>& gates, const std::vector<float>& velocities);
    [[nodiscard]] bool isArpStepGateActive(int step) const noexcept;
    void setArpStepGate(int step, bool active);
    void toggleArpStepGate(int step);
    [[nodiscard]] float getArpStepVelocity(int step) const noexcept;
    void setArpStepVelocity(int step, float velocity);

    [[nodiscard]] std::vector<ArpRhythmPreset> getAvailableArpRhythmPresets() const;
    bool saveUserArpRhythmPreset(const juce::String& name, int length, const std::vector<bool>& gates, const std::vector<float>& velocities);
    bool deleteUserArpRhythmPreset(const juce::String& name);
    [[nodiscard]] juce::File getUserArpRhythmPresetsFile() const;

    // Audio Engine Panic (Reset Sound & Hang Free)
    void triggerPanic() noexcept override;

    // Mixer Peak Metering Accessors
    [[nodiscard]] float getMasterPeakL() const noexcept { return masterPeakL_.load(std::memory_order_relaxed); }
    [[nodiscard]] float getMasterPeakR() const noexcept { return masterPeakR_.load(std::memory_order_relaxed); }
    [[nodiscard]] float getSourcePeak(int slotIndex) const noexcept {
        if (slotIndex >= 0 && slotIndex < 8) return sourcePeaks_[static_cast<std::size_t>(slotIndex)].load(std::memory_order_relaxed);
        return 0.0f;
    }

    // Sustain Pedal Polarity Inversion & Master Tuning (A4 Hz)
    [[nodiscard]] bool isInvertSustainPedal() const noexcept;
    void setInvertSustainPedal(bool inverted);
    [[nodiscard]] float getMasterTuningHz() const noexcept;
    void setMasterTuningHz(float hz);

private:
    void registerFactoryPresets();
    void applyHostTuningHzFromMidi(float tuningHz) noexcept;

    nova::dsp::WorshipSynthEngine synthEngine_;
    nova::dsp::WorshipSynthEngine::Parameters currentParams_{};
    std::atomic<uint8_t> soloMask_{0};
    std::atomic<uint8_t> muteMask_{0};
    std::atomic<bool> presetChangePending_{false};
    std::atomic<bool> panicPending_{false};
    std::atomic<bool> arpPatternResetPending_{false};
    double currentSampleRate_ = 48000.0;
    double currentBpm_ = 120.0;
    bool isHostBpmSynced_ = false;

    // Mixer Live Metering State
    std::atomic<float> masterPeakL_{0.0f};
    std::atomic<float> masterPeakR_{0.0f};
    std::array<std::atomic<float>, 8> sourcePeaks_{};

    // Sustain Pedal, Master Tuning & Arpeggiator APVTS Pointers for lock-free audio thread sync
    std::atomic<float>* invertSustainPedalParam_ = nullptr;
    std::atomic<float>* masterTuningHzParam_ = nullptr;
    int rpnMsb_ = 127;
    int rpnLsb_ = 127;
    int dataEntryMsb_ = 64;
    std::atomic<float>* arpPatternLengthParam_ = nullptr;
    std::array<std::atomic<float>*, 64> arpStepGateParams_{};
    std::array<std::atomic<float>*, 64> arpStepVelParams_{};

    struct PendingNoteEvent {
        int note = 60;
        float velocity = 0.8f;
        bool isNoteOn = true;
    };
    nova::dsp::LockFreeRingBuffer<PendingNoteEvent, 64> uiNoteQueue_;
};

} // namespace nova::synth
