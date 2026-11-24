#include "nova_dsp/synth/WorshipSynthEngine.h"
#include <algorithm>
#include <cmath>

namespace nova::dsp {

void WorshipSynthEngine::processMasterEffectsChain(float* outLeft, float* outRight, int numSamples) noexcept {
    if (params_.delayEnabled && params_.delayMix > 0.001f && !delayBufferL_.empty()) {
        const int bufSize = static_cast<int>(delayBufferL_.size());
        const float effDelaySec = params_.delaySync
            ? getDelayTimeForDivision(params_.bpm, params_.delaySyncDiv)
            : params_.delayTimeSec;
        const float swingFactor = 1.0f + params_.delaySwing * 0.3333f;
        const int dSamplesL = std::clamp(static_cast<int>(effDelaySec * sampleRate_), 1, bufSize - 1);
        const int dSamplesR = std::clamp(static_cast<int>(effDelaySec * swingFactor * sampleRate_), 1, bufSize - 1);
        const float delayMixMod = modMatrix_.getModulationOffset(ModMatrix::Destination::DelayMix);
        const float delayFbMod = modMatrix_.getModulationOffset(ModMatrix::Destination::DelayFeedback);
        const float fdbk = std::clamp(params_.delayFeedback + delayFbMod, 0.0f, 0.88f);
        const float wet = std::clamp(params_.delayMix + delayMixMod, 0.0f, 1.0f);

        float inPeakDelay = 0.0f;
        for (int s = 0; s < numSamples; ++s) {
            inPeakDelay = std::max(inPeakDelay, std::max(std::abs(outLeft[s]), std::abs(outRight[s])));
        }

        if (inPeakDelay < 1e-6f && delayEnergy_ < 1e-5f) {
            delayEnergy_ = 0.0f;
        } else {
            float maxEcho = 0.0f;
            for (int s = 0; s < numSamples; ++s) {
                const int readIdxL = (delayWritePos_ - dSamplesL + bufSize) % bufSize;
                const int readIdxR = (delayWritePos_ - dSamplesR + bufSize) % bufSize;
                const float dL = delayBufferL_[static_cast<std::size_t>(readIdxL)];
                const float dR = delayBufferR_[static_cast<std::size_t>(readIdxR)];
                maxEcho = std::max(maxEcho, std::max(std::abs(dL), std::abs(dR)));

                delayBufferL_[static_cast<std::size_t>(delayWritePos_)] = outLeft[s] + dR * fdbk * 0.95f;
                delayBufferR_[static_cast<std::size_t>(delayWritePos_)] = outRight[s] + dL * fdbk * 0.95f;

                outLeft[s]  += dL * wet;
                outRight[s] += dR * wet;
                delayWritePos_ = (delayWritePos_ + 1) % bufSize;
            }
            delayEnergy_ = maxEcho;
        }
    }

    for (int s = 0; s < numSamples; ++s) {
        if (static_cast<std::size_t>(s) < droneBufferL_.size()) {
            outLeft[s]  += droneBufferL_[static_cast<std::size_t>(s)];
            outRight[s] += droneBufferR_[static_cast<std::size_t>(s)];
        }
    }

    const float revMod = modMatrix_.getModulationOffset(ModMatrix::Destination::ReverbMix);
    const float shimMod = modMatrix_.getModulationOffset(ModMatrix::Destination::ShimmerAmount);
    const float vcMod = modMatrix_.getModulationOffset(ModMatrix::Destination::VocalCarve);
    const float airMod = modMatrix_.getModulationOffset(ModMatrix::Destination::AirAmount);
    const float panMod = modMatrix_.getModulationOffset(ModMatrix::Destination::PanSpread);
    const float panDepth = std::clamp(modMatrix_.getTotalDepth(ModMatrix::Destination::PanSpread), 0.0f, 1.0f);

    const auto curRev = reverb_.getParameters();
    auto revParams = curRev;
    revParams.mix = std::clamp(params_.reverbMix + params_.macroShimmer * 0.25f + revMod, 0.0f, 1.0f);
    revParams.shimmerOctave = std::clamp(params_.reverbShimmer + params_.macroShimmer * 0.50f + shimMod, 0.0f, 1.0f);
    revParams.shimmerLowCutHz = 540.0f;
    const float effAir = std::clamp(params_.macroAir + airMod, 0.0f, 1.0f);
    WorshipSynthAirStage::applyReverbAirCoupling(effAir, params_.macroWarmth, revParams);
    const float cCut = cutoffSmoother_.getCurrentValue();
    const float cOpen = (cCut > 800.0f) ? std::clamp((cCut - 800.0f) / 3200.0f, 0.0f, 1.0f) : 0.0f;
    revParams.vocalCarve = std::clamp(
        params_.vocalCarve + vcMod + cOpen * (0.35f + 0.65f * std::max(modWheel_, 1.0f - params_.macroWarmth)) * 0.40f,
        0.0f, 1.0f);
    const float baseRevWidth = (panDepth > 0.25f) ? std::clamp(1.0f - 0.72f * panDepth, 0.38f, 1.0f) : 1.0f;
    const float panWidthScale = (panDepth > 0.25f) ? 0.85f : 0.50f;
    revParams.stereoWidth = std::clamp(baseRevWidth + panMod * panWidthScale, 0.25f, 1.15f);

    const bool revDirty = std::abs(revMod) > 0.001f || std::abs(shimMod) > 0.001f
        || std::abs(vcMod) > 0.001f || std::abs(airMod) > 0.001f
        || std::abs(revParams.mix - curRev.mix) > 1e-4f
        || std::abs(revParams.shimmerOctave - curRev.shimmerOctave) > 1e-4f
        || std::abs(revParams.highCutHz - curRev.highCutHz) > 1e-4f
        || std::abs(revParams.highDamping - curRev.highDamping) > 1e-4f
        || std::abs(revParams.shimmerHighCutHz - curRev.shimmerHighCutHz) > 1e-4f
        || std::abs(revParams.vocalCarve - curRev.vocalCarve) > 1e-4f
        || std::abs(revParams.stereoWidth - curRev.stereoWidth) > 1e-4f;
    if (revDirty) {
        reverb_.setParameters(revParams);
    }

    const auto revTel = reverb_.getTelemetry();
    const bool revActive = (params_.reverbMix > 0.001f || std::abs(revMod) > 0.001f || params_.reverbFreeze || revTel.tankEnergy > 1e-5f || revTel.outputPeak > 1e-5f);
    if (revActive) {
        reverb_.processBlock(outLeft, outRight, numSamples);
    }

    float preClipPeak = 0.0f;
    for (int s = 0; s < numSamples; ++s) {
        preClipPeak = std::max(preClipPeak, std::max(std::abs(outLeft[s]), std::abs(outRight[s])));
    }

    float peakL = 0.0f, peakR = 0.0f;
    if (preClipPeak > 1e-6f) {
        for (int s = 0; s < numSamples; ++s) {
            const auto softClip = [](float x) noexcept -> float {
                constexpr float threshold = 0.70f, limit = 0.98f;
                const float ax = std::abs(x);
                if (ax <= threshold) return x;
                const float sign = (x > 0.0f) ? 1.0f : -1.0f;
                const float excess = ax - threshold;
                const float ex = excess / (limit - threshold);
                const float ex2 = ex * ex;
                const float tanhVal = (ex <= 2.5f) ? (ex * (27.0f + ex2) / (27.0f + 9.0f * ex2)) : 1.0f;
                return sign * (threshold + (limit - threshold) * tanhVal);
            };

            outLeft[s]  = softClip(outLeft[s]);
            outRight[s] = softClip(outRight[s]);
            peakL = std::max(peakL, std::abs(outLeft[s]));
            peakR = std::max(peakR, std::abs(outRight[s]));
            if ((s & 3) == 0) {
                visualizerRingBuffer_.pushOverwrite(0.5f * (outLeft[s] + outRight[s]));
            }
        }
    }

    int activeCount = 0;
    for (const auto& v : voices_) {
        if (v.active) ++activeCount;
    }

    telemetryActiveVoices_.store(activeCount, std::memory_order_relaxed);
    telemetryPeakL_.store(peakL, std::memory_order_relaxed);
    telemetryPeakR_.store(peakR, std::memory_order_relaxed);
    telemetryPreClipPeak_.store(preClipPeak, std::memory_order_relaxed);
}

} // namespace nova::dsp
