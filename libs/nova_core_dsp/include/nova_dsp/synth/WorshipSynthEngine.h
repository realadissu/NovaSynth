#pragma once

#include "nova_dsp/oscillators/MultiEngineOscillator.h"
#include "nova_dsp/modulation/AdsrEnvelope.h"
#include "nova_dsp/modulation/AnalogDriftGenerator.h"
#include "nova_dsp/modulation/LfoGenerator.h"
#include "nova_dsp/modulation/ModMatrix.h"
#include "nova_dsp/filters/StateVariableFilter.h"
#include "nova_dsp/reverb/DualEngineReverb.h"
#include "nova_dsp/effects/StereoBbdChorus.h"
#include "nova_dsp/synth/WorshipSynthAirStage.h"
#include "nova_dsp/math/OnePoleSmoother.h"
#include "nova_dsp/math/LockFreeRingBuffer.h"
#include <array>
#include <vector>
#include <atomic>
#include <cstdint>

namespace nova::dsp {

/**
 * @brief Complete Realtime-Safe Polyphonic Worship Pad & Pluck Synthesizer Engine (JORNA Architecture).
 * Supports up to 8 dynamic Oscillator/Source layers per voice, individual layer mute/enable,
 * Dual Filters, 4 Envelopes, 4 LFOs, Key-Locked Worship Drone, Arpeggiator, and Embedded Nova Space Reverb.
 */
class WorshipSynthEngine {
public:
    static constexpr int kMaxVoices = 32;
    static constexpr int kNumOscSlots = 8;

    enum class ArpMode : int {
        Up = 0,
        Down = 1,
        UpDown = 2,
        DownUp = 3,
        Random = 4,
        ChordPulse = 5
    };

    [[nodiscard]] static float getDelayTimeForDivision(float bpm, int divisionIndex) noexcept;
    [[nodiscard]] static float getArpRateForDivision(float bpm, int divisionIndex) noexcept;
    [[nodiscard]] static float getLfoRateForDivision(float bpm, int divisionIndex) noexcept;

    struct Parameters {
        std::array<MultiEngineOscillator::Parameters, kNumOscSlots> oscSlots{};
        int activeOscCount = 3; // 1 .. 8 layers in the SOURCES list

        // Dual Filters & Shaping
        bool filter1Enabled = true;
        int filterType = 0;         // 0: LowPass 24, 1: BandPass 12, 2: HighPass 24, 3: LowPass 12 (Smooth)
        float filterCutoffHz = 2800.0f;
        float filterResonance = 0.75f;
        float filterEnvAmount = 0.35f;
        float filterKeyTrack = 0.40f;
        float filterDrive = 0.15f;

        bool filter2Enabled = true;
        int filter2Type = 0;        // 0: LowPass, 1: BandPass, 2: HighPass, 3: Vocal Carve/Notch
        float filter2CutoffHz = 3600.0f;
        float filter2Resonance = 0.70f;
        float filter2EnvAmount = 0.0f;
        float filter2KeyTrack = 0.0f;
        float filter2Drive = 0.0f;
        float vocalCarve = 0.25f;

        int filterRoutingMode = 0;  // 0: Serial (1 -> 2), 1: Parallel (1 + 2)
        float analogDrift = 0.35f;  // 0.0 .. 1.0 per-voice slow analog pitch & cutoff drift

        // Envelopes (ENV 1 = Amp, ENV 2 = Filter, ENV 3 = Mod/Morph, ENV 4 = Pitch/Aux)
        AdsrEnvelope::Parameters ampEnv{ 0.45f, 1.40f, 0.82f, 2.20f };
        AdsrEnvelope::Parameters filterEnv{ 0.60f, 1.80f, 0.40f, 2.00f };
        AdsrEnvelope::Parameters modEnv{ 0.80f, 2.20f, 0.50f, 2.50f };
        AdsrEnvelope::Parameters auxEnv{ 0.20f, 0.80f, 0.30f, 1.50f };

        // LFOs
        LfoGenerator::Shape lfo1Shape = LfoGenerator::Shape::Sine;
        float lfo1RateHz = 0.42f;
        float lfo1Depth  = 0.35f;
        bool  lfo1Sync   = false;
        int   lfo1SyncDiv = 6; // 1/4

