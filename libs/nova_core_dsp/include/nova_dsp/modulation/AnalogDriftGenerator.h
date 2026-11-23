#pragma once

#include <cstdint>

namespace nova::dsp {

/**
 * @brief Realtime-safe per-voice analog drift generator (control-rate OU random walk).
 * Provides independent, smoothly filtered pitch and filter-cutoff drift trajectories per voice.
 */
class AnalogDriftGenerator {
public:
    void prepare(double sampleRate) noexcept;
    void reset(uint32_t seed) noexcept;
    void trigger(uint32_t seed, bool coldStart) noexcept;
    void stepControlRate(int samplesElapsed = 16) noexcept;

    [[nodiscard]] float getPitchDriftCents(float driftAmount) const noexcept;
    [[nodiscard]] float getCutoffDriftFactor(float driftAmount) const noexcept;

    [[nodiscard]] float getRawPitchState() const noexcept { return pitchSmooth_; }
    [[nodiscard]] float getRawCutoffState() const noexcept { return cutoffSmooth_; }

private:
    double sampleRate_ = 48000.0;
    uint32_t rngPitch_ = 0xA341316Cu;
    uint32_t rngCutoff_ = 0xC8013EA4u;
    float pitchWalk_ = 0.0f;
    float pitchSmooth_ = 0.0f;
    float cutoffWalk_ = 0.0f;
    float cutoffSmooth_ = 0.0f;
};

} // namespace nova::dsp
