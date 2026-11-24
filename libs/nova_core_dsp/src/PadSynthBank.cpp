#include "nova_dsp/oscillators/PadSynthBank.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>
#include <vector>

namespace nova::dsp::pad_detail {
void buildPadModelSpectrum(int modelIndex,
                           int zoneIndex,
                           double rootHz,
                           double maxFreqHz,
                           std::vector<double>& outAmp);
} // namespace nova::dsp::pad_detail

namespace nova::dsp {

namespace {

[[nodiscard]] uint64_t splitMix64(uint64_t& state) noexcept {
    state += 0x9E3779B97F4A7C15ull;
    uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

[[nodiscard]] double nextPhase0To2Pi(uint64_t& state) noexcept {
    constexpr double kInv53 = 1.0 / 9007199254740992.0; // 2^-53
    const double u = static_cast<double>(splitMix64(state) >> 11) * kInv53;
    return u * (2.0 * std::numbers::pi);
}

void computeRadix2IfftInPlace(std::vector<double>& re, std::vector<double>& im) noexcept {
    const int n = static_cast<int>(re.size());
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(re[static_cast<std::size_t>(i)], re[static_cast<std::size_t>(j)]);
            std::swap(im[static_cast<std::size_t>(i)], im[static_cast<std::size_t>(j)]);
        }
    }

    for (int len = 2; len <= n; len <<= 1) {
        const double ang = (2.0 * std::numbers::pi) / static_cast<double>(len);
        const double wLenRe = std::cos(ang);
        const double wLenIm = std::sin(ang);
        const int half = len >> 1;
        for (int i = 0; i < n; i += len) {
            double wRe = 1.0;
            double wIm = 0.0;
            for (int j = 0; j < half; ++j) {
                const auto idxEven = static_cast<std::size_t>(i + j);
                const auto idxOdd = static_cast<std::size_t>(i + j + half);
                const double uRe = re[idxEven];
                const double uIm = im[idxEven];
                const double vRe = re[idxOdd] * wRe - im[idxOdd] * wIm;
                const double vIm = re[idxOdd] * wIm + im[idxOdd] * wRe;
                re[idxEven] = uRe + vRe;
                im[idxEven] = uIm + vIm;
                re[idxOdd] = uRe - vRe;
                im[idxOdd] = uIm - vIm;
                const double nextWRe = wRe * wLenRe - wIm * wLenIm;
                wIm = wRe * wLenIm + wIm * wLenRe;
                wRe = nextWRe;
            }
        }
    }

    const double invN = 1.0 / static_cast<double>(n);
    for (int i = 0; i < n; ++i) {
        re[static_cast<std::size_t>(i)] *= invN;
        im[static_cast<std::size_t>(i)] *= invN;
    }
}

void removeDcAndNormalizeRms(std::vector<float>& channel, float targetRms) noexcept {
    if (channel.empty()) {
        return;
    }
    double sum = 0.0;
    for (float s : channel) {
        sum += static_cast<double>(s);
    }
    const double mean = sum / static_cast<double>(channel.size());
    double sumSq = 0.0;
    for (float& s : channel) {
        const double centered = static_cast<double>(s) - mean;
        s = static_cast<float>(centered);
        sumSq += centered * centered;
    }
    const double rms = std::sqrt(sumSq / static_cast<double>(channel.size()));
    const float gain = (rms > 1e-9) ? static_cast<float>(static_cast<double>(targetRms) / rms) : 1.0f;
    for (float& s : channel) {
        s *= gain;
    }
}

} // namespace

