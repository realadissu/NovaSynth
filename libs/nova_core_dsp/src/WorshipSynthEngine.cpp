#include "nova_dsp/synth/WorshipSynthEngine.h"
#include <cmath>
#include <algorithm>

namespace nova::dsp {

namespace {
[[nodiscard]] inline float midiNoteToHz(int note) noexcept {
    return 440.0f * std::pow(2.0f, static_cast<float>(note - 69) / 12.0f);
}
} // namespace

WorshipSynthEngine::WorshipSynthEngine() {
    // Default 6 JORNA-style sources available (3 active by default, expandable up to 8)
    const std::array<MultiEngineOscillator::EngineType, kNumOscSlots> defaultEngines = {
        MultiEngineOscillator::EngineType::WarmAnalog,
        MultiEngineOscillator::EngineType::EvolvingWavetable,
        MultiEngineOscillator::EngineType::OrganicTexture,
        MultiEngineOscillator::EngineType::PhysicalPluckModal,
        MultiEngineOscillator::EngineType::WarmAnalog,
        MultiEngineOscillator::EngineType::EvolvingWavetable,
        MultiEngineOscillator::EngineType::OrganicTexture,
        MultiEngineOscillator::EngineType::PhysicalPluckModal
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
        for (auto& o : v.oscs) {
            o.prepare(sampleRate_);
        }
        v.ampEnv.prepare(sampleRate_);
        v.filterEnv.prepare(sampleRate_);
        v.modEnv.prepare(sampleRate_);
        v.auxEnv.prepare(sampleRate_);
        v.filterL.prepare(sampleRate_);
        v.filterR.prepare(sampleRate_);
        v.filterStage2L.prepare(sampleRate_);
        v.filterStage2R.prepare(sampleRate_);
        v.filter2L.prepare(sampleRate_);
        v.filter2R.prepare(sampleRate_);
    }

    lfo1_.prepare(sampleRate_);
    lfo2_.prepare(sampleRate_);
    lfo3_.prepare(sampleRate_);
    lfo4_.prepare(sampleRate_);
    vocalCarveL_.prepare(sampleRate_);
    vocalCarveR_.prepare(sampleRate_);
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
}

void WorshipSynthEngine::reset() noexcept {
    for (auto& v : voices_) {
        v.active = false;
        v.heldByKey = false;
        v.sustainedByPedal = false;
        v.isDroneVoice = false;
        v.heldByDrone = false;
        v.midiNote = -1;
        for (auto& o : v.oscs) o.reset();
        v.ampEnv.reset();
        v.filterEnv.reset();
        v.modEnv.reset();
        v.auxEnv.reset();
        v.filterL.reset();
        v.filterR.reset();
        v.filterStage2L.reset();
        v.filterStage2R.reset();
        v.filter2L.reset();
        v.filter2R.reset();
    }
    polyHeadroomSmoother_.reset(1.0f);
    physicalHeldNotes_.fill(false);
    latchedCount_ = 0;
    latchedNotes_.fill(0);
    sustainPedalDown_ = false;
    droneTriggered_ = false;
    currentDroneActive_ = false;
    currentDroneKey_ = -1;
    currentDroneVoicing_ = -1;
    currentDroneOctave_ = 0;
    droneFilterL_.reset();
    droneFilterR_.reset();
    currentArpNote_ = -1;
    arpPhaseSamples_ = 0.0;
    lfo1_.reset(0.0f);
    lfo2_.reset(0.25f);
    lfo3_.reset(0.50f);
    lfo4_.reset(0.75f);
    std::fill(delayBufferL_.begin(), delayBufferL_.end(), 0.0f);
    std::fill(delayBufferR_.begin(), delayBufferR_.end(), 0.0f);
    delayWritePos_ = 0;
    delayEnergy_ = 0.0f;
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

    const float rate1 = params_.lfo1Sync
        ? getLfoRateForDivision(params_.bpm, params_.lfo1SyncDiv)
        : params_.lfo1RateHz;
    const float rate2 = params_.lfo2Sync
        ? getLfoRateForDivision(params_.bpm, params_.lfo2SyncDiv)
        : params_.lfo2RateHz;
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

    // Warmth macro darkens/warms the low-pass filter when turned up above 0.50;
    // Air macro gently opens top-end sheen; Shimmer macro belongs strictly to the reverb shimmer bloom!
    const float warmthFactor = std::pow(2.0f, (0.50f - params_.macroWarmth) * 1.35f)
                             * std::pow(2.0f, (params_.macroAir - 0.30f) * 0.70f);
    const float targetCutoff = std::clamp(params_.filterCutoffHz * warmthFactor, 60.0f, 19000.0f);
    cutoffSmoother_.setTargetValue(targetCutoff);

    const float targetCutoff2 = std::clamp(params_.filter2CutoffHz, 40.0f, 19000.0f);
    cutoff2Smoother_.setTargetValue(targetCutoff2);

    const float carveDb = params_.filter2Enabled ? (-18.0f * std::clamp(params_.vocalCarve, 0.0f, 1.0f)) : 0.0f;
    vocalCarveL_.setParameters(StateVariableFilter::Type::Bell, 2200.0f, 1.2f, carveDb);
    vocalCarveR_.setParameters(StateVariableFilter::Type::Bell, 2200.0f, 1.2f, carveDb);

    DualEngineReverb::Parameters revParams = reverb_.getParameters();
    revParams.engineMode       = DualEngineReverb::EngineMode::LushCloud;
    revParams.spaceType        = DualEngineReverb::SpaceType::CathedralCloud;
    revParams.dry              = std::clamp(params_.reverbDry, 0.0f, 1.0f);
    revParams.mix              = std::clamp(params_.reverbMix + params_.macroShimmer * 0.25f, 0.0f, 1.0f);
    revParams.decaySeconds     = std::clamp(params_.reverbDecay + params_.macroShimmer * 2.5f, 0.4f, 26.0f);
    revParams.shimmerPitchMode = std::clamp(params_.reverbShimmerPitch, 0, 3);
    revParams.shimmerOctave    = std::clamp(params_.reverbShimmer + params_.macroShimmer * 0.50f, 0.0f, 1.0f);
    revParams.shimmerFifth     = 0.0f;
    revParams.ducking          = std::clamp(params_.reverbDucking, 0.0f, 1.0f);
    revParams.vocalCarve       = params_.vocalCarve;
    revParams.freeze           = params_.reverbFreeze;
    reverb_.setParameters(revParams);

    masterGainSmoother_.setTargetValue(std::pow(10.0f, std::clamp(params_.masterGainDb, -24.0f, 12.0f) / 20.0f));

    for (auto& v : voices_) {
        if (v.active && (v.heldByKey || v.heldByDrone)) {
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
                v.ampEnv.setParameters(params_.ampEnv);
                v.filterEnv.setParameters(params_.filterEnv);
            } else {
                AdsrEnvelope::Parameters droneAmp;
                droneAmp.attackSec  = std::clamp(params_.droneGlide * 0.75f, 0.25f, 3.5f);
                droneAmp.decaySec   = 0.5f;
                droneAmp.sustainLvl = 1.0f; // 100% steady sustain - NEVER drops
                droneAmp.releaseSec = std::clamp(params_.droneGlide, 0.5f, 4.5f);
                v.ampEnv.setParameters(droneAmp);

                AdsrEnvelope::Parameters droneFlt;
                droneFlt.attackSec  = std::clamp(params_.droneGlide * 0.85f, 0.30f, 3.5f);
                droneFlt.decaySec   = 0.5f;
                droneFlt.sustainLvl = 1.0f;
                droneFlt.releaseSec = std::clamp(params_.droneGlide, 0.5f, 4.5f);
                v.filterEnv.setParameters(droneFlt);
            }
            v.modEnv.setParameters(params_.modEnv);
            v.auxEnv.setParameters(params_.auxEnv);
        }
    }

