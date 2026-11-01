#pragma once

#include <cmath>
#include <algorithm>
#include <numbers>
#include <cstdint>

namespace nova::dsp {

/**
 * @brief Multi-shape Low Frequency Oscillator with Smooth Random Drift mode for organic Worship Pads.
 */
class LfoGenerator {
public:
    enum class Shape : int {
        Sine = 0,
        Triangle = 1,
        SawUp = 2,
        SmoothDrift = 3,
        PulseGate = 4
    };

    static const char* getShapeName(Shape s) noexcept {
        switch (s) {
            case Shape::Sine:        return "Sine";
            case Shape::Triangle:    return "Triangle";
            case Shape::SawUp:       return "Saw Up";
            case Shape::SmoothDrift: return "Smooth Drift";
            case Shape::PulseGate:   return "Pulse Gate";
            default:                 return "Sine";
        }
    }

    void prepare(double sampleRate) noexcept {
        sampleRate_ = std::max(1000.0, sampleRate);
    }

    void reset(float initialPhase = 0.0f) noexcept {
        phase_ = initialPhase - std::floor(initialPhase);
    }

    void setParameters(Shape shape, float rateHz, float depth = 1.0f) noexcept {
        shape_ = shape;
        rateHz_ = std::clamp(rateHz, 0.01f, 30.0f);
        depth_ = std::clamp(depth, 0.0f, 1.0f);
    }

    [[nodiscard]] Shape getShape() const noexcept { return shape_; }
    [[nodiscard]] float getRateHz() const noexcept { return rateHz_; }
    [[nodiscard]] float getDepth() const noexcept { return depth_; }
    [[nodiscard]] float getPhase() const noexcept { return phase_; }

    [[nodiscard]] float processSample() noexcept {
        const float inc = rateHz_ / static_cast<float>(sampleRate_);
        phase_ += inc;
        if (phase_ >= 1.0f) {
            phase_ -= 1.0f;
            prevRandom_ = targetRandom_;
            rngState_ = rngState_ * 1664525u + 1013904223u;
            targetRandom_ = (static_cast<float>(rngState_ & 0xFFFFu) / 32767.5f) - 1.0f;
        }

        float raw = 0.0f;
        switch (shape_) {
            case Shape::Sine:
                raw = std::sin(2.0f * std::numbers::pi_v<float> * phase_);
                break;
            case Shape::Triangle:
                raw = 1.0f - 4.0f * std::abs(phase_ - 0.5f);
                break;
            case Shape::SawUp:
                raw = 2.0f * phase_ - 1.0f;
                break;
            case Shape::SmoothDrift: {
                // Smooth cosine interpolation between random targets
                const float mu = (1.0f - std::cos(phase_ * std::numbers::pi_v<float>)) * 0.5f;
                raw = prevRandom_ + mu * (targetRandom_ - prevRandom_);
                break;
            }
            case Shape::PulseGate:
                raw = (phase_ < 0.5f) ? 1.0f : -1.0f;
                break;
        }

        currentValue_ = raw * depth_;
        return currentValue_;
    }

    [[nodiscard]] float getCurrentValue() const noexcept { return currentValue_; }

private:
    double sampleRate_ = 48000.0;
    Shape shape_ = Shape::Sine;
    float rateHz_ = 0.4f;
    float depth_ = 0.5f;
    float phase_ = 0.0f;
    float currentValue_ = 0.0f;

    uint32_t rngState_ = 0x12345678u;
    float prevRandom_ = 0.0f;
    float targetRandom_ = 0.35f;
};

} // namespace nova::dsp
