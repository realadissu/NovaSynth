#include "JornaBottomFooterBar.h"
#include "../NovaSynthProcessor.h"
#include <vector>

namespace nova::synth {

JornaBottomFooterBar::JornaBottomFooterBar(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc), theme_(theme) {
    for (std::size_t i = 0; i < kAuditionPills.size(); ++i) {
        chordBtns_[i].setButtonText(kAuditionPills[i].name);
        if (i == 0) {
            // Highlight single note audition button with cyan text accent
            chordBtns_[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xff38bdf8));
            chordBtns_[i].setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        }
        chordBtns_[i].onStateChange = [this, i]() {
            const bool isDown = chordBtns_[i].isDown();
            if (isDown && !chordHeld_[i]) {
                chordHeld_[i] = true;
                for (int n = 0; n < kAuditionPills[i].count; ++n) {
                    proc_.triggerAuditionNoteOn(kAuditionPills[i].notes[static_cast<std::size_t>(n)], 0.82f);
                }
            } else if (!isDown && chordHeld_[i]) {
                chordHeld_[i] = false;
                for (int n = 0; n < kAuditionPills[i].count; ++n) {
                    proc_.triggerAuditionNoteOff(kAuditionPills[i].notes[static_cast<std::size_t>(n)]);
                }
            }
        };
        addAndMakeVisible(chordBtns_[i]);
    }
}

void JornaBottomFooterBar::paint(juce::Graphics& g) {
    g.setFont(theme_.getSectionFont(10.5f));

    int x = 6;
    const auto drawBadgeGroup = [&](const juce::String& groupTitle,
                                    juce::Colour titleCol,
                                    const std::vector<const char*>& pills) {
        g.setColour(titleCol);
        g.drawText(groupTitle, x, 0, 32, getHeight(), juce::Justification::centredLeft);
        x += 30;
        for (const char* p : pills) {
            const juce::Rectangle<float> pr(static_cast<float>(x), 2.0f, 20.0f, static_cast<float>(getHeight() - 4));
            g.setColour(theme_.bgElevated);
            g.fillRoundedRectangle(pr, 3.0f);
            g.setColour(theme_.textSecondary);
            g.drawText(p, pr.toNearestInt(), juce::Justification::centred);
            x += 23;
        }
        x += 10;
    };

    drawBadgeGroup("ENV",  juce::Colour(0xff38bdf8), { "E1", "E2", "E3", "E4" });
    drawBadgeGroup("LFO",  juce::Colour(0xfff59e0b), { "L1", "L2", "L3", "L4" });
    drawBadgeGroup("PLAY", juce::Colour(0xffeab308), { "VEL", "KEY", "MW" });
    drawBadgeGroup("MAC",  juce::Colour(0xffa855f7), { "1", "2", "3", "4" });
}

void JornaBottomFooterBar::resized() {
    auto rightArea = getLocalBounds().withTrimmedLeft(getWidth() - 410);
    const int bw = (rightArea.getWidth() - 28) / 7;
    for (auto& b : chordBtns_) {
        b.setBounds(rightArea.removeFromLeft(bw).reduced(0, 1));
        rightArea.removeFromLeft(4);
    }
}

} // namespace nova::synth
