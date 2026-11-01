#include "nova_ui/SectionDeck.h"

namespace nova::ui {

namespace {
constexpr int kHeaderHeight = 36;
} // namespace

SectionDeck::SectionDeck(const juce::String& sectionTitle,
                         const NovaTheme& theme,
                         bool allowDynamicAdd)
    : sectionTitle_(sectionTitle), theme_(theme), allowDynamicAdd_(allowDynamicAdd) {}

int SectionDeck::addSlot(const juce::String& slotTitle,
                         std::unique_ptr<juce::Component> contentComponent,
                         bool useSecondaryAccent) {
    SlotEntry entry;
    entry.title = slotTitle;
    entry.content = std::move(contentComponent);
    entry.useSecondaryAccent = useSecondaryAccent;

    if (entry.content != nullptr) {
        addChildComponent(entry.content.get());
    }

    slots_.push_back(std::move(entry));
    const int newIndex = static_cast<int>(slots_.size()) - 1;
    if (slots_.size() == 1) {
        selectSlot(0, false);
    }
    resized();
    repaint();
    return newIndex;
}

void SectionDeck::selectSlot(int index, bool sendNotification) {
    if (index < 0 || index >= static_cast<int>(slots_.size())) {
        return;
    }

    activeSlotIndex_ = index;
    for (int i = 0; i < static_cast<int>(slots_.size()); ++i) {
        if (auto* comp = slots_[static_cast<std::size_t>(i)].content.get()) {
            comp->setVisible(i == activeSlotIndex_);
        }
    }

    resized();
    repaint();

    if (sendNotification && onSlotSelected) {
        onSlotSelected(activeSlotIndex_);
    }
}

void SectionDeck::setSlotMiniPreviewValues(int index, const std::vector<float>& normalizedCurve) {
    if (index >= 0 && index < static_cast<int>(slots_.size())) {
        slots_[static_cast<std::size_t>(index)].miniPreview = normalizedCurve;
        repaint(0, 0, getWidth(), kHeaderHeight);
    }
}

void SectionDeck::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);

    // Card surface + subtle 1px border
    g.setColour(theme_.bgSurface);
    g.fillRoundedRectangle(bounds, theme_.cornerRadius);

    // Header strip background
    auto headerRect = bounds;
    headerRect.setHeight(static_cast<float>(kHeaderHeight));
    g.setColour(theme_.bgHeader.withAlpha(0.55f));
    g.fillRoundedRectangle(headerRect, theme_.cornerRadius);

    // Outer border
    g.setColour(theme_.borderSubtle);
    g.drawRoundedRectangle(bounds, theme_.cornerRadius, 1.0f);

    // Divider line under header
    g.drawHorizontalLine(kHeaderHeight, bounds.getX(), bounds.getRight());

    // Section title on the left
    g.setFont(theme_.getSectionFont(13.5f));
    g.setColour(theme_.textMuted);
    g.drawText(sectionTitle_.toUpperCase(),
               juce::Rectangle<int>(14, 0, 140, kHeaderHeight),
               juce::Justification::centredLeft, false);

    // Draw Slot Tabs (JORNA style)
    for (int i = 0; i < static_cast<int>(slots_.size()); ++i) {
        const auto& slot = slots_[static_cast<std::size_t>(i)];
        const bool isSelected = (i == activeSlotIndex_);
        const auto accent = slot.useSecondaryAccent ? theme_.accentSecondary : theme_.accentPrimary;
        const auto tabF = slot.tabBounds.toFloat().reduced(2.0f, 5.0f);

        g.setColour(isSelected ? accent.withAlpha(0.18f) : theme_.bgElevated.withAlpha(0.75f));
        g.fillRoundedRectangle(tabF, theme_.pillRadius);

        g.setColour(isSelected ? accent : theme_.borderSubtle);
        g.drawRoundedRectangle(tabF, theme_.pillRadius, isSelected ? 1.4f : 1.0f);

        // Status LED dot
        g.setColour(isSelected ? accent : theme_.textMuted.withAlpha(0.5f));
        g.fillEllipse(tabF.getX() + 7.0f, tabF.getCentreY() - 3.0f, 6.0f, 6.0f);

        // Slot title
        g.setFont(theme_.getSectionFont(12.5f));
        g.setColour(isSelected ? theme_.textPrimary : theme_.textSecondary);
        const int previewReservation = slot.miniPreview.size() > 1 ? 28 : 4;
        g.drawFittedText(slot.title,
                         slot.tabBounds.withTrimmedLeft(18).withTrimmedRight(previewReservation),
                         juce::Justification::centredLeft, 1);

        // Mini Live Preview curve inside the tab (if provided)
        if (slot.miniPreview.size() > 1) {
            const auto curveBox = tabF.withLeft(tabF.getRight() - 26.0f).reduced(3.0f, 5.0f);
            juce::Path p;
            for (std::size_t k = 0; k < slot.miniPreview.size(); ++k) {
                const float normX = static_cast<float>(k) / static_cast<float>(slot.miniPreview.size() - 1);
                const float normY = 1.0f - std::clamp(slot.miniPreview[k], 0.0f, 1.0f);
                const float px = curveBox.getX() + normX * curveBox.getWidth();
                const float py = curveBox.getY() + normY * curveBox.getHeight();
                if (k == 0) p.startNewSubPath(px, py);
                else        p.lineTo(px, py);
            }
            g.setColour(accent.withAlpha(isSelected ? 0.95f : 0.55f));
            g.strokePath(p, juce::PathStrokeType(1.3f));
        }
    }

    // Optional [+] Dynamic Add button
    if (allowDynamicAdd_ && !addBtnBounds_.isEmpty()) {
        const auto addF = addBtnBounds_.toFloat().reduced(2.0f, 5.0f);
        g.setColour(theme_.bgElevated);
        g.fillRoundedRectangle(addF, theme_.pillRadius);
        g.setColour(theme_.borderFocus);
        g.drawRoundedRectangle(addF, theme_.pillRadius, 1.0f);
        g.setFont(theme_.getTitleFont(15.0f));
        g.setColour(theme_.accentPrimary);
        g.drawText("+", addBtnBounds_, juce::Justification::centred, false);
    }
}

