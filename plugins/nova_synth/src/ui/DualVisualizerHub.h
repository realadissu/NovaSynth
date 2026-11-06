#pragma once

#include "nova_ui/NovaTheme.h"
#include "../NovaSynthProcessor.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>
#include <numbers>
#include <algorithm>

namespace nova::synth {

class DualVisualizerHub : public juce::Component, private juce::Timer {
public:
    DualVisualizerHub(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
        : proc_(proc), theme_(theme), fft_(9) {
        circularBuf_.fill(0.0f);
        displayWave_.fill(0.0f);
        smoothedBins_.fill(0.0f);
        peakHoldBins_.fill(0.0f);
        startTimerHz(30);
    }

    ~DualVisualizerHub() override {
        stopTimer();
    }

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds();
        const int gap = 6;
        const int oscW = (area.getWidth() - gap) / 2;

        auto oscArea = area.removeFromLeft(oscW);
        area.removeFromLeft(gap);
        auto specArea = area;

        drawOscilloscope(g, oscArea);
        drawSpectrum(g, specArea);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (rtaBtnRect_.contains(e.getPosition())) {
            rtaMode_ = !rtaMode_;
            repaint();
        }
    }

private:
    void timerCallback() override {
        if (!isVisible()) return;

        // 1. Drain all available audio samples from synth engine into circular buffer
        std::array<float, 512> tempBuf{};
        std::size_t totalPopped = 0;
        for (int chunk = 0; chunk < 4; ++chunk) {
            const std::size_t popped = proc_.popVisualizerSamples(tempBuf.data(), tempBuf.size());
            if (popped == 0) break;
            totalPopped += popped;
            for (std::size_t i = 0; i < popped; ++i) {
                circularBuf_[writePos_] = tempBuf[i];
                writePos_ = (writePos_ + 1) & (circularBuf_.size() - 1);
            }
        }

        // Idle power-saver: when audio engine is silent and bins have decayed, skip FFT and repaint
        bool hasNonZeroBins = false;
        for (float b : smoothedBins_) {
            if (b > 0.005f) { hasNonZeroBins = true; break; }
        }
        if (totalPopped == 0 && !hasNonZeroBins && displayWave_[0] == 0.0f) {
            return;
        }

        // 2. Oscilloscope: Trigger Search (Rising Zero-Crossing) for rock-solid stability
        // Look backwards from (writePos_ - 512) over a 256-sample search window
        std::size_t triggerIdx = (writePos_ + circularBuf_.size() - 512) & (circularBuf_.size() - 1);
        const std::size_t searchWindow = 256;
        for (std::size_t s = 0; s < searchWindow; ++s) {
            const std::size_t idxCurr = (writePos_ + circularBuf_.size() - 512 - s) & (circularBuf_.size() - 1);
            const std::size_t idxPrev = (idxCurr + circularBuf_.size() - 1) & (circularBuf_.size() - 1);
            const float valCurr = circularBuf_[idxCurr];
            const float valPrev = circularBuf_[idxPrev];

            if (valPrev <= 0.0f && valCurr > 0.0f && (valCurr - valPrev) > 0.005f) {
                triggerIdx = idxCurr;
                break;
            }
        }

        // Unroll 512 continuous samples from trigger point into displayWave_
        float maxAbs = 0.001f;
        for (std::size_t i = 0; i < 512; ++i) {
            const std::size_t idx = (triggerIdx + i) & (circularBuf_.size() - 1);
            const float smp = circularBuf_[idx];
            displayWave_[i] = smp;
            maxAbs = std::max(maxAbs, std::abs(smp));
        }

        // Smooth Auto-Gain Control (AGC) for bold, clear oscilloscope height
        const float targetGain = std::clamp(0.72f / maxAbs, 1.0f, 6.0f);
        displayGain_ += (targetGain - displayGain_) * 0.12f;

        // 3. Spectrum Analyzer: 512-point Hann-windowed FFT
        std::array<float, 1024> fftData{};
        const std::size_t fftStart = (writePos_ + circularBuf_.size() - 512) & (circularBuf_.size() - 1);
        for (std::size_t i = 0; i < 512; ++i) {
            const std::size_t idx = (fftStart + i) & (circularBuf_.size() - 1);
            // Hann window
            const float w = 0.5f * (1.0f - std::cos(2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / 511.0f));
            fftData[i] = circularBuf_[idx] * w;
        }

        fft_.performFrequencyOnlyForwardTransform(fftData.data());

        // Bin pooling into 64 logarithmic bands (20 Hz - 20 kHz)
        for (int b = 0; b < 64; ++b) {
            const float fNormLow = std::pow(static_cast<float>(b) / 64.0f, 2.3f);
            const float fNormHigh = std::pow(static_cast<float>(b + 1) / 64.0f, 2.3f);
            const int binStart = std::clamp(static_cast<int>(fNormLow * 255.0f), 0, 255);
            const int binEnd = std::clamp(static_cast<int>(fNormHigh * 255.0f), binStart, 255);

            float peakMag = 0.0f;
            for (int k = binStart; k <= binEnd; ++k) {
                peakMag = std::max(peakMag, fftData[static_cast<std::size_t>(k)]);
            }

            const float db = 20.0f * std::log10(std::max(peakMag, 1e-4f));
            const float normVal = std::clamp((db + 64.0f) / 64.0f, 0.0f, 1.0f);

            if (rtaMode_) {
                // Smooth RTA averaging (long integration)
                smoothedBins_[static_cast<std::size_t>(b)] =
                    smoothedBins_[static_cast<std::size_t>(b)] * 0.82f + normVal * 0.18f;
            } else {
                // Dual-ballistics smoothing (Attack ~25ms, Decay ~320ms) eliminates jitter
                if (normVal > smoothedBins_[static_cast<std::size_t>(b)]) {
                    smoothedBins_[static_cast<std::size_t>(b)] =
                        smoothedBins_[static_cast<std::size_t>(b)] * 0.50f + normVal * 0.50f;
                } else {
                    smoothedBins_[static_cast<std::size_t>(b)] =
                        smoothedBins_[static_cast<std::size_t>(b)] * 0.88f;
                }
            }

            // Peak hold caps
            if (smoothedBins_[static_cast<std::size_t>(b)] > peakHoldBins_[static_cast<std::size_t>(b)]) {
                peakHoldBins_[static_cast<std::size_t>(b)] = smoothedBins_[static_cast<std::size_t>(b)];
            } else {
                peakHoldBins_[static_cast<std::size_t>(b)] *= 0.965f;
            }
        }

        repaint();
    }

