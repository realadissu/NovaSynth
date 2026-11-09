#include "../NovaSynthProcessor.h"

namespace nova::synth {

juce::File NovaSynthProcessor::getUserArpRhythmPresetsFile() const {
    const auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support")
        .getChildFile("Nova Audio");
    if (!dir.exists()) {
        dir.createDirectory();
    }
    return dir.getChildFile("UserArpRhythmPresets.json");
}

std::vector<NovaSynthProcessor::ArpRhythmPreset> NovaSynthProcessor::getAvailableArpRhythmPresets() const {
    std::vector<ArpRhythmPreset> presets;

    // ---------------- 16 STEPS (1 Bar) ----------------
    {
        ArpRhythmPreset p;
        p.name = "Straight 16 (Full Drive)";
        p.length = 16;
        p.gates.assign(16, true);
        p.velocities = { 1.0f, 0.70f, 0.85f, 0.70f,
                         0.95f, 0.70f, 0.85f, 0.70f,
                         1.0f,  0.70f, 0.85f, 0.70f,
                         0.95f, 0.70f, 0.85f, 0.75f };
        p.isUser = false;
        presets.push_back(std::move(p));
    }
    {
        ArpRhythmPreset p;
        p.name = "Dotted 8th Worship Pulse";
        p.length = 16;
        p.gates.assign(16, false);
        p.velocities.assign(16, 0.50f);
        const int hits[] = { 0, 3, 6, 9, 12, 15 };
        const float vels[] = { 1.0f, 0.85f, 0.90f, 0.85f, 0.95f, 0.80f };
        for (int i = 0; i < 6; ++i) {
            p.gates[static_cast<std::size_t>(hits[i])] = true;
            p.velocities[static_cast<std::size_t>(hits[i])] = vels[i];
        }
        p.isUser = false;
        presets.push_back(std::move(p));
    }
    {
        ArpRhythmPreset p;
        p.name = "Offbeat Syncopation";
        p.length = 16;
        p.gates.assign(16, false);
        p.velocities.assign(16, 0.50f);
        const int hits[] = { 0, 2, 5, 6, 8, 10, 13, 14 };
        const float vels[] = { 0.65f, 0.95f, 0.60f, 0.90f, 0.70f, 0.95f, 0.60f, 0.90f };
        for (int i = 0; i < 8; ++i) {
            p.gates[static_cast<std::size_t>(hits[i])] = true;
            p.velocities[static_cast<std::size_t>(hits[i])] = vels[i];
        }
        p.isUser = false;
        presets.push_back(std::move(p));
    }
    {
        ArpRhythmPreset p;
        p.name = "Four-on-the-Floor Groove";
        p.length = 16;
        p.gates.assign(16, false);
        p.velocities.assign(16, 0.50f);
        const int hits[] = { 0, 2, 4, 6, 8, 10, 12, 14, 15 };
        const float vels[] = { 1.0f, 0.65f, 0.95f, 0.65f, 1.0f, 0.65f, 0.95f, 0.70f, 0.80f };
        for (int i = 0; i < 9; ++i) {
            p.gates[static_cast<std::size_t>(hits[i])] = true;
            p.velocities[static_cast<std::size_t>(hits[i])] = vels[i];
        }
        p.isUser = false;
        presets.push_back(std::move(p));
    }
    {
        ArpRhythmPreset p;
        p.name = "Euphoric Build";
        p.length = 16;
        p.gates.assign(16, false);
        p.velocities.assign(16, 0.50f);
        const int hits[] = { 0, 4, 8, 10, 12, 13, 14, 15 };
        const float vels[] = { 0.45f, 0.55f, 0.65f, 0.75f, 0.85f, 0.90f, 0.95f, 1.0f };
        for (int i = 0; i < 8; ++i) {
            p.gates[static_cast<std::size_t>(hits[i])] = true;
            p.velocities[static_cast<std::size_t>(hits[i])] = vels[i];
        }
        p.isUser = false;
        presets.push_back(std::move(p));
    }
    {
        ArpRhythmPreset p;
        p.name = "Ambient Sparse Chime";
        p.length = 16;
        p.gates.assign(16, false);
        p.velocities.assign(16, 0.50f);
        const int hits[] = { 0, 3, 8, 11 };
        const float vels[] = { 0.85f, 0.65f, 0.80f, 0.60f };
        for (int i = 0; i < 4; ++i) {
            p.gates[static_cast<std::size_t>(hits[i])] = true;
            p.velocities[static_cast<std::size_t>(hits[i])] = vels[i];
        }
        p.isUser = false;
        presets.push_back(std::move(p));
    }

    // ---------------- 32 STEPS (2 Bars) ----------------
    {
        ArpRhythmPreset p;
        p.name = "Bethel 2-Bar Anthem";
        p.length = 32;
        p.gates.assign(32, false);
        p.velocities.assign(32, 0.60f);
        const int b1[] = { 0, 3, 6, 9, 12, 15 };
        for (int h : b1) { p.gates[static_cast<std::size_t>(h)] = true; p.velocities[static_cast<std::size_t>(h)] = 0.90f; }
        const int b2[] = { 16, 18, 20, 22, 24, 26, 27, 28, 29, 30, 31 };
        const float b2v[] = { 0.95f, 0.70f, 0.90f, 0.70f, 1.0f, 0.80f, 0.85f, 0.90f, 0.95f, 0.98f, 1.0f };
        for (int i = 0; i < 11; ++i) {
            p.gates[static_cast<std::size_t>(b2[i])] = true;
            p.velocities[static_cast<std::size_t>(b2[i])] = b2v[i];
        }
        p.isUser = false;
        presets.push_back(std::move(p));
    }
    {
        ArpRhythmPreset p;
        p.name = "Call & Response 32";
        p.length = 32;
        p.gates.assign(32, false);
        p.velocities.assign(32, 0.60f);
        const int hits[] = { 0, 2, 4, 6, 8, 10, 12, 14, 16, 20, 24, 28 };
        const float vels[] = { 0.95f, 0.70f, 0.90f, 0.70f, 0.95f, 0.70f, 0.85f, 0.65f, 1.0f, 0.80f, 0.90f, 0.75f };
        for (int i = 0; i < 12; ++i) {
            p.gates[static_cast<std::size_t>(hits[i])] = true;
            p.velocities[static_cast<std::size_t>(hits[i])] = vels[i];
        }
        p.isUser = false;
        presets.push_back(std::move(p));
    }
    {
        ArpRhythmPreset p;
        p.name = "Polyrhythmic 3+3+2 Flow";
        p.length = 32;
        p.gates.assign(32, false);
        p.velocities.assign(32, 0.50f);
        const int hits[] = { 0, 3, 6, 8, 11, 14, 16, 19, 22, 24, 27, 30 };
        for (int h : hits) {
            p.gates[static_cast<std::size_t>(h)] = true;
            p.velocities[static_cast<std::size_t>(h)] = (h % 8 == 0) ? 1.0f : 0.82f;
        }
        p.isUser = false;
        presets.push_back(std::move(p));
    }
    {
        ArpRhythmPreset p;
        p.name = "Progressive Velocity Swell";
        p.length = 32;
        p.gates.assign(32, true);
        p.velocities.resize(32);
        for (int s = 0; s < 32; ++s) {
            p.velocities[static_cast<std::size_t>(s)] = 0.35f + 0.65f * (static_cast<float>(s) / 31.0f);
        }
        p.isUser = false;
        presets.push_back(std::move(p));
    }

    // ---------------- 64 STEPS (4 Bars) ----------------
    {
        ArpRhythmPreset p;
        p.name = "4-Bar Worship Journey";
        p.length = 64;
        p.gates.assign(64, false);
        p.velocities.assign(64, 0.60f);
        for (int h : { 0, 4, 8, 12 }) { p.gates[static_cast<std::size_t>(h)] = true; p.velocities[static_cast<std::size_t>(h)] = 0.75f; }
        for (int h : { 16, 19, 22, 25, 28, 31 }) { p.gates[static_cast<std::size_t>(h)] = true; p.velocities[static_cast<std::size_t>(h)] = 0.85f; }
        for (int h : { 32, 34, 36, 38, 40, 42, 44, 46, 47 }) { p.gates[static_cast<std::size_t>(h)] = true; p.velocities[static_cast<std::size_t>(h)] = 0.90f; }
        const int b4[] = { 48, 50, 52, 54, 56, 58, 59, 60, 61, 62, 63 };
        const float b4v[] = { 1.0f, 0.80f, 0.95f, 0.80f, 1.0f, 0.85f, 0.90f, 0.92f, 0.95f, 0.98f, 1.0f };
        for (int i = 0; i < 11; ++i) {
            p.gates[static_cast<std::size_t>(b4[i])] = true;
            p.velocities[static_cast<std::size_t>(b4[i])] = b4v[i];
        }
        p.isUser = false;
        presets.push_back(std::move(p));
    }
    {
        ArpRhythmPreset p;
        p.name = "Electronic 4-Bar Arp";
        p.length = 64;
        p.gates.assign(64, false);
        p.velocities.assign(64, 0.70f);
        for (int s = 0; s < 64; ++s) {
            const int bar = s / 16;
            const int beat = s % 16;
            bool hit = false;
            float v = 0.70f;
            if (bar < 2) {
                hit = (beat % 2 == 0);
                v = (beat % 4 == 0) ? 0.95f : 0.65f;
            } else if (bar == 2) {
                hit = (beat % 2 == 0) || (beat >= 12);
                v = (beat % 4 == 0) ? 1.0f : 0.75f;
            } else {
                hit = true;
                v = 0.60f + 0.40f * (static_cast<float>(beat) / 15.0f);
            }
            p.gates[static_cast<std::size_t>(s)] = hit;
            p.velocities[static_cast<std::size_t>(s)] = v;
        }
        p.isUser = false;
        presets.push_back(std::move(p));
    }
    {
        ArpRhythmPreset p;
        p.name = "Minimalist Ambient Pulse 64";
        p.length = 64;
        p.gates.assign(64, false);
        p.velocities.assign(64, 0.50f);
        const int sparse[] = { 0, 7, 14, 21, 28, 35, 42, 49, 56, 61, 63 };
        for (int h : sparse) {
            p.gates[static_cast<std::size_t>(h)] = true;
            p.velocities[static_cast<std::size_t>(h)] = (h == 0 || h == 28) ? 0.95f : 0.70f;
        }
        p.isUser = false;
        presets.push_back(std::move(p));
    }

    // Load User Presets from disk
    const auto file = getUserArpRhythmPresetsFile();
    if (file.existsAsFile()) {
        const auto parsed = juce::JSON::parse(file.loadFileAsString());
        if (parsed.isObject()) {
            if (auto* obj = parsed.getDynamicObject()) {
                const auto presetsVar = obj->getProperty("presets");
                if (presetsVar.isArray()) {
                    if (auto* arr = presetsVar.getArray()) {
                        for (const auto& itemVar : *arr) {
                            if (!itemVar.isObject()) continue;
                            auto* item = itemVar.getDynamicObject();
                            if (item == nullptr) continue;

                            ArpRhythmPreset up;
                            up.name = item->getProperty("name").toString();
                            up.length = std::clamp(static_cast<int>(item->getProperty("length")), 16, 64);
                            up.isUser = true;

                            const auto gVar = item->getProperty("gates");
                            if (gVar.isArray()) {
                                if (auto* gArr = gVar.getArray()) {
                                    up.gates.resize(static_cast<std::size_t>(up.length), false);
                                    for (int i = 0; i < up.length && i < gArr->size(); ++i) {
                                        up.gates[static_cast<std::size_t>(i)] = static_cast<bool>((*gArr)[i]);
                                    }
                                }
                            }
                            if (up.gates.size() < static_cast<std::size_t>(up.length)) {
                                up.gates.resize(static_cast<std::size_t>(up.length), true);
                            }

                            const auto vVar = item->getProperty("velocities");
                            if (vVar.isArray()) {
                                if (auto* vArr = vVar.getArray()) {
                                    up.velocities.resize(static_cast<std::size_t>(up.length), 0.8f);
                                    for (int i = 0; i < up.length && i < vArr->size(); ++i) {
                                        up.velocities[static_cast<std::size_t>(i)] = static_cast<float>((*vArr)[i]);
                                    }
                                }
                            }
                            if (up.velocities.size() < static_cast<std::size_t>(up.length)) {
                                up.velocities.resize(static_cast<std::size_t>(up.length), 0.8f);
                            }

                            // Overwrite factory entry if same name, or append
                            bool replaced = false;
                            for (auto& fp : presets) {
                                if (fp.name.equalsIgnoreCase(up.name)) {
                                    fp = up;
                                    replaced = true;
                                    break;
                                }
                            }
                            if (!replaced) {
                                presets.push_back(std::move(up));
                            }
                        }
                    }
                }
            }
        }
    }

    return presets;
}

bool NovaSynthProcessor::saveUserArpRhythmPreset(const juce::String& name, int length,
                                                 const std::vector<bool>& gates,
                                                 const std::vector<float>& velocities) {
    if (name.trim().isEmpty()) return false;

    // Load existing user presets
    std::vector<ArpRhythmPreset> userPresets;
    const auto file = getUserArpRhythmPresetsFile();
    if (file.existsAsFile()) {
        const auto parsed = juce::JSON::parse(file.loadFileAsString());
        if (parsed.isObject()) {
            if (auto* obj = parsed.getDynamicObject()) {
                const auto presetsVar = obj->getProperty("presets");
                if (presetsVar.isArray()) {
                    if (auto* arr = presetsVar.getArray()) {
                        for (const auto& itemVar : *arr) {
                            if (!itemVar.isObject()) continue;
                            auto* item = itemVar.getDynamicObject();
                            if (item == nullptr) continue;

                            ArpRhythmPreset up;
                            up.name = item->getProperty("name").toString();
                            up.length = std::clamp(static_cast<int>(item->getProperty("length")), 16, 64);
                            up.isUser = true;

                            const auto gVar = item->getProperty("gates");
                            if (gVar.isArray()) {
                                if (auto* gArr = gVar.getArray()) {
                                    up.gates.resize(static_cast<std::size_t>(up.length), false);
                                    for (int i = 0; i < up.length && i < gArr->size(); ++i) {
                                        up.gates[static_cast<std::size_t>(i)] = static_cast<bool>((*gArr)[i]);
                                    }
                                }
                            }

                            const auto vVar = item->getProperty("velocities");
                            if (vVar.isArray()) {
                                if (auto* vArr = vVar.getArray()) {
                                    up.velocities.resize(static_cast<std::size_t>(up.length), 0.8f);
                                    for (int i = 0; i < up.length && i < vArr->size(); ++i) {
                                        up.velocities[static_cast<std::size_t>(i)] = static_cast<float>((*vArr)[i]);
                                    }
                                }
                            }
                            userPresets.push_back(std::move(up));
                        }
                    }
                }
            }
        }
    }

    // Check if updating existing user preset
    const int cleanLen = (length == 32) ? 32 : (length == 64) ? 64 : 16;
    bool found = false;
    for (auto& up : userPresets) {
        if (up.name.equalsIgnoreCase(name.trim())) {
            up.name = name.trim();
            up.length = cleanLen;
            const auto countG = static_cast<std::ptrdiff_t>(std::min(gates.size(), static_cast<std::size_t>(cleanLen)));
            const auto countV = static_cast<std::ptrdiff_t>(std::min(velocities.size(), static_cast<std::size_t>(cleanLen)));
            up.gates.assign(gates.begin(), gates.begin() + countG);
            up.velocities.assign(velocities.begin(), velocities.begin() + countV);
            found = true;
            break;
        }
    }
    if (!found) {
        ArpRhythmPreset up;
        up.name = name.trim();
        up.length = cleanLen;
        const auto countG = static_cast<std::ptrdiff_t>(std::min(gates.size(), static_cast<std::size_t>(cleanLen)));
        const auto countV = static_cast<std::ptrdiff_t>(std::min(velocities.size(), static_cast<std::size_t>(cleanLen)));
        up.gates.assign(gates.begin(), gates.begin() + countG);
        up.velocities.assign(velocities.begin(), velocities.begin() + countV);
        up.isUser = true;
        userPresets.push_back(std::move(up));
    }

    // Serialize to JSON
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    juce::Array<juce::var> arr;
    for (const auto& p : userPresets) {
        juce::DynamicObject::Ptr item = new juce::DynamicObject();
        item->setProperty("name", p.name);
        item->setProperty("length", p.length);
        juce::Array<juce::var> gArr;
        for (bool b : p.gates) gArr.add(b);
        item->setProperty("gates", gArr);
        juce::Array<juce::var> vArr;
        for (float v : p.velocities) vArr.add(v);
        item->setProperty("velocities", vArr);
        arr.add(item.get());
    }
    root->setProperty("presets", arr);

    file.replaceWithText(juce::JSON::toString(juce::var(root.get()), true));
    return true;
}

bool NovaSynthProcessor::deleteUserArpRhythmPreset(const juce::String& name) {
    const auto file = getUserArpRhythmPresetsFile();
    if (!file.existsAsFile()) return false;

    auto parsed = juce::JSON::parse(file.loadFileAsString());
    if (!parsed.isObject()) return false;
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr) return false;

    auto presetsVar = obj->getProperty("presets");
    if (!presetsVar.isArray()) return false;
    auto* arr = presetsVar.getArray();
    if (arr == nullptr) return false;

    juce::Array<juce::var> newArr;
    bool removed = false;
    for (const auto& itemVar : *arr) {
        if (auto* item = itemVar.getDynamicObject()) {
            if (item->getProperty("name").toString().equalsIgnoreCase(name.trim())) {
                removed = true;
                continue;
            }
        }
        newArr.add(itemVar);
    }

    if (removed) {
        juce::DynamicObject::Ptr root = new juce::DynamicObject();
        root->setProperty("presets", newArr);
        file.replaceWithText(juce::JSON::toString(juce::var(root.get()), true));
        return true;
    }
    return false;
}

} // namespace nova::synth
