#include "nova_dsp/reverb/DualEngineReverb.h"
#include "DualEngineReverbHelpers.h"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace nova::dsp {

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
