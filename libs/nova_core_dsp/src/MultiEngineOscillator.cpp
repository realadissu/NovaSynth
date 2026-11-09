#include "nova_dsp/oscillators/MultiEngineOscillator.h"
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
    lastFormantShape_ = -999.0f; lastFormantModel_ = -1;
    (void)getWavetableBank(); (void)getTextureSourceBank();
    updateCoefficients(); reset();
}

void MultiEngineOscillator::updateCoefficients() noexcept {
    const float totalSemi = static_cast<float>(params_.octaveOffset * 12 + params_.semitoneOffset) + (params_.fineCents * 0.01f);
    basePitchRatio_ = std::pow(2.0f, totalSemi / 12.0f);
    const float detuneAmt = std::clamp(params_.detune, 0.0f, 1.0f);
    pluckDelayRatioR_ = std::pow(2.0f, (detuneAmt * 14.0f) / 1200.0f);

    for (int u = 0; u < kMaxUnison; ++u) {
        const auto idx = static_cast<std::size_t>(u);
        const float cents = (kUnisonDetuneFactors[idx] * 13.5f + driftOffsetsCents_[idx]) * detuneAmt;
        unisonPitchMultipliers_[idx] = std::pow(2.0f, cents / 1200.0f);
        const float uPan = std::clamp(kUnisonPanPositions[idx] * std::sqrt(detuneAmt), -1.0f, 1.0f);
        const float panAngle = (uPan + 1.0f) * 0.25f * std::numbers::pi_v<float>;
        unisonPanL_[idx] = std::cos(panAngle);
        unisonPanR_[idx] = std::sin(panAngle);
    }
}

void MultiEngineOscillator::reset() noexcept {
    rng_ = 0x9E3779B9u;
    shapeSmoother_.reset(params_.shapeMorph);
    phases_.fill(0.0f);
    driftOffsetsCents_.fill(0.0f);
    texFilterState1_ = 0.0f; texFilterState2_ = 0.0f;
    texGrainPhase_ = 0.0f;   texGrainPhase2_ = 0.0f;
    flutterPhase_ = 0.0f;    rainScatterTimer_ = 0.0f;
    for (std::size_t i = 0; i < formantFiltersL_.size(); ++i) {
        formantFiltersL_[i].reset();
        formantFiltersR_[i].reset();
    }
    lastFormantShape_ = -999.0f; lastFormantModel_ = -1;
    for (int g = 0; g < kNumGrains; ++g) {
        auto& gr = grains_[static_cast<std::size_t>(g)];
        gr.pos = static_cast<float>(g * (4096 / kNumGrains));
        gr.rate = 1.0f; gr.lengthSamples = 2400.0f;
        gr.age = static_cast<float>(g) * (2400.0f / static_cast<float>(kNumGrains));
        gr.panL = 0.7071f; gr.panR = 0.7071f; gr.amp = 1.0f;
    }
    std::fill(stringDelayL_.begin(), stringDelayL_.end(), 0.0f);
    std::fill(stringDelayR_.begin(), stringDelayR_.end(), 0.0f);
    stringWritePos_ = 0; stringLpStateL_ = 0.0f; stringLpStateR_ = 0.0f;
    modalPhases_.fill(0.0f); modalAmplitudes_.fill(0.0f); modalDecays_.fill(0.999f);
    samplePlayhead_ = 0.0; sampleDirection_ = 1.0; sampleFinished_ = false;
    lastPitchModSemitones_ = -999.0f; cachedPitchModRatio_ = 1.0f;
}

