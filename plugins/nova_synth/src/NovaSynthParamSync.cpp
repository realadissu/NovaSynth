#include "NovaSynthProcessor.h"
#include <cmath>
#include <algorithm>

namespace nova::synth {

void NovaSynthProcessor::syncParametersFromAPVTS() noexcept {
    using nova::dsp::WorshipSynthEngine;
    using nova::dsp::MultiEngineOscillator;

    WorshipSynthEngine::Parameters p;
    p.activeOscCount = static_cast<int>(apvts_.getRawParameterValue("activeOscCount")->load());

    const uint8_t sMask = soloMask_.load(std::memory_order_relaxed);
    const uint8_t mMask = muteMask_.load(std::memory_order_relaxed);

    for (int i = 1; i <= WorshipSynthEngine::kNumOscSlots; ++i) {
        const juce::String prefix = "osc" + juce::String(i) + "_";
        auto& slot = p.oscSlots[static_cast<std::size_t>(i - 1)];
        const bool userEnabled = apvts_.getRawParameterValue(prefix + "enabled")->load() > 0.5f;
        const bool isSoloActive = (sMask != 0);
        const bool isThisSoloed = ((sMask & (1 << (i - 1))) != 0);
        const bool isThisMuted = ((mMask & (1 << (i - 1))) != 0);

        // Real Studio Mixer Mute: independent of userEnabled (engine power switch)
        const bool passesSolo = !isSoloActive || isThisSoloed;
        slot.enabled = userEnabled && !isThisMuted && passesSolo;
        if (slot.enabled) {
            p.activeOscCount = std::max(p.activeOscCount, i);
        }

        const int sType = static_cast<int>(apvts_.getRawParameterValue(prefix + "sourceType")->load());
        slot.sourceType = sType;
        slot.modelIndex = static_cast<int>(apvts_.getRawParameterValue(prefix + "model")->load());
        slot.filterRouting = static_cast<int>(apvts_.getRawParameterValue(prefix + "filterRouting")->load());

        if (sType == 1) {
            slot.engine = MultiEngineOscillator::EngineType::SubOscillator;
        } else if (sType == 2) {
            slot.engine = MultiEngineOscillator::EngineType::PhysicalPluckModal;
        } else if (sType == 3) {
            slot.engine = MultiEngineOscillator::EngineType::NoiseTransient;
        } else if (sType == 4) {
            slot.engine = MultiEngineOscillator::EngineType::SamplePlayback;
        } else {
            const int eng = static_cast<int>(apvts_.getRawParameterValue(prefix + "engine")->load());
            if (eng == 3) {
                slot.engine = MultiEngineOscillator::EngineType::SamplePlayback;
            } else {
                slot.engine = static_cast<MultiEngineOscillator::EngineType>(std::clamp(eng, 0, 2));
            }
        }

        slot.octaveOffset   = static_cast<int>(apvts_.getRawParameterValue(prefix + "octave")->load());
        slot.semitoneOffset = static_cast<int>(apvts_.getRawParameterValue(prefix + "semi")->load());
        slot.shapeMorph     = apvts_.getRawParameterValue(prefix + "shape")->load();
        slot.detune         = apvts_.getRawParameterValue(prefix + "detune")->load();
        slot.pan            = apvts_.getRawParameterValue(prefix + "pan")->load();
        slot.level          = apvts_.getRawParameterValue(prefix + "level")->load();
        if (auto* pR = apvts_.getRawParameterValue(prefix + "rate")) slot.rate = pR->load();
        if (auto* pSc = apvts_.getRawParameterValue(prefix + "scatter")) slot.scatter = pSc->load();
        if (auto* pDu = apvts_.getRawParameterValue(prefix + "duration")) slot.duration = pDu->load();
        if (auto* pSp = apvts_.getRawParameterValue(prefix + "spread")) slot.spread = pSp->load();
        if (auto* pW = apvts_.getRawParameterValue(prefix + "warp")) slot.warp = pW->load();
        if (auto* pL = apvts_.getRawParameterValue(prefix + "lowestNoteOnly")) slot.lowestNoteOnly = pL->load() > 0.5f;
        if (auto* pEnv = apvts_.getRawParameterValue(prefix + "ampEnvSource")) slot.ampEnvSource = static_cast<int>(pEnv->load());
        if (auto* pLm = apvts_.getRawParameterValue(prefix + "loopMode")) slot.sampleLoopMode = static_cast<int>(pLm->load());
        if (auto* pLs = apvts_.getRawParameterValue(prefix + "loopStart")) slot.sampleLoopStart = pLs->load();
        if (auto* pLe = apvts_.getRawParameterValue(prefix + "loopEnd")) slot.sampleLoopEnd = pLe->load();
        if (auto* pLx = apvts_.getRawParameterValue(prefix + "loopXfade")) slot.sampleCrossfade = pLx->load();
    }

    p.filter1Enabled   = apvts_.getRawParameterValue("filter1Enabled")->load() > 0.5f;
    p.filterType       = static_cast<int>(apvts_.getRawParameterValue("filterType")->load());
    p.filterCutoffHz   = apvts_.getRawParameterValue("filterCutoffHz")->load();
    p.filterResonance  = apvts_.getRawParameterValue("filterResonance")->load();
    p.filterEnvAmount  = apvts_.getRawParameterValue("filterEnvAmount")->load();
    p.filterDrive      = apvts_.getRawParameterValue("filterDrive")->load();

    p.filter2Enabled   = apvts_.getRawParameterValue("filter2Enabled")->load() > 0.5f;
    p.filter2Type      = static_cast<int>(apvts_.getRawParameterValue("filter2Type")->load());
    p.filter2CutoffHz  = apvts_.getRawParameterValue("filter2CutoffHz")->load();
    p.filter2Resonance = apvts_.getRawParameterValue("filter2Resonance")->load();
    p.filter2EnvAmount = apvts_.getRawParameterValue("filter2EnvAmount")->load();
    p.filter2Drive     = apvts_.getRawParameterValue("filter2Drive")->load();
    p.vocalCarve       = apvts_.getRawParameterValue("vocalCarve")->load();
    if (auto* pFltMode = apvts_.getRawParameterValue("filterRoutingMode")) {
        p.filterRoutingMode = static_cast<int>(pFltMode->load());
    }

    if (auto* pDel = apvts_.getRawParameterValue("ampDelay")) p.ampEnv.delaySec = pDel->load();
    p.ampEnv.attackSec     = apvts_.getRawParameterValue("ampAttack")->load();
    if (auto* pHold = apvts_.getRawParameterValue("ampHold")) p.ampEnv.holdSec = pHold->load();
    p.ampEnv.decaySec      = apvts_.getRawParameterValue("ampDecay")->load();
    p.ampEnv.sustainLvl    = apvts_.getRawParameterValue("ampSustain")->load();
    p.ampEnv.releaseSec    = apvts_.getRawParameterValue("ampRelease")->load();
    if (auto* pVel = apvts_.getRawParameterValue("ampVelocity")) p.ampEnv.velocitySens = pVel->load();

    if (auto* pDel = apvts_.getRawParameterValue("fltDelay")) p.filterEnv.delaySec = pDel->load();
    p.filterEnv.attackSec  = apvts_.getRawParameterValue("fltAttack")->load();
    if (auto* pHold = apvts_.getRawParameterValue("fltHold")) p.filterEnv.holdSec = pHold->load();
    p.filterEnv.decaySec   = apvts_.getRawParameterValue("fltDecay")->load();
    p.filterEnv.sustainLvl = apvts_.getRawParameterValue("fltSustain")->load();
    p.filterEnv.releaseSec = apvts_.getRawParameterValue("fltRelease")->load();
    if (auto* pVel = apvts_.getRawParameterValue("fltVelocity")) p.filterEnv.velocitySens = pVel->load();

    if (auto* pDel = apvts_.getRawParameterValue("modDelay")) p.modEnv.delaySec = pDel->load();
    if (auto* pA = apvts_.getRawParameterValue("modAttack"))  p.modEnv.attackSec  = pA->load();
    if (auto* pHold = apvts_.getRawParameterValue("modHold")) p.modEnv.holdSec = pHold->load();
    if (auto* pD = apvts_.getRawParameterValue("modDecay"))   p.modEnv.decaySec   = pD->load();
    if (auto* pS = apvts_.getRawParameterValue("modSustain")) p.modEnv.sustainLvl = pS->load();
    if (auto* pR = apvts_.getRawParameterValue("modRelease")) p.modEnv.releaseSec = pR->load();
    if (auto* pVel = apvts_.getRawParameterValue("modVelocity")) p.modEnv.velocitySens = pVel->load();

    if (auto* pDel = apvts_.getRawParameterValue("auxDelay")) p.auxEnv.delaySec = pDel->load();
    if (auto* pA = apvts_.getRawParameterValue("auxAttack"))  p.auxEnv.attackSec  = pA->load();
    if (auto* pHold = apvts_.getRawParameterValue("auxHold")) p.auxEnv.holdSec = pHold->load();
    if (auto* pD = apvts_.getRawParameterValue("auxDecay"))   p.auxEnv.decaySec   = pD->load();
    if (auto* pS = apvts_.getRawParameterValue("auxSustain")) p.auxEnv.sustainLvl = pS->load();
    if (auto* pR = apvts_.getRawParameterValue("auxRelease")) p.auxEnv.releaseSec = pR->load();
    if (auto* pVel = apvts_.getRawParameterValue("auxVelocity")) p.auxEnv.velocitySens = pVel->load();

    p.lfo1RateHz = apvts_.getRawParameterValue("lfo1RateHz")->load();
    p.lfo1Depth  = apvts_.getRawParameterValue("lfo1Depth")->load();
    if (auto* pS = apvts_.getRawParameterValue("lfo1Shape"))
        p.lfo1Shape = static_cast<nova::dsp::LfoGenerator::Shape>(static_cast<int>(pS->load()));
    if (auto* pSync = apvts_.getRawParameterValue("lfo1Sync")) p.lfo1Sync = pSync->load() > 0.5f;
    if (auto* pDiv = apvts_.getRawParameterValue("lfo1SyncDiv")) p.lfo1SyncDiv = static_cast<int>(pDiv->load());

    p.lfo2RateHz = apvts_.getRawParameterValue("lfo2RateHz")->load();
    p.lfo2Depth  = apvts_.getRawParameterValue("lfo2Depth")->load();
    if (auto* pS = apvts_.getRawParameterValue("lfo2Shape"))
        p.lfo2Shape = static_cast<nova::dsp::LfoGenerator::Shape>(static_cast<int>(pS->load()));
    if (auto* pSync = apvts_.getRawParameterValue("lfo2Sync")) p.lfo2Sync = pSync->load() > 0.5f;
    if (auto* pDiv = apvts_.getRawParameterValue("lfo2SyncDiv")) p.lfo2SyncDiv = static_cast<int>(pDiv->load());

    if (auto* pR = apvts_.getRawParameterValue("lfo3RateHz")) p.lfo3RateHz = pR->load();
    if (auto* pD = apvts_.getRawParameterValue("lfo3Depth"))  p.lfo3Depth  = pD->load();
    if (auto* pS = apvts_.getRawParameterValue("lfo3Shape"))
        p.lfo3Shape = static_cast<nova::dsp::LfoGenerator::Shape>(static_cast<int>(pS->load()));
    if (auto* pSync = apvts_.getRawParameterValue("lfo3Sync")) p.lfo3Sync = pSync->load() > 0.5f;
    if (auto* pDiv = apvts_.getRawParameterValue("lfo3SyncDiv")) p.lfo3SyncDiv = static_cast<int>(pDiv->load());

    if (auto* pR = apvts_.getRawParameterValue("lfo4RateHz")) p.lfo4RateHz = pR->load();
    if (auto* pD = apvts_.getRawParameterValue("lfo4Depth"))  p.lfo4Depth  = pD->load();
    if (auto* pS = apvts_.getRawParameterValue("lfo4Shape"))
        p.lfo4Shape = static_cast<nova::dsp::LfoGenerator::Shape>(static_cast<int>(pS->load()));
    if (auto* pSync = apvts_.getRawParameterValue("lfo4Sync")) p.lfo4Sync = pSync->load() > 0.5f;
    if (auto* pDiv = apvts_.getRawParameterValue("lfo4SyncDiv")) p.lfo4SyncDiv = static_cast<int>(pDiv->load());

    p.macroWarmth  = apvts_.getRawParameterValue("macroWarmth")->load();
    p.macroMotion  = apvts_.getRawParameterValue("macroMotion")->load();
    p.macroShimmer = apvts_.getRawParameterValue("macroShimmer")->load();
    p.macroAir     = apvts_.getRawParameterValue("macroAir")->load();

    p.droneEnabled = apvts_.getRawParameterValue("droneEnabled")->load() > 0.5f;
    p.droneRootKey = static_cast<int>(apvts_.getRawParameterValue("droneRootKey")->load());
    p.droneLevel   = apvts_.getRawParameterValue("droneLevel")->load();
    if (auto* pDv = apvts_.getRawParameterValue("droneVoicing")) p.droneVoicing = static_cast<int>(pDv->load());
    if (auto* pDo = apvts_.getRawParameterValue("droneOctave"))  p.droneOctave = static_cast<int>(pDo->load());
    if (auto* pDt = apvts_.getRawParameterValue("droneTone"))    p.droneTone = pDt->load();
    if (auto* pDg = apvts_.getRawParameterValue("droneGlide"))   p.droneGlide = pDg->load();

    if (isHostBpmSynced_) {
        p.bpm = static_cast<float>(currentBpm_);
    } else if (auto* pBpm = apvts_.getRawParameterValue("bpm")) {
        p.bpm = pBpm->load();
        currentBpm_ = p.bpm;
        setBpm(currentBpm_, false);
    }

    p.arpEnabled = apvts_.getRawParameterValue("arpEnabled")->load() > 0.5f;
    p.arpMode    = static_cast<WorshipSynthEngine::ArpMode>(
        std::clamp(static_cast<int>(apvts_.getRawParameterValue("arpMode")->load()), 0, 5));
    if (auto* pAs = apvts_.getRawParameterValue("arpSync")) p.arpSync = pAs->load() > 0.5f;
    if (auto* pAsd = apvts_.getRawParameterValue("arpSyncDiv")) p.arpSyncDiv = static_cast<int>(pAsd->load());
    p.arpRateHz  = apvts_.getRawParameterValue("arpRateHz")->load();
    p.arpGate    = apvts_.getRawParameterValue("arpGate")->load();
    if (auto* pOct = apvts_.getRawParameterValue("arpOctaves")) p.arpOctaves = static_cast<int>(pOct->load()) + 1;
    if (auto* pLat = apvts_.getRawParameterValue("arpLatch")) p.arpLatch = pLat->load() > 0.5f;
    if (auto* pSwg = apvts_.getRawParameterValue("arpSwing")) p.arpSwing = pSwg->load();

    if (arpPatternLengthParam_ != nullptr) {
        const int lenIdx = static_cast<int>(arpPatternLengthParam_->load());
        p.arpPatternLength = (lenIdx == 1) ? 32 : ((lenIdx == 2) ? 64 : 16);
    } else if (auto* pLen = apvts_.getRawParameterValue("arpPatternLength")) {
        const int lenIdx = static_cast<int>(pLen->load());
        p.arpPatternLength = (lenIdx == 1) ? 32 : ((lenIdx == 2) ? 64 : 16);
    }

    for (int s = 0; s < 64; ++s) {
        if (arpStepGateParams_[static_cast<std::size_t>(s)] != nullptr) {
            p.arpStepGates[static_cast<std::size_t>(s)] = (arpStepGateParams_[static_cast<std::size_t>(s)]->load() > 0.5f);
        } else if (auto* pG = apvts_.getRawParameterValue("arpStepGate_" + juce::String(s + 1))) {
            p.arpStepGates[static_cast<std::size_t>(s)] = (pG->load() > 0.5f);
        }

        if (arpStepVelParams_[static_cast<std::size_t>(s)] != nullptr) {
            p.arpStepVelocities[static_cast<std::size_t>(s)] = arpStepVelParams_[static_cast<std::size_t>(s)]->load();
        } else if (auto* pV = apvts_.getRawParameterValue("arpStepVel_" + juce::String(s + 1))) {
            p.arpStepVelocities[static_cast<std::size_t>(s)] = pV->load();
        }
    }

    if (auto* pCE = apvts_.getRawParameterValue("chorusEnabled")) p.chorusEnabled = pCE->load() > 0.5f;
    if (auto* pCM = apvts_.getRawParameterValue("chorusMode")) p.chorusMode = static_cast<int>(pCM->load());
    if (auto* pMix = apvts_.getRawParameterValue("chorusMix")) p.chorusMix = pMix->load();
    if (auto* pRate = apvts_.getRawParameterValue("chorusRateHz")) p.chorusRateHz = pRate->load();
    if (auto* pDepth = apvts_.getRawParameterValue("chorusDepth")) p.chorusDepth = pDepth->load();

    p.delayEnabled  = apvts_.getRawParameterValue("delayEnabled")->load() > 0.5f;
    if (auto* pDs = apvts_.getRawParameterValue("delaySync")) p.delaySync = pDs->load() > 0.5f;
    if (auto* pDsd = apvts_.getRawParameterValue("delaySyncDiv")) p.delaySyncDiv = static_cast<int>(pDsd->load());
    p.delayMix      = apvts_.getRawParameterValue("delayMix")->load();
    p.delayTimeSec  = apvts_.getRawParameterValue("delayTimeSec")->load();
    p.delayFeedback = apvts_.getRawParameterValue("delayFeedback")->load();
    if (auto* pSw = apvts_.getRawParameterValue("delaySwing")) p.delaySwing = pSw->load();
    p.subLowestOnly = false; // per-slot lowestNoteOnly governs each sub slot independently!

    p.reverbDry     = apvts_.getRawParameterValue("reverbDry")->load();
    p.reverbMix     = apvts_.getRawParameterValue("reverbMix")->load();
    p.reverbDecay   = apvts_.getRawParameterValue("reverbDecay")->load();
    p.reverbShimmer = apvts_.getRawParameterValue("reverbShimmer")->load();
    if (auto* pPit = apvts_.getRawParameterValue("reverbShimmerPitch")) {
        p.reverbShimmerPitch = static_cast<int>(pPit->load());
    }
    p.reverbDucking = apvts_.getRawParameterValue("reverbDucking")->load();
    p.reverbFreeze  = apvts_.getRawParameterValue("reverbFreeze")->load() > 0.5f;
    p.masterGainDb  = apvts_.getRawParameterValue("masterGainDb")->load();
    if (masterTuningHzParam_ != nullptr) {
        const float rawHz = std::clamp(masterTuningHzParam_->load(std::memory_order_relaxed), 435.0f, 445.0f);
        p.masterTuningHz = std::round(rawHz * 10.0f) * 0.1f;
    } else if (auto* pTune = apvts_.getRawParameterValue("masterTuningHz")) {
        const float rawHz = std::clamp(pTune->load(std::memory_order_relaxed), 435.0f, 445.0f);
        p.masterTuningHz = std::round(rawHz * 10.0f) * 0.1f;
    }

    currentParams_ = p;
    synthEngine_.setParameters(p);
}

} // namespace nova::synth
