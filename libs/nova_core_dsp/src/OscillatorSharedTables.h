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
                    s0 += (1.0f / std::pow(fh, 1.10f)) * std::sin(fh * w + dispPhase(h));
                }
                tables[0][0][static_cast<std::size_t>(i)] = s0;

                float s1 = 0.0f;
                for (int h = 1; h <= 16; ++h) {
                    const float fh = static_cast<float>(h);
                    const float amp = (1.0f / std::pow(fh, 0.98f)) * (1.0f + 0.20f * std::cos(fh * 0.9f));
                    s1 += amp * std::sin(fh * w + dispPhase(h));
                }
                tables[0][1][static_cast<std::size_t>(i)] = s1;

                float s2 = 0.0f;
                for (int h = 1; h <= 24; ++h) {
                    const float fh = static_cast<float>(h);
                    const float amp = (1.0f / std::pow(fh, 0.92f)) * (1.0f + 0.25f * std::cos(fh * 1.2f));
                    s2 += amp * std::sin(fh * w + dispPhase(h));
                }
                tables[0][2][static_cast<std::size_t>(i)] = s2;

                float s3 = 0.0f;
                for (int h = 1; h <= 36; ++h) {
                    const float fh = static_cast<float>(h);
                    const float amp = (1.0f / std::pow(fh, 0.85f)) * (1.0f + 0.20f * std::sin(fh * 1.5f));
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

            // Table 3: Vocal Choir Formants (Human Acoustic Vowel Choir)
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
                for (int h = 1; h <= 34; ++h) {
                    const float fh = static_cast<float>(h);
                    const float hornPeak = 0.85f * std::exp(-0.22f * (fh - 5.0f) * (fh - 5.0f));
                    s3 += ((1.0f / std::pow(fh, 0.68f)) + hornPeak) * std::sin(fh * w);
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

// Shared static 6-model high-resolution texture bank (4096 samples each, 8 evolving cycles)
struct SharedTextureSourceBank {
    static constexpr int kTexTables = 6;
    static constexpr int kTexSize = 4096;
    static constexpr int kCycles = 8;
    static constexpr int kCycleSize = kTexSize / kCycles; // 512 samples per cycle
    std::array<std::array<float, kTexSize>, kTexTables> tables{};

    SharedTextureSourceBank() {
        constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
        auto dispPhase = [](int h) constexpr noexcept -> float {
            return (h <= 1) ? 0.0f : static_cast<float>((h - 1) * 2.39996323f);
        };

        for (int i = 0; i < kTexSize; ++i) {
            const float phCycle = static_cast<float>(i % kCycleSize) / static_cast<float>(kCycleSize);
            const float phTotal = static_cast<float>(i) / static_cast<float>(kTexSize);
            const float w = twoPi * phCycle;
            const float wTot = twoPi * phTotal;

            // Table 0: Ambient Choir Breath
            {
                float v = 0.0f;
                for (int h = 1; h <= 24; ++h) {
                    const float fh = static_cast<float>(h);
                    const float f1 = 0.95f * std::exp(-0.70f * (fh - 3.5f) * (fh - 3.5f));
                    const float f2 = 0.75f * std::exp(-0.45f * (fh - 8.0f) * (fh - 8.0f));
                    const float f3 = 0.40f * std::exp(-0.30f * (fh - 17.0f) * (fh - 17.0f));
                    const float baseAmp = 0.50f / std::pow(fh, 0.75f);
                    const float mod = 1.0f + 0.18f * std::sin(wTot * 2.0f + fh * 0.7f);
                    v += (baseAmp + f1 + f2 + f3) * mod * std::sin(fh * w + dispPhase(h));
                }
                const float breath = 0.12f * std::sin(32.0f * w + 1.2f) * (0.8f + 0.2f * std::sin(wTot * 4.0f));
                tables[0][static_cast<std::size_t>(i)] = v + breath;
            }

            // Table 1: Bowed Cello Texture
            {
                float s = 0.0f;
                for (int h = 1; h <= 28; ++h) {
                    const float fh = static_cast<float>(h);
                    const float m1 = 0.85f * std::exp(-1.20f * (fh - 1.0f) * (fh - 1.0f));
                    const float m2 = 0.70f * std::exp(-0.90f * (fh - 2.0f) * (fh - 2.0f));
                    const float m4 = 0.55f * std::exp(-0.60f * (fh - 4.0f) * (fh - 4.0f));
                    const float m8 = 0.35f * std::exp(-0.40f * (fh - 8.0f) * (fh - 8.0f));
                    const float sawAmp = 1.0f / fh;
                    const float slip = (h % 2 == 1) ? 1.15f : 0.85f;
                    const float bowMotion = 1.0f + 0.15f * std::cos(wTot * 3.0f + fh * 0.4f);
                    s += (sawAmp * slip + m1 + m2 + m4 + m8) * bowMotion * std::sin(fh * w + dispPhase(h));
                }
                tables[1][static_cast<std::size_t>(i)] = s;
            }

            // Table 2: Tape Halo & Flutter
            {
                float tape = 0.0f;
                for (int h = 1; h <= 18; ++h) {
                    const float fh = static_cast<float>(h);
                    const float oddWeight = (h % 2 == 1) ? 1.25f : 0.65f;
                    const float amp = (1.0f / std::pow(fh, 0.90f)) * oddWeight;
                    tape += amp * std::sin(fh * w + dispPhase(h));
                }
                tables[2][static_cast<std::size_t>(i)] = std::tanh(tape * 1.35f);
            }

            // Table 3: Cathedral Rain Cloud / Ethereal Grains
            {
                const float fund = 0.60f * std::sin(w), h2 = 0.45f * std::sin(2.0f * w + dispPhase(2));
                const float h3 = 0.38f * std::sin(3.0f * w + dispPhase(3)), h4 = 0.32f * std::sin(4.0f * w + dispPhase(4));
                const float h6 = 0.24f * std::sin(6.0f * w + dispPhase(6)), h8 = 0.20f * std::sin(8.0f * w + dispPhase(8));
                const float h12 = 0.15f * std::sin(12.0f * w + dispPhase(12)), h16 = 0.12f * std::sin(16.0f * w + dispPhase(16));
                const float bell = 0.18f * std::sin(2.756f * w * 2.0f + wTot * 2.0f) + 0.12f * std::sin(5.404f * w * 2.0f + wTot * 3.0f);
                tables[3][static_cast<std::size_t>(i)] = fund + h2 + h3 + h4 + h6 + h8 + h12 + h16 + bell;
            }

            // Table 4: Warm Wind & Air Swell
            {
                float pipe = 0.0f;
                for (int h = 1; h <= 14; ++h) {
                    const float fh = static_cast<float>(h);
                    const float oddPref = (h % 2 == 1) ? 1.35f : 0.45f;
                    const float amp = (1.0f / std::pow(fh, 1.05f)) * oddPref;
                    pipe += amp * std::sin(fh * w + dispPhase(h));
                }
                const float airChamber = 0.15f * std::sin(21.0f * w + 0.8f) * (0.7f + 0.3f * std::cos(wTot * 2.0f));
                tables[4][static_cast<std::size_t>(i)] = pipe + airChamber;
            }

            // Table 5: Velvet Warmth Pad
            {
                float pad = 0.0f;
                for (int h = 1; h <= 18; ++h) {
                    const float fh = static_cast<float>(h);
                    const float rolloff = 1.0f / (1.0f + (fh / 2.8f) * (fh / 2.8f));
                    const float warmSaw = (1.0f / fh) * rolloff;
                    const float singingCore = 0.70f * std::exp(-0.75f * (fh - 2.0f) * (fh - 2.0f));
                    const float evolve = 1.0f + 0.14f * std::sin(wTot * 2.0f + fh * 0.45f);
                    pad += (warmSaw + singingCore) * evolve * std::sin(fh * w + dispPhase(h));
                }
                const float warmSub = 0.28f * std::sin(0.5f * w);
                tables[5][static_cast<std::size_t>(i)] = std::tanh((pad + warmSub) * 1.25f);
            }
        }

        // Normalize all 6 tables to nominal RMS (~0.50)
        for (std::size_t t = 0; t < static_cast<std::size_t>(kTexTables); ++t) {
            float sumSq = 0.0f;
            for (std::size_t i = 0; i < static_cast<std::size_t>(kTexSize); ++i) {
                sumSq += tables[t][i] * tables[t][i];
            }
            const float rms = std::sqrt(sumSq / static_cast<float>(kTexSize));
            const float gain = (rms > 1e-5f) ? (0.50f / rms) : 1.0f;
            for (std::size_t i = 0; i < static_cast<std::size_t>(kTexSize); ++i) {
                tables[t][i] *= gain;
            }
        }
    }
};

inline const SharedTextureSourceBank& getTextureSourceBank() noexcept {
    static const SharedTextureSourceBank bank;
    return bank;
}

inline float readTextureHermite(const std::array<float, 4096>& table, float pos) noexcept {
    const int i = static_cast<int>(pos);
    const float frac = pos - static_cast<float>(i);
    const int i0 = (i - 1) & 4095, i1 = i & 4095;
    const int i2 = (i + 1) & 4095, i3 = (i + 2) & 4095;

    const float y0 = table[static_cast<std::size_t>(i0)], y1 = table[static_cast<std::size_t>(i1)];
    const float y2 = table[static_cast<std::size_t>(i2)], y3 = table[static_cast<std::size_t>(i3)];

    const float c0 = y1, c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
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
