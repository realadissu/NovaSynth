#include "nova_dsp/oscillators/MultiEngineOscillator.h"
#include "nova_dsp/oscillators/SampleBank.h"
#include "OscillatorSharedTables.h"
#include <cmath>
#include <algorithm>
#include <numbers>
#include <vector>

namespace nova::dsp {

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
            return { "Classic Saw", "Pulse / Square", "Warm Triangle", "Mellow Sine", "Super Saw Multi", "Hard Sync Lead", "Velvet Analog Pad" };
        case EngineType::EvolvingWavetable:
            return { "Worship Harmonics", "Cathedral Drawbars", "Silky Velvet Strings", "Vocal Choir Formants",
                     "Harmonic Octave Pad", "Glass Bell Chimes", "Warm Brass Swell", "Celestial Shimmer Air" };
        case EngineType::OrganicTexture:
            return { "Ambient Choir Breath", "Bowed Cello Texture", "Tape Halo & Flutter", "Cathedral Rain Cloud", "Warm Wind & Air Swell", "Velvet Warmth Pad" };
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
                    case 5: val = 2.0f * std::fmod(u * (1.0f + 2.0f * p.shapeMorph), 1.0f) - 1.0f; break; // Hard Sync
                    case 6: default: val = std::tanh(((2.0f * u - 1.0f) * 0.7f + (2.0f * std::abs(2.0f * (std::fmod(u * 0.5f, 1.0f) - 0.5f)) - 1.0f) * 0.3f) * 1.3f); break; // Velvet Analog Pad
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
            case EngineType::OrganicTexture: {
                const auto& texBank = getTextureSourceBank();
                const int model = std::clamp(p.modelIndex, 0, SharedTextureSourceBank::kTexTables - 1);
                const int idx = std::clamp(static_cast<int>(u * static_cast<float>(SharedTextureSourceBank::kTexSize - 1)), 0, SharedTextureSourceBank::kTexSize - 1);
                val = texBank.tables[static_cast<std::size_t>(model)][static_cast<std::size_t>(idx)];
                break;
            }
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

