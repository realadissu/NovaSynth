#include "nova_dsp/reverb/DualEngineReverb.h"
#include "DualEngineReverbHelpers.h"
#include <algorithm>
#include <cmath>

namespace nova::dsp {

void DualEngineReverb::processShimmerSample(float lateOutL, float lateOutR,
                                            float diffL, float diffR,
                                            float shimOct, float shimFifth,
                                            float activeShim, bool isFrozen,
                                            float& shimmerHaloL, float& shimmerHaloR) noexcept {
    // Opposing +/-7 cent stereo detune breaks rigid 2.0000x organ phase locking into a soft string/cloud halo
    constexpr float kDetuneDown     = 0.9960f; // ~ -6.94 cents
    constexpr float kDetuneUp       = 1.0040f; // ~ +6.91 cents
    constexpr float kHalfDetuneDown = 0.9980f; // ~ -3.47 cents per stage (net ~ -6.93 cents for 2-stage +2 Oct)
    constexpr float kHalfDetuneUp   = 1.0020f; // ~ +3.46 cents per stage (net ~ +6.92 cents for 2-stage +2 Oct)

    // Driven predominantly by the late FDN tank readout with minimal direct diffuser seed so
    // shimmer blooms organically out of the reverb tail instead of sitting on top of chord attacks.
    const float shimInL = lateOutL * 1.38f + (isFrozen ? 0.0f : diffL * 0.10f);
    const float shimInR = lateOutR * 1.38f + (isFrozen ? 0.0f : diffR * 0.10f);

    // HighPass before pitch shifting keeps low-frequency mud out of the grains
    const float hpL = shimmerHighPassL_.processSample(shimInL);
    const float hpR = shimmerHighPassR_.processSample(shimInR);

    float sL = 0.0f;
    float sR = 0.0f;
    if (shimOct > 0.0005f) {
        switch (params_.shimmerPitchMode) {
            case 1: { // +2 Octaves (+24st) via smooth 2x -> 2x cascade with net +/-7 cent stereo detune
                const float oct1L = shimmerInterStageL_.processSample(octaveShifterL_.process(hpL, 2.0f * kHalfDetuneDown));
                const float oct1R = shimmerInterStageR_.processSample(octaveShifterR_.process(hpR, 2.0f * kHalfDetuneUp));
                sL = octave2ShifterL_.process(oct1L, 2.0f * kHalfDetuneDown);
                sR = octave2ShifterR_.process(oct1R, 2.0f * kHalfDetuneUp);
                break;
            }
            case 2: { // +1 & +2 Octaves Combined - Lush Stereo Cloud Halo (+/-7 cents net on both octaves)
                const float oct1L = octaveShifterL_.process(hpL, 2.0f * kDetuneDown);
                const float oct1R = octaveShifterR_.process(hpR, 2.0f * kDetuneUp);
                const float filt1L = shimmerInterStageL_.processSample(oct1L);
                const float filt1R = shimmerInterStageR_.processSample(oct1R);
                const float oct2L = octave2ShifterL_.process(filt1L, 2.0f);
                const float oct2R = octave2ShifterR_.process(filt1R, 2.0f);
                sL = oct1L * 0.72f + oct2L * 0.48f;
                sR = oct1R * 0.72f + oct2R * 0.48f;
                break;
            }
            case 3: // -1 Octave Sub - Deep Atmospheric Foundation with gentle stereo spread
                sL = octaveShifterL_.process(hpL, 0.5f * kDetuneDown) * 1.20f;
                sR = octaveShifterR_.process(hpR, 0.5f * kDetuneUp) * 1.20f;
                break;
            case 0: // +1 Octave (+12st) - Detuned Stereo Cloud Bloom
            default:
                sL = octaveShifterL_.process(hpL, 2.0f * kDetuneDown);
                sR = octaveShifterR_.process(hpR, 2.0f * kDetuneUp);
                break;
        }
    }

    // Harmonic Fifth (+7st): gentle opposing-detuned fifth glow
    if (shimFifth > 0.0005f && !isFrozen) {
        const float fL = fifthShifterL_.process(hpL * 0.75f, 1.498307f * kDetuneUp);
        const float fR = fifthShifterR_.process(hpR * 0.75f, 1.498307f * kDetuneDown);
        sL = sL * (shimOct > 0.0005f ? 1.0f : 0.0f) + fL * 0.65f;
        sR = sR * (shimOct > 0.0005f ? 1.0f : 0.0f) + fR * 0.65f;
    }

    // Silk LowPass melts away ultrasonic grain edges so ascending octaves stay velvety
    sL = shimmerSilkL_.processSample(sL);
    sR = shimmerSilkR_.processSample(sR);

    // 2-stage prime allpass diffusion + stereo cross-scattering for enveloping cloud width
    sL = shimmerDiffusersL_[0].process(sL);
    sR = shimmerDiffusersR_[0].process(sR);
    const float crossL = 0.84f * sL + 0.16f * sR;
    const float crossR = 0.84f * sR + 0.16f * sL;
    const float cloudL = shimmerDiffusersL_[1].process(crossL);
    const float cloudR = shimmerDiffusersR_[1].process(crossR);

    shimmerCloudDelayL_.write(flushDenormal(cloudL));
    shimmerCloudDelayR_.write(flushDenormal(cloudR));

    // Delayed stereo halo tap from the cloud (~38ms diffusion delay) for wide cathedral bloom
    shimmerHaloL = shimmerCloudDelayL_.readInt(shimmerCloudTapSamples_[9]) * activeShim;
    shimmerHaloR = shimmerCloudDelayR_.readInt(shimmerCloudTapSamples_[10]) * activeShim;
}

} // namespace nova::dsp
