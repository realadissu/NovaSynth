#pragma once

namespace nova::plugin {
class PresetManager;
}

namespace nova::synth {

void registerFactoryPresetsPads(nova::plugin::PresetManager& presetManager_);
void registerFactoryPresetsPlucks(nova::plugin::PresetManager& presetManager_);
void registerFactoryPresetsLeads(nova::plugin::PresetManager& presetManager_);
void registerFactoryPresetsAcoustic(nova::plugin::PresetManager& presetManager_);
void registerFactoryPresetsSwell(nova::plugin::PresetManager& presetManager_);

} // namespace nova::synth

