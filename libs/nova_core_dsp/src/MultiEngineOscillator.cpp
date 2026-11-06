#include "nova_dsp/oscillators/MultiEngineOscillator.h"
#include "nova_dsp/oscillators/SampleBank.h"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace nova::dsp {

namespace {

// Shared static 8-model x 4-frame bandlimited wavetable bank (initialized once on first prepare)
struct SharedWavetableBank {
    static constexpr int kWtTables = 8;
    static constexpr int kWtFrames = 4;
    static constexpr int kWtSize = 1024;
    std::array<std::array<std::array<float, kWtSize>, kWtFrames>, kWtTables> tables{};

    SharedWavetableBank() {
        constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
        // Golden-angle harmonic phase dispersion preserves exact amplitude spectra without phase peaking
        auto dispPhase = [](int h) constexpr noexcept -> float {
            return (h <= 1) ? 0.0f : static_cast<float>((h - 1) * 2.39996323f);
        };

        for (int i = 0; i < kWtSize; ++i) {
            const float ph = static_cast<float>(i) / static_cast<float>(kWtSize);
            const float w = twoPi * ph;

            // ========================================================
            // Table 0: Worship Harmonics (Rich, warm worship pad overtone series)
            // ========================================================
            tables[0][0][static_cast<std::size_t>(i)] =
                0.85f * std::sin(w) + 0.35f * std::sin(2.0f * w) + 0.18f * std::sin(3.0f * w);

            tables[0][1][static_cast<std::size_t>(i)] =
                0.75f * std::sin(w) + 0.40f * std::sin(2.0f * w) + 0.32f * std::sin(3.0f * w + dispPhase(3))
                + 0.22f * std::sin(4.0f * w + dispPhase(4)) + 0.14f * std::sin(5.0f * w + dispPhase(5)) + 0.10f * std::sin(6.0f * w);

            {
                float s = 0.0f;
                for (int h = 1; h <= 12; ++h) {
                    const float fh = static_cast<float>(h);
                    const float amp = (1.0f / std::pow(fh, 0.95f)) * (1.0f + 0.30f * std::cos(fh * 1.2f));
                    s += amp * std::sin(fh * w + dispPhase(h));
                }
                tables[0][2][static_cast<std::size_t>(i)] = s;
            }
            {
                float s = 0.0f;
                for (int h = 1; h <= 20; ++h) {
                    const float fh = static_cast<float>(h);
                    const float amp = (1.0f / std::pow(fh, 0.88f)) * (1.0f + 0.20f * std::sin(fh * 1.5f));
                    s += amp * std::sin(fh * w + dispPhase(h));
                }
                tables[0][3][static_cast<std::size_t>(i)] = s;
            }

            // ========================================================
            // Table 1: Cathedral Drawbars (Hammond B3 Tonewheel Organ)
            // Harmonics: 16' (h=1), 8' (h=2, root), 5 1/3' (h=3), 4' (h=4),
            //            2 2/3' (h=6), 2' (h=8), 1 3/5' (h=10), 1 1/3' (h=12), 1' (h=16)
            // ========================================================
            // Frame 0: 888000000 (Mellow Jazz Tonewheel: 16', 8', 5 1/3')
            tables[1][0][static_cast<std::size_t>(i)] =
                0.85f * std::sin(2.0f * w) + 0.75f * std::sin(w) + 0.65f * std::sin(3.0f * w);

            // Frame 1: 888800000 (Gospel Foundation: adds 4')
            tables[1][1][static_cast<std::size_t>(i)] =
                0.85f * std::sin(2.0f * w) + 0.75f * std::sin(w) + 0.65f * std::sin(3.0f * w)
                + 0.55f * std::sin(4.0f * w);

            // Frame 2: 888888000 (Classic Church Rock / Shout: adds 2 2/3' & 2')
            tables[1][2][static_cast<std::size_t>(i)] =
                0.85f * std::sin(2.0f * w) + 0.75f * std::sin(w) + 0.65f * std::sin(3.0f * w)
                + 0.55f * std::sin(4.0f * w) + 0.50f * std::sin(6.0f * w) + 0.45f * std::sin(8.0f * w);

            // Frame 3: 888888888 (Full Cathedral Organ: all drawbars out with biting upper brilliance)
            tables[1][3][static_cast<std::size_t>(i)] =
                0.90f * std::sin(2.0f * w) + 0.80f * std::sin(w) + 0.70f * std::sin(3.0f * w)
                + 0.65f * std::sin(4.0f * w) + 0.60f * std::sin(6.0f * w) + 0.55f * std::sin(8.0f * w)
                + 0.45f * std::sin(10.0f * w) + 0.40f * std::sin(12.0f * w) + 0.35f * std::sin(16.0f * w);

            // ========================================================
            // Table 2: Silky Velvet Strings (Orchestral Bowed String Ensemble)
            // Cello/Violin cavity resonances at 450 Hz, 1200 Hz, 2800 Hz
            // ========================================================
            {
                float s0 = 0.0f;
                for (int h = 1; h <= 8; ++h) {
                    const float fh = static_cast<float>(h);
                    const float amp = (1.0f / fh) * (1.0f + 0.40f * std::exp(-0.80f * (fh - 3.0f) * (fh - 3.0f)));
                    s0 += amp * std::sin(fh * w + dispPhase(h));
                }
                tables[2][0][static_cast<std::size_t>(i)] = s0;

                float s1 = 0.0f;
                for (int h = 1; h <= 16; ++h) {
                    const float fh = static_cast<float>(h);
                    const float f1 = 0.45f * std::exp(-0.40f * (fh - 3.5f) * (fh - 3.5f));
                    const float f2 = 0.25f * std::exp(-0.30f * (fh - 8.5f) * (fh - 8.5f));
                    s1 += ((1.0f / fh) + f1 + f2) * std::sin(fh * w + dispPhase(h));
                }
                tables[2][1][static_cast<std::size_t>(i)] = s1;

                float s2 = 0.0f;
                for (int h = 1; h <= 24; ++h) {
                    const float fh = static_cast<float>(h);
                    const float body = (1.0f / std::pow(fh, 0.92f));
                    const float sheen = 0.22f * std::exp(-0.15f * (fh - 16.0f) * (fh - 16.0f));
                    s2 += (body + sheen) * std::sin(fh * w + dispPhase(h));
                }
                tables[2][2][static_cast<std::size_t>(i)] = s2;

                float s3 = 0.0f;
                for (int h = 1; h <= 32; ++h) {
                    const float fh = static_cast<float>(h);
                    const float body = (1.0f / std::pow(fh, 0.88f));
                    const float sheen = 0.35f * std::exp(-0.12f * (fh - 18.0f) * (fh - 18.0f));
                    s3 += (body + sheen) * std::sin(fh * w + dispPhase(h));
                }
                tables[2][3][static_cast<std::size_t>(i)] = s3;
            }

            // ========================================================
            // Table 3: Vocal Choir Formants (Human Acoustic Vowel Choir)
            // Distinct vowels: /u/ ("Ooo") -> /o/ ("Ohh") -> /a/ ("Aah") -> /i/ ("Eee")
            // ========================================================
            {
                float s0 = 0.0f;
                for (int h = 1; h <= 16; ++h) {
                    const float fh = static_cast<float>(h);
                    const float f1 = 0.90f * std::exp(-1.20f * (fh - 2.0f) * (fh - 2.0f));
                    const float f2 = 0.50f * std::exp(-0.80f * (fh - 5.0f) * (fh - 5.0f));
                    s0 += ((h == 1 ? 0.60f : 0.05f) + f1 + f2) * std::sin(fh * w + dispPhase(h));
                }
                tables[3][0][static_cast<std::size_t>(i)] = s0;

                float s1 = 0.0f;
                for (int h = 1; h <= 18; ++h) {
                    const float fh = static_cast<float>(h);
                    const float f1 = 0.85f * std::exp(-0.90f * (fh - 3.2f) * (fh - 3.2f));
                    const float f2 = 0.60f * std::exp(-0.60f * (fh - 7.0f) * (fh - 7.0f));
                    s1 += ((h == 1 ? 0.50f : 0.05f) + f1 + f2) * std::sin(fh * w + dispPhase(h));
                }
                tables[3][1][static_cast<std::size_t>(i)] = s1;

                float s2 = 0.0f;
                for (int h = 1; h <= 22; ++h) {
                    const float fh = static_cast<float>(h);
                    const float f1 = 0.80f * std::exp(-0.60f * (fh - 5.2f) * (fh - 5.2f));
                    const float f2 = 0.70f * std::exp(-0.40f * (fh - 9.0f) * (fh - 9.0f));
                    const float f3 = 0.35f * std::exp(-0.30f * (fh - 16.0f) * (fh - 16.0f));
                    s2 += ((h == 1 ? 0.40f : 0.05f) + f1 + f2 + f3) * std::sin(fh * w + dispPhase(h));
                }
                tables[3][2][static_cast<std::size_t>(i)] = s2;

                float s3 = 0.0f;
                for (int h = 1; h <= 24; ++h) {
                    const float fh = static_cast<float>(h);
                    const float f1 = 0.75f * std::exp(-1.00f * (fh - 2.2f) * (fh - 2.2f));
                    const float f2 = 0.85f * std::exp(-0.25f * (fh - 15.0f) * (fh - 15.0f));
                    const float f3 = 0.55f * std::exp(-0.20f * (fh - 20.0f) * (fh - 20.0f));
                    s3 += ((h == 1 ? 0.35f : 0.04f) + f1 + f2 + f3) * std::sin(fh * w + dispPhase(h));
                }
                tables[3][3][static_cast<std::size_t>(i)] = s3;
            }

            // ========================================================
            // Table 4: Harmonic Octave Pad (Pristine Octave Stacks)
            // ========================================================
            tables[4][0][static_cast<std::size_t>(i)] =
                0.85f * std::sin(w) + 0.55f * std::sin(2.0f * w);

            tables[4][1][static_cast<std::size_t>(i)] =
                0.75f * std::sin(w) + 0.65f * std::sin(2.0f * w) + 0.45f * std::sin(4.0f * w);

            tables[4][2][static_cast<std::size_t>(i)] =
                0.65f * std::sin(w) + 0.55f * std::sin(2.0f * w) + 0.30f * std::sin(3.0f * w)
                + 0.45f * std::sin(4.0f * w) + 0.25f * std::sin(6.0f * w) + 0.35f * std::sin(8.0f * w);

            tables[4][3][static_cast<std::size_t>(i)] =
                0.55f * std::sin(w) + 0.50f * std::sin(2.0f * w) + 0.25f * std::sin(3.0f * w)
                + 0.40f * std::sin(4.0f * w) + 0.22f * std::sin(6.0f * w) + 0.32f * std::sin(8.0f * w)
                + 0.18f * std::sin(12.0f * w) + 0.24f * std::sin(16.0f * w) + 0.14f * std::sin(24.0f * w);

            // ========================================================
            // Table 5: Glass Bell Chimes (Metallic Inharmonic Bell / D-50 / DX7 Tine)
            // ========================================================
            tables[5][0][static_cast<std::size_t>(i)] =
                0.85f * std::sin(w) + 0.35f * std::sin(2.0f * w) + 0.25f * std::sin(3.0f * w)
                + 0.12f * std::sin(5.0f * w);

            tables[5][1][static_cast<std::size_t>(i)] =
                0.60f * std::sin(w) + 0.45f * std::sin(3.0f * w) + 0.38f * std::sin(5.0f * w)
                + 0.32f * std::sin(7.0f * w) + 0.25f * std::sin(11.0f * w);

            tables[5][2][static_cast<std::size_t>(i)] =
                0.50f * std::sin(w) + 0.30f * std::sin(2.0f * w) + 0.40f * std::sin(3.0f * w)
                + 0.35f * std::sin(5.0f * w) + 0.30f * std::sin(7.0f * w) + 0.28f * std::sin(11.0f * w)
                + 0.22f * std::sin(14.0f * w) + 0.18f * std::sin(19.0f * w);

            tables[5][3][static_cast<std::size_t>(i)] =
                0.40f * std::sin(w) + 0.35f * std::sin(3.0f * w) + 0.32f * std::sin(5.0f * w)
                + 0.30f * std::sin(7.0f * w) + 0.28f * std::sin(11.0f * w) + 0.24f * std::sin(14.0f * w)
                + 0.22f * std::sin(19.0f * w) + 0.18f * std::sin(23.0f * w) + 0.15f * std::sin(27.0f * w);

            // ========================================================
            // Table 6: Warm Brass Swell (Oberheim OB-Xa / CS-80 Analog Brass)
            // ========================================================
            {
                float s0 = 0.0f;
                for (int h = 1; h <= 6; ++h) {
                    const float fh = static_cast<float>(h);
                    s0 += (1.0f / fh) * std::sin(fh * w);
                }
                tables[6][0][static_cast<std::size_t>(i)] = s0;

                float s1 = 0.0f;
                for (int h = 1; h <= 12; ++h) {
                    const float fh = static_cast<float>(h);
                    const float hornPeak = 0.45f * std::exp(-0.60f * (fh - 3.5f) * (fh - 3.5f));
                    s1 += ((1.0f / std::pow(fh, 0.85f)) + hornPeak) * std::sin(fh * w);
                }
                tables[6][1][static_cast<std::size_t>(i)] = s1;

                float s2 = 0.0f;
                for (int h = 1; h <= 18; ++h) {
                    const float fh = static_cast<float>(h);
                    const float hornPeak = 0.65f * std::exp(-0.35f * (fh - 4.2f) * (fh - 4.2f));
                    s2 += ((1.0f / std::pow(fh, 0.75f)) + hornPeak) * std::sin(fh * w);
                }
                tables[6][2][static_cast<std::size_t>(i)] = s2;

                float s3 = 0.0f;
                for (int h = 1; h <= 26; ++h) {
                    const float fh = static_cast<float>(h);
                    const float hornPeak = 0.85f * std::exp(-0.25f * (fh - 5.0f) * (fh - 5.0f));
                    s3 += ((1.0f / std::pow(fh, 0.70f)) + hornPeak) * std::sin(fh * w);
                }
                tables[6][3][static_cast<std::size_t>(i)] = s3;
            }

            // ========================================================
            // Table 7: Celestial Shimmer Air (High Air Halo / Angelic Sizzle)
            // ========================================================
            tables[7][0][static_cast<std::size_t>(i)] =
                0.75f * std::sin(w) + 0.35f * std::sin(4.0f * w) + 0.25f * std::sin(8.0f * w);

            tables[7][1][static_cast<std::size_t>(i)] =
                0.55f * std::sin(w) + 0.40f * std::sin(4.0f * w) + 0.35f * std::sin(6.0f * w)
                + 0.30f * std::sin(8.0f * w) + 0.22f * std::sin(12.0f * w);

            tables[7][2][static_cast<std::size_t>(i)] =
                0.45f * std::sin(w) + 0.35f * std::sin(4.0f * w) + 0.32f * std::sin(8.0f * w)
                + 0.28f * std::sin(12.0f * w) + 0.24f * std::sin(16.0f * w) + 0.20f * std::sin(20.0f * w)
                + 0.16f * std::sin(24.0f * w);

            tables[7][3][static_cast<std::size_t>(i)] =
                0.35f * std::sin(w) + 0.30f * std::sin(4.0f * w) + 0.28f * std::sin(8.0f * w)
                + 0.26f * std::sin(12.0f * w) + 0.24f * std::sin(16.0f * w) + 0.22f * std::sin(20.0f * w)
                + 0.20f * std::sin(24.0f * w) + 0.18f * std::sin(28.0f * w) + 0.15f * std::sin(32.0f * w);
        }

        // Normalize all tables & frames to consistent RMS (~0.62)
        for (std::size_t t = 0; t < static_cast<std::size_t>(kWtTables); ++t) {
            for (std::size_t f = 0; f < static_cast<std::size_t>(kWtFrames); ++f) {
                float sumSq = 0.0f;
                for (std::size_t i = 0; i < static_cast<std::size_t>(kWtSize); ++i) {
                    sumSq += tables[t][f][i] * tables[t][f][i];
                }
                const float rms = std::sqrt(sumSq / static_cast<float>(kWtSize));
                const float gain = (rms > 1e-5f) ? (0.62f / rms) : 1.0f;
                for (std::size_t i = 0; i < static_cast<std::size_t>(kWtSize); ++i) {
                    tables[t][f][i] *= gain;
                }
            }
        }
    }
};

const SharedWavetableBank& getWavetableBank() {
    static const SharedWavetableBank bank;
    return bank;
}

constexpr std::array<float, 5> kUnisonDetuneFactors = { 0.0f, -0.42f, 0.42f, -0.88f, 0.88f };
constexpr std::array<float, 5> kUnisonPanPositions  = { 0.0f, -0.55f, 0.55f, -0.90f, 0.90f };

inline float readSampleHermite(const int16_t* pcm, int numSamples, double playhead) noexcept {
    if (pcm == nullptr || numSamples <= 0) return 0.0f;
    const double clampedPos = std::clamp(playhead, 0.0, static_cast<double>(numSamples - 1));
    const int i = static_cast<int>(clampedPos);
    const float frac = static_cast<float>(clampedPos - static_cast<double>(i));

    const float y0 = static_cast<float>(i > 0 ? pcm[i - 1] : pcm[i]) * (1.0f / 32768.0f);
    const float y1 = static_cast<float>(pcm[i]) * (1.0f / 32768.0f);
    const float y2 = static_cast<float>(i + 1 < numSamples ? pcm[i + 1] : pcm[i]) * (1.0f / 32768.0f);
    const float y3 = static_cast<float>(i + 2 < numSamples ? pcm[i + 2] : (i + 1 < numSamples ? pcm[i + 1] : pcm[i])) * (1.0f / 32768.0f);

    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

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

        const float uPan = std::clamp(kUnisonPanPositions[idx] * detuneAmt, -1.0f, 1.0f);
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
    std::fill(stringDelayL_.begin(), stringDelayL_.end(), 0.0f);
    std::fill(stringDelayR_.begin(), stringDelayR_.end(), 0.0f);
    stringWritePos_ = 0;
    stringLpStateL_ = 0.0f;
    stringLpStateR_ = 0.0f;
    modalPhases_.fill(0.0f);
    modalAmplitudes_.fill(0.0f);
    modalDecays_.fill(0.999f);
    samplePlayhead_ = 0.0;
    sampleDirection_ = 1.0;
    sampleFinished_ = false;
    lastPitchModSemitones_ = -999.0f;
    cachedPitchModRatio_ = 1.0f;
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
            const int numS = activeSampleZone_->numSamples;
            const double sMorph = static_cast<double>(std::clamp(params_.shapeMorph, 0.0f, 1.0f));
            const double lStart = static_cast<double>(std::clamp(params_.sampleLoopStart, 0.0f, 0.98f)) * (numS - 4);
            const double lEnd = static_cast<double>(std::clamp(params_.sampleLoopEnd, 0.02f, 1.0f)) * (numS - 4);
            const double effectiveEnd = std::max(lStart + 100.0, lEnd);

            if (params_.sampleLoopMode == 3) {
                // Reverse loop mode: playhead travels backward
                sampleDirection_ = -1.0;
                if (sMorph > 0.001) {
                    const double range = effectiveEnd - lStart;
                    samplePlayhead_ = std::clamp(effectiveEnd - sMorph * range, lStart, effectiveEnd);
                } else {
                    samplePlayhead_ = effectiveEnd;
                }
            } else {
                // Forward, Ping-Pong, or One-Shot
                sampleDirection_ = 1.0;
                samplePlayhead_ = sMorph * static_cast<double>(numS - 4);
            }
        } else {
            samplePlayhead_ = 0.0;
            sampleDirection_ = 1.0;
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
                                          float& outRight,
                                          float levelMod) noexcept {
    const float effLevel = std::clamp(params_.level + levelMod, 0.0f, 1.0f);
    if (!params_.enabled || effLevel <= 0.0001f) {
        return;
    }

    const float sr = static_cast<float>(sampleRate_);
    const float invSr = 1.0f / sr;
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
            constexpr float kUnisonNormGain = 0.85f / 2.2360679775f; // 0.85 / sqrt(5)
            const float normGain = (activeUnison > 1) ? kUnisonNormGain : 1.0f;

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
            const float twoPi = 2.0f * std::numbers::pi_v<float>;
            const int tableIdx = std::clamp(params_.modelIndex, 0, kWtTables - 1);

            // Organic internal wavetable scanning so held chords breathe and evolve even without external LFO
            const float scanRate = (0.12f + 0.45f * params_.rate) * (0.7f + 0.6f * detuneAmt);
            texGrainPhase_ += scanRate * invSr;
            if (texGrainPhase_ >= 1.0f) texGrainPhase_ -= 1.0f;

            const int activeUnison = (detuneAmt > 0.02f) ? kMaxUnison : 1;
            const float normGain = (activeUnison > 1) ? (0.85f / std::sqrt(static_cast<float>(activeUnison))) : 1.0f;

            for (int u = 0; u < activeUnison; ++u) {
                const auto idx = static_cast<std::size_t>(u);
                const float uFreq = std::min(sr * 0.46f, slotFreq * unisonPitchMultipliers_[idx]);
                const float dt = uFreq * invSr;

                float ph = phases_[idx] + dt;
                if (ph >= 1.0f) ph -= 1.0f;
                phases_[idx] = ph;

                // Subtle per-unison organic wavetable frame drift creates lush stereo spectral movement
                const float uScan = std::sin(twoPi * (texGrainPhase_ + static_cast<float>(u) * 0.21f)) *
                                    (0.18f * (0.25f + 0.75f * detuneAmt));
                // Morph across the 4 frames (0..3) of the selected wavetable with WT POS / shape
                const float fPos = std::clamp(shape * 3.0f + uScan, 0.0f, 3.0f);
                const int frame0 = std::clamp(static_cast<int>(fPos), 0, kWtFrames - 2);
                const int frame1 = frame0 + 1;
                const float frameFrac = fPos - static_cast<float>(frame0);

                // Asymmetric wavetable warp (controlled by params_.warp knob)
                float phWarped = ph;
                const float warpAmt = std::clamp(params_.warp, 0.0f, 1.0f);
                if (warpAmt > 0.01f) {
                    if (phWarped < 0.5f) {
                        phWarped = 0.5f * std::pow(phWarped * 2.0f, 1.0f + warpAmt * 2.2f);
                    } else {
                        phWarped = 1.0f - 0.5f * std::pow((1.0f - phWarped) * 2.0f, 1.0f + warpAmt * 2.2f);
                    }
                }

                const float tableIdxF = phWarped * static_cast<float>(kWtSize);
                const int s0 = static_cast<int>(tableIdxF) & (kWtSize - 1);
                const int s1 = (s0 + 1) & (kWtSize - 1);
                const float sFrac = tableIdxF - std::floor(tableIdxF);

                const auto& activeTable = bank.tables[static_cast<std::size_t>(tableIdx)];
                const float v00 = activeTable[static_cast<std::size_t>(frame0)][static_cast<std::size_t>(s0)];
                const float v01 = activeTable[static_cast<std::size_t>(frame0)][static_cast<std::size_t>(s1)];
                const float v10 = activeTable[static_cast<std::size_t>(frame1)][static_cast<std::size_t>(s0)];
                const float v11 = activeTable[static_cast<std::size_t>(frame1)][static_cast<std::size_t>(s1)];

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
            const int activeUnison = (detuneAmt > 0.02f) ? kMaxUnison : 3;
            const float normGain = 0.82f / std::sqrt(static_cast<float>(activeUnison));

            // Advance all unison phases for true stereo ensemble width
            for (int u = 0; u < activeUnison; ++u) {
                const auto idx = static_cast<std::size_t>(u);
                const float uFreq = std::min(sr * 0.46f, slotFreq * unisonPitchMultipliers_[idx]);
                phases_[idx] += uFreq * invSr;
                if (phases_[idx] >= 1.0f) phases_[idx] -= 1.0f;
            }

            // Texture modulation rate from parameters (rate knob + detune)
            const float grainRate = (0.35f + 2.4f * params_.rate) * (0.75f + 0.85f * detuneAmt);
            texGrainPhase_ += grainRate * invSr;
            if (texGrainPhase_ >= 1.0f) texGrainPhase_ -= 1.0f;
            texGrainPhase2_ += (grainRate * 1.382f) * invSr;
            if (texGrainPhase2_ >= 1.0f) texGrainPhase2_ -= 1.0f;

            rng_ = rng_ * 1664525u + 1013904223u;
            const float noiseSample = ((static_cast<float>(rng_ & 0xFFFFu) / 32767.5f) - 1.0f);

            float texL = 0.0f;
            float texR = 0.0f;

            switch (params_.modelIndex) {
                case 0: // Ambient Choir Breath (5-voice stereo vowel-formant vocal choir + aspirated breath halo)
                    {
                        const float vowelMorph = std::clamp(shape + 0.22f * std::sin(twoPi * texGrainPhase_), 0.0f, 1.0f);
                        // Formant harmonic ratios morph between warm "Ohhh" (F1~2x, F2~5x) and open "Ahhh" (F1~3x, F2~7x)
                        const float form1Ratio = 2.0f + 1.2f * vowelMorph;
                        const float form2Ratio = 5.0f + 2.4f * vowelMorph;

                        for (int u = 0; u < activeUnison; ++u) {
                            const auto idx = static_cast<std::size_t>(u);
                            const float ph = phases_[idx];
                            const float win = 0.5f * (1.0f - std::cos(twoPi * ph)); // Smooth Hann formant window
                            const float fund = std::sin(twoPi * ph);
                            const float f1 = std::sin(twoPi * ph * form1Ratio) * win;
                            const float f2 = std::sin(twoPi * ph * form2Ratio) * win;
                            const float voice = fund * 0.48f + f1 * 0.44f + f2 * (0.20f + 0.16f * vowelMorph);

                            texL += voice * unisonPanL_[idx] * normGain;
                            texR += voice * unisonPanR_[idx] * normGain;
                        }

                        const float breathEnv = 0.78f + 0.22f * std::sin(twoPi * texGrainPhase_);
                        const float cutoff = std::clamp((850.0f + 2200.0f * shape) * invSr * 2.0f, 0.02f, 0.40f);
                        texFilterState1_ += cutoff * (noiseSample * 0.25f - texFilterState1_ - 0.32f * texFilterState2_);
                        texFilterState2_ += cutoff * texFilterState1_;

                        const float airLvl = 0.06f + 0.14f * shape;
                        texL = (texL * 0.90f + texFilterState1_ * airLvl) * breathEnv;
                        texR = (texR * 0.90f - texFilterState1_ * airLvl) * breathEnv;
                    }
                    break;

                case 1: // Bowed Cello Texture (Stereo bowed string section with rosin stick-slip & warm cello body)
                    {
                        float ensL = 0.0f;
                        float ensR = 0.0f;
                        for (int u = 0; u < activeUnison; ++u) {
                            const auto idx = static_cast<std::size_t>(u);
                            const float ph = phases_[idx];
                            const float dt = std::min(0.45f, slotFreq * unisonPitchMultipliers_[idx] * invSr);
                            const float saw = (2.0f * ph - 1.0f) - polyBlep(ph, dt);
                            // Warm asymmetric bowed string stick-slip waveshaping (zero DC offset via odd-symmetric polynomial + sub-octave body)
                            const float bowed = saw - 0.24f * (saw * saw * saw) + 0.18f * std::sin(twoPi * ph);
                            ensL += bowed * unisonPanL_[idx] * normGain;
                            ensR += bowed * unisonPanR_[idx] * normGain;
                        }

                        const float bowPressure = 0.80f + 0.20f * std::sin(twoPi * texGrainPhase_);
                        const float bowRosin = noiseSample * (0.04f + 0.08f * shape);

                        const float bodyCutoff = std::clamp((240.0f + 420.0f * shape) * invSr * 2.0f, 0.01f, 0.35f);
                        texFilterState1_ += bodyCutoff * ((0.5f * (ensL + ensR) + bowRosin) - texFilterState1_ - 0.48f * texFilterState2_);
                        texFilterState2_ += bodyCutoff * texFilterState1_;

                        texL = (ensL * 0.62f + texFilterState2_ * 0.58f) * bowPressure;
                        texR = (ensR * 0.62f + texFilterState1_ * 0.52f) * bowPressure;
                    }
                    break;

                case 2: // Tape Halo & Flutter (Vintage Mellotron / Tape String-Flute ensemble with wow, flutter & saturation)
                    {
                        flutterPhase_ += 5.4f * invSr;
                        if (flutterPhase_ >= 1.0f) flutterPhase_ -= 1.0f;

                        const float wow = std::sin(twoPi * texGrainPhase_) * (0.0028f + 0.0020f * shape);
                        const float flutter = std::sin(twoPi * flutterPhase_) * 0.0014f;

                        for (int u = 0; u < activeUnison; ++u) {
                            const auto idx = static_cast<std::size_t>(u);
                            const float tapePh = std::fmod(phases_[idx] + wow + flutter * ((u & 1) ? -1.0f : 1.0f) + 1.0f, 1.0f);
                            const float core = 0.65f * std::sin(twoPi * tapePh)
                                             + 0.32f * std::sin(2.0f * twoPi * tapePh + 0.8f)
                                             + 0.18f * std::sin(3.0f * twoPi * tapePh + 1.9f);
                            texL += core * unisonPanL_[idx] * normGain;
                            texR += core * unisonPanR_[idx] * normGain;
                        }

                        const float drive = 1.15f + 1.45f * shape;
                        const float tapeHiss = noiseSample * (0.018f + 0.020f * shape);
                        texL = std::tanh(texL * drive) * 0.78f + tapeHiss;
                        texR = std::tanh(texR * drive) * 0.78f - tapeHiss;
                    }
                    break;

                case 3: // Cathedral Rain Cloud (Asynchronous stereo harmonic grain cloud with octave/fifth shimmer particles)
                    {
                        constexpr std::array<float, 5> kGrainHarmonics = { 1.0f, 2.0f, 1.5f, 2.0f, 3.0f };
                        for (int u = 0; u < activeUnison; ++u) {
                            const auto idx = static_cast<std::size_t>(u);
                            const float gPh = std::fmod(texGrainPhase_ + static_cast<float>(u) * 0.20f, 1.0f);
                            const float win = std::sin(std::numbers::pi_v<float> * gPh);
                            const float env = win * win;
                            const float harm = kGrainHarmonics[idx];
                            const float carrier = std::sin(twoPi * phases_[idx] * harm)
                                                + (0.30f * shape) * std::sin(twoPi * phases_[idx] * (harm * 2.0f));
                            texL += carrier * env * unisonPanL_[idx] * normGain;
                            texR += carrier * env * unisonPanR_[idx] * normGain;
                        }

                        const float dropletClick = (noiseSample > (0.95f - shape * 0.06f)) ? (noiseSample * 0.45f) : 0.0f;
                        texL = texL * 0.92f + dropletClick;
                        texR = texR * 0.92f - dropletClick * 0.8f;
                    }
                    break;

                case 4: // Warm Wind & Air Swell (Tuned stereo orchestral woodwind/pipe ensemble + resonant breath air)
                default:
                    {
                        for (int u = 0; u < activeUnison; ++u) {
                            const auto idx = static_cast<std::size_t>(u);
                            const float ph = phases_[idx];
                            const float pipe = 0.72f * std::sin(twoPi * ph)
                                             + 0.22f * std::sin(2.0f * twoPi * ph + 1.1f)
                                             + (0.16f + 0.18f * shape) * std::sin(3.0f * twoPi * ph + 2.4f);
                            texL += pipe * unisonPanL_[idx] * normGain;
                            texR += pipe * unisonPanR_[idx] * normGain;
                        }

                        const float gust = 0.72f + 0.28f * std::sin(twoPi * texGrainPhase_ * 0.5f);
                        const float windQ = 0.18f + 0.55f * shape;
                        const float windCutoff = std::clamp(slotFreq * invSr * 2.0f, 0.01f, 0.42f);
                        texFilterState1_ += windCutoff * (noiseSample * 0.45f - texFilterState1_ - windQ * texFilterState2_);
                        texFilterState2_ += windCutoff * texFilterState1_;

                        const float airMix = 0.25f + 0.45f * shape;
                        texL = (texL * (1.0f - 0.30f * shape) + texFilterState1_ * airMix) * gust;
                        texR = (texR * (1.0f - 0.30f * shape) - texFilterState1_ * airMix) * gust;
                    }
                    break;
            }

            sigL = texL * 0.78f;
            sigR = texR * 0.78f;
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

                const auto readWaveguide = [this](const std::vector<float>& buf, float delaySamples) noexcept {
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
                case 3: // Attack Key Click: Punchy mechanical keybed & hammer contact transient
                    {
                        constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
                        const float clickDurSec = std::clamp(params_.duration > 0.001f ? params_.duration : (0.035f + 0.065f * shape), 0.015f, 0.150f);
                        const float clickProgress = phases_[0];
                        if (phases_[0] < 1.0f) {
                            phases_[0] += invSr / clickDurSec;
                        }

                        // Sharp exponential attack envelope with tactile contact decay
                        const float clickEnv = (clickProgress < 1.0f)
                            ? std::exp(-clickProgress * (4.5f + shape * 3.5f))
                            : 0.0f;

                        // Dual acoustic contact resonance:
                        // 1) High-frequency key contact snap (felt strike / hammer impact)
                        texFilterState2_ += 0.55f * (white - texFilterState2_);
                        const float snap = (white - texFilterState2_);

                        // 2) Resonant wooden keybed thump (hollow acoustic body thud around 800 - 1200 Hz)
                        texFilterState1_ += 0.16f * (white - texFilterState1_);
                        const float thud = texFilterState1_;

                        // 3) Acoustic mechanical contact pulse (sine click at ~1.4 kHz)
                        const float clickSine = std::sin(twoPi * std::min(1.0f, clickProgress) * 18.0f) * (1.0f - std::min(1.0f, clickProgress));

                        // Combined tactile transient burst
                        noiseSig = (snap * 1.6f + thud * 2.2f + clickSine * 0.55f) * clickEnv * 2.2f;

                        // Subtle vintage keybed noise floor while key is held
                        noiseSig += texFilterState1_ * 0.045f * (0.25f + 0.75f * shape);
                    }
                    break;
                default:
                    break;
            }

            sigL = noiseSig;
            sigR = noiseSig;
            break;
        }

        case EngineType::SamplePlayback: {
            if (sampleFinished_ || activeSampleZone_ == nullptr) break;
            const auto& smp = SampleBank::getSample(params_.modelIndex);
            const auto* zone = activeSampleZone_;
            if (zone->pcmL == nullptr || zone->numSamples <= 8) break;

            const float detuneCents = (std::clamp(params_.scatter, 0.0f, 1.0f) - 0.5f) * 100.0f;
            const float detuneRatio = std::pow(2.0f, detuneCents / 1200.0f);
            const double playbackRate = (static_cast<double>(slotFreq) * static_cast<double>(detuneRatio) / static_cast<double>(zone->rootFrequencyHz)) * (smp.sampleRate / sampleRate_);
            const double step = playbackRate * sampleDirection_;

            const int numS = zone->numSamples;
            const double lStart = std::clamp(static_cast<double>(params_.sampleLoopStart), 0.0, 0.98) * static_cast<double>(numS - 4);
            double lEnd = std::clamp(static_cast<double>(params_.sampleLoopEnd), 0.02, 1.0) * static_cast<double>(numS - 4);
            if (lEnd < lStart + 100.0) lEnd = std::min(static_cast<double>(numS - 4), lStart + 100.0);
            const double loopLen = lEnd - lStart;

            const double xfadeUser = static_cast<double>(std::clamp(params_.sampleCrossfade, 0.0f, 0.25f));
            const double xfadeLen = std::max(16.0, std::min(loopLen * 0.25, xfadeUser * loopLen));

            const int16_t* pcmL = zone->pcmL;
            const int16_t* pcmR = (zone->pcmR != nullptr) ? zone->pcmR : zone->pcmL;
            const float spreadAmt = std::clamp(params_.detune, 0.0f, 1.0f);
            const double rightDelay = static_cast<double>(spreadAmt * 0.015f * sampleRate_);

            float rawL = 0.0f;
            float rawR = 0.0f;

            switch (params_.sampleLoopMode) {
                case 0: { // One-Shot (No Looping)
                    if (samplePlayhead_ >= numS - 4 || samplePlayhead_ < 0.0) {
                        sampleFinished_ = true;
                        break;
                    }
                    rawL = readSampleHermite(pcmL, numS, samplePlayhead_);
                    rawR = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
                    samplePlayhead_ += step;
                    break;
                }

                case 1: { // Forward Loop with Equal-Power Crossfade
                    if (samplePlayhead_ < lStart) {
                        rawL = readSampleHermite(pcmL, numS, samplePlayhead_);
                        rawR = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
                    } else if (samplePlayhead_ < lEnd - xfadeLen) {
                        rawL = readSampleHermite(pcmL, numS, samplePlayhead_);
                        rawR = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
                    } else if (samplePlayhead_ < lEnd) {
                        const double t = (samplePlayhead_ - (lEnd - xfadeLen)) / xfadeLen;
                        const float angle = static_cast<float>(t * (std::numbers::pi * 0.5));
                        const float g1 = std::cos(angle);
                        const float g2 = std::sin(angle);

                        const double secPlayhead = lStart + (samplePlayhead_ - (lEnd - xfadeLen));
                        const float p1L = readSampleHermite(pcmL, numS, samplePlayhead_);
                        const float p1R = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
                        const float p2L = readSampleHermite(pcmL, numS, secPlayhead);
                        const float p2R = readSampleHermite(pcmR, numS, std::max(0.0, secPlayhead - rightDelay));

                        rawL = p1L * g1 + p2L * g2;
                        rawR = p1R * g1 + p2R * g2;
                    } else {
                        samplePlayhead_ = (lStart + xfadeLen) + std::fmod(samplePlayhead_ - lEnd, loopLen);
                        rawL = readSampleHermite(pcmL, numS, samplePlayhead_);
                        rawR = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
                    }
                    samplePlayhead_ += step;
                    break;
                }

                case 2: { // Ping-Pong Loop (<->)
                    if (sampleDirection_ > 0.0) {
                        if (samplePlayhead_ >= lEnd) {
                            sampleDirection_ = -1.0;
                            samplePlayhead_ = lEnd - (samplePlayhead_ - lEnd);
                        }
                    } else {
                        if (samplePlayhead_ <= lStart) {
                            sampleDirection_ = 1.0;
                            samplePlayhead_ = lStart + (lStart - samplePlayhead_);
                        }
                    }
                    rawL = readSampleHermite(pcmL, numS, samplePlayhead_);
                    rawR = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
                    samplePlayhead_ += playbackRate * sampleDirection_;
                    break;
                }

                case 3: { // Reverse Loop (<-) with Equal-Power Crossfade
                    if (samplePlayhead_ > lEnd) {
                        rawL = readSampleHermite(pcmL, numS, samplePlayhead_);
                        rawR = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
                    } else if (samplePlayhead_ > lStart + xfadeLen) {
                        rawL = readSampleHermite(pcmL, numS, samplePlayhead_);
                        rawR = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
                    } else if (samplePlayhead_ > lStart) {
                        const double t = ((lStart + xfadeLen) - samplePlayhead_) / xfadeLen;
                        const float angle = static_cast<float>(t * (std::numbers::pi * 0.5));
                        const float g1 = std::cos(angle);
                        const float g2 = std::sin(angle);

                        const double secPlayhead = lEnd - ((lStart + xfadeLen) - samplePlayhead_);
                        const float p1L = readSampleHermite(pcmL, numS, samplePlayhead_);
                        const float p1R = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
                        const float p2L = readSampleHermite(pcmL, numS, secPlayhead);
                        const float p2R = readSampleHermite(pcmR, numS, std::max(0.0, secPlayhead - rightDelay));

                        rawL = p1L * g1 + p2L * g2;
                        rawR = p1R * g1 + p2R * g2;
                    } else {
                        samplePlayhead_ = (lEnd - xfadeLen) - std::fmod(lStart - samplePlayhead_, loopLen);
                        rawL = readSampleHermite(pcmL, numS, samplePlayhead_);
                        rawR = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
                    }
                    samplePlayhead_ += step;
                    break;
                }
            }

            const float mid = 0.5f * (rawL + rawR);
            const float side = 0.5f * (rawL - rawR);
            sigL = mid + spreadAmt * side;
            sigR = mid - spreadAmt * side;
            break;
        }
    }

