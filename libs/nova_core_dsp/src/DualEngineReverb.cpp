#include "nova_dsp/reverb/DualEngineReverb.h"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace nova::dsp {

namespace {
// 16 mutually prime base delay lengths in milliseconds (spanning 23.27ms to 127.91ms)
constexpr std::array<float, 16> kBaseFdnDelayMs = {
    23.27f, 27.53f, 31.91f, 36.73f,
    41.87f, 47.59f, 53.81f, 60.47f,
    67.61f, 75.29f, 83.53f, 92.09f,
    100.93f, 109.79f, 118.67f, 127.91f
};

// 16 mutually prime in-loop allpass diffuser lengths in milliseconds (for exponential modal density)
constexpr std::array<float, 16> kInLoopDiffuserMs = {
    3.11f, 3.67f, 4.19f, 4.79f,
    5.23f, 5.77f, 6.31f, 6.83f,
    7.19f, 7.61f, 8.09f, 8.53f,
    8.87f, 9.19f, 9.47f, 9.83f
};

// 16 mutually prime shimmer cloud distribution tap lengths in milliseconds
// Ensures all 16 FDN channels receive independent, decorrelated shimmer signals (zero Hadamard cancellation!)
constexpr std::array<float, 16> kShimmerCloudTapMs = {
    0.67f,  3.47f,  6.83f, 10.61f,
    14.89f, 19.33f, 24.11f, 29.09f,
    33.73f, 38.21f, 42.53f, 46.79f,
    50.39f, 53.87f, 56.53f, 58.97f
};

// Full-rank aperiodic 16-channel input excitation weights (excites all 16 Walsh-Hadamard eigenmodes)
constexpr std::array<float, 16> kInputExcitationSigns = {
     1.00f,  1.00f, -1.00f,  1.00f,
    -1.00f,  0.88f,  1.00f, -0.88f,
     0.92f, -1.00f, -0.92f, -1.00f,
     1.00f, -0.94f,  0.94f,  1.00f
};

// Decorrelated 16-channel stereo readout weights
constexpr std::array<float, 16> kReadoutSignsL = {
     1.00f,  0.28f, -1.00f, -0.28f,
     0.94f, -0.28f,  0.94f,  0.28f,
    -0.96f,  0.28f,  0.96f, -0.28f,
    -0.92f, -0.28f, -0.92f,  0.28f
};

constexpr std::array<float, 16> kReadoutSignsR = {
     0.28f,  1.00f, -0.28f, -1.00f,
    -0.28f,  0.94f,  0.28f,  0.94f,
     0.28f, -0.96f, -0.28f,  0.96f,
    -0.28f, -0.92f,  0.28f, -0.92f
};

// 4 mutually irrational LFO rate multipliers for zero-pitch-drift quadrature modulation quads
constexpr std::array<float, 4> kQuadRateMultipliers = { 0.73f, 0.91f, 1.13f, 1.37f };

// 4 prime input diffuser lengths in milliseconds
constexpr std::array<float, 4> kDiffuserDelayMsL = { 4.73f, 7.39f, 11.53f, 16.97f };
constexpr std::array<float, 4> kDiffuserDelayMsR = { 5.21f, 8.09f, 12.37f, 17.83f };

// 12 Early Reflection tap times in ms and gains
constexpr std::array<float, 12> kEarlyTapMsL = {
    3.1f, 5.8f, 9.2f, 13.7f, 18.4f, 23.9f, 29.5f, 36.1f, 43.8f, 51.2f, 60.4f, 71.0f
};
constexpr std::array<float, 12> kEarlyTapMsR = {
    3.7f, 6.4f, 10.1f, 14.9f, 19.8f, 25.1f, 31.2f, 38.0f, 45.3f, 53.7f, 62.9f, 73.5f
};
constexpr std::array<float, 12> kEarlyTapGains = {
    0.84f, -0.71f, 0.63f, 0.56f, -0.49f, 0.43f, -0.38f, 0.33f, 0.29f, -0.25f, 0.21f, -0.18f
};

[[nodiscard]] inline float flushDenormal(float x) noexcept {
    return (std::isfinite(x) && std::abs(x) >= 1e-20f) ? x : 0.0f;
}

[[nodiscard]] inline float getSpaceSizeScale(DualEngineReverb::SpaceType st) noexcept {
    switch (st) {
        case DualEngineReverb::SpaceType::StudioRoom:     return 0.58f;
        case DualEngineReverb::SpaceType::WarmChamber:    return 0.82f;
        case DualEngineReverb::SpaceType::ConcertHall:    return 1.05f;
        case DualEngineReverb::SpaceType::CathedralCloud: return 1.32f;
    }
    return 1.0f;
}

[[nodiscard]] inline float getSpaceErScale(DualEngineReverb::SpaceType st) noexcept {
    switch (st) {
        case DualEngineReverb::SpaceType::StudioRoom:     return 1.25f;
        case DualEngineReverb::SpaceType::WarmChamber:    return 1.05f;
        case DualEngineReverb::SpaceType::ConcertHall:    return 0.85f;
        case DualEngineReverb::SpaceType::CathedralCloud: return 0.55f;
    }
    return 1.0f;
}

[[nodiscard]] inline float softClipTank(float x) noexcept {
    const float ax = std::abs(x);
    if (ax <= 1.15f) {
        return x;
    }
    const float sign = (x >= 0.0f) ? 1.0f : -1.0f;
    const float ex = (ax - 1.15f) * 0.80f;
    const float ex2 = ex * ex;
    const float t = (ex <= 3.0f) ? (ex * (27.0f + ex2) / (27.0f + 9.0f * ex2)) : 1.0f;
    return sign * (1.15f + 0.75f * t);
}

[[nodiscard]] inline float nextRandomUnit(uint32_t& state) noexcept {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state & 0x00FFFFFFu) * (1.0f / 16777216.0f);
}
} // namespace

