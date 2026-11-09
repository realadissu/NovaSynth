#include "NovaSynthProcessor.h"
#include "NovaSynthEditor.h"
#include "presets/FactoryPresets.h"
#include <chrono>

namespace nova::synth {

NovaSynthProcessor::NovaSynthProcessor()
    : NovaProcessorBase(
          BusesProperties()
              .withOutput("Output", juce::AudioChannelSet::stereo(), true),
          createParameterLayout(),
          "Nova Synth") {
    invertSustainPedalParam_ = apvts_.getRawParameterValue("invertSustainPedal");
    masterTuningHzParam_ = apvts_.getRawParameterValue("masterTuningHz");
    arpPatternLengthParam_ = apvts_.getRawParameterValue("arpPatternLength");
    for (int s = 0; s < 64; ++s) {
        arpStepGateParams_[static_cast<std::size_t>(s)] = apvts_.getRawParameterValue("arpStepGate_" + juce::String(s + 1));
        arpStepVelParams_[static_cast<std::size_t>(s)]  = apvts_.getRawParameterValue("arpStepVel_" + juce::String(s + 1));
    }
    registerFactoryPresets();
    syncParametersFromAPVTS();
}


void NovaSynthProcessor::registerFactoryPresets() {
    registerFactoryPresetsPads(presetManager_);
    registerFactoryPresetsPlucks(presetManager_);
    registerFactoryPresetsLeads(presetManager_);
    registerFactoryPresetsAcoustic(presetManager_);
    registerFactoryPresetsSwell(presetManager_);
}

void NovaSynthProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    currentSampleRate_ = sampleRate;
    synthEngine_.prepare(sampleRate, samplesPerBlock);

    invertSustainPedalParam_ = apvts_.getRawParameterValue("invertSustainPedal");
    arpPatternLengthParam_ = apvts_.getRawParameterValue("arpPatternLength");
    for (int s = 0; s < 64; ++s) {
        arpStepGateParams_[static_cast<std::size_t>(s)] = apvts_.getRawParameterValue("arpStepGate_" + juce::String(s + 1));
        arpStepVelParams_[static_cast<std::size_t>(s)]  = apvts_.getRawParameterValue("arpStepVel_" + juce::String(s + 1));
    }

    syncParametersFromAPVTS();
}

void NovaSynthProcessor::releaseResources() {
    synthEngine_.reset();
}

bool NovaSynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void NovaSynthProcessor::triggerAuditionNoteOn(int midiNote, float velocity) noexcept {
    uiNoteQueue_.pushOverwrite(PendingNoteEvent{ midiNote, velocity, true });
}

void NovaSynthProcessor::triggerAuditionNoteOff(int midiNote) noexcept {
    uiNoteQueue_.pushOverwrite(PendingNoteEvent{ midiNote, 0.0f, false });
}


void NovaSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    const auto startTime = std::chrono::high_resolution_clock::now();

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    if (auto* playHead = getPlayHead()) {
        if (auto posOpt = playHead->getPosition()) {
            if (auto bpmOpt = posOpt->getBpm()) {
                if (*bpmOpt >= 20.0 && *bpmOpt <= 400.0) {
                    currentBpm_ = *bpmOpt;
                    isHostBpmSynced_ = true;
                    setBpm(*bpmOpt, true);
                }
            }
        }
    }

    const bool isPanic = panicPending_.exchange(false, std::memory_order_acq_rel);
    if (isPanic) {
        synthEngine_.allNotesOff();
        synthEngine_.reset();
        synthEngine_.snapSmoothersToTarget();
        PendingNoteEvent ev{};
        while (uiNoteQueue_.pop(ev)) {}
        inputPeak_.store(0.0f, std::memory_order_relaxed);
        outputPeak_.store(0.0f, std::memory_order_relaxed);
    }

    if (arpPatternResetPending_.exchange(false, std::memory_order_acq_rel)) {
        synthEngine_.stopActiveArpVoices();
    }

    const bool isPresetChange = presetChangePending_.exchange(false, std::memory_order_acq_rel);
    if (isPresetChange) {
        synthEngine_.reset();
    }

    syncParametersFromAPVTS();

    if (isPresetChange) {
        synthEngine_.snapSmoothersToTarget();
    }

    PendingNoteEvent uiEv{};
    while (uiNoteQueue_.pop(uiEv)) {
        if (uiEv.isNoteOn) {
            synthEngine_.noteOn(uiEv.note, uiEv.velocity);
        } else {
            synthEngine_.noteOff(uiEv.note);
        }
    }

    const bool invertPedal = (invertSustainPedalParam_ != nullptr && invertSustainPedalParam_->load(std::memory_order_relaxed) > 0.5f);

    for (const auto metadata : midiMessages) {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn()) {
            synthEngine_.noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
        } else if (msg.isNoteOff()) {
            synthEngine_.noteOff(msg.getNoteNumber());
        } else if (msg.isController() && msg.getControllerNumber() == 64) {
            const bool isDown = (msg.getControllerValue() >= 64);
            synthEngine_.setSustainPedal(invertPedal ? !isDown : isDown);
        } else if (msg.isPitchWheel()) {
            const float bend = (static_cast<float>(msg.getPitchWheelValue()) - 8192.0f) / 8192.0f;
            synthEngine_.setPitchBend(bend * 2.0f);
        } else if (msg.isController() && (msg.getControllerNumber() == 1
                                          || msg.getControllerNumber() == 11
                                          || msg.getControllerNumber() == 4)) {
            synthEngine_.setModWheel(static_cast<float>(msg.getControllerValue()) / 127.0f);
        } else if (msg.isController()) {
            const int cc = msg.getControllerNumber();
            const int val = msg.getControllerValue();
            if (cc == 101) {
                rpnMsb_ = val;
            } else if (cc == 100) {
                rpnLsb_ = val;
            } else if (cc == 6 && rpnMsb_ == 0 && rpnLsb_ == 1) {
                // MIDI RPN 0x0001: Master Fine Tuning MSB (coarse, 64 = 440.0 Hz)
                dataEntryMsb_ = val;
                const int combined14 = (dataEntryMsb_ << 7);
                const float cents = (static_cast<float>(combined14 - 8192) / 8192.0f) * 100.0f;
                applyHostTuningHzFromMidi(440.0f * std::pow(2.0f, cents / 1200.0f));
            } else if (cc == 38 && rpnMsb_ == 0 && rpnLsb_ == 1) {
                // MIDI RPN 0x0001: Master Fine Tuning 14-bit (MSB + LSB, 8192 = 440.0 Hz)
                const int combined14 = (dataEntryMsb_ << 7) | (val & 0x7F);
                const float cents = (static_cast<float>(combined14 - 8192) / 8192.0f) * 100.0f;
                applyHostTuningHzFromMidi(440.0f * std::pow(2.0f, cents / 1200.0f));
            }
        } else if (msg.isSysEx()) {
            const uint8_t* syx = msg.getSysExData();
            const int syxLen = msg.getSysExDataSize();
            // Universal Real-Time (0x7F) or Non-Real-Time (0x7E) Master Fine Tuning:
            // [7F/7E, deviceId, 04 (Device Control), 03 (Master Fine Tuning), lsb, msb]
            if (syx != nullptr && syxLen >= 6 && (syx[0] == 0x7F || syx[0] == 0x7E)
                && syx[2] == 0x04 && syx[3] == 0x03) {
                const int combined14 = ((static_cast<int>(syx[5]) & 0x7F) << 7) | (static_cast<int>(syx[4]) & 0x7F);
                const float cents = (static_cast<float>(combined14 - 8192) / 8192.0f) * 100.0f;
                applyHostTuningHzFromMidi(440.0f * std::pow(2.0f, cents / 1200.0f));
            }
        } else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
            synthEngine_.allNotesOff();
        }
    }

    if (buffer.getNumChannels() >= 2 && numSamples > 0) {
        synthEngine_.processBlock(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    const double elapsedSec = std::chrono::duration<double>(endTime - startTime).count();
    const double blockDurSec = static_cast<double>(numSamples) / std::max(1.0, currentSampleRate_);

    const auto tel = synthEngine_.getTelemetry();
    const float outPeak = std::max(tel.outputPeakL, tel.outputPeakR);
    updateMetersAndCpu(outPeak * 0.85f, outPeak, blockDurSec, elapsedSec);

    // Update real-time mixer meters with decay
    const float pkL = tel.outputPeakL;
    const float pkR = tel.outputPeakR;
    constexpr float decay = 0.88f;

    const float curL = masterPeakL_.load(std::memory_order_relaxed);
    const float curR = masterPeakR_.load(std::memory_order_relaxed);
    masterPeakL_.store(std::max(pkL, curL * decay), std::memory_order_relaxed);
    masterPeakR_.store(std::max(pkR, curR * decay), std::memory_order_relaxed);

    const auto curP = currentParams_;
    for (int i = 0; i < 8; ++i) {
        float srcPk = 0.0f;
        if (i < curP.activeOscCount && curP.oscSlots[static_cast<std::size_t>(i)].enabled) {
            const float lvl = curP.oscSlots[static_cast<std::size_t>(i)].level;
            srcPk = std::max(pkL, pkR) * lvl;
        }
        const float curSrc = sourcePeaks_[static_cast<std::size_t>(i)].load(std::memory_order_relaxed);
        sourcePeaks_[static_cast<std::size_t>(i)].store(std::max(srcPk, curSrc * decay), std::memory_order_relaxed);
    }
}

void NovaSynthProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto& mm = synthEngine_.getModMatrix();
    auto modTree = apvts_.state.getOrCreateChildWithName("ModMatrix", nullptr);
    modTree.removeAllChildren(nullptr);
    for (std::size_t i = 0; i < nova::dsp::ModMatrix::kMaxSlots; ++i) {
        const auto& slot = mm.getSlot(i);
        if (slot.enabled || slot.source != nova::dsp::ModMatrix::Source::None) {
            juce::ValueTree slotTree("Slot");
            slotTree.setProperty("index", static_cast<int>(i), nullptr);
            slotTree.setProperty("src", static_cast<int>(slot.source), nullptr);
            slotTree.setProperty("dst", static_cast<int>(slot.destination), nullptr);
            slotTree.setProperty("amount", slot.amount, nullptr);
            slotTree.setProperty("enabled", slot.enabled, nullptr);
            modTree.addChild(slotTree, -1, nullptr);
        }
    }
    nova::plugin::NovaProcessorBase::getStateInformation(destData);
}

