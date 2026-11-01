#include "nova_ui/RadialPieMenu.h"
#include <cmath>
#include <numbers>

namespace nova::ui {

namespace {
constexpr float kInnerRadius = 26.0f;
constexpr float kOuterRadius = 96.0f;
} // namespace

RadialPieMenu::RadialPieMenu(const NovaTheme& theme, std::vector<SliceItem> items)
    : theme_(theme), items_(std::move(items)) {
    setInterceptsMouseClicks(true, true);
}

void RadialPieMenu::show(juce::Component* topLevelParent,
                         juce::Point<int> centerInParent,
                         const NovaTheme& theme,
                         std::vector<SliceItem> items) {
    if (topLevelParent == nullptr || items.empty()) {
        return;
    }

    auto* menu = new RadialPieMenu(theme, std::move(items));
    menu->setBounds(topLevelParent->getLocalBounds());
    menu->menuCenter_ = centerInParent.toFloat();
    topLevelParent->addAndMakeVisible(menu);
    menu->toFront(true);
}

int RadialPieMenu::getSliceAtPoint(juce::Point<float> localPt) const noexcept {
    if (items_.empty()) return -1;
    const float dx = localPt.x - menuCenter_.x;
    const float dy = localPt.y - menuCenter_.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < kInnerRadius || dist > kOuterRadius + 14.0f) {
        return -1;
    }

    float angle = std::atan2(dx, -dy); // 0 at top, clockwise
    if (angle < 0.0f) {
        angle += 2.0f * std::numbers::pi_v<float>;
    }

    const float sliceSpan = (2.0f * std::numbers::pi_v<float>) / static_cast<float>(items_.size());
    float shifted = angle + sliceSpan * 0.5f;
    if (shifted >= 2.0f * std::numbers::pi_v<float>) {
        shifted -= 2.0f * std::numbers::pi_v<float>;
    }
    return static_cast<int>(shifted / sliceSpan) % static_cast<int>(items_.size());
}

void RadialPieMenu::paint(juce::Graphics& g) {
    // Subtle dark backdrop
    g.fillAll(juce::Colours::black.withAlpha(0.38f));

    if (items_.empty()) return;

    const int n = static_cast<int>(items_.size());
    const float sliceSpan = (2.0f * std::numbers::pi_v<float>) / static_cast<float>(n);
    const float gap = 0.045f;

    for (int i = 0; i < n; ++i) {
        const float startA = static_cast<float>(i) * sliceSpan - sliceSpan * 0.5f + gap;
        const float endA   = startA + sliceSpan - 2.0f * gap;
        const bool isHover = (i == hoveredSlice_);

        juce::Path sector;
        sector.addPieSegment(menuCenter_.x - kOuterRadius, menuCenter_.y - kOuterRadius,
                             kOuterRadius * 2.0f, kOuterRadius * 2.0f,
                             startA, endA, kInnerRadius / kOuterRadius);

        g.setColour(isHover ? theme_.accentPrimary.withAlpha(0.32f) : theme_.bgElevated.withAlpha(0.96f));
        g.fillPath(sector);

        g.setColour(isHover ? theme_.accentPrimary : theme_.borderFocus);
        g.strokePath(sector, juce::PathStrokeType(isHover ? 1.8f : 1.0f));

        // Label positioned at mid-radius
        const float midAngle = (startA + endA) * 0.5f;
        const float labelR = (kInnerRadius + kOuterRadius) * 0.52f;
        const float lx = menuCenter_.x + std::sin(midAngle) * labelR;
        const float ly = menuCenter_.y - std::cos(midAngle) * labelR;

        g.setFont(theme_.getSectionFont(12.5f));
        g.setColour(isHover ? theme_.textPrimary : theme_.textSecondary);
        g.drawFittedText(items_[static_cast<std::size_t>(i)].label,
                         juce::Rectangle<int>(static_cast<int>(lx - 38.0f), static_cast<int>(ly - 12.0f), 76, 24),
                         juce::Justification::centred, 2);
    }

    // Center hub circle
    g.setColour(theme_.bgHeader);
    g.fillEllipse(menuCenter_.x - kInnerRadius + 3.0f, menuCenter_.y - kInnerRadius + 3.0f,
                  (kInnerRadius - 3.0f) * 2.0f, (kInnerRadius - 3.0f) * 2.0f);
    g.setColour(theme_.accentPrimary.withAlpha(0.6f));
    g.drawEllipse(menuCenter_.x - kInnerRadius + 3.0f, menuCenter_.y - kInnerRadius + 3.0f,
                  (kInnerRadius - 3.0f) * 2.0f, (kInnerRadius - 3.0f) * 2.0f, 1.2f);
}

void RadialPieMenu::mouseMove(const juce::MouseEvent& event) {
    const int idx = getSliceAtPoint(event.position);
    if (idx != hoveredSlice_) {
        hoveredSlice_ = idx;
        repaint();
    }
}

void RadialPieMenu::mouseDown(const juce::MouseEvent& event) {
    const int idx = getSliceAtPoint(event.position);
    std::function<void()> callback;
    if (idx >= 0 && idx < static_cast<int>(items_.size())) {
        callback = items_[static_cast<std::size_t>(idx)].action;
    }

    if (auto* parent = getParentComponent()) {
        parent->removeChildComponent(this);
    }
    if (callback) {
        callback();
    }
    delete this;
}

} // namespace nova::ui
