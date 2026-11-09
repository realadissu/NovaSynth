#include "nova_dsp/reverb/DualEngineReverb.h"
#include "DualEngineReverbHelpers.h"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace nova::dsp {

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

} // namespace nova::dsp

