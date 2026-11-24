#include "nova_dsp/oscillators/PadSynthBank.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace nova::dsp::pad_detail {

namespace {

void addGaussianPartial(std::vector<double>& power,
                        std::vector<double>& scratchWeights,
                        double freqHz,
                        double amp,
                        double bwCents,
                        double binHz,
                        double maxFreqHz) {
    if (freqHz < 22.0 || freqHz >= maxFreqHz || amp <= 0.0) {
        return;
    }

    const double bwHz = (std::pow(2.0, bwCents / 1200.0) - 1.0) * freqHz;
    const double sigmaHz = std::max(binHz * 0.75, 0.5 * bwHz);
    const double sigmaBins = sigmaHz / binHz;
    const double centerBin = freqHz / binHz;

    const int radius = static_cast<int>(std::ceil(4.0 * sigmaBins));
    const int numBins = static_cast<int>(power.size());
    const int bMin = std::max(1, static_cast<int>(std::floor(centerBin)) - radius);
    const int bMax = std::min(numBins - 2, static_cast<int>(std::ceil(centerBin)) + radius);
    if (bMax < bMin) {
        return;
    }

    const int count = bMax - bMin + 1;
    if (static_cast<int>(scratchWeights.size()) < count) {
        scratchWeights.resize(static_cast<std::size_t>(count));
    }

    double energySum = 0.0;
    for (int i = 0; i < count; ++i) {
        const double d = (static_cast<double>(bMin + i) - centerBin) / sigmaBins;
        const double g2 = std::exp(-d * d);
        scratchWeights[static_cast<std::size_t>(i)] = g2;
        energySum += g2;
    }

    if (energySum <= 1e-18) {
        return;
    }

    const double scale = (amp * amp) / energySum;
    for (int i = 0; i < count; ++i) {
        power[static_cast<std::size_t>(bMin + i)] += scale * scratchWeights[static_cast<std::size_t>(i)];
    }
}

[[nodiscard]] double gaussianBump(double freqHz, double centerHz, double widthHz) noexcept {
    const double d = (freqHz - centerHz) / widthHz;
    return std::exp(-d * d);
}

[[nodiscard]] double computeContour(int modelIndex, double freqHz) noexcept {
    switch (modelIndex) {
        case 0: { // Ambient Choir Breath: soft /oo/-/ah/ fixed vowel formants + nasal carve
            const double f1 = 1.45 * gaussianBump(freqHz, 380.0, 140.0);
            const double f2 = 0.95 * gaussianBump(freqHz, 920.0, 220.0);
            const double f3 = 0.62 * gaussianBump(freqHz, 2550.0, 480.0);
            const double nasalDip = 1.0 - 0.35 * gaussianBump(freqHz, 1550.0, 220.0);
            return std::max(0.08, (0.18 + f1 + f2 + f3) * nasalDip);
        }
        case 1: { // Bowed Cello Texture: woody body + anti-nasal dip + bow sheen
            const double woodBody = 0.75 * gaussianBump(freqHz, 290.0, 135.0);
            const double nasalDip = 0.42 * gaussianBump(freqHz, 950.0, 230.0);
            const double bowSheen = 0.55 * gaussianBump(freqHz, 5200.0, 2100.0);
            return std::max(0.25, 1.0 + woodBody - nasalDip + bowSheen);
        }
        case 2: { // Tape Halo & Flutter: warm head bump + gentle 5.5 kHz tape rolloff
            const double headBump = 1.0 + 0.45 * gaussianBump(freqHz, 210.0, 110.0);
            const double ratio = freqHz / 5500.0;
            const double rolloff = 1.0 / std::sqrt(1.0 + ratio * ratio * ratio * ratio);
            return headBump * rolloff;
        }
        case 3: { // Cathedral Rain Cloud / Crystal Glass: low poise + crystal presence
            const double lowPoise = 0.75 + 0.25 * std::min(1.0, freqHz / 250.0);
            const double crystalLift = 0.85 * gaussianBump(freqHz, 4800.0, 2600.0);
            return lowPoise * (1.0 + crystalLift);
        }
        case 4: { // Warm Wind & Air Swell: hollow mid scoop for weightless transparency
            return std::max(0.45, 1.0 - 0.40 * gaussianBump(freqHz, 1200.0, 450.0));
        }
        case 5:
        default: { // Velvet Warmth Pad: rich low-mid body + gentle mid dip + silky 6.8 kHz sheen
            const double warmth = 0.35 * gaussianBump(freqHz, 260.0, 140.0);
            const double midDip = 0.25 * gaussianBump(freqHz, 1100.0, 260.0);
            const double silk = 0.42 * gaussianBump(freqHz, 6800.0, 1800.0);
            return std::max(0.45, 1.0 + warmth - midDip + silk);
        }
    }
}

void addSpectralCloud(std::vector<double>& power,
                      double baseTotalPower,
                      double lowHz,
                      double highHz,
                      double fadeInHz,
                      double centerHz,
                      double sigmaHz,
                      double targetFractionOfBase,
                      double binHz) {
    if (baseTotalPower <= 1e-18 || targetFractionOfBase <= 0.0) {
        return;
    }

    const int numBins = static_cast<int>(power.size());
    const int bStart = std::max(1, static_cast<int>(std::ceil(lowHz / binHz)));
    const int bEnd = std::min(numBins - 2, static_cast<int>(std::floor(highHz / binHz)));
    if (bEnd <= bStart) {
        return;
    }

    std::vector<double> shape(static_cast<std::size_t>(bEnd - bStart + 1), 0.0);
    double sumShape = 0.0;
    for (int b = bStart; b <= bEnd; ++b) {
        const double f = static_cast<double>(b) * binHz;
        double w = gaussianBump(f, centerHz, sigmaHz);
        if (fadeInHz > 0.0 && f < lowHz + fadeInHz) {
            const double t = (f - lowHz) / fadeInHz;
            w *= 0.5 * (1.0 - std::cos(std::numbers::pi * t));
        }
        shape[static_cast<std::size_t>(b - bStart)] = w;
        sumShape += w;
    }

    if (sumShape <= 1e-18) {
        return;
    }

    const double scale = (targetFractionOfBase * baseTotalPower) / sumShape;
    for (int b = bStart; b <= bEnd; ++b) {
        power[static_cast<std::size_t>(b)] += scale * shape[static_cast<std::size_t>(b - bStart)];
    }
}

} // namespace

