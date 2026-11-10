#pragma once

#include "nova_ui/NovaTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace nova::synth {

class NovaSynthProcessor;

class JornaBottomFooterBar : public juce::Component {
public:
    struct AuditionDef {
        const char* name;
        int count;
        std::array<int, 4> notes;
    };

    static constexpr std::array<AuditionDef, 7> kAuditionPills = {{
        { "C3",     1, { 60, 0,  0,  0  } },
        { "C Maj9", 4, { 48, 60, 64, 74 } },
        { "F Maj7", 4, { 41, 57, 60, 64 } },
        { "Am7",    4, { 45, 57, 60, 67 } },
        { "G6 Sus", 4, { 43, 55, 62, 64 } },
        { "Dm9",    4, { 50, 60, 65, 76 } },
        { "Em7",    4, { 40, 55, 59, 62 } }
    }};

    JornaBottomFooterBar(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~JornaBottomFooterBar() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    [[maybe_unused]] NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    std::array<juce::TextButton, 7> chordBtns_;
    std::array<bool, 7> chordHeld_{};
};

} // namespace nova::synth
