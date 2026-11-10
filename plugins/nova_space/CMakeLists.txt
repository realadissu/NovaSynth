nova_add_audio_plugin(NovaSpace
    PRODUCT_NAME "Nova Space"
    PLUGIN_CODE "NvSp"
    BUNDLE_ID "${NOVA_BUNDLE_PREFIX}.novaspace"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    CLAP_FEATURES "audio-effect" "reverb" "stereo"
    SOURCES
        src/NovaSpaceProcessor.h
        src/NovaSpaceProcessor.cpp
        src/NovaSpaceEditor.h
        src/NovaSpaceEditor.cpp
        src/NovaSpaceEditorDrawing.cpp
)
