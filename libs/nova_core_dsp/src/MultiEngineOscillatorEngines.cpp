#include "nova_dsp/oscillators/MultiEngineOscillator.h"
#include "nova_dsp/oscillators/SampleBank.h"
#include "OscillatorSharedTables.h"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace nova::dsp {

void MultiEngineOscillator::processOrganicTexture(float slotFreq, float shape, float invSr, float& sigL, float& sigR) noexcept {
    const auto& texBank = getTextureSourceBank();
    const int model = std::clamp(params_.modelIndex, 0, SharedTextureSourceBank::kTexTables - 1);
    const auto& activeTable = texBank.tables[static_cast<std::size_t>(model)];

    // Playback increment: 512 samples per waveform cycle in the 4096-sample buffer
    const float baseInc = std::min(400.0f, slotFreq * 512.0f * invSr);
    constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
    constexpr std::array<float, 5> kRainHarmonics = { 1.0f, 1.5f, 2.0f, 3.0f, 4.0f };

    float grainSumL = 0.0f, grainSumR = 0.0f;

    for (int g = 0; g < kNumGrains; ++g) {
        auto& gr = grains_[static_cast<std::size_t>(g)];

        // Hann window: 0.5 * (1 - cos(2*pi * age / length))
        const float normAge = gr.age / gr.lengthSamples;
        const float win = 0.5f * (1.0f - std::cos(twoPi * normAge));
        const float s = readTextureHermite(activeTable, gr.pos);
        const float sWin = s * win * gr.amp;

        grainSumL += sWin * gr.panL;
        grainSumR += gr.panR * sWin;

        // Advance playhead
        gr.pos += gr.rate * baseInc;
        if (gr.pos >= 4096.0f) gr.pos = std::fmod(gr.pos, 4096.0f);
        else if (gr.pos < 0.0f) gr.pos = std::fmod(gr.pos, 4096.0f) + 4096.0f;

        gr.age += 1.0f;
        if (gr.age >= gr.lengthSamples) {
            // Respawn grain
            rng_ = rng_ * 1664525u + 1013904223u;
            const float r1 = static_cast<float>(rng_ & 0xFFFFu) / 65536.0f;
            rng_ = rng_ * 1664525u + 1013904223u;
            const float r2 = static_cast<float>(rng_ & 0xFFFFu) / 65536.0f;
            rng_ = rng_ * 1664525u + 1013904223u;
            const float r3 = static_cast<float>(rng_ & 0xFFFFu) / 65536.0f;

            const float durSec = 0.035f + 0.35f * params_.duration;
            gr.lengthSamples = std::clamp(durSec * static_cast<float>(sampleRate_) * (0.80f + 0.40f * r1), 400.0f, 24000.0f);
            gr.age = 0.0f;

            // Scatter playhead position
            const float scatterOffset = params_.scatter * r2 * 4096.0f;
            gr.pos = std::fmod(gr.pos + scatterOffset + 512.0f, 4096.0f);

            // Stereo pan spread
            const float uPan = std::clamp((r3 * 2.0f - 1.0f) * params_.spread, -1.0f, 1.0f);
            const float panAngle = (uPan + 1.0f) * 0.25f * std::numbers::pi_v<float>;
            gr.panL = std::cos(panAngle);
            gr.panR = std::sin(panAngle);
            gr.amp = 1.0f;

            switch (model) {
                case 0: gr.rate = (g % 4 == 0) ? 2.0f : ((g % 4 == 1) ? 0.5f : (1.0f + ((r3 * 2.0f - 1.0f) * 0.004f))); break;
                case 1: gr.rate = (g % 4 == 0) ? 0.5f : (1.0f + ((r3 * 2.0f - 1.0f) * 0.005f)); break;
                case 2: gr.rate = (g % 4 == 0) ? 1.5f : ((g % 4 == 1) ? 2.0f : 1.0f); break;
                case 3: gr.rate = kRainHarmonics[static_cast<std::size_t>(g % 5)]; break;
                case 4: default: gr.rate = (g % 3 == 0) ? 2.0f : ((g % 3 == 1) ? 1.5f : 1.0f); break;
            }
        }
    }

    constexpr float kGrainNorm = 0.22f;
    grainSumL *= kGrainNorm;
    grainSumR *= kGrainNorm;
    float texL = grainSumL, texR = grainSumR;

    switch (model) {
        case 0: { // Ambient Choir Breath: gentle acoustic vocal tract formant body (/u/ -> /o/ -> /a/ -> /i/)
            const float effShape = std::clamp(shape, 0.0f, 1.0f);
            if (std::abs(effShape - lastFormantShape_) > 0.002f || lastFormantModel_ != 0) {
                lastFormantShape_ = effShape;
                lastFormantModel_ = 0;
                float f1Hz = 320.0f, f2Hz = 800.0f, f3Hz = 2400.0f;
                float q1 = 1.2f, q2 = 1.4f, q3 = 1.2f;
                if (effShape < 0.333f) {
                    const float t = effShape / 0.333f;
                    f1Hz = 320.0f + t * (500.0f - 320.0f);
                    f2Hz = 800.0f + t * (1000.0f - 800.0f);
                    f3Hz = 2400.0f + t * (2500.0f - 2400.0f);
                } else if (effShape < 0.667f) {
                    const float t = (effShape - 0.333f) / 0.334f;
                    f1Hz = 500.0f + t * (750.0f - 500.0f);
                    f2Hz = 1000.0f + t * (1250.0f - 1000.0f);
                    f3Hz = 2500.0f + t * (2700.0f - 2500.0f);
                    q1 = 1.2f + t * 0.2f; q2 = 1.4f + t * 0.2f;
                } else {
                    const float t = (effShape - 0.667f) / 0.333f;
                    f1Hz = 750.0f + t * (320.0f - 750.0f);
                    f2Hz = 1250.0f + t * (2200.0f - 1250.0f);
                    f3Hz = 2700.0f + t * (3000.0f - 2700.0f);
                }
                formantFiltersL_[0].setParameters(StateVariableFilter::Type::BandPass, f1Hz, q1);
                formantFiltersL_[1].setParameters(StateVariableFilter::Type::BandPass, f2Hz, q2);
                formantFiltersL_[2].setParameters(StateVariableFilter::Type::BandPass, f3Hz, q3);
                formantFiltersR_[0].setParameters(StateVariableFilter::Type::BandPass, f1Hz, q1);
                formantFiltersR_[1].setParameters(StateVariableFilter::Type::BandPass, f2Hz, q2);
                formantFiltersR_[2].setParameters(StateVariableFilter::Type::BandPass, f3Hz, q3);
            }
            const float f1L = formantFiltersL_[0].processSample(grainSumL), f2L = formantFiltersL_[1].processSample(grainSumL), f3L = formantFiltersL_[2].processSample(grainSumL);
            const float f1R = formantFiltersR_[0].processSample(grainSumR), f2R = formantFiltersR_[1].processSample(grainSumR), f3R = formantFiltersR_[2].processSample(grainSumR);
            const float formL = f1L * 0.45f + f2L * 0.40f + f3L * 0.30f, formR = f1R * 0.45f + f2R * 0.40f + f3R * 0.30f;
            texL = (grainSumL * 0.72f + formL * 0.35f) * 1.06f;
            texR = (grainSumR * 0.72f + formR * 0.35f) * 1.06f;
            break;
        }
        case 1: { // Bowed Cello: gentle warm body resonances (110, 220, 440 Hz)
            if (lastFormantModel_ != 1) {
                lastFormantModel_ = 1;
                lastFormantShape_ = -999.0f;
                const float freqs[3] = { 110.0f, 220.0f, 440.0f }, qs[3] = { 1.2f, 1.4f, 1.3f };
                for (int i = 0; i < 3; ++i) {
                    formantFiltersL_[static_cast<std::size_t>(i)].setParameters(StateVariableFilter::Type::BandPass, freqs[i], qs[i]);
                    formantFiltersR_[static_cast<std::size_t>(i)].setParameters(StateVariableFilter::Type::BandPass, freqs[i], qs[i]);
                }
            }
            const float f1L = formantFiltersL_[0].processSample(grainSumL), f2L = formantFiltersL_[1].processSample(grainSumL), f3L = formantFiltersL_[2].processSample(grainSumL);
            const float f1R = formantFiltersR_[0].processSample(grainSumR), f2R = formantFiltersR_[1].processSample(grainSumR), f3R = formantFiltersR_[2].processSample(grainSumR);
            const float woodL = f1L * 0.45f + f2L * 0.35f + f3L * 0.25f, woodR = f1R * 0.45f + f2R * 0.35f + f3R * 0.25f;
            texL = grainSumL * 0.70f + woodL * 0.30f;
            texR = grainSumR * 0.70f + woodR * 0.30f;
            break;
        }
        case 2: { // Tape Halo: warm Mellotron saturation
            flutterPhase_ += 3.2f * invSr;
            if (flutterPhase_ >= 1.0f) flutterPhase_ -= 1.0f;
            const float drive = 1.15f + 0.85f * shape;
            texL = std::tanh(grainSumL * drive) * 0.88f;
            texR = std::tanh(grainSumR * drive) * 0.88f;
            break;
        }
        case 3: // Cathedral Rain Cloud: crystalline high sheen
            texL = grainSumL * 0.95f;
            texR = grainSumR * 0.95f;
            break;
        case 4: default: // Warm Wind & Air Swell
            texL = grainSumL * 0.90f;
            texR = grainSumR * 0.90f;
            break;
    }

    sigL = texL;
    sigR = texR;
}

