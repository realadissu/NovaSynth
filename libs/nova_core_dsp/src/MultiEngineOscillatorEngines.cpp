#include "nova_dsp/oscillators/MultiEngineOscillator.h"
#include "nova_dsp/oscillators/PadSynthBank.h"
#include "nova_dsp/oscillators/SampleBank.h"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace nova::dsp {

void MultiEngineOscillator::processOrganicTexture(float slotFreq, float shape, float invSr, float& sigL, float& sigR) noexcept {
    const auto& padBank = getPadSynthBank();
    const int model = std::clamp(params_.modelIndex, 0, PadSynthBank::kNumModels - 1);

    constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
    constexpr double kWrapSpan = static_cast<double>(PadSynthBank::kTableSize) * 8.0;
    const float sr = static_cast<float>(sampleRate_);
    const float rateAmt = std::clamp(params_.rate, 0.0f, 1.0f);
    const float scatterAmt = std::clamp(params_.scatter, 0.0f, 1.0f);
    const float effShape = std::clamp(shape, 0.0f, 1.0f);
    const float warpAmt = std::clamp(params_.warp, 0.0f, 1.0f);
    const float durAmt = std::clamp(params_.duration, 0.0f, 1.0f);

    const double baseIncZ0 = (static_cast<double>(slotFreq) / static_cast<double>(PadSynthBank::kZoneRootHz[0]))
                           * (PadSynthBank::kRefSampleRate / sampleRate_);

    texGrainPhase_ += (0.07f + 0.55f * rateAmt) * invSr;
    if (texGrainPhase_ >= 1.0f) texGrainPhase_ -= 1.0f;

    const float driftSin = std::sin(twoPi * texGrainPhase_);
    double driftMult = 1.0 + 0.00085 * static_cast<double>(driftSin) * (0.35 + 0.65 * static_cast<double>(rateAmt));
    if (model == 2) { // Tape Halo & Flutter: warm wow + organic tape flutter
        flutterPhase_ += (3.2f + 4.2f * rateAmt) * invSr;
        if (flutterPhase_ >= 1.0f) flutterPhase_ -= 1.0f;
        const double fltSin = static_cast<double>(std::sin(twoPi * flutterPhase_));
        driftMult = 1.0 + (0.0026 * static_cast<double>(driftSin) + 0.0016 * fltSin)
                        * (0.45 + 0.55 * static_cast<double>(rateAmt));
    }

    float p1L = 0.0f, p1R = 0.0f, p2L = 0.0f, p2R = 0.0f;
    PadSynthBank::sampleZoneCrossfaded(padBank, model, slotFreq, padPosPrimary_, p1L, p1R);
    PadSynthBank::sampleZoneCrossfaded(padBank, model, slotFreq, padPosSecondary_, p2L, p2R);

    padPosPrimary_ += baseIncZ0 * padDetuneRatioPrimary_ * driftMult;
    if (padPosPrimary_ >= kWrapSpan) padPosPrimary_ -= kWrapSpan;
    padPosSecondary_ += baseIncZ0 * padDetuneRatioSecondary_ / driftMult;
    if (padPosSecondary_ >= kWrapSpan) padPosSecondary_ -= kWrapSpan;

    const float swayAngle = scatterAmt * 0.65f * driftSin;
    const float cosSway = std::cos(swayAngle);
    const float sinSway = std::sin(swayAngle);
    const float rot2L = p2L * cosSway - p2R * sinSway;
    const float rot2R = p2L * sinSway + p2R * cosSway;

    const float durMix = 0.55f + 0.45f * durAmt;
    const float invNorm = 1.0f / std::sqrt(1.0f + durMix * durMix);
    float texL = (p1L + rot2L * durMix) * invNorm;
    float texR = (p1R + rot2R * durMix) * invNorm;

    if (warpAmt > 0.005f && slotFreq * 2.0f < sr * 0.44f) {
        float octL = 0.0f, octR = 0.0f;
        double octPos = padPosSecondary_ * 2.0 + 21845.0;
        if (octPos >= kWrapSpan) octPos = std::fmod(octPos, kWrapSpan);
        PadSynthBank::sampleZoneCrossfaded(padBank, model, slotFreq * 2.0f, octPos, octL, octR);
        const float nyqGuard = std::clamp((0.42f * sr - 2.0f * slotFreq) / (0.10f * sr), 0.0f, 1.0f);
        const float haloAmt = warpAmt * 0.52f * nyqGuard;
        const float baseAtten = 1.0f / std::sqrt(1.0f + haloAmt * haloAmt);
        texL = texL * baseAtten + octR * haloAmt;
        texR = texR * baseAtten + octL * haloAmt;
    }

    if (model == 0) { // Ambient Choir Breath: 3-band vowel formant morph (/u/ -> /o/ -> /a/ -> /i/)
        float f1Hz = 320.0f, f2Hz = 760.0f, f3Hz = 2300.0f;
        float q1 = 2.4f, q2 = 2.8f, q3 = 2.2f;
        if (effShape < 0.333f) {
            const float t = effShape / 0.333f;
            f1Hz = 320.0f + t * (460.0f - 320.0f);
            f2Hz = 760.0f + t * (920.0f - 760.0f);
            f3Hz = 2300.0f + t * (2480.0f - 2300.0f);
        } else if (effShape < 0.667f) {
            const float t = (effShape - 0.333f) / 0.334f;
            f1Hz = 460.0f + t * (720.0f - 460.0f);
            f2Hz = 920.0f + t * (1180.0f - 920.0f);
            f3Hz = 2480.0f + t * (2680.0f - 2480.0f);
            q1 = 2.4f + t * 0.2f;
            q2 = 2.8f + t * 0.2f;
            q3 = 2.2f + t * 0.2f;
        } else {
            const float t = (effShape - 0.667f) / 0.333f;
            f1Hz = 720.0f + t * (300.0f - 720.0f);
            f2Hz = 1180.0f + t * (2150.0f - 1180.0f);
            f3Hz = 2680.0f + t * (2950.0f - 2680.0f);
            q1 = 2.6f - t * 0.1f;
            q2 = 3.0f + t * 0.2f;
            q3 = 2.4f + t * 0.2f;
        }

        if (std::abs(effShape - lastFormantShape_) > 0.002f || lastFormantModel_ != 0) {
            lastFormantShape_ = effShape;
            lastFormantModel_ = 0;
            formantFiltersL_[0].setParameters(StateVariableFilter::Type::BandPass, f1Hz, q1);
            formantFiltersL_[1].setParameters(StateVariableFilter::Type::BandPass, f2Hz, q2);
            formantFiltersL_[2].setParameters(StateVariableFilter::Type::BandPass, f3Hz, q3);
            formantFiltersR_[0].setParameters(StateVariableFilter::Type::BandPass, f1Hz, q1);
            formantFiltersR_[1].setParameters(StateVariableFilter::Type::BandPass, f2Hz, q2);
            formantFiltersR_[2].setParameters(StateVariableFilter::Type::BandPass, f3Hz, q3);
        }

        const float f1L = formantFiltersL_[0].processSample(texL) / q1;
        const float f2L = formantFiltersL_[1].processSample(texL) / q2;
        const float f3L = formantFiltersL_[2].processSample(texL) / q3;
        const float f1R = formantFiltersR_[0].processSample(texR) / q1;
        const float f2R = formantFiltersR_[1].processSample(texR) / q2;
        const float f3R = formantFiltersR_[2].processSample(texR) / q3;
        const float w1 = 0.85f - 0.35f * effShape;
        const float w2 = 0.40f + 1.20f * effShape;
        const float w3 = 0.08f + 0.48f * effShape;
        const float formL = f1L * w1 + f2L * w2 + f3L * w3;
        const float formR = f1R * w1 + f2R * w2 + f3R * w3;
        texL = (texL * 0.30f + formL * 0.70f) * 1.08f;
        texR = (texR * 0.30f + formR * 0.70f) * 1.08f;
    } else {
        const float tiltCutHz = 550.0f + effShape * effShape * 11500.0f;
        const float wTilt = twoPi * tiltCutHz * invSr;
        const float lpAlpha = std::clamp(wTilt / (1.0f + wTilt), 0.02f, 0.98f);
        texFilterState1_ += lpAlpha * (texL - texFilterState1_);
        texFilterState2_ += lpAlpha * (texR - texFilterState2_);
        const float presenceMix = std::clamp((effShape - 0.35f) * 1.15f, 0.0f, 0.75f);
        const float tiltGainComp = 1.0f + (1.0f - effShape) * 0.12f;
        texL = (texFilterState1_ + (texL - texFilterState1_) * presenceMix) * tiltGainComp;
        texR = (texFilterState2_ + (texR - texFilterState2_) * presenceMix) * tiltGainComp;
    }

    const float mid = 0.5f * (texL + texR);
    const float side = 0.5f * (texL - texR);
    const float width = std::clamp(params_.spread, 0.0f, 1.0f);
    const float monoComp = 1.0f + 0.35f * (1.0f - width);
    sigL = (mid + side * width) * monoComp * 0.68f;
    sigR = (mid - side * width) * monoComp * 0.68f;
}

