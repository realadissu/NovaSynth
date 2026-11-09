#pragma once

#include "nova_dsp/reverb/DualEngineReverb.h"
#include <array>
#include <cmath>
#include <cstdint>

namespace nova::dsp {

// 16 mutually prime base delay lengths in milliseconds (spanning 23.27ms to 127.91ms)
inline constexpr std::array<float, 16> kBaseFdnDelayMs = {
    23.27f, 27.53f, 31.91f, 36.73f,
    41.87f, 47.59f, 53.81f, 60.47f,
    67.61f, 75.29f, 83.53f, 92.09f,
    100.93f, 109.79f, 118.67f, 127.91f
};

// 16 mutually prime in-loop allpass diffuser lengths in milliseconds (for exponential modal density)
inline constexpr std::array<float, 16> kInLoopDiffuserMs = {
    3.11f, 3.67f, 4.19f, 4.79f,
    5.23f, 5.77f, 6.31f, 6.83f,
    7.19f, 7.61f, 8.09f, 8.53f,
    8.87f, 9.19f, 9.47f, 9.83f
};

// 16 mutually prime shimmer cloud distribution tap lengths in milliseconds
// Ensures all 16 FDN channels receive independent, decorrelated shimmer signals (zero Hadamard cancellation!)
inline constexpr std::array<float, 16> kShimmerCloudTapMs = {
    0.67f,  3.47f,  6.83f, 10.61f,
    14.89f, 19.33f, 24.11f, 29.09f,
    33.73f, 38.21f, 42.53f, 46.79f,
    50.39f, 53.87f, 56.53f, 58.97f
};

// Full-rank aperiodic 16-channel input excitation weights (excites all 16 Walsh-Hadamard eigenmodes)
inline constexpr std::array<float, 16> kInputExcitationSigns = {
     1.00f,  1.00f, -1.00f,  1.00f,
    -1.00f,  0.88f,  1.00f, -0.88f,
     0.92f, -1.00f, -0.92f, -1.00f,
     1.00f, -0.94f,  0.94f,  1.00f
};

// Decorrelated 16-channel stereo readout weights
inline constexpr std::array<float, 16> kReadoutSignsL = {
     1.00f,  0.28f, -1.00f, -0.28f,
     0.94f, -0.28f,  0.94f,  0.28f,
    -0.96f,  0.28f,  0.96f, -0.28f,
    -0.92f, -0.28f, -0.92f,  0.28f
};

inline constexpr std::array<float, 16> kReadoutSignsR = {
     0.28f,  1.00f, -0.28f, -1.00f,
    -0.28f,  0.94f,  0.28f,  0.94f,
     0.28f, -0.96f, -0.28f,  0.96f,
    -0.28f, -0.92f,  0.28f, -0.92f
};

// 4 mutually irrational LFO rate multipliers for zero-pitch-drift quadrature modulation quads
inline constexpr std::array<float, 4> kQuadRateMultipliers = { 0.73f, 0.91f, 1.13f, 1.37f };

// 4 prime input diffuser lengths in milliseconds
inline constexpr std::array<float, 4> kDiffuserDelayMsL = { 4.73f, 7.39f, 11.53f, 16.97f };
inline constexpr std::array<float, 4> kDiffuserDelayMsR = { 5.21f, 8.09f, 12.37f, 17.83f };

// 12 Early Reflection tap times in ms and gains
inline constexpr std::array<float, 12> kEarlyTapMsL = {
    3.1f, 5.8f, 9.2f, 13.7f, 18.4f, 23.9f, 29.5f, 36.1f, 43.8f, 51.2f, 60.4f, 71.0f
};
inline constexpr std::array<float, 12> kEarlyTapMsR = {
    3.7f, 6.4f, 10.1f, 14.9f, 19.8f, 25.1f, 31.2f, 38.0f, 45.3f, 53.7f, 62.9f, 73.5f
};
inline constexpr std::array<float, 12> kEarlyTapGains = {
    0.84f, -0.71f, 0.63f, 0.56f, -0.49f, 0.43f, -0.38f, 0.33f, 0.29f, -0.25f, 0.21f, -0.18f
};

[[nodiscard]] inline float flushDenormal(float x) noexcept {
    return (std::isfinite(x) && std::abs(x) >= 1e-20f) ? x : 0.0f;
}

[[nodiscard]] inline float getSpaceSizeScale(DualEngineReverb::SpaceType st) noexcept {
    switch (st) {
        case DualEngineReverb::SpaceType::StudioRoom:     return 0.58f;
        case DualEngineReverb::SpaceType::WarmChamber:    return 0.82f;
        case DualEngineReverb::SpaceType::ConcertHall:    return 1.05f;
        case DualEngineReverb::SpaceType::CathedralCloud: return 1.32f;
    }
    return 1.0f;
}

[[nodiscard]] inline float getSpaceErScale(DualEngineReverb::SpaceType st) noexcept {
    switch (st) {
        case DualEngineReverb::SpaceType::StudioRoom:     return 1.25f;
        case DualEngineReverb::SpaceType::WarmChamber:    return 1.05f;
        case DualEngineReverb::SpaceType::ConcertHall:    return 0.85f;
        case DualEngineReverb::SpaceType::CathedralCloud: return 0.55f;
    }
    return 1.0f;
}

[[nodiscard]] inline float softClipTank(float x) noexcept {
    const float ax = std::abs(x);
    if (ax <= 1.15f) {
        return x;
    }
    const float sign = (x >= 0.0f) ? 1.0f : -1.0f;
    const float ex = (ax - 1.15f) * 0.80f;
    const float ex2 = ex * ex;
    const float t = (ex <= 3.0f) ? (ex * (27.0f + ex2) / (27.0f + 9.0f * ex2)) : 1.0f;
    return sign * (1.15f + 0.75f * t);
}

[[nodiscard]] inline float nextRandomUnit(uint32_t& state) noexcept {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state & 0x00FFFFFFu) * (1.0f / 16777216.0f);
}

} // namespace nova::dsp