void NovaSynthProcessor::onPresetLoaded() {
    presetChangePending_.store(true, std::memory_order_release);
    auto& mm = synthEngine_.getModMatrix();
    mm.clear();
    const auto& allPresets = presetManager_.getAllPresets();
    const int idx = presetManager_.getCurrentPresetIndex();
    if (idx >= 0 && idx < static_cast<int>(allPresets.size())
        && !allPresets[static_cast<std::size_t>(idx)].modSlots.empty()) {
        const auto& slots = allPresets[static_cast<std::size_t>(idx)].modSlots;
        for (std::size_t i = 0; i < slots.size() && i < nova::dsp::ModMatrix::kMaxSlots; ++i) {
            mm.setSlot(i,
                       static_cast<nova::dsp::ModMatrix::Source>(slots[i].source),
                       static_cast<nova::dsp::ModMatrix::Destination>(slots[i].destination),
                       slots[i].amount,
                       slots[i].enabled);
        }
    } else {
        mm.setSlot(0, nova::dsp::ModMatrix::Source::Lfo1,        nova::dsp::ModMatrix::Destination::FilterCutoff, 0.35f, true);
        mm.setSlot(1, nova::dsp::ModMatrix::Source::Lfo2,        nova::dsp::ModMatrix::Destination::Osc1Shape,    0.25f, true);
        mm.setSlot(2, nova::dsp::ModMatrix::Source::MacroMotion, nova::dsp::ModMatrix::Destination::Osc2Shape,    0.40f, true);
        mm.setSlot(3, nova::dsp::ModMatrix::Source::MacroWarmth, nova::dsp::ModMatrix::Destination::FilterCutoff, -0.35f, true);
        mm.setSlot(4, nova::dsp::ModMatrix::Source::ModWheel,    nova::dsp::ModMatrix::Destination::FilterCutoff, 0.45f, true);
    }
}

void NovaSynthProcessor::setStateInformation(const void* data, int sizeInBytes) {
    nova::plugin::NovaProcessorBase::setStateInformation(data, sizeInBytes);
    auto modTree = apvts_.state.getChildWithName("ModMatrix");
    if (modTree.isValid()) {
        auto& mm = synthEngine_.getModMatrix();
        mm.clear();
        for (int i = 0; i < modTree.getNumChildren(); ++i) {
            auto slotTree = modTree.getChild(i);
            const int idx = slotTree.getProperty("index", -1);
            if (idx >= 0 && idx < static_cast<int>(nova::dsp::ModMatrix::kMaxSlots)) {
                const auto src = static_cast<nova::dsp::ModMatrix::Source>(static_cast<int>(slotTree.getProperty("src", 0)));
                const auto dst = static_cast<nova::dsp::ModMatrix::Destination>(static_cast<int>(slotTree.getProperty("dst", 0)));
                const float amt = static_cast<float>(slotTree.getProperty("amount", 0.0f));
                const bool en = slotTree.getProperty("enabled", true);
                mm.setSlot(static_cast<std::size_t>(idx), src, dst, amt, en);
            }
        }
    }
}

int NovaSynthProcessor::getArpPatternLength() const noexcept {
    if (arpPatternLengthParam_ != nullptr) {
        const int idx = static_cast<int>(arpPatternLengthParam_->load());
        return (idx == 1) ? 32 : ((idx == 2) ? 64 : 16);
    }
    if (auto* p = apvts_.getRawParameterValue("arpPatternLength")) {
        const int idx = static_cast<int>(p->load());
        return (idx == 1) ? 32 : ((idx == 2) ? 64 : 16);
    }
    return 16;
}