float DualEngineReverb::GranularPitchShifter::process(float input, float pitchRatio) noexcept {
    const float cleanIn = flushDenormal(input);
    buffer.write(cleanIn);

    const float gSize = std::max(512.0f, grainSizeSamples);
    const float phaseInc = (1.0f - pitchRatio) / gSize;

    phaseA += phaseInc;
    phaseA -= std::floor(phaseA);
    const float phaseB = (phaseA + 0.5f >= 1.0f) ? (phaseA - 0.5f) : (phaseA + 0.5f);

    // Valhalla-style Zero-Weight Random Delay Latch:
    // Whenever a grain wraps around (at phase == 0, where sin(pi * phase) == 0.0),
    // latch a fresh random delay offset (0 .. 16ms). Because the window weight is zero at the wrap,
    // this is 100% click-free and breaks up periodic Floquet sidebands into a silky ensemble cloud!
    const bool wrapA = (phaseInc < 0.0f && phaseA > prevPhaseA) || (phaseInc > 0.0f && phaseA < prevPhaseA);
    if (wrapA) {
        offsetA = nextRandomUnit(rng) * maxJitterSamples;
    }
    const bool wrapB = (phaseInc < 0.0f && phaseB > prevPhaseB) || (phaseInc > 0.0f && phaseB < prevPhaseB);
    if (wrapB) {
        offsetB = nextRandomUnit(rng) * maxJitterSamples;
    }
    prevPhaseA = phaseA;
    prevPhaseB = phaseB;

    constexpr float kBaseMargin = 96.0f;
    const float dA = phaseA * gSize + offsetA + kBaseMargin;
    const float dB = phaseB * gSize + offsetB + kBaseMargin;

    // Dual-Grain Half-Sine Window: wA^2 + wB^2 = sin^2(pi*p) + cos^2(pi*p) == 1.000000 (0.00 dB power ripple!)
    const float wA = std::sin(std::numbers::pi_v<float> * phaseA);
    const float wB = std::sin(std::numbers::pi_v<float> * phaseB);

    const float sA = buffer.readCubic(dA);
    const float sB = buffer.readCubic(dB);

    return (sA * wA + sB * wB) * 0.90f;
}

void DualEngineReverb::prepare(double sampleRate, int /*maxBlockSize*/) {
    sampleRate_ = std::max(8000.0, sampleRate);
    const auto sr = static_cast<float>(sampleRate_);

    // Allocate Pre-Delay (up to 350ms)
    preDelayL_.allocate(static_cast<int>(sr * 0.35f));
    preDelayR_.allocate(static_cast<int>(sr * 0.35f));

    // Prepare filters
    inLowCutL_.prepare(sampleRate_);
    inLowCutR_.prepare(sampleRate_);
    inHighCutL_.prepare(sampleRate_);
    inHighCutR_.prepare(sampleRate_);
    vocalCarveL_.prepare(sampleRate_);
    vocalCarveR_.prepare(sampleRate_);

    // Valhalla-style Celestial Shimmer Silk LowPass:
    // Allows ascending shimmer overtones to sparkle freely into the 8 - 14 kHz air band
    const float initShimHigh = std::clamp(params_.shimmerHighCutHz, 2500.0f, static_cast<float>(sampleRate_ * 0.45));
    shimmerSilkL_.prepare(sampleRate_);
    shimmerSilkR_.prepare(sampleRate_);
    shimmerSilkL_.setParameters(StateVariableFilter::Type::LowPass, initShimHigh, 0.55f);
    shimmerSilkR_.setParameters(StateVariableFilter::Type::LowPass, initShimHigh, 0.55f);

    // Inter-stage filter for cascaded +2 octave (+24st) shimmer
    shimmerInterStageL_.prepare(sampleRate_);
    shimmerInterStageR_.prepare(sampleRate_);
    shimmerInterStageL_.setParameters(StateVariableFilter::Type::LowPass, std::min(12500.0f, initShimHigh * 1.15f), 0.58f);
    shimmerInterStageR_.setParameters(StateVariableFilter::Type::LowPass, std::min(12500.0f, initShimHigh * 1.15f), 0.58f);

    const float initEffShimmerLowCut = (params_.shimmerPitchMode == 3) ? std::min(140.0f, params_.shimmerLowCutHz) : params_.shimmerLowCutHz;
    shimmerHighPassL_.prepare(sampleRate_);
    shimmerHighPassR_.prepare(sampleRate_);
    shimmerHighPassL_.setParameters(StateVariableFilter::Type::HighPass, initEffShimmerLowCut, 0.65f);
    shimmerHighPassR_.setParameters(StateVariableFilter::Type::HighPass, initEffShimmerLowCut, 0.65f);

    // Shimmer Diffusers: 2-stage prime allpass phase scattering
    constexpr std::array<float, 2> kShimmerDiffMsL = { 9.13f, 14.87f };
    constexpr std::array<float, 2> kShimmerDiffMsR = { 10.31f, 16.43f };
    for (int i = 0; i < 2; ++i) {
        const int sL = static_cast<int>(sr * kShimmerDiffMsL[static_cast<std::size_t>(i)] * 0.001f);
        const int sR = static_cast<int>(sr * kShimmerDiffMsR[static_cast<std::size_t>(i)] * 0.001f);
        shimmerDiffusersL_[static_cast<std::size_t>(i)].delay.allocate(sL * 2 + 64);
        shimmerDiffusersR_[static_cast<std::size_t>(i)].delay.allocate(sR * 2 + 64);
        shimmerDiffusersL_[static_cast<std::size_t>(i)].delaySamples = std::max(8, sL);
        shimmerDiffusersR_[static_cast<std::size_t>(i)].delaySamples = std::max(8, sR);
        shimmerDiffusersL_[static_cast<std::size_t>(i)].feedback = 0.64f;
        shimmerDiffusersR_[static_cast<std::size_t>(i)].feedback = 0.64f;
    }

    // 16-Tap Shimmer Cloud Distribution Delay Lines (up to 85ms)
    shimmerCloudDelayL_.allocate(static_cast<int>(sr * 0.085f) + 64);
    shimmerCloudDelayR_.allocate(static_cast<int>(sr * 0.085f) + 64);
    for (int ch = 0; ch < kNumFdnChannels; ++ch) {
        const auto idx = static_cast<std::size_t>(ch);
        shimmerCloudTapSamples_[idx] = std::max(2, static_cast<int>(sr * kShimmerCloudTapMs[idx] * 0.001f));
    }

    // Allocate Input Diffusers
    for (int i = 0; i < kNumDiffusers; ++i) {
        const int maxSamplesL = static_cast<int>(sr * (kDiffuserDelayMsL[static_cast<std::size_t>(i)] * 0.0025f));
        const int maxSamplesR = static_cast<int>(sr * (kDiffuserDelayMsR[static_cast<std::size_t>(i)] * 0.0025f));
        diffusersL_[static_cast<std::size_t>(i)].delay.allocate(std::max(128, maxSamplesL));
        diffusersR_[static_cast<std::size_t>(i)].delay.allocate(std::max(128, maxSamplesR));
        diffusersL_[static_cast<std::size_t>(i)].delaySamples =
            std::max(8, static_cast<int>(sr * kDiffuserDelayMsL[static_cast<std::size_t>(i)] * 0.001f));
        diffusersR_[static_cast<std::size_t>(i)].delaySamples =
            std::max(8, static_cast<int>(sr * kDiffuserDelayMsR[static_cast<std::size_t>(i)] * 0.001f));
    }

    // Allocate Early Reflections
    erDelayL_.allocate(static_cast<int>(sr * 0.15f));
    erDelayR_.allocate(static_cast<int>(sr * 0.15f));
    for (int i = 0; i < kNumEarlyTaps; ++i) {
        erTapSamplesL_[static_cast<std::size_t>(i)] =
            std::max(4, static_cast<int>(sr * kEarlyTapMsL[static_cast<std::size_t>(i)] * 0.001f));
        erTapSamplesR_[static_cast<std::size_t>(i)] =
            std::max(4, static_cast<int>(sr * kEarlyTapMsR[static_cast<std::size_t>(i)] * 0.001f));
        erTapGains_[static_cast<std::size_t>(i)] = kEarlyTapGains[static_cast<std::size_t>(i)];
    }

    // Allocate 16-Channel FDN Tank + In-Loop Allpass Diffusers
    for (int ch = 0; ch < kNumFdnChannels; ++ch) {
        const auto idx = static_cast<std::size_t>(ch);
        const float baseSamples = sr * kBaseFdnDelayMs[idx] * 0.001f;
        fdnBaseDelaySamples_[idx] = baseSamples;
        fdnCurrentDelayLen_[idx] = baseSamples;
        fdnDelayStep_[idx] = 0.0f;
        fdnDelays_[idx].allocate(static_cast<int>(baseSamples * 2.5f) + 256);

        const int apSamples = std::max(8, static_cast<int>(sr * kInLoopDiffuserMs[idx] * 0.001f));
        fdnInLoopDiffusers_[idx].delay.allocate(apSamples * 2 + 64);
        fdnInLoopDiffusers_[idx].delaySamples = apSamples;
        fdnInLoopDiffusers_[idx].feedback = 0.56f;
    }

    for (int q = 0; q < 4; ++q) {
        fdnQuadLfoPhase_[static_cast<std::size_t>(q)] = 0.25f * static_cast<float>(q);
    }

    // Long, Valhalla-style dual-grain windows (186ms .. 197ms) with distinct RNG seeds per channel
    octaveShifterL_.prepare(sampleRate_, 0.00f, 186.0f, 0x13579BDFu);
    octaveShifterR_.prepare(sampleRate_, 0.37f, 197.0f, 0x2468ACE1u);
    octave2ShifterL_.prepare(sampleRate_, 0.19f, 174.0f, 0x369CF258u);
    octave2ShifterR_.prepare(sampleRate_, 0.61f, 183.0f, 0x48AE159Du);
    fifthShifterL_.prepare(sampleRate_, 0.11f, 181.0f, 0x5A5A5A5Au);
    fifthShifterR_.prepare(sampleRate_, 0.53f, 191.0f, 0x7C3E9F1Bu);

    // Smoothers
    drySmoother_.prepare(sampleRate_, 15.0f);
    mixSmoother_.prepare(sampleRate_, 15.0f);
    decaySmoother_.prepare(sampleRate_, 30.0f);
    sizeSmoother_.prepare(sampleRate_, 45.0f);
    preDelaySmoother_.prepare(sampleRate_, 35.0f);
    dampingSmoother_.prepare(sampleRate_, 20.0f);
    shimmerOctSmoother_.prepare(sampleRate_, 25.0f);
    shimmerFifthSmoother_.prepare(sampleRate_, 25.0f);
    duckingSmoother_.prepare(sampleRate_, 15.0f);
    erLevelSmoother_.prepare(sampleRate_, 15.0f);
    widthSmoother_.prepare(sampleRate_, 15.0f);
    outGainSmoother_.prepare(sampleRate_, 15.0f);

    reset();
    setParameters(params_);
}

