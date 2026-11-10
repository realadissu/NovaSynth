add_library(nova_core_ui STATIC
    src/NovaTheme.cpp
    src/MorningLookAndFeel.cpp
    src/RadialPieMenu.cpp
    src/ArcKnob.cpp
    src/SectionDeck.cpp
    src/PluginShell.cpp
    src/PluginShellToolbar.cpp
)

target_include_directories(nova_core_ui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(nova_core_ui PUBLIC
    nova_core_plugin
    nova_core_dsp
    NovaSharedResources
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_processors
    juce::juce_graphics
)

target_compile_options(nova_core_ui PRIVATE
    -Wall -Wextra -Wpedantic
)