void buildPadModelSpectrum(int modelIndex,
                           int zoneIndex,
                           double rootHz,
                           double maxFreqHz,
                           std::vector<double>& outAmp) {
    constexpr int kNumBins = PadSynthBank::kTableSize / 2 + 1;
    constexpr double kBinHz = PadSynthBank::kRefSampleRate / static_cast<double>(PadSynthBank::kTableSize);

    const double zoneMaxHz = (zoneIndex >= 3)
        ? std::min(maxFreqHz, 11800.0)
        : std::min(maxFreqHz, 15000.0);
    const double taperStartHz = (zoneIndex >= 3) ? 9800.0 : 12800.0;

    outAmp.assign(static_cast<std::size_t>(kNumBins), 0.0);
    std::vector<double> power(static_cast<std::size_t>(kNumBins), 0.0);
    std::vector<double> scratchWeights;

    switch (modelIndex) {
        case 0: { // Ambient Choir Breath
            for (int h = 1; h <= 48; ++h) {
                const double fh = static_cast<double>(h);
                const double amp = ((h == 1) ? 1.15 : 1.0) / std::pow(fh, 1.05);
                const double bw = 38.0 * (1.0 + 0.06 * std::log(fh));
                addGaussianPartial(power, scratchWeights, fh * rootHz, amp, bw, kBinHz, zoneMaxHz);
            }
            break;
        }
        case 1: { // Bowed Cello Texture
            for (int h = 1; h <= 56; ++h) {
                const double fh = static_cast<double>(h);
                const double oddEven = (h % 2 == 1) ? 1.08 : 0.92;
                const double amp = (1.0 / std::pow(fh, 1.02)) * oddEven;
                const double bw = 27.0 * (1.0 + 0.08 * std::log(fh));
                addGaussianPartial(power, scratchWeights, fh * rootHz, amp, bw, kBinHz, zoneMaxHz);
            }
            break;
        }
        case 2: { // Tape Halo & Flutter
            for (int h = 1; h <= 28; ++h) {
                const double fh = static_cast<double>(h);
                const double oddWeight = (h % 2 == 1) ? 1.35 : 0.42;
                const double amp = oddWeight / std::pow(fh, 1.28);
                const double bw = 24.0 * (1.0 + 0.05 * std::log(fh));
                addGaussianPartial(power, scratchWeights, fh * rootHz, amp, bw, kBinHz, zoneMaxHz);
            }
            break;
        }
        case 3: { // Cathedral Rain Cloud (Crystal / Glass Bell Pad)
            constexpr std::array<double, 10> kRatios = {
                1.0, 2.0, 2.756, 3.0, 4.0, 5.404, 6.0, 8.0, 10.81, 12.0
            };
            constexpr std::array<double, 10> kAmps = {
                0.72, 0.85, 0.58, 0.44, 0.68, 0.42, 0.46, 0.52, 0.24, 0.18
            };
            for (std::size_t i = 0; i < kRatios.size(); ++i) {
                const double bw = 26.0 + 4.5 * std::log(kRatios[i]);
                addGaussianPartial(power, scratchWeights, kRatios[i] * rootHz, kAmps[i], bw, kBinHz, zoneMaxHz);
            }
            break;
        }
        case 4: { // Warm Wind & Air Swell
            constexpr std::array<int, 11> kAnchors = {
                1, 2, 3, 4, 6, 8, 10, 12, 16, 20, 24
            };
            for (int h : kAnchors) {
                const double fh = static_cast<double>(h);
                const double amp = (h == 1) ? 0.85 : ((h == 2) ? 0.48 : (0.30 / std::pow(fh, 0.75)));
                const double bw = 56.0 * (1.0 + 0.08 * std::log(fh));
                addGaussianPartial(power, scratchWeights, fh * rootHz, amp, bw, kBinHz, zoneMaxHz);
            }
            break;
        }
        case 5:
        default: { // Velvet Warmth Pad
            if (zoneIndex >= 1) {
                addGaussianPartial(power, scratchWeights, 0.5 * rootHz, 0.36, 34.0, kBinHz, zoneMaxHz);
            }
            for (int h = 1; h <= 48; ++h) {
                const double fh = static_cast<double>(h);
                const double amp = 1.0 / std::pow(fh, 1.10);
                const double bw = 35.0 * (1.0 + 0.06 * std::log(std::max(1.0, fh)));
                addGaussianPartial(power, scratchWeights, fh * rootHz, amp, bw, kBinHz, zoneMaxHz);
            }
            break;
        }
    }

    double shapedPowerSum = 0.0;
    for (int b = 1; b < kNumBins - 1; ++b) {
        const double f = static_cast<double>(b) * kBinHz;
        const double c = computeContour(modelIndex, f);
        power[static_cast<std::size_t>(b)] *= (c * c);
        shapedPowerSum += power[static_cast<std::size_t>(b)];
    }

    if (modelIndex == 0) {
        const double centerHz = (zoneIndex >= 3) ? 7000.0 : 7600.0;
        addSpectralCloud(power, shapedPowerSum, 5500.0, std::min(11000.0, zoneMaxHz),
                         800.0, centerHz, 1750.0, 0.018, kBinHz);
    } else if (modelIndex == 4) {
        addSpectralCloud(power, shapedPowerSum, 3500.0, std::min(13000.0, zoneMaxHz),
                         1100.0, 5800.0, 2800.0, 0.85, kBinHz);
    }

    for (int b = 1; b < kNumBins - 1; ++b) {
        const double f = static_cast<double>(b) * kBinHz;
        if (f >= zoneMaxHz) {
            outAmp[static_cast<std::size_t>(b)] = 0.0;
            continue;
        }
        double taper = 1.0;
        if (f < 35.0) {
            const double t = std::clamp((f - 20.0) / 15.0, 0.0, 1.0);
            taper *= 0.5 * (1.0 - std::cos(std::numbers::pi * t));
        }
        if (f > taperStartHz) {
            const double t = std::clamp((f - taperStartHz) / (zoneMaxHz - taperStartHz), 0.0, 1.0);
            taper *= 0.5 * (1.0 + std::cos(std::numbers::pi * t));
        }
        outAmp[static_cast<std::size_t>(b)] = std::sqrt(std::max(0.0, power[static_cast<std::size_t>(b)])) * taper;
    }

    outAmp[0] = 0.0;
    outAmp[static_cast<std::size_t>(kNumBins - 1)] = 0.0;
}

} // namespace nova::dsp::pad_detail