    if (!params_.arpLatch || !params_.arpEnabled) {
        latchedCount_ = 0;
    }

    if (!params_.droneEnabled) {
        droneTriggered_ = false;
    }

    updateDroneVoices();
}

void WorshipSynthEngine::noteOn(int midiNote, float velocity) noexcept {
    if (midiNote < 0 || midiNote > 127) return;

    bool anyPhysicalHeldBefore = false;
    for (bool b : physicalHeldNotes_) {
        if (b) { anyPhysicalHeldBefore = true; break; }
    }

    if (params_.arpEnabled && params_.arpLatch) {
        // If no physical keys were held before this note, start a new latched chord
        if (!anyPhysicalHeldBefore) {
            latchedCount_ = 0;
        }
        if (latchedCount_ < static_cast<int>(latchedNotes_.size())) {
            bool alreadyIn = false;
            for (int i = 0; i < latchedCount_; ++i) {
                if (latchedNotes_[static_cast<std::size_t>(i)] == midiNote) {
                    alreadyIn = true;
                    break;
                }
            }
            if (!alreadyIn) {
                latchedNotes_[static_cast<std::size_t>(latchedCount_++)] = midiNote;
                std::sort(latchedNotes_.begin(), latchedNotes_.begin() + latchedCount_);
            }
        }
    }

    if (params_.arpEnabled && !anyPhysicalHeldBefore) {
        // First key pressed: start rhythm pattern immediately on step 0
        arpStepIndex_ = 0;
        arpPhaseSamples_ = 0.0;
        arpTriggerPending_ = true;
    }

    if (params_.droneEnabled && !droneTriggered_) {
        droneTriggered_ = true;
        updateDroneVoices();
    }

    physicalHeldNotes_[static_cast<std::size_t>(midiNote)] = true;
    lastVelocity_ = velocity;

    if (!params_.arpEnabled) {
        startVoice(midiNote, velocity, false);
    }
}

void WorshipSynthEngine::noteOff(int midiNote) noexcept {
    if (midiNote < 0 || midiNote > 127) return;
    physicalHeldNotes_[static_cast<std::size_t>(midiNote)] = false;

    bool anyPhysicalHeldAfter = false;
    for (bool b : physicalHeldNotes_) {
        if (b) { anyPhysicalHeldAfter = true; break; }
    }
    if (!anyPhysicalHeldAfter && !params_.arpLatch) {
        arpStepIndex_ = 0;
        arpPhaseSamples_ = 0.0;
        currentArpNote_ = -1;
    }

    for (auto& v : voices_) {
        if (v.active && !v.isDroneVoice && v.midiNote == midiNote) {
            v.heldByKey = false;
            if (sustainPedalDown_) {
                v.sustainedByPedal = true;
            } else {
                v.ampEnv.noteOff();
                v.filterEnv.noteOff();
                v.modEnv.noteOff();
                v.auxEnv.noteOff();
            }
        }
    }
}

void WorshipSynthEngine::setSustainPedal(bool down) noexcept {
    sustainPedalDown_ = down;
    if (!down) {
        for (auto& v : voices_) {
            if (v.active && !v.isDroneVoice && v.sustainedByPedal && !v.heldByKey) {
                v.sustainedByPedal = false;
                v.ampEnv.noteOff();
                v.filterEnv.noteOff();
                v.modEnv.noteOff();
                v.auxEnv.noteOff();
            }
        }
    }
}

void WorshipSynthEngine::allNotesOff() noexcept {
    physicalHeldNotes_.fill(false);
    latchedCount_ = 0;
    sustainPedalDown_ = false;
    for (auto& v : voices_) {
        if (v.active && !v.isDroneVoice) {
            v.heldByKey = false;
            v.sustainedByPedal = false;
            v.ampEnv.noteOff();
            v.filterEnv.noteOff();
            v.modEnv.noteOff();
            v.auxEnv.noteOff();
        }
    }
}

