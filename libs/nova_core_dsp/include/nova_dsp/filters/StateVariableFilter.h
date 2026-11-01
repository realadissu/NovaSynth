#pragma once

#include <cmath>
#include <algorithm>
#include <numbers>

namespace nova::dsp {

/**
 * @brief Zero-Delay Feedback (TPT) State Variable Filter (Cytomic / Andrew Simper topology).
 * Realtime-safe, unconditionally stable under fast modulation.
 */
class StateVariableFilter {
public:
    enum class Type {
        LowPass,
        HighPass,
        BandPass,
        Notch,
        Bell,
        HighShelf
    };

    void prepare(double sampleRate) noexcept {
        sampleRate_ = std::max(1.0, sampleRate);
        reset();
        updateCoefficients();
    }

    void reset() noexcept {
        ic1eq_ = 0.0f;
        ic2eq_ = 0.0f;
    }

    void setParameters(Type type, float cutoffHz, float q = 0.7071f, float gainDb = 0.0f) noexcept {
        type_ = type;
        cutoffHz_ = std::clamp(cutoffHz, 15.0f, static_cast<float>(sampleRate_ * 0.48));
        q_ = std::clamp(q, 0.1f, 18.0f);
        gainDb_ = std::clamp(gainDb, -24.0f, 24.0f);
        updateCoefficients();
    }

    [[nodiscard]] float processSample(float x) noexcept {
        const float v3 = x - ic2eq_;
        const float v1 = a1_ * ic1eq_ + a2_ * v3;
        const float v2 = ic2eq_ + a2_ * ic1eq_ + a3_ * v3;
        ic1eq_ = 2.0f * v1 - ic1eq_;
        ic2eq_ = 2.0f * v2 - ic2eq_;

        return m0_ * x + m1_ * v1 + m2_ * v2;
    }

    [[nodiscard]] float getMagnitudeAtFrequency(float freqHz) const noexcept {
        const float w = 2.0f * std::numbers::pi_v<float> * freqHz / static_cast<float>(sampleRate_);
        const float cosW = std::cos(w);
        const float sinW = std::sin(w);
        // Approximate transfer magnitude for UI curve plotting
        const float ratio = freqHz / std::max(1.0f, cutoffHz_);
        switch (type_) {
            case Type::LowPass:
                return 1.0f / std::sqrt(1.0f + std::pow(ratio, 4.0f) + (1.0f / (q_ * q_) - 2.0f) * ratio * ratio);
            case Type::HighPass:
                return (ratio * ratio) / std::sqrt(1.0f + std::pow(ratio, 4.0f) + (1.0f / (q_ * q_) - 2.0f) * ratio * ratio);
            case Type::BandPass:
                return (ratio / q_) / std::sqrt((1.0f - ratio * ratio) * (1.0f - ratio * ratio) + (ratio / q_) * (ratio / q_));
            case Type::Notch:
                return std::abs(1.0f - ratio * ratio) / std::sqrt((1.0f - ratio * ratio) * (1.0f - ratio * ratio) + (ratio / q_) * (ratio / q_));
            case Type::Bell: {
                const float A = std::pow(10.0f, gainDb_ / 20.0f);
                const float bw = (ratio - 1.0f / std::max(0.001f, ratio)) * q_;
                const float weight = 1.0f / (1.0f + bw * bw);
                return 1.0f + (A - 1.0f) * weight;
            }
            case Type::HighShelf: {
                (void)cosW;
                (void)sinW;
                const float A = std::pow(10.0f, gainDb_ / 20.0f);
                const float weight = (ratio * ratio) / (1.0f + ratio * ratio);
                return 1.0f + (A - 1.0f) * weight;
            }
        }
        return 1.0f;
    }

private:
    void updateCoefficients() noexcept {
        const float pi = std::numbers::pi_v<float>;
        const float gNorm = std::tan(pi * cutoffHz_ / static_cast<float>(sampleRate_));

        switch (type_) {
            case Type::LowPass: {
                const float g = gNorm;
                const float k = 1.0f / q_;
                a1_ = 1.0f / (1.0f + g * (g + k));
                a2_ = g * a1_;
                a3_ = g * a2_;
                m0_ = 0.0f;
                m1_ = 0.0f;
                m2_ = 1.0f;
                break;
            }
            case Type::HighPass: {
                const float g = gNorm;
                const float k = 1.0f / q_;
                a1_ = 1.0f / (1.0f + g * (g + k));
                a2_ = g * a1_;
                a3_ = g * a2_;
                m0_ = 1.0f;
                m1_ = -k;
                m2_ = -1.0f;
                break;
            }
            case Type::BandPass: {
                const float g = gNorm;
                const float k = 1.0f / q_;
                a1_ = 1.0f / (1.0f + g * (g + k));
                a2_ = g * a1_;
                a3_ = g * a2_;
                m0_ = 0.0f;
                m1_ = 1.0f;
                m2_ = 0.0f;
                break;
            }
            case Type::Notch: {
                const float g = gNorm;
                const float k = 1.0f / q_;
                a1_ = 1.0f / (1.0f + g * (g + k));
                a2_ = g * a1_;
                a3_ = g * a2_;
                m0_ = 1.0f;
                m1_ = -k;
                m2_ = 0.0f;
                break;
            }
            case Type::Bell: {
                const float A = std::pow(10.0f, gainDb_ / 40.0f);
                const float g = gNorm;
                const float k = 1.0f / (q_ * A);
                a1_ = 1.0f / (1.0f + g * (g + k));
                a2_ = g * a1_;
                a3_ = g * a2_;
                m0_ = 1.0f;
                m1_ = k * (A * A - 1.0f);
                m2_ = 0.0f;
                break;
            }
            case Type::HighShelf: {
                const float A = std::pow(10.0f, gainDb_ / 40.0f);
                const float g = gNorm * std::sqrt(A);
                const float k = 1.0f / q_;
                a1_ = 1.0f / (1.0f + g * (g + k));
                a2_ = g * a1_;
                a3_ = g * a2_;
                m0_ = A * A;
                m1_ = k * (1.0f - A) * A;
                m2_ = 1.0f - A * A;
                break;
            }
        }
    }

    double sampleRate_ = 48000.0;
    Type type_ = Type::LowPass;
    float cutoffHz_ = 1000.0f;
    float q_ = 0.7071f;
    float gainDb_ = 0.0f;

    float a1_ = 0.0f, a2_ = 0.0f, a3_ = 0.0f;
    float m0_ = 0.0f, m1_ = 0.0f, m2_ = 1.0f;
    float ic1eq_ = 0.0f, ic2eq_ = 0.0f;
};

} // namespace nova::dsp
