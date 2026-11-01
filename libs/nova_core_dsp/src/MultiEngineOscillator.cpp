#include "nova_dsp/oscillators/MultiEngineOscillator.h"
#include "nova_dsp/oscillators/SampleBank.h"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace nova::dsp {

namespace {

// Shared static 8-frame bandlimited wavetable (initialized once on first prepare)
struct SharedWavetableBank {
    std::array<std::array<float, 1024>, 8> frames{};

    SharedWavetableBank() {
        constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
        for (int i = 0; i < 1024; ++i) {
            const float ph = static_cast<float>(i) / 1024.0f;
            const float w = twoPi * ph;

            // Frame 0: Warm Fundamental + Soft 2nd/3rd
            frames[0][static_cast<std::size_t>(i)] =
                0.75f * std::sin(w) + 0.22f * std::sin(2.0f * w) + 0.08f * std::sin(3.0f * w);

            // Frame 1: Warm Organ Drawbars (1, 2, 3, 4, 6, 8)
            frames[1][static_cast<std::size_t>(i)] =
                0.50f * std::sin(w) + 0.28f * std::sin(2.0f * w) + 0.20f * std::sin(3.0f * w) +
                0.15f * std::sin(4.0f * w) + 0.10f * std::sin(6.0f * w) + 0.07f * std::sin(8.0f * w);

            // Frame 2: Silky Worship String (rich odd/even taper)
            float s2 = 0.0f;
            for (int h = 1; h <= 12; ++h) {
                s2 += (1.0f / static_cast<float>(h)) * std::exp(-0.08f * static_cast<float>(h)) *
                      std::sin(static_cast<float>(h) * w);
            }
            frames[2][static_cast<std::size_t>(i)] = s2 * 0.55f;

            // Frame 3: Vocal Formant Choir ("Ah/Oh" spectral peaks at harmonics 3, 4, 7, 8)
            frames[3][static_cast<std::size_t>(i)] =
                0.45f * std::sin(w) + 0.15f * std::sin(2.0f * w) + 0.38f * std::sin(3.0f * w) +
                0.32f * std::sin(4.0f * w) + 0.22f * std::sin(7.0f * w) + 0.16f * std::sin(8.0f * w);

            // Frame 4: Fifth & Octave Harmonic Pad (1, 2, 3, 6, 9, 12)
            frames[4][static_cast<std::size_t>(i)] =
                0.52f * std::sin(w) + 0.30f * std::sin(2.0f * w) + 0.34f * std::sin(3.0f * w) +
                0.20f * std::sin(6.0f * w) + 0.12f * std::sin(9.0f * w) + 0.08f * std::sin(12.0f * w);

            // Frame 5: Glass Bell Harmonics
            frames[5][static_cast<std::size_t>(i)] =
                0.48f * std::sin(w) + 0.35f * std::sin(3.0f * w) + 0.28f * std::sin(5.0f * w) +
                0.22f * std::sin(9.0f * w) + 0.16f * std::sin(11.0f * w) + 0.12f * std::sin(15.0f * w);

            // Frame 6: Warm Brass Swell (harmonics 1..16 with gentle brilliance)
            float s6 = 0.0f;
            for (int h = 1; h <= 16; ++h) {
                s6 += (1.0f / std::pow(static_cast<float>(h), 0.85f)) * std::sin(static_cast<float>(h) * w);
            }
            frames[6][static_cast<std::size_t>(i)] = s6 * 0.42f;

            // Frame 7: Celestial Shimmer Air (strong upper octave/fifth partials)
            frames[7][static_cast<std::size_t>(i)] =
                0.40f * std::sin(w) + 0.32f * std::sin(2.0f * w) + 0.26f * std::sin(4.0f * w) +
                0.24f * std::sin(6.0f * w) + 0.20f * std::sin(8.0f * w) + 0.18f * std::sin(12.0f * w) +
                0.14f * std::sin(16.0f * w);
        }
    }
};

const SharedWavetableBank& getWavetableBank() {
    static const SharedWavetableBank bank;
    return bank;
}

constexpr std::array<float, 5> kUnisonDetuneFactors = { 0.0f, -0.42f, 0.42f, -0.88f, 0.88f };
constexpr std::array<float, 5> kUnisonPanPositions  = { 0.0f, -0.55f, 0.55f, -0.90f, 0.90f };

} // namespace

