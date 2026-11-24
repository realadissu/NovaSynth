#pragma once

#include <array>
#include <cmath>
#include <algorithm>
#include <numbers>
#include <cstdint>
#include <cstddef>

namespace nova::dsp {

// Shared static 8-model x 4-frame bandlimited wavetable bank (initialized once on first prepare)
struct SharedWavetableBank {
    static constexpr int kWtTables = 8;
    static constexpr int kWtFrames = 4;
    static constexpr int kWtSize = 1024;
    std::array<std::array<std::array<float, kWtSize>, kWtFrames>, kWtTables> tables{};

    SharedWavetableBank() {
        constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
        auto dispPhase = [](int h) constexpr noexcept -> float {
            return (h <= 1) ? 0.0f : static_cast<float>((h - 1) * 2.39996323f);
        };

        for (int i = 0; i < kWtSize; ++i) {
            const float ph = static_cast<float>(i) / static_cast<float>(kWtSize);
            const float w = twoPi * ph;

            // Table 0: Worship Harmonics (Rich, warm worship pad overtone series)
            {
                float s0 = 0.0f;
                for (int h = 1; h <= 8; ++h) {
                    const float fh = static_cast<float>(h);
                    s0 += (1.0f / std::pow(fh, 1.20f)) * std::sin(fh * w + dispPhase(h));
                }
                tables[0][0][static_cast<std::size_t>(i)] = s0;

                float s1 = 0.0f;
                for (int h = 1; h <= 16; ++h) {
                    const float fh = static_cast<float>(h);
                    const float amp = (1.0f / std::pow(fh, 1.20f)) * (1.0f + 0.12f * std::cos(fh * 0.9f));
                    s1 += amp * std::sin(fh * w + dispPhase(h));
                }
                tables[0][1][static_cast<std::size_t>(i)] = s1;

                float s2 = 0.0f;
                for (int h = 1; h <= 24; ++h) {
                    const float fh = static_cast<float>(h);
                    const float amp = (1.0f / std::pow(fh, 1.15f)) * (1.0f + 0.15f * std::cos(fh * 1.2f));
                    s2 += amp * std::sin(fh * w + dispPhase(h));
                }
                tables[0][2][static_cast<std::size_t>(i)] = s2;

                float s3 = 0.0f;
                for (int h = 1; h <= 36; ++h) {
                    const float fh = static_cast<float>(h);
                    const float amp = (1.0f / std::pow(fh, 1.10f)) * (1.0f + 0.12f * std::sin(fh * 1.5f));
                    s3 += amp * std::sin(fh * w + dispPhase(h));
                }
                tables[0][3][static_cast<std::size_t>(i)] = s3;
            }

            // Table 1: Cathedral Drawbars (Hammond B3 Tonewheel Organ)
            tables[1][0][static_cast<std::size_t>(i)] = 0.85f * std::sin(2.0f * w) + 0.75f * std::sin(w) + 0.65f * std::sin(3.0f * w);
            tables[1][1][static_cast<std::size_t>(i)] = 0.85f * std::sin(2.0f * w) + 0.75f * std::sin(w) + 0.65f * std::sin(3.0f * w) + 0.55f * std::sin(4.0f * w);
            tables[1][2][static_cast<std::size_t>(i)] = 0.85f * std::sin(2.0f * w) + 0.75f * std::sin(w) + 0.65f * std::sin(3.0f * w)
                + 0.55f * std::sin(4.0f * w) + 0.50f * std::sin(6.0f * w) + 0.45f * std::sin(8.0f * w);
            tables[1][3][static_cast<std::size_t>(i)] = 0.90f * std::sin(2.0f * w) + 0.80f * std::sin(w) + 0.70f * std::sin(3.0f * w)
                + 0.65f * std::sin(4.0f * w) + 0.60f * std::sin(6.0f * w) + 0.55f * std::sin(8.0f * w)
                + 0.45f * std::sin(10.0f * w) + 0.40f * std::sin(12.0f * w) + 0.35f * std::sin(16.0f * w);

            // Table 2: Silky Velvet Strings (Orchestral Bowed String Ensemble)
            {
                float s0 = 0.0f;
                for (int h = 1; h <= 8; ++h) {
                    const float fh = static_cast<float>(h);
                    const float amp = (1.0f / std::pow(fh, 1.20f)) * (1.0f + 0.12f * std::exp(-1.50f * (fh - 3.0f) * (fh - 3.0f)));
                    s0 += amp * std::sin(fh * w + dispPhase(h));
                }
                tables[2][0][static_cast<std::size_t>(i)] = s0;

                float s1 = 0.0f;
                for (int h = 1; h <= 16; ++h) {
                    const float fh = static_cast<float>(h);
                    const float f1 = 0.14f * std::exp(-0.80f * (fh - 3.5f) * (fh - 3.5f));
                    const float f2 = 0.08f * std::exp(-0.60f * (fh - 8.5f) * (fh - 8.5f));
                    s1 += ((1.0f / std::pow(fh, 1.20f)) + f1 + f2) * std::sin(fh * w + dispPhase(h));
                }
                tables[2][1][static_cast<std::size_t>(i)] = s1;

                float s2 = 0.0f;
                for (int h = 1; h <= 24; ++h) {
                    const float fh = static_cast<float>(h);
                    const float body = (1.0f / std::pow(fh, 1.20f));
                    const float sheen = 0.12f * std::exp(-0.15f * (fh - 16.0f) * (fh - 16.0f));
                    s2 += (body + sheen) * std::sin(fh * w + dispPhase(h));
                }
                tables[2][2][static_cast<std::size_t>(i)] = s2;

                float s3 = 0.0f;
                for (int h = 1; h <= 32; ++h) {
                    const float fh = static_cast<float>(h);
                    const float body = (1.0f / std::pow(fh, 1.20f));
                    const float sheen = 0.18f * std::exp(-0.12f * (fh - 18.0f) * (fh - 18.0f));
                    s3 += (body + sheen) * std::sin(fh * w + dispPhase(h));
                }
                tables[2][3][static_cast<std::size_t>(i)] = s3;
            }

            // Table 3: Vocal Choir Formants (Human Acoustic Vowel Choir)
            {
                float s0 = 0.0f;
                for (int h = 1; h <= 16; ++h) {
                    const float fh = static_cast<float>(h);
                    const float f1 = 0.35f * std::exp(-1.50f * (fh - 2.0f) * (fh - 2.0f));
                    const float f2 = 0.18f * std::exp(-1.00f * (fh - 5.0f) * (fh - 5.0f));
                    s0 += ((1.0f / std::pow(fh, 1.20f)) * (h == 1 ? 0.70f : 0.20f) + f1 + f2) * std::sin(fh * w + dispPhase(h));
                }
                tables[3][0][static_cast<std::size_t>(i)] = s0;

                float s1 = 0.0f;
                for (int h = 1; h <= 18; ++h) {
                    const float fh = static_cast<float>(h);
                    const float f1 = 0.30f * std::exp(-1.20f * (fh - 3.2f) * (fh - 3.2f));
                    const float f2 = 0.20f * std::exp(-0.90f * (fh - 7.0f) * (fh - 7.0f));
                    s1 += ((1.0f / std::pow(fh, 1.20f)) * (h == 1 ? 0.65f : 0.20f) + f1 + f2) * std::sin(fh * w + dispPhase(h));
                }
                tables[3][1][static_cast<std::size_t>(i)] = s1;

                float s2 = 0.0f;
                for (int h = 1; h <= 22; ++h) {
                    const float fh = static_cast<float>(h);
                    const float f1 = 0.25f * std::exp(-0.90f * (fh - 5.2f) * (fh - 5.2f));
                    const float f2 = 0.22f * std::exp(-0.60f * (fh - 9.0f) * (fh - 9.0f));
                    const float f3 = 0.15f * std::exp(-0.35f * (fh - 16.0f) * (fh - 16.0f));
                    s2 += ((1.0f / std::pow(fh, 1.20f)) * (h == 1 ? 0.60f : 0.20f) + f1 + f2 + f3) * std::sin(fh * w + dispPhase(h));
                }
                tables[3][2][static_cast<std::size_t>(i)] = s2;

                float s3 = 0.0f;
                for (int h = 1; h <= 24; ++h) {
                    const float fh = static_cast<float>(h);
                    const float f1 = 0.25f * std::exp(-1.20f * (fh - 2.2f) * (fh - 2.2f));
                    const float f2 = 0.28f * std::exp(-0.30f * (fh - 15.0f) * (fh - 15.0f));
                    const float f3 = 0.18f * std::exp(-0.25f * (fh - 20.0f) * (fh - 20.0f));
                    s3 += ((1.0f / std::pow(fh, 1.20f)) * (h == 1 ? 0.55f : 0.18f) + f1 + f2 + f3) * std::sin(fh * w + dispPhase(h));
                }
                tables[3][3][static_cast<std::size_t>(i)] = s3;
            }

            // Table 4: Harmonic Octave Pad (Pristine Octave Stacks)
            tables[4][0][static_cast<std::size_t>(i)] = 0.85f * std::sin(w) + 0.55f * std::sin(2.0f * w) + 0.35f * std::sin(4.0f * w);
            tables[4][1][static_cast<std::size_t>(i)] = 0.75f * std::sin(w) + 0.65f * std::sin(2.0f * w) + 0.45f * std::sin(4.0f * w) + 0.30f * std::sin(8.0f * w);
            tables[4][2][static_cast<std::size_t>(i)] = 0.65f * std::sin(w) + 0.55f * std::sin(2.0f * w) + 0.30f * std::sin(3.0f * w)
                + 0.45f * std::sin(4.0f * w) + 0.25f * std::sin(6.0f * w) + 0.35f * std::sin(8.0f * w) + 0.20f * std::sin(16.0f * w);
            tables[4][3][static_cast<std::size_t>(i)] = 0.55f * std::sin(w) + 0.50f * std::sin(2.0f * w) + 0.25f * std::sin(3.0f * w)
                + 0.40f * std::sin(4.0f * w) + 0.22f * std::sin(6.0f * w) + 0.32f * std::sin(8.0f * w)
                + 0.22f * std::sin(12.0f * w) + 0.26f * std::sin(16.0f * w) + 0.18f * std::sin(24.0f * w) + 0.15f * std::sin(32.0f * w);

            // Table 5: Glass Bell Chimes (Metallic Inharmonic Bell / D-50 / DX7 Tine)
            tables[5][0][static_cast<std::size_t>(i)] = 0.85f * std::sin(w) + 0.35f * std::sin(2.0f * w) + 0.25f * std::sin(3.0f * w) + 0.12f * std::sin(5.0f * w);
            tables[5][1][static_cast<std::size_t>(i)] = 0.60f * std::sin(w) + 0.45f * std::sin(3.0f * w) + 0.38f * std::sin(5.0f * w)
                + 0.32f * std::sin(7.0f * w) + 0.25f * std::sin(11.0f * w);
            tables[5][2][static_cast<std::size_t>(i)] = 0.50f * std::sin(w) + 0.30f * std::sin(2.0f * w) + 0.40f * std::sin(3.0f * w)
                + 0.35f * std::sin(5.0f * w) + 0.30f * std::sin(7.0f * w) + 0.28f * std::sin(11.0f * w)
                + 0.22f * std::sin(14.0f * w) + 0.18f * std::sin(19.0f * w);
            tables[5][3][static_cast<std::size_t>(i)] = 0.40f * std::sin(w) + 0.35f * std::sin(3.0f * w) + 0.32f * std::sin(5.0f * w)
                + 0.30f * std::sin(7.0f * w) + 0.28f * std::sin(11.0f * w) + 0.24f * std::sin(14.0f * w)
                + 0.22f * std::sin(19.0f * w) + 0.18f * std::sin(23.0f * w) + 0.15f * std::sin(27.0f * w);

            // Table 6: Warm Brass Swell (Oberheim OB-Xa / CS-80 Analog Brass)
            {
                float s0 = 0.0f;
                for (int h = 1; h <= 6; ++h) {
                    const float fh = static_cast<float>(h);
                    s0 += (1.0f / std::pow(fh, 1.20f)) * std::sin(fh * w);
                }
                tables[6][0][static_cast<std::size_t>(i)] = s0;

                float s1 = 0.0f;
                for (int h = 1; h <= 12; ++h) {
                    const float fh = static_cast<float>(h);
                    const float hornPeak = 0.18f * std::exp(-0.80f * (fh - 3.5f) * (fh - 3.5f));
                    s1 += ((1.0f / std::pow(fh, 1.20f)) + hornPeak) * std::sin(fh * w);
                }
                tables[6][1][static_cast<std::size_t>(i)] = s1;

                float s2 = 0.0f;
                for (int h = 1; h <= 18; ++h) {
                    const float fh = static_cast<float>(h);
                    const float hornPeak = 0.22f * std::exp(-0.50f * (fh - 4.2f) * (fh - 4.2f));
                    s2 += ((1.0f / std::pow(fh, 1.20f)) + hornPeak) * std::sin(fh * w);
                }
                tables[6][2][static_cast<std::size_t>(i)] = s2;

                float s3 = 0.0f;
                for (int h = 1; h <= 34; ++h) {
                    const float fh = static_cast<float>(h);
                    const float hornPeak = 0.26f * std::exp(-0.35f * (fh - 5.0f) * (fh - 5.0f));
                    s3 += ((1.0f / std::pow(fh, 1.20f)) + hornPeak) * std::sin(fh * w);
                }
                tables[6][3][static_cast<std::size_t>(i)] = s3;
            }

            // Table 7: Celestial Shimmer Air (High Air Halo / Angelic Sizzle)
            tables[7][0][static_cast<std::size_t>(i)] = 0.75f * std::sin(w) + 0.35f * std::sin(4.0f * w) + 0.25f * std::sin(8.0f * w);
            tables[7][1][static_cast<std::size_t>(i)] = 0.55f * std::sin(w) + 0.40f * std::sin(4.0f * w) + 0.35f * std::sin(6.0f * w)
                + 0.30f * std::sin(8.0f * w) + 0.22f * std::sin(12.0f * w);
            tables[7][2][static_cast<std::size_t>(i)] = 0.45f * std::sin(w) + 0.35f * std::sin(4.0f * w) + 0.32f * std::sin(8.0f * w)
                + 0.28f * std::sin(12.0f * w) + 0.24f * std::sin(16.0f * w) + 0.20f * std::sin(20.0f * w) + 0.16f * std::sin(24.0f * w);
            tables[7][3][static_cast<std::size_t>(i)] = 0.35f * std::sin(w) + 0.30f * std::sin(4.0f * w) + 0.28f * std::sin(8.0f * w)
                + 0.26f * std::sin(12.0f * w) + 0.24f * std::sin(16.0f * w) + 0.20f * std::sin(20.0f * w)
                + 0.20f * std::sin(24.0f * w) + 0.18f * std::sin(28.0f * w) + 0.15f * std::sin(32.0f * w);
        }

        // Normalize all tables & frames to consistent RMS (~0.52)
        for (std::size_t t = 0; t < static_cast<std::size_t>(kWtTables); ++t) {
            for (std::size_t f = 0; f < static_cast<std::size_t>(kWtFrames); ++f) {
                float sumSq = 0.0f;
                for (std::size_t i = 0; i < static_cast<std::size_t>(kWtSize); ++i) {
                    sumSq += tables[t][f][i] * tables[t][f][i];
                }
                const float rms = std::sqrt(sumSq / static_cast<float>(kWtSize));
                const float gain = (rms > 1e-5f) ? (0.52f / rms) : 1.0f;
                for (std::size_t i = 0; i < static_cast<std::size_t>(kWtSize); ++i) {
                    tables[t][f][i] *= gain;
                }
            }
        }
    }
};

inline const SharedWavetableBank& getWavetableBank() noexcept {
    static const SharedWavetableBank bank;
    return bank;
}

inline constexpr std::array<float, 5> kUnisonDetuneFactors = { 0.0f, -0.42f, 0.42f, -0.88f, 0.88f };
inline constexpr std::array<float, 5> kUnisonPanPositions  = { 0.0f, -0.55f, 0.55f, -0.90f, 0.90f };

inline float readSampleHermite(const int16_t* pcm, int numSamples, double playhead) noexcept {
    if (pcm == nullptr || numSamples <= 0) return 0.0f;
    const double clampedPos = std::clamp(playhead, 0.0, static_cast<double>(numSamples - 1));
    const int i = static_cast<int>(clampedPos);
    const float frac = static_cast<float>(clampedPos - static_cast<double>(i));

    const float y0 = static_cast<float>(i > 0 ? pcm[i - 1] : pcm[i]) * (1.0f / 32768.0f);
    const float y1 = static_cast<float>(pcm[i]) * (1.0f / 32768.0f);
    const float y2 = static_cast<float>(i + 1 < numSamples ? pcm[i + 1] : pcm[i]) * (1.0f / 32768.0f);
    const float y3 = static_cast<float>(i + 2 < numSamples ? pcm[i + 2] : (i + 1 < numSamples ? pcm[i + 1] : pcm[i])) * (1.0f / 32768.0f);

    const float c0 = y1, c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

} // namespace nova::dsp
