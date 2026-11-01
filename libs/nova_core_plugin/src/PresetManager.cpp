#include "nova_plugin/PresetManager.h"

namespace nova::plugin {

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts, const juce::String& pluginFolderName)
    : apvts_(apvts), pluginFolderName_(pluginFolderName) {
    slotAState_ = apvts_.copyState();
    slotBState_ = apvts_.copyState();
    loadUserPresetsFromDisk();
}

void PresetManager::registerFactoryPreset(PresetEntry preset) {
    preset.isFactory = true;
    presets_.push_back(std::move(preset));
    if (presets_.size() == 1) {
        loadPresetByIndex(0);
        captureCurrentStateToSlot(slotAState_);
        captureCurrentStateToSlot(slotBState_);
    }
}

void PresetManager::loadPresetByIndex(int index) {
    if (index < 0 || index >= static_cast<int>(presets_.size())) {
        return;
    }

    currentIndex_ = index;
    const auto& preset = presets_[static_cast<std::size_t>(index)];

    for (const auto& [paramId, rawVal] : preset.parameterValues) {
        if (auto* param = apvts_.getParameter(paramId)) {
            const float normVal = param->convertTo0to1(rawVal);
            param->setValueNotifyingHost(normVal);
        }
    }
}

bool PresetManager::loadPresetByName(const juce::String& name) {
    for (int i = 0; i < static_cast<int>(presets_.size()); ++i) {
        if (presets_[static_cast<std::size_t>(i)].name.equalsIgnoreCase(name)) {
            loadPresetByIndex(i);
            return true;
        }
    }
    return false;
}

void PresetManager::loadNextPreset() {
    if (presets_.empty()) return;
    const int next = (currentIndex_ + 1) % static_cast<int>(presets_.size());
    loadPresetByIndex(next);
}

void PresetManager::loadPreviousPreset() {
    if (presets_.empty()) return;
    const int prev = (currentIndex_ - 1 + static_cast<int>(presets_.size())) % static_cast<int>(presets_.size());
    loadPresetByIndex(prev);
}

juce::File PresetManager::getPresetsDirectory() const {
    const auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support")
        .getChildFile("Nova Audio")
        .getChildFile(pluginFolderName_)
        .getChildFile("Presets");
    if (!dir.exists()) {
        dir.createDirectory();
    }
    return dir;
}

void PresetManager::loadUserPresetsFromDisk() {
    auto dir = getPresetsDirectory();
    if (!dir.isDirectory()) return;

    const auto files = dir.findChildFiles(juce::File::findFiles, false, "*.novapreset");
    for (const auto& f : files) {
        const auto text = f.loadFileAsString();
        auto parsed = juce::JSON::parse(text);
        if (!parsed.isObject()) continue;

        PresetEntry entry;
        entry.name = parsed["name"].toString();
        entry.category = parsed["category"].toString();
        if (entry.category.isEmpty()) entry.category = "User";
        entry.description = parsed["description"].toString();
        entry.isFactory = false;

        if (auto* tagsArr = parsed["tags"].getArray()) {
            for (const auto& t : *tagsArr) entry.tags.add(t.toString());
        }

        if (auto* pObj = parsed["parameters"].getDynamicObject()) {
            for (const auto& prop : pObj->getProperties()) {
                entry.parameterValues[prop.name.toString()] = static_cast<float>(prop.value);
            }
        }

        if (entry.name.isNotEmpty()) {
            bool found = false;
            for (auto& p : presets_) {
                if (!p.isFactory && p.name.equalsIgnoreCase(entry.name)) {
                    p = entry;
                    found = true;
                    break;
                }
            }
            if (!found) {
                presets_.push_back(std::move(entry));
            }
        }
    }
}

