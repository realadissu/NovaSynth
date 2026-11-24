#include "nova_dsp/oscillators/MultiEngineOscillator.h"
#include "nova_dsp/oscillators/PadSynthBank.h"
#include "nova_dsp/oscillators/SampleBank.h"
#include "OscillatorSharedTables.h"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace nova::dsp {

float MultiEngineOscillator::polyBlep(float phase, float phaseInc) noexcept {
    if (phase < phaseInc) { const float t = phase / phaseInc; return t + t - t * t - 1.0f; }
    if (phase > 1.0f - phaseInc) { const float t = (phase - 1.0f) / phaseInc; return t * t + t + t + 1.0f; }
    return 0.0f;
}

MultiEngineOscillator::MultiEngineOscillator() {
    stringDelayL_.assign(static_cast<std::size_t>(kMaxStringDelay), 0.0f);
    stringDelayR_.assign(static_cast<std::size_t>(kMaxStringDelay), 0.0f);
}

void MultiEngineOscillator::prepare(double sampleRate) {
    sampleRate_ = std::max(8000.0, sampleRate);
    if (stringDelayL_.size() != static_cast<std::size_t>(kMaxStringDelay)) {
        stringDelayL_.assign(static_cast<std::size_t>(kMaxStringDelay), 0.0f);
        stringDelayR_.assign(static_cast<std::size_t>(kMaxStringDelay), 0.0f);
    }
    shapeSmoother_.prepare(sampleRate_, 20.0f); shapeSmoother_.reset(params_.shapeMorph);
    for (std::size_t i = 0; i < formantFiltersL_.size(); ++i) {
        formantFiltersL_[i].prepare(sampleRate_);
        formantFiltersR_[i].prepare(sampleRate_);
    }
    noiseGen_.prepare(sampleRate_);
    lastFormantShape_ = -999.0f; lastFormantModel_ = -1;
    (void)getWavetableBank(); (void)getPadSynthBank();
    updateCoefficients(); reset();
}

void MultiEngineOscillator::updateCoefficients() noexcept {
    const float totalSemi = static_cast<float>(params_.octaveOffset * 12 + params_.semitoneOffset) + (params_.fineCents * 0.01f);
    basePitchRatio_ = std::pow(2.0f, totalSemi / 12.0f);
    const float detuneAmt = std::clamp(params_.detune, 0.0f, 1.0f);
    const float scatterAmt = std::clamp(params_.scatter, 0.0f, 1.0f);
    const float scatterCurve = scatterAmt * (0.45f + 0.55f * scatterAmt);
    const float maxDetuneCents = 13.5f * detuneAmt + 32.0f * scatterCurve;
    const float driftScale = std::max(detuneAmt, scatterAmt);
    pluckDelayRatioR_ = std::pow(2.0f, (detuneAmt * 14.0f) / 1200.0f);
    const float stereoWidth = std::clamp(std::max(params_.spread, driftScale * 0.7f), 0.0f, 1.0f);

    for (int u = 0; u < kMaxUnison; ++u) {
        const auto idx = static_cast<std::size_t>(u);
        const float cents = kUnisonDetuneFactors[idx] * maxDetuneCents + driftOffsetsCents_[idx] * driftScale;
        unisonPitchMultipliers_[idx] = std::pow(2.0f, cents / 1200.0f);
        const float uPan = std::clamp(kUnisonPanPositions[idx] * stereoWidth, -1.0f, 1.0f);
        const float panAngle = (uPan + 1.0f) * 0.25f * std::numbers::pi_v<float>;
        unisonPanL_[idx] = std::cos(panAngle);
        unisonPanR_[idx] = std::sin(panAngle);

        const float s2Sign = ((u & 1) == 0) ? 1.0f : -1.0f;
        const float s2Cents = s2Sign * (5.5f + 8.5f * driftScale);
        const float sOctCents = -s2Sign * (3.5f + 6.5f * driftScale);
        superDetuneRatio2_[idx] = std::pow(2.0f, s2Cents / 1200.0f);
        superDetuneRatioOct_[idx] = 2.0f * std::pow(2.0f, sOctCents / 1200.0f);
    }

    const double halfSampleCents = static_cast<double>(scatterAmt) * 25.0;
    sampleDetuneRatioL_ = std::pow(2.0, -halfSampleCents / 1200.0);
    sampleDetuneRatioR_ = std::pow(2.0, +halfSampleCents / 1200.0);

    const double halfPadCents = static_cast<double>(detuneAmt) * 9.0 + static_cast<double>(scatterAmt) * 3.0;
    padDetuneRatioPrimary_ = std::pow(2.0, -halfPadCents / 1200.0);
    padDetuneRatioSecondary_ = std::pow(2.0, +halfPadCents / 1200.0);
}