void MultiEngineOscillator::processPhysicalPluck(float slotFreq, float shape, float invSr, float& sigL, float& sigR) noexcept {
    const float sr = static_cast<float>(sampleRate_);
    if (params_.modelIndex >= 2) {
        // Modal Inharmonic Resonator (Kalimba / Glass Mallet)
        const float twoPi = 2.0f * std::numbers::pi_v<float>;
        const std::array<float, 4> modeRatios = (params_.modelIndex == 2)
            ? std::array<float, 4>{ 1.0f, 2.756f, 5.404f, 8.933f }
            : std::array<float, 4>{ 1.0f, 2.0f, 3.162f, 4.520f };

        float modalSumL = 0.0f, modalSumR = 0.0f;
        for (int m = 0; m < 4; ++m) {
            const auto idx = static_cast<std::size_t>(m);
            const float mFreq = std::min(sr * 0.46f, slotFreq * modeRatios[idx]);
            modalPhases_[idx] += mFreq * invSr;
            if (modalPhases_[idx] >= 1.0f) modalPhases_[idx] -= 1.0f;

            modalAmplitudes_[idx] *= modalDecays_[idx];
            const float s = std::sin(twoPi * modalPhases_[idx]) * modalAmplitudes_[idx];
            if ((m & 1) == 0) {
                modalSumL += s;
                modalSumR += s * 0.70f;
            } else {
                modalSumL += s * 0.70f;
                modalSumR += s;
            }
        }

        const float modalScale = (params_.modelIndex == 2) ? 0.49f : 0.42f;
        sigL = modalSumL * modalScale;
        sigR = modalSumR * modalScale;
    } else {
        // Waveguide Strings (Steel String & Nylon Harp)
        const float delayLenL = std::clamp(sr / slotFreq, 8.0f, static_cast<float>(kMaxStringDelay - 8));
        const float delayLenR = std::clamp(delayLenL * pluckDelayRatioR_, 8.0f, static_cast<float>(kMaxStringDelay - 8));

        const auto readWaveguide = [this](const std::vector<float>& buf, float delaySamples) noexcept {
            const int dInt = static_cast<int>(delaySamples);
            const float frac = delaySamples - static_cast<float>(dInt);
            const int idx0 = (stringWritePos_ - dInt + kMaxStringDelay) & (kMaxStringDelay - 1);
            const int idx1 = (idx0 - 1 + kMaxStringDelay) & (kMaxStringDelay - 1);
            return buf[static_cast<std::size_t>(idx0)] + frac * (buf[static_cast<std::size_t>(idx1)] - buf[static_cast<std::size_t>(idx0)]);
        };

        const float rawL = readWaveguide(stringDelayL_, delayLenL);
        const float rawR = readWaveguide(stringDelayR_, delayLenR);
        const float brightness = (params_.modelIndex == 0) ? (0.35f + 0.60f * shape) : (0.15f + 0.40f * shape);
        stringLpStateL_ = rawL * brightness + stringLpStateL_ * (1.0f - brightness);
        stringLpStateR_ = rawR * brightness + stringLpStateR_ * (1.0f - brightness);

        const float sustainFeedback = (params_.modelIndex == 0) ? (0.9950f + 0.0045f * shape) : (0.9910f + 0.0060f * shape);
        stringDelayL_[static_cast<std::size_t>(stringWritePos_)] = stringLpStateL_ * sustainFeedback;
        stringDelayR_[static_cast<std::size_t>(stringWritePos_)] = stringLpStateR_ * sustainFeedback;
        stringWritePos_ = (stringWritePos_ + 1) & (kMaxStringDelay - 1);

        phases_[0] += slotFreq * invSr;
        if (phases_[0] >= 1.0f) phases_[0] -= 1.0f;
        const float modalBody = (params_.modelIndex == 0)
            ? (0.15f * (1.0f - 2.0f * phases_[0]))
            : (0.28f * std::sin(2.0f * std::numbers::pi_v<float> * phases_[0]));

        const float pluckScale = (params_.modelIndex == 0) ? 4.7f : 2.5f;
        sigL = (stringLpStateL_ + modalBody) * pluckScale;
        sigR = (stringLpStateR_ + modalBody) * pluckScale;
    }
}

