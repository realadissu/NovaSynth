#include "nova_dsp/oscillators/MultiEngineOscillator.h"
#include "nova_dsp/oscillators/SampleBank.h"
#include "OscillatorSharedTables.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace nova::dsp {

namespace {

[[nodiscard]] float advanceSampleChannel(const int16_t* pcm,
                                         int numS,
                                         double& playhead,
                                         double& direction,
                                         bool& finished,
                                         double playbackRate,
                                         double delaySamples,
                                         double lStart,
                                         double lEnd,
                                         double xfadeLen,
                                         int loopMode) noexcept {
    if (finished || pcm == nullptr || numS <= 8) return 0.0f;

    const double loopLen = lEnd - lStart;
    const double step = playbackRate * direction;
    const auto readDelayed = [pcm, numS, delaySamples](double pos) noexcept {
        return readSampleHermite(pcm, numS, std::max(0.0, pos - delaySamples));
    };

    float out = 0.0f;
    switch (loopMode) {
        case 0: { // One-Shot
            if (playhead >= numS - 4 || playhead < 0.0) {
                finished = true;
                return 0.0f;
            }
            out = readDelayed(playhead);
            playhead += step;
            break;
        }
        case 1: { // Forward Loop with Equal-Power Crossfade
            if (playhead < lStart || playhead < lEnd - xfadeLen) {
                out = readDelayed(playhead);
            } else if (playhead < lEnd) {
                const double t = (playhead - (lEnd - xfadeLen)) / xfadeLen;
                const float angle = static_cast<float>(t * (std::numbers::pi * 0.5));
                const double secPlayhead = lStart + (playhead - (lEnd - xfadeLen));
                out = readDelayed(playhead) * std::cos(angle) + readDelayed(secPlayhead) * std::sin(angle);
            } else {
                playhead = (lStart + xfadeLen) + std::fmod(playhead - lEnd, loopLen);
                out = readDelayed(playhead);
            }
            playhead += step;
            break;
        }
        case 2: { // Ping-Pong Loop (<->)
            if (direction > 0.0 && playhead >= lEnd) {
                direction = -1.0;
                playhead = lEnd - (playhead - lEnd);
            } else if (direction <= 0.0 && playhead <= lStart) {
                direction = 1.0;
                playhead = lStart + (lStart - playhead);
            }
            out = readDelayed(playhead);
            playhead += playbackRate * direction;
            break;
        }
        case 3:
        default: { // Reverse Loop (<-) with Equal-Power Crossfade
            if (playhead > lEnd || playhead > lStart + xfadeLen) {
                out = readDelayed(playhead);
            } else if (playhead > lStart) {
                const double t = ((lStart + xfadeLen) - playhead) / xfadeLen;
                const float angle = static_cast<float>(t * (std::numbers::pi * 0.5));
                const double secPlayhead = lEnd - ((lStart + xfadeLen) - playhead);
                out = readDelayed(playhead) * std::cos(angle) + readDelayed(secPlayhead) * std::sin(angle);
            } else {
                playhead = (lEnd - xfadeLen) - std::fmod(lStart - playhead, loopLen);
                out = readDelayed(playhead);
            }
            playhead += step;
            break;
        }
    }
    return out;
}

} // namespace

