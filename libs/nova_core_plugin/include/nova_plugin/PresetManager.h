#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <map>
#include <functional>

namespace nova::plugin {

struct PresetEntry {
    juce::String name;
    juce::String category;
    juce::String description;
    juce::StringArray tags;
    std::map<juce::String, float> parameterValues;
    bool isFactory = true;
    bool isFavorite = false;
};

/**
 * @brief Shared Preset & A/B State Manager for all Nova Plugins.
 * Supports Factory Presets, User Presets, Tag/Category filtering, and instant A/B state comparison.
 */
class PresetManager {
public:
    PresetManager(juce::AudioProcessorValueTreeState& apvts, const juce::String& pluginFolderName);

    std::function<void()> onPresetLoaded;

    void registerFactoryPreset(PresetEntry preset);
    void loadPresetByIndex(int index);
    bool loadPresetByName(const juce::String& name);

    void loadNextPreset();
    void loadPreviousPreset();

    void saveUserPreset(const juce::String& name,
                        const juce::String& category,
                        const juce::String& description,
                        const juce::StringArray& tags);

    void loadUserPresetsFromDisk();
    [[nodiscard]] juce::File getPresetsDirectory() const;

    void toggleFavorite(int index);

    [[nodiscard]] const std::vector<PresetEntry>& getAllPresets() const noexcept { return presets_; }
    [[nodiscard]] std::vector<int> getFilteredPresetIndices(const juce::String& searchQuery,
                                                            const juce::String& categoryFilter = {},
                                                            bool favoritesOnly = false) const;
    [[nodiscard]] juce::StringArray getCategories() const;

    [[nodiscard]] int getCurrentPresetIndex() const noexcept { return currentIndex_; }
    [[nodiscard]] juce::String getCurrentPresetName() const;
    [[nodiscard]] juce::String getCurrentPresetCategory() const;

    // A/B State Comparison
    void switchABState();
    void copyActiveToOtherABState();
    [[nodiscard]] bool isSlotBActive() const noexcept { return isSlotB_; }

private:
    void captureCurrentStateToSlot(juce::ValueTree& slot);
    void applyStateFromSlot(const juce::ValueTree& slot);

    juce::AudioProcessorValueTreeState& apvts_;
    juce::String pluginFolderName_;
    std::vector<PresetEntry> presets_;
    int currentIndex_ = 0;

    bool isSlotB_ = false;
    juce::ValueTree slotAState_;
    juce::ValueTree slotBState_;
};

} // namespace nova::plugin
