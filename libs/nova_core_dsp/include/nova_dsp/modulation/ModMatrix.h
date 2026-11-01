#pragma once

#include <array>
#include <cstddef>
#include <algorithm>

namespace nova::dsp {

/**
 * @brief Realtime-safe 32-Slot Modulation Matrix (JORNA-style).
 * Maps Modulation Sources (Envelopes, LFOs, Macros, Velocity, ModWheel) to Synth Destinations.
 */
class ModMatrix {
public:
    static constexpr std::size_t kMaxSlots = 32;

    enum class Source : int {
        None = 0,
        AmpEnv,
        FilterEnv,
        ModEnv3,
        AuxEnv4,
        Lfo1,
        Lfo2,
        Lfo3,
        Lfo4,
        Velocity,
        ModWheel,
        MacroWarmth,
        MacroMotion,
        MacroShimmer,
        MacroAir,
        Count
    };

    static const char* getSourceName(Source src) noexcept {
        switch (src) {
            case Source::None:         return "None";
            case Source::AmpEnv:       return "Amp Env";
            case Source::FilterEnv:    return "Filter Env";
            case Source::ModEnv3:      return "Mod Env 3";
            case Source::AuxEnv4:      return "Aux Env 4";
            case Source::Lfo1:         return "LFO 1";
            case Source::Lfo2:         return "LFO 2";
            case Source::Lfo3:         return "LFO 3";
            case Source::Lfo4:         return "LFO 4";
            case Source::Velocity:     return "Velocity";
            case Source::ModWheel:     return "Mod Wheel";
            case Source::MacroWarmth:  return "Warmth";
            case Source::MacroMotion:  return "Motion";
            case Source::MacroShimmer: return "Shimmer";
            case Source::MacroAir:     return "Air";
            case Source::Count:        return "None";
        }
    }

    enum class Destination : int {
        None = 0,
        Osc1Shape,
        Osc2Shape,
        Osc3Shape,
        Osc4Shape,
        OscPitchAll,
        FilterCutoff,
        FilterResonance,
        VocalCarve,
        ReverbMix,
        ShimmerAmount,
        PanSpread,
        Count
    };

    static const char* getDestinationName(Destination dst) noexcept {
        switch (dst) {
            case Destination::None:            return "None";
            case Destination::Osc1Shape:       return "OSC 1 Shape";
            case Destination::Osc2Shape:       return "OSC 2 Shape";
            case Destination::Osc3Shape:       return "OSC 3 Shape";
            case Destination::Osc4Shape:       return "OSC 4 Shape";
            case Destination::OscPitchAll:     return "Pitch (All)";
            case Destination::FilterCutoff:    return "Cutoff 1";
            case Destination::FilterResonance: return "Resonance";
            case Destination::VocalCarve:      return "Vocal Carve";
            case Destination::ReverbMix:       return "Reverb Mix";
            case Destination::ShimmerAmount:   return "Shimmer";
            case Destination::PanSpread:       return "Pan Spread";
            case Destination::Count:           return "None";
        }
    }

    struct Slot {
        Source source = Source::None;
        Destination destination = Destination::None;
        float amount = 0.0f; // -1.0 .. +1.0
        bool enabled = false;
    };

    void clear() noexcept {
        for (auto& s : slots_) {
            s = Slot{};
        }
        sourceValues_.fill(0.0f);
        destOffsets_.fill(0.0f);
    }

    void setSlot(std::size_t index, Source src, Destination dst, float amount, bool enabled = true) noexcept {
        if (index < kMaxSlots) {
            slots_[index] = Slot{ src, dst, std::clamp(amount, -1.0f, 1.0f), enabled };
        }
    }

    [[nodiscard]] const Slot& getSlot(std::size_t index) const noexcept {
        return slots_[std::min(index, kMaxSlots - 1)];
    }

    void setSourceValue(Source src, float value) noexcept {
        const auto idx = static_cast<std::size_t>(src);
        if (idx < static_cast<std::size_t>(Source::Count)) {
            sourceValues_[idx] = value;
        }
    }

    void evaluate() noexcept {
        destOffsets_.fill(0.0f);
        for (const auto& s : slots_) {
            if (!s.enabled || s.source == Source::None || s.destination == Destination::None) {
                continue;
            }
            const auto srcIdx = static_cast<std::size_t>(s.source);
            const auto dstIdx = static_cast<std::size_t>(s.destination);
            destOffsets_[dstIdx] += sourceValues_[srcIdx] * s.amount;
        }
    }

    [[nodiscard]] float getModulationOffset(Destination dst) const noexcept {
        const auto idx = static_cast<std::size_t>(dst);
        if (idx < static_cast<std::size_t>(Destination::Count)) {
            return destOffsets_[idx];
        }
        return 0.0f;
    }

private:
    std::array<Slot, kMaxSlots> slots_{};
    std::array<float, static_cast<std::size_t>(Source::Count)> sourceValues_{};
    std::array<float, static_cast<std::size_t>(Destination::Count)> destOffsets_{};
};

} // namespace nova::dsp
