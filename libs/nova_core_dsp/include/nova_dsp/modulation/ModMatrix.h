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
        // OSC Shape / Morph (1 - 8)
        Osc1Shape,
        Osc2Shape,
        Osc3Shape,
        Osc4Shape,
        Osc5Shape,
        Osc6Shape,
        Osc7Shape,
        Osc8Shape,

        // OSC Level / Swell (1 - 8)
        Osc1Level,
        Osc2Level,
        Osc3Level,
        Osc4Level,
        Osc5Level,
        Osc6Level,
        Osc7Level,
        Osc8Level,

        // OSC Pitch (1 - 8)
        Osc1Pitch,
        Osc2Pitch,
        Osc3Pitch,
        Osc4Pitch,
        Osc5Pitch,
        Osc6Pitch,
        Osc7Pitch,
        Osc8Pitch,
        OscPitchAll,

        // Dual Filters
        FilterCutoff,
        FilterResonance,
        Filter2Cutoff,
        Filter2Resonance,
        VocalCarve,

        // Envelopes
        AmpAttack,
        AmpDecay,
        AmpRelease,
        FilterAttack,
        FilterDecay,
        FilterRelease,
        ModEnv3Attack,
        ModEnv3Decay,
        ModEnv3Release,
        AuxEnv4Attack,
        AuxEnv4Decay,
        AuxEnv4Release,

        // LFO Rates
        Lfo1Rate,
        Lfo2Rate,
        Lfo3Rate,
        Lfo4Rate,

        // Space & FX
        ReverbMix,
        ShimmerAmount,
        PanSpread,
        DelayMix,
        DelayFeedback,

        Count
    };

    static const char* getDestinationName(Destination dst) noexcept {
        switch (dst) {
            case Destination::None:            return "None";
            case Destination::Osc1Shape:       return "OSC 1 Shape";
            case Destination::Osc2Shape:       return "OSC 2 Shape";
            case Destination::Osc3Shape:       return "OSC 3 Shape";
            case Destination::Osc4Shape:       return "OSC 4 Shape";
            case Destination::Osc5Shape:       return "OSC 5 Shape";
            case Destination::Osc6Shape:       return "OSC 6 Shape";
            case Destination::Osc7Shape:       return "OSC 7 Shape";
            case Destination::Osc8Shape:       return "OSC 8 Shape";

            case Destination::Osc1Level:       return "OSC 1 Level";
            case Destination::Osc2Level:       return "OSC 2 Level";
            case Destination::Osc3Level:       return "OSC 3 Level";
            case Destination::Osc4Level:       return "OSC 4 Level";
            case Destination::Osc5Level:       return "OSC 5 Level";
            case Destination::Osc6Level:       return "OSC 6 Level";
            case Destination::Osc7Level:       return "OSC 7 Level";
            case Destination::Osc8Level:       return "OSC 8 Level";

            case Destination::Osc1Pitch:       return "OSC 1 Pitch";
            case Destination::Osc2Pitch:       return "OSC 2 Pitch";
            case Destination::Osc3Pitch:       return "OSC 3 Pitch";
            case Destination::Osc4Pitch:       return "OSC 4 Pitch";
            case Destination::Osc5Pitch:       return "OSC 5 Pitch";
            case Destination::Osc6Pitch:       return "OSC 6 Pitch";
            case Destination::Osc7Pitch:       return "OSC 7 Pitch";
            case Destination::Osc8Pitch:       return "OSC 8 Pitch";
            case Destination::OscPitchAll:     return "Pitch (All)";

            case Destination::FilterCutoff:    return "Cutoff 1";
            case Destination::FilterResonance: return "Resonance 1";
            case Destination::Filter2Cutoff:   return "Cutoff 2";
            case Destination::Filter2Resonance:return "Resonance 2";
            case Destination::VocalCarve:      return "Vocal Carve";

            case Destination::AmpAttack:       return "Amp Attack";
            case Destination::AmpDecay:        return "Amp Decay";
            case Destination::AmpRelease:      return "Amp Release";
            case Destination::FilterAttack:    return "Filter Attack";
            case Destination::FilterDecay:     return "Filter Decay";
            case Destination::FilterRelease:   return "Filter Release";
            case Destination::ModEnv3Attack:   return "Mod 3 Attack";
            case Destination::ModEnv3Decay:    return "Mod 3 Decay";
            case Destination::ModEnv3Release:  return "Mod 3 Release";
            case Destination::AuxEnv4Attack:   return "Aux 4 Attack";
            case Destination::AuxEnv4Decay:    return "Aux 4 Decay";
            case Destination::AuxEnv4Release:  return "Aux 4 Release";

            case Destination::Lfo1Rate:        return "LFO 1 Rate";
            case Destination::Lfo2Rate:        return "LFO 2 Rate";
            case Destination::Lfo3Rate:        return "LFO 3 Rate";
            case Destination::Lfo4Rate:        return "LFO 4 Rate";

            case Destination::ReverbMix:       return "Reverb Mix";
            case Destination::ShimmerAmount:   return "Shimmer";
            case Destination::PanSpread:       return "Pan Spread";
            case Destination::DelayMix:        return "Delay Mix";
            case Destination::DelayFeedback:   return "Delay Feedback";
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

    [[nodiscard]] float getTotalDepth(Destination dst) const noexcept {
        float total = 0.0f;
        for (const auto& s : slots_) {
            if (s.enabled && s.destination == dst) {
                total += s.amount;
            }
        }
        return std::clamp(total, -1.0f, 1.0f);
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

    struct VoiceSources {
        float ampEnv = 0.0f;
        float filterEnv = 0.0f;
        float modEnv = 0.0f;
        float auxEnv = 0.0f;
        float velocity = 0.0f;
    };

    struct VoiceOffsets {
        std::array<float, static_cast<std::size_t>(Destination::Count)> offsets{};
    };

    [[nodiscard]] VoiceOffsets evaluateVoice(const VoiceSources& vs) const noexcept {
        VoiceOffsets vo;
        vo.offsets = destOffsets_;
        for (const auto& s : slots_) {
            if (!s.enabled || s.source == Source::None || s.destination == Destination::None) {
                continue;
            }
            float vVal = 0.0f;
            switch (s.source) {
                case Source::AmpEnv:    vVal = vs.ampEnv; break;
                case Source::FilterEnv: vVal = vs.filterEnv; break;
                case Source::ModEnv3:   vVal = vs.modEnv; break;
                case Source::AuxEnv4:   vVal = vs.auxEnv; break;
                case Source::Velocity:  vVal = vs.velocity; break;
                case Source::None:
                case Source::Lfo1:
                case Source::Lfo2:
                case Source::Lfo3:
                case Source::Lfo4:
                case Source::ModWheel:
                case Source::MacroWarmth:
                case Source::MacroMotion:
                case Source::MacroShimmer:
                case Source::MacroAir:
                case Source::Count:
                    break;
            }
            if (vVal != 0.0f) {
                const auto dstIdx = static_cast<std::size_t>(s.destination);
                vo.offsets[dstIdx] += vVal * s.amount;
            }
        }
        return vo;
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