PadSynthBank::PadSynthBank() {
    constexpr int kHalfN = kTableSize / 2;
    std::vector<double> amp(static_cast<std::size_t>(kHalfN + 1), 0.0);
    std::vector<double> re(static_cast<std::size_t>(kTableSize), 0.0);
    std::vector<double> im(static_cast<std::size_t>(kTableSize), 0.0);

    for (int m = 0; m < kNumModels; ++m) {
        for (int z = 0; z < kNumZones; ++z) {
            const double rootHz = static_cast<double>(kZoneRootHz[static_cast<std::size_t>(z)]);
            pad_detail::buildPadModelSpectrum(m, z, rootHz, 18500.0, amp);

            double totalPower = 0.0;
            for (int k = 1; k < kHalfN; ++k) {
                const double a = amp[static_cast<std::size_t>(k)];
                totalPower += a * a;
            }

            uint64_t bestSeed = 0;
            double bestAbsCorr = 1e9;
            for (int attempt = 0; attempt < 64; ++attempt) {
                const uint64_t trialSeed = 0x51A7B3C9D2E4F103ull
                                       ^ (static_cast<uint64_t>(m + 1) * 0x9E3779B97F4A7C15ull)
                                       ^ (static_cast<uint64_t>(z + 1) * 0xBF58476D1CE4E5B9ull)
                                       ^ (static_cast<uint64_t>(attempt) * 0x94D049BB133111EBull);
                uint64_t s = trialSeed;
                double crossSum = 0.0;
                for (int k = 1; k < kHalfN; ++k) {
                    const double a = amp[static_cast<std::size_t>(k)];
                    const double pL = nextPhase0To2Pi(s);
                    const double pR = nextPhase0To2Pi(s);
                    if (a > 0.0) {
                        crossSum += (a * a) * std::cos(pL - pR);
                    }
                }
                const double absCorr = (totalPower > 1e-18) ? std::abs(crossSum / totalPower) : 0.0;
                if (absCorr < bestAbsCorr) {
                    bestAbsCorr = absCorr;
                    bestSeed = trialSeed;
                    if (absCorr < 0.08) {
                        break;
                    }
                }
            }

            uint64_t rngState = bestSeed;

            std::fill(re.begin(), re.end(), 0.0);
            std::fill(im.begin(), im.end(), 0.0);

            for (int k = 1; k < kHalfN; ++k) {
                const double a = amp[static_cast<std::size_t>(k)];
                const double phaseL = nextPhase0To2Pi(rngState);
                const double phaseR = nextPhase0To2Pi(rngState);
                if (a <= 0.0) {
                    continue;
                }
                const double aL = a * std::cos(phaseL);
                const double bL = a * std::sin(phaseL);
                const double aR = a * std::cos(phaseR);
                const double bR = a * std::sin(phaseR);
                re[static_cast<std::size_t>(k)] = aL - bR;
                im[static_cast<std::size_t>(k)] = bL + aR;
                re[static_cast<std::size_t>(kTableSize - k)] = aL + bR;
                im[static_cast<std::size_t>(kTableSize - k)] = -bL + aR;
            }

            computeRadix2IfftInPlace(re, im);

            auto& zone = tables_[static_cast<std::size_t>(m)][static_cast<std::size_t>(z)];
            zone.left.resize(static_cast<std::size_t>(kTableSize));
            zone.right.resize(static_cast<std::size_t>(kTableSize));
            for (int n = 0; n < kTableSize; ++n) {
                zone.left[static_cast<std::size_t>(n)] = static_cast<float>(re[static_cast<std::size_t>(n)]);
                zone.right[static_cast<std::size_t>(n)] = static_cast<float>(im[static_cast<std::size_t>(n)]);
            }

            removeDcAndNormalizeRms(zone.left, kTargetRms);
            removeDcAndNormalizeRms(zone.right, kTargetRms);
        }
    }
}

const PadSynthBank::ZoneTable& PadSynthBank::getZone(int modelIndex, int zoneIndex) const noexcept {
    const int m = std::clamp(modelIndex, 0, kNumModels - 1);
    const int z = std::clamp(zoneIndex, 0, kNumZones - 1);
    return tables_[static_cast<std::size_t>(m)][static_cast<std::size_t>(z)];
}

float PadSynthBank::readHermite(const float* table, double pos) noexcept {
    const double fFloor = std::floor(pos);
    const auto iBase = static_cast<int64_t>(fFloor);
    const float frac = static_cast<float>(pos - fFloor);
    const int i0 = static_cast<int>((iBase - 1) & kTableMask);
    const int i1 = static_cast<int>(iBase & kTableMask);
    const int i2 = static_cast<int>((iBase + 1) & kTableMask);
    const int i3 = static_cast<int>((iBase + 2) & kTableMask);

    const float y0 = table[i0];
    const float y1 = table[i1];
    const float y2 = table[i2];
    const float y3 = table[i3];

    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void PadSynthBank::sampleZoneCrossfaded(const PadSynthBank& bank,
                                        int modelIndex,
                                        float freqHz,
                                        double basePosZone0,
                                        float& outL,
                                        float& outR) noexcept {
    const float zoneF = std::clamp(std::log2(std::max(20.0f, freqHz) / kZoneRootHz[0]),
                                   0.0f,
                                   static_cast<float>(kNumZones - 1));
    const int z0 = std::min(static_cast<int>(zoneF), kNumZones - 2);
    const int z1 = z0 + 1;
    const float t = zoneF - static_cast<float>(z0);
    const float angle = t * (0.5f * std::numbers::pi_v<float>);
    const float w0 = std::cos(angle);
    const float w1 = std::sin(angle);

    float l0 = 0.0f, r0 = 0.0f;
    if (w0 >= 1e-4f) {
        const auto& zone0 = bank.getZone(modelIndex, z0);
        const double pos0 = basePosZone0 / static_cast<double>(1 << z0);
        l0 = readHermite(zone0.left.data(), pos0);
        r0 = readHermite(zone0.right.data(), pos0);
    }

    float l1 = 0.0f, r1 = 0.0f;
    if (w1 >= 1e-4f) {
        const auto& zone1 = bank.getZone(modelIndex, z1);
        const double pos1 = basePosZone0 / static_cast<double>(1 << z1);
        l1 = readHermite(zone1.left.data(), pos1);
        r1 = readHermite(zone1.right.data(), pos1);
    }

    outL = w0 * l0 + w1 * l1;
    outR = w0 * r0 + w1 * r1;
}

const PadSynthBank& getPadSynthBank() noexcept {
    static const PadSynthBank bank;
    return bank;
}

} // namespace nova::dsp