void MultiEngineOscillator::processSubOscillator(float slotFreq, float shape, float invSr, float& sigL, float& sigR) noexcept {
    // Dedicated clean sub bass: 1 octave below fundamental (slotFreq * 0.5f)
    const float subFreq = slotFreq * 0.5f;
    const float dt = subFreq * invSr;
    phases_[0] += dt;
    if (phases_[0] >= 1.0f) phases_[0] -= 1.0f;
    const float ph = phases_[0];
    const float twoPi = 2.0f * std::numbers::pi_v<float>;

    const float sine = std::sin(twoPi * ph);
    const float tri = 2.0f * std::abs(2.0f * (ph - std::floor(ph + 0.5f))) - 1.0f;
    const float sqr = (ph < 0.5f ? 0.85f : -0.85f) + polyBlep(ph, dt) - polyBlep(std::fmod(ph + 0.5f, 1.0f), dt);

    float subVal = sine;
    switch (params_.modelIndex) {
        case 0: subVal = sine; break;
        case 1: subVal = (1.0f - shape * 0.45f) * tri + (shape * 0.45f) * sine; break;
        case 2: {
            const float driveX = sine * (1.6f + shape * 2.4f);
            const float x2 = driveX * driveX;
            subVal = (driveX * (27.0f + x2) / (27.0f + 9.0f * x2)) * 0.88f;
            break;
        }
        case 3: subVal = (1.0f - shape * 0.35f) * sqr + (shape * 0.35f) * sine; break;
        default: subVal = sine; break;
    }

    sigL = subVal * 0.95f;
    sigR = subVal * 0.95f;
}

