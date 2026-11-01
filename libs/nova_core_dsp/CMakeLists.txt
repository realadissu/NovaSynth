add_library(nova_core_dsp STATIC
    src/DualEngineReverb.cpp
    src/MultiEngineOscillator.cpp
    src/SampleBankData.cpp
    src/WorshipSynthEngine.cpp
)

target_include_directories(nova_core_dsp PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_options(nova_core_dsp PRIVATE
    -Wall -Wextra -Wpedantic
)