        LfoGenerator::Shape lfo2Shape = LfoGenerator::Shape::SmoothDrift;
        float lfo2RateHz = 0.18f;
        float lfo2Depth  = 0.25f;
        bool  lfo2Sync   = false;
        int   lfo2SyncDiv = 7; // 1/8 D

        LfoGenerator::Shape lfo3Shape = LfoGenerator::Shape::Triangle;
        float lfo3RateHz = 1.20f;
        float lfo3Depth  = 0.00f;
        bool  lfo3Sync   = false;
        int   lfo3SyncDiv = 8; // 1/8

        LfoGenerator::Shape lfo4Shape = LfoGenerator::Shape::PulseGate;
        float lfo4RateHz = 0.25f;
        float lfo4Depth  = 0.00f;
        bool  lfo4Sync   = false;
        int   lfo4SyncDiv = 2; // 1 Bar

        // 4 Worship Performance Macros (0..1)
        float macroWarmth  = 0.50f;
        float macroMotion  = 0.35f;
        float macroShimmer = 0.40f;
        float macroAir     = 0.30f;

        // Worship Drone / Infinite Pad Generator
        bool droneEnabled = false;
        int droneRootKey  = 0;      // 0 = C, 1 = C#, ..., 11 = B
        int droneOctave   = 0;      // -2 to +2 octaves
        float droneLevel  = 0.45f;
        int droneVoicing  = 0;      // 0 = Lush 5th, 1 = Deep Sub, 2 = Ambient 9th, 3 = Celestial 8ve
        float droneGlide  = 1.5f;   // Key crossfade time in seconds
        float droneTone   = 0.70f;  // Filter rolloff / warmth

        // BPM & Tempo Synchronization
        float bpm = 120.0f;

        // Arpeggiator / Sequencer
        bool arpEnabled   = false;
        ArpMode arpMode   = ArpMode::Up;
        bool arpSync      = true;
        int  arpSyncDiv   = 4;      // 0: 1/4, 1: 1/8, 2: 1/8d, 3: 1/8t, 4: 1/16, 5: 1/16d, 6: 1/32
        float arpRateHz   = 4.0f;   // Steps per second (when free)
        float arpGate     = 0.65f;
        int   arpOctaves  = 1;      // 1 to 4 octaves
        bool  arpLatch    = false;  // Latch / Hold notes
        float arpSwing    = 0.0f;   // 0.0 to 0.75
        int   arpPatternLength = 16; // 16, 32, or 64 steps
        static constexpr int kMaxArpSteps = 64;
        std::array<bool, kMaxArpSteps> arpStepGates = [] {
            std::array<bool, kMaxArpSteps> arr{}; arr.fill(true); return arr;
        }();
        std::array<float, kMaxArpSteps> arpStepVelocities = [] {
            std::array<float, kMaxArpSteps> arr{};
            for (std::size_t i = 0; i < kMaxArpSteps; ++i) arr[i] = (i % 4 == 0) ? 0.95f : ((i % 2 == 0) ? 0.80f : 0.65f);
            return arr;
        }();

        // Embedded Roland Juno-60 / Solina Stereo BBD Chorus
        bool  chorusEnabled      = false;
        int   chorusMode         = 0;      // 0: Juno I, 1: Juno II, 2: Juno I+II, 3: Solina Ensemble, 4: Dimension D
        float chorusMix          = 0.0f;   // 0.0 .. 1.0 (wet/dry balance)
        float chorusRateHz       = 0.52f;  // 0.1 .. 8.0 Hz
        float chorusDepth        = 0.65f;  // 0.0 .. 1.0
        float chorusWarmth       = 0.60f;  // 0.0 .. 1.0 BBD lowpass warmth

        // Embedded Stereo Delay
        bool delayEnabled   = true;
        bool delaySync      = true;
        int  delaySyncDiv   = 4;      // 0: 1/2, 1: 1/4, 2: 1/4d, 3: 1/8, 4: 1/8d, 5: 1/8t, 6: 1/16, 7: 1/16d
        float delayMix      = 0.28f;
        float delayTimeSec  = 0.375f;
        float delayFeedback = 0.45f;
        float delaySwing    = 0.0f;   // 0.0 = Straight 1:1 Ping-Pong, >0.0 = Swing/Shuffle timing offset
        bool  subLowestOnly = true;   // When chords are played, only the lowest note activates Sub Bass (prevents mud)

