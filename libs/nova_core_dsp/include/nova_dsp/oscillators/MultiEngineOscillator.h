#pragma once

#include "nova_dsp/math/OnePoleSmoother.h"
#include "nova_dsp/filters/StateVariableFilter.h"
#include <array>
#include <vector>
#include <cstdint>

namespace nova::dsp {

struct SampleZone;

/**
 * @brief JORNA-style Multi-Engine Oscillator Slot.
 * Each slot can switch seamlessly between 4 distinct synthesis engines:
 *  0: Warm Analog (PolyBLEP Saw/Pulse/Tri + Stereo Unison + Vintage Drift)
 *  1: Evolving Wavetable (8-Frame Interpolated Harmonic Morphing)
 *  2: Organic Texture / Grain (Choir Air, Bowed String, Tape Halo, Bell Dust)
 *  3: Physical Pluck / Modal (Karplus-Strong Waveguide + Modal String Resonator)
 */
class MultiEngineOscillator {
public:
    enum class EngineType : int {
        WarmAnalog = 0,
        EvolvingWavetable = 1,
        OrganicTexture = 2,
        PhysicalPluckModal = 3,
        SubOscillator = 4,
        NoiseTransient = 5,
        SamplePlayback = 6
    };

    struct Parameters {
        bool enabled = true;
        int sourceType = 0;         // 0: OSC, 1: SUB, 2: PLUCK, 3: NOISE, 4: SAMPLE
        EngineType engine = EngineType::WarmAnalog;
        int modelIndex = 0;         // 0..7 specific Waveform / Profile / Model index
        int filterRouting = 0;      // 0: FLT 1, 1: FLT 2, 2: FLT 1+2, 3: DIRECT
        int octaveOffset = 0;       // -2 .. +2
        int semitoneOffset = 0;     // -12 .. +12
        float fineCents = 0.0f;     // -100 .. +100 cents
        float shapeMorph = 0.35f;   // 0.0 .. 1.0 (Wave Shape / WT Position / Texture / Pluck Brightness)
        float detune = 0.28f;       // 0.0 .. 1.0 (Unison Detune & Stereo Spread)
        float level = 0.75f;        // 0.0 .. 1.0
        float pan = 0.0f;           // -1.0 .. +1.0
        float rate = 0.50f;         // 0.0 .. 1.0 (Grain rate / LFO rate)
        float scatter = 0.0f;       // 0.0 .. 1.0 (Grain scatter / random / Sample detune)
        float duration = 0.40f;     // 0.0 .. 1.0 (Grain duration / size)
        float spread = 0.85f;       // 0.0 .. 1.0 (Stereo spread)
        float warp = 0.0f;          // 0.0 .. 1.0 (Wavetable warp mode/amount)
        bool lowestNoteOnly = false; // Sub lowest note priority
        int ampEnvSource = 0;        // 0: Main Amp Env 1, 1: Filter Env 2, 2: Mod Env 3, 3: Aux Env 4
        int sampleLoopMode = 1;      // 0: One-Shot, 1: Forward, 2: Ping-Pong, 3: Reverse
        float sampleLoopStart = 0.20f; // 0.0 .. 1.0 (Loop start fraction)
        float sampleLoopEnd = 0.90f;   // 0.0 .. 1.0 (Loop end fraction)
        float sampleCrossfade = 0.05f; // 0.0 .. 0.25 (Equal-power crossfade length fraction)
    };

    [[nodiscard]] static std::vector<const char*> getModelsForEngine(EngineType engine, int sourceType);
    [[nodiscard]] static const char* getModelName(EngineType engine, int sourceType, int modelIndex);

    MultiEngineOscillator();

    void prepare(double sampleRate);
    void reset() noexcept;

    void triggerNote(float baseFrequencyHz, float velocity, uint32_t seedOffset = 0) noexcept;
    void setParameters(const Parameters& params) noexcept {
        params_ = params;
        updateCoefficients();
    }
    [[nodiscard]] const Parameters& getParameters() const noexcept { return params_; }

