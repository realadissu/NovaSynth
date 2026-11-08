add_executable(offline_pad_renderer
    offline_pad_renderer.cpp
)

target_link_libraries(offline_pad_renderer PRIVATE
    NovaSynth
    nova_core_ui
    nova_core_plugin
    nova_core_dsp
    NovaSharedResources
    juce::juce_audio_formats
    juce::juce_events
    juce::juce_audio_processors
    juce::juce_gui_basics
)

target_include_directories(offline_pad_renderer PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/nova_synth/src
)

target_compile_options(offline_pad_renderer PRIVATE
    -Wall -Wextra -Wpedantic
)