        // Embedded Nova Space Reverb & Master
        float reverbDry          = 1.0f;
        float reverbMix          = 0.42f;
        float reverbDecay        = 6.2f;
        float reverbPreDelay     = 18.0f;  // 0.0 .. 100.0 ms
        float reverbShimmer      = 0.45f;
        int   reverbShimmerPitch = 0;   // 0: +1 Oct (+12st), 1: +2 Oct (+24st), 2: +1 & +2 Oct, 3: -1 Oct Sub
        float reverbDucking      = 0.25f;
        bool  reverbFreeze       = false;
        float masterGainDb       = 0.0f;
        float masterTuningHz     = 440.0f; // Reference A4 tuning in Hz (435.0 .. 445.0 Hz)
    };

    struct Telemetry {
        int activeVoiceCount = 0;
        float outputPeakL = 0.0f;
        float outputPeakR = 0.0f;
        float preClipPeak = 0.0f;
        float lfo1Value = 0.0f;
        float lfo2Value = 0.0f;
        float lfo3Value = 0.0f;
        float lfo4Value = 0.0f;
        float filterCutoffMod = 0.0f;
        float filter2CutoffMod = 0.0f;
        float filterResoMod = 0.0f;
        float filter2ResoMod = 0.0f;
        float lastEnvVal = 0.0f;
        float lastPlayedVelocity = 0.8f;
        float modWheelValue = 0.0f;
        float pitchBendValue = 0.0f;
    };

    WorshipSynthEngine();

    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void setParameters(const Parameters& params) noexcept;
    [[nodiscard]] const Parameters& getParameters() const noexcept { return params_; }
    [[nodiscard]] ModMatrix& getModMatrix() noexcept { return modMatrix_; }
    [[nodiscard]] const ModMatrix& getModMatrix() const noexcept { return modMatrix_; }
    [[nodiscard]] const LfoGenerator& getLfo1() const noexcept { return lfo1_; }
    [[nodiscard]] const LfoGenerator& getLfo2() const noexcept { return lfo2_; }
    [[nodiscard]] const LfoGenerator& getLfo3() const noexcept { return lfo3_; }
    [[nodiscard]] const LfoGenerator& getLfo4() const noexcept { return lfo4_; }

    // MIDI Event Handling (Realtime-Safe)
    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff(int midiNote) noexcept;
    void setSustainPedal(bool down) noexcept;
    void setPitchBend(float semitones) noexcept { pitchBendSemitones_ = semitones; }
    [[nodiscard]] float getPitchBend() const noexcept { return pitchBendSemitones_; }
    void setModWheel(float normVal) noexcept { modWheel_ = std::clamp(normVal, 0.0f, 1.0f); }
    [[nodiscard]] float getModWheel() const noexcept { return modWheel_; }
    void allNotesOff() noexcept;
    void allSoundOff() noexcept { reset(); }
    void stopActiveArpVoices() noexcept;
    void snapSmoothersToTarget() noexcept;

    void triggerDrone(bool triggered = true) noexcept;
    [[nodiscard]] bool isDroneTriggered() const noexcept { return droneTriggered_; }

    void processBlock(float* outLeft, float* outRight, int numSamples) noexcept;

    [[nodiscard]] Telemetry getTelemetry() const noexcept;
    [[nodiscard]] int getActiveVoiceCount() const noexcept { return telemetryActiveVoices_.load(std::memory_order_relaxed); }

    // Pop recent output audio samples for the real-time SPECTRUM / WAVE display
    std::size_t popVisualizerSamples(float* dest, std::size_t maxCount) noexcept {
        return visualizerRingBuffer_.popAvailable(dest, maxCount);
    }

    [[nodiscard]] int getArpCurrentStep() const noexcept { return currentArpStep_; }

private:
    struct Voice {
        bool active = false;
        bool heldByKey = false;
        bool sustainedByPedal = false;
        bool isDroneVoice = false;
        bool heldByDrone = false;
        bool isArpVoice = false;
        bool heldByArp = false;
        int midiNote = -1;
        float baseFreqHz = 440.0f;
        float velocity = 0.8f;
        uint64_t ageCounter = 0;

        std::array<MultiEngineOscillator, kNumOscSlots> oscs{};
        AdsrEnvelope ampEnv{};
        AdsrEnvelope filterEnv{};
        AdsrEnvelope modEnv{};
        AdsrEnvelope auxEnv{};
        StateVariableFilter filterL{};
        StateVariableFilter filterR{};
        StateVariableFilter filterStage2L{};
        StateVariableFilter filterStage2R{};
        StateVariableFilter filter2L{};
        StateVariableFilter filter2R{};
        AnalogDriftGenerator drift{};
        int driftSampleCounter = 0;