void PresetManager::saveUserPreset(const juce::String& name,
                                   const juce::String& category,
                                   const juce::String& description,
                                   const juce::StringArray& tags) {
    PresetEntry entry;
    entry.name = name.isNotEmpty() ? name : "User Patch";
    entry.category = category.isNotEmpty() ? category : "User";
    entry.description = description;
    entry.tags = tags;
    entry.isFactory = false;

    // Capture ALL APVTS parameters!
    for (auto* param : apvts_.processor.getParameters()) {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param)) {
            const auto id = ranged->getParameterID();
            const float val = ranged->convertFrom0to1(ranged->getValue());
            entry.parameterValues[id] = val;
        }
    }

    // Write JSON file to disk
    auto dir = getPresetsDirectory();
    auto file = dir.getChildFile(entry.name + ".novapreset");
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("name", entry.name);
    obj->setProperty("category", entry.category);
    obj->setProperty("description", entry.description);

    juce::Array<juce::var> tagsVar;
    for (const auto& t : entry.tags) tagsVar.add(t);
    obj->setProperty("tags", tagsVar);

    juce::DynamicObject::Ptr pObj = new juce::DynamicObject();
    for (const auto& [k, v] : entry.parameterValues) {
        pObj->setProperty(k, v);
    }
    obj->setProperty("parameters", juce::var(pObj.get()));

    file.replaceWithText(juce::JSON::toString(juce::var(obj.get()), true));

    // Update in-memory presets: if exists, overwrite; else append
    for (int i = 0; i < static_cast<int>(presets_.size()); ++i) {
        if (!presets_[static_cast<std::size_t>(i)].isFactory &&
            presets_[static_cast<std::size_t>(i)].name.equalsIgnoreCase(entry.name)) {
            presets_[static_cast<std::size_t>(i)] = std::move(entry);
            currentIndex_ = i;
            return;
        }
    }

    presets_.push_back(std::move(entry));
    currentIndex_ = static_cast<int>(presets_.size()) - 1;
}

void PresetManager::toggleFavorite(int index) {
    if (index >= 0 && index < static_cast<int>(presets_.size())) {
        presets_[static_cast<std::size_t>(index)].isFavorite =
            !presets_[static_cast<std::size_t>(index)].isFavorite;
    }
}

std::vector<int> PresetManager::getFilteredPresetIndices(const juce::String& searchQuery,
                                                         const juce::String& categoryFilter,
                                                         bool favoritesOnly) const {
    std::vector<int> indices;
    const auto trimmedQuery = searchQuery.trim().toLowerCase();

    for (int i = 0; i < static_cast<int>(presets_.size()); ++i) {
        const auto& p = presets_[static_cast<std::size_t>(i)];
        if (favoritesOnly && !p.isFavorite) {
            continue;
        }
        if (categoryFilter.isNotEmpty() && !categoryFilter.equalsIgnoreCase("All")
            && !p.category.equalsIgnoreCase(categoryFilter)) {
            continue;
        }
        if (trimmedQuery.isNotEmpty()) {
            bool matches = p.name.toLowerCase().contains(trimmedQuery)
                        || p.category.toLowerCase().contains(trimmedQuery)
                        || p.description.toLowerCase().contains(trimmedQuery);
            if (!matches) {
                for (const auto& t : p.tags) {
                    if (t.toLowerCase().contains(trimmedQuery)) {
                        matches = true;
                        break;
                    }
                }
            }
            if (!matches) continue;
        }
        indices.push_back(i);
    }
    return indices;
}

juce::StringArray PresetManager::getCategories() const {
    juce::StringArray cats;
    cats.add("All");
    for (const auto& p : presets_) {
        cats.addIfNotAlreadyThere(p.category);
    }
    return cats;
}

juce::String PresetManager::getCurrentPresetName() const {
    if (currentIndex_ >= 0 && currentIndex_ < static_cast<int>(presets_.size())) {
        return presets_[static_cast<std::size_t>(currentIndex_)].name;
    }
    return "Init";
}

juce::String PresetManager::getCurrentPresetCategory() const {
    if (currentIndex_ >= 0 && currentIndex_ < static_cast<int>(presets_.size())) {
        return presets_[static_cast<std::size_t>(currentIndex_)].category;
    }
    return "Default";
}

void PresetManager::switchABState() {
    if (!isSlotB_) {
        captureCurrentStateToSlot(slotAState_);
        isSlotB_ = true;
        applyStateFromSlot(slotBState_);
    } else {
        captureCurrentStateToSlot(slotBState_);
        isSlotB_ = false;
        applyStateFromSlot(slotAState_);
    }
}

void PresetManager::copyActiveToOtherABState() {
    if (!isSlotB_) {
        captureCurrentStateToSlot(slotAState_);
        slotBState_ = slotAState_.createCopy();
    } else {
        captureCurrentStateToSlot(slotBState_);
        slotAState_ = slotBState_.createCopy();
    }
}

void PresetManager::captureCurrentStateToSlot(juce::ValueTree& slot) {
    slot = apvts_.copyState();
}

void PresetManager::applyStateFromSlot(const juce::ValueTree& slot) {
    if (slot.isValid()) {
        apvts_.replaceState(slot.createCopy());
    }
}

} // namespace nova::plugin
