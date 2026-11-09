add_library(nova_core_dsp STATIC
    src/DualEngineReverb.cpp
    src/DualEngineReverbProcess.cpp
    src/MultiEngineOscillator.cpp
    src/MultiEngineOscillatorEngines.cpp
    src/MultiEngineOscillatorPreview.cpp
    src/SampleBankData.cpp
    src/WorshipSynthDroneArp.cpp
    src/WorshipSynthEngine.cpp
    src/WorshipSynthProcess.cpp
    src/WorshipSynthVoices.cpp
)

target_include_directories(nova_core_dsp PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_options(nova_core_dsp PRIVATE
    -Wall -Wextra -Wpedantic
)