        // Snapshot of voice parameters for Seamless Patch-Remain
        int activeOscCountSnapshot = 3;
        float oscNormSnapshot = 1.0f;
        float flt1ResComp = 1.0f;
        float flt2ResComp = 1.0f;
        int filterTypeSnapshot = 0;
        float filterCutoffSnapshot = 2800.0f;
        float filterResSnapshot = 0.75f;
        float filterEnvAmtSnapshot = 0.35f;
        float filterDriveSnapshot = 0.15f;

        int filter2TypeSnapshot = 0;
        float filter2CutoffSnapshot = 3600.0f;
        float filter2ResSnapshot = 0.70f;
        float filter2EnvAmtSnapshot = 0.0f;
        float filter2DriveSnapshot = 0.0f;
    };

    void startVoice(int midiNote, float velocity, bool isDrone = false, bool isArp = false) noexcept;
    void updateDroneVoices() noexcept;
    void stepArpeggiator(int numSamples) noexcept;
    void processMasterEffectsChain(float* outLeft, float* outRight, int numSamples) noexcept;

    double sampleRate_ = 48000.0;
    Parameters params_{};
    uint64_t voiceCounter_ = 0;

    std::array<Voice, kMaxVoices> voices_{};
    bool sustainPedalDown_ = false;
    float pitchBendSemitones_ = 0.0f;
    float modWheel_ = 0.0f;
    float lastVelocity_ = 0.8f;

    // Drone tracking & dedicated Warmth filter
    bool droneTriggered_ = false;
    bool currentDroneActive_ = false;
    int currentDroneKey_ = -1;
    int currentDroneVoicing_ = -1;
    int currentDroneOctave_ = 0;
    StateVariableFilter droneFilterL_{};
    StateVariableFilter droneFilterR_{};
    OnePoleSmoother droneToneSmoother_{};

    // Arpeggiator state
    std::array<bool, 128> physicalHeldNotes_{};
    std::array<int, 32> latchedNotes_{};
    int latchedCount_ = 0;
    double arpPhaseSamples_ = 0.0;
    int arpStepIndex_ = 0;
    int currentArpStep_ = 0;
    int currentArpNote_ = -1;
    bool arpTriggerPending_ = false;

    // Global Modulation & Filtering
    LfoGenerator lfo1_{};
    LfoGenerator lfo2_{};
    LfoGenerator lfo3_{};
    LfoGenerator lfo4_{};
    ModMatrix modMatrix_{};
    WorshipSynthAirStage airStage_{};
    StereoBbdChorus chorus_{};
    DualEngineReverb reverb_{};

    // Stereo Ping-Pong Delay buffer
    std::vector<float> delayBufferL_;
    std::vector<float> delayBufferR_;
    int delayWritePos_ = 0;
    float delayEnergy_ = 0.0f;

    // Realtime-safe Drone Block Buffers
    std::vector<float> droneBufferL_;
    std::vector<float> droneBufferR_;

    OnePoleSmoother cutoffSmoother_{};
    OnePoleSmoother cutoff2Smoother_{};
    OnePoleSmoother masterGainSmoother_{};
    OnePoleSmoother polyHeadroomSmoother_{};

    LockFreeRingBuffer<float, 2048> visualizerRingBuffer_{};

    std::atomic<int> telemetryActiveVoices_{0};
    std::atomic<float> telemetryPeakL_{0.0f};
    std::atomic<float> telemetryPeakR_{0.0f};
    std::atomic<float> telemetryPreClipPeak_{0.0f};
    std::atomic<float> telemetryLfo1_{0.0f};
    std::atomic<float> telemetryLfo2_{0.0f};
    std::atomic<float> telemetryLfo3_{0.0f};
    std::atomic<float> telemetryLfo4_{0.0f};
    std::atomic<float> telemetryFilterCutoffMod_{0.0f};
    std::atomic<float> telemetryFilter2CutoffMod_{0.0f};
    std::atomic<float> telemetryFilterResoMod_{0.0f};
    std::atomic<float> telemetryFilter2ResoMod_{0.0f};
    std::atomic<float> telemetryLastEnvVal_{0.0f};
};

} // namespace nova::dsp
