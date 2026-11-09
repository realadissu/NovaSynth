#include "nova_dsp/synth/WorshipSynthEngine.h"
#include <cmath>
#include <algorithm>

namespace nova::dsp {

namespace {
inline float tubeSat(float x) noexcept {
    const float x2 = x * x;
    return (x * (27.0f + x2) / (27.0f + 9.0f * x2));
}

constexpr std::array<ModMatrix::Destination, 8> kSlotShapeDests{
    ModMatrix::Destination::Osc1Shape, ModMatrix::Destination::Osc2Shape,
    ModMatrix::Destination::Osc3Shape, ModMatrix::Destination::Osc4Shape,
    ModMatrix::Destination::Osc5Shape, ModMatrix::Destination::Osc6Shape,
    ModMatrix::Destination::Osc7Shape, ModMatrix::Destination::Osc8Shape
};
constexpr std::array<ModMatrix::Destination, 8> kSlotPitchDests{
    ModMatrix::Destination::Osc1Pitch, ModMatrix::Destination::Osc2Pitch,
    ModMatrix::Destination::Osc3Pitch, ModMatrix::Destination::Osc4Pitch,
    ModMatrix::Destination::Osc5Pitch, ModMatrix::Destination::Osc6Pitch,
    ModMatrix::Destination::Osc7Pitch, ModMatrix::Destination::Osc8Pitch
};
constexpr std::array<ModMatrix::Destination, 8> kSlotLevelDests{
    ModMatrix::Destination::Osc1Level, ModMatrix::Destination::Osc2Level,
    ModMatrix::Destination::Osc3Level, ModMatrix::Destination::Osc4Level,
    ModMatrix::Destination::Osc5Level, ModMatrix::Destination::Osc6Level,
    ModMatrix::Destination::Osc7Level, ModMatrix::Destination::Osc8Level
};
} // namespace

void WorshipSynthEngine::processBlock(float* outLeft, float* outRight, int numSamples) noexcept {
    if (outLeft == nullptr || outRight == nullptr || numSamples <= 0) return;

    bool anyVoiceActive = false;
    for (const auto& v : voices_) {
        if (v.active) { anyVoiceActive = true; break; }
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

    float lastLfo1 = 0.0f, lastLfo2 = 0.0f, lastLfo3 = 0.0f, lastLfo4 = 0.0f;
    float lastEnvVal = 0.0f, lastCutoffModOct = 0.0f, lastCutoff2ModOct = 0.0f;
    float lastResoMod = 0.0f, lastReso2Mod = 0.0f;

    modMatrix_.setSourceValue(ModMatrix::Source::ModWheel,     modWheel_);
    modMatrix_.setSourceValue(ModMatrix::Source::MacroWarmth,  params_.macroWarmth);
    modMatrix_.setSourceValue(ModMatrix::Source::MacroMotion,  params_.macroMotion);
    modMatrix_.setSourceValue(ModMatrix::Source::MacroShimmer, params_.macroShimmer);
    modMatrix_.setSourceValue(ModMatrix::Source::MacroAir,     params_.macroAir);

    int lowestKeyNote = 128;
    for (const auto& v : voices_) {
        if (v.active && !v.isDroneVoice && (v.heldByKey || v.sustainedByPedal || v.heldByArp)) {
            lowestKeyNote = std::min(lowestKeyNote, v.midiNote);
        }
    }
    if (lowestKeyNote == 128) {
        for (const auto& v : voices_) {
            if (v.active && !v.isDroneVoice) lowestKeyNote = std::min(lowestKeyNote, v.midiNote);
        }
    }

    int lowestHeldDroneNote = 128, lowestReleasingDroneNote = 128;
    for (const auto& v : voices_) {
        if (v.active && v.isDroneVoice) {
            if (v.heldByDrone) lowestHeldDroneNote = std::min(lowestHeldDroneNote, v.midiNote);
            else lowestReleasingDroneNote = std::min(lowestReleasingDroneNote, v.midiNote);
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
        int activeNonDroneVoices = 0, activeVoiceCount = 0;
        std::array<int, kMaxVoices> activeVoiceIndices{};
        for (int i = 0; i < kMaxVoices; ++i) {
            if (voices_[static_cast<std::size_t>(i)].active) {
                activeVoiceIndices[static_cast<std::size_t>(activeVoiceCount++)] = i;
                if (!voices_[static_cast<std::size_t>(i)].isDroneVoice) ++activeNonDroneVoices;
            }
        }
        const float targetPolyScale = 1.0f / std::sqrt(1.0f + 0.45f * static_cast<float>(std::max(0, activeNonDroneVoices - 1)));
        polyHeadroomSmoother_.setTargetValue(targetPolyScale);

        const float flt1Drive = params_.filterDrive, flt2Drive = params_.filter2Drive;
        const bool flt1DriveActive = (flt1Drive > 0.01f), flt2DriveActive = (flt2Drive > 0.01f);
        const float flt1DriveGain = 1.0f + flt1Drive * 2.2f, flt2DriveGain = 1.0f + flt2Drive * 2.2f;
        const float flt1DriveComp = flt1DriveActive ? (1.0f / std::sqrt(1.0f + flt1Drive * 1.4f)) : 1.0f;
        const float flt2DriveComp = flt2DriveActive ? (1.0f / std::sqrt(1.0f + flt2Drive * 1.4f)) : 1.0f;

        bool fltEnvAsAmp = false, modEnvAsAmp = false, auxEnvAsAmp = false;
        float fltBothWeight = 0.0f, flt1OnlyWeight = 0.0f;
        for (int i = 0; i < params_.activeOscCount; ++i) {
            const auto& slot = params_.oscSlots[static_cast<std::size_t>(i)];
            if (slot.ampEnvSource == 1) fltEnvAsAmp = true;
            if (slot.ampEnvSource == 2) modEnvAsAmp = true;
            if (slot.ampEnvSource == 3) auxEnvAsAmp = true;
            if (slot.enabled) {
                if (slot.filterRouting == 2) fltBothWeight += slot.level;
                else if (slot.filterRouting == 0) flt1OnlyWeight += slot.level;
            }
        }
        const float bothFraction = (fltBothWeight + flt1OnlyWeight > 1e-5f)
            ? (fltBothWeight / (fltBothWeight + flt1OnlyWeight)) : 0.0f;

        for (int s = 0; s < numSamples; ++s) {
            const float lfo1Val = lfo1_.processSample();
            const float lfo2Val = lfo2_.processSample();
            const float lfo3Val = lfo3_.processSample();
            const float lfo4Val = lfo4_.processSample();
            lastLfo1 = lfo1Val; lastLfo2 = lfo2Val; lastLfo3 = lfo3Val; lastLfo4 = lfo4Val;

            const float baseCutoff = cutoffSmoother_.getNextValue(), baseCutoff2 = cutoff2Smoother_.getNextValue();
            const float masterGain = masterGainSmoother_.getNextValue(), polyHeadroom = polyHeadroomSmoother_.getNextValue();

            if ((s & 7) == 0) {
                modMatrix_.setSourceValue(ModMatrix::Source::Lfo1, lfo1Val);
                modMatrix_.setSourceValue(ModMatrix::Source::Lfo2, lfo2Val);
                modMatrix_.setSourceValue(ModMatrix::Source::Lfo3, lfo3Val);
                modMatrix_.setSourceValue(ModMatrix::Source::Lfo4, lfo4Val);
                modMatrix_.evaluate();

                const float lfo1RMod = modMatrix_.getModulationOffset(ModMatrix::Destination::Lfo1Rate);
                if (std::abs(lfo1RMod) > 1e-4f) {
                    const float baseR1 = params_.lfo1Sync ? getLfoRateForDivision(params_.bpm, params_.lfo1SyncDiv) : params_.lfo1RateHz;
                    lfo1_.setParameters(params_.lfo1Shape, baseR1 * (0.6f + 1.2f * params_.macroMotion) * std::pow(2.0f, lfo1RMod * 3.0f),
                                        std::clamp(params_.lfo1Depth + params_.macroMotion * 0.25f, 0.0f, 1.0f));
                }
                const float lfo2RMod = modMatrix_.getModulationOffset(ModMatrix::Destination::Lfo2Rate);
                if (std::abs(lfo2RMod) > 1e-4f) {
                    const float baseR2 = params_.lfo2Sync ? getLfoRateForDivision(params_.bpm, params_.lfo2SyncDiv) : params_.lfo2RateHz;
                    lfo2_.setParameters(params_.lfo2Shape, baseR2 * std::pow(2.0f, lfo2RMod * 3.0f), params_.lfo2Depth);
                }
                const float lfo3RMod = modMatrix_.getModulationOffset(ModMatrix::Destination::Lfo3Rate);
                if (std::abs(lfo3RMod) > 1e-4f) {
                    const float baseR3 = params_.lfo3Sync ? getLfoRateForDivision(params_.bpm, params_.lfo3SyncDiv) : params_.lfo3RateHz;
                    lfo3_.setParameters(params_.lfo3Shape, baseR3 * std::pow(2.0f, lfo3RMod * 3.0f), params_.lfo3Depth);
                }
                const float lfo4RMod = modMatrix_.getModulationOffset(ModMatrix::Destination::Lfo4Rate);
                if (std::abs(lfo4RMod) > 1e-4f) {
                    const float baseR4 = params_.lfo4Sync ? getLfoRateForDivision(params_.bpm, params_.lfo4SyncDiv) : params_.lfo4RateHz;
                    lfo4_.setParameters(params_.lfo4Shape, baseR4 * std::pow(2.0f, lfo4RMod * 3.0f), params_.lfo4Depth);
                }
            }

            const float pitchMod = pitchBendSemitones_ + modMatrix_.getModulationOffset(ModMatrix::Destination::OscPitchAll) * 2.0f;
            const float cutoffModOctaves = modMatrix_.getModulationOffset(ModMatrix::Destination::FilterCutoff) * 3.6f;
            const float filterResoMod = modMatrix_.getModulationOffset(ModMatrix::Destination::FilterResonance) * 0.5f;
            const float vocalCarveMod = modMatrix_.getModulationOffset(ModMatrix::Destination::VocalCarve);
            lastCutoffModOct = cutoffModOctaves;
            lastCutoff2ModOct = modMatrix_.getModulationOffset(ModMatrix::Destination::Filter2Cutoff) * 3.6f;
            lastResoMod = filterResoMod;
            lastReso2Mod = modMatrix_.getModulationOffset(ModMatrix::Destination::Filter2Resonance) * 0.5f;

            float synthL = 0.0f, synthR = 0.0f;
            float droneL = 0.0f, droneR = 0.0f;

            for (int vi = 0; vi < activeVoiceCount; ++vi) {
                auto& v = voices_[static_cast<std::size_t>(activeVoiceIndices[static_cast<std::size_t>(vi)])];
                if (!v.active) continue;

                const float ampEnvVal = v.ampEnv.processSample();
                const float fltEnvVal = v.filterEnv.processSample();
                const float modEnvVal = v.modEnv.processSample();
                const float auxEnvVal = v.auxEnv.processSample();
                lastEnvVal = std::max(lastEnvVal, fltEnvVal);

                bool anyEnvActive = v.ampEnv.isActive();
                float maxEnv = ampEnvVal;
                if (fltEnvAsAmp) { anyEnvActive = anyEnvActive || v.filterEnv.isActive(); maxEnv = std::max(maxEnv, fltEnvVal); }
                if (modEnvAsAmp) { anyEnvActive = anyEnvActive || v.modEnv.isActive();    maxEnv = std::max(maxEnv, modEnvVal); }
                if (auxEnvAsAmp) { anyEnvActive = anyEnvActive || v.auxEnv.isActive();    maxEnv = std::max(maxEnv, auxEnvVal); }

                if (!anyEnvActive || (!v.heldByKey && !v.sustainedByPedal && !v.heldByDrone && !v.heldByArp && maxEnv < 1e-4f)) {
                    v.active = false; v.isDroneVoice = false; v.heldByDrone = false; v.isArpVoice = false; v.heldByArp = false;
                    continue;
                }

                const ModMatrix::VoiceSources vSources{ ampEnvVal, fltEnvVal, modEnvVal, auxEnvVal, v.velocity };
                const auto voiceOffsets = modMatrix_.evaluateVoice(vSources);

                float voiceFlt1L = 0.0f, voiceFlt1R = 0.0f;
                float voiceFlt2L = 0.0f, voiceFlt2R = 0.0f;
                float voiceBothL = 0.0f, voiceBothR = 0.0f;
                float voiceDirL  = 0.0f, voiceDirR  = 0.0f;

                const float oscNorm = v.oscNormSnapshot;
                for (int i = 0; i < v.activeOscCountSnapshot; ++i) {
                    const auto& slotParam = params_.oscSlots[static_cast<std::size_t>(i)];
                    const bool isSubSlot = (slotParam.sourceType == 1 || slotParam.engine == MultiEngineOscillator::EngineType::SubOscillator);
                    const bool isLowestOnly = slotParam.lowestNoteOnly || params_.subLowestOnly;
                    if (isSubSlot && isLowestOnly) {
                        if (v.isDroneVoice) {
                            const bool isLowestDrone = v.heldByDrone ? (v.midiNote == lowestHeldDroneNote) : (v.midiNote == lowestReleasingDroneNote);
                            if (!isLowestDrone) continue;
                        } else {
                            if (v.midiNote != lowestKeyNote) continue;
                        }
                    }

                    const auto sIdx = static_cast<std::size_t>(i);
                    const float sMod = (sIdx < 8) ? voiceOffsets.offsets[static_cast<std::size_t>(kSlotShapeDests[sIdx])] : 0.0f;
                    const float slotPitchMod = (sIdx < 8) ? voiceOffsets.offsets[static_cast<std::size_t>(kSlotPitchDests[sIdx])] : 0.0f;
                    const float slotLevelMod = (sIdx < 8) ? voiceOffsets.offsets[static_cast<std::size_t>(kSlotLevelDests[sIdx])] : 0.0f;

                    const float vPitch = pitchMod + slotPitchMod * 24.0f;
                    float oL = 0.0f, oR = 0.0f;
                    v.oscs[static_cast<std::size_t>(i)].processSample(v.baseFreqHz, vPitch, sMod, oL, oR, slotLevelMod);

                    const float effEnv = (slotParam.ampEnvSource == 1) ? fltEnvVal :
                                         ((slotParam.ampEnvSource == 2) ? modEnvVal :
                                         ((slotParam.ampEnvSource == 3) ? auxEnvVal : ampEnvVal));

                    oL *= (oscNorm * effEnv);
                    oR *= (oscNorm * effEnv);

                    const int route = params_.oscSlots[static_cast<std::size_t>(i)].filterRouting;
                    if (route == 0)      { voiceFlt1L += oL; voiceFlt1R += oR; }
                    else if (route == 1) { voiceFlt2L += oL; voiceFlt2R += oR; }
                    else if (route == 2) { voiceBothL += oL; voiceBothR += oR; }
                    else                 { voiceDirL  += oL; voiceDirR  += oR; }
                }

                float flt1InL = voiceFlt1L + voiceBothL, flt1InR = voiceFlt1R + voiceBothR;

                if (params_.filter1Enabled) {
                    const float voiceResoMod = voiceOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::FilterResonance)] * 0.5f;
                    const float voiceCutMod = voiceOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::FilterCutoff)] * 3.6f;
                    const float effReso = std::clamp(params_.filterResonance + voiceResoMod, 0.35f, 6.0f);
                    const bool isFourPoleLadder = (params_.filterType == 0 || params_.filterType == 2);

                    if ((s & 15) == 0) {
                        const float envOctaves = fltEnvVal * params_.filterEnvAmount * 2.1f;
                        const float voiceCutoff = std::clamp(baseCutoff * std::exp2(envOctaves + voiceCutMod), 40.0f, static_cast<float>(sampleRate_ * 0.46));
                        auto svfType = StateVariableFilter::Type::LowPass;
                        if (params_.filterType == 1) svfType = StateVariableFilter::Type::BandPass;
                        else if (params_.filterType == 2) svfType = StateVariableFilter::Type::HighPass;

                        if (isFourPoleLadder) {
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

                    if (flt1DriveActive) {
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

                const bool isParallel = (params_.filterRoutingMode == 1);
                const float flt1DirectL = isParallel ? flt1InL : (flt1InL * (1.0f - bothFraction));
                const float flt1DirectR = isParallel ? flt1InR : (flt1InR * (1.0f - bothFraction));
                float flt2InL = isParallel ? (voiceFlt2L + voiceBothL) : (voiceFlt2L + flt1InL * bothFraction);
                float flt2InR = isParallel ? (voiceFlt2R + voiceBothR) : (voiceFlt2R + flt1InR * bothFraction);

                if (params_.filter2Enabled) {
                    const float voiceResoMod2 = voiceOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::Filter2Resonance)] * 0.5f;
                    const float voiceCutMod2 = voiceOffsets.offsets[static_cast<std::size_t>(ModMatrix::Destination::Filter2Cutoff)] * 3.6f;
                    const float effReso2 = std::clamp(params_.filter2Resonance + voiceResoMod2, 0.35f, 6.0f);
                    if ((s & 15) == 0) {
                        const float envOctaves2 = fltEnvVal * params_.filter2EnvAmount * 2.1f;
                        const float voiceCutoff2 = std::clamp(baseCutoff2 * std::exp2(envOctaves2 + voiceCutMod2), 40.0f, static_cast<float>(sampleRate_ * 0.46));
                        auto svf2Type = StateVariableFilter::Type::LowPass;
                        float bellGainDb = 0.0f;
                        if (params_.filter2Type == 1) svf2Type = StateVariableFilter::Type::BandPass;
                        else if (params_.filter2Type == 2) svf2Type = StateVariableFilter::Type::HighPass;
                        else if (params_.filter2Type == 3) {
                            svf2Type = StateVariableFilter::Type::Bell;
                            bellGainDb = -12.0f * std::clamp(params_.vocalCarve + vocalCarveMod, 0.0f, 1.0f);
                        }
                        v.filter2L.setParameters(svf2Type, voiceCutoff2, effReso2, bellGainDb);
                        v.filter2R.setParameters(svf2Type, voiceCutoff2, effReso2, bellGainDb);
                        v.flt2ResComp = 1.0f / std::sqrt(1.0f + 0.30f * std::max(0.0f, effReso2 - 0.707f));
                    }

                    if (flt2DriveActive) {
                        flt2InL = tubeSat(flt2InL * flt2DriveGain) * flt2DriveComp;
                        flt2InR = tubeSat(flt2InR * flt2DriveGain) * flt2DriveComp;
                    }

                    flt2InL = v.filter2L.processSample(flt2InL) * v.flt2ResComp;
                    flt2InR = v.filter2R.processSample(flt2InR) * v.flt2ResComp;
                }

                const float velGain = v.isArpVoice ? (v.velocity * (0.15f + 0.85f * v.velocity)) : (0.40f + 0.60f * v.velocity);
                const float droneScale = v.isDroneVoice ? (params_.droneLevel * 0.70f) : polyHeadroom;
                const float gain = velGain * droneScale * 0.82f;

                const float vOutL = (flt1DirectL + flt2InL + voiceDirL) * gain;
                const float vOutR = (flt1DirectR + flt2InR + voiceDirR) * gain;

                if (v.isDroneVoice) { droneL += vOutL; droneR += vOutR; }
                else                { synthL += vOutL; synthR += vOutR; }
            }

            const float panSpreadMod = modMatrix_.getModulationOffset(ModMatrix::Destination::PanSpread);
            if (std::abs(panSpreadMod) > 1e-4f) {
                const float spread = std::clamp(1.0f + panSpreadMod * 1.5f, 0.0f, 2.5f);
                const float mid = 0.5f * (synthL + synthR), side = 0.5f * (synthL - synthR) * spread;
                synthL = mid + side;
                synthR = mid - side;
            }

            chorus_.process(synthL, synthR, synthL, synthR);

            droneToneSmoother_.setTargetValue(std::clamp(params_.droneTone, 0.0f, 1.0f));
            const float toneNorm = droneToneSmoother_.getNextValue();
            if ((s & 15) == 0) {
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

    telemetryLfo1_.store(lastLfo1, std::memory_order_relaxed);
    telemetryLfo2_.store(lastLfo2, std::memory_order_relaxed);
    telemetryLfo3_.store(lastLfo3, std::memory_order_relaxed);
    telemetryLfo4_.store(lastLfo4, std::memory_order_relaxed);
    telemetryFilterCutoffMod_.store(lastCutoffModOct, std::memory_order_relaxed);
    telemetryFilter2CutoffMod_.store(lastCutoff2ModOct, std::memory_order_relaxed);
    telemetryFilterResoMod_.store(lastResoMod, std::memory_order_relaxed);
    telemetryFilter2ResoMod_.store(lastReso2Mod, std::memory_order_relaxed);
    telemetryLastEnvVal_.store(lastEnvVal, std::memory_order_relaxed);

    processMasterEffectsChain(outLeft, outRight, numSamples);
}

} // namespace nova::dsp