void WorshipSynthEngine::startVoice(int midiNote, float velocity, bool isDrone, bool isArp) noexcept {
    Voice* chosen = nullptr;

    // 1. First priority: look for a completely free/inactive voice so release tails of previous notes
    // continue to decay naturally and smoothly without being abruptly cut or hijacked.
    for (auto& v : voices_) {
        if (!v.active) {
            chosen = &v;
            break;
        }
    }

    // 2. If all voices are active, steal a released voice (not held by key or pedal) with the quietest envelope level
    if (chosen == nullptr) {
        float lowestLevel = 1e9f;
        for (auto& v : voices_) {
            if (!v.isDroneVoice && !v.heldByKey && !v.sustainedByPedal) {
                const float lvl = v.ampEnv.getCurrentLevel();
                if (lvl < lowestLevel) {
                    lowestLevel = lvl;
                    chosen = &v;
                }
            }
        }
    }

    // 3. If no quiet released voice found, steal the oldest released voice
    if (chosen == nullptr) {
        uint64_t oldestAge = UINT64_MAX;
        for (auto& v : voices_) {
            if (!v.isDroneVoice && !v.heldByKey && v.ageCounter < oldestAge) {
                oldestAge = v.ageCounter;
                chosen = &v;
            }
        }
    }

    // 4. Fallback if all 32 voices are actively held: steal the oldest non-drone voice
    if (chosen == nullptr) {
        uint64_t oldestAge = UINT64_MAX;
        for (auto& v : voices_) {
            if (!v.isDroneVoice && v.ageCounter < oldestAge) {
                oldestAge = v.ageCounter;
                chosen = &v;
            }
        }
    }

    if (chosen == nullptr) {
        chosen = &voices_[0];
    }

    // Reset voice filter states only if voice was idle, to prevent clicks when retriggering active voices
    if (!chosen->active) {
        chosen->filterL.reset();
        chosen->filterR.reset();
        chosen->filterStage2L.reset();
        chosen->filterStage2R.reset();
        chosen->filter2L.reset();
        chosen->filter2R.reset();
    }
    chosen->active = true;
    chosen->heldByKey = !isDrone && !isArp;
    chosen->sustainedByPedal = false;
    chosen->isDroneVoice = isDrone;
    chosen->heldByDrone = isDrone;
    chosen->isArpVoice = isArp;
    chosen->midiNote = midiNote;
    chosen->baseFreqHz = midiNoteToHz(midiNote);
    chosen->velocity = std::clamp(velocity, 0.05f, 1.0f);
    chosen->ageCounter = ++voiceCounter_;

    chosen->activeOscCountSnapshot = params_.activeOscCount;
    chosen->oscNormSnapshot = (chosen->activeOscCountSnapshot > 1)
        ? (1.0f / std::sqrt(static_cast<float>(chosen->activeOscCountSnapshot) * 0.85f))
        : 1.0f;
    chosen->flt1ResComp = 1.0f / std::sqrt(1.0f + 0.35f * std::max(0.0f, chosen->filterResSnapshot - 0.707f));
    chosen->flt2ResComp = 1.0f / std::sqrt(1.0f + 0.30f * std::max(0.0f, chosen->filter2ResSnapshot - 0.707f));
    chosen->filterTypeSnapshot     = params_.filterType;
    chosen->filterCutoffSnapshot   = cutoffSmoother_.getTargetValue();
    chosen->filterResSnapshot      = params_.filterResonance;
    chosen->filterEnvAmtSnapshot   = params_.filterEnvAmount;
    chosen->filterDriveSnapshot    = params_.filterDrive;

    chosen->filter2TypeSnapshot     = params_.filter2Type;
    chosen->filter2CutoffSnapshot   = cutoff2Smoother_.getTargetValue();
    chosen->filter2ResSnapshot      = params_.filter2Resonance;
    chosen->filter2EnvAmtSnapshot   = params_.filter2EnvAmount;
    chosen->filter2DriveSnapshot    = params_.filter2Drive;

    for (int i = 0; i < kNumOscSlots; ++i) {
        auto slotP = params_.oscSlots[static_cast<std::size_t>(i)];
        slotP.enabled = (i < params_.activeOscCount) && slotP.enabled;
        chosen->oscs[static_cast<std::size_t>(i)].setParameters(slotP);
        chosen->oscs[static_cast<std::size_t>(i)].triggerNote(
            chosen->baseFreqHz, chosen->velocity, static_cast<uint32_t>(chosen->ageCounter + i * 31u));
    }

    if (isDrone) {
        chosen->ampEnv.reset();
        chosen->filterEnv.reset();

        AdsrEnvelope::Parameters droneAmp;
        droneAmp.attackSec  = std::clamp(params_.droneGlide * 0.75f, 0.25f, 3.5f);
        droneAmp.decaySec   = 0.5f;
        droneAmp.sustainLvl = 1.0f; // 100% steady sustain - NEVER drops after initial attack!
        droneAmp.releaseSec = std::clamp(params_.droneGlide, 0.5f, 4.5f);
        chosen->ampEnv.setParameters(droneAmp);

        AdsrEnvelope::Parameters droneFlt;
        droneFlt.attackSec  = std::clamp(params_.droneGlide * 0.85f, 0.30f, 3.5f);
        droneFlt.decaySec   = 0.5f;
        droneFlt.sustainLvl = 1.0f; // Steady filter cutoff without punchy decay
        droneFlt.releaseSec = std::clamp(params_.droneGlide, 0.5f, 4.5f);
        chosen->filterEnv.setParameters(droneFlt);
    } else if (isArp) {
        auto arpAmp = params_.ampEnv;
        arpAmp.attackSec = std::min(params_.ampEnv.attackSec, 0.025f);
        chosen->ampEnv.setParameters(arpAmp);
        chosen->filterEnv.setParameters(params_.filterEnv);
    } else {
        chosen->ampEnv.setParameters(params_.ampEnv);
        chosen->filterEnv.setParameters(params_.filterEnv);
    }
    chosen->modEnv.setParameters(params_.modEnv);
    chosen->auxEnv.setParameters(params_.auxEnv);
    chosen->ampEnv.noteOn(chosen->velocity);
    chosen->filterEnv.noteOn(chosen->velocity);
    chosen->modEnv.noteOn(chosen->velocity);
    chosen->auxEnv.noteOn(chosen->velocity);
}

void WorshipSynthEngine::triggerDrone(bool triggered) noexcept {
    if (droneTriggered_ != triggered) {
        droneTriggered_ = triggered;
        updateDroneVoices();
    }
}

void WorshipSynthEngine::updateDroneVoices() noexcept {
    const bool shouldRunDrone = params_.droneEnabled && droneTriggered_;
    const int key = std::clamp(params_.droneRootKey, 0, 11);
    const int voicing = std::clamp(params_.droneVoicing, 0, 3);
    const int octave = std::clamp(params_.droneOctave, -2, 2);

    if (shouldRunDrone == currentDroneActive_ &&
        (!shouldRunDrone || (key == currentDroneKey_ && voicing == currentDroneVoicing_ && octave == currentDroneOctave_))) {
        return;
    }

    currentDroneActive_ = shouldRunDrone;
    currentDroneKey_ = key;
    currentDroneVoicing_ = voicing;
    currentDroneOctave_ = octave;

    if (!shouldRunDrone) {
        // Drone disabled or waiting for trigger: smoothly fade out all active drone voices
        for (auto& v : voices_) {
            if (v.active && v.isDroneVoice && v.heldByDrone) {
                v.heldByDrone = false;
                v.ampEnv.noteOff();
                v.filterEnv.noteOff();
                v.modEnv.noteOff();
                v.auxEnv.noteOff();
            }
        }
        return;
    }

    // Drone enabled: calculate target chord voicing
    const int rootNote = std::clamp(48 + (octave * 12) + key, 24, 96);
    const float droneVel = std::clamp(params_.droneLevel, 0.15f, 0.95f);

    std::array<int, 4> targetNotes{};
    std::array<float, 4> targetVels{};
    int targetCount = 0;

    switch (voicing) {
        case 0: // Lush 5th: Root + 5th + 8ve + 12th
            targetNotes = { rootNote, rootNote + 7, rootNote + 12, rootNote + 19 };
            targetVels  = { droneVel * 0.65f, droneVel * 0.50f, droneVel * 0.42f, droneVel * 0.30f };
            targetCount = 4;
            break;
        case 1: // Deep Sub: Sub (-12) + Root + 5th
            targetNotes = { rootNote - 12, rootNote, rootNote + 7, 0 };
            targetVels  = { droneVel * 0.75f, droneVel * 0.55f, droneVel * 0.40f, 0.0f };
            targetCount = 3;
            break;
        case 2: // Ambient 9th: Root + 5th + 9th + 8ve
            targetNotes = { rootNote, rootNote + 7, rootNote + 14, rootNote + 12 };
            targetVels  = { droneVel * 0.65f, droneVel * 0.48f, droneVel * 0.40f, droneVel * 0.35f };
            targetCount = 4;
            break;
        case 3: // Celestial Octaves: Root + 8ve + 15th
            targetNotes = { rootNote, rootNote + 12, rootNote + 24, 0 };
            targetVels  = { droneVel * 0.65f, droneVel * 0.50f, droneVel * 0.35f, 0.0f };
            targetCount = 3;
            break;
        default:
            targetNotes = { rootNote, 0, 0, 0 };
            targetVels  = { droneVel * 0.65f, 0.0f, 0.0f, 0.0f };
            targetCount = 1;
            break;
    }

    // 1. Release currently held drone voices that are NOT part of the new chord
    for (auto& v : voices_) {
        if (v.active && v.isDroneVoice && v.heldByDrone) {
            bool keep = false;
            for (int i = 0; i < targetCount; ++i) {
                if (v.midiNote == targetNotes[static_cast<std::size_t>(i)]) {
                    keep = true;
                    break;
                }
            }
            if (!keep) {
                v.heldByDrone = false;
                // Keep v.isDroneVoice = true so it continues using drone scaling & direct reverb routing!
                v.ampEnv.noteOff();
                v.filterEnv.noteOff();
                v.modEnv.noteOff();
                v.auxEnv.noteOff();
            }
        }
    }

    // 2. Trigger new voices for any notes in the chord not already held
    for (int i = 0; i < targetCount; ++i) {
        const int note = targetNotes[static_cast<std::size_t>(i)];
        const float vel = targetVels[static_cast<std::size_t>(i)];

        bool alreadyHeld = false;
        for (auto& v : voices_) {
            if (v.active && v.isDroneVoice && v.heldByDrone && v.midiNote == note) {
                alreadyHeld = true;
                break;
            }
        }
        if (!alreadyHeld) {
            startVoice(note, vel, true);
        }
    }
}

