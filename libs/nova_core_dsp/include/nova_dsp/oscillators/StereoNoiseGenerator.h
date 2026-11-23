#pragma once

#include "nova_dsp/filters/StateVariableFilter.h"
#include <cstdint>

namespace nova::dsp {

/**
 * @brief Uncorrelated stereo noise & transient air generator for MultiEngineOscillator::NoiseTransient.
 * Uses independent L/R PRNGs and filter states for wide stereo breath without L=R mono collapse.
 */
class StereoNoiseGenerator {
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void trigger(uint32_t seedOffset) noexcept;
    void processSample(int modelIndex, float shape, float duration, float invSr, float& sigL, float& sigR) noexcept;

private:
    double sampleRate_ = 48000.0;
    uint32_t rngL_ = 0x9E3779B9u;
    uint32_t rngR_ = 0x7F4A7C15u;
    float pinkL_ = 0.0f;
    float pinkR_ = 0.0f;
    float hpL_ = 0.0f;
    float hpR_ = 0.0f;
    float clickPhase_ = 0.0f;
    StateVariableFilter airHpL_{};
    StateVariableFilter airHpR_{};
    StateVariableFilter airLpL_{};
    StateVariableFilter airLpR_{};
    float lastAirShape_ = -999.0f;
    float lastPinkShape_ = -999.0f;
    float pinkCoeff_ = 0.45f;
};

} // namespace nova::dsp
