#include "nova_dsp/synth/WorshipSynthEngine.h"
#include <cmath>
#include <algorithm>

namespace nova::dsp {

namespace {
[[nodiscard]] inline float midiNoteToHz(int note, float masterTuningHz = 440.0f) noexcept {
    const float refA4 = std::clamp(masterTuningHz, 435.0f, 445.0f);
    return refA4 * std::pow(2.0f, static_cast<float>(note - 69) / 12.0f);
}

[[nodiscard]] inline float applyEnvTimeMod(float baseSec, float modOffset, float minSec, float maxSec) noexcept {
    if (std::abs(modOffset) < 1e-4f) return baseSec;
    if (modOffset < 0.0f) {
        return std::clamp(baseSec * std::pow(2.0f, modOffset * 3.5f), minSec, maxSec);
    }
    return std::clamp(baseSec * std::pow(2.0f, modOffset * 2.0f) + modOffset * 3.0f, minSec, maxSec);
}
} // namespace

WorshipSynthEngine::WorshipSynthEngine() {
    const std::array<MultiEngineOscillator::EngineType, kNumOscSlots> defaultEngines = {
        MultiEngineOscillator::EngineType::WarmAnalog, MultiEngineOscillator::EngineType::EvolvingWavetable,
        MultiEngineOscillator::EngineType::OrganicTexture, MultiEngineOscillator::EngineType::PhysicalPluckModal,
        MultiEngineOscillator::EngineType::WarmAnalog, MultiEngineOscillator::EngineType::EvolvingWavetable,
        MultiEngineOscillator::EngineType::OrganicTexture, MultiEngineOscillator::EngineType::PhysicalPluckModal
    };

    for (int i = 0; i < kNumOscSlots; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        params_.oscSlots[idx].enabled = (i < 2);
        params_.oscSlots[idx].engine = defaultEngines[idx];
        params_.oscSlots[idx].octaveOffset = (i == 1) ? 1 : (i == 4 ? -1 : 0);
        params_.oscSlots[idx].shapeMorph = 0.30f + 0.08f * static_cast<float>(i);
        params_.oscSlots[idx].detune = 0.28f;
        params_.oscSlots[idx].level = (i == 0) ? 0.78f : 0.50f;
    }

    modMatrix_.setSlot(0, ModMatrix::Source::Lfo1,        ModMatrix::Destination::FilterCutoff, 0.35f, true);
    modMatrix_.setSlot(1, ModMatrix::Source::Lfo2,        ModMatrix::Destination::Osc1Shape,    0.25f, true);
    modMatrix_.setSlot(2, ModMatrix::Source::MacroMotion, ModMatrix::Destination::Osc2Shape,    0.40f, true);
    modMatrix_.setSlot(3, ModMatrix::Source::MacroWarmth, ModMatrix::Destination::FilterCutoff, -0.35f, true);
}

void WorshipSynthEngine::prepare(double sampleRate, int maxBlockSize) {
    sampleRate_ = std::max(8000.0, sampleRate);

    for (auto& v : voices_) {
        for (auto& o : v.oscs) o.prepare(sampleRate_);
        v.ampEnv.prepare(sampleRate_); v.filterEnv.prepare(sampleRate_);
        v.modEnv.prepare(sampleRate_); v.auxEnv.prepare(sampleRate_);
        v.filterL.prepare(sampleRate_); v.filterR.prepare(sampleRate_);
        v.filterStage2L.prepare(sampleRate_); v.filterStage2R.prepare(sampleRate_);
        v.filter2L.prepare(sampleRate_); v.filter2R.prepare(sampleRate_);
    }

    lfo1_.prepare(sampleRate_);
    lfo2_.prepare(sampleRate_);
    lfo3_.prepare(sampleRate_);
    lfo4_.prepare(sampleRate_);
    chorus_.prepare(sampleRate_);
    reverb_.prepare(sampleRate_, maxBlockSize);

    const int maxDelaySamples = static_cast<int>(sampleRate_ * 2.0);
    delayBufferL_.assign(static_cast<std::size_t>(maxDelaySamples), 0.0f);
    delayBufferR_.assign(static_cast<std::size_t>(maxDelaySamples), 0.0f);
    delayWritePos_ = 0;
    delayEnergy_ = 0.0f;

    cutoffSmoother_.prepare(sampleRate_, 12.0f);
    cutoff2Smoother_.prepare(sampleRate_, 12.0f);
    masterGainSmoother_.prepare(sampleRate_, 12.0f);
    polyHeadroomSmoother_.prepare(sampleRate_, 25.0f);
    polyHeadroomSmoother_.reset(1.0f);
    droneToneSmoother_.prepare(sampleRate_, 15.0f);
    droneFilterL_.prepare(sampleRate_);
    droneFilterR_.prepare(sampleRate_);

    const std::size_t maxBlock = static_cast<std::size_t>(std::max(maxBlockSize, 2048));
    droneBufferL_.assign(maxBlock, 0.0f);
    droneBufferR_.assign(maxBlock, 0.0f);

    visualizerRingBuffer_.clear();

    reset();
    setParameters(params_);
    snapSmoothersToTarget();
}

void WorshipSynthEngine::reset() noexcept {
    snapSmoothersToTarget();
    for (auto& v : voices_) {
        v.active = false; v.heldByKey = false; v.sustainedByPedal = false;
        v.isDroneVoice = false; v.heldByDrone = false; v.isArpVoice = false; v.heldByArp = false;
        v.midiNote = -1;
        for (auto& o : v.oscs) o.reset();
        v.ampEnv.reset(); v.filterEnv.reset(); v.modEnv.reset(); v.auxEnv.reset();
        v.filterL.reset(); v.filterR.reset(); v.filterStage2L.reset(); v.filterStage2R.reset();
        v.filter2L.reset(); v.filter2R.reset();
    }
    polyHeadroomSmoother_.reset(1.0f);
    voiceCounter_ = 0;
    physicalHeldNotes_.fill(false);
    latchedCount_ = 0;
    latchedNotes_.fill(0);
    sustainPedalDown_ = false;
    droneTriggered_ = false;
    currentDroneActive_ = false;
    currentDroneKey_ = -1; currentDroneVoicing_ = -1; currentDroneOctave_ = 0;
    droneFilterL_.reset(); droneFilterR_.reset();
    currentArpNote_ = -1;
    arpPhaseSamples_ = 0.0;
    lfo1_.reset(0.0f); lfo2_.reset(0.25f); lfo3_.reset(0.50f); lfo4_.reset(0.75f);
    std::fill(delayBufferL_.begin(), delayBufferL_.end(), 0.0f);
    std::fill(delayBufferR_.begin(), delayBufferR_.end(), 0.0f);
    delayWritePos_ = 0;
    delayEnergy_ = 0.0f;
    chorus_.reset();
    reverb_.reset();
}

void WorshipSynthEngine::snapSmoothersToTarget() noexcept {
    cutoffSmoother_.snapToTarget();
    cutoff2Smoother_.snapToTarget();
    masterGainSmoother_.snapToTarget();
    droneToneSmoother_.snapToTarget();
    polyHeadroomSmoother_.reset(1.0f);
}

void WorshipSynthEngine::setParameters(const Parameters& params) noexcept {
    params_ = params;
    params_.activeOscCount = std::clamp(params_.activeOscCount, 1, kNumOscSlots);

    const float lfo1RateMod = modMatrix_.getModulationOffset(ModMatrix::Destination::Lfo1Rate);
    const float lfo2RateMod = modMatrix_.getModulationOffset(ModMatrix::Destination::Lfo2Rate);

    const float rate1 = (params_.lfo1Sync
        ? getLfoRateForDivision(params_.bpm, params_.lfo1SyncDiv)
        : params_.lfo1RateHz) * std::pow(2.0f, lfo1RateMod * 3.0f);
    const float rate2 = (params_.lfo2Sync
        ? getLfoRateForDivision(params_.bpm, params_.lfo2SyncDiv)
        : params_.lfo2RateHz) * std::pow(2.0f, lfo2RateMod * 3.0f);
    const float rate3 = params_.lfo3Sync
        ? getLfoRateForDivision(params_.bpm, params_.lfo3SyncDiv)
        : params_.lfo3RateHz;
    const float rate4 = params_.lfo4Sync
        ? getLfoRateForDivision(params_.bpm, params_.lfo4SyncDiv)
        : params_.lfo4RateHz;

    lfo1_.setParameters(params_.lfo1Shape,
                        rate1 * (0.6f + 1.2f * params_.macroMotion),
                        std::clamp(params_.lfo1Depth + params_.macroMotion * 0.25f, 0.0f, 1.0f));
    lfo2_.setParameters(params_.lfo2Shape, rate2, params_.lfo2Depth);
    lfo3_.setParameters(params_.lfo3Shape, rate3, params_.lfo3Depth);
    lfo4_.setParameters(params_.lfo4Shape, rate4, params_.lfo4Depth);

    const float warmthFactor = std::pow(2.0f, (0.50f - params_.macroWarmth) * 1.35f)
                             * std::pow(2.0f, (params_.macroAir - 0.30f) * 1.35f);
    const float targetCutoff = std::clamp(params_.filterCutoffHz * warmthFactor, 60.0f, 19000.0f);
    cutoffSmoother_.setTargetValue(targetCutoff);

    const float targetCutoff2 = std::clamp(params_.filter2CutoffHz, 40.0f, 19000.0f);
    cutoff2Smoother_.setTargetValue(targetCutoff2);

    StereoBbdChorus::Parameters cp;
    cp.enabled = params_.chorusEnabled;
    cp.mode = static_cast<StereoBbdChorus::Mode>(std::clamp(params_.chorusMode, 0, 4));
    cp.mix = std::clamp(params_.chorusMix + params_.macroMotion * 0.35f, 0.0f, 1.0f);
    cp.rateHz = std::clamp(params_.chorusRateHz * (0.8f + 0.6f * params_.macroMotion), 0.1f, 8.0f);
    cp.depth = std::clamp(params_.chorusDepth + params_.macroMotion * 0.20f, 0.0f, 1.0f);
    cp.warmth = std::clamp(params_.chorusWarmth, 0.0f, 1.0f);
    chorus_.setParameters(cp);

    DualEngineReverb::Parameters revParams = reverb_.getParameters();
    revParams.engineMode       = DualEngineReverb::EngineMode::LushCloud;
    revParams.spaceType        = DualEngineReverb::SpaceType::CathedralCloud;
    revParams.dry              = std::clamp(params_.reverbDry, 0.0f, 1.0f);
    revParams.mix              = std::clamp(params_.reverbMix + params_.macroShimmer * 0.25f, 0.0f, 1.0f);
    revParams.decaySeconds     = std::clamp(params_.reverbDecay + params_.macroShimmer * 2.5f, 0.4f, 26.0f);
    revParams.shimmerPitchMode = std::clamp(params_.reverbShimmerPitch, 0, 3);
    revParams.shimmerOctave    = std::clamp(params_.reverbShimmer + params_.macroShimmer * 0.50f, 0.0f, 1.0f);
    revParams.shimmerFifth     = 0.0f;
    revParams.shimmerLowCutHz  = 540.0f;
    revParams.shimmerHighCutHz = 10500.0f;
    revParams.ducking          = std::clamp(params_.reverbDucking, 0.0f, 1.0f);
    revParams.vocalCarve       = params_.vocalCarve;
    revParams.freeze           = params_.reverbFreeze;
    reverb_.setParameters(revParams);

    masterGainSmoother_.setTargetValue(std::pow(10.0f, std::clamp(params_.masterGainDb, -24.0f, 12.0f) / 20.0f));

    for (auto& v : voices_) {
        if (v.active) {
            v.baseFreqHz = midiNoteToHz(v.midiNote, params_.masterTuningHz);
            v.activeOscCountSnapshot = params_.activeOscCount;
            v.filterTypeSnapshot = params_.filterType;
            v.filterResSnapshot = params_.filterResonance;
            v.filterEnvAmtSnapshot = params_.filterEnvAmount;
            v.filterDriveSnapshot = params_.filterDrive;
            v.filter2TypeSnapshot = params_.filter2Type;
            v.filter2ResSnapshot = params_.filter2Resonance;
            v.filter2EnvAmtSnapshot = params_.filter2EnvAmount;
            v.filter2DriveSnapshot = params_.filter2Drive;
            for (int i = 0; i < kNumOscSlots; ++i) {
                auto slotP = params_.oscSlots[static_cast<std::size_t>(i)];
                slotP.enabled = (i < params_.activeOscCount) && slotP.enabled;
                v.oscs[static_cast<std::size_t>(i)].setParameters(slotP);
            }
            if (!v.isDroneVoice) {
                auto effAmp = params_.ampEnv;
                const float ampAttMod = modMatrix_.getModulationOffset(ModMatrix::Destination::AmpAttack);
                const float ampDecMod = modMatrix_.getModulationOffset(ModMatrix::Destination::AmpDecay);
                const float ampRelMod = modMatrix_.getModulationOffset(ModMatrix::Destination::AmpRelease);
                effAmp.attackSec  = applyEnvTimeMod(effAmp.attackSec,  ampAttMod, 0.001f, 15.0f);
                effAmp.decaySec   = applyEnvTimeMod(effAmp.decaySec,   ampDecMod, 0.005f, 15.0f);
                effAmp.releaseSec = applyEnvTimeMod(effAmp.releaseSec, ampRelMod, 0.005f, 20.0f);
                v.ampEnv.setParameters(effAmp);

                auto effFlt = params_.filterEnv;
                const float fltAttMod = modMatrix_.getModulationOffset(ModMatrix::Destination::FilterAttack);
                const float fltDecMod = modMatrix_.getModulationOffset(ModMatrix::Destination::FilterDecay);
                const float fltRelMod = modMatrix_.getModulationOffset(ModMatrix::Destination::FilterRelease);
                effFlt.attackSec  = applyEnvTimeMod(effFlt.attackSec,  fltAttMod, 0.001f, 15.0f);
                effFlt.decaySec   = applyEnvTimeMod(effFlt.decaySec,   fltDecMod, 0.005f, 15.0f);
                effFlt.releaseSec = applyEnvTimeMod(effFlt.releaseSec, fltRelMod, 0.005f, 20.0f);
                v.filterEnv.setParameters(effFlt);
            } else {
                AdsrEnvelope::Parameters droneAmp;
                droneAmp.attackSec  = std::clamp(params_.droneGlide * 0.75f, 0.25f, 3.5f);
                droneAmp.decaySec   = 0.5f;
                droneAmp.sustainLvl = 1.0f;
                droneAmp.releaseSec = std::clamp(params_.droneGlide, 0.5f, 4.5f);
                v.ampEnv.setParameters(droneAmp);

                AdsrEnvelope::Parameters droneFlt;
                droneFlt.attackSec  = std::clamp(params_.droneGlide * 0.85f, 0.30f, 3.5f);
                droneFlt.decaySec   = 0.5f;
                droneFlt.sustainLvl = 1.0f;
                droneFlt.releaseSec = std::clamp(params_.droneGlide, 0.5f, 4.5f);
                v.filterEnv.setParameters(droneFlt);
            }
            auto effMod = params_.modEnv;
            const float modAttMod = modMatrix_.getModulationOffset(ModMatrix::Destination::ModEnv3Attack);
            const float modDecMod = modMatrix_.getModulationOffset(ModMatrix::Destination::ModEnv3Decay);
            const float modRelMod = modMatrix_.getModulationOffset(ModMatrix::Destination::ModEnv3Release);
            effMod.attackSec  = applyEnvTimeMod(effMod.attackSec,  modAttMod, 0.001f, 15.0f);
            effMod.decaySec   = applyEnvTimeMod(effMod.decaySec,   modDecMod, 0.005f, 15.0f);
            effMod.releaseSec = applyEnvTimeMod(effMod.releaseSec, modRelMod, 0.005f, 20.0f);
            v.modEnv.setParameters(effMod);

            auto effAux = params_.auxEnv;
            const float auxAttMod = modMatrix_.getModulationOffset(ModMatrix::Destination::AuxEnv4Attack);
            const float auxDecMod = modMatrix_.getModulationOffset(ModMatrix::Destination::AuxEnv4Decay);
            const float auxRelMod = modMatrix_.getModulationOffset(ModMatrix::Destination::AuxEnv4Release);
            effAux.attackSec  = applyEnvTimeMod(effAux.attackSec,  auxAttMod, 0.001f, 15.0f);
            effAux.decaySec   = applyEnvTimeMod(effAux.decaySec,   auxDecMod, 0.005f, 15.0f);
            effAux.releaseSec = applyEnvTimeMod(effAux.releaseSec, auxRelMod, 0.005f, 20.0f);
            v.auxEnv.setParameters(effAux);
        }
    }

    if (!params_.arpLatch || !params_.arpEnabled) latchedCount_ = 0;
    if (!params_.droneEnabled) droneTriggered_ = false;

    updateDroneVoices();
}

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
    if (std::abs(revMod) > 0.001f || std::abs(shimMod) > 0.001f) {
        auto revParams = reverb_.getParameters();
        revParams.mix = std::clamp(params_.reverbMix + params_.macroShimmer * 0.25f + revMod, 0.0f, 1.0f);
        revParams.shimmerOctave = std::clamp(params_.reverbShimmer + params_.macroShimmer * 0.50f + shimMod, 0.0f, 1.0f);
        revParams.shimmerLowCutHz = 540.0f;
        revParams.shimmerHighCutHz = 10500.0f;
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

WorshipSynthEngine::Telemetry WorshipSynthEngine::getTelemetry() const noexcept {
    Telemetry t{};
    t.activeVoiceCount = telemetryActiveVoices_.load(std::memory_order_relaxed);
    t.outputPeakL = telemetryPeakL_.load(std::memory_order_relaxed);
    t.outputPeakR = telemetryPeakR_.load(std::memory_order_relaxed);
    t.preClipPeak = telemetryPreClipPeak_.load(std::memory_order_relaxed);
    t.lfo1Value = telemetryLfo1_.load(std::memory_order_relaxed);
    t.lfo2Value = telemetryLfo2_.load(std::memory_order_relaxed);
    t.lfo3Value = telemetryLfo3_.load(std::memory_order_relaxed);
    t.lfo4Value = telemetryLfo4_.load(std::memory_order_relaxed);
    t.filterCutoffMod = telemetryFilterCutoffMod_.load(std::memory_order_relaxed);
    t.filter2CutoffMod = telemetryFilter2CutoffMod_.load(std::memory_order_relaxed);
    t.filterResoMod = telemetryFilterResoMod_.load(std::memory_order_relaxed);
    t.filter2ResoMod = telemetryFilter2ResoMod_.load(std::memory_order_relaxed);
    t.lastEnvVal = telemetryLastEnvVal_.load(std::memory_order_relaxed);
    t.lastPlayedVelocity = lastVelocity_;
    t.modWheelValue = modWheel_;
    t.pitchBendValue = pitchBendSemitones_;
    return t;
}

} // namespace nova::dsp
