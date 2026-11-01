# ==============================================================================
# Nova Plugins - Global Manufacturer Metadata & Plugin Factory Macro
# ==============================================================================

set(NOVA_COMPANY_NAME "Nova Audio")
set(NOVA_COMPANY_WEBSITE "https://novaplugins.audio")
set(NOVA_COMPANY_EMAIL "info@novaplugins.audio")
set(NOVA_MANUFACTURER_CODE "NovA") # 4-char code (uppercase + lowercase required by Apple auval)
set(NOVA_BUNDLE_PREFIX "com.novaplugins")

# Helper function to create a multi-format (AU, VST3, CLAP, Standalone) plugin
function(nova_add_audio_plugin TARGET_NAME)
    set(options "")
    set(oneValueArgs PRODUCT_NAME PLUGIN_CODE BUNDLE_ID VERSION_STRING IS_SYNTH NEEDS_MIDI_INPUT NEEDS_MIDI_OUTPUT IS_MIDI_EFFECT ICON_BIG ICON_SMALL)
    set(multiValueArgs SOURCES CLAP_FEATURES EXTRA_LIBS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_VERSION_STRING)
        set(ARG_VERSION_STRING "${PROJECT_VERSION}")
    endif()

    if(ARG_IS_SYNTH)
        set(VST3_CAT "Instrument" "Synth")
        set(AU_TYPE "kAudioUnitType_MusicDevice")
    else()
        set(VST3_CAT "Fx" "Reverb")
        set(AU_TYPE "kAudioUnitType_Effect")
    endif()

    set(NOVA_PLUGIN_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND NOVA_PLUGIN_FORMATS AU)
    endif()

    set(plugin_icon_args)
    if(ARG_ICON_BIG)
        list(APPEND plugin_icon_args ICON_BIG "${ARG_ICON_BIG}")
    endif()
    if(ARG_ICON_SMALL)
        list(APPEND plugin_icon_args ICON_SMALL "${ARG_ICON_SMALL}")
    endif()

    juce_add_plugin(${TARGET_NAME}
        COMPANY_NAME "${NOVA_COMPANY_NAME}"
        COMPANY_WEBSITE "${NOVA_COMPANY_WEBSITE}"
        COMPANY_EMAIL "${NOVA_COMPANY_EMAIL}"
        PLUGIN_MANUFACTURER_CODE "${NOVA_MANUFACTURER_CODE}"
        PLUGIN_CODE "${ARG_PLUGIN_CODE}"
        PRODUCT_NAME "${ARG_PRODUCT_NAME}"
        BUNDLE_ID "${ARG_BUNDLE_ID}"
        VERSION "${ARG_VERSION_STRING}"
        IS_SYNTH ${ARG_IS_SYNTH}
        NEEDS_MIDI_INPUT ${ARG_NEEDS_MIDI_INPUT}
        NEEDS_MIDI_OUTPUT ${ARG_NEEDS_MIDI_OUTPUT}
        IS_MIDI_EFFECT ${ARG_IS_MIDI_EFFECT}
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES ${VST3_CAT}
        AU_MAIN_TYPE ${AU_TYPE}
        FORMATS ${NOVA_PLUGIN_FORMATS}
        ${plugin_icon_args}
    )

    clap_juce_extensions_plugin(
        TARGET ${TARGET_NAME}
        CLAP_ID "${ARG_BUNDLE_ID}"
        CLAP_FEATURES ${ARG_CLAP_FEATURES}
    )

    target_sources(${TARGET_NAME} PRIVATE ${ARG_SOURCES})

    target_compile_definitions(${TARGET_NAME} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )

    target_link_libraries(${TARGET_NAME} PRIVATE
        nova_core_ui
        nova_core_plugin
        nova_core_dsp
        NovaSharedResources
        ${ARG_EXTRA_LIBS}
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )
endfunction()