    /**
     * @brief Renders one stereo sample frame for this oscillator slot.
     * @param pitchModSemitones Dynamic pitch modulation offset in semitones
     * @param shapeMod Dynamic shape/morph modulation offset (-1..+1)
     */
    void processSample(float baseFrequencyHz,
                       float pitchModSemitones,
                       float shapeMod,
                       float& outLeft,
                       float& outRight,
                       float levelMod = 0.0f) noexcept;

    [[nodiscard]] static std::vector<float> buildWaveformPreview(const Parameters& p, int numPoints = 24);

private:
    static constexpr int kMaxUnison = 5;
    static constexpr int kWtTables = 8;
    static constexpr int kWtFrames = 4;
    static constexpr int kWtSize = 1024;
    static constexpr int kMaxStringDelay = 4096;

    [[nodiscard]] static float polyBlep(float phase, float phaseInc) noexcept;

    void updateCoefficients() noexcept;

    void processOrganicTexture(float slotFreq, float shape, float invSr, float& sigL, float& sigR) noexcept;
    void processPhysicalPluck(float slotFreq, float shape, float invSr, float& sigL, float& sigR) noexcept;
    void processSubOscillator(float slotFreq, float shape, float invSr, float& sigL, float& sigR) noexcept;
    void processNoiseTransient(float shape, float invSr, float& sigL, float& sigR) noexcept;
    void processSamplePlayback(float slotFreq, float& sigL, float& sigR) noexcept;

    double sampleRate_ = 48000.0;
    Parameters params_{};
    OnePoleSmoother shapeSmoother_{};

    // Precomputed coefficients for high-performance audio loop (0 transcendental calls per sample)
    std::array<float, kMaxUnison> unisonPitchMultipliers_{ 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    std::array<float, kMaxUnison> unisonPanL_{ 0.7071f, 0.7071f, 0.7071f, 0.7071f, 0.7071f };
    std::array<float, kMaxUnison> unisonPanR_{ 0.7071f, 0.7071f, 0.7071f, 0.7071f, 0.7071f };
    float basePitchRatio_ = 1.0f;
    float pluckDelayRatioR_ = 1.0f;
    float lastPitchModSemitones_ = -999.0f;
    float cachedPitchModRatio_ = 1.0f;

    // Unison phases & slow analog drift states
    std::array<float, kMaxUnison> phases_{};
    std::array<float, kMaxUnison> driftOffsetsCents_{};
    uint32_t rng_ = 0x9E3779B9u;

    // Organic Texture / Formant / Granular states
    struct TextureGrain {
        float pos = 0.0f;
        float rate = 1.0f;
        float lengthSamples = 1200.0f;
        float age = 0.0f;
        float panL = 0.7071f;
        float panR = 0.7071f;
        float amp = 1.0f;
    };
    static constexpr int kNumGrains = 16;
    std::array<TextureGrain, kNumGrains> grains_{};

    // 3-Band Fixed-Frequency Formant Filter Bank
    std::array<StateVariableFilter, 3> formantFiltersL_{};
    std::array<StateVariableFilter, 3> formantFiltersR_{};
    float lastFormantShape_ = -999.0f;
    int lastFormantModel_ = -1;

    float texFilterState1_ = 0.0f;
    float texFilterState2_ = 0.0f;
    float texGrainPhase_ = 0.0f;
    float texGrainPhase2_ = 0.0f;
    float flutterPhase_ = 0.0f;
    float rainScatterTimer_ = 0.0f;

    // Physical Modeling Pluck / String Waveguide buffer (pre-allocated in prepare)
    std::vector<float> stringDelayL_{};
    std::vector<float> stringDelayR_{};
    int stringWritePos_ = 0;
    float stringLpStateL_ = 0.0f;
    float stringLpStateR_ = 0.0f;

    // Modal Tine / Bell states (Kalimba & Glass Mallet)
    std::array<float, 4> modalPhases_{};
    std::array<float, 4> modalAmplitudes_{};
    std::array<float, 4> modalDecays_{};

    // Acoustic / Hybrid Sample Playback states
    const SampleZone* activeSampleZone_ = nullptr;
    double samplePlayhead_ = 0.0;
    double sampleDirection_ = 1.0;
    bool sampleFinished_ = false;
};

} // namespace nova::dsp
