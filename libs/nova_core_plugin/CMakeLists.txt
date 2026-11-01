add_library(nova_core_plugin STATIC
    src/PresetManager.cpp
    src/NovaProcessorBase.cpp
)

target_include_directories(nova_core_plugin PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(nova_core_plugin PUBLIC
    nova_core_dsp
    juce::juce_audio_processors
    juce::juce_data_structures
    juce::juce_core
)

target_compile_options(nova_core_plugin PRIVATE
    -Wall -Wextra -Wpedantic
)
