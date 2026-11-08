#include "NovaSynthProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>

namespace {
void writeWavFile(const std::string& path, const std::vector<float>& left, const std::vector<float>& right, int sampleRate) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Failed to open output file: " << path << std::endl;
        return;
    }

    const uint32_t numSamples = static_cast<uint32_t>(left.size());
    const uint16_t numChannels = 2;
    const uint16_t bitsPerSample = 16;
    const uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
    const uint16_t blockAlign = numChannels * (bitsPerSample / 8);
    const uint32_t subchunk2Size = numSamples * numChannels * (bitsPerSample / 8);
    const uint32_t chunkSize = 36 + subchunk2Size;

    // RIFF Header
    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&chunkSize), 4);
    out.write("WAVE", 4);

    // fmt subchunk
    out.write("fmt ", 4);
    const uint32_t subchunk1Size = 16;
    const uint16_t audioFormat = 1; // PCM
    out.write(reinterpret_cast<const char*>(&subchunk1Size), 4);
    out.write(reinterpret_cast<const char*>(&audioFormat), 2);
    out.write(reinterpret_cast<const char*>(&numChannels), 2);
    const uint32_t sr = static_cast<uint32_t>(sampleRate);
    out.write(reinterpret_cast<const char*>(&sr), 4);
    out.write(reinterpret_cast<const char*>(&byteRate), 4);
    out.write(reinterpret_cast<const char*>(&blockAlign), 2);
    out.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // data subchunk
    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&subchunk2Size), 4);

    for (std::size_t i = 0; i < left.size(); ++i) {
        float l = std::clamp(left[i], -1.0f, 1.0f);
        float r = std::clamp(right[i], -1.0f, 1.0f);
        int16_t sampL = static_cast<int16_t>(l * 32767.0f);
        int16_t sampR = static_cast<int16_t>(r * 32767.0f);
        out.write(reinterpret_cast<const char*>(&sampL), 2);
        out.write(reinterpret_cast<const char*>(&sampR), 2);
    }
}
} // namespace

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI juceInit;

    int presetIdx = 44; // Default to Preset 44 ("Ambient Swell Pad")
    std::string outPath = "rendered_pad.wav";
    float durationSec = 6.0f;
    float swellPedal = 0.70f;
    bool shimmerBypass = true;

    if (argc >= 2) presetIdx = std::stoi(argv[1]);
    if (argc >= 3) outPath = argv[2];
    if (argc >= 4) durationSec = std::stof(argv[3]);
    if (argc >= 5) swellPedal = std::stof(argv[4]);
    if (argc >= 6) shimmerBypass = (std::stoi(argv[5]) != 0);

    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;

    nova::synth::NovaSynthProcessor proc;
    proc.prepareToPlay(kSampleRate, kBlockSize);

    auto& pm = proc.getPresetManager();
    const auto& allPresets = pm.getAllPresets();
    const int numPresets = static_cast<int>(allPresets.size());
    if (presetIdx >= 0 && presetIdx < numPresets) {
        pm.loadPresetByIndex(presetIdx);
        std::cout << "Loaded Preset [" << presetIdx << "]: " << allPresets[static_cast<std::size_t>(presetIdx)].name.toStdString() << std::endl;
    } else {
        std::cout << "Using current/init patch (preset index " << presetIdx << " out of bounds, count=" << numPresets << ")" << std::endl;
    }

    // Swell Pedal / Expression (CC11) via parameter or MIDI
    if (auto* p = proc.getAPVTS().getParameter("swellPedal")) {
        p->setValueNotifyingHost(swellPedal);
    }

    if (shimmerBypass) {
        if (auto* p = proc.getAPVTS().getParameter("macroShimmer")) p->setValueNotifyingHost(0.0f);
        if (auto* p = proc.getAPVTS().getParameter("reverbShimmer")) p->setValueNotifyingHost(0.0f);
    }

    const int totalSamples = static_cast<int>(durationSec * kSampleRate);
    std::vector<float> recL, recR;
    recL.reserve(static_cast<std::size_t>(totalSamples));
    recR.reserve(static_cast<std::size_t>(totalSamples));

    juce::AudioBuffer<float> buffer(2, kBlockSize);
    juce::MidiBuffer midi;

    // Classic worship voicing: Eb major 9 (Eb2, Bb2, G3, Bb3, D4, F4)
    const std::vector<int> worshipChord = { 39, 46, 55, 58, 62, 65 };
    for (int note : worshipChord) {
        midi.addEvent(juce::MidiMessage::noteOn(1, note, 0.85f), 0);
    }

    int samplesProcessed = 0;
    while (samplesProcessed < totalSamples) {
        const int curBlock = std::min(kBlockSize, totalSamples - samplesProcessed);
        buffer.clear();
        proc.processBlock(buffer, midi);
        midi.clear(); // Note-ons only sent at block 0

        const float* readL = buffer.getReadPointer(0);
        const float* readR = buffer.getReadPointer(1);
        for (int i = 0; i < curBlock; ++i) {
            recL.push_back(readL[i]);
            recR.push_back(readR[i]);
        }
        samplesProcessed += curBlock;
    }

    writeWavFile(outPath, recL, recR, static_cast<int>(kSampleRate));
    std::cout << "Successfully rendered " << durationSec << "s audio to: " << outPath << std::endl;
    return 0;
}
