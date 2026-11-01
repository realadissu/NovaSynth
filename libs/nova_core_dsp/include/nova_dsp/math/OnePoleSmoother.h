#pragma once

#include <cmath>
#include <algorithm>

namespace nova::dsp {

/**
 * @brief Realtime-safe exponential one-pole parameter smoother.
 * Prevents zipper noise when parameters change on the audio thread.
 */
class OnePoleSmoother {
public:
    OnePoleSmoother() = default;

    void prepare(double sampleRate, float smoothingTimeMs = 15.0f) noexcept {
        sampleRate_ = std::max(1.0, sampleRate);
        setTimeMs(smoothingTimeMs);
        current_ = target_;
    }

    void setTimeMs(float smoothingTimeMs) noexcept {
        if (smoothingTimeMs <= 0.01f) {
            coeff_ = 1.0f;
            return;
        }
        const double tau = static_cast<double>(smoothingTimeMs) * 0.001;
        coeff_ = static_cast<float>(1.0 - std::exp(-1.0 / (tau * sampleRate_)));
    }

    void reset(float initialValue) noexcept {
        current_ = initialValue;
        target_ = initialValue;
    }

    void setTargetValue(float newTarget) noexcept {
        target_ = newTarget;
    }

    [[nodiscard]] float getNextValue() noexcept {
        current_ += coeff_ * (target_ - current_);
        return current_;
    }

    [[nodiscard]] float getCurrentValue() const noexcept {
        return current_;
    }

    [[nodiscard]] float getTargetValue() const noexcept {
        return target_;
    }

private:
    double sampleRate_ = 48000.0;
    float coeff_ = 1.0f;
    float current_ = 0.0f;
    float target_ = 0.0f;
};

} // namespace nova::dsp
