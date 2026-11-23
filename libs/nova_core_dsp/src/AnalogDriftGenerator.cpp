#include "nova_dsp/modulation/AnalogDriftGenerator.h"
#include <algorithm>
#include <cmath>

namespace nova::dsp {

namespace {

[[nodiscard]] inline uint32_t xorshift32(uint32_t& state) noexcept {
    if (state == 0u) state = 0x9E3779B9u;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

[[nodiscard]] inline float nextBipolar(uint32_t& state) noexcept {
    return (static_cast<float>(xorshift32(state) & 0xFFFFu) / 32767.5f) - 1.0f;
}

} // namespace

void AnalogDriftGenerator::prepare(double sampleRate) noexcept {
    sampleRate_ = std::max(8000.0, sampleRate);
}

void AnalogDriftGenerator::reset(uint32_t seed) noexcept {
    rngPitch_ = (seed ^ 0x9E3779B9u) | 1u;
    rngCutoff_ = ((seed * 2246822519u) ^ 0x7F4A7C15u) | 1u;
    pitchWalk_ = 0.0f;
    pitchSmooth_ = 0.0f;
    cutoffWalk_ = 0.0f;
    cutoffSmooth_ = 0.0f;
}

void AnalogDriftGenerator::trigger(uint32_t seed, bool coldStart) noexcept {
    rngPitch_ = (seed ^ 0x9E3779B9u) | 1u;
    rngCutoff_ = ((seed * 2246822519u) ^ 0x7F4A7C15u) | 1u;

    const float initP = nextBipolar(rngPitch_) * 0.65f;
    const float initC = nextBipolar(rngCutoff_) * 0.65f;
    pitchWalk_ = initP;
    cutoffWalk_ = initC;

    if (coldStart) {
        pitchSmooth_ = initP * 0.30f;
        cutoffSmooth_ = initC * 0.30f;
    }
}

void AnalogDriftGenerator::stepControlRate(int samplesElapsed) noexcept {
    if (!std::isfinite(pitchWalk_) || !std::isfinite(pitchSmooth_)) {
        pitchWalk_ = 0.0f;
        pitchSmooth_ = 0.0f;
    }
    if (!std::isfinite(cutoffWalk_) || !std::isfinite(cutoffSmooth_)) {
        cutoffWalk_ = 0.0f;
        cutoffSmooth_ = 0.0f;
    }

    const float dt = std::clamp(
        static_cast<float>(samplesElapsed) / static_cast<float>(std::max(8000.0, sampleRate_)),
        1e-5f, 0.05f);
    const float sqrtDt = std::sqrt(dt);

    const float zPitch = 1.7320508f * nextBipolar(rngPitch_);
    const float zCutoff = 1.7320508f * nextBipolar(rngCutoff_);

    pitchWalk_ = std::clamp(
        pitchWalk_ - 0.55f * pitchWalk_ * dt + 1.05f * sqrtDt * zPitch,
        -1.5f, 1.5f);
    pitchSmooth_ = std::clamp(
        pitchSmooth_ + (1.0f - std::exp(-dt / 0.30f)) * (pitchWalk_ - pitchSmooth_),
        -1.0f, 1.0f);

    cutoffWalk_ = std::clamp(
        cutoffWalk_ - 0.42f * cutoffWalk_ * dt + 0.98f * sqrtDt * zCutoff,
        -1.5f, 1.5f);
    cutoffSmooth_ = std::clamp(
        cutoffSmooth_ + (1.0f - std::exp(-dt / 0.38f)) * (cutoffWalk_ - cutoffSmooth_),
        -1.0f, 1.0f);
}

float AnalogDriftGenerator::getPitchDriftCents(float driftAmount) const noexcept {
    if (driftAmount <= 1e-5f) return 0.0f;
    const float amt = std::clamp(driftAmount, 0.0f, 1.35f);
    return std::clamp(pitchSmooth_ * 4.8f * amt, -6.0f, 6.0f);
}

float AnalogDriftGenerator::getCutoffDriftFactor(float driftAmount) const noexcept {
    if (driftAmount <= 1e-5f) return 1.0f;
    const float amt = std::clamp(driftAmount, 0.0f, 1.35f);
    const float rel = std::clamp(cutoffSmooth_ * 0.048f * amt, -0.06f, 0.06f);
    return 1.0f + rel;
}

} // namespace nova::dsp
