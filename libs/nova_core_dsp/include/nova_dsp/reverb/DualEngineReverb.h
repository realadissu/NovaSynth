#pragma once

#include "nova_dsp/math/OnePoleSmoother.h"
#include "nova_dsp/filters/StateVariableFilter.h"
#include <array>
#include <vector>
#include <atomic>
#include <cstdint>

namespace nova::dsp {

/**
 * @brief Studio-grade, low-CPU Dual-Engine Reverb:
 *  - EngineMode::Acoustic: Phase-pure 16-Channel Householder FDN + 12-Tap Early Reflections
 *    with 3-Band RT60 Damping (tailored for Live Piano, Vocals, and acoustic instruments).
 *  - EngineMode::LushCloud: Modulated Diffusion Tank + Dual-Grain Octave/Fifth Shimmer,
 *    Infinite Freeze, and Dynamic Input Ducking (tailored for Worship Pads & Ambient).
 *
 * Strictly Realtime-Safe: Zero allocations and zero locks inside processBlock().
 */
class DualEngineReverb {
public:
    enum class EngineMode : int {
        Acoustic = 0,
        LushCloud = 1
    };

    enum class SpaceType : int {
        StudioRoom = 0,
        WarmChamber = 1,
        ConcertHall = 2,
        CathedralCloud = 3
    };

    struct Parameters {
        EngineMode engineMode = EngineMode::Acoustic;
        SpaceType spaceType = SpaceType::ConcertHall;

        float preDelayMs = 18.0f;       // 0.0 .. 250.0 ms
        float decaySeconds = 2.8f;      // 0.2 .. 30.0 s
        float size = 0.75f;             // 0.2 .. 1.5
        float diffusion = 0.80f;        // 0.0 .. 1.0
        float erLevel = 0.35f;          // 0.0 .. 1.0 (Early Reflections mix)

        // Tone & Damping
        float lowCutHz = 90.0f;         // 20 .. 600 Hz
        float highCutHz = 9500.0f;      // 1500 .. 20000 Hz
        float highDamping = 0.45f;      // 0.0 .. 1.0 (HF absorption in tank)
        float bassMultiplier = 1.15f;   // 0.4 .. 2.0 (LF RT60 ratio)
        float vocalCarve = 0.20f;       // 0.0 .. 1.0 (Carves 1.8 - 3.2 kHz for vocals/piano clarity)
        float shimmerLowCutHz = 190.0f; // 40 .. 2500 Hz (cuts mud & fundamentals out of shimmer shifter)
        float shimmerHighCutHz = 10500.0f; // 2000 .. 20000 Hz (silky high-end opening for celestial bloom)

        // Lush / Worship Cloud Controls
        float modRateHz = 0.65f;        // 0.05 .. 3.0 Hz
        float modDepth = 0.35f;         // 0.0 .. 1.0
        float shimmerOctave = 0.0f;     // 0.0 .. 1.0 (+12 st angelic shimmer)
        float shimmerFifth = 0.0f;      // 0.0 .. 1.0 (+7 st harmonic fifth glow)
        float ducking = 0.0f;           // 0.0 .. 1.0 (Sidechain ducking of reverb tail)
        bool freeze = false;            // Infinite sustain hold

        // Output & Parallel Levels
        float stereoWidth = 1.0f;       // 0.0 .. 1.5
        float dry = 1.0f;               // 0.0 .. 1.0 (Separate Dry Signal Level)
        float mix = 0.35f;              // 0.0 .. 1.0 (Separate Reverb Wet Level)
        int shimmerPitchMode = 0;       // 0: +1 Oct (+12st), 1: +2 Oct (+24st), 2: +1 & +2 Oct, 3: -1 Oct Sub
        float outputGainDb = 0.0f;      // -24.0 .. +12.0 dB
    };

    struct Telemetry {
        float inputPeak = 0.0f;
        float outputPeak = 0.0f;
        float tankEnergy = 0.0f;
        float duckingGainReductionDb = 0.0f;
        std::array<float, 16> channelEnergies{};
    };

    DualEngineReverb() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void setParameters(const Parameters& params) noexcept;
    [[nodiscard]] const Parameters& getParameters() const noexcept { return params_; }