void MultiEngineOscillator::reset() noexcept {
    rng_ = 0x9E3779B9u;
    shapeSmoother_.reset(params_.shapeMorph);
    phases_.fill(0.0f);
    subPhases_.fill(0.0f);
    superPhases2_.fill(0.0f);
    superPhasesOct_.fill(0.0f);
    driftOffsetsCents_.fill(0.0f);
    texFilterState1_ = 0.0f; texFilterState2_ = 0.0f;
    texGrainPhase_ = 0.0f;   flutterPhase_ = 0.0f;
    padPosPrimary_ = 0.0;    padPosSecondary_ = 21845.0;
    for (std::size_t i = 0; i < formantFiltersL_.size(); ++i) {
        formantFiltersL_[i].reset();
        formantFiltersR_[i].reset();
    }
    noiseGen_.reset();
    lastFormantShape_ = -999.0f; lastFormantModel_ = -1;
    std::fill(stringDelayL_.begin(), stringDelayL_.end(), 0.0f);
    std::fill(stringDelayR_.begin(), stringDelayR_.end(), 0.0f);
    stringWritePos_ = 0; stringLpStateL_ = 0.0f; stringLpStateR_ = 0.0f;
    modalPhases_.fill(0.0f); modalAmplitudes_.fill(0.0f); modalDecays_.fill(0.999f);
    samplePlayhead_ = 0.0; sampleDirection_ = 1.0; sampleFinished_ = false;
    samplePlayheadR_ = 0.0; sampleDirectionR_ = 1.0; sampleFinishedR_ = false;
    lastPitchModSemitones_ = -999.0f; cachedPitchModRatio_ = 1.0f;
}

void MultiEngineOscillator::triggerNote(float baseFrequencyHz, float velocity, uint32_t seedOffset) noexcept {
    shapeSmoother_.reset(params_.shapeMorph);
    rng_ ^= (seedOffset * 747796405u + 2891336453u);
    noiseGen_.trigger(seedOffset);

    const bool isPolyPadEngine = (params_.engine == EngineType::WarmAnalog ||
                                  params_.engine == EngineType::EvolvingWavetable ||
                                  params_.engine == EngineType::OrganicTexture);
    driftOffsetsCents_[0] = 0.0f;
    for (int u = 0; u < kMaxUnison; ++u) {
        const auto idx = static_cast<std::size_t>(u);
        rng_ = rng_ * 1664525u + 1013904223u;
        phases_[idx] = isPolyPadEngine ? (static_cast<float>(rng_ & 0xFFFFu) / 65536.0f) : 0.0f;
        subPhases_[idx] = isPolyPadEngine ? (0.5f * phases_[idx] + (((rng_ >> 16) & 1u) ? 0.5f : 0.0f)) : 0.0f;
        rng_ = rng_ * 1664525u + 1013904223u;
        superPhases2_[idx] = isPolyPadEngine ? (static_cast<float>(rng_ & 0xFFFFu) / 65536.0f) : 0.0f;
        rng_ = rng_ * 1664525u + 1013904223u;
        superPhasesOct_[idx] = isPolyPadEngine ? (static_cast<float>(rng_ & 0xFFFFu) / 65536.0f) : 0.0f;
        if (u > 0) {
            rng_ = rng_ * 1664525u + 1013904223u;
            driftOffsetsCents_[idx] = ((static_cast<float>(rng_ & 0xFFFFu) / 32767.5f) - 1.0f) * 1.1f;
        }
    }
    updateCoefficients();

    texGrainPhase_ = 0.0f;
    texFilterState1_ = 0.0f; texFilterState2_ = 0.0f;

    if (params_.engine == EngineType::OrganicTexture) {
        constexpr double kWrapSpan = static_cast<double>(PadSynthBank::kTableSize) * 8.0;
        rng_ = rng_ * 1664525u + 1013904223u;
        const double r1 = static_cast<double>(rng_ & 0xFFFFu) / 65536.0;
        rng_ = rng_ * 1664525u + 1013904223u;
        const double r2 = static_cast<double>(rng_ & 0xFFFFu) / 65536.0;
        rng_ = rng_ * 1664525u + 1013904223u;
        const float r3 = static_cast<float>(rng_ & 0xFFFFu) / 65536.0f;
        rng_ = rng_ * 1664525u + 1013904223u;
        const float r4 = static_cast<float>(rng_ & 0xFFFFu) / 65536.0f;

        padPosPrimary_ = r1 * kWrapSpan;
        padPosSecondary_ = std::fmod(
            padPosPrimary_ + (0.28 + 0.44 * r2) * static_cast<double>(PadSynthBank::kTableSize),
            kWrapSpan);
        texGrainPhase_ = r3;
        flutterPhase_ = r4;
    }

    if (params_.engine == EngineType::SamplePlayback || params_.sourceType == 4) {
        triggerSamplePlayback(baseFrequencyHz);
    }

    triggerPhysicalPluck(baseFrequencyHz, velocity);
}

