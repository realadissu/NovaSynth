#pragma once

#include <array>
#include <vector>

namespace nova::dsp {

/**
 * @brief Precomputed multi-zone stereo PADsynth ensemble wavetable bank.
 * Generates 6 distinct musical profiles across 4 octave zones (C2-C5) using
 * Gaussian harmonic broadening, fixed-frequency acoustic formant shaping,
 * and single-IFFT stereo phase decorrelation.
 */
class PadSynthBank {
public:
    static constexpr int kNumModels = 6;
    static constexpr int kNumZones = 4;
    static constexpr int kTableSize = 65536;
    static constexpr int kTableMask = kTableSize - 1;
    static constexpr double kRefSampleRate = 48000.0;
    static constexpr float kTargetRms = 0.48f;
    static constexpr std::array<float, kNumZones> kZoneRootHz = {
        65.40639f, 130.81278f, 261.62556f, 523.25113f
    };

    struct ZoneTable {
        std::vector<float> left;
        std::vector<float> right;
    };

    PadSynthBank();

    [[nodiscard]] const ZoneTable& getZone(int modelIndex, int zoneIndex) const noexcept;
    [[nodiscard]] static float readHermite(const float* table, double pos) noexcept;

    static void sampleZoneCrossfaded(const PadSynthBank& bank,
                                     int modelIndex,
                                     float freqHz,
                                     double basePosZone0,
                                     float& outL,
                                     float& outR) noexcept;

private:
    std::array<std::array<ZoneTable, kNumZones>, kNumModels> tables_{};
};

[[nodiscard]] const PadSynthBank& getPadSynthBank() noexcept;

} // namespace nova::dsp
