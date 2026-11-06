#include "nova_ui/ArcKnob.h"
#include "nova_ui/RadialPieMenu.h"

namespace nova::ui {

ArcKnob::ArcKnob(const juce::String& labelText,
                 const juce::String& suffix,
                 const NovaTheme& theme,
                 bool useSecondaryAccent)
    : labelText_(labelText), suffix_(suffix), theme_(theme) {
    slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    // Borderless compact value readout (JORNA style - no clunky white rectangle outline)
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 14);
    slider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider_.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider_.setColour(juce::Slider::textBoxTextColourId, theme_.textSecondary);
    slider_.setNumDecimalPlacesToDisplay(2);

    slider_.getProperties().set("useSecondaryAccent", useSecondaryAccent);
    slider_.addMouseListener(this, false);
    setupTextFormatters();
    addAndMakeVisible(slider_);
}

void ArcKnob::setupTextFormatters() {
    slider_.textFromValueFunction = [this](double val) -> juce::String {
        double displayVal = val;
        const bool isPercentage = (suffix_ == "%" && slider_.getMaximum() <= 1.05);
        if (isPercentage) {
            displayVal = val * 100.0;
        }

        if (suffix_ == "Hz" && std::abs(displayVal) >= 1000.0) {
            return juce::String(displayVal / 1000.0, std::abs(displayVal) >= 10000.0 ? 1 : 2) + "k";
        }

        juce::String s;
        if (std::abs(displayVal - std::round(displayVal)) < 0.005) {
            s = juce::String(static_cast<int>(std::round(displayVal)));
        } else if (std::abs(displayVal) >= 1000.0) {
            s = juce::String(static_cast<int>(std::round(displayVal)));
        } else if (std::abs(displayVal) >= 10.0) {
            s = juce::String(displayVal, 1);
        } else {
            s = juce::String(displayVal, 2);
        }
        if (suffix_.isNotEmpty()) {
            s += " " + suffix_;
        }
        return s;
    };

    slider_.valueFromTextFunction = [this](const juce::String& text) -> double {
        auto clean = text.trim();
        if (suffix_ == "Hz") {
            if (clean.endsWithIgnoreCase("khz")) {
                return clean.dropLastCharacters(3).trim().getDoubleValue() * 1000.0;
            }
            if (clean.endsWithIgnoreCase("k")) {
                return clean.dropLastCharacters(1).trim().getDoubleValue() * 1000.0;
            }
            if (clean.endsWithIgnoreCase("hz")) {
                return clean.dropLastCharacters(2).trim().getDoubleValue();
            }
            return clean.getDoubleValue();
        }

        if (suffix_.isNotEmpty() && clean.endsWithIgnoreCase(suffix_)) {
            clean = clean.dropLastCharacters(suffix_.length()).trim();
        }
        double v = clean.getDoubleValue();
        const bool isPercentage = (suffix_ == "%" && slider_.getMaximum() <= 1.05);
        if (isPercentage) {
            v /= 100.0;
        }
        return v;
    };
}

void ArcKnob::attachToParameter(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId) {
    // Explicitly destroy any previous SliderAttachment first, so that range updates
    // and initial value pushes do not leak back into the previously attached parameter!
    attachment_.reset();

    if (auto* param = apvts.getParameter(paramId)) {
        defaultValue_ = param->convertFrom0to1(param->getDefaultValue());
        slider_.setDoubleClickReturnValue(true, defaultValue_);
        isChoiceParam_ = (dynamic_cast<juce::AudioParameterChoice*>(param) != nullptr);
        attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, paramId, slider_);
        // If not a choice parameter, re-apply our custom formatters for units (Hz, %, s, etc.)
        if (!isChoiceParam_) {
            setupTextFormatters();
        }
    }
    slider_.updateText();
}

void ArcKnob::setModulation(float rangeDelta, float liveOffset) {
    const float prevMod = static_cast<float>(slider_.getProperties().getWithDefault("modAmount", 0.0));
    const float prevLive = static_cast<float>(slider_.getProperties().getWithDefault("liveModOffset", 0.0));
    if (std::abs(prevMod - rangeDelta) > 0.002f || std::abs(prevLive - liveOffset) > 0.002f) {
        slider_.getProperties().set("modAmount", rangeDelta);
        slider_.getProperties().set("liveModOffset", liveOffset);
        slider_.repaint();
    }
}

void ArcKnob::setModulationAmount(float normalizedDelta) {
    setModulation(normalizedDelta, 0.0f);
}

void ArcKnob::setLabel(const juce::String& newLabel) {
    labelText_ = newLabel;
    repaint();
}

void ArcKnob::setSuffix(const juce::String& newSuffix) {
    suffix_ = newSuffix;
    if (!isChoiceParam_) {
        setupTextFormatters();
    }
    slider_.updateText();
    repaint();
}

void ArcKnob::setCenterLabel(const juce::String& text) {
    hasCenterLabel_ = text.isNotEmpty();
    slider_.getProperties().set("centerLabel", text);
    resized();
    repaint();
}

void ArcKnob::paint(juce::Graphics& g) {
    if (hasCenterLabel_ && getHeight() < 64) return;
    const bool isCompact = (getHeight() < 50);
    auto topLabelBounds = getLocalBounds().removeFromTop(isCompact ? 11 : 13);
    g.setFont(theme_.getLabelFont(isCompact ? 8.5f : 10.5f));
    g.setColour(slider_.isMouseOverOrDragging() ? theme_.textPrimary : theme_.textSecondary);
    g.drawFittedText(labelText_.toUpperCase(), topLabelBounds, juce::Justification::centred, 1, 0.80f);
}

void ArcKnob::resized() {
    auto area = getLocalBounds();
    const bool isCompact = (getHeight() < 50);
    if (!hasCenterLabel_ || getHeight() >= 64) {
        area.removeFromTop(isCompact ? 11 : 12);
    }
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, std::min(68, getWidth() + 4), isCompact ? 11 : 14);
    slider_.setBounds(area);
}

void ArcKnob::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu()) {
        openRadialMenu(event.getScreenPosition());
    }
}

void ArcKnob::openRadialMenu(juce::Point<int> screenPos) {
    auto* topComp = getTopLevelComponent();
    if (topComp == nullptr) return;

    const auto minVal = slider_.getMinimum();
    const auto maxVal = slider_.getMaximum();
    const auto defVal = defaultValue_;

    std::vector<RadialPieMenu::SliceItem> slices = {
        { "Reset Default", [this, defVal]() { slider_.setValue(defVal, juce::sendNotificationSync); } },
        { "25%",           [this, minVal, maxVal]() { slider_.setValue(minVal + 0.25 * (maxVal - minVal), juce::sendNotificationSync); } },
        { "50% Center",    [this, minVal, maxVal]() { slider_.setValue(minVal + 0.50 * (maxVal - minVal), juce::sendNotificationSync); } },
        { "75%",           [this, minVal, maxVal]() { slider_.setValue(minVal + 0.75 * (maxVal - minVal), juce::sendNotificationSync); } },
        { "100% Max",      [this, maxVal]() { slider_.setValue(maxVal, juce::sendNotificationSync); } },
        { "0% Min",        [this, minVal]() { slider_.setValue(minVal, juce::sendNotificationSync); } }
    };

    RadialPieMenu::show(topComp, topComp->getLocalPoint(nullptr, screenPos), theme_, std::move(slices));
}

} // namespace nova::ui