    /**
     * @brief Realtime-safe stereo in-place processing.
     * Guaranteed zero heap allocations and zero locks.
     */
    void processBlock(float* leftChannel, float* rightChannel, int numSamples) noexcept;

    [[nodiscard]] Telemetry getTelemetry() const noexcept;

private:
    static constexpr int kNumFdnChannels = 16;
    static constexpr int kNumDiffusers = 4;
    static constexpr int kNumEarlyTaps = 12;

    // Pre-allocated power-of-two delay line for fast bitmask wrapping
    struct DelayLine {
        std::vector<float> buffer;
        int mask = 0;
        int writePos = 0;

        void allocate(int minSamples) {
            int pow2 = 1;
            while (pow2 <= minSamples + 8) {
                pow2 <<= 1;
            }
            buffer.assign(static_cast<std::size_t>(pow2), 0.0f);
            mask = pow2 - 1;
            writePos = 0;
        }

        void clear() noexcept {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
            writePos = 0;
        }

        inline void write(float sample) noexcept {
            buffer[static_cast<std::size_t>(writePos)] = sample;
            writePos = (writePos + 1) & mask;
        }

        [[nodiscard]] inline float readInt(int delaySamples) const noexcept {
            const int idx = (writePos - delaySamples) & mask;
            return buffer[static_cast<std::size_t>(idx)];
        }

        [[nodiscard]] inline float readFrac(float delaySamples) const noexcept {
            const int intDelay = static_cast<int>(delaySamples);
            const float frac = delaySamples - static_cast<float>(intDelay);
            const int idx0 = (writePos - intDelay) & mask;
            const int idx1 = (idx0 - 1) & mask;
            const float s0 = buffer[static_cast<std::size_t>(idx0)];
            const float s1 = buffer[static_cast<std::size_t>(idx1)];
            return s0 + frac * (s1 - s0);
        }