void MultiEngineOscillator::processNoiseTransient(float shape, float invSr, float& sigL, float& sigR) noexcept {
    // Noise / Transient: Pink noise with tilt filter + air sizzle
    rng_ = rng_ * 1664525u + 1013904223u;
    const float white = ((static_cast<float>(rng_ & 0xFFFFu) / 32767.5f) - 1.0f);

    // 1-pole low-pass / pinking filter
    const float pinkCoeff = 0.15f + 0.70f * shape;
    texFilterState1_ = texFilterState1_ + pinkCoeff * (white - texFilterState1_);

    // High-pass component for air / transient sparkle
    const float air = white - texFilterState1_;
    float noiseSig = (texFilterState1_ * (1.0f - shape * 0.5f) + air * shape * 0.8f) * 0.55f;

    switch (params_.modelIndex) {
        case 0: noiseSig = texFilterState1_ * 0.65f; break;
        case 1: noiseSig = (std::abs(white) > (0.90f - shape * 0.12f)) ? (white * 1.6f) : (white * 0.08f); break;
        case 2:
            texFilterState2_ += 0.42f * (white - texFilterState2_);
            noiseSig = (white - texFilterState2_) * 0.52f;
            break;
        case 3: {
            constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
            const float clickDurSec = std::clamp(params_.duration > 0.001f ? params_.duration : (0.035f + 0.065f * shape), 0.015f, 0.150f);
            const float clickProgress = phases_[0];
            if (phases_[0] < 1.0f) phases_[0] += invSr / clickDurSec;

            const float clickEnv = (clickProgress < 1.0f) ? std::exp(-clickProgress * (4.5f + shape * 3.5f)) : 0.0f;
            texFilterState2_ += 0.55f * (white - texFilterState2_);
            texFilterState1_ += 0.16f * (white - texFilterState1_);
            const float clickSine = std::sin(twoPi * std::min(1.0f, clickProgress) * 18.0f) * (1.0f - std::min(1.0f, clickProgress));
            noiseSig = ((white - texFilterState2_) * 1.6f + texFilterState1_ * 2.2f + clickSine * 0.55f) * clickEnv * 2.2f;
            noiseSig += texFilterState1_ * 0.045f * (0.25f + 0.75f * shape);
            break;
        }
        default: break;
    }

    sigL = noiseSig;
    sigR = noiseSig;
}

