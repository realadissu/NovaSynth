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
        src/NovaSynthParameters.cpp
        src/NovaSynthParametersFx.cpp
        src/NovaSynthParamSync.cpp
        src/NovaSynthEditor.h
        src/NovaSynthEditor.cpp
        src/ui/DualFilterDeck.h
        src/ui/DualFilterDeck.cpp
        src/ui/DualFilterDeckDrawing.cpp
        src/ui/JornaCenterStageColumn.h
        src/ui/JornaCenterStageColumn.cpp
        src/ui/JornaCenterStageColumnDrawing.cpp
        src/ui/JornaCenterStageColumnDrawingModes.cpp
        src/ui/JornaRightColumn.h
        src/ui/JornaRightColumn.cpp
        src/ui/JornaRightColumnDrawing.cpp
        src/ui/JornaRightColumnInteraction.cpp
        src/ui/JornaBottomFooterBar.h
        src/ui/JornaBottomFooterBar.cpp
        src/ui/NovaSynthWorkspacePage.h
        src/ui/NovaSynthWorkspacePage.cpp
        src/ui/NovaSynthUiHelpers.h
        src/ui/NovaSynthUiHelpers.cpp
        src/ui/JornaSourcesColumn.h
        src/ui/JornaSourcesColumn.cpp
        src/ui/JornaSourcesColumnDrawing.cpp
        src/ui/WorkbenchVisualizers.h
        src/ui/WorkbenchVisualizers.cpp
        src/ui/SignalFlowPanel.h
        src/ui/SignalFlowDrawing.cpp
        src/ui/SignalFlowView.cpp
        src/ui/SignalFlowInteraction.cpp
        src/ui/FxRackPanel.h
        src/ui/FxRackPanel.cpp
        src/ui/FxRackDrawing.cpp
        src/ui/JornaLowerWorkbench.h
        src/ui/JornaLowerWorkbench.cpp
        src/ui/SleekHorizontalSlider.h
        src/ui/SleekHorizontalSlider.cpp
        src/ui/SourceDeck.h
        src/ui/SourceDeck.cpp
        src/ui/ExpandableEnvelopeCard.h
        src/ui/ExpandableEnvelopeCard.cpp
        src/ui/ExpandableEnvelopeCardDrawing.cpp
        src/ui/EnvelopeQuadDeck.h
        src/ui/EnvelopeQuadDeck.cpp
        src/ui/ArpPatternSaveOverlay.h
        src/ui/ArpPatternSaveOverlay.cpp
        src/ui/ArpeggiatorHubPanel.h
        src/ui/ArpeggiatorHubPanel.cpp
        src/ui/ArpeggiatorHubPanelDrawing.cpp
        src/ui/ModulationHubPanel.h
        src/ui/ModulationHubPanel.cpp
        src/ui/ModulationHubPanelDrawing.cpp
        src/ui/ModulationHubPanelMatrix.cpp
        src/ui/MixerSlideOverDrawer.h
        src/ui/MixerSlideOverDrawer.cpp
        src/ui/MixerSlideOverDrawerLayout.cpp
        src/ui/MixerSlideOverDrawerDrawing.cpp
        src/ui/SourceCardComponent.h
        src/ui/SourceCardComponent.cpp
        src/ui/SourceCardComponentLayout.cpp
        src/ui/SourceCardComponentInteraction.cpp
        src/ui/SourceCardComponentDrawing.cpp
        src/ui/SourceCardVisualizers.cpp
        src/ui/SourceCardWaveforms.cpp
        src/presets/FactoryPresets.h
        src/presets/FactoryPresetsPads.cpp
        src/presets/FactoryPresetsPlucks.cpp
        src/presets/FactoryPresetsLeads.cpp
        src/presets/FactoryPresetsAcoustic.cpp
        src/presets/FactoryPresetsSwell.cpp
        src/presets/NovaSynthArpPresets.cpp
        src/MacDockIcon.h
        src/MacDockIcon.mm
)