    // Apply slot pan with true constant-power stereo panning law across all engines
    const float uPan = std::clamp(params_.pan, -1.0f, 1.0f);
    const float panAngle = (uPan + 1.0f) * 0.25f * std::numbers::pi_v<float>;
    const float panGainL = std::cos(panAngle) * 1.41421356f;
    const float panGainR = std::sin(panAngle) * 1.41421356f;

    outLeft  += sigL * panGainL * effLevel;
    outRight += sigR * panGainR * effLevel;
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
                const int table = std::clamp(p.modelIndex, 0, kWtTables - 1);
                const float fPos = std::clamp(p.shapeMorph * 3.0f, 0.0f, 3.0f);
                const int frame0 = std::clamp(static_cast<int>(fPos), 0, kWtFrames - 2);
                const int frame1 = frame0 + 1;
                const float frameFrac = fPos - static_cast<float>(frame0);
                const int idx = static_cast<int>(u * static_cast<float>(kWtSize - 1));
                const auto& activeTable = bank.tables[static_cast<std::size_t>(table)];
                const float w0 = activeTable[static_cast<std::size_t>(frame0)][static_cast<std::size_t>(idx)];
                const float w1 = activeTable[static_cast<std::size_t>(frame1)][static_cast<std::size_t>(idx)];
                val = w0 + frameFrac * (w1 - w0);
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