void MultiEngineOscillator::processSample(float baseFrequencyHz,
                                          float pitchModSemitones,
                                          float shapeMod,
                                          float& outLeft,
                                          float& outRight,
                                          float levelMod) noexcept {
    const float effLevel = std::clamp(params_.level + levelMod, 0.0f, 1.0f);
    if (!params_.enabled || effLevel <= 0.0001f) return;

    const float sr = static_cast<float>(sampleRate_), invSr = 1.0f / sr;
    float pitchModRatio = 1.0f;
    if (pitchModSemitones != 0.0f) {
        if (std::abs(pitchModSemitones - lastPitchModSemitones_) < 1e-4f) {
            pitchModRatio = cachedPitchModRatio_;
        } else {
            lastPitchModSemitones_ = pitchModSemitones;
            cachedPitchModRatio_ = std::exp2(pitchModSemitones * (1.0f / 12.0f));
            pitchModRatio = cachedPitchModRatio_;
        }
    }
    const float slotFreq = std::clamp(baseFrequencyHz * basePitchRatio_ * pitchModRatio, 20.0f, sr * 0.46f);
    shapeSmoother_.setTargetValue(params_.shapeMorph);
    const float shape = std::clamp(shapeSmoother_.getNextValue() + shapeMod, 0.0f, 1.0f);
    const float detuneAmt = std::clamp(params_.detune, 0.0f, 1.0f);
    const float scatterAmt = std::clamp(params_.scatter, 0.0f, 1.0f);

    float sigL = 0.0f, sigR = 0.0f;

    switch (params_.engine) {
        case EngineType::WarmAnalog: {
            const int activeUnison = (detuneAmt > 0.02f || scatterAmt > 0.02f) ? kMaxUnison : 1;
            constexpr float kUnisonNormGain = 0.85f / 2.2360679775f; // 0.85 / sqrt(5)
            const float normGain = (activeUnison > 1) ? kUnisonNormGain : 1.0f;

            for (int u = 0; u < activeUnison; ++u) {
                const auto idx = static_cast<std::size_t>(u);
                const float uFreq = std::min(sr * 0.46f, slotFreq * unisonPitchMultipliers_[idx]), dt = uFreq * invSr;
                const float dtSub = dt * 0.5f;

                float ph = phases_[idx] + dt;
                if (ph >= 1.0f) ph -= 1.0f;
                phases_[idx] = ph;

                float phSub = subPhases_[idx] + dtSub;
                if (phSub >= 1.0f) phSub -= 1.0f;
                subPhases_[idx] = phSub;

                float wave = 0.0f;
                switch (params_.modelIndex) {
                    case 0: { // Classic Saw
                        const float saw = (2.0f * ph - 1.0f) - polyBlep(ph, dt);
                        if (shape <= 0.001f) {
                            wave = saw;
                        } else {
                            const float subSaw = (2.0f * phSub - 1.0f) - polyBlep(phSub, dtSub);
                            wave = saw * (1.0f - shape * 0.35f) + subSaw * (shape * 0.35f);
                        }
                        break;
                    }
                    case 1: { // Pulse / Square
                        const float pw = 0.5f - 0.38f * shape;
                        float pulse = (ph < pw ? 1.0f : -1.0f) + polyBlep(ph, dt);
                        float shiftedPh = ph + (1.0f - pw);
                        if (shiftedPh >= 1.0f) shiftedPh -= 1.0f;
                        wave = pulse - polyBlep(shiftedPh, dt);
                        break;
                    }
                    case 2: { // Warm Triangle
                        const float tri = 2.0f * std::abs(2.0f * (ph - 0.5f)) - 1.0f;
                        wave = tri * (1.0f - shape * 0.70f) + std::tanh(tri * (1.0f + 2.5f * shape)) * (shape * 0.70f);
                        break;
                    }
                    case 3: { // Mellow Sine
                        const float s = std::sin(2.0f * std::numbers::pi_v<float> * ph);
                        if (shape <= 0.001f) {
                            wave = s;
                        } else {
                            const float drive = 1.0f + shape * 3.2f;
                            const float folded = std::sin(s * drive * (0.5f * std::numbers::pi_v<float>));
                            const float harmonic2 = std::sin(4.0f * std::numbers::pi_v<float> * ph);
                            wave = (1.0f - shape * 0.45f) * s + (shape * 0.45f) * folded + (shape * 0.15f) * harmonic2;
                        }
                        break;
                    }
                    case 4: { // Super Saw Multi
                        const float saw1 = (2.0f * ph - 1.0f) - polyBlep(ph, dt);
                        const float dt2 = std::min(0.48f, dt * superDetuneRatio2_[idx]);
                        float ph2 = superPhases2_[idx] + dt2;
                        if (ph2 >= 1.0f) ph2 -= 1.0f;
                        superPhases2_[idx] = ph2;
                        const float saw2 = (2.0f * ph2 - 1.0f) - polyBlep(ph2, dt2);

                        const float rawDtOct = dt * superDetuneRatioOct_[idx];
                        const float dtOct = std::min(0.48f, rawDtOct);
                        float phOct = superPhasesOct_[idx] + dtOct;
                        if (phOct >= 1.0f) phOct -= 1.0f;
                        superPhasesOct_[idx] = phOct;
                        const float sawOct = (rawDtOct < 0.46f) ? ((2.0f * phOct - 1.0f) - polyBlep(phOct, dtOct)) : 0.0f;

                        wave = (0.58f - 0.12f * shape) * saw1 + 0.38f * saw2 + (0.08f + 0.28f * shape) * sawOct;
                        break;
                    }
                    case 5: { // Hard Sync Lead
                        const float syncPh = std::fmod(ph * (1.0f + 2.0f * shape), 1.0f);
                        wave = ((2.0f * syncPh - 1.0f) - polyBlep(syncPh, std::min(0.48f, dt * (1.0f + 2.0f * shape)))) * 0.85f;
                        break;
                    }
                    case 6: { // Velvet Analog Pad
                        const float saw = (2.0f * ph - 1.0f) - polyBlep(ph, dt);
                        const float subTri = 2.0f * std::abs(2.0f * (phSub - 0.5f)) - 1.0f;
                        wave = (saw * (0.60f + 0.30f * shape) + subTri * (0.20f + 0.12f * (1.0f - shape))) * 0.75f;
                        break;
                    }
                    default:
                        wave = (2.0f * ph - 1.0f) - polyBlep(ph, dt);
                        break;
                }

                sigL += wave * unisonPanL_[idx] * normGain;
                sigR += wave * unisonPanR_[idx] * normGain;
            }
            break;
        }

        case EngineType::EvolvingWavetable: {
            const auto& bank = getWavetableBank();
            const float twoPi = 2.0f * std::numbers::pi_v<float>;
            const int tableIdx = std::clamp(params_.modelIndex, 0, kWtTables - 1);
            const float effDetune = std::max(detuneAmt, scatterAmt);

            const float scanRate = (0.12f + 0.45f * params_.rate) * (0.7f + 0.6f * effDetune);
            texGrainPhase_ += scanRate * invSr;
            if (texGrainPhase_ >= 1.0f) texGrainPhase_ -= 1.0f;

            const int activeUnison = (detuneAmt > 0.02f || scatterAmt > 0.02f) ? kMaxUnison : 1;
            const float normGain = (activeUnison > 1) ? (0.85f / std::sqrt(static_cast<float>(activeUnison))) : 1.0f;

            for (int u = 0; u < activeUnison; ++u) {
                const auto idx = static_cast<std::size_t>(u);
                const float uFreq = std::min(sr * 0.46f, slotFreq * unisonPitchMultipliers_[idx]), dt = uFreq * invSr;

                float ph = phases_[idx] + dt;
                if (ph >= 1.0f) ph -= 1.0f;
                phases_[idx] = ph;

                const float uScan = std::sin(twoPi * (texGrainPhase_ + static_cast<float>(u) * 0.21f)) *
                                    (0.18f * (0.25f + 0.75f * effDetune));
                const float fPos = std::clamp(shape * 3.0f + uScan, 0.0f, 3.0f);
                const int frame0 = std::clamp(static_cast<int>(fPos), 0, kWtFrames - 2);
                const int frame1 = frame0 + 1;
                const float frameFrac = fPos - static_cast<float>(frame0);

                float phWarped = ph;
                const float warpAmt = std::clamp(params_.warp, 0.0f, 1.0f);
                if (warpAmt > 0.01f) {
                    phWarped = (phWarped < 0.5f)
                        ? (0.5f * std::pow(phWarped * 2.0f, 1.0f + warpAmt * 2.2f))
                        : (1.0f - 0.5f * std::pow((1.0f - phWarped) * 2.0f, 1.0f + warpAmt * 2.2f));
                }

                const float tableIdxF = phWarped * static_cast<float>(kWtSize);
                const int s0 = static_cast<int>(tableIdxF) & (kWtSize - 1), s1 = (s0 + 1) & (kWtSize - 1);
                const float sFrac = tableIdxF - std::floor(tableIdxF);

                const auto& activeTable = bank.tables[static_cast<std::size_t>(tableIdx)];
                const float v00 = activeTable[static_cast<std::size_t>(frame0)][static_cast<std::size_t>(s0)];
                const float v01 = activeTable[static_cast<std::size_t>(frame0)][static_cast<std::size_t>(s1)];
                const float v10 = activeTable[static_cast<std::size_t>(frame1)][static_cast<std::size_t>(s0)];
                const float v11 = activeTable[static_cast<std::size_t>(frame1)][static_cast<std::size_t>(s1)];

                const float w0 = v00 + sFrac * (v01 - v00), w1 = v10 + sFrac * (v11 - v10);
                const float wave = w0 + frameFrac * (w1 - w0);

                sigL += wave * unisonPanL_[idx] * normGain;
                sigR += wave * unisonPanR_[idx] * normGain;
            }
            break;
        }

        case EngineType::OrganicTexture: processOrganicTexture(slotFreq, shape, invSr, sigL, sigR); break;
        case EngineType::PhysicalPluckModal: processPhysicalPluck(slotFreq, shape, invSr, sigL, sigR); break;
        case EngineType::SubOscillator: processSubOscillator(slotFreq, shape, invSr, sigL, sigR); break;
        case EngineType::NoiseTransient: processNoiseTransient(shape, invSr, sigL, sigR); break;
        case EngineType::SamplePlayback: processSamplePlayback(slotFreq, sigL, sigR); break;
    }

    const float uPan = std::clamp(params_.pan, -1.0f, 1.0f);
    const float panAngle = (uPan + 1.0f) * 0.25f * std::numbers::pi_v<float>;
    outLeft  += sigL * (std::cos(panAngle) * 1.41421356f) * effLevel;
    outRight += sigR * (std::sin(panAngle) * 1.41421356f) * effLevel;
}

} // namespace nova::dsp