void MultiEngineOscillator::triggerSamplePlayback(float baseFrequencyHz) noexcept {
    const auto& smp = SampleBank::getSample(params_.modelIndex);
    activeSampleZone_ = nullptr;
    if (smp.zones != nullptr && smp.numZones > 0) {
        const float targetFreq = std::clamp(baseFrequencyHz * basePitchRatio_, 20.0f, 15000.0f);
        float bestDist = 1e9f;
        int bestIdx = 0;
        for (int z = 0; z < smp.numZones; ++z) {
            const float dist = std::abs(std::log2(targetFreq / smp.zones[z].rootFrequencyHz));
            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = z;
            }
        }
        activeSampleZone_ = &smp.zones[bestIdx];
        const int numS = activeSampleZone_->numSamples;
        const double sMorph = static_cast<double>(std::clamp(params_.shapeMorph, 0.0f, 1.0f));
        const double lStart = static_cast<double>(std::clamp(params_.sampleLoopStart, 0.0f, 0.98f)) * (numS - 4);
        const double lEnd = static_cast<double>(std::clamp(params_.sampleLoopEnd, 0.02f, 1.0f)) * (numS - 4);
        const double effectiveEnd = std::max(lStart + 100.0, lEnd);

        if (params_.sampleLoopMode == 3) {
            sampleDirection_ = -1.0;
            samplePlayhead_ = (sMorph > 0.001)
                ? std::clamp(effectiveEnd - sMorph * (effectiveEnd - lStart), lStart, effectiveEnd)
                : effectiveEnd;
        } else {
            sampleDirection_ = 1.0;
            samplePlayhead_ = sMorph * static_cast<double>(numS - 4);
        }
    } else {
        samplePlayhead_ = 0.0;
        sampleDirection_ = 1.0;
    }
    sampleFinished_ = false;
    samplePlayheadR_ = samplePlayhead_;
    sampleDirectionR_ = sampleDirection_;
    sampleFinishedR_ = false;
}

void MultiEngineOscillator::processSamplePlayback(float slotFreq, float& sigL, float& sigR) noexcept {
    if ((sampleFinished_ && sampleFinishedR_) || activeSampleZone_ == nullptr) return;
    const auto& smp = SampleBank::getSample(params_.modelIndex);
    const auto* zone = activeSampleZone_;
    if (zone->pcmL == nullptr || zone->numSamples <= 8) return;

    const double baseRate = (static_cast<double>(slotFreq) / static_cast<double>(zone->rootFrequencyHz))
                          * (smp.sampleRate / sampleRate_);
    const double rateL = baseRate * sampleDetuneRatioL_;
    const double rateR = baseRate * sampleDetuneRatioR_;

    const int numS = zone->numSamples;
    const double lStart = std::clamp(static_cast<double>(params_.sampleLoopStart), 0.0, 0.98) * static_cast<double>(numS - 4);
    double lEnd = std::clamp(static_cast<double>(params_.sampleLoopEnd), 0.02, 1.0) * static_cast<double>(numS - 4);
    if (lEnd < lStart + 100.0) lEnd = std::min(static_cast<double>(numS - 4), lStart + 100.0);
    const double loopLen = lEnd - lStart;

    const double xfadeUser = static_cast<double>(std::clamp(params_.sampleCrossfade, 0.0f, 0.25f));
    const double xfadeLen = std::max(16.0, std::min(loopLen * 0.25, xfadeUser * loopLen));

    const int16_t* pcmL = zone->pcmL;
    const int16_t* pcmR = (zone->pcmR != nullptr) ? zone->pcmR : zone->pcmL;
    const float spreadAmt = std::clamp(params_.detune, 0.0f, 1.0f);
    const float scatterAmt = std::clamp(params_.scatter, 0.0f, 1.0f);
    const double rightDelay = static_cast<double>(spreadAmt * 0.015f * sampleRate_);

    const float rawL = advanceSampleChannel(pcmL, numS, samplePlayhead_, sampleDirection_, sampleFinished_,
                                            rateL, 0.0, lStart, lEnd, xfadeLen, params_.sampleLoopMode);
    const float rawR = advanceSampleChannel(pcmR, numS, samplePlayheadR_, sampleDirectionR_, sampleFinishedR_,
                                            rateR, rightDelay, lStart, lEnd, xfadeLen, params_.sampleLoopMode);

    const float effWidth = std::clamp(std::max(spreadAmt, scatterAmt), 0.0f, 1.0f);
    const float mid = 0.5f * (rawL + rawR);
    const float side = 0.5f * (rawL - rawR);
    const float smpModelScale = (params_.modelIndex == 7) ? 4.2f : 1.0f;
    sigL = (mid + effWidth * side) * smpModelScale;
    sigR = (mid - effWidth * side) * smpModelScale;
}

} // namespace nova::dsp