    void drawOscilloscope(juce::Graphics& g, juce::Rectangle<int> bounds) {
        const auto bF = bounds.toFloat().reduced(1.0f);
        g.setColour(theme_.bgSurface);
        g.fillRoundedRectangle(bF, theme_.cornerRadius);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(bF, theme_.cornerRadius, 1.0f);

        // Grid lines
        const auto inner = bounds.reduced(10, 8).toFloat();
        const float midY = inner.getCentreY();

        g.setColour(theme_.borderSubtle.withAlpha(0.35f));
        g.drawHorizontalLine(static_cast<int>(midY), inner.getX(), inner.getRight());
        g.drawHorizontalLine(static_cast<int>(midY - inner.getHeight() * 0.35f), inner.getX(), inner.getRight());
        g.drawHorizontalLine(static_cast<int>(midY + inner.getHeight() * 0.35f), inner.getX(), inner.getRight());

        // Header label
        g.setFont(theme_.getValueFont(9.5f));
        g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.7f));
        g.drawText("LIVE OSCILLOSCOPE", bounds.reduced(10, 6).removeFromTop(14), juce::Justification::topLeft, false);

        // Build continuous Waveform Path
        juce::Path p;
        juce::Path fillP;
        const float xStep = inner.getWidth() / 511.0f;
        const float halfH = inner.getHeight() * 0.44f;

        p.preallocateSpace(1024);
        fillP.preallocateSpace(1026);
        fillP.startNewSubPath(inner.getX(), midY);

        for (std::size_t i = 0; i < 512; ++i) {
            const float x = inner.getX() + static_cast<float>(i) * xStep;
            const float scaledSmp = std::clamp(displayWave_[i] * displayGain_, -1.0f, 1.0f);
            const float y = midY - scaledSmp * halfH;

            if (i == 0) {
                p.startNewSubPath(x, y);
            } else {
                p.lineTo(x, y);
            }
            fillP.lineTo(x, y);
        }

        fillP.lineTo(inner.getRight(), midY);
        fillP.closeSubPath();

