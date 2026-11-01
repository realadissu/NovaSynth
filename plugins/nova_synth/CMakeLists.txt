nova_add_audio_plugin(NovaSynth
    PRODUCT_NAME "Nova Synth"
    PLUGIN_CODE "NvSy"
    BUNDLE_ID "${NOVA_BUNDLE_PREFIX}.novasynth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    ICON_BIG "${CMAKE_SOURCE_DIR}/Resources/Icons/NovaSynth.icns"
    ICON_SMALL "${CMAKE_SOURCE_DIR}/Resources/Icons/NovaSynth_Icon_256.png"
    CLAP_FEATURES "instrument" "synthesizer" "stereo"
    SOURCES
        src/NovaSynthProcessor.h
        src/NovaSynthProcessor.cpp
        src/NovaSynthEditor.h
        src/NovaSynthEditor.cpp
        src/MacDockIcon.h
        src/MacDockIcon.mm
)
