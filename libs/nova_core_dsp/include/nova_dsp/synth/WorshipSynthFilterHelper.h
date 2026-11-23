#pragma once

#include "nova_dsp/filters/StateVariableFilter.h"

namespace nova::dsp::synth_filter {

[[nodiscard]] float computeKeyTrackFactor(float baseFreqHz, float keyTrack) noexcept;

struct Filter1Context {
    bool enabled = true;
    int filterType = 0; // 0: LP24, 1: BP12, 2: HP24, 3: LP12 (Smooth)
    float baseCutoff = 2800.0f;
    float resonance = 0.75f;
    float envAmount = 0.35f;
    float keyTrack = 0.40f;
    float drive = 0.15f;
    float driveGain = 1.0f;
    float driveComp = 1.0f;
    float baseFreqHz = 261.63f;
    float fltEnvVal = 0.0f;
    float voiceCutMod = 0.0f;
    float voiceResoMod = 0.0f;
    float cutoffDriftFactor = 1.0f;
    double sampleRate = 48000.0;
    bool updateCoeffs = false;
};

struct Filter2Context {
    bool enabled = true;
    int filter2Type = 0; // 0: LP12, 1: BP12, 2: HP12, 3: Vocal Carve Bell
    float baseCutoff = 2800.0f;
    float baseCutoff2 = 3600.0f;
    float filter2CutoffHzParam = 3600.0f;
    float resonance = 0.70f;
    float envAmount = 0.0f;
    float keyTrack = 0.0f;
    float drive = 0.0f;
    float driveGain = 1.0f;
    float driveComp = 1.0f;
    float vocalCarve = 0.25f;
    float vocalCarveMod = 0.0f;
    float macroWarmth = 0.50f;
    float modWheel = 0.0f;
    float baseFreqHz = 261.63f;
    float fltEnvVal = 0.0f;
    float voiceCutMod = 0.0f;
    float voiceResoMod = 0.0f;
    float cutoffDriftFactor = 1.0f;
    double sampleRate = 48000.0;
    bool updateCoeffs = false;
};

void processFilter1(const Filter1Context& ctx,
                    StateVariableFilter& fL,
                    StateVariableFilter& fR,
                    StateVariableFilter& s2L,
                    StateVariableFilter& s2R,
                    float& resComp,
                    float& inL,
                    float& inR) noexcept;

void processFilter2(const Filter2Context& ctx,
                    StateVariableFilter& f2L,
                    StateVariableFilter& f2R,
                    float& resComp,
                    float& inL,
                    float& inR) noexcept;

} // namespace nova::dsp::synth_filter
