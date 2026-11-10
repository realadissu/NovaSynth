#include "JornaRightColumn.h"
#include "../NovaSynthProcessor.h"
#include <algorithm>

namespace nova::synth {

JornaRightColumn::JornaRightColumn(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme)
    : proc_(proc),
      theme_(theme),
      cutoffKnob_("CUTOFF", "Hz", theme, false),
      resoKnob_("RESO", "", theme, false),
      carveKnob_("CARVE", "", theme, true),
      driveKnob_("DRIVE", "", theme, false),
      envAmtKnob_("KEYTRK", "", theme, false),
      atkKnob_("ATK", "s", theme, false),
      decKnob_("DEC", "s", theme, false),
      susKnob_("SUS", "", theme, false),
      relKnob_("REL", "s", theme, false) {
    auto& apvts = proc_.getAPVTS();
    cutoffKnob_.attachToParameter(apvts, "filterCutoffHz");
    resoKnob_.attachToParameter(apvts,   "filterResonance");
    carveKnob_.attachToParameter(apvts,  "vocalCarve");
    driveKnob_.attachToParameter(apvts,  "filterDrive");
    envAmtKnob_.attachToParameter(apvts, "filterEnvAmount");

    addAndMakeVisible(cutoffKnob_);
    addAndMakeVisible(resoKnob_);
    addAndMakeVisible(carveKnob_);
    addAndMakeVisible(driveKnob_);
    addAndMakeVisible(envAmtKnob_);

    addAndMakeVisible(atkKnob_);
    addAndMakeVisible(decKnob_);
    addAndMakeVisible(susKnob_);
    addAndMakeVisible(relKnob_);

    selectFilterTab(0);
    selectEnvelopeTab(0);
}

void JornaRightColumn::selectFilterTab(int fltIdx) {
    activeFilterTab_ = std::clamp(fltIdx, 0, 1);
    auto& apvts = proc_.getAPVTS();
    if (activeFilterTab_ == 0) {
        cutoffKnob_.setLabel("CUTOFF");
        cutoffKnob_.setSuffix("Hz");
        cutoffKnob_.attachToParameter(apvts, "filterCutoffHz");

        resoKnob_.setLabel("RESO");
        resoKnob_.setSuffix("");
        resoKnob_.attachToParameter(apvts, "filterResonance");

        carveKnob_.setLabel("DRIVE");
        carveKnob_.setSuffix("");
        carveKnob_.attachToParameter(apvts, "filterDrive");

        driveKnob_.setLabel("ENV AMT");
        driveKnob_.setSuffix("");
        driveKnob_.attachToParameter(apvts, "filterEnvAmount");

        envAmtKnob_.setLabel("WARMTH");
        envAmtKnob_.setSuffix("");
        envAmtKnob_.attachToParameter(apvts, "macroWarmth");
    } else {
        cutoffKnob_.setLabel("CUTOFF");
        cutoffKnob_.setSuffix("Hz");
        cutoffKnob_.attachToParameter(apvts, "filter2CutoffHz");

        resoKnob_.setLabel("RESO");
        resoKnob_.setSuffix("");
        resoKnob_.attachToParameter(apvts, "filter2Resonance");

        carveKnob_.setLabel("DRIVE");
        carveKnob_.setSuffix("");
        carveKnob_.attachToParameter(apvts, "filter2Drive");

        driveKnob_.setLabel("ENV AMT");
        driveKnob_.setSuffix("");
        driveKnob_.attachToParameter(apvts, "filter2EnvAmount");

        envAmtKnob_.setLabel("CARVE");
        envAmtKnob_.setSuffix("");
        envAmtKnob_.attachToParameter(apvts, "vocalCarve");
    }
    repaint();
}

JornaRightColumn::EnvParamIds JornaRightColumn::getActiveEnvParamIds() const {
    switch (activeEnvTab_) {
        case 1:  return { "fltAttack", "fltDecay", "fltSustain", "fltRelease" };
        case 2:  return { "modAttack", "modDecay", "modSustain", "modRelease" };
        case 3:  return { "auxAttack", "auxDecay", "auxSustain", "auxRelease" };
        case 0:
        default: return { "ampAttack", "ampDecay", "ampSustain", "ampRelease" };
    }
}

void JornaRightColumn::selectEnvelopeTab(int envIdx) {
    activeEnvTab_ = std::clamp(envIdx, 0, 3);
    auto& apvts = proc_.getAPVTS();
    const auto ids = getActiveEnvParamIds();
    atkKnob_.attachToParameter(apvts, ids.attack);
    decKnob_.attachToParameter(apvts, ids.decay);
    susKnob_.attachToParameter(apvts, ids.sustain);
    relKnob_.attachToParameter(apvts, ids.release);
    repaint();
}

void JornaRightColumn::resized() {
    // Filter knobs: 3 on top row, 2 on second row (generous large bounds matching OSC section)
    const int w3 = (getWidth() - 16) / 3;
    cutoffKnob_.setBounds(8,          56, w3, 72);
    resoKnob_.setBounds(8 + w3,       56, w3, 72);
    carveKnob_.setBounds(8 + w3 * 2,  56, w3, 72);

    const int w2 = (getWidth() - 32) / 2;
    driveKnob_.setBounds(16,          128, w2, 72);
    envAmtKnob_.setBounds(16 + w2,    128, w2, 72);

    // Envelope knobs row (ATK, DEC, SUS, REL): full 72px height matching OSC knobs!
    const int envKnobTop = 242;
    const int ew = (getWidth() - 16) / 4;
    atkKnob_.setBounds(8,          envKnobTop, ew, 72);
    decKnob_.setBounds(8 + ew,     envKnobTop, ew, 72);
    susKnob_.setBounds(8 + ew * 2, envKnobTop, ew, 72);
    relKnob_.setBounds(8 + ew * 3, envKnobTop, ew, 72);
}

} // namespace nova::synth