void MultiEngineOscillator::triggerPhysicalPluck(float baseFrequencyHz, float velocity) noexcept {
    const float freq = std::clamp(baseFrequencyHz * basePitchRatio_, 25.0f, 4000.0f);
    const int periodSamples = std::clamp(static_cast<int>(sampleRate_ / freq), 8, kMaxStringDelay - 8);
    const float velScale = std::clamp(0.35f + 0.65f * velocity, 0.1f, 1.0f);
    const float pluckHardness = std::clamp(params_.shapeMorph, 0.05f, 0.95f);
    const float srF = static_cast<float>(sampleRate_);
    float smoothExcL = 0.0f;
    switch (params_.modelIndex) {
        case 0:
            for (int i = 0; i < periodSamples; ++i) {
                rng_ = rng_ * 1664525u + 1013904223u;
                const float nL = (static_cast<float>(rng_ & 0xFFFFu) / 32767.5f) - 1.0f;
                const float win = std::sin(std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(periodSamples));
                smoothExcL += (0.35f + 0.55f * pluckHardness) * (nL - smoothExcL);
                const float body = std::sin(2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(periodSamples));
                stringDelayL_[static_cast<std::size_t>(i)] = stringDelayR_[static_cast<std::size_t>(i)] = (smoothExcL * 1.5f + body * 0.70f) * win * velScale;
            }
            break;
        case 1:
            for (int i = 0; i < periodSamples; ++i) {
                rng_ = rng_ * 1664525u + 1013904223u;
                const float nL = (static_cast<float>(rng_ & 0xFFFFu) / 32767.5f) - 1.0f;
                const float win = std::pow(std::sin(std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(periodSamples)), 1.8f);
                smoothExcL += (0.16f + 0.45f * pluckHardness) * (nL - smoothExcL);
                const float body = std::sin(std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(periodSamples));
                stringDelayL_[static_cast<std::size_t>(i)] = stringDelayR_[static_cast<std::size_t>(i)] = (smoothExcL * 0.85f + body * 1.35f) * win * velScale;
            }
            break;
        case 2:
            modalPhases_.fill(0.0f);
            modalAmplitudes_ = { 0.95f * velScale, (0.55f + 0.30f * pluckHardness) * velScale, (0.35f + 0.25f * pluckHardness) * velScale, (0.20f + 0.20f * pluckHardness) * velScale };
            modalDecays_ = { 1.0f - (12.0f / (srF * (0.8f + pluckHardness * 1.8f))), 1.0f - (28.0f / (srF * (0.5f + pluckHardness * 1.2f))),
                             1.0f - (55.0f / (srF * (0.3f + pluckHardness * 0.8f))), 1.0f - (90.0f / (srF * (0.2f + pluckHardness * 0.5f))) };
            for (int i = 0; i < periodSamples; ++i) {
                const float click = (i < 8) ? ((8 - i) * 0.12f * velScale) : 0.0f;
                stringDelayL_[static_cast<std::size_t>(i)] = stringDelayR_[static_cast<std::size_t>(i)] = click;
            }
            break;
        case 3: default:
            modalPhases_.fill(0.0f);
            modalAmplitudes_ = { 0.90f * velScale, 0.65f * velScale, (0.45f + 0.35f * pluckHardness) * velScale, (0.25f + 0.30f * pluckHardness) * velScale };
            modalDecays_ = { 1.0f - (4.0f / (srF * (1.5f + pluckHardness * 2.5f))), 1.0f - (9.0f / (srF * (1.2f + pluckHardness * 2.0f))),
                             1.0f - (18.0f / (srF * (0.8f + pluckHardness * 1.5f))), 1.0f - (35.0f / (srF * (0.5f + pluckHardness * 1.0f))) };
            for (int i = 0; i < periodSamples; ++i) {
                const float chimeClick = (i < 5) ? ((5 - i) * 0.15f * velScale) : 0.0f;
                stringDelayL_[static_cast<std::size_t>(i)] = stringDelayR_[static_cast<std::size_t>(i)] = chimeClick;
            }
            break;
    }
    stringWritePos_ = 0; stringLpStateL_ = 0.0f; stringLpStateR_ = 0.0f;
}