void WorshipSynthEngine::stepArpeggiator(int numSamples) noexcept {
    if (!params_.arpEnabled) {
        if (currentArpNote_ >= 0) {
            for (auto& v : voices_) {
                if (v.active && !v.isDroneVoice && v.isArpVoice) {
                    v.heldByKey = false;
                    v.ampEnv.noteOff();
                    v.filterEnv.noteOff();
                    v.modEnv.noteOff();
                    v.auxEnv.noteOff();
                }
            }
            currentArpNote_ = -1;
        }
        return;
    }

    std::array<int, 32> heldList{};
    int heldCount = 0;

    if (params_.arpLatch && latchedCount_ > 0) {
        heldCount = std::min(32, latchedCount_);
        for (int i = 0; i < heldCount; ++i) {
            heldList[static_cast<std::size_t>(i)] = latchedNotes_[static_cast<std::size_t>(i)];
        }
    } else {
        for (int n = 0; n < 128 && heldCount < 32; ++n) {
            if (physicalHeldNotes_[static_cast<std::size_t>(n)]) {
                heldList[static_cast<std::size_t>(heldCount++)] = n;
            }
        }
    }

    if (heldCount == 0) {
        if (currentArpNote_ >= 0) {
            for (auto& v : voices_) {
                if (v.active && !v.isDroneVoice && v.isArpVoice) {
                    v.heldByKey = false;
                    v.ampEnv.noteOff();
                    v.filterEnv.noteOff();
                    v.modEnv.noteOff();
                    v.auxEnv.noteOff();
                }
            }
            currentArpNote_ = -1;
        }
        return;
    }

    // Expand notes across octaves (1 to 4 octaves)
    const int octaves = std::clamp(params_.arpOctaves, 1, 4);
    std::array<int, 128> expandedList{};
    int totalNotes = 0;
    for (int oct = 0; oct < octaves && totalNotes < 128; ++oct) {
        for (int i = 0; i < heldCount && totalNotes < 128; ++i) {
            const int n = heldList[static_cast<std::size_t>(i)] + oct * 12;
            if (n <= 127) {
                expandedList[static_cast<std::size_t>(totalNotes++)] = n;
            }
        }
    }
    if (totalNotes == 0) return;

    const float effArpRate = params_.arpSync
        ? getArpRateForDivision(params_.bpm, params_.arpSyncDiv)
        : params_.arpRateHz;
    double stepSamples = std::max(256.0, sampleRate_ / std::clamp(static_cast<double>(effArpRate), 0.5, 32.0));

    // Swing calculation
    if (params_.arpSwing > 0.01f) {
        const double swingFactor = std::clamp(static_cast<double>(params_.arpSwing), 0.0, 0.75) * 0.4;
        if ((arpStepIndex_ % 2) == 0) {
            stepSamples *= (1.0 + swingFactor);
        } else {
            stepSamples *= std::max(0.3, 1.0 - swingFactor);
        }
    }

    const bool doImmediateStep = arpTriggerPending_;
    arpTriggerPending_ = false;

    if (!doImmediateStep) {
        const double prevPhase = arpPhaseSamples_;
        arpPhaseSamples_ += static_cast<double>(numSamples);

        // GATE CUTOFF: When phase passes gateThreshold, end the note with a crisp, click-free release!
        const double gateRatio = std::clamp(static_cast<double>(params_.arpGate), 0.05, 1.0);
        const double gateThreshold = stepSamples * gateRatio;

        if (prevPhase < gateThreshold && arpPhaseSamples_ >= gateThreshold) {
            for (auto& v : voices_) {
                if (v.active && !v.isDroneVoice && v.isArpVoice) {
                    v.heldByKey = false;
                    // Fast release tailored to remaining time before next step (staccato duration)
                    const double remainingSec = std::max(0.005, (stepSamples - arpPhaseSamples_) / sampleRate_);
                    auto ampParams = v.ampEnv.getParameters();
                    ampParams.releaseSec = std::clamp(static_cast<float>(remainingSec * 0.5), 0.008f, 0.050f);
                    v.ampEnv.setParameters(ampParams);

                    v.ampEnv.noteOff();
                    v.filterEnv.noteOff();
                    v.modEnv.noteOff();
                    v.auxEnv.noteOff();
                }
            }
        }
    }

    // STEP ADVANCE: When phase reaches stepSamples, trigger the next step!
    if (doImmediateStep || arpPhaseSamples_ >= stepSamples) {
        if (!doImmediateStep) {
            arpPhaseSamples_ = std::fmod(arpPhaseSamples_, stepSamples);
        }

        const int patternLen = (params_.arpPatternLength == 32) ? 32
                             : (params_.arpPatternLength == 64) ? 64 : 16;
        currentArpStep_ = arpStepIndex_ % patternLen;
        const bool stepGate = params_.arpStepGates[static_cast<std::size_t>(currentArpStep_)];
        const float stepVel = std::clamp(params_.arpStepVelocities[static_cast<std::size_t>(currentArpStep_)], 0.05f, 1.0f);

        // Terminate any previous arp notes cleanly on step boundaries
        for (auto& v : voices_) {
            if (v.active && !v.isDroneVoice && v.isArpVoice) {
                v.heldByKey = false;
                auto ampParams = v.ampEnv.getParameters();
                ampParams.releaseSec = 0.008f; // Rapid 8ms fade to avoid clicks while guaranteeing silence on rests
                v.ampEnv.setParameters(ampParams);
                v.ampEnv.noteOff();
                v.filterEnv.noteOff();
                v.modEnv.noteOff();
                v.auxEnv.noteOff();
            }
        }

        int pickIdx = 0;
        switch (params_.arpMode) {
            case ArpMode::Up:
                pickIdx = arpStepIndex_ % totalNotes;
                break;
            case ArpMode::Down:
                pickIdx = (totalNotes - 1) - (arpStepIndex_ % totalNotes);
                break;
            case ArpMode::UpDown: {
                const int cycle = std::max(1, totalNotes * 2 - 2);
                const int pos = arpStepIndex_ % cycle;
                pickIdx = (pos < totalNotes) ? pos : (cycle - pos);
                break;
            }
            case ArpMode::DownUp: {
                const int cycle = std::max(1, totalNotes * 2 - 2);
                const int pos = arpStepIndex_ % cycle;
                pickIdx = (pos < totalNotes) ? (totalNotes - 1 - pos) : (pos - totalNotes + 1);
                break;
            }
            case ArpMode::Random: {
                const uint32_t hash = static_cast<uint32_t>(arpStepIndex_ * 1664525u + 1013904223u);
                pickIdx = static_cast<int>(hash % static_cast<uint32_t>(totalNotes));
                break;
            }
            case ArpMode::ChordPulse:
                if (stepGate) {
                    for (int i = 0; i < totalNotes; ++i) {
                        startVoice(expandedList[static_cast<std::size_t>(i)], stepVel * 0.85f, false, true);
                    }
                    currentArpNote_ = expandedList[0];
                } else {
                    currentArpNote_ = -1;
                }
                ++arpStepIndex_;
                return;
        }

        ++arpStepIndex_;

        if (stepGate) {
            currentArpNote_ = expandedList[static_cast<std::size_t>(pickIdx)];
            startVoice(currentArpNote_, stepVel, false, true);
        } else {
            currentArpNote_ = -1;
        }
    }
}

