#pragma once

#include <cmath>
#include <algorithm>
#include <vector>

namespace nova::dsp {

/**
 * @brief Realtime-safe exponential/curved ADSR envelope generator.
 * Supports smooth retriggering from current level (zero clicks on voice stealing or fast legato).
 */
class AdsrEnvelope {
public:
    enum class Stage {
        Idle,
        Delay,
        Attack,
        Hold,
        Decay,
        Sustain,
        Release
    };

    struct Parameters {
        float delaySec     = 0.0f;  // 0.0 .. 2.0 s (Pre-Delay)
        float attackSec    = 0.35f; // 0.001 .. 10.0 s
        float holdSec      = 0.0f;  // 0.0 .. 3.0 s (Peak Hold Time)
        float decaySec     = 1.20f; // 0.005 .. 15.0 s
        float sustainLvl   = 0.78f; // 0.0 .. 1.0
        float releaseSec   = 1.80f; // 0.01 .. 20.0 s
        float velocitySens = 0.0f;  // 0.0 .. 1.0 (Dynamic velocity sensitivity, default 0.0 = full volume)

        Parameters() = default;
        Parameters(float a, float d, float s, float r, float del = 0.0f, float h = 0.0f, float vel = 0.0f)
            : delaySec(del), attackSec(a), holdSec(h), decaySec(d), sustainLvl(s), releaseSec(r), velocitySens(vel) {}
    };

    void prepare(double sampleRate) noexcept {
        sampleRate_ = std::max(1000.0, sampleRate);
        recalculateRates();
    }

    void reset() noexcept {
        stage_ = Stage::Idle;
        level_ = 0.0f;
        delaySamplesRemaining_ = 0;
        holdSamplesRemaining_ = 0;
        velocityScale_ = 1.0f;
    }

    void setParameters(const Parameters& p) noexcept {
        params_ = p;
        params_.delaySec     = std::clamp(params_.delaySec,     0.0f,   2.0f);
        params_.attackSec    = std::clamp(params_.attackSec,    0.001f, 12.0f);
        params_.holdSec      = std::clamp(params_.holdSec,      0.0f,   3.0f);
        params_.decaySec     = std::clamp(params_.decaySec,     0.005f, 15.0f);
        params_.sustainLvl   = std::clamp(params_.sustainLvl,   0.0f,   1.0f);
        params_.releaseSec   = std::clamp(params_.releaseSec,   0.005f, 20.0f);
        params_.velocitySens = std::clamp(params_.velocitySens, 0.0f,   1.0f);
        recalculateRates();
    }

    [[nodiscard]] const Parameters& getParameters() const noexcept { return params_; }

    void noteOn(float velocity = 1.0f) noexcept {
        const float safeVel = std::clamp(velocity, 0.05f, 1.0f);
        velocityScale_ = (1.0f - params_.velocitySens) + params_.velocitySens * safeVel;

        level_ = std::clamp(level_, 0.0f, 1.0f);
        if (params_.delaySec > 0.001f) {
            delaySamplesRemaining_ = static_cast<int>(params_.delaySec * sampleRate_);
            stage_ = Stage::Delay;
        } else {
            delaySamplesRemaining_ = 0;
            stage_ = Stage::Attack;
        }
    }

    void noteOff() noexcept {
        if (stage_ != Stage::Idle) {
            stage_ = Stage::Release;
        }
    }

    [[nodiscard]] float processSample() noexcept {
        switch (stage_) {
            case Stage::Idle:
                return 0.0f;

            case Stage::Delay:
                if (--delaySamplesRemaining_ <= 0) {
                    stage_ = Stage::Attack;
                }
                return 0.0f;

            case Stage::Attack:
                // Slightly curved overshoot target (1.08f) for natural analog attack curve
                level_ += attackCoeff_ * (1.08f - level_);
                if (level_ >= 1.0f) {
                    level_ = 1.0f;
                    if (params_.holdSec > 0.001f) {
                        holdSamplesRemaining_ = static_cast<int>(params_.holdSec * sampleRate_);
                        stage_ = Stage::Hold;
                    } else {
                        stage_ = Stage::Decay;
                    }
                }
                return level_ * velocityScale_;

            case Stage::Hold:
                if (--holdSamplesRemaining_ <= 0) {
                    stage_ = Stage::Decay;
                }
                return level_ * velocityScale_;

            case Stage::Decay:
                level_ += decayCoeff_ * (params_.sustainLvl - level_);
                if (std::abs(level_ - params_.sustainLvl) < 1e-4f) {
                    level_ = params_.sustainLvl;
                    stage_ = Stage::Sustain;
                }
                return level_ * velocityScale_;

            case Stage::Sustain:
                level_ = params_.sustainLvl;
                return level_ * velocityScale_;

            case Stage::Release:
                level_ += releaseCoeff_ * (0.0f - level_);
                if (level_ < 1e-4f) {
                    level_ = 0.0f;
                    stage_ = Stage::Idle;
                }
                return level_ * velocityScale_;
        }
        return 0.0f;
    }

    [[nodiscard]] bool isActive() const noexcept { return stage_ != Stage::Idle; }
    [[nodiscard]] float getCurrentLevel() const noexcept { return level_; }
    [[nodiscard]] Stage getStage() const noexcept { return stage_; }

    [[nodiscard]] static std::vector<float> buildPreviewCurve(const Parameters& p, int numPoints = 16) {
        std::vector<float> pts(static_cast<std::size_t>(std::max(4, numPoints)), 0.0f);
        const float total = std::max(0.2f, p.attackSec + p.decaySec + 0.8f + p.releaseSec);
        const float aEnd = p.attackSec / total;
        const float dEnd = (p.attackSec + p.decaySec) / total;
        const float sEnd = (p.attackSec + p.decaySec + 0.8f) / total;

        for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
            const float u = static_cast<float>(i) / static_cast<float>(pts.size() - 1);
            if (u <= aEnd) {
                pts[static_cast<std::size_t>(i)] = (aEnd > 1e-4f) ? (u / aEnd) : 1.0f;
            } else if (u <= dEnd) {
                const float t = (u - aEnd) / std::max(1e-4f, dEnd - aEnd);
                pts[static_cast<std::size_t>(i)] = 1.0f + (p.sustainLvl - 1.0f) * std::sqrt(t);
            } else if (u <= sEnd) {
                pts[static_cast<std::size_t>(i)] = p.sustainLvl;
            } else {
                const float t = (u - sEnd) / std::max(1e-4f, 1.0f - sEnd);
                pts[static_cast<std::size_t>(i)] = p.sustainLvl * (1.0f - t) * (1.0f - t);
            }
        }
        return pts;
    }

private:
    void recalculateRates() noexcept {
        const float sr = static_cast<float>(sampleRate_);
        attackCoeff_  = 1.0f - std::exp(-2.6f / std::max(1.0f, params_.attackSec * sr));
        decayCoeff_   = 1.0f - std::exp(-4.5f / std::max(1.0f, params_.decaySec * sr));
        releaseCoeff_ = 1.0f - std::exp(-5.0f / std::max(1.0f, params_.releaseSec * sr));
    }

    double sampleRate_ = 48000.0;
    Parameters params_{};
    Stage stage_ = Stage::Idle;
    float level_ = 0.0f;
    int delaySamplesRemaining_ = 0;
    int holdSamplesRemaining_ = 0;
    float velocityScale_ = 1.0f;
    float attackCoeff_  = 0.01f;
    float decayCoeff_   = 0.005f;
    float releaseCoeff_ = 0.005f;
};

} // namespace nova::dsp
