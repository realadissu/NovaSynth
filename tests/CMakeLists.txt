add_executable(NovaPluginsTests
    NovaPluginsTests.cpp
    NovaSynthRegressionTests.cpp
    PsychoacousticPadOptimizationTests.cpp
)

target_link_libraries(NovaPluginsTests PRIVATE
    gtest
    NovaSynth
    nova_core_ui
    nova_core_plugin
    nova_core_dsp
    NovaSharedResources
    juce::juce_events
    juce::juce_audio_processors
    juce::juce_gui_basics
)

target_include_directories(NovaPluginsTests PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/nova_synth/src
)

target_compile_options(NovaPluginsTests PRIVATE
    -Wall -Wextra -Wpedantic
)

include(GoogleTest)
gtest_discover_tests(NovaPluginsTests DISCOVERY_TIMEOUT 60)
