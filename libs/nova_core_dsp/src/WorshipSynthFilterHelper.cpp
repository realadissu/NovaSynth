#include "nova_dsp/synth/WorshipSynthFilterHelper.h"
#include <algorithm>
#include <cmath>

namespace nova::dsp::synth_filter {

namespace {
[[nodiscard]] inline float tubeSat(float x) noexcept {
    const float x2 = x * x;
    return (x * (27.0f + x2) / (27.0f + 9.0f * x2));
}
} // namespace

float computeKeyTrackFactor(float baseFreqHz, float keyTrack) noexcept {
    const float kt = std::clamp(keyTrack, 0.0f, 1.0f);
    if (kt <= 1e-4f) return 1.0f;
    return std::pow(std::max(16.0f, baseFreqHz) / 261.63f, kt);
}

void processFilter1(const Filter1Context& ctx,
                    StateVariableFilter& fL,
                    StateVariableFilter& fR,
                    StateVariableFilter& s2L,
                    StateVariableFilter& s2R,
                    float& resComp,
                    float& inL,
                    float& inR) noexcept {
    if (!ctx.enabled) return;

    const bool isFourPoleLadder = (ctx.filterType == 0 || ctx.filterType == 2);
    const float effReso = std::clamp(ctx.resonance + ctx.voiceResoMod, 0.35f, 6.0f);
    const bool antiHowl = (ctx.resonance <= 0.25f);

    if (ctx.updateCoeffs) {
        const float ktFactor = computeKeyTrackFactor(ctx.baseFreqHz, ctx.keyTrack);
        const float envOctaves = ctx.fltEnvVal * ctx.envAmount * 2.1f;
        const float voiceCutoff = std::clamp(
            ctx.baseCutoff * ktFactor * ctx.cutoffDriftFactor * std::exp2(envOctaves + ctx.voiceCutMod),
            40.0f, static_cast<float>(ctx.sampleRate * 0.46));

        auto svfType = StateVariableFilter::Type::LowPass;
        if (ctx.filterType == 1) svfType = StateVariableFilter::Type::BandPass;
        else if (ctx.filterType == 2) svfType = StateVariableFilter::Type::HighPass;

        if (isFourPoleLadder) {
            constexpr float kButterQ1 = 0.5411961f;
            constexpr float kButterQ2 = 1.3065630f;
            const float resoExcess = std::max(0.0f, effReso - 0.35f);
            const float r = resoExcess / (1.0f + 0.14f * resoExcess);
            const float stageQ1 = std::clamp(kButterQ1 + 0.45f * r, kButterQ1, 2.20f);
            const float stageQ2 = std::clamp(kButterQ2 + 0.72f * r, kButterQ2, 3.80f);
            fL.setParameters(svfType, voiceCutoff, stageQ1);
            fR.setParameters(svfType, voiceCutoff, stageQ1);
            s2L.setParameters(svfType, voiceCutoff, stageQ2);
            s2R.setParameters(svfType, voiceCutoff, stageQ2);
        } else {
            const float maxQ = antiHowl ? 0.60f : 6.0f;
            const float svfQ = std::clamp(effReso, 0.35f, maxQ);
            fL.setParameters(svfType, voiceCutoff, svfQ);
            fR.setParameters(svfType, voiceCutoff, svfQ);
        }
        resComp = 1.0f / std::sqrt(1.0f + 0.35f * std::max(0.0f, effReso - 0.707f));
    }

    if (ctx.drive > 0.01f) {
        inL = tubeSat(inL * ctx.driveGain) * ctx.driveComp;
        inR = tubeSat(inR * ctx.driveGain) * ctx.driveComp;
    }

    inL = fL.processSample(inL);
    inR = fR.processSample(inR);
    if (isFourPoleLadder) {
        inL = s2L.processSample(inL);
        inR = s2R.processSample(inR);
    }
    inL *= resComp;
    inR *= resComp;
}

void processFilter2(const Filter2Context& ctx,
                    StateVariableFilter& f2L,
                    StateVariableFilter& f2R,
                    float& resComp,
                    float& inL,
                    float& inR) noexcept {
    if (!ctx.enabled) return;

    const float effReso2 = std::clamp(ctx.resonance + ctx.voiceResoMod, 0.35f, 6.0f);
    const bool antiHowl2 = (ctx.resonance <= 0.25f);

    if (ctx.updateCoeffs) {
        const float ktFactor2 = (ctx.filter2Type == 3) ? 1.0f : computeKeyTrackFactor(ctx.baseFreqHz, ctx.keyTrack);
        const float drift2 = (ctx.filter2Type == 3) ? 1.0f : ctx.cutoffDriftFactor;
        const float envOctaves2 = ctx.fltEnvVal * ctx.envAmount * 2.1f;
        const float voiceCutoff2 = std::clamp(
            ctx.baseCutoff2 * ktFactor2 * drift2 * std::exp2(envOctaves2 + ctx.voiceCutMod),
            40.0f, static_cast<float>(ctx.sampleRate * 0.46));

        auto svf2Type = StateVariableFilter::Type::LowPass;
        float bellGainDb = 0.0f;
        float f2Q = effReso2;
        float f2Cutoff = voiceCutoff2;

        if (ctx.filter2Type == 1) {
            svf2Type = StateVariableFilter::Type::BandPass;
        } else if (ctx.filter2Type == 2) {
            svf2Type = StateVariableFilter::Type::HighPass;
        } else if (ctx.filter2Type == 3) {
            svf2Type = StateVariableFilter::Type::Bell;
            const float cutoffOpen = (ctx.baseCutoff > 800.0f)
                ? std::clamp((ctx.baseCutoff - 800.0f) / 3200.0f, 0.0f, 1.0f)
                : 0.0f;
            const float swellBoost = cutoffOpen * (0.35f + 0.65f * std::max(ctx.modWheel, 1.0f - ctx.macroWarmth));
            const float effCarve = std::clamp(ctx.vocalCarve + ctx.vocalCarveMod + swellBoost * 0.45f, 0.0f, 1.0f);
            bellGainDb = -14.0f * effCarve;
            f2Q = 0.88f;
            f2Cutoff = (ctx.filter2CutoffHzParam >= 450.0f && ctx.filter2CutoffHzParam <= 1200.0f)
                ? voiceCutoff2
                : 720.0f;
        }
        if (ctx.filter2Type != 3 && antiHowl2) {
            f2Q = std::clamp(f2Q, 0.35f, 0.60f);
        }
        f2L.setParameters(svf2Type, f2Cutoff, f2Q, bellGainDb);
        f2R.setParameters(svf2Type, f2Cutoff, f2Q, bellGainDb);
        resComp = 1.0f / std::sqrt(1.0f + 0.30f * std::max(0.0f, effReso2 - 0.707f));
    }

    if (ctx.drive > 0.01f) {
        inL = tubeSat(inL * ctx.driveGain) * ctx.driveComp;
        inR = tubeSat(inR * ctx.driveGain) * ctx.driveComp;
    }

    inL = f2L.processSample(inL) * resComp;
    inR = f2R.processSample(inR) * resComp;
}

} // namespace nova::dsp::synth_filter
