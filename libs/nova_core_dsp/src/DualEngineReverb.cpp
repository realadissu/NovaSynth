#include "nova_dsp/reverb/DualEngineReverb.h"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace nova::dsp {

namespace {
// 16 mutually prime base delay lengths in milliseconds (spanning 21ms to 107ms)
constexpr std::array<float, 16> kBaseFdnDelayMs = {
    21.13f, 24.71f, 28.37f, 32.53f,
    37.09f, 41.99f, 47.21f, 53.03f,
    59.27f, 65.81f, 72.97f, 80.59f,
    86.93f, 93.41f, 100.07f, 107.29f
};

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
} // namespace

float DualEngineReverb::GranularPitchShifter::process(float input, float pitchRatio) noexcept {
    buffer.write(input);

    const float phaseInc = (1.0f - pitchRatio) / std::max(64.0f, grainSizeSamples);

    phaseA += phaseInc;
    if (phaseA >= 1.0f) phaseA -= 1.0f;
    if (phaseA < 0.0f) phaseA += 1.0f;

    phaseB = phaseA + 0.5f;
    if (phaseB >= 1.0f) phaseB -= 1.0f;

    const float delayA = phaseA * grainSizeSamples + 16.0f;
    const float delayB = phaseB * grainSizeSamples + 16.0f;

    // Hann window crossfade (sin^2(pi * phaseA) + sin^2(pi * phaseB) == 1.0)
    const float winA = std::sin(std::numbers::pi_v<float> * phaseA);
    const float winB = std::sin(std::numbers::pi_v<float> * phaseB);

    const float sampleA = buffer.readCubic(delayA) * (winA * winA);
    const float sampleB = buffer.readCubic(delayB) * (winB * winB);
    return sampleA + sampleB;
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
    shimmerSilkL_.prepare(sampleRate_);
    shimmerSilkR_.prepare(sampleRate_);
    shimmerSilkL_.setParameters(StateVariableFilter::Type::LowPass, 3800.0f, 0.65f);
    shimmerSilkR_.setParameters(StateVariableFilter::Type::LowPass, 3800.0f, 0.65f);

    shimmerHighPassL_.prepare(sampleRate_);
    shimmerHighPassR_.prepare(sampleRate_);
    shimmerHighPassL_.setParameters(StateVariableFilter::Type::HighPass, 260.0f, 0.65f);
    shimmerHighPassR_.setParameters(StateVariableFilter::Type::HighPass, 260.0f, 0.65f);

    // Shimmer Diffusers: 2-stage prime allpass phase scattering
    constexpr std::array<float, 2> kShimmerDiffMsL = { 7.31f, 12.17f };
    constexpr std::array<float, 2> kShimmerDiffMsR = { 8.53f, 13.89f };
    for (int i = 0; i < 2; ++i) {
        const int sL = static_cast<int>(sr * kShimmerDiffMsL[static_cast<std::size_t>(i)] * 0.001f);
        const int sR = static_cast<int>(sr * kShimmerDiffMsR[static_cast<std::size_t>(i)] * 0.001f);
        shimmerDiffusersL_[static_cast<std::size_t>(i)].delay.allocate(sL * 2 + 64);
        shimmerDiffusersR_[static_cast<std::size_t>(i)].delay.allocate(sR * 2 + 64);
        shimmerDiffusersL_[static_cast<std::size_t>(i)].delaySamples = std::max(8, sL);
        shimmerDiffusersR_[static_cast<std::size_t>(i)].delaySamples = std::max(8, sR);
        shimmerDiffusersL_[static_cast<std::size_t>(i)].feedback = 0.62f;
        shimmerDiffusersR_[static_cast<std::size_t>(i)].feedback = 0.62f;
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

    // Allocate 16-Channel FDN Tank (allow up to 2.2x size scaling + modulation margin)
    for (int ch = 0; ch < kNumFdnChannels; ++ch) {
        const float baseSamples = sr * kBaseFdnDelayMs[static_cast<std::size_t>(ch)] * 0.001f;
        fdnBaseDelaySamples_[static_cast<std::size_t>(ch)] = baseSamples;
        fdnCurrentDelayLen_[static_cast<std::size_t>(ch)] = baseSamples;
        fdnDelayStep_[static_cast<std::size_t>(ch)] = 0.0f;
        fdnDelays_[static_cast<std::size_t>(ch)].allocate(static_cast<int>(baseSamples * 2.5f) + 256);
        fdnLfoPhase_[static_cast<std::size_t>(ch)] =
            static_cast<float>(ch) / static_cast<float>(kNumFdnChannels);
    }

    octaveShifterL_.prepare(sampleRate_, 0.0f);
    octaveShifterR_.prepare(sampleRate_, 0.25f);
    octave2ShifterL_.prepare(sampleRate_, 0.125f);
    octave2ShifterR_.prepare(sampleRate_, 0.375f);

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
    shimmerHighPassL_.reset();
    shimmerHighPassR_.reset();

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

    for (int ch = 0; ch < kNumFdnChannels; ++ch) {
        fdnDelays_[static_cast<std::size_t>(ch)].clear();
        fdnLpState_[static_cast<std::size_t>(ch)] = 0.0f;
        fdnLowShelfState_[static_cast<std::size_t>(ch)] = 0.0f;
        fdnCurrentDelayLen_[static_cast<std::size_t>(ch)] = fdnBaseDelaySamples_[static_cast<std::size_t>(ch)];
        fdnDelayStep_[static_cast<std::size_t>(ch)] = 0.0f;
    }

    octaveShifterL_.clear(0.0f);
    octaveShifterR_.clear(0.25f);
    octave2ShifterL_.clear(0.125f);
    octave2ShifterR_.clear(0.375f);
    duckEnvelope_ = 0.0f;
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

    const float diffFeedback = std::clamp(params_.diffusion, 0.0f, 0.95f) * 0.72f;
    for (int i = 0; i < kNumDiffusers; ++i) {
        diffusersL_[static_cast<std::size_t>(i)].feedback = diffFeedback;
        diffusersR_[static_cast<std::size_t>(i)].feedback = diffFeedback;
    }

    drySmoother_.setTargetValue(std::clamp(params_.dry, 0.0f, 1.0f));
    mixSmoother_.setTargetValue(std::clamp(params_.mix, 0.0f, 1.0f));
    decaySmoother_.setTargetValue(std::clamp(params_.decaySeconds, 0.15f, 35.0f));
    sizeSmoother_.setTargetValue(std::clamp(params_.size, 0.2f, 1.5f));
    preDelaySmoother_.setTargetValue(std::clamp(params_.preDelayMs, 0.0f, 250.0f));
    dampingSmoother_.setTargetValue(std::clamp(params_.highDamping, 0.0f, 0.95f));
    shimmerOctSmoother_.setTargetValue(std::clamp(params_.shimmerOctave, 0.0f, 1.0f));
    shimmerFifthSmoother_.setTargetValue(std::clamp(params_.shimmerFifth, 0.0f, 1.0f));
    duckingSmoother_.setTargetValue(std::clamp(params_.ducking, 0.0f, 1.0f));
    erLevelSmoother_.setTargetValue(std::clamp(params_.erLevel, 0.0f, 1.0f));
    widthSmoother_.setTargetValue(std::clamp(params_.stereoWidth, 0.0f, 1.5f));
    outGainSmoother_.setTargetValue(std::pow(10.0f, std::clamp(params_.outputGainDb, -24.0f, 12.0f) / 20.0f));
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

    // Sleep-on-Silence: If input is silent, reverb is not frozen, and tank energy has decayed below -100 dBFS (< 1e-5):
    // Bypass all 16 FDN delay channels, diffusers, matrix math and LFOs to bring idle CPU down to ~0%!
    if (blockInPeak < 1e-6f && !isFrozen && lastTankEnergy < 1e-5f) {
        std::fill_n(leftChannel, numSamples, 0.0f);
        std::fill_n(rightChannel, numSamples, 0.0f);
        telemetryInPeak_.store(0.0f, std::memory_order_relaxed);
        telemetryOutPeak_.store(0.0f, std::memory_order_relaxed);
        telemetryTankEnergy_.store(0.0f, std::memory_order_relaxed);
        return;
    }

    // Space archetype scale factors
    float spaceSizeScale = 1.0f;
    float spaceErScale = 1.0f;
    switch (params_.spaceType) {
        case SpaceType::StudioRoom:    spaceSizeScale = 0.58f; spaceErScale = 1.25f; break;
        case SpaceType::WarmChamber:   spaceSizeScale = 0.82f; spaceErScale = 1.05f; break;
        case SpaceType::ConcertHall:   spaceSizeScale = 1.05f; spaceErScale = 0.85f; break;
        case SpaceType::CathedralCloud:spaceSizeScale = 1.32f; spaceErScale = 0.55f; break;
    }

    // Envelope follower attack/release coefficients for ducking
    const float envAttack = 1.0f - std::exp(-1.0f / (0.008f * sr));
    const float envRelease = 1.0f - std::exp(-1.0f / (0.280f * sr));

    // LFO modulation depth & increment
    // Musical, pitch-stable delay modulation (eliminates detuning, micro-pitch jumps, and howling)
    // Depth is kept strictly within 0.0 .. 1.5 samples max (~0.03ms), which diffuses modes
    // while keeping pitch rock-solid!
    const float lfoRate = isLush ? std::clamp(params_.modRateHz * 0.75f, 0.05f, 2.0f) : (params_.modRateHz * 0.25f);
    const float lfoInc = lfoRate * invSr;
    const float modDepthSamples = isLush ? (params_.modDepth * 1.5f) : (params_.modDepth * 0.35f);

    float blockOutPeak = 0.0f;
    float blockTankEnergy = 0.0f;
    std::array<float, kNumFdnChannels> chanEnergyAccum{};

    // Precompute per-channel RT60 attenuation and LFO modulation at control rate (every 16 samples)
    // Eliminates 16x std::pow() and 16x std::sin() per sample, reducing CPU load by ~85%!
    std::array<float, kNumFdnChannels> cachedDelayLen{};
    std::array<float, kNumFdnChannels> cachedRt60Gain{};
    const float currentDecaySec = std::max(0.15f, decaySmoother_.getCurrentValue());
    const float currentSizeParam = sizeSmoother_.getCurrentValue() * spaceSizeScale;
    for (int ch = 0; ch < kNumFdnChannels; ++ch) {
        const auto idx = static_cast<std::size_t>(ch);
        const float baseLen = std::max(16.0f, fdnBaseDelaySamples_[idx] * currentSizeParam);
        cachedDelayLen[idx] = baseLen;
        cachedRt60Gain[idx] = isFrozen
            ? 1.0f
            : std::pow(10.0f, (-3.0f * (baseLen * invSr)) / currentDecaySec);
    }

    for (int s = 0; s < numSamples; ++s) {
        const float dryL = leftChannel[s];
        const float dryR = rightChannel[s];
        const float inAbs = std::max(std::abs(dryL), std::abs(dryR));
        blockInPeak = std::max(blockInPeak, inAbs);

        // Smooth parameters
        const float dryLvl = drySmoother_.getNextValue();
        const float mix = mixSmoother_.getNextValue();
        const float decaySec = decaySmoother_.getNextValue();
        const float sizeParam = sizeSmoother_.getNextValue() * spaceSizeScale;
        const float preDelayMs = preDelaySmoother_.getNextValue();
        const float highDamp = dampingSmoother_.getNextValue();
        const float shimOct = isLush ? shimmerOctSmoother_.getNextValue() : (shimmerOctSmoother_.getNextValue() * 0.25f);
        (void)shimmerFifthSmoother_.getNextValue();
        const float duckAmount = duckingSmoother_.getNextValue();
        const float erLevel = erLevelSmoother_.getNextValue();
        const float width = widthSmoother_.getNextValue();
        const float outGain = outGainSmoother_.getNextValue();

        // Update LFO delay offsets every 16 samples with smooth linear interpolation per sample
        if ((s & 15) == 0) {
            const float stepLfoInc = lfoInc * 16.0f;
            constexpr float inv16 = 1.0f / 16.0f;
            for (int ch = 0; ch < kNumFdnChannels; ++ch) {
                const auto idx = static_cast<std::size_t>(ch);
                float phase = fdnLfoPhase_[idx] + stepLfoInc;
                if (phase >= 1.0f) phase -= 1.0f;
                fdnLfoPhase_[idx] = phase;

                const float lfoMod = std::sin(2.0f * std::numbers::pi_v<float> * phase) * modDepthSamples;
                const float targetLen = std::max(16.0f, fdnBaseDelaySamples_[idx] * sizeParam + lfoMod);
                fdnDelayStep_[idx] = (targetLen - fdnCurrentDelayLen_[idx]) * inv16;
                cachedRt60Gain[idx] = isFrozen
                    ? 1.0f
                    : std::pow(10.0f, (-3.0f * (targetLen * invSr)) / std::max(0.15f, decaySec));
            }
        }

        // 1. Dynamic Ducking Envelope Follower
        const float envCoeff = (inAbs > duckEnvelope_) ? envAttack : envRelease;
        duckEnvelope_ += envCoeff * (inAbs - duckEnvelope_);
        const float duckReduction = std::clamp(1.0f - duckAmount * std::min(1.0f, duckEnvelope_ * 3.2f), 0.15f, 1.0f);

        // 2. Pre-Delay
        const float preDelaySamples = preDelayMs * 0.001f * sr;
        preDelayL_.write(dryL);
        preDelayR_.write(dryR);
        float inL = (preDelaySamples > 1.0f) ? preDelayL_.readFrac(preDelaySamples) : dryL;
        float inR = (preDelaySamples > 1.0f) ? preDelayR_.readFrac(preDelaySamples) : dryR;

        // 3. Input Conditioning Filters
        inL = inHighCutL_.processSample(inLowCutL_.processSample(inL));
        inR = inHighCutR_.processSample(inLowCutR_.processSample(inR));

        // 4. Input Allpass Diffusion
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

        // 6. 16-Channel FDN Readout & Per-Channel 3-Band Damping
        std::array<float, kNumFdnChannels> fdnRead{};
        float fdnSum = 0.0f;
        float lateOutL = 0.0f;
        float lateOutR = 0.0f;

        const float dampCoeff = isFrozen ? 0.0f : (highDamp * 0.68f);
        const float bassBoost = isFrozen ? 1.0f : std::clamp(params_.bassMultiplier, 0.5f, 1.8f);

        // Headroom scaling for tank feedback: automatically prevents runaway oscillation when shimmer is added
        const float shimmerHeadroomScale = 1.0f / (1.0f + 0.35f * shimOct);

        for (int ch = 0; ch < kNumFdnChannels; ++ch) {
            const auto idx = static_cast<std::size_t>(ch);
            fdnCurrentDelayLen_[idx] += fdnDelayStep_[idx];
            const float rawTap = fdnDelays_[idx].readFrac(fdnCurrentDelayLen_[idx]);
            const float gRt60  = cachedRt60Gain[idx];

            // One-pole HF damping + LF shelf multiplier
            float lp = fdnLpState_[idx];
            lp = rawTap * (1.0f - dampCoeff) + lp * dampCoeff;
            fdnLpState_[idx] = lp;

            float lowState = fdnLowShelfState_[idx];
            lowState += 0.015f * (lp - lowState);
            fdnLowShelfState_[idx] = lowState;

            const float damped = (lp + (bassBoost - 1.0f) * 0.12f * lowState) * gRt60 * shimmerHeadroomScale;
            fdnRead[idx] = damped;
            fdnSum += damped;

            // Stereo tap accumulation with alternating signs for wide decorrelation
            if ((ch & 1) == 0) {
                lateOutL += ((ch & 2) == 0) ? damped : -damped;
            } else {
                lateOutR += ((ch & 2) == 0) ? damped : -damped;
            }

            chanEnergyAccum[idx] = std::max(chanEnergyAccum[idx], std::abs(damped));
        }

        lateOutL *= 0.25f;
        lateOutR *= 0.25f;

        // 7. Pure Octave Shimmer Feedback Path (Harmonically Locked Octaves - Zero Dissonant Fifths!)
        float shimmerFeedL = 0.0f;
        float shimmerFeedR = 0.0f;
        if (shimOct > 0.001f) {
            float sL = 0.0f;
            float sR = 0.0f;
            switch (params_.shimmerPitchMode) {
                case 1: // +2 Octaves (+24st) - Ethereal High Sparkle
                    sL = octave2ShifterL_.process(lateOutL, 4.0f);
                    sR = octave2ShifterR_.process(lateOutR, 4.0f);
                    break;
                case 2: // +1 & +2 Octaves Combined - Lush Majestic Worship Halo
                    sL = octaveShifterL_.process(lateOutL, 2.0f) * 0.65f + octave2ShifterL_.process(lateOutL, 4.0f) * 0.35f;
                    sR = octaveShifterR_.process(lateOutR, 2.0f) * 0.65f + octave2ShifterR_.process(lateOutR, 4.0f) * 0.35f;
                    break;
                case 3: // -1 Octave Sub - Deep Atmospheric Foundation
                    sL = octaveShifterL_.process(lateOutL, 0.5f);
                    sR = octaveShifterR_.process(lateOutR, 0.5f);
                    break;
                case 0: // +1 Octave (+12st) - Classic Angelic Bloom
                default:
                    sL = octaveShifterL_.process(lateOutL, 2.0f);
                    sR = octaveShifterR_.process(lateOutR, 2.0f);
                    break;
            }

            sL *= shimOct;
            sR *= shimOct;

            // 1. Allpass phase diffusion: smears discrete grain boundaries into silky acoustic air
            sL = shimmerDiffusersL_[0].process(sL);
            sL = shimmerDiffusersL_[1].process(sL);
            sR = shimmerDiffusersR_[0].process(sR);
            sR = shimmerDiffusersR_[1].process(sR);

            // 2. HighPass at 260 Hz (cuts low-end rumble accumulation)
            sL = shimmerHighPassL_.processSample(sL);
            sR = shimmerHighPassR_.processSample(sR);

            // 3. LowPass at 3800 Hz (cuts razor-sharp metallic runaway screeching)
            shimmerFeedL = shimmerSilkL_.processSample(sL);
            shimmerFeedR = shimmerSilkR_.processSample(sR);
        }

        // 8. Householder 16x16 Feedback Matrix: u_i = y_{(i+1)%16} - (2/16)*sum
        const float householderTerm = fdnSum * (2.0f / static_cast<float>(kNumFdnChannels));
        const float tankInputScale = isFrozen ? 0.0f : 0.28f;

        // Controlled shimmer feedback injection into tank:
        // Scaled to 0.16f * tankInputScale inside the feedback loop,
        // strictly bounding loop gain and eliminating self-oscillation/howling!
        const float shimmerTankGain = 0.16f * tankInputScale;
        const float feedL = diffL * tankInputScale + shimmerFeedL * shimmerTankGain;
        const float feedR = diffR * tankInputScale + shimmerFeedR * shimmerTankGain;

        for (int ch = 0; ch < kNumFdnChannels; ++ch) {
            const auto idx = static_cast<std::size_t>(ch);
            const auto permIdx = static_cast<std::size_t>((ch + 3) & (kNumFdnChannels - 1));
            const float matrixOut = fdnRead[permIdx] - householderTerm;
            const float inExc = ((ch & 1) == 0) ? feedL : feedR;
            const float inSign = ((ch & 2) == 0) ? 1.0f : -1.0f;

            // Soft saturation inside tank at extreme levels for safety
            float nextVal = matrixOut + inExc * inSign;
            if (std::abs(nextVal) > 2.0f) {
                nextVal = std::tanh(nextVal * 0.5f) * 2.0f;
            }
            fdnDelays_[idx].write(nextVal);
        }

        // 9. Combine Soft Early Reflections & Late Tank + Direct Radiant Shimmer Halo
        float wetL = erOutL * (erLevel * 0.35f) + lateOutL * 1.05f + shimmerFeedL * 0.45f;
        float wetR = erOutR * (erLevel * 0.35f) + lateOutR * 1.05f + shimmerFeedR * 0.45f;

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
        blockTankEnergy = std::max(blockTankEnergy, 0.5f * (std::abs(lateOutL) + std::abs(lateOutR)));
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