void DualEngineReverb::reset() noexcept {
    preDelayL_.clear();
    preDelayR_.clear();
    inLowCutL_.reset();
    inLowCutR_.reset();
    inHighCutL_.reset();
    inHighCutR_.reset();
    vocalCarveL_.reset();
    vocalCarveR_.reset();
    shimmerSilkL_.reset();
    shimmerSilkR_.reset();
    shimmerInterStageL_.reset();
    shimmerInterStageR_.reset();
    shimmerHighPassL_.reset();
    shimmerHighPassR_.reset();
    shimmerCloudDelayL_.clear();
    shimmerCloudDelayR_.clear();

    for (int i = 0; i < 2; ++i) {
        shimmerDiffusersL_[static_cast<std::size_t>(i)].clear();
        shimmerDiffusersR_[static_cast<std::size_t>(i)].clear();
    }

    for (int i = 0; i < kNumDiffusers; ++i) {
        diffusersL_[static_cast<std::size_t>(i)].clear();
        diffusersR_[static_cast<std::size_t>(i)].clear();
    }
    erDelayL_.clear();
    erDelayR_.clear();

    const float initScale = std::clamp(params_.size, 0.2f, 1.5f) * getSpaceSizeScale(params_.spaceType);
    for (int ch = 0; ch < kNumFdnChannels; ++ch) {
        const auto idx = static_cast<std::size_t>(ch);
        fdnDelays_[idx].clear();
        fdnInLoopDiffusers_[idx].clear();
        fdnLpState_[idx] = 0.0f;
        fdnLowShelfState_[idx] = 0.0f;
        fdnDcInPrev_[idx] = 0.0f;
        fdnDcOutPrev_[idx] = 0.0f;
        fdnCurrentDelayLen_[idx] = std::max(16.0f, fdnBaseDelaySamples_[idx] * initScale);
        fdnDelayStep_[idx] = 0.0f;
    }

    for (int q = 0; q < 4; ++q) {
        fdnQuadLfoPhase_[static_cast<std::size_t>(q)] = 0.25f * static_cast<float>(q);
    }

    octaveShifterL_.clear(0.00f);
    octaveShifterR_.clear(0.37f);
    octave2ShifterL_.clear(0.19f);
    octave2ShifterR_.clear(0.61f);
    fifthShifterL_.clear(0.11f);
    fifthShifterR_.clear(0.53f);
    duckEnvelope_ = 0.0f;
    isTankSilent_ = true;
    silenceHoldSamples_ = 0;
    telemetryInPeak_.store(0.0f, std::memory_order_relaxed);
    telemetryOutPeak_.store(0.0f, std::memory_order_relaxed);
    telemetryTankEnergy_.store(0.0f, std::memory_order_relaxed);
}