void MultiEngineOscillator::triggerNote(float baseFrequencyHz, float velocity, uint32_t seedOffset) noexcept {
    shapeSmoother_.reset(params_.shapeMorph);
    rng_ ^= (seedOffset * 747796405u + 2891336453u);

    if (params_.engine == EngineType::WarmAnalog ||
        params_.engine == EngineType::EvolvingWavetable ||
        params_.engine == EngineType::OrganicTexture) {
        rng_ = rng_ * 1664525u + 1013904223u;
        phases_[0] = static_cast<float>(rng_ & 0xFFFFu) / 65536.0f;
    } else {
        phases_[0] = 0.0f;
    }
    driftOffsetsCents_[0] = 0.0f;

    for (int u = 1; u < kMaxUnison; ++u) {
        rng_ = rng_ * 1664525u + 1013904223u;
        phases_[static_cast<std::size_t>(u)] = static_cast<float>(rng_ & 0xFFFFu) / 65536.0f;
        rng_ = rng_ * 1664525u + 1013904223u;
        driftOffsetsCents_[static_cast<std::size_t>(u)] = ((static_cast<float>(rng_ & 0xFFFFu) / 32767.5f) - 1.0f) * 1.1f;
    }
    updateCoefficients();

    texGrainPhase_ = 0.0f; texGrainPhase2_ = 0.25f; rainScatterTimer_ = 0.0f;

    if (params_.engine == EngineType::OrganicTexture) {
        constexpr std::array<float, 5> kRainHarmonics = { 1.0f, 1.5f, 2.0f, 3.0f, 4.0f };
        for (int g = 0; g < kNumGrains; ++g) {
            auto& gr = grains_[static_cast<std::size_t>(g)];
            rng_ = rng_ * 1664525u + 1013904223u; const float r1 = static_cast<float>(rng_ & 0xFFFFu) / 65536.0f;
            rng_ = rng_ * 1664525u + 1013904223u; const float r2 = static_cast<float>(rng_ & 0xFFFFu) / 65536.0f;
            rng_ = rng_ * 1664525u + 1013904223u; const float r3 = static_cast<float>(rng_ & 0xFFFFu) / 65536.0f;

            const float durSec = 0.035f + 0.35f * params_.duration;
            gr.lengthSamples = std::clamp(durSec * static_cast<float>(sampleRate_) * (0.80f + 0.40f * r1), 400.0f, 24000.0f);
            gr.age = gr.lengthSamples * (static_cast<float>(g) / static_cast<float>(kNumGrains));
            gr.pos = std::fmod(r2 * 4096.0f, 4096.0f);

            const float uPan = std::clamp((static_cast<float>(g) / static_cast<float>(kNumGrains - 1) * 2.0f - 1.0f) * params_.spread, -1.0f, 1.0f);
            const float panAngle = (uPan + 1.0f) * 0.25f * std::numbers::pi_v<float>;
            gr.panL = std::cos(panAngle); gr.panR = std::sin(panAngle); gr.amp = 1.0f;

            switch (params_.modelIndex) {
                case 0: gr.rate = (g % 4 == 0) ? 2.0f : ((g % 4 == 1) ? 0.5f : (1.0f + ((r3 * 2.0f - 1.0f) * 0.004f))); break;
                case 1: gr.rate = (g % 4 == 0) ? 0.5f : (1.0f + ((r3 * 2.0f - 1.0f) * 0.005f)); break;
                case 2: gr.rate = (g % 4 == 0) ? 1.5f : ((g % 4 == 1) ? 2.0f : 1.0f); break;
                case 3: gr.rate = kRainHarmonics[static_cast<std::size_t>(g % 5)]; break;
                case 4: default: gr.rate = (g % 3 == 0) ? 2.0f : ((g % 3 == 1) ? 1.5f : 1.0f); break;
            }
        }
    }

    if (params_.engine == EngineType::SamplePlayback || params_.sourceType == 4) {
        const auto& smp = SampleBank::getSample(params_.modelIndex);
        activeSampleZone_ = nullptr;
        if (smp.zones != nullptr && smp.numZones > 0) {
            const float targetFreq = std::clamp(baseFrequencyHz * basePitchRatio_, 20.0f, 15000.0f);
            float bestDist = 1e9f; int bestIdx = 0;
            for (int z = 0; z < smp.numZones; ++z) {
                const float dist = std::abs(std::log2(targetFreq / smp.zones[z].rootFrequencyHz));
                if (dist < bestDist) { bestDist = dist; bestIdx = z; }
            }
            activeSampleZone_ = &smp.zones[bestIdx];
            const int numS = activeSampleZone_->numSamples;
            const double sMorph = static_cast<double>(std::clamp(params_.shapeMorph, 0.0f, 1.0f));
            const double lStart = static_cast<double>(std::clamp(params_.sampleLoopStart, 0.0f, 0.98f)) * (numS - 4);
            const double lEnd = static_cast<double>(std::clamp(params_.sampleLoopEnd, 0.02f, 1.0f)) * (numS - 4);
            const double effectiveEnd = std::max(lStart + 100.0, lEnd);

            if (params_.sampleLoopMode == 3) {
                sampleDirection_ = -1.0;
                samplePlayhead_ = (sMorph > 0.001) ? std::clamp(effectiveEnd - sMorph * (effectiveEnd - lStart), lStart, effectiveEnd) : effectiveEnd;
            } else {
                sampleDirection_ = 1.0;
                samplePlayhead_ = sMorph * static_cast<double>(numS - 4);
            }
        } else {
            samplePlayhead_ = 0.0; sampleDirection_ = 1.0;
        }
        sampleFinished_ = false;
    }

    const float freq = std::clamp(baseFrequencyHz * basePitchRatio_, 25.0f, 4000.0f);
    const int periodSamples = std::clamp(static_cast<int>(sampleRate_ / freq), 8, kMaxStringDelay - 8);
    const float velScale = std::clamp(0.35f + 0.65f * velocity, 0.1f, 1.0f), pluckHardness = std::clamp(params_.shapeMorph, 0.05f, 0.95f);
    float smoothExcL = 0.0f;

    switch (params_.modelIndex) {
        case 0:
            for (int i = 0; i < periodSamples; ++i) {
                rng_ = rng_ * 1664525u + 1013904223u;
                const float nL = ((static_cast<float>(rng_ & 0xFFFFu) / 32767.5f) - 1.0f);
                const float win = std::sin(std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(periodSamples));
                smoothExcL += (0.35f + 0.55f * pluckHardness) * (nL - smoothExcL);
                const float body = std::sin(2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(periodSamples));
                stringDelayL_[static_cast<std::size_t>(i)] = (smoothExcL * 1.5f + body * 0.70f) * win * velScale;
                stringDelayR_[static_cast<std::size_t>(i)] = stringDelayL_[static_cast<std::size_t>(i)];
            }
            break;
        case 1:
            for (int i = 0; i < periodSamples; ++i) {
                rng_ = rng_ * 1664525u + 1013904223u;
                const float nL = ((static_cast<float>(rng_ & 0xFFFFu) / 32767.5f) - 1.0f);
                const float win = std::pow(std::sin(std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(periodSamples)), 1.8f);
                smoothExcL += (0.16f + 0.45f * pluckHardness) * (nL - smoothExcL);
                const float body = std::sin(std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(periodSamples));
                stringDelayL_[static_cast<std::size_t>(i)] = (smoothExcL * 0.85f + body * 1.35f) * win * velScale;
                stringDelayR_[static_cast<std::size_t>(i)] = stringDelayL_[static_cast<std::size_t>(i)];
            }
            break;
        case 2: {
            modalPhases_.fill(0.0f);
            modalAmplitudes_ = { 0.95f * velScale, (0.55f + 0.30f * pluckHardness) * velScale, (0.35f + 0.25f * pluckHardness) * velScale, (0.20f + 0.20f * pluckHardness) * velScale };
            const float srF = static_cast<float>(sampleRate_);
            modalDecays_ = { 1.0f - (12.0f / (srF * (0.8f + pluckHardness * 1.8f))), 1.0f - (28.0f / (srF * (0.5f + pluckHardness * 1.2f))),
                             1.0f - (55.0f / (srF * (0.3f + pluckHardness * 0.8f))), 1.0f - (90.0f / (srF * (0.2f + pluckHardness * 0.5f))) };
            for (int i = 0; i < periodSamples; ++i) {
                const float click = (i < 8) ? ((8 - i) * 0.12f * velScale) : 0.0f;
                stringDelayL_[static_cast<std::size_t>(i)] = click;
                stringDelayR_[static_cast<std::size_t>(i)] = click;
            }
            break;
        }
        case 3: default: {
            modalPhases_.fill(0.0f);
            modalAmplitudes_ = { 0.90f * velScale, 0.65f * velScale, (0.45f + 0.35f * pluckHardness) * velScale, (0.25f + 0.30f * pluckHardness) * velScale };
            const float srF = static_cast<float>(sampleRate_);
            modalDecays_ = { 1.0f - (4.0f / (srF * (1.5f + pluckHardness * 2.5f))), 1.0f - (9.0f / (srF * (1.2f + pluckHardness * 2.0f))),
                             1.0f - (18.0f / (srF * (0.8f + pluckHardness * 1.5f))), 1.0f - (35.0f / (srF * (0.5f + pluckHardness * 1.0f))) };
            for (int i = 0; i < periodSamples; ++i) {
                const float chimeClick = (i < 5) ? ((5 - i) * 0.15f * velScale) : 0.0f;
                stringDelayL_[static_cast<std::size_t>(i)] = chimeClick;
                stringDelayR_[static_cast<std::size_t>(i)] = chimeClick;
            }
            break;
        }
    }

    stringWritePos_ = 0; stringLpStateL_ = 0.0f; stringLpStateR_ = 0.0f;
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
    const float shape = std::clamp(shapeSmoother_.getNextValue() + shapeMod, 0.0f, 1.0f), detuneAmt = std::clamp(params_.detune, 0.0f, 1.0f);

    float sigL = 0.0f, sigR = 0.0f;

    switch (params_.engine) {
        case EngineType::WarmAnalog: {
            const int activeUnison = (detuneAmt > 0.02f) ? kMaxUnison : 1;
            constexpr float kUnisonNormGain = 0.85f / 2.2360679775f; // 0.85 / sqrt(5)
            const float normGain = (activeUnison > 1) ? kUnisonNormGain : 1.0f;

            for (int u = 0; u < activeUnison; ++u) {
                const auto idx = static_cast<std::size_t>(u);
                const float uFreq = std::min(sr * 0.46f, slotFreq * unisonPitchMultipliers_[idx]), dt = uFreq * invSr;

                float ph = phases_[idx] + dt;
                if (ph >= 1.0f) ph -= 1.0f;
                phases_[idx] = ph;

                float wave = 0.0f;
                switch (params_.modelIndex) {
                    case 0: { // Classic Saw
                        const float saw = (2.0f * ph - 1.0f) - polyBlep(ph, dt);
                        if (shape <= 0.001f) {
                            wave = saw;
                        } else {
                            const float phSub = std::fmod(ph * 0.5f, 1.0f);
                            wave = saw * (1.0f - shape * 0.35f) + ((2.0f * phSub - 1.0f) - polyBlep(phSub, dt * 0.5f)) * (shape * 0.35f);
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
                        const float ph2 = (ph + 0.381966f >= 1.0f) ? (ph + 0.381966f - 1.0f) : (ph + 0.381966f);
                        const float saw2 = (2.0f * ph2 - 1.0f) - polyBlep(ph2, dt);
                        const float phOct = std::fmod(ph * 2.0f, 1.0f);
                        const float sawOct = (2.0f * phOct - 1.0f) - polyBlep(phOct, std::min(0.48f, dt * 2.0f));
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
                        const float phSub = std::fmod(ph * 0.5f, 1.0f);
                        const float subTri = 2.0f * std::abs(2.0f * (phSub - 0.5f)) - 1.0f;
                        wave = std::tanh((saw * (0.60f + 0.30f * shape) + subTri * (0.20f + 0.12f * (1.0f - shape))) * 1.25f);
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

            const float scanRate = (0.12f + 0.45f * params_.rate) * (0.7f + 0.6f * detuneAmt);
            texGrainPhase_ += scanRate * invSr;
            if (texGrainPhase_ >= 1.0f) texGrainPhase_ -= 1.0f;

            const int activeUnison = (detuneAmt > 0.02f) ? kMaxUnison : 1;
            const float normGain = (activeUnison > 1) ? (0.85f / std::sqrt(static_cast<float>(activeUnison))) : 1.0f;

            for (int u = 0; u < activeUnison; ++u) {
                const auto idx = static_cast<std::size_t>(u);
                const float uFreq = std::min(sr * 0.46f, slotFreq * unisonPitchMultipliers_[idx]), dt = uFreq * invSr;

                float ph = phases_[idx] + dt;
                if (ph >= 1.0f) ph -= 1.0f;
                phases_[idx] = ph;

                const float uScan = std::sin(twoPi * (texGrainPhase_ + static_cast<float>(u) * 0.21f)) *
                                    (0.18f * (0.25f + 0.75f * detuneAmt));
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
