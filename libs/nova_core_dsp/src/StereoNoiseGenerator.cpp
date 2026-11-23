#include "nova_dsp/oscillators/StereoNoiseGenerator.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace nova::dsp {

namespace {

[[nodiscard]] inline float nextNoiseBipolar(uint32_t& state) noexcept {
    if (state == 0u) state = 0x9E3779B9u;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return (static_cast<float>(state & 0xFFFFu) / 32767.5f) - 1.0f;
}

} // namespace

void StereoNoiseGenerator::prepare(double sampleRate) noexcept {
    sampleRate_ = std::max(8000.0, sampleRate);
    airHpL_.prepare(sampleRate_);
    airHpR_.prepare(sampleRate_);
    airLpL_.prepare(sampleRate_);
    airLpR_.prepare(sampleRate_);
    reset();
}

void StereoNoiseGenerator::reset() noexcept {
    rngL_ = 0x9E3779B9u;
    rngR_ = 0x7F4A7C15u;
    pinkL_ = 0.0f;
    pinkR_ = 0.0f;
    hpL_ = 0.0f;
    hpR_ = 0.0f;
    clickPhase_ = 0.0f;
    airHpL_.reset();
    airHpR_.reset();
    airLpL_.reset();
    airLpR_.reset();
    lastAirShape_ = -999.0f;
    lastPinkShape_ = -999.0f;
    pinkCoeff_ = 0.45f;
}

void StereoNoiseGenerator::trigger(uint32_t seedOffset) noexcept {
    rngL_ = (0x9E3779B9u ^ (seedOffset * 747796405u + 2891336453u)) | 1u;
    rngR_ = (0x7F4A7C15u ^ (seedOffset * 2246822519u + 3266489917u)) | 1u;
    pinkL_ = 0.0f;
    pinkR_ = 0.0f;
    hpL_ = 0.0f;
    hpR_ = 0.0f;
    clickPhase_ = 0.0f;
    airHpL_.reset();
    airHpR_.reset();
    airLpL_.reset();
    airLpR_.reset();
    lastAirShape_ = -999.0f;
}

void StereoNoiseGenerator::processSample(int modelIndex,
                                         float shape,
                                         float duration,
                                         float invSr,
                                         float& sigL,
                                         float& sigR) noexcept {
    const float sr = static_cast<float>(sampleRate_);
    const float whiteL = nextNoiseBipolar(rngL_);
    const float whiteR = nextNoiseBipolar(rngR_);

    if (std::abs(shape - lastPinkShape_) > 0.002f) {
        lastPinkShape_ = shape;
        const float pinkHz = std::min(1200.0f + shape * 7500.0f, sr * 0.42f);
        pinkCoeff_ = std::clamp(1.0f - std::exp(-2.0f * std::numbers::pi_v<float> * pinkHz * invSr), 0.05f, 0.92f);
    }

    switch (modelIndex) {
        case 0: { // Analog Pink Noise
            pinkL_ += pinkCoeff_ * (whiteL - pinkL_);
            pinkR_ += pinkCoeff_ * (whiteR - pinkR_);
            sigL = pinkL_ * 0.65f;
            sigR = pinkR_ * 0.65f;
            break;
        }
        case 1: { // Velvet Vinyl Dust
            const float thresh = 0.90f - shape * 0.12f;
            sigL = (std::abs(whiteL) > thresh) ? (whiteL * 1.6f) : (whiteL * 0.08f);
            sigR = (std::abs(whiteR) > thresh) ? (whiteR * 1.6f) : (whiteR * 0.08f);
            break;
        }
        case 2: { // White Shimmer Air (Tuned silky 4..14 kHz stereo breath)
            if (std::abs(shape - lastAirShape_) > 0.003f) {
                lastAirShape_ = shape;
                const float hpHz = std::min(3800.0f + shape * 4000.0f, sr * 0.36f);
                const float lpHz = std::clamp(10500.0f + shape * 4000.0f, hpHz * 1.15f, sr * 0.45f);
                airHpL_.setParameters(StateVariableFilter::Type::HighPass, hpHz, 0.65f);
                airHpR_.setParameters(StateVariableFilter::Type::HighPass, hpHz, 0.65f);
                airLpL_.setParameters(StateVariableFilter::Type::LowPass, lpHz, 0.65f);
                airLpR_.setParameters(StateVariableFilter::Type::LowPass, lpHz, 0.65f);
            }
            sigL = airLpL_.processSample(airHpL_.processSample(whiteL)) * 0.72f;
            sigR = airLpR_.processSample(airHpR_.processSample(whiteR)) * 0.72f;
            break;
        }
        case 3: { // Attack Key Click
            constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
            const float clickDurSec = std::clamp(
                duration > 0.001f ? duration : (0.035f + 0.065f * shape), 0.015f, 0.150f);
            const float clickProgress = clickPhase_;
            if (clickPhase_ < 1.0f) {
                clickPhase_ += invSr / clickDurSec;
            }
            const float clickEnv = (clickProgress < 1.0f)
                ? std::exp(-clickProgress * (4.5f + shape * 3.5f))
                : 0.0f;
            hpL_ += 0.55f * (whiteL - hpL_);
            hpR_ += 0.55f * (whiteR - hpR_);
            pinkL_ += 0.16f * (whiteL - pinkL_);
            pinkR_ += 0.16f * (whiteR - pinkR_);
            const float clickSine = std::sin(twoPi * std::min(1.0f, clickProgress) * 18.0f)
                                  * (1.0f - std::min(1.0f, clickProgress));
            const float sustain = 0.045f * (0.25f + 0.75f * shape);
            sigL = std::clamp(((whiteL - hpL_) * 1.6f + pinkL_ * 2.2f + clickSine * 0.45f) * clickEnv * 1.45f + pinkL_ * sustain, -2.6f, 2.6f);
            sigR = std::clamp(((whiteR - hpR_) * 1.6f + pinkR_ * 2.2f + clickSine * 0.45f) * clickEnv * 1.45f + pinkR_ * sustain, -2.6f, 2.6f);
            break;
        }
        default: {
            pinkL_ += pinkCoeff_ * (whiteL - pinkL_);
            pinkR_ += pinkCoeff_ * (whiteR - pinkR_);
            const float airL = whiteL - pinkL_;
            const float airR = whiteR - pinkR_;
            sigL = (pinkL_ * (1.0f - shape * 0.5f) + airL * shape * 0.8f) * 0.55f;
            sigR = (pinkR_ * (1.0f - shape * 0.5f) + airR * shape * 0.8f) * 0.55f;
            break;
        }
    }
}

} // namespace nova::dsp