void DualEngineReverb::setParameters(const Parameters& params) noexcept {
    params_ = params;

    inLowCutL_.setParameters(StateVariableFilter::Type::HighPass, params_.lowCutHz, 0.707f);
    inLowCutR_.setParameters(StateVariableFilter::Type::HighPass, params_.lowCutHz, 0.707f);
    inHighCutL_.setParameters(StateVariableFilter::Type::LowPass, params_.highCutHz, 0.707f);
    inHighCutR_.setParameters(StateVariableFilter::Type::LowPass, params_.highCutHz, 0.707f);

    // Vocal Carve gently attenuates 2.2 kHz on the wet reverb tail so vocals and piano cut through
    const float carveDb = -9.5f * std::clamp(params_.vocalCarve, 0.0f, 1.0f);
    vocalCarveL_.setParameters(StateVariableFilter::Type::Bell, 2200.0f, 0.85f, carveDb);
    vocalCarveR_.setParameters(StateVariableFilter::Type::Bell, 2200.0f, 0.85f, carveDb);

    const float shimHigh = std::clamp(params_.shimmerHighCutHz, 2500.0f, static_cast<float>(sampleRate_ * 0.45));
    shimmerSilkL_.setParameters(StateVariableFilter::Type::LowPass, shimHigh, 0.55f);
    shimmerSilkR_.setParameters(StateVariableFilter::Type::LowPass, shimHigh, 0.55f);
    shimmerInterStageL_.setParameters(StateVariableFilter::Type::LowPass, std::min(12500.0f, shimHigh * 1.15f), 0.58f);
    shimmerInterStageR_.setParameters(StateVariableFilter::Type::LowPass, std::min(12500.0f, shimHigh * 1.15f), 0.58f);

    const float effShimmerLowCut = (params_.shimmerPitchMode == 3) ? std::min(140.0f, params_.shimmerLowCutHz) : params_.shimmerLowCutHz;
    shimmerHighPassL_.setParameters(StateVariableFilter::Type::HighPass, effShimmerLowCut, 0.65f);
    shimmerHighPassR_.setParameters(StateVariableFilter::Type::HighPass, effShimmerLowCut, 0.65f);

    const float clampedDiff = std::clamp(params_.diffusion, 0.0f, 0.95f);
    const float diffFeedback = clampedDiff * 0.72f;
    for (int i = 0; i < kNumDiffusers; ++i) {
        diffusersL_[static_cast<std::size_t>(i)].feedback = diffFeedback;
        diffusersR_[static_cast<std::size_t>(i)].feedback = diffFeedback;
    }

    const float inLoopApFeedback = 0.35f + clampedDiff * 0.28f;
    for (int ch = 0; ch < kNumFdnChannels; ++ch) {
        fdnInLoopDiffusers_[static_cast<std::size_t>(ch)].feedback = inLoopApFeedback;
    }

    const float tDry      = std::clamp(params_.dry, 0.0f, 1.0f);
    const float tMix      = std::clamp(params_.mix, 0.0f, 1.0f);
    const float tDecay    = std::clamp(params_.decaySeconds, 0.15f, 35.0f);
    const float tSize     = std::clamp(params_.size, 0.2f, 1.5f);
    const float tPreDelay = std::clamp(params_.preDelayMs, 0.0f, 250.0f);
    const float tDamp     = std::clamp(params_.highDamping, 0.0f, 0.95f);
    const float tShimOct  = std::clamp(params_.shimmerOctave, 0.0f, 1.0f);
    const float tShim5th  = std::clamp(params_.shimmerFifth, 0.0f, 1.0f);
    const float tDuck     = std::clamp(params_.ducking, 0.0f, 1.0f);
    const float tEr       = std::clamp(params_.erLevel, 0.0f, 1.0f);
    const float tWidth    = std::clamp(params_.stereoWidth, 0.0f, 1.5f);
    const float tOutGain  = std::pow(10.0f, std::clamp(params_.outputGainDb, -24.0f, 12.0f) / 20.0f);

    if (isTankSilent_) {
        // When tank is completely silent (e.g. after prepare/reset or preset load),
        // snap smoothers and delay lengths immediately to prevent initial Doppler sweep!
        drySmoother_.reset(tDry);
        mixSmoother_.reset(tMix);
        decaySmoother_.reset(tDecay);
        sizeSmoother_.reset(tSize);
        preDelaySmoother_.reset(tPreDelay);
        dampingSmoother_.reset(tDamp);
        shimmerOctSmoother_.reset(tShimOct);
        shimmerFifthSmoother_.reset(tShim5th);
        duckingSmoother_.reset(tDuck);
        erLevelSmoother_.reset(tEr);
        widthSmoother_.reset(tWidth);
        outGainSmoother_.reset(tOutGain);

        const float scale = tSize * getSpaceSizeScale(params_.spaceType);
        for (int ch = 0; ch < kNumFdnChannels; ++ch) {
            const auto idx = static_cast<std::size_t>(ch);
            fdnCurrentDelayLen_[idx] = std::max(16.0f, fdnBaseDelaySamples_[idx] * scale);
            fdnDelayStep_[idx] = 0.0f;
        }
    } else {
        drySmoother_.setTargetValue(tDry);
        mixSmoother_.setTargetValue(tMix);
        decaySmoother_.setTargetValue(tDecay);
        sizeSmoother_.setTargetValue(tSize);
        preDelaySmoother_.setTargetValue(tPreDelay);
        dampingSmoother_.setTargetValue(tDamp);
        shimmerOctSmoother_.setTargetValue(tShimOct);
        shimmerFifthSmoother_.setTargetValue(tShim5th);
        duckingSmoother_.setTargetValue(tDuck);
        erLevelSmoother_.setTargetValue(tEr);
        widthSmoother_.setTargetValue(tWidth);
        outGainSmoother_.setTargetValue(tOutGain);
    }
}

