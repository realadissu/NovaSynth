#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <nova_ui/NovaTheme.h>
#include <nova_ui/ArcKnob.h>
#include "../NovaSynthProcessor.h"
#include <array>
#include <memory>

namespace nova::synth::ui {

class MixerSlideOverDrawer : public juce::Component, private juce::Timer {
public:
    struct ConsoleFaderLookAndFeel : public juce::LookAndFeel_V4 {
        static float calculateTrackCenterX(float x, float width) noexcept;
        static float calculateThumbLeft(float x, float width) noexcept;
        void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    };

    struct DrawerCloseButton : public juce::Button {
        DrawerCloseButton();
        void paintButton(juce::Graphics& g, bool isHighlighted, bool isDown) override;
    };

    class ConsoleStripButton : public juce::Component {
    public:
        std::function<void()> onClick;
        void setButtonText(const juce::String& text);
        void setActive(bool active);
        [[nodiscard]] bool isActive() const noexcept { return isActive_; }
        void setColors(juce::Colour activeBg, juce::Colour activeBorder, juce::Colour activeText,
                       juce::Colour inactiveBg, juce::Colour inactiveBorder, juce::Colour inactiveText);
        void setFont(juce::Font font);
        void paint(juce::Graphics& g) override;
        void mouseEnter(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent& event) override;
    private:
        juce::String text_;
        bool isActive_ = false, isHovered_ = false, isMouseDown_ = false;
        juce::Font font_{juce::FontOptions(11.0f)};
        juce::Colour activeBg_{0xff38bdf8}, activeBorder_{0xff38bdf8}, activeText_{0xffffffff};
        juce::Colour inactiveBg_{0xff0d1522}, inactiveBorder_{0xff1e293b}, inactiveText_{0xff64748b};
    };

    struct OctaveStepperComponent : public juce::Component {
        std::function<void(int delta)> onStep;
        int currentOctave = 0;
        const nova::ui::NovaTheme& theme;
        juce::Image minusIcon, plusIcon;
        bool isLeftHover_ = false, isRightHover_ = false;
        OctaveStepperComponent(const nova::ui::NovaTheme& t, juce::Image minusImg, juce::Image plusImg);
        void paint(juce::Graphics& g) override;
        void mouseMove(const juce::MouseEvent& event) override;
        void mouseExit(const juce::MouseEvent&) override;
        void mouseDown(const juce::MouseEvent& event) override;
    };

    MixerSlideOverDrawer(NovaSynthProcessor& proc, const nova::ui::NovaTheme& theme);
    ~MixerSlideOverDrawer() override;
    [[nodiscard]] bool isDrawerOpen() const noexcept { return isOpen_; }
    void setDrawerOpen(bool open);
    void toggleDrawer();
    bool hitTest(int x, int y) override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    struct ChannelStripUI {
        std::unique_ptr<OctaveStepperComponent> octStepper;
        ConsoleStripButton flt1Btn, flt2Btn, soloBtn, muteBtn;
        std::unique_ptr<nova::ui::ArcKnob> pan;
        juce::Slider fader;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> faderAtt;
    };

    void initMasterAndEffectsControls();
    void toggleFilterRouting(int slotIdx, int targetFilter);
    void updateDimming();
    void timerCallback() override;
    static juce::Path buildDrawerFillPath(juce::Rectangle<float> dRect, juce::Rectangle<float> hArea, float winRight);
    static juce::Path buildDrawerStrokePath(juce::Rectangle<float> dRect, juce::Rectangle<float> hArea, float winRight);
    void drawClosedTab(juce::Graphics& g, int totalW, juce::Rectangle<float> hArea);
    void drawTabDetails(juce::Graphics& g, juce::Rectangle<float> hArea);
    void drawHeader(juce::Graphics& g);
    void drawChannelStrips(juce::Graphics& g);
    void drawMasterSection(juce::Graphics& g);
    static float getFaderThumbLeft(const juce::Slider& slider) noexcept;
    void drawChannelFaderTicks(juce::Graphics& g, const juce::Slider& slider, juce::Colour accentCol);
    void drawMasterFaderTicks(juce::Graphics& g, const juce::Slider& slider);
    static void drawMeterBar(juce::Graphics& g, juce::Rectangle<float> area, float level);
    static juce::Image loadIcon(const char* resourceName, const juce::String& fallbackFilename);
    static juce::Colour getLayerColour(int index) noexcept;
    static juce::String getLayerShortName(const nova::dsp::WorshipSynthEngine::Parameters& p, int i);
    static juce::String getEngineShortName(const nova::dsp::WorshipSynthEngine::Parameters& p, int i);

    NovaSynthProcessor& proc_;
    const nova::ui::NovaTheme& theme_;
    ConsoleFaderLookAndFeel faderLookAndFeel_;
    bool isOpen_ = false, isHandleHovered_ = false;
    float openProgress_ = 0.0f;
    int topY_ = 50, bottomY_ = 650;
    juce::Rectangle<int> drawerRect_, handleRect_, channelsCardRect_, masterCardRect_;
    std::array<ChannelStripUI, 8> strips_;
    std::array<juce::Rectangle<int>, 8> stripMeterRects_;
    juce::Slider masterFader_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterFaderAtt_;
    nova::ui::ArcKnob delayReturn_, reverbReturn_;
    juce::Rectangle<int> masterMeterRect_;
    DrawerCloseButton closeBtn_;
    juce::TextButton clearSolosBtn_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerSlideOverDrawer)
};
} // namespace nova::synth::ui