void WorshipSynthEngine::processBlock(float* outLeft, float* outRight, int numSamples) noexcept {
    if (outLeft == nullptr || outRight == nullptr || numSamples <= 0) {
        return;
    }

    // Fast-path for absolute idle silence: Zero out buffers and return instantly!
    // No voice active, no held notes, arpeggiator inactive, no reverb/delay tails.
    bool anyVoiceActive = false;
    for (const auto& v : voices_) {
        if (v.active) {
            anyVoiceActive = true;
            break;
        }
    }
    bool anyKeyHeld = false;
    for (bool b : physicalHeldNotes_) {
        if (b) { anyKeyHeld = true; break; }
    }
    const bool arpActive = params_.arpEnabled && (anyKeyHeld || (params_.arpLatch && latchedCount_ > 0));
    const bool droneActive = params_.droneEnabled && droneTriggered_;

    if (!anyVoiceActive && !droneActive && !arpActive) {
        const auto revTel = reverb_.getTelemetry();
        if (delayEnergy_ < 1e-5f && revTel.tankEnergy < 1e-5f && !params_.reverbFreeze) {
            std::fill_n(outLeft, numSamples, 0.0f);
            std::fill_n(outRight, numSamples, 0.0f);
            telemetryPeakL_.store(0.0f, std::memory_order_relaxed);
            telemetryPeakR_.store(0.0f, std::memory_order_relaxed);
            telemetryActiveVoices_.store(0, std::memory_order_relaxed);
            return;
        }
    }

    constexpr int kMaxChunk = 128;
    if (numSamples > kMaxChunk) {
        for (int offset = 0; offset < numSamples; offset += kMaxChunk) {
            const int chunk = std::min(kMaxChunk, numSamples - offset);
            processBlock(outLeft + offset, outRight + offset, chunk);
        }
        return;
    }

    stepArpeggiator(numSamples);

    float peakL = 0.0f;
    float peakR = 0.0f;
    float lastLfo1 = 0.0f;
    float lastLfo2 = 0.0f;
    float lastLfo3 = 0.0f;
    float lastLfo4 = 0.0f;
    float lastEnvVal = 0.0f;
    float lastCutoffModOct = 0.0f;
    float lastCutoff2ModOct = 0.0f;
    float lastResoMod = 0.0f;
    float lastReso2Mod = 0.0f;

    modMatrix_.setSourceValue(ModMatrix::Source::ModWheel,     modWheel_);
    modMatrix_.setSourceValue(ModMatrix::Source::MacroWarmth,  params_.macroWarmth);
    modMatrix_.setSourceValue(ModMatrix::Source::MacroMotion,  params_.macroMotion);
    modMatrix_.setSourceValue(ModMatrix::Source::MacroShimmer, params_.macroShimmer);
    modMatrix_.setSourceValue(ModMatrix::Source::MacroAir,     params_.macroAir);

    // Determine lowest active notes for Monophonic Sub-Bass Tracking (Lowest-Note Priority)
    // Separately track keyboard-played notes and drone voices so crossfades don't delay or cut the sub!
    int lowestKeyNote = 128;
    for (const auto& v : voices_) {
        if (v.active && !v.isDroneVoice && (v.heldByKey || v.sustainedByPedal)) {
            lowestKeyNote = std::min(lowestKeyNote, v.midiNote);
        }
    }
    if (lowestKeyNote == 128) {
        for (const auto& v : voices_) {
            if (v.active && !v.isDroneVoice) {
                lowestKeyNote = std::min(lowestKeyNote, v.midiNote);
            }
        }
    }

    int lowestHeldDroneNote = 128;
    for (const auto& v : voices_) {
        if (v.active && v.isDroneVoice && v.heldByDrone) {
            lowestHeldDroneNote = std::min(lowestHeldDroneNote, v.midiNote);
        }
    }
    int lowestReleasingDroneNote = 128;
    for (const auto& v : voices_) {
        if (v.active && v.isDroneVoice && !v.heldByDrone) {
            lowestReleasingDroneNote = std::min(lowestReleasingDroneNote, v.midiNote);
        }
    }

    if (!anyVoiceActive) {
        std::fill_n(outLeft, numSamples, 0.0f);
        std::fill_n(outRight, numSamples, 0.0f);
        for (int s = 0; s < numSamples; ++s) {
            lastLfo1 = lfo1_.processSample();
            lastLfo2 = lfo2_.processSample();
            (void)lfo3_.processSample();
            (void)lfo4_.processSample();
        }
    } else {
        int activeNonDroneVoices = 0;
        std::array<int, kMaxVoices> activeVoiceIndices{};
        int activeVoiceCount = 0;
        for (int i = 0; i < kMaxVoices; ++i) {
            if (voices_[static_cast<std::size_t>(i)].active) {
                activeVoiceIndices[static_cast<std::size_t>(activeVoiceCount++)] = i;
                if (!voices_[static_cast<std::size_t>(i)].isDroneVoice) {
                    ++activeNonDroneVoices;
                }
            }
        }
        const float targetPolyScale = 1.0f / std::sqrt(1.0f + 0.45f * static_cast<float>(std::max(0, activeNonDroneVoices - 1)));
        polyHeadroomSmoother_.setTargetValue(targetPolyScale);

        const float flt1Drive = params_.filterDrive;
        const bool flt1DriveActive = (flt1Drive > 0.01f);
        const float flt1DriveGain = 1.0f + flt1Drive * 2.2f;
        const float flt1DriveComp = flt1DriveActive ? (1.0f / std::sqrt(1.0f + flt1Drive * 1.4f)) : 1.0f;

        const float flt2Drive = params_.filter2Drive;
        const bool flt2DriveActive = (flt2Drive > 0.01f);
        const float flt2DriveGain = 1.0f + flt2Drive * 2.2f;
        const float flt2DriveComp = flt2DriveActive ? (1.0f / std::sqrt(1.0f + flt2Drive * 1.4f)) : 1.0f;

        bool modEnvInUse = false;
        bool auxEnvInUse = false;
        for (int i = 0; i < params_.activeOscCount; ++i) {
            if (params_.oscSlots[static_cast<std::size_t>(i)].ampEnvSource == 2) modEnvInUse = true;
            if (params_.oscSlots[static_cast<std::size_t>(i)].ampEnvSource == 3) auxEnvInUse = true;
        }
        for (std::size_t sIdx = 0; sIdx < ModMatrix::kMaxSlots; ++sIdx) {
            const auto& sl = modMatrix_.getSlot(sIdx);
            if (sl.enabled) {
                if (sl.source == ModMatrix::Source::ModEnv3) modEnvInUse = true;
                if (sl.source == ModMatrix::Source::AuxEnv4) auxEnvInUse = true;
            }
        }

        for (int s = 0; s < numSamples; ++s) {
            const float lfo1Val = lfo1_.processSample();
            const float lfo2Val = lfo2_.processSample();
            const float lfo3Val = lfo3_.processSample();
            const float lfo4Val = lfo4_.processSample();
            lastLfo1 = lfo1Val;
            lastLfo2 = lfo2Val;
            lastLfo3 = lfo3Val;
            lastLfo4 = lfo4Val;

            const float baseCutoff = cutoffSmoother_.getNextValue();
            const float baseCutoff2 = cutoff2Smoother_.getNextValue();
            const float masterGain = masterGainSmoother_.getNextValue();
            const float polyHeadroom = polyHeadroomSmoother_.getNextValue();

        if ((s & 7) == 0) {
            modMatrix_.setSourceValue(ModMatrix::Source::Lfo1, lfo1Val);
            modMatrix_.setSourceValue(ModMatrix::Source::Lfo2, lfo2Val);
            modMatrix_.setSourceValue(ModMatrix::Source::Lfo3, lfo3Val);
            modMatrix_.setSourceValue(ModMatrix::Source::Lfo4, lfo4Val);
            modMatrix_.evaluate();
        }

        const float pitchMod = pitchBendSemitones_
                             + modMatrix_.getModulationOffset(ModMatrix::Destination::OscPitchAll) * 2.0f;
        const float oscShapeMod1 = modMatrix_.getModulationOffset(ModMatrix::Destination::Osc1Shape);
        const float oscShapeMod2 = modMatrix_.getModulationOffset(ModMatrix::Destination::Osc2Shape);
        const float oscShapeMod3 = modMatrix_.getModulationOffset(ModMatrix::Destination::Osc3Shape);
        const float oscShapeMod4 = modMatrix_.getModulationOffset(ModMatrix::Destination::Osc4Shape);
        const float cutoffModOctaves = modMatrix_.getModulationOffset(ModMatrix::Destination::FilterCutoff) * 1.8f;
        const float filterResoMod = modMatrix_.getModulationOffset(ModMatrix::Destination::FilterResonance) * 0.5f;
        const float vocalCarveMod = modMatrix_.getModulationOffset(ModMatrix::Destination::VocalCarve);
        lastCutoffModOct = cutoffModOctaves;
        lastCutoff2ModOct = modMatrix_.getModulationOffset(ModMatrix::Destination::Filter2Cutoff) * 1.8f;
        lastResoMod = filterResoMod;
        lastReso2Mod = modMatrix_.getModulationOffset(ModMatrix::Destination::Filter2Resonance) * 0.5f;

        float synthL = 0.0f;
        float synthR = 0.0f;
        float droneL = 0.0f;
        float droneR = 0.0f;

        for (int vi = 0; vi < activeVoiceCount; ++vi) {
            auto& v = voices_[static_cast<std::size_t>(activeVoiceIndices[static_cast<std::size_t>(vi)])];
            if (!v.active) continue;

            const float ampEnvVal = v.ampEnv.processSample();
            const float fltEnvVal = v.filterEnv.processSample();
            const float modEnvVal = v.modEnv.processSample();
            const float auxEnvVal = v.auxEnv.processSample();
            lastEnvVal = std::max(lastEnvVal, fltEnvVal);

            // Voice Activity Culling: Instant termination when release has faded to silence
            bool anyEnvActive = v.ampEnv.isActive();
            float maxEnv = ampEnvVal;
            if (modEnvInUse) {
                anyEnvActive = anyEnvActive || v.modEnv.isActive();
                maxEnv = std::max(maxEnv, modEnvVal);
            }
            if (auxEnvInUse) {
                anyEnvActive = anyEnvActive || v.auxEnv.isActive();
                maxEnv = std::max(maxEnv, auxEnvVal);
            }

            if (!anyEnvActive || (!v.heldByKey && !v.sustainedByPedal && !v.heldByDrone && maxEnv < 1e-4f)) {
                v.active = false;
                v.isDroneVoice = false;
                v.heldByDrone = false;
                continue;
            }

            const ModMatrix::VoiceSources vSources{
                ampEnvVal, fltEnvVal, modEnvVal, auxEnvVal, v.velocity
            };
            const auto voiceOffsets = modMatrix_.evaluateVoice(vSources);

            float voiceFlt1L = 0.0f, voiceFlt1R = 0.0f;
            float voiceFlt2L = 0.0f, voiceFlt2R = 0.0f;
            float voiceBothL = 0.0f, voiceBothR = 0.0f;
            float voiceDirL  = 0.0f, voiceDirR  = 0.0f;

            const float oscNorm = v.oscNormSnapshot;
            for (int i = 0; i < v.activeOscCountSnapshot; ++i) {
                const auto& slotParam = params_.oscSlots[static_cast<std::size_t>(i)];
                const bool isSubSlot = (slotParam.sourceType == 1 ||
                    slotParam.engine == MultiEngineOscillator::EngineType::SubOscillator);
                if (isSubSlot && params_.subLowestOnly) {
                    if (v.isDroneVoice) {
                        const bool isLowestDrone = v.heldByDrone
                            ? (v.midiNote == lowestHeldDroneNote)
                            : (v.midiNote == lowestReleasingDroneNote);
                        if (!isLowestDrone) {
                            continue;
                        }
                    } else {
                        if (v.midiNote != lowestKeyNote) {
                            continue;
                        }
                    }
                }

                const float sMod = (i == 0) ? oscShapeMod1
                                 : ((i == 1) ? oscShapeMod2
                                 : ((i == 2) ? oscShapeMod3
                                 : ((i == 3) ? oscShapeMod4 : 0.0f)));
                const float slotPitchMod = (i == 0) ? voiceOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::Osc1Pitch)]
                                         : ((i == 1) ? voiceOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::Osc2Pitch)]
                                         : ((i == 2) ? voiceOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::Osc3Pitch)]
                                         : voiceOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::Osc4Pitch)]));
                const float slotLevelMod = (i == 0) ? voiceOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::Osc1Level)]
                                         : ((i == 1) ? voiceOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::Osc2Level)]
                                         : ((i == 2) ? voiceOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::Osc3Level)]
                                         : voiceOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::Osc4Level)]));

                const float vPitch = pitchMod + slotPitchMod * 24.0f;
                float oL = 0.0f, oR = 0.0f;
                v.oscs[static_cast<std::size_t>(i)].processSample(
                    v.baseFreqHz, vPitch, sMod, oL, oR, slotLevelMod);

                float effEnv = ampEnvVal;
                if (slotParam.ampEnvSource == 1)      effEnv = fltEnvVal;
                else if (slotParam.ampEnvSource == 2) effEnv = modEnvVal;
                else if (slotParam.ampEnvSource == 3) effEnv = auxEnvVal;

                oL *= (oscNorm * effEnv);
                oR *= (oscNorm * effEnv);

                const int route = params_.oscSlots[static_cast<std::size_t>(i)].filterRouting;
                if (route == 0) {
                    voiceFlt1L += oL; voiceFlt1R += oR;
                } else if (route == 1) {
                    voiceFlt2L += oL; voiceFlt2R += oR;
                } else if (route == 2) {
                    voiceBothL += oL; voiceBothR += oR;
                } else {
                    voiceDirL  += oL; voiceDirR  += oR;
                }
            }

            // Route 0 (FLT 1) and Route 2 (BOTH) feed into Filter 1
            float flt1InL = voiceFlt1L + voiceBothL;
            float flt1InR = voiceFlt1R + voiceBothR;

            if (params_.filter1Enabled) {
                const float voiceResoMod = voiceOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::FilterResonance)] * 0.5f;
                const float voiceCutMod = voiceOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::FilterCutoff)] * 1.8f;
                const float effReso = std::clamp(params_.filterResonance + filterResoMod + voiceResoMod, 0.35f, 6.0f);
                const bool isFourPoleLadder = (params_.filterType == 0 || params_.filterType == 2);

                if ((s & 15) == 0) {
                    const float baseCut = baseCutoff;
                    const float envOctaves = fltEnvVal * params_.filterEnvAmount * 2.1f;
                    const float voiceCutoff = std::clamp(
                        baseCut * std::exp2(envOctaves + cutoffModOctaves + voiceCutMod),
                        40.0f, static_cast<float>(sampleRate_ * 0.46));

                    auto svfType = StateVariableFilter::Type::LowPass;
                    if (params_.filterType == 1) svfType = StateVariableFilter::Type::BandPass;
                    else if (params_.filterType == 2) svfType = StateVariableFilter::Type::HighPass;

                    if (isFourPoleLadder) {
                        // True 24 dB/oct 4-pole ladder cascade: split Q across two 2-pole stages with slight pole stagger
                        const float stageQ = std::clamp(std::sqrt(effReso), 0.50f, 2.45f);
                        const float cutStage2 = std::clamp(voiceCutoff * 1.03f, 40.0f, static_cast<float>(sampleRate_ * 0.46));
                        v.filterL.setParameters(svfType, voiceCutoff, stageQ);
                        v.filterR.setParameters(svfType, voiceCutoff, stageQ);
                        v.filterStage2L.setParameters(svfType, cutStage2, stageQ);
                        v.filterStage2R.setParameters(svfType, cutStage2, stageQ);
                    } else {
                        v.filterL.setParameters(svfType, voiceCutoff, effReso);
                        v.filterR.setParameters(svfType, voiceCutoff, effReso);
                    }
                    v.flt1ResComp = 1.0f / std::sqrt(1.0f + 0.35f * std::max(0.0f, effReso - 0.707f));
                }

                // Zero-DC warm tube/ladder drive saturation (prevents DC bias build-up across chord voices!)
                if (flt1DriveActive) {
                    auto tubeSat = [](float x) noexcept {
                        const float x2 = x * x;
                        return (x * (27.0f + x2) / (27.0f + 9.0f * x2));
                    };
                    flt1InL = tubeSat(flt1InL * flt1DriveGain) * flt1DriveComp;
                    flt1InR = tubeSat(flt1InR * flt1DriveGain) * flt1DriveComp;
                }

                flt1InL = v.filterL.processSample(flt1InL);
                flt1InR = v.filterR.processSample(flt1InR);
                if (isFourPoleLadder) {
                    flt1InL = v.filterStage2L.processSample(flt1InL);
                    flt1InR = v.filterStage2R.processSample(flt1InR);
                }

                flt1InL *= v.flt1ResComp;
                flt1InR *= v.flt1ResComp;
            }

            const bool hasBoth = (voiceBothL != 0.0f || voiceBothR != 0.0f);
            const bool hasDedicatedFlt2 = (voiceFlt2L != 0.0f || voiceFlt2R != 0.0f);
            // When Filter 2 is enabled and no oscillators are routed exclusively to FLT 2,
            // route Filter 1 in series into Filter 2 so Filter 2 always shapes the voice!
            const bool seriesFlt1IntoFlt2 = params_.filter2Enabled && !hasDedicatedFlt2 && !hasBoth;

            float flt2InL = seriesFlt1IntoFlt2 ? flt1InL : (voiceFlt2L + (hasBoth ? (flt1InL * 0.5f) : 0.0f));
            float flt2InR = seriesFlt1IntoFlt2 ? flt1InR : (voiceFlt2R + (hasBoth ? (flt1InR * 0.5f) : 0.0f));

            if (params_.filter2Enabled) {
                const float voiceResoMod2 = voiceOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::Filter2Resonance)] * 0.5f;
                const float voiceCutMod2 = voiceOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::Filter2Cutoff)] * 1.8f;
                const float effReso2 = std::clamp(params_.filter2Resonance + voiceResoMod2, 0.35f, 6.0f);
                if ((s & 15) == 0) {
                    const float baseCut2 = baseCutoff2;
                    const float envOctaves2 = fltEnvVal * params_.filter2EnvAmount * 2.1f;
                    const float voiceCutoff2 = std::clamp(
                        baseCut2 * std::exp2(envOctaves2 + voiceCutMod2),
                        40.0f, static_cast<float>(sampleRate_ * 0.46));

                    auto svf2Type = StateVariableFilter::Type::LowPass;
                    float bellGainDb = 0.0f;
                    if (params_.filter2Type == 1) svf2Type = StateVariableFilter::Type::BandPass;
                    else if (params_.filter2Type == 2) svf2Type = StateVariableFilter::Type::HighPass;
                    else if (params_.filter2Type == 3) {
                        svf2Type = StateVariableFilter::Type::Bell;
                        bellGainDb = -12.0f * std::clamp(params_.vocalCarve, 0.25f, 1.0f);
                    }

                    v.filter2L.setParameters(svf2Type, voiceCutoff2, effReso2, bellGainDb);
                    v.filter2R.setParameters(svf2Type, voiceCutoff2, effReso2, bellGainDb);
                    v.flt2ResComp = 1.0f / std::sqrt(1.0f + 0.30f * std::max(0.0f, effReso2 - 0.707f));
                }

                if (flt2DriveActive) {
                    auto tubeSat2 = [](float x) noexcept {
                        const float x2 = x * x;
                        return (x * (27.0f + x2) / (27.0f + 9.0f * x2));
                    };
                    flt2InL = tubeSat2(flt2InL * flt2DriveGain) * flt2DriveComp;
                    flt2InR = tubeSat2(flt2InR * flt2DriveGain) * flt2DriveComp;
                }

                flt2InL = v.filter2L.processSample(flt2InL);
                flt2InR = v.filter2R.processSample(flt2InR);
                flt2InL *= v.flt2ResComp;
                flt2InR *= v.flt2ResComp;
            }

            const float velGain = v.isArpVoice ? (v.velocity * (0.15f + 0.85f * v.velocity)) : (0.40f + 0.60f * v.velocity);
            const float droneScale = v.isDroneVoice ? (params_.droneLevel * 0.70f) : polyHeadroom;
            const float gain = velGain * droneScale * 0.82f;

            float vOutL = 0.0f;
            float vOutR = 0.0f;
            if (seriesFlt1IntoFlt2) {
                vOutL = (flt2InL + voiceDirL) * gain;
                vOutR = (flt2InR + voiceDirR) * gain;
            } else if (hasBoth) {
                vOutL = (flt1InL * 0.5f + flt2InL + voiceDirL) * gain;
                vOutR = (flt1InR * 0.5f + flt2InR + voiceDirR) * gain;
            } else {
                vOutL = (flt1InL + flt2InL + voiceDirL) * gain;
                vOutR = (flt1InR + flt2InR + voiceDirR) * gain;
            }

            if (v.isDroneVoice) {
                droneL += vOutL;
                droneR += vOutR;
            } else {
                synthL += vOutL;
                synthR += vOutR;
            }
        }

        // Optional subtle Vocal Carve scoop if enabled on played synth voices
        const float effCarve = std::clamp(params_.vocalCarve + vocalCarveMod, 0.0f, 1.0f);
        if (effCarve > 0.02f) {
            synthL = vocalCarveL_.processSample(synthL);
            synthR = vocalCarveR_.processSample(synthR);
        }

        // Dedicated Drone Warmth Filter
        droneToneSmoother_.setTargetValue(std::clamp(params_.droneTone, 0.0f, 1.0f));
        const float toneNorm = droneToneSmoother_.getNextValue();
        if ((s & 15) == 0) {
            // Warmth sweeps smoothly from dark rumble (180 Hz) to open airy brightness (14000 Hz)
            const float toneCutoff = std::clamp(180.0f * std::pow(77.0f, toneNorm), 140.0f, static_cast<float>(sampleRate_ * 0.45));
            droneFilterL_.setParameters(StateVariableFilter::Type::LowPass, toneCutoff, 0.15f);
            droneFilterR_.setParameters(StateVariableFilter::Type::LowPass, toneCutoff, 0.15f);
        }
        droneL = droneFilterL_.processSample(droneL);
        droneR = droneFilterR_.processSample(droneR);

        outLeft[s]  = synthL * masterGain;
        outRight[s] = synthR * masterGain;
        if (static_cast<std::size_t>(s) < droneBufferL_.size()) {
            droneBufferL_[static_cast<std::size_t>(s)] = droneL * masterGain;
            droneBufferR_[static_cast<std::size_t>(s)] = droneR * masterGain;
        }
    }
    }

    // Embedded Stereo Tape / Ping-Pong Delay (processes synth notes; drone bypasses rhythmic delay)
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

        // Delay sleep-on-silence: skip buffer loops when input is silent and echoes have died away
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

    // Sum Drone Audio Stream into Master Output
    for (int s = 0; s < numSamples; ++s) {
        if (static_cast<std::size_t>(s) < droneBufferL_.size()) {
            outLeft[s]  += droneBufferL_[static_cast<std::size_t>(s)];
            outRight[s] += droneBufferR_[static_cast<std::size_t>(s)];
        }
    }

    // Only run reverb tank if wet mix is active or tank still has decaying energy / freeze
    const float revMod = modMatrix_.getModulationOffset(ModMatrix::Destination::ReverbMix);
    const float shimMod = modMatrix_.getModulationOffset(ModMatrix::Destination::ShimmerAmount);
    if (std::abs(revMod) > 0.001f || std::abs(shimMod) > 0.001f) {
        auto revParams = reverb_.getParameters();
        revParams.mix = std::clamp(params_.reverbMix + params_.macroShimmer * 0.25f + revMod, 0.0f, 1.0f);
        revParams.shimmerOctave = std::clamp(params_.reverbShimmer + params_.macroShimmer * 0.50f + shimMod, 0.0f, 1.0f);
        reverb_.setParameters(revParams);
    }

    const auto revTel = reverb_.getTelemetry();
    const bool revActive = (params_.reverbMix > 0.001f || std::abs(revMod) > 0.001f || params_.reverbFreeze || revTel.tankEnergy > 1e-5f || revTel.outputPeak > 1e-5f);
    if (revActive) {
        reverb_.processBlock(outLeft, outRight, numSamples);
    }

    // Musical Master Soft-Clipper (Analog Tape Saturation Stage)
    // Ensures single notes and big Worship chords sound loud, present, and full without digital clipping.
    // 100% linear / pristine below 0.70 (-3.1 dBFS), smoothly curving asymptotically to 0.98 (-0.18 dBFS).
    float preClipPeak = 0.0f;
    for (int s = 0; s < numSamples; ++s) {
        preClipPeak = std::max(preClipPeak, std::max(std::abs(outLeft[s]), std::abs(outRight[s])));
    }

    if (preClipPeak > 1e-6f) {
        for (int s = 0; s < numSamples; ++s) {
            const auto softClip = [](float x) noexcept -> float {
                constexpr float threshold = 0.70f;
                constexpr float limit = 0.98f;
                const float ax = std::abs(x);
                if (ax <= threshold) {
                    return x;
                }
                const float sign = (x > 0.0f) ? 1.0f : -1.0f;
                const float excess = ax - threshold;
                const float ex = excess / (limit - threshold);
                const float ex2 = ex * ex;
                const float tanhVal = (ex <= 2.5f) ? (ex * (27.0f + ex2) / (27.0f + 9.0f * ex2)) : 1.0f;
                const float sat = threshold + (limit - threshold) * tanhVal;
                return sign * sat;
            };

            outLeft[s]  = softClip(outLeft[s]);
            outRight[s] = softClip(outRight[s]);

            peakL = std::max(peakL, std::abs(outLeft[s]));
            peakR = std::max(peakR, std::abs(outRight[s]));
            if ((s & 3) == 0) {
                visualizerRingBuffer_.pushOverwrite(0.5f * (outLeft[s] + outRight[s]));
            }
        }
    } else {
        peakL = 0.0f;
        peakR = 0.0f;
    }

    int activeCount = 0;
    for (const auto& v : voices_) {
        if (v.active) ++activeCount;
    }

    telemetryActiveVoices_.store(activeCount, std::memory_order_relaxed);
    telemetryPeakL_.store(peakL, std::memory_order_relaxed);
    telemetryPeakR_.store(peakR, std::memory_order_relaxed);
    telemetryPreClipPeak_.store(preClipPeak, std::memory_order_relaxed);
    telemetryLfo1_.store(lastLfo1, std::memory_order_relaxed);
    telemetryLfo2_.store(lastLfo2, std::memory_order_relaxed);
    telemetryLfo3_.store(lastLfo3, std::memory_order_relaxed);
    telemetryLfo4_.store(lastLfo4, std::memory_order_relaxed);
    telemetryFilterCutoffMod_.store(lastCutoffModOct, std::memory_order_relaxed);
    telemetryFilter2CutoffMod_.store(lastCutoff2ModOct, std::memory_order_relaxed);
    telemetryFilterResoMod_.store(lastResoMod, std::memory_order_relaxed);
    telemetryFilter2ResoMod_.store(lastReso2Mod, std::memory_order_relaxed);
    telemetryLastEnvVal_.store(lastEnvVal, std::memory_order_relaxed);
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