void MultiEngineOscillator::processPhysicalPluck(float slotFreq, float shape, float invSr, float& sigL, float& sigR) noexcept {
    const float sr = static_cast<float>(sampleRate_);
    if (params_.modelIndex >= 2) {
        // Modal Inharmonic Resonator (Kalimba / Glass Mallet)
        const float twoPi = 2.0f * std::numbers::pi_v<float>;
        const std::array<float, 4> modeRatios = (params_.modelIndex == 2)
            ? std::array<float, 4>{ 1.0f, 2.756f, 5.404f, 8.933f }
            : std::array<float, 4>{ 1.0f, 2.0f, 3.162f, 4.520f };

        float modalSumL = 0.0f, modalSumR = 0.0f;
        for (int m = 0; m < 4; ++m) {
            const auto idx = static_cast<std::size_t>(m);
            const float mFreq = std::min(sr * 0.46f, slotFreq * modeRatios[idx]);
            modalPhases_[idx] += mFreq * invSr;
            if (modalPhases_[idx] >= 1.0f) modalPhases_[idx] -= 1.0f;

            modalAmplitudes_[idx] *= modalDecays_[idx];
            const float s = std::sin(twoPi * modalPhases_[idx]) * modalAmplitudes_[idx];
            if ((m & 1) == 0) {
                modalSumL += s;
                modalSumR += s * 0.70f;
            } else {
                modalSumL += s * 0.70f;
                modalSumR += s;
            }
        }

        const float modalScale = (params_.modelIndex == 2) ? 0.49f : 0.42f;
        sigL = modalSumL * modalScale;
        sigR = modalSumR * modalScale;
    } else {
        // Waveguide Strings (Steel String & Nylon Harp)
        const float delayLenL = std::clamp(sr / slotFreq, 8.0f, static_cast<float>(kMaxStringDelay - 8));
        const float delayLenR = std::clamp(delayLenL * pluckDelayRatioR_, 8.0f, static_cast<float>(kMaxStringDelay - 8));

        const auto readWaveguide = [this](const std::vector<float>& buf, float delaySamples) noexcept {
            const int dInt = static_cast<int>(delaySamples);
            const float frac = delaySamples - static_cast<float>(dInt);
            const int idx0 = (stringWritePos_ - dInt + kMaxStringDelay) & (kMaxStringDelay - 1);
            const int idx1 = (idx0 - 1 + kMaxStringDelay) & (kMaxStringDelay - 1);
            return buf[static_cast<std::size_t>(idx0)] + frac * (buf[static_cast<std::size_t>(idx1)] - buf[static_cast<std::size_t>(idx0)]);
        };

        const float rawL = readWaveguide(stringDelayL_, delayLenL);
        const float rawR = readWaveguide(stringDelayR_, delayLenR);
        const float brightness = (params_.modelIndex == 0) ? (0.35f + 0.60f * shape) : (0.15f + 0.40f * shape);
        stringLpStateL_ = rawL * brightness + stringLpStateL_ * (1.0f - brightness);
        stringLpStateR_ = rawR * brightness + stringLpStateR_ * (1.0f - brightness);

        const float sustainFeedback = (params_.modelIndex == 0) ? (0.9950f + 0.0045f * shape) : (0.9910f + 0.0060f * shape);
        stringDelayL_[static_cast<std::size_t>(stringWritePos_)] = stringLpStateL_ * sustainFeedback;
        stringDelayR_[static_cast<std::size_t>(stringWritePos_)] = stringLpStateR_ * sustainFeedback;
        stringWritePos_ = (stringWritePos_ + 1) & (kMaxStringDelay - 1);

        phases_[0] += slotFreq * invSr;
        if (phases_[0] >= 1.0f) phases_[0] -= 1.0f;
        const float modalBody = (params_.modelIndex == 0)
            ? (0.15f * (1.0f - 2.0f * phases_[0]))
            : (0.28f * std::sin(2.0f * std::numbers::pi_v<float> * phases_[0]));

        const float pluckScale = (params_.modelIndex == 0) ? 4.7f : 2.5f;
        sigL = (stringLpStateL_ + modalBody) * pluckScale;
        sigR = (stringLpStateR_ + modalBody) * pluckScale;
    }
}