float MultiEngineOscillator::polyBlep(float phase, float phaseInc) noexcept {
    if (phase < phaseInc) {
        const float t = phase / phaseInc;
        return t + t - t * t - 1.0f;
    }
    if (phase > 1.0f - phaseInc) {
        const float t = (phase - 1.0f) / phaseInc;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

void MultiEngineOscillator::prepare(double sampleRate) {
    sampleRate_ = std::max(8000.0, sampleRate);
    shapeSmoother_.prepare(sampleRate_, 20.0f);
    shapeSmoother_.reset(params_.shapeMorph);
    (void)getWavetableBank();
    updateCoefficients();
    reset();
}

void MultiEngineOscillator::updateCoefficients() noexcept {
    const float totalSemi = static_cast<float>(params_.octaveOffset * 12 + params_.semitoneOffset)
                          + (params_.fineCents * 0.01f);
    basePitchRatio_ = std::pow(2.0f, totalSemi / 12.0f);

    const float detuneAmt = std::clamp(params_.detune, 0.0f, 1.0f);
    pluckDelayRatioR_ = std::pow(2.0f, (detuneAmt * 14.0f) / 1200.0f);

    for (int u = 0; u < kMaxUnison; ++u) {
        const auto idx = static_cast<std::size_t>(u);
        const float cents = kUnisonDetuneFactors[idx] * detuneAmt * 34.0f + driftOffsetsCents_[idx];
        unisonPitchMultipliers_[idx] = std::pow(2.0f, cents / 1200.0f);

        const float uPan = std::clamp(params_.pan + kUnisonPanPositions[idx] * detuneAmt, -1.0f, 1.0f);
        const float panAngle = (uPan + 1.0f) * 0.25f * std::numbers::pi_v<float>;
        unisonPanL_[idx] = std::cos(panAngle);
        unisonPanR_[idx] = std::sin(panAngle);
    }
}

void MultiEngineOscillator::reset() noexcept {
    shapeSmoother_.reset(params_.shapeMorph);
    phases_.fill(0.0f);
    driftOffsetsCents_.fill(0.0f);
    texFilterState1_ = 0.0f;
    texFilterState2_ = 0.0f;
    texGrainPhase_ = 0.0f;
    texGrainPhase2_ = 0.0f;
    flutterPhase_ = 0.0f;
    rainScatterTimer_ = 0.0f;
    stringDelayL_.fill(0.0f);
    stringDelayR_.fill(0.0f);
    stringWritePos_ = 0;
    stringLpStateL_ = 0.0f;
    stringLpStateR_ = 0.0f;
    modalPhases_.fill(0.0f);
    modalAmplitudes_.fill(0.0f);
    modalDecays_.fill(0.999f);
    samplePlayhead_ = 0.0;
    sampleFinished_ = false;
}

void MultiEngineOscillator::triggerNote(float baseFrequencyHz, float velocity, uint32_t seedOffset) noexcept {
    shapeSmoother_.reset(params_.shapeMorph);
    rng_ ^= (seedOffset * 747796405u + 2891336453u);

    // Main fundamental voice (index 0) starts strictly at zero-crossing to prevent pops/clicks on note triggers
    phases_[0] = 0.0f;
    driftOffsetsCents_[0] = 0.0f;

    // Remaining unison voices (indices 1..kMaxUnison-1) receive randomized phases for rich ensemble decorrelation
    for (int u = 1; u < kMaxUnison; ++u) {
        rng_ = rng_ * 1664525u + 1013904223u;
        phases_[static_cast<std::size_t>(u)] = static_cast<float>(rng_ & 0xFFFFu) / 65536.0f;

        rng_ = rng_ * 1664525u + 1013904223u;
        driftOffsetsCents_[static_cast<std::size_t>(u)] =
            ((static_cast<float>(rng_ & 0xFFFFu) / 32767.5f) - 1.0f) * 4.5f; // +/- 4.5 cents vintage drift
    }
    updateCoefficients();

    texGrainPhase_ = 0.0f;
    texGrainPhase2_ = 0.25f;
    rainScatterTimer_ = 0.0f;

    if (params_.engine == EngineType::SamplePlayback || params_.sourceType == 4) {
        const auto& smp = SampleBank::getSample(params_.modelIndex);
        activeSampleZone_ = nullptr;
        if (smp.zones != nullptr && smp.numZones > 0) {
            const float targetFreq = std::clamp(baseFrequencyHz * basePitchRatio_, 20.0f, 15000.0f);
            float bestDist = 1e9f;
            int bestIdx = 0;
            for (int z = 0; z < smp.numZones; ++z) {
                const float ratio = targetFreq / smp.zones[z].rootFrequencyHz;
                const float dist = std::abs(std::log2(ratio));
                if (dist < bestDist) {
                    bestDist = dist;
                    bestIdx = z;
                }
            }
            activeSampleZone_ = &smp.zones[bestIdx];
            const double startOffset = static_cast<double>(params_.shapeMorph) * std::min(static_cast<double>(activeSampleZone_->numSamples) * 0.20, 5000.0);
            samplePlayhead_ = startOffset;
        } else {
            samplePlayhead_ = 0.0;
        }
        sampleFinished_ = false;
    }

    // Populate Karplus-Strong / Modal waveguide excitation burst for PhysicalPluckModal
    const float freq = std::clamp(baseFrequencyHz * basePitchRatio_, 25.0f, 4000.0f);
    const int periodSamples = std::clamp(static_cast<int>(sampleRate_ / freq), 8, kMaxStringDelay - 8);

    const float velScale = std::clamp(0.35f + 0.65f * velocity, 0.1f, 1.0f);
    const float pluckHardness = std::clamp(params_.shapeMorph, 0.05f, 0.95f);
    float smoothExcL = 0.0f;

    switch (params_.modelIndex) {
        case 0: // Acoustic Steel String (sharp pick, bright click + full spectrum)
            for (int i = 0; i < periodSamples; ++i) {
                rng_ = rng_ * 1664525u + 1013904223u;
                const float nL = ((static_cast<float>(rng_ & 0xFFFFu) / 32767.5f) - 1.0f);
                const float win = std::sin(std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(periodSamples));
                smoothExcL += (0.25f + 0.70f * pluckHardness) * (nL - smoothExcL);
                const float body = std::sin(2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(periodSamples));
                stringDelayL_[static_cast<std::size_t>(i)] = (smoothExcL * 0.85f + body * 0.35f) * win * velScale;
                stringDelayR_[static_cast<std::size_t>(i)] = stringDelayL_[static_cast<std::size_t>(i)];
            }
            break;

        case 1: // Nylon Worship Harp (soft finger pad, warm lowpass impulse)
            for (int i = 0; i < periodSamples; ++i) {
                rng_ = rng_ * 1664525u + 1013904223u;
                const float nL = ((static_cast<float>(rng_ & 0xFFFFu) / 32767.5f) - 1.0f);
                const float win = std::pow(std::sin(std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(periodSamples)), 1.8f);
                smoothExcL += (0.08f + 0.32f * pluckHardness) * (nL - smoothExcL);
                const float body = std::sin(std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(periodSamples));
                stringDelayL_[static_cast<std::size_t>(i)] = (smoothExcL * 0.45f + body * 0.80f) * win * velScale;
                stringDelayR_[static_cast<std::size_t>(i)] = stringDelayL_[static_cast<std::size_t>(i)];
            }
            break;

        case 2: // Kalimba / Music Box (metal tine strike + inharmonic modal resonators)
            modalPhases_.fill(0.0f);
            modalAmplitudes_[0] = 0.95f * velScale;
            modalAmplitudes_[1] = (0.55f + 0.30f * pluckHardness) * velScale;
            modalAmplitudes_[2] = (0.35f + 0.25f * pluckHardness) * velScale;
            modalAmplitudes_[3] = (0.20f + 0.20f * pluckHardness) * velScale;
            modalDecays_[0] = 1.0f - (12.0f / static_cast<float>(sampleRate_ * (0.8f + pluckHardness * 1.8f)));
            modalDecays_[1] = 1.0f - (28.0f / static_cast<float>(sampleRate_ * (0.5f + pluckHardness * 1.2f)));
            modalDecays_[2] = 1.0f - (55.0f / static_cast<float>(sampleRate_ * (0.3f + pluckHardness * 0.8f)));
            modalDecays_[3] = 1.0f - (90.0f / static_cast<float>(sampleRate_ * (0.2f + pluckHardness * 0.5f)));

            for (int i = 0; i < periodSamples; ++i) {
                const float click = (i < 8) ? ((8 - i) * 0.12f * velScale) : 0.0f;
                stringDelayL_[static_cast<std::size_t>(i)] = click;
                stringDelayR_[static_cast<std::size_t>(i)] = click;
            }
            break;

        case 3: // Glass Mallet Resonance (crystal chime modes 1.0, 2.0, 3.16, 4.52)
        default:
            modalPhases_.fill(0.0f);
            modalAmplitudes_[0] = 0.90f * velScale;
            modalAmplitudes_[1] = 0.65f * velScale;
            modalAmplitudes_[2] = (0.45f + 0.35f * pluckHardness) * velScale;
            modalAmplitudes_[3] = (0.25f + 0.30f * pluckHardness) * velScale;
            modalDecays_[0] = 1.0f - (4.0f / static_cast<float>(sampleRate_ * (1.5f + pluckHardness * 2.5f)));
            modalDecays_[1] = 1.0f - (9.0f / static_cast<float>(sampleRate_ * (1.2f + pluckHardness * 2.0f)));
            modalDecays_[2] = 1.0f - (18.0f / static_cast<float>(sampleRate_ * (0.8f + pluckHardness * 1.5f)));
            modalDecays_[3] = 1.0f - (35.0f / static_cast<float>(sampleRate_ * (0.5f + pluckHardness * 1.0f)));

            for (int i = 0; i < periodSamples; ++i) {
                const float chimeClick = (i < 5) ? ((5 - i) * 0.15f * velScale) : 0.0f;
                stringDelayL_[static_cast<std::size_t>(i)] = chimeClick;
                stringDelayR_[static_cast<std::size_t>(i)] = chimeClick;
            }
            break;
    }

    stringWritePos_ = 0;
    stringLpStateL_ = 0.0f;
    stringLpStateR_ = 0.0f;
}

void MultiEngineOscillator::processSample(float baseFrequencyHz,
                                          float pitchModSemitones,
                                          float shapeMod,
                                          float& outLeft,
                                          float& outRight) noexcept {
    if (!params_.enabled || params_.level <= 0.0001f) {
        return;
    }

    const float sr = static_cast<float>(sampleRate_);
    const float invSr = 1.0f / sr;
    const float pitchModRatio = (pitchModSemitones == 0.0f) ? 1.0f : std::exp2(pitchModSemitones * (1.0f / 12.0f));
    const float slotFreq = std::clamp(baseFrequencyHz * basePitchRatio_ * pitchModRatio,
                                      20.0f, sr * 0.46f);
    shapeSmoother_.setTargetValue(params_.shapeMorph);
    const float smoothedShape = shapeSmoother_.getNextValue();
    const float shape = std::clamp(smoothedShape + shapeMod, 0.0f, 1.0f);
    const float detuneAmt = std::clamp(params_.detune, 0.0f, 1.0f);

    float sigL = 0.0f;
    float sigR = 0.0f;

    switch (params_.engine) {
        case EngineType::WarmAnalog: {
            const int activeUnison = (detuneAmt > 0.02f) ? kMaxUnison : 1;
            // Power-conserving RMS normalization avoids master clipping when unison is cranked
            const float normGain = (activeUnison > 1) ? (0.85f / std::sqrt(static_cast<float>(activeUnison))) : 1.0f;

            for (int u = 0; u < activeUnison; ++u) {
                const auto idx = static_cast<std::size_t>(u);
                const float uFreq = std::min(sr * 0.46f, slotFreq * unisonPitchMultipliers_[idx]);
                const float dt = uFreq * invSr;

                float ph = phases_[idx] + dt;
                if (ph >= 1.0f) ph -= 1.0f;
                phases_[idx] = ph;

                float wave = 0.0f;
                switch (params_.modelIndex) {
                    case 0: // Classic Saw
                        {
                            const float saw = (2.0f * ph - 1.0f) - polyBlep(ph, dt);
                            if (shape <= 0.001f) {
                                wave = saw;
                            } else {
                                const float phSub = std::fmod(ph * 0.5f, 1.0f);
                                const float subSaw = (2.0f * phSub - 1.0f) - polyBlep(phSub, dt * 0.5f);
                                wave = saw * (1.0f - shape * 0.35f) + subSaw * (shape * 0.35f);
                            }
                        }
                        break;
                    case 1: // Pulse / Square
                        {
                            const float pw = 0.5f - 0.38f * shape;
                            float pulse = (ph < pw ? 1.0f : -1.0f);
                            pulse += polyBlep(ph, dt);
                            float shiftedPh = ph + (1.0f - pw);
                            if (shiftedPh >= 1.0f) shiftedPh -= 1.0f;
                            pulse -= polyBlep(shiftedPh, dt);
                            wave = pulse;
                        }
                        break;
                    case 2: // Warm Triangle
                        {
                            const float tri = 2.0f * std::abs(2.0f * (ph - 0.5f)) - 1.0f;
                            const float sat = std::tanh(tri * (1.0f + 2.5f * shape));
                            wave = tri * (1.0f - shape * 0.70f) + sat * (shape * 0.70f);
                        }
                        break;
                    case 3: // Mellow Sine
                        {
                            const float s = std::sin(2.0f * std::numbers::pi_v<float> * ph);
                            if (shape <= 0.001f) {
                                wave = s;
                            } else {
                                // West-Coast musical wavefolding & warm tube saturation
                                const float drive = 1.0f + shape * 3.2f;
                                const float folded = std::sin(s * drive * (0.5f * std::numbers::pi_v<float>));
                                const float harmonic2 = std::sin(4.0f * std::numbers::pi_v<float> * ph);
                                wave = (1.0f - shape * 0.45f) * s + (shape * 0.45f) * folded + (shape * 0.15f) * harmonic2;
                            }
                        }
                        break;
                    case 4: // Super Saw Multi
                        {
                            const float saw1 = (2.0f * ph - 1.0f) - polyBlep(ph, dt);
                            const float offset = 0.25f + 0.25f * shape;
                            const float ph2 = (ph + offset >= 1.0f) ? (ph + offset - 1.0f) : (ph + offset);
                            const float saw2 = (2.0f * ph2 - 1.0f) - polyBlep(ph2, dt);
                            const float ph3 = (ph + 2.0f * offset >= 1.0f) ? (ph + 2.0f * offset - 1.0f) : (ph + 2.0f * offset);
                            const float saw3 = (2.0f * ph3 - 1.0f) - polyBlep(ph3, dt);
                            wave = 0.45f * saw1 + 0.35f * saw2 + 0.20f * saw3;
                        }
                        break;
                    case 5: // Hard Sync Lead
                        {
                            const float syncFactor = 1.0f + 2.0f * shape;
                            const float syncPh = std::fmod(ph * syncFactor, 1.0f);
                            const float dtSync = dt * syncFactor;
                            const float syncSaw = (2.0f * syncPh - 1.0f) - polyBlep(syncPh, std::min(0.48f, dtSync));
                            wave = syncSaw * 0.85f;
                        }
                        break;
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
            const int baseFrame = std::clamp(params_.modelIndex, 0, kWtFrames - 1);
            const float fPos = std::clamp(static_cast<float>(baseFrame) + shape * 1.5f, 0.0f, static_cast<float>(kWtFrames - 1));
            const int frame0 = std::clamp(static_cast<int>(fPos), 0, kWtFrames - 2);
            const int frame1 = frame0 + 1;
            const float frameFrac = fPos - static_cast<float>(frame0);

            const int activeUnison = (detuneAmt > 0.02f) ? 3 : 1;
            const float normGain = (activeUnison > 1) ? (0.85f / std::sqrt(static_cast<float>(activeUnison))) : 1.0f;

            for (int u = 0; u < activeUnison; ++u) {
                const auto idx = static_cast<std::size_t>(u);
                const float uFreq = std::min(sr * 0.46f, slotFreq * unisonPitchMultipliers_[idx]);
                const float dt = uFreq * invSr;

                float ph = phases_[idx] + dt;
                if (ph >= 1.0f) ph -= 1.0f;
                phases_[idx] = ph;

                const float tableIdxF = ph * static_cast<float>(kWtSize);
                const int s0 = static_cast<int>(tableIdxF) & (kWtSize - 1);
                const int s1 = (s0 + 1) & (kWtSize - 1);
                const float sFrac = tableIdxF - std::floor(tableIdxF);

                const float v00 = bank.frames[static_cast<std::size_t>(frame0)][static_cast<std::size_t>(s0)];
                const float v01 = bank.frames[static_cast<std::size_t>(frame0)][static_cast<std::size_t>(s1)];
                const float v10 = bank.frames[static_cast<std::size_t>(frame1)][static_cast<std::size_t>(s0)];
                const float v11 = bank.frames[static_cast<std::size_t>(frame1)][static_cast<std::size_t>(s1)];

                const float w0 = v00 + sFrac * (v01 - v00);
                const float w1 = v10 + sFrac * (v11 - v10);
                const float wave = w0 + frameFrac * (w1 - w0);

                sigL += wave * unisonPanL_[idx] * normGain;
                sigR += wave * unisonPanR_[idx] * normGain;
            }
            break;
        }

        case EngineType::OrganicTexture: {
            const float twoPi = 2.0f * std::numbers::pi_v<float>;
            const float dt = slotFreq * invSr;
            phases_[0] += dt;
            if (phases_[0] >= 1.0f) phases_[0] -= 1.0f;

            // Texture grain rate from parameters (rate knob or detune)
            const float grainRate = (0.5f + 3.0f * params_.rate) * (0.8f + 1.2f * detuneAmt);
            texGrainPhase_ += grainRate * invSr;
            if (texGrainPhase_ >= 1.0f) texGrainPhase_ -= 1.0f;
            texGrainPhase2_ += (grainRate * 1.414f) * invSr;
            if (texGrainPhase2_ >= 1.0f) texGrainPhase2_ -= 1.0f;

            rng_ = rng_ * 1664525u + 1013904223u;
            const float noiseSample = ((static_cast<float>(rng_ & 0xFFFFu) / 32767.5f) - 1.0f);

            float texL = 0.0f;
            float texR = 0.0f;

            switch (params_.modelIndex) {
                case 0: // Ambient Choir Breath (Dual-formant vocal choir + breathing throat air)
                    {
                        const float f1 = std::sin(twoPi * phases_[0]);
                        const float f2 = std::sin(twoPi * phases_[0] * 2.0f);
                        const float f3 = std::sin(twoPi * phases_[0] * 3.0f);
                        const float f4 = std::sin(twoPi * phases_[0] * 4.0f);

                        const float choirVoice = f1 * 0.50f + f2 * (0.35f + 0.15f * shape) +
                                                 f3 * (0.18f + 0.20f * (1.0f - shape)) + f4 * 0.12f;

                        const float breathEnv = 0.65f + 0.35f * std::sin(twoPi * texGrainPhase_);
                        const float cutoff = std::clamp((900.0f + 2800.0f * shape) * invSr * 2.0f, 0.02f, 0.42f);
                        texFilterState1_ += cutoff * (noiseSample * 0.28f - texFilterState1_ - 0.30f * texFilterState2_);
                        texFilterState2_ += cutoff * texFilterState1_;

                        const float airLvl = 0.08f + 0.18f * shape;
                        texL = (choirVoice * 0.82f + texFilterState1_ * airLvl) * breathEnv;
                        texR = (choirVoice * 0.82f - texFilterState1_ * airLvl) * breathEnv;
                    }
                    break;

                case 1: // Bowed Cello Texture (Rosin friction, stick-slip rasp & wooden cavity)
                    {
                        const float ph = phases_[0];
                        const float rawSaw = 2.0f * ph - 1.0f;
                        const float rosinBite = rawSaw - 0.22f * (rawSaw * rawSaw * rawSaw);

                        const float bowPressure = 0.70f + 0.30f * std::sin(twoPi * texGrainPhase_);
                        const float bowScratch = noiseSample * (0.10f + 0.20f * shape);

                        const float bodyCutoff = std::clamp((220.0f + 350.0f * shape) * invSr * 2.0f, 0.01f, 0.35f);
                        texFilterState1_ += bodyCutoff * ((rosinBite + bowScratch) - texFilterState1_ - 0.55f * texFilterState2_);
                        texFilterState2_ += bodyCutoff * texFilterState1_;

                        texL = (rosinBite * 0.45f + texFilterState2_ * 0.75f) * bowPressure;
                        texR = (rosinBite * 0.40f + texFilterState1_ * 0.65f) * bowPressure;
                    }
                    break;

                case 2: // Tape Halo & Flutter (Vintage tape head warmth, 0.35Hz wow + 5.2Hz flutter, and magnetic hiss)
                    {
                        flutterPhase_ += 5.2f * invSr;
                        if (flutterPhase_ >= 1.0f) flutterPhase_ -= 1.0f;

                        const float wow = std::sin(twoPi * texGrainPhase_) * 0.0035f;
                        const float flutter = std::sin(twoPi * flutterPhase_) * 0.0018f;
                        const float tapePh = std::fmod(phases_[0] + wow + flutter + 1.0f, 1.0f);

                        const float core = std::sin(twoPi * tapePh) + 0.40f * std::sin(4.0f * std::numbers::pi_v<float> * tapePh);
                        const float driven = core * (1.2f + 1.8f * shape);
                        const float tapeSat = std::tanh(driven) * 0.75f;

                        const float tapeHiss = noiseSample * 0.035f;

                        texL = tapeSat + tapeHiss;
                        texR = tapeSat - tapeHiss;
                    }
                    break;

                case 3: // Cathedral Rain Cloud (Asynchronous multi-grain micro-particle scatter)
                    {
                        const float win1 = std::sin(std::numbers::pi_v<float> * texGrainPhase_);
                        const float win2 = std::sin(std::numbers::pi_v<float> * texGrainPhase2_);

                        const float grainSine1 = std::sin(twoPi * phases_[0] * 2.0f);
                        const float grainSine2 = std::sin(twoPi * phases_[0] * 3.0f + 0.4f);
                        const float dropletClick = (noiseSample > (0.94f - shape * 0.08f)) ? (noiseSample * 0.85f) : 0.0f;

                        const float grain1 = (grainSine1 * (win1 * win1) + dropletClick * 0.4f) * (0.6f + 0.4f * shape);
                        const float grain2 = (grainSine2 * (win2 * win2) + dropletClick * 0.4f) * (0.6f + 0.4f * shape);

                        texL = grain1 * 0.80f + grain2 * 0.35f;
                        texR = grain1 * 0.35f + grain2 * 0.80f;
                    }
                    break;

                case 4: // Warm Wind & Air Swell (Tuned vortex shedding, aerodynamic resonant pipe + fluty fundamental)
                default:
                    {
                        const float flute = std::sin(twoPi * phases_[0]) + 0.18f * std::sin(4.0f * std::numbers::pi_v<float> * phases_[0]);
                        const float gust = 0.55f + 0.45f * std::sin(twoPi * texGrainPhase_ * 0.5f);

                        const float windQ = 0.15f + 0.65f * shape;
                        const float windCutoff = std::clamp(slotFreq * invSr * 2.0f, 0.01f, 0.45f);
                        texFilterState1_ += windCutoff * (noiseSample * 0.55f - texFilterState1_ - windQ * texFilterState2_);
                        texFilterState2_ += windCutoff * texFilterState1_;

                        texL = (flute * (1.0f - 0.45f * shape) + texFilterState1_ * (0.4f + 0.6f * shape)) * gust;
                        texR = (flute * (1.0f - 0.45f * shape) - texFilterState1_ * (0.4f + 0.6f * shape)) * gust;
                    }
                    break;
            }

            sigL = texL * 0.72f;
            sigR = texR * 0.72f;
            break;
        }

        case EngineType::PhysicalPluckModal: {
            if (params_.modelIndex >= 2) {
                // Modal Inharmonic Resonator (Kalimba / Glass Mallet)
                const float twoPi = 2.0f * std::numbers::pi_v<float>;
                const std::array<float, 4> modeRatios = (params_.modelIndex == 2)
                    ? std::array<float, 4>{ 1.0f, 2.756f, 5.404f, 8.933f }
                    : std::array<float, 4>{ 1.0f, 2.0f, 3.162f, 4.520f };

                float modalSumL = 0.0f;
                float modalSumR = 0.0f;

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

                sigL = modalSumL * 0.85f;
                sigR = modalSumR * 0.85f;
            } else {
                // Waveguide Strings (Steel String & Nylon Harp)
                const float delayLenL = std::clamp(sr / slotFreq, 8.0f, static_cast<float>(kMaxStringDelay - 8));
                const float delayLenR = std::clamp(delayLenL * pluckDelayRatioR_, 8.0f, static_cast<float>(kMaxStringDelay - 8));

                const auto readWaveguide = [this](const std::array<float, kMaxStringDelay>& buf, float delaySamples) noexcept {
                    const int dInt = static_cast<int>(delaySamples);
                    const float frac = delaySamples - static_cast<float>(dInt);
                    const int idx0 = (stringWritePos_ - dInt + kMaxStringDelay) & (kMaxStringDelay - 1);
                    const int idx1 = (idx0 - 1 + kMaxStringDelay) & (kMaxStringDelay - 1);
                    return buf[static_cast<std::size_t>(idx0)] +
                           frac * (buf[static_cast<std::size_t>(idx1)] - buf[static_cast<std::size_t>(idx0)]);
                };

                const float rawL = readWaveguide(stringDelayL_, delayLenL);
                const float rawR = readWaveguide(stringDelayR_, delayLenR);

                const float brightness = (params_.modelIndex == 0)
                    ? (0.35f + 0.60f * shape)
                    : (0.15f + 0.40f * shape);
                stringLpStateL_ = rawL * brightness + stringLpStateL_ * (1.0f - brightness);
                stringLpStateR_ = rawR * brightness + stringLpStateR_ * (1.0f - brightness);

                const float sustainFeedback = (params_.modelIndex == 0)
                    ? (0.9950f + 0.0045f * shape)
                    : (0.9910f + 0.0060f * shape);
                stringDelayL_[static_cast<std::size_t>(stringWritePos_)] = stringLpStateL_ * sustainFeedback;
                stringDelayR_[static_cast<std::size_t>(stringWritePos_)] = stringLpStateR_ * sustainFeedback;
                stringWritePos_ = (stringWritePos_ + 1) & (kMaxStringDelay - 1);

                phases_[0] += slotFreq * invSr;
                if (phases_[0] >= 1.0f) phases_[0] -= 1.0f;
                const float modalBody = (params_.modelIndex == 0)
                    ? (0.15f * (1.0f - 2.0f * phases_[0]))
                    : (0.28f * std::sin(2.0f * std::numbers::pi_v<float> * phases_[0]));

                sigL = (stringLpStateL_ + modalBody) * 0.85f;
                sigR = (stringLpStateR_ + modalBody) * 0.85f;
            }
            break;
        }

        case EngineType::SubOscillator: {
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
                case 0: // Pure Sine Sub
                    subVal = sine;
                    break;
                case 1: // Warm Triangle Sub
                    subVal = (1.0f - shape * 0.45f) * tri + (shape * 0.45f) * sine;
                    break;
                case 2: // Driven 808 Sub
                    {
                        const float driveX = sine * (1.6f + shape * 2.4f);
                        const float x2 = driveX * driveX;
                        subVal = (driveX * (27.0f + x2) / (27.0f + 9.0f * x2)) * 0.88f;
                    }
                    break;
                case 3: // Square Punch Sub
                    subVal = (1.0f - shape * 0.35f) * sqr + (shape * 0.35f) * sine;
                    break;
                default:
                    subVal = sine;
                    break;
            }

            // Sub is always centered in the mix for solid low end
            sigL = subVal * 0.95f;
            sigR = subVal * 0.95f;
            break;
        }

        case EngineType::NoiseTransient: {
            // Noise / Transient: Pink noise with tilt filter + air sizzle
            rng_ = rng_ * 1664525u + 1013904223u;
            const float white = ((static_cast<float>(rng_ & 0xFFFFu) / 32767.5f) - 1.0f);

            // 1-pole low-pass / pinking filter
            const float pinkCoeff = 0.15f + 0.70f * shape;
            texFilterState1_ = texFilterState1_ + pinkCoeff * (white - texFilterState1_);

            // High-pass component for air / transient sparkle
            const float air = white - texFilterState1_;
            float noiseSig = (texFilterState1_ * (1.0f - shape * 0.5f) + air * shape * 0.8f) * 0.55f;

            switch (params_.modelIndex) {
                case 0: // Analog Pink Noise
                    noiseSig = texFilterState1_ * 0.65f;
                    break;
                case 1: // Velvet Vinyl Dust
                    noiseSig = (std::abs(white) > (0.90f - shape * 0.12f)) ? (white * 1.6f) : (white * 0.08f);
                    break;
                case 2: // White Shimmer Air
                    texFilterState2_ += 0.42f * (white - texFilterState2_);
                    noiseSig = (white - texFilterState2_) * 0.52f;
                    break;
                case 3: // Attack Key Click
                    noiseSig = white * std::exp(-phases_[0] * (16.0f + shape * 32.0f)) * 1.25f;
                    phases_[0] += 0.005f;
                    break;
                default:
                    break;
            }

            const float uPan = std::clamp(params_.pan, -1.0f, 1.0f);
            const float panAngle = (uPan + 1.0f) * 0.25f * std::numbers::pi_v<float>;
            sigL = noiseSig * std::cos(panAngle);
            sigR = noiseSig * std::sin(panAngle);
            break;
        }

        case EngineType::SamplePlayback: {
            if (sampleFinished_ || activeSampleZone_ == nullptr) break;
            const auto& smp = SampleBank::getSample(params_.modelIndex);
            const auto* zone = activeSampleZone_;
            if (zone->pcmL == nullptr || zone->numSamples <= 4) break;

            const double playbackRate = (static_cast<double>(slotFreq) / static_cast<double>(zone->rootFrequencyHz)) * (smp.sampleRate / sampleRate_);
            const int idx = static_cast<int>(samplePlayhead_);

            if (idx >= zone->numSamples - 3) {
                if (smp.isLooping && zone->numSamples > 1000) {
                    const int loopStart = static_cast<int>(zone->numSamples * 0.25);
                    const int loopLen = zone->numSamples - loopStart - 4;
                    if (loopLen > 100) {
                        samplePlayhead_ = loopStart + std::fmod(samplePlayhead_ - loopStart, static_cast<double>(loopLen));
                    } else {
                        sampleFinished_ = true;
                        break;
                    }
                } else {
                    sampleFinished_ = true;
                    break;
                }
            }

            const int i = static_cast<int>(samplePlayhead_);
            const float frac = static_cast<float>(samplePlayhead_ - static_cast<double>(i));

            // Hermite cubic interpolation for Left channel
            const float y0L = static_cast<float>(i > 0 ? zone->pcmL[i - 1] : zone->pcmL[i]) * (1.0f / 32768.0f);
            const float y1L = static_cast<float>(zone->pcmL[i]) * (1.0f / 32768.0f);
            const float y2L = static_cast<float>(zone->pcmL[i + 1]) * (1.0f / 32768.0f);
            const float y3L = static_cast<float>(i + 2 < zone->numSamples ? zone->pcmL[i + 2] : zone->pcmL[i + 1]) * (1.0f / 32768.0f);

            const float c0L = y1L;
            const float c1L = 0.5f * (y2L - y0L);
            const float c2L = y0L - 2.5f * y1L + 2.0f * y2L - 0.5f * y3L;
            const float c3L = 0.5f * (y3L - y0L) + 1.5f * (y1L - y2L);
            const float rawL = ((c3L * frac + c2L) * frac + c1L) * frac + c0L;

            // Hermite cubic interpolation for Right channel
            const int16_t* pcmR = zone->pcmR ? zone->pcmR : zone->pcmL;
            const float y0R = static_cast<float>(i > 0 ? pcmR[i - 1] : pcmR[i]) * (1.0f / 32768.0f);
            const float y1R = static_cast<float>(pcmR[i]) * (1.0f / 32768.0f);
            const float y2R = static_cast<float>(pcmR[i + 1]) * (1.0f / 32768.0f);
            const float y3R = static_cast<float>(i + 2 < zone->numSamples ? pcmR[i + 2] : pcmR[i + 1]) * (1.0f / 32768.0f);

            const float c0R = y1R;
            const float c1R = 0.5f * (y2R - y0R);
            const float c2R = y0R - 2.5f * y1R + 2.0f * y2R - 0.5f * y3R;
            const float c3R = 0.5f * (y3R - y0R) + 1.5f * (y1R - y2R);
            const float rawR = ((c3R * frac + c2R) * frac + c1R) * frac + c0R;

            // Stereo width & detune spray spread
            const float spray = std::clamp(params_.detune, 0.0f, 1.0f);
            const float uPan = std::clamp(params_.pan, -1.0f, 1.0f);
            const float panAngle = (uPan + 1.0f) * 0.25f * std::numbers::pi_v<float>;

            // Mid/Side stereo balance preserving acoustic space
            const float mid = 0.5f * (rawL + rawR);
            const float side = 0.5f * (rawL - rawR);
            const float width = 1.0f + spray * 0.6f;
            const float wideL = mid + side * width;
            const float wideR = mid - side * width;

            sigL = wideL * std::cos(panAngle) * 1.4142f;
            sigR = wideR * std::sin(panAngle) * 1.4142f;

            samplePlayhead_ += playbackRate;
            break;
        }
    }

    outLeft  += sigL * params_.level;
    outRight += sigR * params_.level;
}

std::vector<const char*> MultiEngineOscillator::getModelsForEngine(EngineType engine, int sourceType) {
    if (sourceType == 1) { // SUB
        return { "Pure Sine Sub", "Warm Triangle Sub", "Driven 808 Sub", "Square Punch Sub" };
    }
    if (sourceType == 2) { // PLUCK
        return { "Acoustic Steel String", "Nylon Worship Harp", "Kalimba / Music Box", "Glass Mallet Resonance" };
    }
    if (sourceType == 3) { // NOISE
        return { "Analog Pink Noise", "Velvet Vinyl Dust", "White Shimmer Air", "Attack Key Click" };
    }
    if (sourceType == 4 || engine == EngineType::SamplePlayback) { // SAMPLE
        return {
            "Grand Piano Hammer",
            "Concert Harp Pluck",
            "Upright Felt Thump",
            "Celesta Soft Bell",
            "Glockenspiel Chime",
            "Worship Hand Chime",
            "Kalimba Metal Tine",
            "Sacred Flute Pipe"
        };
    }

    switch (engine) {
        case EngineType::WarmAnalog:
            return { "Classic Saw", "Pulse / Square", "Warm Triangle", "Mellow Sine", "Super Saw Multi", "Hard Sync Lead" };
        case EngineType::EvolvingWavetable:
            return { "Worship Harmonics", "Cathedral Drawbars", "Silky Velvet Strings", "Vocal Choir Formants",
                     "Harmonic Octave Pad", "Glass Bell Chimes", "Warm Brass Swell", "Celestial Shimmer Air" };
        case EngineType::OrganicTexture:
            return { "Ambient Choir Breath", "Bowed Cello Texture", "Tape Halo & Flutter", "Cathedral Rain Cloud", "Warm Wind & Air Swell" };
        case EngineType::PhysicalPluckModal:
            return { "Acoustic Steel String", "Nylon Worship Harp", "Kalimba / Music Box", "Glass Mallet Resonance" };
        case EngineType::SubOscillator:
            return { "Pure Sine Sub", "Warm Triangle Sub", "Driven 808 Sub", "Square Punch Sub" };
        case EngineType::NoiseTransient:
            return { "Analog Pink Noise", "Velvet Vinyl Dust", "White Shimmer Air", "Attack Key Click" };
        case EngineType::SamplePlayback:
            return {
                "Grand Piano Hammer",
                "Concert Harp Pluck",
                "Upright Felt Thump",
                "Celesta Soft Bell",
                "Glockenspiel Chime",
                "Worship Hand Chime",
                "Kalimba Metal Tine",
                "Sacred Flute Pipe"
            };
    }
    return { "Classic Saw" };
}

const char* MultiEngineOscillator::getModelName(EngineType engine, int sourceType, int modelIndex) {
    const auto list = getModelsForEngine(engine, sourceType);
    if (list.empty()) return "Classic Saw";
    const int idx = std::clamp(modelIndex, 0, static_cast<int>(list.size()) - 1);
    return list[static_cast<std::size_t>(idx)];
}

std::vector<float> MultiEngineOscillator::buildWaveformPreview(const Parameters& p, int numPoints) {
    std::vector<float> pts(static_cast<std::size_t>(std::max(8, numPoints)), 0.5f);
    const float twoPi = 2.0f * std::numbers::pi_v<float>;
    for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(pts.size() - 1);
        float val = 0.0f;
        switch (p.engine) {
            case EngineType::WarmAnalog:
                switch (p.modelIndex) {
                    case 0: val = 2.0f * u - 1.0f; break; // Saw
                    case 1: val = (u < (0.5f - 0.38f * p.shapeMorph)) ? 1.0f : -1.0f; break; // Pulse
                    case 2: val = 2.0f * std::abs(2.0f * (u - 0.5f)) - 1.0f; break; // Triangle
                    case 3: val = std::sin(twoPi * u); break; // Sine
                    case 4: val = 0.5f * (2.0f * u - 1.0f) + 0.5f * (2.0f * std::fmod(u + 0.33f, 1.0f) - 1.0f); break; // SuperSaw
                    case 5: default: val = 2.0f * std::fmod(u * (1.0f + 2.0f * p.shapeMorph), 1.0f) - 1.0f; break; // Hard Sync
                }
                break;
            case EngineType::EvolvingWavetable: {
                const auto& bank = getWavetableBank();
                const int frame = std::clamp(p.modelIndex, 0, kWtFrames - 1);
                const int idx = static_cast<int>(u * (kWtSize - 1));
                val = bank.frames[static_cast<std::size_t>(frame)][static_cast<std::size_t>(idx)];
                break;
            }
            case EngineType::OrganicTexture:
                switch (p.modelIndex) {
                    case 0: val = 0.55f * std::sin(twoPi * u) + 0.35f * std::sin(twoPi * u * 2.0f); break; // Ambient Choir
                    case 1: val = (2.0f * u - 1.0f) * 0.7f + 0.3f * std::sin(twoPi * u * 3.0f); break; // Bowed Cello
                    case 2: val = std::tanh((std::sin(twoPi * u) + 0.3f * std::sin(twoPi * u * 2.0f)) * 2.0f); break; // Tape Halo
                    case 3: val = std::sin(twoPi * u * 3.0f) * std::sin(twoPi * u * 8.0f); break; // Rain Cloud
                    case 4: default: val = 0.7f * std::sin(twoPi * u) * (0.5f + 0.5f * std::sin(twoPi * u * 5.0f)); break; // Wind Swell
                }
                break;
            case EngineType::PhysicalPluckModal:
                switch (p.modelIndex) {
                    case 0: val = std::exp(-2.5f * u) * std::sin(twoPi * u * 4.0f); break; // Steel String
                    case 1: val = std::exp(-4.0f * u) * std::sin(twoPi * u * 2.0f); break; // Nylon Harp
                    case 2: val = std::exp(-2.0f * u) * (std::sin(twoPi * u * 2.0f) + 0.6f * std::sin(twoPi * u * 5.51f)); break; // Kalimba
                    case 3: default: val = std::exp(-1.5f * u) * (std::sin(twoPi * u * 3.0f) + 0.5f * std::sin(twoPi * u * 6.32f)); break; // Glass Mallet
                }
                break;
            case EngineType::SubOscillator:
                switch (p.modelIndex) {
                    case 0: val = std::sin(twoPi * u); break;
                    case 1: val = 2.0f * std::abs(2.0f * (u - 0.5f)) - 1.0f; break;
                    case 2: val = std::tanh(std::sin(twoPi * u) * 2.5f); break;
                    case 3: default: val = u < 0.5f ? 0.85f : -0.85f; break;
                }
                break;
            case EngineType::NoiseTransient:
                switch (p.modelIndex) {
                    case 0: val = 0.5f * std::sin(twoPi * u * 13.0f); break;
                    case 1: val = (std::fmod(u * 7.0f, 1.0f) > 0.8f ? 0.8f : 0.05f) * std::sin(twoPi * u * 29.0f); break;
                    case 2: val = 0.6f * std::sin(twoPi * u * 23.0f); break;
                    case 3: default: val = std::exp(-12.0f * u) * std::sin(twoPi * u * 19.0f); break;
                }
                break;
            case EngineType::SamplePlayback: {
                const auto& smp = SampleBank::getSample(p.modelIndex);
                if (smp.zones != nullptr && smp.numZones > 0 && smp.zones[0].pcmL != nullptr && smp.zones[0].numSamples > 0) {
                    const auto& z0 = smp.zones[0];
                    const int step = std::max(1, z0.numSamples / static_cast<int>(pts.size()));
                    const int sIdx = std::clamp(i * step, 0, z0.numSamples - 1);
                    val = static_cast<float>(z0.pcmL[sIdx]) * (1.0f / 32768.0f);
                } else {
                    val = std::sin(twoPi * u * 4.0f) * std::exp(-3.0f * u);
                }
                break;
            }
        }
        pts[static_cast<std::size_t>(i)] = std::clamp(0.5f + 0.42f * val * p.level, 0.05f, 0.95f);
    }
    return pts;
}

} // namespace nova::dsp