void NovaSynthProcessor::setArpPatternLength(int length) {
    const int idx = (length == 32) ? 1 : ((length == 64) ? 2 : 0);
    if (auto* p = apvts_.getParameter("arpPatternLength")) {
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(idx)));
    }
    arpPatternResetPending_.store(true, std::memory_order_release);
}

void NovaSynthProcessor::applyArpRhythmPattern(int length, const std::vector<bool>& gates, const std::vector<float>& velocities) {
    setArpPatternLength(length);
    for (int s = 0; s < 64; ++s) {
        const bool active = (s < static_cast<int>(gates.size())) ? gates[static_cast<std::size_t>(s)] : (s < length);
        const float vel   = (s < static_cast<int>(velocities.size())) ? velocities[static_cast<std::size_t>(s)] : 0.8f;
        setArpStepGate(s, active);
        setArpStepVelocity(s, vel);
    }
    arpPatternResetPending_.store(true, std::memory_order_release);
}

void NovaSynthProcessor::triggerPanic() noexcept {
    panicPending_.store(true, std::memory_order_release);
}

bool NovaSynthProcessor::isArpStepGateActive(int step) const noexcept {
    if (step >= 0 && step < 64) {
        if (arpStepGateParams_[static_cast<std::size_t>(step)] != nullptr) {
            return arpStepGateParams_[static_cast<std::size_t>(step)]->load() > 0.5f;
        }
        if (auto* p = apvts_.getRawParameterValue("arpStepGate_" + juce::String(step + 1))) {
            return p->load() > 0.5f;
        }
    }
    return true;
}

void NovaSynthProcessor::setArpStepGate(int step, bool active) {
    if (step >= 0 && step < 64) {
        if (auto* p = apvts_.getParameter("arpStepGate_" + juce::String(step + 1))) {
            p->setValueNotifyingHost(active ? 1.0f : 0.0f);
        }
    }
}

void NovaSynthProcessor::toggleArpStepGate(int step) {
    setArpStepGate(step, !isArpStepGateActive(step));
}

float NovaSynthProcessor::getArpStepVelocity(int step) const noexcept {
    if (step >= 0 && step < 64) {
        if (arpStepVelParams_[static_cast<std::size_t>(step)] != nullptr) {
            return arpStepVelParams_[static_cast<std::size_t>(step)]->load();
        }
        if (auto* p = apvts_.getRawParameterValue("arpStepVel_" + juce::String(step + 1))) {
            return p->load();
        }
    }
    return 0.8f;
}

void NovaSynthProcessor::setArpStepVelocity(int step, float velocity) {
    if (step >= 0 && step < 64) {
        if (auto* p = apvts_.getParameter("arpStepVel_" + juce::String(step + 1))) {
            p->setValueNotifyingHost(p->convertTo0to1(std::clamp(velocity, 0.05f, 1.0f)));
        }
    }
}

bool NovaSynthProcessor::isInvertSustainPedal() const noexcept {
    if (invertSustainPedalParam_ != nullptr) {
        return invertSustainPedalParam_->load(std::memory_order_relaxed) > 0.5f;
    }
    return false;
}

void NovaSynthProcessor::setInvertSustainPedal(bool inverted) {
    if (auto* p = apvts_.getParameter("invertSustainPedal")) {
        p->setValueNotifyingHost(inverted ? 1.0f : 0.0f);
    }
}

float NovaSynthProcessor::getMasterTuningHz() const noexcept {
    if (masterTuningHzParam_ != nullptr) {
        const float rawHz = std::clamp(masterTuningHzParam_->load(std::memory_order_relaxed), 435.0f, 445.0f);
        return std::round(rawHz * 10.0f) * 0.1f;
    }
    return 440.0f;
}

void NovaSynthProcessor::setMasterTuningHz(float hz) {
    const float clamped = std::round(std::clamp(hz, 435.0f, 445.0f) * 10.0f) * 0.1f;
    if (auto* p = apvts_.getParameter("masterTuningHz")) {
        p->setValueNotifyingHost(p->convertTo0to1(clamped));
    }
    syncParametersFromAPVTS();
}

void NovaSynthProcessor::applyHostTuningHzFromMidi(float tuningHz) noexcept {
    const float clamped = std::round(std::clamp(tuningHz, 435.0f, 445.0f) * 10.0f) * 0.1f;
    if (auto* p = apvts_.getParameter("masterTuningHz")) {
        p->setValueNotifyingHost(p->convertTo0to1(clamped));
    }
    currentParams_.masterTuningHz = clamped;
    synthEngine_.setParameters(currentParams_);
}


juce::AudioProcessorEditor* NovaSynthProcessor::createEditor() {
    return new NovaSynthEditor(*this);
}

} // namespace nova::synth

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new nova::synth::NovaSynthProcessor();
}