void MultiEngineOscillator::processSubOscillator(float slotFreq, float shape, float invSr, float& sigL, float& sigR) noexcept {
    // Dedicated clean sub bass: 1 octave below fundamental (slotFreq * 0.5f)
    const float subFreq = slotFreq * 0.5f;
    const float dt = subFreq * invSr;
    phases_[0] += dt;
    if (phases_[0] >= 1.0f) phases_[0] -= 1.0f;
    const float ph = phases_[0];
    const float twoPi = 2.0f * std::numbers::pi_v<float>;

    const float sine = std::sin(twoPi * ph);
    const float tri = 2.0f * std::abs(2.0f * (ph - std::floor(ph + 0.5f))) - 1.0f;
    const float sqr = (ph < 0.5f ? 0.85f : -0.85f) + polyBlep(ph, dt) - polyBlep(std::fmod(ph + 0.5f, 1.0f), dt);

    float subVal = sine;
    switch (params_.modelIndex) {
        case 0: subVal = sine; break;
        case 1: subVal = (1.0f - shape * 0.45f) * tri + (shape * 0.45f) * sine; break;
        case 2: {
            const float driveX = sine * (1.6f + shape * 2.4f);
            const float x2 = driveX * driveX;
            subVal = (driveX * (27.0f + x2) / (27.0f + 9.0f * x2)) * 0.88f;
            break;
        }
        case 3: subVal = (1.0f - shape * 0.35f) * sqr + (shape * 0.35f) * sine; break;
        default: subVal = sine; break;
    }

    sigL = subVal * 0.95f;
    sigR = subVal * 0.95f;
}

void MultiEngineOscillator::processNoiseTransient(float shape, float invSr, float& sigL, float& sigR) noexcept {
    noiseGen_.processSample(params_.modelIndex, shape, params_.duration, invSr, sigL, sigR);
}

} // namespace nova::dsp
