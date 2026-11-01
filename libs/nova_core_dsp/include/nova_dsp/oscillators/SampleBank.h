#pragma once

#include <cstdint>
#include <cstddef>

namespace nova::dsp {

/**
 * @brief Represents one octave/pitch zone for multi-sampled instrument layers.
 */
struct SampleZone {
    const int16_t* pcmL = nullptr;
    const int16_t* pcmR = nullptr;
    int numSamples = 0;
    float rootFrequencyHz = 261.63f;
};

/**
 * @brief High-precision, true-stereo sample instrument with multi-octave zones.
 */
struct SampleSound {
    const SampleZone* zones = nullptr;
    int numZones = 1;
    double sampleRate = 44100.0;
    const char* name = "";
    bool isLooping = false;
};

class SampleBank {
public:
    static constexpr int kNumSamples = 8;

    [[nodiscard]] static const SampleSound& getSample(int index) noexcept;
    [[nodiscard]] static constexpr int getSampleCount() noexcept { return kNumSamples; }
    [[nodiscard]] static const char* getSampleName(int index) noexcept;
};

} // namespace nova::dsp