        // Subtle gradient under waveform
        g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.08f));
        g.fillPath(fillP);

        // Glow stroke + crisp bright core stroke
        g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.32f));
        g.strokePath(p, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xffe0f2fe));
        g.strokePath(p, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawSpectrum(juce::Graphics& g, juce::Rectangle<int> bounds) {
        const auto bF = bounds.toFloat().reduced(1.0f);
        g.setColour(theme_.bgSurface);
        g.fillRoundedRectangle(bF, theme_.cornerRadius);
        g.setColour(theme_.borderSubtle);
        g.drawRoundedRectangle(bF, theme_.cornerRadius, 1.0f);

        // Header label & FFT/RTA Toggle Button
        auto topBar = bounds.reduced(10, 6).removeFromTop(18);
        g.setFont(theme_.getValueFont(9.5f));
        g.setColour(juce::Colour(0xff2dd4bf).withAlpha(0.7f));
        g.drawText("SPECTRUM ANALYZER", topBar.removeFromLeft(140), juce::Justification::centredLeft, false);

        // [ FFT | RTA ] pill
        rtaBtnRect_ = topBar.removeFromRight(64);
        g.setColour(theme_.bgElevated);
        g.fillRoundedRectangle(rtaBtnRect_.toFloat(), 3.0f);
        g.setColour(theme_.borderFocus);
        g.drawRoundedRectangle(rtaBtnRect_.toFloat(), 3.0f, 0.8f);

        auto lRect = rtaBtnRect_.removeFromLeft(32);
        auto rRect = rtaBtnRect_;
        if (!rtaMode_) {
            g.setColour(theme_.accentPrimary.withAlpha(0.35f));
            g.fillRoundedRectangle(lRect.toFloat(), 3.0f);
        } else {
            g.setColour(theme_.accentPrimary.withAlpha(0.35f));
            g.fillRoundedRectangle(rRect.toFloat(), 3.0f);
        }
        g.setFont(theme_.getValueFont(9.0f));
        g.setColour(!rtaMode_ ? juce::Colours::white : theme_.textMuted);
        g.drawText("FFT", lRect, juce::Justification::centred, false);
        g.setColour(rtaMode_ ? juce::Colours::white : theme_.textMuted);
        g.drawText("RTA", rRect, juce::Justification::centred, false);
        rtaBtnRect_ = juce::Rectangle<int>(lRect.getX(), lRect.getY(), 64, lRect.getHeight());

        // Grid & frequency scale
        const auto inner = bounds.reduced(10, 18).toFloat();
        g.setColour(theme_.borderSubtle.withAlpha(0.35f));
        g.drawHorizontalLine(static_cast<int>(inner.getBottom()), inner.getX(), inner.getRight());
        g.drawHorizontalLine(static_cast<int>(inner.getY() + inner.getHeight() * 0.33f), inner.getX(), inner.getRight());
        g.drawHorizontalLine(static_cast<int>(inner.getY() + inner.getHeight() * 0.66f), inner.getX(), inner.getRight());

        // Draw 64 Spectrum Bars
        const float numBars = 64.0f;
        const float barGap = 1.0f;
        const float barW = (inner.getWidth() - (numBars - 1.0f) * barGap) / numBars;

        for (int i = 0; i < 64; ++i) {
            const float val = smoothedBins_[static_cast<std::size_t>(i)];
            if (val <= 0.005f) continue;

            const float barH = std::max(2.0f, val * inner.getHeight());
            const float x = inner.getX() + static_cast<float>(i) * (barW + barGap);
            const float y = inner.getBottom() - barH;

            // Multicolored gradient: Cyan (low) -> Teal (mid) -> Purple (high-mid) -> Amber (high)
            const float progress = static_cast<float>(i) / 63.0f;
            juce::Colour barCol;
            if (progress < 0.35f) {
                barCol = juce::Colour(0xff38bdf8).interpolatedWith(juce::Colour(0xff2dd4bf), progress / 0.35f);
            } else if (progress < 0.70f) {
                barCol = juce::Colour(0xff2dd4bf).interpolatedWith(juce::Colour(0xffc084fc), (progress - 0.35f) / 0.35f);
            } else {
                barCol = juce::Colour(0xffc084fc).interpolatedWith(juce::Colour(0xfff59e0b), (progress - 0.70f) / 0.30f);
            }

            g.setColour(barCol.withAlpha(0.85f));
            g.fillRect(x, y, barW, barH);

            // Subtle peak cap line
            const float peakVal = peakHoldBins_[static_cast<std::size_t>(i)];
            if (peakVal > val && peakVal > 0.02f) {
                const float peakY = inner.getBottom() - peakVal * inner.getHeight();
                g.setColour(juce::Colours::white.withAlpha(0.75f));
                g.fillRect(x, peakY, barW, 1.5f);
            }
        }

        // Freq Axis Labels (20, 100, 1k, 10k, 20k)
        g.setFont(theme_.getValueFont(8.0f));
        g.setColour(theme_.textMuted.withAlpha(0.6f));
        const int labelY = static_cast<int>(inner.getBottom() + 1.0f);
        g.drawText("20",   static_cast<int>(inner.getX()), labelY, 25, 11, juce::Justification::centredLeft, false);
        g.drawText("100",  static_cast<int>(inner.getX() + inner.getWidth() * 0.22f), labelY, 25, 11, juce::Justification::centred, false);
        g.drawText("1k",   static_cast<int>(inner.getX() + inner.getWidth() * 0.52f), labelY, 25, 11, juce::Justification::centred, false);
        g.drawText("10k",  static_cast<int>(inner.getX() + inner.getWidth() * 0.82f), labelY, 25, 11, juce::Justification::centred, false);
        g.drawText("20k",  static_cast<int>(inner.getRight() - 25), labelY, 25, 11, juce::Justification::centredRight, false);
    }

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    bool rtaMode_ = false;
    juce::Rectangle<int> rtaBtnRect_;

    juce::dsp::FFT fft_;
    std::array<float, 2048> circularBuf_{};
    std::size_t writePos_ = 0;
    std::array<float, 512> displayWave_{};
    float displayGain_ = 1.0f;
    std::array<float, 64> smoothedBins_{};
    std::array<float, 64> peakHoldBins_{};
};

} // namespace nova::synth
