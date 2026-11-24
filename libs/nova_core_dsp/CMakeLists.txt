add_library(nova_core_dsp STATIC
    src/AnalogDriftGenerator.cpp
    src/DualEngineReverb.cpp
    src/DualEngineReverbProcess.cpp
    src/DualEngineReverbShimmer.cpp
    src/MultiEngineOscillator.cpp
    src/MultiEngineOscillatorEngines.cpp
    src/MultiEngineOscillatorPreview.cpp
    src/MultiEngineOscillatorSample.cpp
    src/PadSynthBank.cpp
    src/PadSynthProfiles.cpp
    src/SampleBankData.cpp
    src/StereoNoiseGenerator.cpp
    src/WorshipSynthAirStage.cpp
    src/WorshipSynthDroneArp.cpp
    src/WorshipSynthEngine.cpp
    src/WorshipSynthFilterHelper.cpp
    src/WorshipSynthMasterChain.cpp
    src/WorshipSynthProcess.cpp
    src/WorshipSynthVoices.cpp
)

target_include_directories(nova_core_dsp PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_options(nova_core_dsp PRIVATE
    -Wall -Wextra -Wpedantic
)
