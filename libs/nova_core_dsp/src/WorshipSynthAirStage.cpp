#include "nova_dsp/synth/WorshipSynthAirStage.h"
#include <algorithm>
#include <cmath>

namespace nova::dsp {

namespace {
constexpr float kAirCenterHz = 7500.0f;
constexpr float kAirShelfQ = 0.65f;
} // namespace

void WorshipSynthAirStage::prepare(double sampleRate) noexcept {
    sampleRate_ = std::max(8000.0, sampleRate);
    shelfL_.prepare(sampleRate_);
    shelfR_.prepare(sampleRate_);
    gainDbSmoother_.prepare(sampleRate_, 15.0f);
    reset();
}

void WorshipSynthAirStage::reset() noexcept {
    shelfL_.reset();
    shelfR_.reset();
    gainDbSmoother_.reset(targetGainDb_);
    lastAppliedGainDb_ = -999.0f;
}

void WorshipSynthAirStage::snapToTarget() noexcept {
    gainDbSmoother_.snapToTarget();
    const float gDb = gainDbSmoother_.getCurrentValue();
    const float centerHz = std::min(kAirCenterHz, static_cast<float>(sampleRate_ * 0.42));
    shelfL_.setParameters(StateVariableFilter::Type::HighShelf, centerHz, kAirShelfQ, gDb);
    shelfR_.setParameters(StateVariableFilter::Type::HighShelf, centerHz, kAirShelfQ, gDb);
    lastAppliedGainDb_ = gDb;
}

void WorshipSynthAirStage::setMacroAir(float macroAir) noexcept {
    const float air = std::clamp(macroAir, 0.0f, 1.0f);
    if (air >= 0.30f) {
        targetGainDb_ = ((air - 0.30f) / 0.70f) * 7.5f;
    } else {
        targetGainDb_ = ((air - 0.30f) / 0.30f) * 3.0f;
    }
    gainDbSmoother_.setTargetValue(targetGainDb_);
}

void WorshipSynthAirStage::processSample(float& left, float& right, bool updateCoeffs) noexcept {
    const float gDb = gainDbSmoother_.getNextValue();
    if (lastAppliedGainDb_ < -100.0f || (updateCoeffs && std::abs(gDb - lastAppliedGainDb_) > 0.01f)) {
        const float centerHz = std::min(kAirCenterHz, static_cast<float>(sampleRate_ * 0.42));
        shelfL_.setParameters(StateVariableFilter::Type::HighShelf, centerHz, kAirShelfQ, gDb);
        shelfR_.setParameters(StateVariableFilter::Type::HighShelf, centerHz, kAirShelfQ, gDb);
        lastAppliedGainDb_ = gDb;
    }

    if (std::abs(lastAppliedGainDb_) > 0.01f || std::abs(gDb) > 0.01f) {
        left = shelfL_.processSample(left);
        right = shelfR_.processSample(right);
    }
}

void WorshipSynthAirStage::applyReverbAirCoupling(float macroAir,
                                                  float macroWarmth,
                                                  DualEngineReverb::Parameters& revParams) noexcept {
    const float air = std::clamp(macroAir, 0.0f, 1.0f);
    const float warmth = std::clamp(macroWarmth, 0.0f, 1.0f);
    const float airNorm = std::clamp(air + (0.50f - warmth) * 0.22f, 0.0f, 1.0f);

    revParams.highCutHz = std::clamp(7500.0f + airNorm * 9000.0f, 7500.0f, 16500.0f);
    revParams.highDamping = std::clamp(0.58f - airNorm * 0.36f, 0.22f, 0.58f);
    revParams.shimmerHighCutHz = std::clamp(9000.0f + airNorm * 6500.0f, 9000.0f, 15500.0f);
}

} // namespace nova::dsp
