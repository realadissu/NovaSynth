#pragma once

#include "nova_dsp/filters/StateVariableFilter.h"
#include "nova_dsp/math/OnePoleSmoother.h"
#include "nova_dsp/reverb/DualEngineReverb.h"

namespace nova::dsp {

/**
 * @brief Dedicated post-filter silky High-Shelf EQ stage and reverb air-coupling helper.
 * Decouples macroAir from opening filterCutoffHz, delivering smooth +7.5 dB shelf air above 7.5 kHz.
 */
class WorshipSynthAirStage {
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void snapToTarget() noexcept;
    void setMacroAir(float macroAir) noexcept;
    void processSample(float& left, float& right, bool updateCoeffs) noexcept;

    static void applyReverbAirCoupling(float macroAir,
                                       float macroWarmth,
                                       DualEngineReverb::Parameters& revParams) noexcept;

    [[nodiscard]] float getTargetGainDb() const noexcept { return targetGainDb_; }

private:
    double sampleRate_ = 48000.0;
    StateVariableFilter shelfL_{};
    StateVariableFilter shelfR_{};
    OnePoleSmoother gainDbSmoother_{};
    float targetGainDb_ = 0.0f;
    float lastAppliedGainDb_ = -999.0f;
};

} // namespace nova::dsp
