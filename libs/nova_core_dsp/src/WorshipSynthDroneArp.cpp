#include "nova_dsp/synth/WorshipSynthEngine.h"
#include <cmath>
#include <algorithm>

namespace nova::dsp {

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
                if (v.active && !v.isDroneVoice && v.isArpVoice && v.heldByArp) {
                    v.heldByArp = false;
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
        for (auto& v : voices_) {
            if (v.active && !v.isDroneVoice && v.isArpVoice && v.heldByArp) {
                v.heldByArp = false;
                v.ampEnv.noteOff();
                v.filterEnv.noteOff();
                v.modEnv.noteOff();
                v.auxEnv.noteOff();
            }
        }
        currentArpNote_ = -1;
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

        // GATE CUTOFF: When phase passes gateThreshold, end the gate for the currently held arp note.
        // Voices release naturally according to their full envelope parameters, enabling lush overlap and wash!
        const double gateRatio = std::clamp(static_cast<double>(params_.arpGate), 0.05, 1.0);
        const double gateThreshold = stepSamples * gateRatio;

        if (prevPhase < gateThreshold && arpPhaseSamples_ >= gateThreshold) {
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

        // End gate on any voice still held (e.g. gateRatio == 1.0 or legato), leaving releasing notes to decay naturally!
        for (auto& v : voices_) {
            if (v.active && !v.isDroneVoice && v.isArpVoice && v.heldByArp) {
                v.heldByArp = false;
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


} // namespace nova::dsp
