# ==============================================================================
# Nova Plugins - External Dependencies (JUCE 8, CLAP Extensions, GoogleTest)
# ==============================================================================

set(JUCE_USE_MP3AUDIOFORMAT ON CACHE BOOL "Enable MP3 support" FORCE)
set(JUCE_USE_FLAC ON CACHE BOOL "Enable FLAC support" FORCE)
set(JUCE_USE_OGGVORBIS ON CACHE BOOL "Enable OGG Vorbis support" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "Disable JUCE examples" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "Disable JUCE extras" FORCE)

# 1. JUCE Framework
add_subdirectory("${CMAKE_SOURCE_DIR}/external/JUCE" "${CMAKE_BINARY_DIR}/external/JUCE" EXCLUDE_FROM_ALL)

# 2. CLAP JUCE Extensions (for native CLAP plugin targets alongside VST3/AU/Standalone)
set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "Disable CLAP examples" FORCE)
add_subdirectory("${CMAKE_SOURCE_DIR}/external/clap-juce-extensions" "${CMAKE_BINARY_DIR}/external/clap-juce-extensions" EXCLUDE_FROM_ALL)

# 3. Embedded Shared Fonts & Resources
juce_add_binary_data(NovaSharedResources
    NAMESPACE NovaResources
    SOURCES
        "${CMAKE_SOURCE_DIR}/Resources/Fonts/RajdhaniBold.ttf"
        "${CMAKE_SOURCE_DIR}/Resources/Fonts/RajdhaniLight.ttf"
        "${CMAKE_SOURCE_DIR}/Resources/Fonts/RajdhaniMedium.ttf"
        "${CMAKE_SOURCE_DIR}/Resources/Fonts/RajdhaniRegular.ttf"
        "${CMAKE_SOURCE_DIR}/Resources/Fonts/RajdhaniSemiBold.ttf"
        "${CMAKE_SOURCE_DIR}/Resources/Icons/pin_filled.png"
        "${CMAKE_SOURCE_DIR}/Resources/Icons/pin_outline.png"
        "${CMAKE_SOURCE_DIR}/Resources/Icons/NovaSynth_Icon_256.png"
        "${CMAKE_SOURCE_DIR}/Resources/Icons/NovaSynth_Logo.png"
        "${CMAKE_SOURCE_DIR}/Resources/Icons/octave_minus.png"
        "${CMAKE_SOURCE_DIR}/Resources/Icons/octave_plus.png"
        "${CMAKE_SOURCE_DIR}/Resources/Samples/01_Steinway_Grand_Piano_Hammer.wav"
        "${CMAKE_SOURCE_DIR}/Resources/Samples/02_Concert_Harp_Acoustic_Pluck.wav"
        "${CMAKE_SOURCE_DIR}/Resources/Samples/03_Yamaha_Upright_Felt_Thump.wav"
        "${CMAKE_SOURCE_DIR}/Resources/Samples/04_Celesta_Soft_Mallet_Bell.wav"
        "${CMAKE_SOURCE_DIR}/Resources/Samples/05_Orchestral_Glockenspiel_Chime.wav"
        "${CMAKE_SOURCE_DIR}/Resources/Samples/06_Worship_Hand_Chimes.wav"
        "${CMAKE_SOURCE_DIR}/Resources/Samples/07_African_Kalimba_Tine.wav"
        "${CMAKE_SOURCE_DIR}/Resources/Samples/08_Sacred_Flute_Organ_Pipe.wav"
)
set_target_properties(NovaSharedResources PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# 4. GoogleTest (for TDD & Realtime Safety Tests)
set(BUILD_GMOCK OFF CACHE BOOL "Disable GMock" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Disable GTest install" FORCE)
add_subdirectory("${CMAKE_SOURCE_DIR}/external/googletest" "${CMAKE_BINARY_DIR}/external/googletest" EXCLUDE_FROM_ALL)