        [[nodiscard]] inline float readCubic(float delaySamples) const noexcept {
            const int intDelay = static_cast<int>(delaySamples);
            const float frac = delaySamples - static_cast<float>(intDelay);
            const int idx0 = (writePos - intDelay + 1) & mask;
            const int idx1 = (writePos - intDelay) & mask;
            const int idx2 = (writePos - intDelay - 1) & mask;
            const int idx3 = (writePos - intDelay - 2) & mask;
            const float y0 = buffer[static_cast<std::size_t>(idx0)];
            const float y1 = buffer[static_cast<std::size_t>(idx1)];
            const float y2 = buffer[static_cast<std::size_t>(idx2)];
            const float y3 = buffer[static_cast<std::size_t>(idx3)];
            const float c0 = y1;
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * frac + c2) * frac + c1) * frac + c0;
        }
    };

    struct AllpassDiffuser {
        DelayLine delay;
        int delaySamples = 100;
        float feedback = 0.65f;

        void clear() noexcept { delay.clear(); }

        [[nodiscard]] inline float process(float input) noexcept {
            const float delayed = delay.readInt(delaySamples);
            const float vn = input - feedback * delayed;
            delay.write(vn);
            return delayed + feedback * vn;
        }
    };

    // Valhalla-style Dual-Grain Half-Sine Pitch Shifter with Zero-Weight Latch Jitter
    struct GranularPitchShifter {
        DelayLine buffer;
        float phaseA = 0.0f;
        float grainSizeSamples = 8800.0f;
        float prevPhaseA = 0.0f;
        float prevPhaseB = 0.5f;
        float offsetA = 0.0f;
        float offsetB = 0.0f;
        float maxJitterSamples = 720.0f;
        uint32_t rng = 0x9E3779B9u;

        void prepare(double sampleRate, float phaseOffset = 0.0f, float grainMs = 190.0f, uint32_t seed = 0x9E3779B9u) {
            buffer.allocate(static_cast<int>(sampleRate * 0.50));
            grainSizeSamples = static_cast<float>(sampleRate * (grainMs * 0.001));
            maxJitterSamples = static_cast<float>(sampleRate * 0.016); // 16ms decorrelation jitter latched at zero-crossing
            rng = seed;
            clear(phaseOffset);
        }

        void clear(float phaseOffset = 0.0f) noexcept {
            buffer.clear();
            phaseA = phaseOffset;
            prevPhaseA = phaseOffset;
            prevPhaseB = std::fmod(phaseOffset + 0.5f, 1.0f);
            offsetA = 0.0f;
            offsetB = 0.0f;
        }

        [[nodiscard]] float process(float input, float pitchRatio) noexcept;
    };

    double sampleRate_ = 48000.0;
    Parameters params_{};
    bool isTankSilent_ = true;
    int silenceHoldSamples_ = 0;

    // Pre-delay
    DelayLine preDelayL_;
    DelayLine preDelayR_;

    // Input filters & Vocal Carve filter
    StateVariableFilter inLowCutL_, inLowCutR_;
    StateVariableFilter inHighCutL_, inHighCutR_;
    StateVariableFilter vocalCarveL_, vocalCarveR_;

    // Input allpass diffusers (stereo)
    std::array<AllpassDiffuser, kNumDiffusers> diffusersL_;
    std::array<AllpassDiffuser, kNumDiffusers> diffusersR_;

    // Early reflections delay line
    DelayLine erDelayL_;
    DelayLine erDelayR_;
    std::array<int, kNumEarlyTaps> erTapSamplesL_{};
    std::array<int, kNumEarlyTaps> erTapSamplesR_{};
    std::array<float, kNumEarlyTaps> erTapGains_{};

    // 16-Channel Unitary Hadamard FDN Tank + In-Loop Allpass Diffusers
    std::array<DelayLine, kNumFdnChannels> fdnDelays_;
    std::array<AllpassDiffuser, kNumFdnChannels> fdnInLoopDiffusers_{};
    std::array<float, kNumFdnChannels> fdnBaseDelaySamples_{};
    std::array<float, kNumFdnChannels> fdnCurrentDelayLen_{};
    std::array<float, kNumFdnChannels> fdnDelayStep_{};
    std::array<float, kNumFdnChannels> fdnLpState_{};
    std::array<float, kNumFdnChannels> fdnLowShelfState_{};
    std::array<float, kNumFdnChannels> fdnDcInPrev_{};
    std::array<float, kNumFdnChannels> fdnDcOutPrev_{};
    std::array<float, 4> fdnQuadLfoPhase_{};

    // Shimmer Pitch Shifters, 16-Tap Cloud Distribution Delay Lines, Diffusers & Filters
    GranularPitchShifter octaveShifterL_, octaveShifterR_;
    GranularPitchShifter octave2ShifterL_, octave2ShifterR_;
    GranularPitchShifter fifthShifterL_, fifthShifterR_;
    DelayLine shimmerCloudDelayL_, shimmerCloudDelayR_;
    std::array<int, kNumFdnChannels> shimmerCloudTapSamples_{};
    std::array<AllpassDiffuser, 2> shimmerDiffusersL_{};
    std::array<AllpassDiffuser, 2> shimmerDiffusersR_{};
    StateVariableFilter shimmerSilkL_, shimmerSilkR_;
    StateVariableFilter shimmerInterStageL_, shimmerInterStageR_;
    StateVariableFilter shimmerHighPassL_, shimmerHighPassR_;

    // Ducking envelope state
    float duckEnvelope_ = 0.0f;

    // Parameter Smoothers
    OnePoleSmoother drySmoother_;
    OnePoleSmoother mixSmoother_;
    OnePoleSmoother decaySmoother_;
    OnePoleSmoother sizeSmoother_;
    OnePoleSmoother preDelaySmoother_;
    OnePoleSmoother dampingSmoother_;
    OnePoleSmoother shimmerOctSmoother_;
    OnePoleSmoother shimmerFifthSmoother_;
    OnePoleSmoother duckingSmoother_;
    OnePoleSmoother erLevelSmoother_;
    OnePoleSmoother widthSmoother_;
    OnePoleSmoother outGainSmoother_;

    // Atomic telemetry for UI
    std::atomic<float> telemetryInPeak_{0.0f};
    std::atomic<float> telemetryOutPeak_{0.0f};
    std::atomic<float> telemetryTankEnergy_{0.0f};
    std::atomic<float> telemetryDuckDb_{0.0f};
    std::array<std::atomic<float>, kNumFdnChannels> telemetryChannelEnergy_{};
};

} // namespace nova::dsp
