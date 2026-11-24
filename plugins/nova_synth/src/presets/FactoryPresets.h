#pragma once

namespace nova::plugin {
class PresetManager;
}

namespace nova::synth {

void registerFactoryPresetsWarmPads(nova::plugin::PresetManager& presetManager_);
void registerFactoryPresetsAiryPads(nova::plugin::PresetManager& presetManager_);
void registerFactoryPresetsUtility(nova::plugin::PresetManager& presetManager_);

} // namespace nova::synth
