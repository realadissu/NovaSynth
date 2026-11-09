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
        for (auto& v : voices_) {
            if (v.active && !v.isDroneVoice && v.isArpVoice && v.heldByArp) {
                v.heldByArp = false;
                v.ampEnv.noteOff();
                v.filterEnv.noteOff();
                v.modEnv.noteOff();
                v.auxEnv.noteOff();
            }
        }
    }

    for (auto& v : voices_) {
        if (v.active && !v.isDroneVoice && !v.isArpVoice && v.midiNote == midiNote) {
            v.heldByKey = false;
            if (sustainPedalDown_) {
                v.sustainedByPedal = true;
            } else {
                const float ampRelMod = modMatrix_.getModulationOffset(ModMatrix::Destination::AmpRelease);
                if (std::abs(ampRelMod) > 1e-4f) {
                    auto p = v.ampEnv.getParameters();
                    p.releaseSec = applyEnvTimeMod(params_.ampEnv.releaseSec, ampRelMod, 0.005f, 20.0f);
                    v.ampEnv.setParameters(p);
                }
                const float fltRelMod = modMatrix_.getModulationOffset(ModMatrix::Destination::FilterRelease);
                if (std::abs(fltRelMod) > 1e-4f) {
                    auto p = v.filterEnv.getParameters();
                    p.releaseSec = applyEnvTimeMod(params_.filterEnv.releaseSec, fltRelMod, 0.005f, 20.0f);
                    v.filterEnv.setParameters(p);
                }
                const float modRelMod = modMatrix_.getModulationOffset(ModMatrix::Destination::ModEnv3Release);
                if (std::abs(modRelMod) > 1e-4f) {
                    auto p = v.modEnv.getParameters();
                    p.releaseSec = applyEnvTimeMod(params_.modEnv.releaseSec, modRelMod, 0.005f, 20.0f);
                    v.modEnv.setParameters(p);
                }
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
            v.heldByArp = false;
            v.ampEnv.noteOff();
            v.filterEnv.noteOff();
            v.modEnv.noteOff();
            v.auxEnv.noteOff();
        }
    }
}

void WorshipSynthEngine::stopActiveArpVoices() noexcept {
    for (auto& v : voices_) {
        if (v.active && !v.isDroneVoice && v.isArpVoice) {
            v.heldByKey = false;
            v.heldByArp = false;
            constexpr float kStepEndRelease = 0.015f;
            auto ampParams = v.ampEnv.getParameters();
            ampParams.releaseSec = std::min(ampParams.releaseSec, kStepEndRelease);
            v.ampEnv.setParameters(ampParams);

            auto fltParams = v.filterEnv.getParameters();
            fltParams.releaseSec = std::min(fltParams.releaseSec, kStepEndRelease);
            v.filterEnv.setParameters(fltParams);

            auto modParams = v.modEnv.getParameters();
            modParams.releaseSec = std::min(modParams.releaseSec, kStepEndRelease);
            v.modEnv.setParameters(modParams);

            auto auxParams = v.auxEnv.getParameters();
            auxParams.releaseSec = std::min(auxParams.releaseSec, kStepEndRelease);
            v.auxEnv.setParameters(auxParams);

            v.ampEnv.noteOff();
            v.filterEnv.noteOff();
            v.modEnv.noteOff();
            v.auxEnv.noteOff();
        }
    }
    arpPhaseSamples_ = 0.0;
    arpTriggerPending_ = true;
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

    // 2. If all voices are active, steal a released voice (not held by key, pedal, or arp) with the quietest envelope level
    if (chosen == nullptr) {
        float lowestLevel = 1e9f;
        for (auto& v : voices_) {
            if (!v.isDroneVoice && !v.heldByKey && !v.sustainedByPedal && !v.heldByArp) {
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
            if (!v.isDroneVoice && !v.heldByKey && !v.sustainedByPedal && !v.heldByArp && v.ageCounter < oldestAge) {
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
    chosen->heldByArp = isArp;
    chosen->midiNote = midiNote;
    chosen->baseFreqHz = midiNoteToHz(midiNote, params_.masterTuningHz);
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
    } else {
        const ModMatrix::VoiceSources initSources{
            0.0f, 0.0f, 0.0f, 0.0f, chosen->velocity
        };
        const auto initOffsets = modMatrix_.evaluateVoice(initSources);

        auto effAmp = params_.ampEnv;
        const float ampAttMod = initOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::AmpAttack)];
        const float ampDecMod = initOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::AmpDecay)];
        const float ampRelMod = initOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::AmpRelease)];
        effAmp.attackSec  = applyEnvTimeMod(effAmp.attackSec,  ampAttMod, 0.001f, 15.0f);
        effAmp.decaySec   = applyEnvTimeMod(effAmp.decaySec,   ampDecMod, 0.005f, 15.0f);
        effAmp.releaseSec = applyEnvTimeMod(effAmp.releaseSec, ampRelMod, 0.005f, 20.0f);
        chosen->ampEnv.setParameters(effAmp);

        auto effFlt = params_.filterEnv;
        const float fltAttMod = initOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::FilterAttack)];
        const float fltDecMod = initOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::FilterDecay)];
        const float fltRelMod = initOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::FilterRelease)];
        effFlt.attackSec  = applyEnvTimeMod(effFlt.attackSec,  fltAttMod, 0.001f, 15.0f);
        effFlt.decaySec   = applyEnvTimeMod(effFlt.decaySec,   fltDecMod, 0.005f, 15.0f);
        effFlt.releaseSec = applyEnvTimeMod(effFlt.releaseSec, fltRelMod, 0.005f, 20.0f);
        chosen->filterEnv.setParameters(effFlt);

        auto effMod = params_.modEnv;
        const float modAttMod = initOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::ModEnv3Attack)];
        const float modDecMod = initOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::ModEnv3Decay)];
        const float modRelMod = initOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::ModEnv3Release)];
        effMod.attackSec  = applyEnvTimeMod(effMod.attackSec,  modAttMod, 0.001f, 15.0f);
        effMod.decaySec   = applyEnvTimeMod(effMod.decaySec,   modDecMod, 0.005f, 15.0f);
        effMod.releaseSec = applyEnvTimeMod(effMod.releaseSec, modRelMod, 0.005f, 20.0f);
        chosen->modEnv.setParameters(effMod);

        auto effAux = params_.auxEnv;
        const float auxAttMod = initOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::AuxEnv4Attack)];
        const float auxDecMod = initOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::AuxEnv4Decay)];
        const float auxRelMod = initOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::AuxEnv4Release)];
        effAux.attackSec  = applyEnvTimeMod(effAux.attackSec,  auxAttMod, 0.001f, 15.0f);
        effAux.decaySec   = applyEnvTimeMod(effAux.decaySec,   auxDecMod, 0.005f, 15.0f);
        effAux.releaseSec = applyEnvTimeMod(effAux.releaseSec, auxRelMod, 0.005f, 20.0f);
        chosen->auxEnv.setParameters(effAux);
    }
    if (isDrone) {
        chosen->modEnv.setParameters(params_.modEnv);
        chosen->auxEnv.setParameters(params_.auxEnv);
    }
    chosen->ampEnv.noteOn(chosen->velocity);
    chosen->filterEnv.noteOn(chosen->velocity);
    chosen->modEnv.noteOn(chosen->velocity);
    chosen->auxEnv.noteOn(chosen->velocity);
}


} // namespace nova::dsp