void MultiEngineOscillator::processSamplePlayback(float slotFreq, float& sigL, float& sigR) noexcept {
    if (sampleFinished_ || activeSampleZone_ == nullptr) return;
    const auto& smp = SampleBank::getSample(params_.modelIndex);
    const auto* zone = activeSampleZone_;
    if (zone->pcmL == nullptr || zone->numSamples <= 8) return;

    const float detuneCents = std::clamp(params_.scatter, 0.0f, 1.0f) * 50.0f;
    const float detuneRatio = std::pow(2.0f, detuneCents / 1200.0f);
    const double playbackRate = (static_cast<double>(slotFreq) * static_cast<double>(detuneRatio) / static_cast<double>(zone->rootFrequencyHz)) * (smp.sampleRate / sampleRate_);
    const double step = playbackRate * sampleDirection_;

    const int numS = zone->numSamples;
    const double lStart = std::clamp(static_cast<double>(params_.sampleLoopStart), 0.0, 0.98) * static_cast<double>(numS - 4);
    double lEnd = std::clamp(static_cast<double>(params_.sampleLoopEnd), 0.02, 1.0) * static_cast<double>(numS - 4);
    if (lEnd < lStart + 100.0) lEnd = std::min(static_cast<double>(numS - 4), lStart + 100.0);
    const double loopLen = lEnd - lStart;

    const double xfadeUser = static_cast<double>(std::clamp(params_.sampleCrossfade, 0.0f, 0.25f));
    const double xfadeLen = std::max(16.0, std::min(loopLen * 0.25, xfadeUser * loopLen));

    const int16_t* pcmL = zone->pcmL;
    const int16_t* pcmR = (zone->pcmR != nullptr) ? zone->pcmR : zone->pcmL;
    const float spreadAmt = std::clamp(params_.detune, 0.0f, 1.0f);
    const double rightDelay = static_cast<double>(spreadAmt * 0.015f * sampleRate_);

    float rawL = 0.0f, rawR = 0.0f;

    switch (params_.sampleLoopMode) {
        case 0: { // One-Shot (No Looping)
            if (samplePlayhead_ >= numS - 4 || samplePlayhead_ < 0.0) {
                sampleFinished_ = true;
                return;
            }
            rawL = readSampleHermite(pcmL, numS, samplePlayhead_);
            rawR = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
            samplePlayhead_ += step;
            break;
        }
        case 1: { // Forward Loop with Equal-Power Crossfade
            if (samplePlayhead_ < lStart || samplePlayhead_ < lEnd - xfadeLen) {
                rawL = readSampleHermite(pcmL, numS, samplePlayhead_);
                rawR = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
            } else if (samplePlayhead_ < lEnd) {
                const double t = (samplePlayhead_ - (lEnd - xfadeLen)) / xfadeLen;
                const float angle = static_cast<float>(t * (std::numbers::pi * 0.5));
                const float g1 = std::cos(angle), g2 = std::sin(angle);
                const double secPlayhead = lStart + (samplePlayhead_ - (lEnd - xfadeLen));
                const float p1L = readSampleHermite(pcmL, numS, samplePlayhead_), p1R = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
                const float p2L = readSampleHermite(pcmL, numS, secPlayhead), p2R = readSampleHermite(pcmR, numS, std::max(0.0, secPlayhead - rightDelay));
                rawL = p1L * g1 + p2L * g2;
                rawR = p1R * g1 + p2R * g2;
            } else {
                samplePlayhead_ = (lStart + xfadeLen) + std::fmod(samplePlayhead_ - lEnd, loopLen);
                rawL = readSampleHermite(pcmL, numS, samplePlayhead_);
                rawR = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
            }
            samplePlayhead_ += step;
            break;
        }
        case 2: { // Ping-Pong Loop (<->)
            if (sampleDirection_ > 0.0 && samplePlayhead_ >= lEnd) {
                sampleDirection_ = -1.0;
                samplePlayhead_ = lEnd - (samplePlayhead_ - lEnd);
            } else if (sampleDirection_ <= 0.0 && samplePlayhead_ <= lStart) {
                sampleDirection_ = 1.0;
                samplePlayhead_ = lStart + (lStart - samplePlayhead_);
            }
            rawL = readSampleHermite(pcmL, numS, samplePlayhead_);
            rawR = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
            samplePlayhead_ += playbackRate * sampleDirection_;
            break;
        }
        case 3: { // Reverse Loop (<-) with Equal-Power Crossfade
            if (samplePlayhead_ > lEnd || samplePlayhead_ > lStart + xfadeLen) {
                rawL = readSampleHermite(pcmL, numS, samplePlayhead_);
                rawR = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
            } else if (samplePlayhead_ > lStart) {
                const double t = ((lStart + xfadeLen) - samplePlayhead_) / xfadeLen;
                const float angle = static_cast<float>(t * (std::numbers::pi * 0.5));
                const float g1 = std::cos(angle), g2 = std::sin(angle);
                const double secPlayhead = lEnd - ((lStart + xfadeLen) - samplePlayhead_);
                const float p1L = readSampleHermite(pcmL, numS, samplePlayhead_), p1R = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
                const float p2L = readSampleHermite(pcmL, numS, secPlayhead), p2R = readSampleHermite(pcmR, numS, std::max(0.0, secPlayhead - rightDelay));
                rawL = p1L * g1 + p2L * g2;
                rawR = p1R * g1 + p2R * g2;
            } else {
                samplePlayhead_ = (lEnd - xfadeLen) - std::fmod(lStart - samplePlayhead_, loopLen);
                rawL = readSampleHermite(pcmL, numS, samplePlayhead_);
                rawR = readSampleHermite(pcmR, numS, std::max(0.0, samplePlayhead_ - rightDelay));
            }
            samplePlayhead_ += step;
            break;
        }
    }

    const float mid = 0.5f * (rawL + rawR);
    const float side = 0.5f * (rawL - rawR);
    const float smpModelScale = (params_.modelIndex == 7) ? 4.2f : 1.0f;
    sigL = (mid + spreadAmt * side) * smpModelScale;
    sigR = (mid - spreadAmt * side) * smpModelScale;
}

} // namespace nova::dsp