void DualEngineReverb::processBlock(float* leftChannel, float* rightChannel, int numSamples) noexcept {
    if (leftChannel == nullptr || rightChannel == nullptr || numSamples <= 0) {
        return;
    }

    const float sr = static_cast<float>(sampleRate_);
    const float invSr = 1.0f / sr;
    const bool isLush = (params_.engineMode == EngineMode::LushCloud);
    const bool isFrozen = params_.freeze;

    // Quick peak scan for sleep-on-silence detection
    float blockInPeak = 0.0f;
    for (int s = 0; s < numSamples; ++s) {
        blockInPeak = std::max(blockInPeak, std::max(std::abs(leftChannel[s]), std::abs(rightChannel[s])));
    }

    const float lastTankEnergy = telemetryTankEnergy_.load(std::memory_order_relaxed);
    if (blockInPeak >= 1e-6f) {
        silenceHoldSamples_ = static_cast<int>(sr * 0.65f);
    } else if (silenceHoldSamples_ > 0) {
        silenceHoldSamples_ -= numSamples;
    }

    // Sleep-on-Silence: If input is silent, transit hold-off has elapsed, reverb is not frozen, and tank energy < -100 dBFS
    if (blockInPeak < 1e-6f && silenceHoldSamples_ <= 0 && !isFrozen && lastTankEnergy < 1e-5f) {
        isTankSilent_ = true;
        std::fill_n(leftChannel, numSamples, 0.0f);
        std::fill_n(rightChannel, numSamples, 0.0f);
        telemetryInPeak_.store(0.0f, std::memory_order_relaxed);
        telemetryOutPeak_.store(0.0f, std::memory_order_relaxed);
        telemetryTankEnergy_.store(0.0f, std::memory_order_relaxed);
        return;
    }
    isTankSilent_ = false;

    const float spaceSizeScale = getSpaceSizeScale(params_.spaceType);
    const float spaceErScale = getSpaceErScale(params_.spaceType);

    // Envelope follower attack/release coefficients for ducking
    const float envAttack = 1.0f - std::exp(-1.0f / (0.008f * sr));
    const float envRelease = 1.0f - std::exp(-1.0f / (0.280f * sr));

    // Sample-rate-invariant filter coefficients for tank 3-band crossover and 28 Hz DC blocker
    const float hfCutCoeff = std::exp(-2.0f * std::numbers::pi_v<float> * 4200.0f * invSr);
    const float lfShelfCoeff = 1.0f - std::exp(-2.0f * std::numbers::pi_v<float> * 220.0f * invSr);
    const float dcBlockerPole = std::exp(-2.0f * std::numbers::pi_v<float> * 28.0f * invSr);

    // Zero-Pitch-Drift Quadrature LFO Modulation:
    // Each quad of 4 FDN channels uses (+sin, -sin, +cos, -cos) so net Doppler pitch shift is identically 0.0!
    const float srScale = sr / 48000.0f;
    const float baseLfoRate = isLush
        ? std::clamp(params_.modRateHz * 0.45f, 0.06f, 0.90f)
        : std::clamp(params_.modRateHz * 0.25f, 0.04f, 0.50f);
    const float lfoInc = baseLfoRate * invSr;
    const float clampedModDepth = std::clamp(params_.modDepth, 0.0f, 1.0f);
    const float modDepthSamples = srScale * (isLush
        ? (clampedModDepth * 2.2f)
        : (clampedModDepth * 0.8f));

    float blockOutPeak = 0.0f;
    float blockTankEnergy = 0.0f;
    float runningTankEnergy = std::clamp(lastTankEnergy, 0.0f, 2.0f);
    std::array<float, kNumFdnChannels> chanEnergyAccum{};

    // Precompute per-channel RT60 attenuation and HF damping ratio at control rate (every 16 samples)
    std::array<float, kNumFdnChannels> cachedRt60Gain{};
    std::array<float, kNumFdnChannels> cachedHfRatio{};
    const float initDecaySec = std::max(0.15f, decaySmoother_.getCurrentValue());
    const float initSizeParam = sizeSmoother_.getCurrentValue() * spaceSizeScale;
    const float initHighDamp = dampingSmoother_.getCurrentValue();
    for (int ch = 0; ch < kNumFdnChannels; ++ch) {
        const auto idx = static_cast<std::size_t>(ch);
        const float baseLen = std::max(16.0f, fdnBaseDelaySamples_[idx] * initSizeParam);
        const float gMid = isFrozen
            ? 1.0f
            : std::pow(10.0f, (-3.0f * (baseLen * invSr)) / initDecaySec);
        cachedRt60Gain[idx] = gMid;
        cachedHfRatio[idx] = isFrozen ? 1.0f : std::pow(gMid, 2.4f * initHighDamp);
    }

    for (int s = 0; s < numSamples; ++s) {
        const float dryL = flushDenormal(leftChannel[s]);
        const float dryR = flushDenormal(rightChannel[s]);
        const float inAbs = std::max(std::abs(dryL), std::abs(dryR));
        blockInPeak = std::max(blockInPeak, inAbs);

        // Smooth parameters
        const float dryLvl = drySmoother_.getNextValue();
        const float mix = mixSmoother_.getNextValue();
        const float decaySec = decaySmoother_.getNextValue();
        const float sizeParam = sizeSmoother_.getNextValue() * spaceSizeScale;
        const float preDelayMs = preDelaySmoother_.getNextValue();
        const float highDamp = dampingSmoother_.getNextValue();
        const float rawShimOct = shimmerOctSmoother_.getNextValue();
        const float rawShimFifth = shimmerFifthSmoother_.getNextValue();
        const float shimOct = isLush ? rawShimOct : (rawShimOct * 0.80f);
        const float shimFifth = isLush ? rawShimFifth : (rawShimFifth * 0.65f);
        const float duckAmount = duckingSmoother_.getNextValue();
        const float erLevel = erLevelSmoother_.getNextValue();
        const float width = widthSmoother_.getNextValue();
        const float outGain = outGainSmoother_.getNextValue();

        // Update balanced quadrature LFO delay offsets and delay-compensated RT60 gains every 16 samples
        if ((s & 15) == 0) {
            const float stepLfoInc = lfoInc * 16.0f;
            constexpr float inv16 = 1.0f / 16.0f;
            const float safeDecay = std::max(0.15f, decaySec);

            for (int q = 0; q < 4; ++q) {
                const auto qIdx = static_cast<std::size_t>(q);
                float phase = fdnQuadLfoPhase_[qIdx] + stepLfoInc * kQuadRateMultipliers[qIdx];
                if (phase >= 1.0f) phase -= 1.0f;
                fdnQuadLfoPhase_[qIdx] = phase;

                const float ang = 2.0f * std::numbers::pi_v<float> * phase;
                const float sinMod = std::sin(ang) * modDepthSamples;
                const float cosMod = std::cos(ang) * modDepthSamples;
                // Balanced zero-sum quad offsets: (+sin) + (-sin) + (+cos) + (-cos) == 0.0
                const std::array<float, 4> quadOffsets = { sinMod, -sinMod, cosMod, -cosMod };

                for (int sub = 0; sub < 4; ++sub) {
                    const auto idx = static_cast<std::size_t>(q * 4 + sub);
                    const float targetLen = std::max(16.0f, fdnBaseDelaySamples_[idx] * sizeParam + quadOffsets[static_cast<std::size_t>(sub)]);
                    fdnDelayStep_[idx] = (targetLen - fdnCurrentDelayLen_[idx]) * inv16;

                    const float gMid = isFrozen
                        ? 1.0f
                        : std::pow(10.0f, (-3.0f * (targetLen * invSr)) / safeDecay);
                    cachedRt60Gain[idx] = gMid;
                    // Delay-length-compensated HF damping ensures all 16 channels share uniform HF RT60
                    cachedHfRatio[idx] = isFrozen ? 1.0f : std::pow(gMid, 2.4f * highDamp);
                }
            }
        }

        // 1. Dynamic Ducking Envelope Follower
        const float envCoeff = (inAbs > duckEnvelope_) ? envAttack : envRelease;
        duckEnvelope_ += envCoeff * (inAbs - duckEnvelope_);
        const float duckReduction = std::clamp(1.0f - duckAmount * std::min(1.0f, duckEnvelope_ * 3.2f), 0.15f, 1.0f);

        // 2. Pre-Delay (4-point cubic interpolation for zipper-free pre-delay changes)
        const float preDelaySamples = preDelayMs * 0.001f * sr;
        preDelayL_.write(dryL);
        preDelayR_.write(dryR);
        float inL = (preDelaySamples > 2.0f) ? preDelayL_.readCubic(preDelaySamples) : dryL;
        float inR = (preDelaySamples > 2.0f) ? preDelayR_.readCubic(preDelaySamples) : dryR;

        // 3. Input Conditioning Filters
        inL = inHighCutL_.processSample(inLowCutL_.processSample(inL));
        inR = inHighCutR_.processSample(inLowCutR_.processSample(inR));

        // 4. Input Allpass Diffusion Cascade
        float diffL = inL;
        float diffR = inR;
        for (int d = 0; d < kNumDiffusers; ++d) {
            diffL = diffusersL_[static_cast<std::size_t>(d)].process(diffL);
            diffR = diffusersR_[static_cast<std::size_t>(d)].process(diffR);
        }

        // 5. Early Reflections (12-tap geometric stereo network)
        erDelayL_.write(diffL);
        erDelayR_.write(diffR);
        float erOutL = 0.0f;
        float erOutR = 0.0f;
        for (int t = 0; t < kNumEarlyTaps; ++t) {
            const int tapL = std::max(2, static_cast<int>(static_cast<float>(erTapSamplesL_[static_cast<std::size_t>(t)]) * spaceErScale));
            const int tapR = std::max(2, static_cast<int>(static_cast<float>(erTapSamplesR_[static_cast<std::size_t>(t)]) * spaceErScale));
            const float g = erTapGains_[static_cast<std::size_t>(t)];
            erOutL += erDelayL_.readInt(tapL) * g;
            erOutR += erDelayR_.readInt(tapR) * g;
        }
        erOutL *= 0.28f;
        erOutR *= 0.28f;

        // 6. 16-Channel FDN Readout, In-Loop Valhalla Shimmer Blend & 3-Band RT60 Damping
        // Notice: gRt60 is NEVER scaled down by shimmer! Instead, each of the 16 channels blends its
        // unshifted diffused tap with its own decorrelated shimmer tap from shimmerCloudDelayL_/R_.
        std::array<float, kNumFdnChannels> fdnRead{};
        float lateOutL = 0.0f;
        float lateOutR = 0.0f;

        const float bassBoost = isFrozen ? 1.0f : std::clamp(params_.bassMultiplier, 0.5f, 1.8f);
        const float rawActiveShim = std::clamp(shimOct + 0.55f * shimFifth, 0.0f, 1.0f);
        const float activeShim = (rawActiveShim > 0.0005f) ? std::pow(rawActiveShim, 0.72f) : 0.0f;
        // Because pitch shifting is strictly triangular in octave space (f0 -> 2*f0 -> 4*f0 -> 8*f0 -> absorbed
        // by 5.2 kHz LPF, or -> 0.5*f0 -> absorbed by 185 Hz HPF), the asymptotic RT60 eigenvalues of the tank
        // are determined by unshiftedLoopGain * gRt60. Keeping unshiftedLoopGain == 1.0 preserves 100% of RT60
        // while generating the Gamma-envelope (t^k * exp(-t/tau)) Valhalla multi-octave shimmer bloom!
        const float unshiftedLoopGain = 1.0f;
        const float shimmerLoopGain = isFrozen ? 0.0f : (0.44f * activeShim / (1.0f + 1.4f * runningTankEnergy));

        for (int ch = 0; ch < kNumFdnChannels; ++ch) {
            const auto idx = static_cast<std::size_t>(ch);
            fdnCurrentDelayLen_[idx] += fdnDelayStep_[idx];

            // Cubic Hermite delay readout + In-Loop Allpass Diffuser for lush, non-metallic modal density
            const float rawTap = fdnDelays_[idx].readCubic(fdnCurrentDelayLen_[idx]);
            const float diffusedTap = fdnInLoopDiffusers_[idx].process(rawTap);

            // Read this channel's decorrelated shimmer tap from the 16-tap shimmer cloud delay line
            const float shimTap = ((ch & 1) == 0)
                ? shimmerCloudDelayL_.readInt(shimmerCloudTapSamples_[idx])
                : shimmerCloudDelayR_.readInt(shimmerCloudTapSamples_[idx]);

            const float loopTap = diffusedTap * unshiftedLoopGain + shimTap * shimmerLoopGain;

            const float gRt60 = cachedRt60Gain[idx];
            const float hfRatio = cachedHfRatio[idx];

            // Delay-compensated HF damping (uniform high-frequency decay across all 16 channels)
            float lp = flushDenormal(fdnLpState_[idx] * hfCutCoeff + loopTap * (1.0f - hfCutCoeff));
            fdnLpState_[idx] = lp;
            const float hfDamped = isFrozen ? loopTap : (loopTap * hfRatio + lp * (1.0f - hfRatio));

            // Smooth Low-Shelf RT60 contour
            float lowState = flushDenormal(fdnLowShelfState_[idx] + lfShelfCoeff * (hfDamped - fdnLowShelfState_[idx]));
            fdnLowShelfState_[idx] = lowState;

            const float bandDamped = (hfDamped + (bassBoost - 1.0f) * 0.12f * lowState) * gRt60;

            // 28 Hz DC & sub-rumble blocker inside tank
            const float dcOut = isFrozen
                ? bandDamped
                : flushDenormal(bandDamped - fdnDcInPrev_[idx] + dcBlockerPole * fdnDcOutPrev_[idx]);
            fdnDcInPrev_[idx] = bandDamped;
            fdnDcOutPrev_[idx] = dcOut;

            fdnRead[idx] = dcOut;

            lateOutL += dcOut * kReadoutSignsL[idx];
            lateOutR += dcOut * kReadoutSignsR[idx];

            chanEnergyAccum[idx] = std::max(chanEnergyAccum[idx], std::abs(dcOut));
        }

        lateOutL *= 0.30f;
        lateOutR *= 0.30f;

        // Track smooth tank energy for self-regulating shimmer loop stability
        const float instantTank = 0.5f * (std::abs(lateOutL) + std::abs(lateOutR));
        runningTankEnergy = 0.995f * runningTankEnergy + 0.005f * instantTank;

        // 7. Valhalla-Style Regenerative Dual-Grain Shimmer Cloud
        // Driven primarily by the normalized FDN tank readout (lateOutL/R * 1.18f) so shimmer swells
        // organically from the reverb tail and cascades across octaves (f0 -> 2*f0 -> 4*f0 -> 8*f0),
        // with a gentle diffused-input seed (diffL/R * 0.25f) so the first octave bloom starts smoothly.
        float shimmerHaloL = 0.0f;
        float shimmerHaloR = 0.0f;
        if (shimOct > 0.0005f || shimFifth > 0.0005f) {
            const float shimInL = lateOutL * 1.28f + (isFrozen ? 0.0f : diffL * 0.52f);
            const float shimInR = lateOutR * 1.28f + (isFrozen ? 0.0f : diffR * 0.52f);

            // HighPass at 185 Hz before pitch shifting keeps low-frequency mud out of the grains
            const float hpL = shimmerHighPassL_.processSample(shimInL);
            const float hpR = shimmerHighPassR_.processSample(shimInR);

            float sL = 0.0f;
            float sR = 0.0f;
            if (shimOct > 0.0005f) {
                switch (params_.shimmerPitchMode) {
                    case 1: { // +2 Octaves (+24st) via smooth 2x -> 2x cascade
                        const float oct1L = shimmerInterStageL_.processSample(octaveShifterL_.process(hpL, 2.0f));
                        const float oct1R = shimmerInterStageR_.processSample(octaveShifterR_.process(hpR, 2.0f));
                        sL = octave2ShifterL_.process(oct1L, 2.0f);
                        sR = octave2ShifterR_.process(oct1R, 2.0f);
                        break;
                    }
                    case 2: { // +1 & +2 Octaves Combined - Lush Majestic Worship Halo
                        const float oct1L = octaveShifterL_.process(hpL, 2.0f);
                        const float oct1R = octaveShifterR_.process(hpR, 2.0f);
                        const float filt1L = shimmerInterStageL_.processSample(oct1L);
                        const float filt1R = shimmerInterStageR_.processSample(oct1R);
                        const float oct2L = octave2ShifterL_.process(filt1L, 2.0f);
                        const float oct2R = octave2ShifterR_.process(filt1R, 2.0f);
                        sL = oct1L * 0.72f + oct2L * 0.48f;
                        sR = oct1R * 0.72f + oct2R * 0.48f;
                        break;
                    }
                    case 3: // -1 Octave Sub - Deep Atmospheric Foundation
                        sL = octaveShifterL_.process(hpL, 0.5f);
                        sR = octaveShifterR_.process(hpR, 0.5f);
                        break;
                    case 0: // +1 Octave (+12st) - Classic Valhalla Angelic Bloom
                    default:
                        sL = octaveShifterL_.process(hpL, 2.0f);
                        sR = octaveShifterR_.process(hpR, 2.0f);
                        break;
                }
            }

            // Harmonic Fifth (+7st): gentle fifth glow
            if (shimFifth > 0.0005f && !isFrozen) {
                const float fL = fifthShifterL_.process(hpL * 0.75f, 1.498307f);
                const float fR = fifthShifterR_.process(hpR * 0.75f, 1.498307f);
                sL = sL * (shimOct > 0.0005f ? 1.0f : 0.0f) + fL * 0.65f;
                sR = sR * (shimOct > 0.0005f ? 1.0f : 0.0f) + fR * 0.65f;
            }

            // Warm 5.2 kHz LowPass melts away ultrasonic grain edges so ascending octaves stay silky
            sL = shimmerSilkL_.processSample(sL);
            sR = shimmerSilkR_.processSample(sR);

            // 2-stage prime allpass diffusion + stereo cross-scattering for enveloping cloud width
            sL = shimmerDiffusersL_[0].process(sL);
            sR = shimmerDiffusersR_[0].process(sR);
            const float crossL = 0.84f * sL + 0.16f * sR;
            const float crossR = 0.84f * sR + 0.16f * sL;
            const float cloudL = shimmerDiffusersL_[1].process(crossL);
            const float cloudR = shimmerDiffusersR_[1].process(crossR);

            shimmerCloudDelayL_.write(flushDenormal(cloudL));
            shimmerCloudDelayR_.write(flushDenormal(cloudR));

            // Delayed stereo halo tap from the cloud (after ~38ms diffusion delay) for wide cathedral bloom
            shimmerHaloL = shimmerCloudDelayL_.readInt(shimmerCloudTapSamples_[9]) * activeShim;
            shimmerHaloR = shimmerCloudDelayR_.readInt(shimmerCloudTapSamples_[10]) * activeShim;
        } else {
            shimmerCloudDelayL_.write(0.0f);
            shimmerCloudDelayR_.write(0.0f);
        }

        // 8. Unitary 16x16 Fast Walsh-Hadamard Transform (FWHT-16) Feedback Matrix:
        // Scatters every channel equally (+/- 0.25) across all 16 delay lines on every pass.
        std::array<float, kNumFdnChannels> hadamard = fdnRead;
        for (std::size_t i = 0; i < 16; i += 2) {
            const float a = hadamard[i];
            const float b = hadamard[i + 1];
            hadamard[i]     = a + b;
            hadamard[i + 1] = a - b;
        }
        for (std::size_t i = 0; i < 16; i += 4) {
            for (std::size_t j = 0; j < 2; ++j) {
                const float a = hadamard[i + j];
                const float b = hadamard[i + j + 2];
                hadamard[i + j]     = a + b;
                hadamard[i + j + 2] = a - b;
            }
        }
        for (std::size_t i = 0; i < 16; i += 8) {
            for (std::size_t j = 0; j < 4; ++j) {
                const float a = hadamard[i + j];
                const float b = hadamard[i + j + 4];
                hadamard[i + j]     = a + b;
                hadamard[i + j + 4] = a - b;
            }
        }
        for (std::size_t j = 0; j < 8; ++j) {
            const float a = hadamard[j];
            const float b = hadamard[j + 8];
            hadamard[j]     = (a + b) * 0.25f;
            hadamard[j + 8] = (a - b) * 0.25f;
        }

        const float tankInputScale = isFrozen ? 0.0f : (0.26f * (1.0f - 0.38f * activeShim));
        const float feedL = diffL * tankInputScale;
        const float feedR = diffR * tankInputScale;

        for (int ch = 0; ch < kNumFdnChannels; ++ch) {
            const auto idx = static_cast<std::size_t>(ch);
            // Odd cyclic permutation (ch + 3) breaks symmetric 2-cycles while preserving 100% unitarity
            const auto permIdx = static_cast<std::size_t>((ch + 3) & (kNumFdnChannels - 1));
            const float matrixOut = hadamard[permIdx];
            const float inExc = ((ch & 1) == 0) ? feedL : feedR;

            const float nextVal = softClipTank(matrixOut + inExc * kInputExcitationSigns[idx]);
            fdnDelays_[idx].write(flushDenormal(nextVal));
        }

        // 9. Combine Soft Early Reflections, Dense 16-Channel Late Tank & Enveloping Shimmer Cloud Halo
        // In LushCloud mode, early reflections are smoothed into diffuse cloud mist (erScale 0.04)
        // rather than discrete acoustic room reflections to avoid metallic/grainy slap.
        const float erScale = isLush ? (erLevel * 0.04f) : (erLevel * 0.38f);
        const float tankWetScale = 1.15f - 0.22f * activeShim;
        float wetL = erOutL * erScale + lateOutL * tankWetScale + shimmerHaloL * 0.78f;
        float wetR = erOutR * erScale + lateOutR * tankWetScale + shimmerHaloR * 0.78f;

        wetL = vocalCarveL_.processSample(wetL) * duckReduction;
        wetR = vocalCarveR_.processSample(wetR) * duckReduction;

        // Mid/Side Stereo Width
        const float mid = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR) * width;
        wetL = mid + side;
        wetR = mid - side;

        // Independent Parallel Dry & Wet summing: Dry synth remains 100% solid & punchy!
        const float outL = (dryL * dryLvl + wetL * mix) * outGain;
        const float outR = (dryR * dryLvl + wetR * mix) * outGain;

        leftChannel[s] = outL;
        rightChannel[s] = outR;

        blockOutPeak = std::max(blockOutPeak, std::max(std::abs(outL), std::abs(outR)));
        blockTankEnergy = std::max(blockTankEnergy, instantTank + 0.35f * (std::abs(shimmerHaloL) + std::abs(shimmerHaloR)));
    }

    // Store lock-free telemetry for 60-FPS UI
    telemetryInPeak_.store(blockInPeak, std::memory_order_relaxed);
    telemetryOutPeak_.store(blockOutPeak, std::memory_order_relaxed);
    telemetryTankEnergy_.store(blockTankEnergy, std::memory_order_relaxed);
    const float lastDuckAmount = duckingSmoother_.getCurrentValue();
    const float lastDuckLin = std::clamp(1.0f - lastDuckAmount * std::min(1.0f, duckEnvelope_ * 3.2f), 0.15f, 1.0f);
    telemetryDuckDb_.store(20.0f * std::log10(lastDuckLin), std::memory_order_relaxed);

    for (int ch = 0; ch < kNumFdnChannels; ++ch) {
        telemetryChannelEnergy_[static_cast<std::size_t>(ch)].store(
            chanEnergyAccum[static_cast<std::size_t>(ch)], std::memory_order_relaxed);
    }
}

DualEngineReverb::Telemetry DualEngineReverb::getTelemetry() const noexcept {
    Telemetry t{};
    t.inputPeak = telemetryInPeak_.load(std::memory_order_relaxed);
    t.outputPeak = telemetryOutPeak_.load(std::memory_order_relaxed);
    t.tankEnergy = telemetryTankEnergy_.load(std::memory_order_relaxed);
    t.duckingGainReductionDb = telemetryDuckDb_.load(std::memory_order_relaxed);
    for (int ch = 0; ch < kNumFdnChannels; ++ch) {
        t.channelEnergies[static_cast<std::size_t>(ch)] =
            telemetryChannelEnergy_[static_cast<std::size_t>(ch)].load(std::memory_order_relaxed);
    }
    return t;
}

} // namespace nova::dsp