void SectionDeck::resized() {
    const int titleAllowance = sectionTitle_.isNotEmpty() ? 132 : 12;
    int xCursor = titleAllowance;
    const int tabWidth = std::clamp((getWidth() - titleAllowance - (allowDynamicAdd_ ? 40 : 12)) /
                                    std::max(1, static_cast<int>(slots_.size())),
                                    86, 165);

    for (auto& slot : slots_) {
        slot.tabBounds = juce::Rectangle<int>(xCursor, 0, tabWidth, kHeaderHeight);
        xCursor += tabWidth + 4;
    }

    if (allowDynamicAdd_) {
        addBtnBounds_ = juce::Rectangle<int>(xCursor, 0, 28, kHeaderHeight);
    } else {
        addBtnBounds_ = {};
    }

    auto contentArea = getLocalBounds().withTrimmedTop(kHeaderHeight).reduced(8, 8);
    for (auto& slot : slots_) {
        if (slot.content != nullptr) {
            slot.content->setBounds(contentArea);
        }
    }
}

void SectionDeck::mouseDown(const juce::MouseEvent& event) {
    const auto pos = event.getPosition();
    if (pos.y > kHeaderHeight) {
        return;
    }

    for (int i = 0; i < static_cast<int>(slots_.size()); ++i) {
        if (slots_[static_cast<std::size_t>(i)].tabBounds.contains(pos)) {
            selectSlot(i, true);
            return;
        }
    }

    if (allowDynamicAdd_ && addBtnBounds_.contains(pos) && onAddSlotRequested) {
        onAddSlotRequested();
    }
}

} // namespace nova::ui
