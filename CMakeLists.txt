cmake_minimum_required(VERSION 3.22)

project(ABDAudioLab VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable MSVC optimizations & UTF-8
if(MSVC)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>" "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /MP)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

set(JUCE_USE_WIN_WEBVIEW2 ON CACHE BOOL "Use WebView2 on Windows" FORCE)

include(FetchContent)

# ------------------------------------------------------------------------------
# Fetch JUCE 8
# ------------------------------------------------------------------------------
message(STATUS "Fetching JUCE 8.0.4...")
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# ------------------------------------------------------------------------------
# Fetch nlohmann_json for strict JSON parsing
# ------------------------------------------------------------------------------
message(STATUS "Fetching nlohmann_json...")
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ------------------------------------------------------------------------------
# Fetch Catch2 v3 for unit testing
# ------------------------------------------------------------------------------
message(STATUS "Fetching Catch2 v3.5.2...")
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

# ------------------------------------------------------------------------------
# Fetch Celemony ARA_SDK (Audio Random Access 2.0 API & Library)
# ------------------------------------------------------------------------------
message(STATUS "Fetching Celemony ARA_SDK...")
FetchContent_Declare(
    ARA_SDK
    GIT_REPOSITORY https://github.com/Celemony/ARA_SDK.git
    GIT_TAG releases/2.2.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ARA_SDK)

# ------------------------------------------------------------------------------
# ABDSharedCode (Shared Libraries: AutoUpdater, etc.)
# ------------------------------------------------------------------------------
set(ABDSHARED_CODE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode")
if(EXISTS "${ABDSHARED_CODE_DIR}/CMakeLists.txt")
    message(STATUS "ABDSharedCode: Using local at ${ABDSHARED_CODE_DIR}")
    add_subdirectory("${ABDSHARED_CODE_DIR}" "${CMAKE_BINARY_DIR}/ABDSharedCode")
else()
    include(FetchContent)
    FetchContent_Declare(
      ABDSharedCode
      GIT_REPOSITORY https://github.com/ajabadia/ABDSharedCode.git
      GIT_TAG        master
    )
    FetchContent_MakeAvailable(ABDSharedCode)
endif()

# ------------------------------------------------------------------------------
# ABDScope (Unified Audio Visualizer)
# ------------------------------------------------------------------------------
set(ABDSCOPE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ABDScope")
if(EXISTS "${ABDSCOPE_DIR}/CMakeLists.txt")
    message(STATUS "ABDScope: Using local workspace at ${ABDSCOPE_DIR}")
    add_subdirectory("${ABDSCOPE_DIR}" ABDScope_Build EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    message(STATUS "ABDScope: Fetching from GitHub repository...")
    FetchContent_Declare(
      ABDScope
      GIT_REPOSITORY https://github.com/ajabadia/ABDScope.git
      GIT_TAG        main
      GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(ABDScope)
endif()

# ------------------------------------------------------------------------------
# Target: ABDAudioLab (Standalone Executable)
# ------------------------------------------------------------------------------
juce_add_gui_app(ABDAudioLab
    PRODUCT_NAME "ABDAudioLab"
    VERSION "1.1.0"
    COMPANY_NAME "ABDSynths"
    BUNDLE_ID "com.abdsynths.abdaudiolab"
    NEEDS_WEBVIEW2 TRUE
)

# Source files
target_sources(ABDAudioLab
    PRIVATE
        src/main.cpp
        src/BuildVersion.h
        
        # Core Architecture
        src/core/ProfilingSession.h
        src/core/ProfilingSession.cpp
        src/core/ProfilingHardwareDispatcher.h
        src/core/ProfilingHardwareDispatcher.cpp
        src/core/ProfilingAudioCapture.h
        src/core/ProfilingAudioCapture.cpp
        src/core/ProfilingSequencer.h
        src/core/ProfilingSequencer.cpp
        src/core/HardwareContractRegistry.h
        src/core/HardwareContractRegistry.cpp
        src/core/SessionSerializer.h
        src/core/SessionSerializer.cpp
        src/core/ExperimentRecord.h
        src/core/ExperimentStorage.h
        src/core/ExperimentStorage.cpp
        src/core/LabDataDirectories.h
        src/core/LabDataDirectories.cpp
        src/core/GeneratedAcousticModel.h
        src/core/GeneratedAcousticModel.cpp
        src/core/HoldoutSequence.h
        src/core/HoldoutSequence.cpp
        src/core/ModelHoldoutValidator.h
        src/core/ModelHoldoutValidator.cpp
        src/core/ValidationUiSummary.h
        src/core/ValidationUiSummary.cpp
        src/core/SessionManager.h
        src/core/SessionManager.cpp
        src/core/HardwareManager.h
        src/core/HardwareManager.cpp
        src/core/SharedHardwareContractAdapter.h
        src/core/SharedHardwareContractAdapter.cpp
        src/core/AutoTestPresetEngine.h
        src/core/AutoTestPresetEngine.cpp
        src/core/presets/FilterPresetCatalog.h
        src/core/presets/FilterPresetCatalog.cpp
        src/core/presets/ModulationPresetCatalog.h
        src/core/presets/ModulationPresetCatalog.cpp
        src/core/presets/DynamicsPresetCatalog.h
        src/core/presets/DynamicsPresetCatalog.cpp
        src/core/presets/TimeAcousticPresetCatalog.h
        src/core/presets/TimeAcousticPresetCatalog.cpp
        src/core/presets/CasioCzPresetCatalog.h
        src/core/presets/CasioCzPresetCatalog.cpp

        # Audio Engine & DSP
        src/audio/LabStimulusGenerator.h
        src/audio/LabStimulusGenerator.cpp
        src/audio/LabAudioReceiver.h
        src/audio/LabAudioReceiver.cpp
        src/audio/LabAudioEngine.h
        src/audio/LabAudioEngine.cpp
        
        # Mathematical & Statistical Processing
        src/math/LabAnalyticEngine.h
        src/math/LabAnalyticEngine.cpp
        src/math/analytics/FilterAnalytics.h
        src/math/analytics/FilterAnalytics.cpp
        src/math/analytics/EnvelopeAnalytics.h
        src/math/analytics/EnvelopeAnalytics.cpp
        src/math/analytics/DynamicsAnalytics.h
        src/math/analytics/DynamicsAnalytics.cpp
        src/math/analytics/ModulationAnalytics.h
        src/math/analytics/ModulationAnalytics.cpp
        src/math/FarinaDeconvolver.h
        src/math/FarinaDeconvolver.cpp
        src/math/IntermodulationAnalyzer.h
        src/math/IntermodulationAnalyzer.cpp
        src/math/WienerHammersteinFitter.h
        src/math/WienerHammersteinFitter.cpp
        src/math/NoiseFloorTracker.h
        src/math/NoiseFloorTracker.cpp
        src/math/SplineInterpolator2D.h
        src/math/LoopbackCalibrator.h
        src/math/LoopbackCalibrator.cpp
        src/math/PreScanSpectrumAnalyzer.h
        src/math/PreScanSpectrumAnalyzer.cpp
        src/math/ModulationMatrixProfile.h
        src/math/ModulationMatrixProfile.cpp
        src/math/ModulationEstimator.h
        src/math/ModulationEstimator.cpp
        src/dsp/LutEvaluatorSimd.h
        
        # Hardware Control Contracts & Validation
        src/hardware/HardwareController.h
        src/hardware/MidiCcController.h
        src/hardware/AiraSysExController.h
        src/hardware/FskAudioModem.h
        src/hardware/MidiIdentityDetector.h
        src/hardware/MidiIdentityDetector.cpp
        src/hardware/AudioMidiInterfaceDetector.h
        src/hardware/AudioLabMidiBackend.h
        src/hardware/AudioLabMidiBackend.cpp
        src/hardware/ManualAnalogueController.h
        src/hardware/MockHardwareController.h
        src/hardware/CasioCzVirtualController.h
        src/hardware/RoutingValidator.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/HardwareDrivers/CasioCzVirtualController.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/HardwareDrivers/CasioNibbleCodec.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/HardwareDrivers/FskAudioModem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/HardwareDrivers/JunoTapeModem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/HardwareDrivers/SysExCodec.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/HardwareDrivers/NRPNParser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/AudioComparator/AudioABComparator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/AudioComparator/AudioABComparator_Alignment.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/AudioComparator/AudioABComparator_Spectral.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/AudioComparator/AudioABVerdictEngine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/HardwareMidiDetect/JuceMidiHardwareBackend.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/WebView2Bridge/WebView2ResourceProvider.cpp
        
        # Plugin Hosting & Software Target (VST3, AU, LV2, ARA)
        src/core/plugins/PluginHostManager.h
        src/core/plugins/PluginHostManager.cpp
        src/core/plugins/PluginHardwareContractAdapter.h
        src/core/plugins/PluginHardwareContractAdapter.cpp
        src/gui/plugins/PluginWindowController.h
        src/gui/plugins/PluginWindowController.cpp
        src/gui/plugins/PluginScanDirectoriesModal.h
        src/gui/plugins/PluginScanDirectoriesModal.cpp
        # Measurement Framework (Phase 20.10)
        src/measurement/MeasurementContracts.h
        src/measurement/MeasurementSerialization.h
        src/measurement/MeasurementSerialization.cpp
        src/measurement/MeasurementStimulusCoordinator.h
        src/measurement/MeasurementStimulusCoordinator.cpp
        src/measurement/MeasurementCaptureCoordinator.h
        src/measurement/MeasurementCaptureCoordinator.cpp
        src/measurement/LoopbackCalibrator.h
        src/measurement/LoopbackCalibrator.cpp
        src/measurement/PhysicalLoopbackAdapter.h
        src/measurement/PhysicalLoopbackAdapter.cpp
        src/measurement/DexedVerticalCampaign.h
        src/measurement/DexedVerticalCampaign.cpp
        src/measurement/DexedParametricCampaignContracts.h
        src/measurement/DexedParametricCampaignCoordinator.h
        src/measurement/DexedParametricCampaignCoordinator.cpp
        src/measurement/ComplexEnvelopeContracts.h
        src/measurement/ComplexEnvelopeAnalyzer.h
        src/measurement/ComplexEnvelopeAnalyzer.cpp
        src/measurement/ComplexEnvelopeOrchestratorContracts.h
        src/measurement/ComplexEnvelopeCaptureSession.h
        src/measurement/ComplexEnvelopeCaptureSession.cpp
        src/measurement/TimingReferenceResolver.h
        src/measurement/TimingReferenceResolver.cpp
        src/measurement/ObservableComparisonEngine.h
        src/measurement/ObservableComparisonEngine.cpp
        src/measurement/ComplexEnvelopeOrchestrator.h
        src/measurement/ComplexEnvelopeOrchestrator.cpp
        src/measurement/ComplexEnvelopeExportContracts.h
        src/measurement/ComplexEnvelopeSvgRenderer.h
        src/measurement/ComplexEnvelopeSvgRenderer.cpp
        src/measurement/ComplexEnvelopeHtmlReportGenerator.h
        src/measurement/ComplexEnvelopeHtmlReportGenerator.cpp
        src/measurement/ComplexEnvelopeFairExporter.h
        src/measurement/ComplexEnvelopeFairExporter.cpp
        src/measurement/FineLatencyContracts.h
        src/measurement/FineLatencyAnalyzer.h
        src/measurement/FineLatencyAnalyzer.cpp
        src/measurement/MeasurementDspUtils.h
        src/measurement/MeasurementDspUtils.cpp
        src/measurement/AnalogDutCharacterizationContracts.h
        src/measurement/AnalogDutCharacterizer.h
        src/measurement/AnalogDutCharacterizer.cpp
        src/measurement/AnalogChainCompensationContracts.h
        src/measurement/AnalogChainReversibleCompensator.h
        src/measurement/AnalogChainReversibleCompensator.cpp
        src/measurement/MeasurementSessionContracts.h
        src/measurement/MeasurementSessionContracts.cpp
        src/measurement/CoordinatorStateMachine.h
        src/measurement/CoordinatorStateMachine.cpp
        src/measurement/MeasurementContainerExporter.h
        src/measurement/MeasurementContainerExporter.cpp
        src/measurement/MeasurementSvgGenerator.h
        src/measurement/MeasurementSvgGenerator.cpp
        src/measurement/MeasurementReportGenerator.h
        src/measurement/MeasurementReportGenerator.cpp
        src/measurement/MeasurementComparisonReportGenerator.h
        src/measurement/MeasurementComparisonReportGenerator.cpp
        src/measurement/adapters/EnvelopeMeasurementAdapter.h
        src/measurement/adapters/EnvelopeMeasurementAdapter.cpp
        src/measurement/adapters/FilterMeasurementAdapter.h
        src/measurement/adapters/FilterMeasurementAdapter.cpp
        src/measurement/adapters/DynamicsMeasurementAdapter.h
        src/measurement/adapters/DynamicsMeasurementAdapter.cpp
        src/measurement/adapters/ModulationMeasurementAdapter.h
        src/measurement/adapters/ModulationMeasurementAdapter.cpp
        # Casio CZ-101 Target Adapter (Phase 20.11.7)
        src/measurement/adapters/casio/CasioCz101SysExContracts.h
        src/measurement/adapters/casio/CasioCz101SysExFrameValidator.h
        src/measurement/adapters/casio/CasioCz101SysExFrameValidator.cpp
        src/measurement/adapters/casio/CasioCz101PatchDecoder.h
        src/measurement/adapters/casio/CasioCz101PatchDecoder.cpp
        src/measurement/adapters/casio/CasioCz101PatchEncoder.h
        src/measurement/adapters/casio/CasioCz101PatchEncoder.cpp
        src/measurement/adapters/casio/CasioCz101ObservableBinding.h
        src/measurement/adapters/casio/CasioCz101ObservableBinding.cpp
        src/measurement/adapters/casio/CasioCz101NativeStateProvider.h
        src/measurement/adapters/casio/CasioCz101NativeStateProvider.cpp
        # Measurement GUI (Phase 20.10.1 & Phase 20.11.1)
        src/gui/measurement/MeasurementViewModel.h
        src/gui/measurement/MeasurementViewModelLoader.h
        src/gui/measurement/MeasurementViewModelLoader.cpp
        src/gui/measurement/MeasurementTemporalCurveComponent.h
        src/gui/measurement/MeasurementTemporalCurveComponent.cpp
        src/gui/measurement/MeasurementFrequencyCurveComponent.h
        src/gui/measurement/MeasurementFrequencyCurveComponent.cpp
        src/gui/measurement/MeasurementAudioPlayerComponent.h
        src/gui/measurement/MeasurementAudioPlayerComponent.cpp
        src/gui/measurement/MeasurementViewerPanel.h
        src/gui/measurement/MeasurementViewerPanel.cpp
        src/gui/measurement/MeasurementComparisonSession.h
        src/gui/measurement/MeasurementComparisonSession.cpp
        src/gui/measurement/MeasurementContainerListPanel.h
        src/gui/measurement/MeasurementContainerListPanel.cpp
        src/gui/measurement/MeasurementDynamicsComparisonComponent.h
        src/gui/measurement/MeasurementDynamicsComparisonComponent.cpp
        src/gui/measurement/MeasurementStateEquivalenceCard.h
        src/gui/measurement/MeasurementStateEquivalenceCard.cpp
        src/gui/measurement/MeasurementComparisonPanel.h
        src/gui/measurement/MeasurementComparisonPanel.cpp
        
        # Digital Synth Profiler (Phase 20)
        src/synth/Sha256.h
        src/synth/SynthPresetState.h
        src/synth/MidiExcitationSequence.h
        src/synth/SynthObservation.h
        src/synth/ISynthTarget.h
        src/synth/ISynthTargetLifecycleAdapter.h
        src/synth/SynthTargetLifecycleAdapters.h
        src/synth/SynthTargetLifecycleAdapters.cpp
        src/synth/OutOfProcessVst3LifecycleAdapter.h
        src/synth/OutOfProcessVst3LifecycleAdapter.cpp
        src/synth/SyntheticSynthFixture.h
        src/synth/SyntheticSynthTarget.h
        src/synth/SynthPitchEstimator.h
        src/synth/SynthPitchEstimator.cpp

        # IPC & Out-Of-Process Worker (Phase 20.8.3)
        src/ipc/IpcProtocol.h
        src/ipc/Win32NamedPipe.h
        src/ipc/Win32NamedPipe.cpp
        src/ipc/WorkerProcessHost.h
        src/ipc/WorkerProcessHost.cpp
        src/synth/SynthEnvelopeAnalyzer.h
        src/synth/SynthEnvelopeAnalyzer.cpp
        src/synth/MidiAudioSynchronizer.h
        src/synth/MidiAudioSynchronizer.cpp
        src/synth/DigitalSynthMvpProfiler.h
        src/synth/DigitalSynthMvpProfiler.cpp
        src/synth/TargetAuditor.h
        src/synth/TargetAuditor.cpp
        src/synth/TargetContract.h
        src/synth/ExperimentPlan.h
        src/synth/ExperimentRecipe.h
        src/synth/AcousticObserver.h
        src/synth/AcousticObserver.cpp
        src/synth/TargetEventDispatcher.h
        src/synth/TargetEventDispatcher.cpp
        src/synth/ParameterExcitationEngine.h
        src/synth/ParameterExcitationEngine.cpp
        src/synth/ExternalPluginTypes.h
        src/synth/SysExContracts.h
        src/synth/TargetContractDiscovery.h
        src/synth/TargetContractDiscovery.cpp
        src/synth/ExternalPluginFixture.h
        src/synth/ExternalPluginFixture.cpp
        src/synth/ModelEvaluationTypes.h
        src/synth/ModelEvaluationBuilder.h
        src/synth/ModelEvaluationBuilder.cpp
        src/synth/AdaptiveExperimentPlanner.h
        src/synth/AdaptiveExperimentPlanner.cpp

        # Session Domain Controller & Contracts (Phase 16 / UX Simplification)
        src/gui/session/ProfilingSessionContracts.h
        src/gui/session/ProfilingSessionCoordinator.h
        src/gui/session/ProfilingSessionCoordinator.cpp
        src/gui/session/ProfilingSessionController.h
        src/gui/session/ProfilingSessionController.cpp

        # GUI & Interactive Visualizers (SoundID Architecture)
        src/gui/SoundIdTheme.h
        src/gui/SoundIdCurvePlotter.h
        src/gui/SoundIdCurvePlotter.cpp
        src/gui/SoundIdMeterStrip.h
        src/gui/SoundIdMeterStrip.cpp
        src/gui/SoundIdSuiteList.h
        src/gui/SoundIdSuiteList.cpp
        src/gui/suite/SuiteDataModels.h
        src/gui/suite/SuiteQueueModelManager.h
        src/gui/suite/SuiteQueueModelManager.cpp
        src/gui/suite/SuiteListEventHandler.h
        src/gui/suite/SuiteListEventHandler.cpp
        src/gui/suite/SuiteIcons.h
        src/gui/suite/SuiteRowLayout.h
        src/gui/suite/SuiteRowLayout.cpp
        src/gui/suite/SuiteRowRenderer.h
        src/gui/suite/SuiteRowRenderer.cpp
        src/gui/CenterSplitterBar.h
        src/gui/TopHeaderWidgets.h
        src/gui/MainContentComponent.h
        src/gui/MainContentComponent.cpp
        src/gui/LabApplication.h
        src/gui/SlideInDrawer.h
        src/gui/SlideInDrawer.cpp
        src/gui/drawers/AssetLocator.h
        src/gui/drawers/DrawerDataModels.h
        src/gui/drawers/DrawerFileSessionTab.h
        src/gui/drawers/DrawerFileSessionTab.cpp
        src/gui/drawers/DrawerHardwareTab.h
        src/gui/drawers/DrawerHardwareTab.cpp
        src/gui/drawers/DrawerSetupTab.h
        src/gui/drawers/DrawerSetupTab.cpp
        src/gui/InfoDrawer.h
        src/gui/InfoDrawer.cpp
        src/gui/AboutModalDialog.h
        src/gui/AboutModalDialog.cpp
        src/gui/LoopbackCalibrationModal.h
        src/gui/LoopbackCalibrationModal.cpp
        src/gui/HardwareWiringDiagramComponent.h
        src/gui/HardwareWiringDiagramComponent.cpp
        src/gui/HardwareDeviceDisplayCardComponent.h
        src/gui/HardwareDeviceDisplayCardComponent.cpp
        src/gui/HardwareRoutingPanel.h
        src/gui/HardwareRoutingPanel.cpp
        src/gui/NativeCalibrationPanel.h
        src/gui/NativeCalibrationPanel.cpp
        src/gui/TestConfigModal.h
        src/gui/TestConfigModal.cpp
        src/gui/MatrixResolutionTableComponent.h
        src/gui/MatrixResolutionTableComponent.cpp
        src/gui/TestPlanEstimationCardComponent.h
        src/gui/TestPlanEstimationCardComponent.cpp
        src/gui/TestEditorPanel.h
        src/gui/TestEditorPanel.cpp
        src/gui/HardwareControlRenderer.h
        src/gui/HardwareControlRenderer.cpp
        src/gui/SoundIdSplashScreen.h
        src/gui/HardwareSelectorPill.h
        src/gui/WorkflowStepperBar.h
        src/gui/soundid/SoundIdSidebarStepper.h
        src/gui/soundid/SoundIdSidebarStepper.cpp
        src/gui/soundid/SoundIdHardwareCatalogSelector.h
        src/gui/soundid/SoundIdHardwareCatalogSelector.cpp
        src/gui/soundid/SoundIdTopHeaderStrip.h
        src/gui/soundid/SoundIdTopHeaderStrip.cpp
        src/gui/soundid/SoundIdTargetView.h
        src/gui/soundid/SoundIdTargetView.cpp
        src/gui/soundid/SoundIdProfilingRunView.h
        src/gui/soundid/SoundIdProfilingRunView.cpp
        src/gui/soundid/SoundIdResultsSummaryView.h
        src/gui/soundid/SoundIdResultsSummaryView.cpp
        src/gui/soundid/SoundIdGuidedWorkflowContainer.h
        src/gui/soundid/SoundIdGuidedWorkflowContainer.cpp
        src/gui/ScopeWebFloatingWindow.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/StudioTopology/StudioTopologyResourceProvider.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/StudioTopology/StudioTopologyResourceProvider.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/StudioTopology/JuceStudioTopologyComponent.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/StudioTopology/StudioTopologyFloatingWindow.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/MidiKeyboard/MidiKeyboardResourceProvider.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/MidiKeyboard/MidiKeyboardResourceProvider.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/MidiKeyboard/JuceMidiKeyboardComponent.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/MidiKeyboard/MidiKeyboardFloatingWindow.h
        src/gui/MeasurementHealthPanel.h
        src/gui/OperatorCardsContainerComponent.h
        src/gui/OperatorCardsContainerComponent.cpp
        src/gui/OperatorStepModalDialog.h
        src/gui/OperatorStepModalDialog.cpp
        src/gui/ConfirmationModalDialog.h
        src/gui/SessionReportManager.h
        src/gui/SessionReportManager.cpp
        src/gui/MainHeaderController.h
        src/gui/MainHeaderController.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/StudioTopology/AudioMidiInterfaceDetector.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/StudioTopology/StudioTopologyController.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/StudioTopology/StudioTopologyController.cpp
        src/gui/SessionExecutionCoordinator.h
        src/gui/SessionExecutionCoordinator.cpp
        src/gui/controllers/SessionIoController.h
        src/gui/controllers/SessionIoController.cpp
        src/gui/controllers/WorkflowNavigationController.h
        src/gui/controllers/WorkflowNavigationController.cpp
        src/gui/AudioABVerificationModal.h
        src/gui/AudioABVerificationModal.cpp
        src/gui/Waterfall3DComponent.h
        src/gui/Waterfall3DComponent.cpp
        src/gui/PlotterModulationTableRenderer.h
        src/gui/PlotterModulationTableRenderer.cpp
        src/gui/PlotterHeatmapRenderer.h
        src/gui/PlotterHeatmapRenderer.cpp
        src/gui/PlotterFrequencyCurveRenderer.h
        src/gui/PlotterFrequencyCurveRenderer.cpp
        src/gui/ControlIcon.h
        
        # Exporting & Code Generation
        src/export/LutExporter.h
        src/export/LutExporter.cpp
        src/export/CertificationReportExporter.h
        src/export/CertificationReportExporter.cpp
        src/export/NamDatasetExporter.h
        src/export/NamDatasetExporter.cpp
        src/export/ModulationPresetExporter.h
        src/export/ModulationPresetExporter.cpp
        src/export/ModelExportNaming.h
        src/export/ModelExportNaming.cpp
        src/config/AutoUpdaterConfig.h
)

target_include_directories(ABDAudioLab
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDScope/Source
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode
        ${ara_sdk_SOURCE_DIR}
)

target_compile_definitions(ABDAudioLab
    PRIVATE
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_USE_CURL=0
        JUCE_ASIO=0
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=1
        JUCE_ALSA=0
        JUCE_JACK=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=$<IF:$<PLATFORM_ID:Darwin>,1,0>
        JUCE_PLUGINHOST_LV2=1
        JUCE_PLUGINHOST_ARA=1
)

target_link_libraries(ABDAudioLab
    PRIVATE
        juce::juce_core
        juce::juce_events
        juce::juce_data_structures
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_audio_utils
        juce::juce_gui_basics
        juce::juce_gui_extra
        nlohmann_json::nlohmann_json
        ABDScope::ABDScopeCore
        ABDShared::AutoUpdater
        ABDShared::HardwareMidiDetect
        ABDShared::StudioTopologyAssets
)

# ------------------------------------------------------------------------------
# Target: ABDAudioLab_Tests (Unit Test Suite with Catch2 / CTest)
# ------------------------------------------------------------------------------
enable_testing()

add_executable(ABDAudioLab_Tests
    src/tests/test_FarinaDeconvolver.cpp
    src/tests/test_SplineInterpolator2D.cpp
    src/tests/test_SessionSerializer.cpp
    src/tests/test_LabAnalyticEngine.cpp
    src/tests/test_CertificationReportExporter.cpp
    src/tests/test_WienerHammerstein.cpp
    src/tests/test_MidiIdentityDetector.cpp
    src/tests/test_NamDatasetExporter.cpp
    src/tests/test_AudioEngineBounds.cpp
    src/tests/test_LutEvaluatorSimd.cpp
    src/tests/test_AnalogLutFilterModule.cpp
    src/tests/test_HardwareLifecycle.cpp
    src/tests/test_HardwareProfileValidation.cpp
    src/tests/test_PlateauCollapseFilter.cpp
    src/tests/test_MidiDeviceHotplugMonitor.cpp
    src/tests/test_MidiSynthAutomation.cpp
    src/tests/test_OfflineReanalysis.cpp
    src/tests/test_PointRangePatching.cpp
    src/tests/test_MeasurementPresetRecipes.cpp
    src/tests/test_FskAudioModem.cpp
    src/tests/test_HardwareProtocols.cpp
    src/tests/test_AudioABComparator.cpp
    src/tests/test_OfficialLutGeneration.cpp
    src/tests/test_VoiceDispersionAndBBD.cpp
    src/tests/test_PhaseGroupDelay.cpp
    src/tests/test_LoopbackDiagnostics.cpp
    src/tests/test_LnlManifestExport.cpp
    src/tests/test_DeColoringCompensation.cpp
    src/tests/test_AudioMidiInterfaceDetector.cpp
    src/tests/test_SysexPresetGenerator.cpp
    src/tests/test_JsLutExport.cpp
    src/tests/test_ThermalDriftAnalyzer.cpp
    src/tests/test_AutoTestPresetEngine.cpp
    src/tests/test_PreScanSpectrumAnalyzer.cpp
    src/tests/test_ModulationEstimator.cpp
    src/tests/test_ModulationPresetExporter.cpp
    src/tests/test_ProfilingSequencerModulation.cpp
    src/tests/test_SoundIdCurvePlotterModulation.cpp
    src/tests/test_ProfilingArchitectureRefactor.cpp
    src/tests/test_PresetEngineDecomposition.cpp
    src/tests/test_SessionReportManager.cpp
    src/tests/test_MainHeaderController.cpp
    src/tests/test_SessionExecutionCoordinator.cpp
    src/tests/test_Waterfall3DComponent.cpp
    src/tests/test_PlotterModulationTableRenderer.cpp
    src/tests/test_PlotterRenderers.cpp
    src/tests/test_OperatorCardsContainerComponent.cpp
    src/tests/test_MatrixResolutionTableComponent.cpp
    src/tests/test_HardwareRoutingComponents.cpp
    src/tests/test_CasioCzVirtualController.cpp
    src/tests/test_CasioCzProfilingRecipes.cpp
    src/tests/test_CasioCzLiveScanSuite.cpp
    src/tests/test_SoundIdSuiteListRefactor.cpp
    src/tests/test_SoundIdSidebarStepper.cpp
    src/tests/test_SoundIdHardwareCatalogSelector.cpp
    src/tests/test_SoundIdThemeMode.cpp
    src/tests/test_PauseResume.cpp
    src/tests/test_HarmonicAndImdAnalysis.cpp
    src/tests/test_PluginHostManager.cpp
    src/tests/test_PluginCalibrationSafety.cpp
    src/tests/test_DigitalSynthMvpProfiler.cpp
    src/tests/test_TargetAuditor.cpp
    src/tests/test_ParameterExcitationEngine.cpp
    src/tests/test_Vst3Validation.cpp
    src/tests/test_DexedValidation.cpp
    src/tests/test_GuidedPluginLoad_Dexed.cpp
    src/tests/test_Vst3DexedRealHosting_T2.cpp
    src/tests/test_MidiAndSysExContracts_T3.cpp
    src/tests/test_CaptureAutomationAndFair_T3.cpp
    src/tests/test_LoopbackCalibration_T4.cpp
    src/tests/test_PhysicalLoopback_T4_2.cpp
    src/tests/test_MeasurementContracts.cpp
    src/tests/test_DynamicsAndModulationContracts.cpp
    src/tests/test_EnvelopeMeasurementAdapter.cpp
    src/tests/test_FilterMeasurementAdapter.cpp
    src/tests/test_DynamicsMeasurementAdapter.cpp
    src/tests/test_ModulationMeasurementAdapter.cpp
    src/tests/test_DynamicsAndModulationExport.cpp
    src/tests/test_MeasurementStimulusCapture.cpp
    src/tests/test_MeasurementAudioThroughCapture.cpp
    src/tests/test_MeasurementPersistenceAndReport.cpp
    src/tests/test_FilterMeasurementPersistenceAndReport.cpp
    src/tests/test_DexedEnvelopeMeasurement_T5.cpp
    src/tests/test_Phase20_11_VerticalDexedAndFair_T5.cpp
    src/tests/test_Phase20_11_1_InteractiveViewerComparison.cpp
    src/tests/test_Phase20_11_2_SessionRobustnessAndA11y.cpp
    src/tests/test_Phase20_11_3_DigitalExpansion.cpp
    src/tests/test_Phase20_11_4_AdvancedDigitalCampaigns.cpp
    src/tests/test_Phase20_11_5_PhysicalEstimatorsAndRelease.cpp
    src/tests/test_Phase20_11_7_ComplexEnvelopes_T1.cpp
    src/tests/test_Phase20_11_7_ComplexEnvelopes_T2.cpp
    src/tests/test_Phase20_11_7_ComplexEnvelopes_T3.cpp
    src/tests/test_Phase20_11_7_ComplexEnvelopes_T4.cpp
    src/tests/test_Phase20_11_7_ComplexEnvelopes_T5.cpp
    src/tests/test_Phase20_11_7_ComplexEnvelopes_T6.cpp
    src/tests/test_Phase20_12_FineLatency_T1.cpp
    src/tests/test_Phase20_12_AnalogDutCharacterization_T2.cpp
    src/tests/test_Phase20_12_ReversibleCompensation_T3.cpp
    src/tests/test_Integration_SmokeTest_SessionWorkflow.cpp
    src/tests/test_CoordinatorStateMachine.cpp
    src/tests/test_MeasurementDspUtils.cpp
    src/tests/test_MeasurementViewModelAndUI.cpp
    src/tests/test_FilterViewerPanelAndUI.cpp
    src/measurement/MeasurementSerialization.cpp
    src/measurement/MeasurementStimulusCoordinator.cpp
    src/measurement/MeasurementCaptureCoordinator.cpp
    src/measurement/LoopbackCalibrator.cpp
    src/measurement/PhysicalLoopbackAdapter.cpp
    src/measurement/DexedVerticalCampaign.cpp
    src/measurement/DexedParametricCampaignCoordinator.cpp
    src/measurement/ComplexEnvelopeAnalyzer.cpp
    src/measurement/ComplexEnvelopeCaptureSession.cpp
    src/measurement/TimingReferenceResolver.cpp
    src/measurement/ObservableComparisonEngine.cpp
    src/measurement/ComplexEnvelopeOrchestrator.cpp
    src/measurement/ComplexEnvelopeSvgRenderer.cpp
    src/measurement/ComplexEnvelopeHtmlReportGenerator.cpp
    src/measurement/ComplexEnvelopeFairExporter.cpp
    src/measurement/FineLatencyAnalyzer.cpp
    src/measurement/MeasurementDspUtils.cpp
    src/measurement/AnalogDutCharacterizer.cpp
    src/measurement/AnalogChainReversibleCompensator.cpp
    src/measurement/MeasurementSessionContracts.cpp
    src/measurement/CoordinatorStateMachine.cpp
    src/measurement/MeasurementContainerExporter.cpp
    src/measurement/MeasurementSvgGenerator.cpp
    src/measurement/MeasurementReportGenerator.cpp
    src/measurement/MeasurementComparisonReportGenerator.cpp
    src/measurement/adapters/EnvelopeMeasurementAdapter.cpp
    src/measurement/adapters/FilterMeasurementAdapter.cpp
    src/measurement/adapters/DynamicsMeasurementAdapter.cpp
    src/measurement/adapters/ModulationMeasurementAdapter.cpp
    src/measurement/adapters/casio/CasioCz101SysExFrameValidator.cpp
    src/measurement/adapters/casio/CasioCz101PatchDecoder.cpp
    src/measurement/adapters/casio/CasioCz101PatchEncoder.cpp
    src/measurement/adapters/casio/CasioCz101ObservableBinding.cpp
    src/measurement/adapters/casio/CasioCz101NativeStateProvider.cpp
    src/gui/measurement/MeasurementViewModelLoader.cpp
    src/gui/measurement/MeasurementTemporalCurveComponent.cpp
    src/gui/measurement/MeasurementFrequencyCurveComponent.cpp
    src/gui/measurement/MeasurementAudioPlayerComponent.cpp
    src/gui/measurement/MeasurementViewerPanel.cpp
    src/gui/measurement/MeasurementComparisonSession.cpp
    src/gui/measurement/MeasurementContainerListPanel.cpp
    src/gui/measurement/MeasurementDynamicsComparisonComponent.cpp
    src/gui/measurement/MeasurementStateEquivalenceCard.cpp
    src/gui/measurement/MeasurementComparisonPanel.cpp
    src/tests/test_ModelEvaluation.cpp
    src/tests/test_AdaptiveExperimentPlanner.cpp
    src/tests/test_ProfilingSessionController.cpp
    src/tests/test_ProfilingSessionCoordinator.cpp
    src/tests/test_SynthTargetLifecycleAdapter.cpp
    src/tests/test_SoundIdViews.cpp
    
    src/fixtures/reference_vst3/ReferenceSynthProcessor.cpp
    src/synth/SynthPitchEstimator.cpp
    src/synth/SynthEnvelopeAnalyzer.cpp
    src/synth/MidiAudioSynchronizer.cpp
    src/synth/DigitalSynthMvpProfiler.cpp
    src/synth/TargetAuditor.cpp
    src/synth/AcousticObserver.cpp
    src/synth/TargetEventDispatcher.cpp
    src/synth/ParameterExcitationEngine.cpp
    src/synth/TargetContractDiscovery.cpp
    src/synth/ExternalPluginFixture.cpp
    src/synth/SynthTargetLifecycleAdapters.cpp
    src/synth/ModelEvaluationBuilder.cpp
    src/synth/AdaptiveExperimentPlanner.cpp
    src/gui/session/ProfilingSessionCoordinator.cpp
    src/gui/session/ProfilingSessionController.cpp
    src/gui/soundid/SoundIdTopHeaderStrip.cpp
    src/gui/soundid/SoundIdTargetView.cpp
    src/gui/soundid/SoundIdProfilingRunView.cpp
    src/gui/soundid/SoundIdResultsSummaryView.cpp
    src/gui/soundid/SoundIdGuidedWorkflowContainer.cpp
    src/audio/LabAudioEngine.cpp
    src/audio/LabAudioReceiver.cpp
    src/audio/LabStimulusGenerator.cpp
    src/export/NamDatasetExporter.cpp
    src/export/LutExporter.cpp
    src/export/ModulationPresetExporter.cpp
    src/math/FarinaDeconvolver.cpp
    src/math/IntermodulationAnalyzer.cpp
    src/math/LabAnalyticEngine.cpp
    src/math/analytics/FilterAnalytics.cpp
    src/math/analytics/EnvelopeAnalytics.cpp
    src/math/analytics/DynamicsAnalytics.cpp
    src/math/analytics/ModulationAnalytics.cpp
    src/math/PreScanSpectrumAnalyzer.cpp
    src/math/ModulationMatrixProfile.cpp
    src/math/ModulationEstimator.cpp
    src/math/WienerHammersteinFitter.cpp
    src/math/NoiseFloorTracker.cpp
    src/math/LoopbackCalibrator.cpp
    src/hardware/MidiIdentityDetector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/HardwareDrivers/CasioCzVirtualController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/HardwareDrivers/CasioNibbleCodec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/HardwareDrivers/FskAudioModem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/HardwareDrivers/JunoTapeModem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/HardwareDrivers/SysExCodec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/HardwareDrivers/NRPNParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/AudioComparator/AudioABComparator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/AudioComparator/AudioABComparator_Alignment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/AudioComparator/AudioABComparator_Spectral.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/AudioComparator/AudioABVerdictEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/HardwareMidiDetect/JuceMidiHardwareBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/WebView2Bridge/WebView2ResourceProvider.cpp
    src/core/AutoTestPresetEngine.cpp
    src/core/presets/FilterPresetCatalog.cpp
    src/core/presets/ModulationPresetCatalog.cpp
    src/core/presets/DynamicsPresetCatalog.cpp
    src/core/presets/TimeAcousticPresetCatalog.cpp
    src/core/presets/CasioCzPresetCatalog.cpp
    src/core/SessionSerializer.cpp
    src/core/ProfilingSession.cpp
    src/core/ProfilingHardwareDispatcher.cpp
    src/core/ProfilingAudioCapture.cpp
    src/core/ProfilingSequencer.cpp
    src/core/SessionManager.cpp
    src/core/HardwareContractRegistry.cpp
    src/core/HardwareManager.cpp
    src/core/SharedHardwareContractAdapter.cpp
    src/core/plugins/PluginHostManager.cpp
    src/core/plugins/PluginHardwareContractAdapter.cpp
    src/gui/HardwareControlRenderer.cpp
    src/gui/SoundIdCurvePlotter.cpp
    src/gui/SoundIdMeterStrip.cpp
    src/gui/SoundIdSuiteList.cpp
    src/gui/suite/SuiteQueueModelManager.cpp
    src/gui/suite/SuiteListEventHandler.cpp
    src/gui/suite/SuiteRowLayout.cpp
    src/gui/suite/SuiteRowRenderer.cpp
    src/gui/MatrixResolutionTableComponent.cpp
    src/gui/TestPlanEstimationCardComponent.cpp
    src/gui/OperatorCardsContainerComponent.cpp
    src/gui/OperatorStepModalDialog.cpp
    src/gui/HardwareWiringDiagramComponent.cpp
    src/gui/HardwareDeviceDisplayCardComponent.cpp
    src/gui/HardwareRoutingPanel.cpp
    src/gui/AudioABVerificationModal.cpp
    src/gui/SessionReportManager.cpp
    src/gui/MainHeaderController.cpp
    src/gui/SessionExecutionCoordinator.cpp
    src/gui/Waterfall3DComponent.cpp
    src/gui/PlotterModulationTableRenderer.cpp
    src/gui/PlotterHeatmapRenderer.cpp
    src/gui/PlotterFrequencyCurveRenderer.cpp
    src/gui/soundid/SoundIdSidebarStepper.cpp
    src/gui/soundid/SoundIdHardwareCatalogSelector.cpp
    src/export/CertificationReportExporter.cpp
    src/export/ModelExportNaming.cpp
    src/ipc/Win32NamedPipe.cpp
    src/ipc/WorkerProcessHost.cpp
    src/synth/OutOfProcessVst3LifecycleAdapter.cpp
    src/core/ExperimentStorage.cpp
    src/core/LabDataDirectories.cpp
    src/core/GeneratedAcousticModel.cpp
    src/core/HoldoutSequence.cpp
    src/core/ModelHoldoutValidator.cpp
    src/core/ValidationUiSummary.cpp
    src/tests/test_WorkerProcessHost.cpp
    src/tests/test_OutOfProcessVst3LifecycleAdapter.cpp
    src/tests/test_ModelExportNaming.cpp
    src/tests/test_ExperimentStorage.cpp
    src/tests/test_LabDataDirectories.cpp
    src/tests/test_GeneratedAcousticModel.cpp
    src/tests/test_HoldoutSequence.cpp
    src/tests/test_ModelHoldoutValidation.cpp
    src/tests/test_CertificationReportExport.cpp
    src/tests/test_UiCoordinatorGovernance.cpp
)

# ------------------------------------------------------------------------------
# Target: ABDAudioLab_PluginWorker (Isolated Out-of-Process VST3 Worker)
# ------------------------------------------------------------------------------
add_executable(ABDAudioLab_PluginWorker
    src/worker/PluginWorkerMain.cpp
    src/ipc/Win32NamedPipe.cpp
    src/synth/ExternalPluginFixture.cpp
    src/synth/TargetContractDiscovery.cpp
)

target_include_directories(ABDAudioLab_PluginWorker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(ABDAudioLab_PluginWorker PRIVATE
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=$<IF:$<PLATFORM_ID:Darwin>,1,0>
)
target_link_libraries(ABDAudioLab_PluginWorker PRIVATE
    juce::juce_core
    juce::juce_events
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_audio_formats
    nlohmann_json::nlohmann_json
)
if(WIN32)
    target_link_libraries(ABDAudioLab_PluginWorker PRIVATE advapi32)
endif()

add_custom_command(TARGET ABDAudioLab_PluginWorker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/ABDAudioLab_artefacts/$<CONFIG>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:ABDAudioLab_PluginWorker>
        "${CMAKE_BINARY_DIR}/ABDAudioLab_artefacts/$<CONFIG>/$<TARGET_FILE_NAME:ABDAudioLab_PluginWorker>"
    COMMENT "Copying ABDAudioLab_PluginWorker alongside ABDAudioLab executable"
)

target_include_directories(ABDAudioLab_Tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDScope/Source
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode
)

target_compile_definitions(ABDAudioLab_Tests
    PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=$<IF:$<PLATFORM_ID:Darwin>,1,0>
)

target_link_libraries(ABDAudioLab_Tests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_core
        juce::juce_events
        juce::juce_data_structures
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
        nlohmann_json::nlohmann_json
        ABDScope::ABDScopeCore
        ABDShared::HardwareMidiDetect
)

include(CTest)
include(Catch)
catch_discover_tests(ABDAudioLab_Tests)

# ==============================================================================
# VST3 Validation Targets (Phase 20.3.B)
# ==============================================================================

juce_add_plugin(ReferenceSynth
    FORMATS VST3
    PLUGIN_MANUFACTURER_CODE "ABDS"
    PLUGIN_CODE "RefS"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PRODUCT_NAME "ReferenceSynth"
)

target_sources(ReferenceSynth PRIVATE
    src/fixtures/reference_vst3/ReferenceSynthProcessor.h
    src/fixtures/reference_vst3/ReferenceSynthProcessor.cpp
    src/fixtures/reference_vst3/ReferenceSynthGroundTruth.h
)

target_compile_definitions(ReferenceSynth PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ReferenceSynth PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

juce_add_console_app(ABDAudioLab_Vst3Validation
    PRODUCT_NAME "ABDAudioLab_Vst3Validation"
)

target_sources(ABDAudioLab_Vst3Validation PRIVATE
    src/apps/Vst3ValidationMain.cpp
    src/synth/ExternalPluginFixture.h
    src/synth/ExternalPluginFixture.cpp
    src/synth/TargetContractDiscovery.h
    src/synth/TargetContractDiscovery.cpp
    src/synth/TargetAuditor.cpp
    src/synth/AcousticObserver.cpp
    src/synth/TargetEventDispatcher.cpp
    src/synth/ParameterExcitationEngine.cpp
    src/synth/SynthPitchEstimator.cpp
    src/synth/SynthEnvelopeAnalyzer.cpp
    src/synth/MidiAudioSynchronizer.cpp
)

target_include_directories(ABDAudioLab_Vst3Validation PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(ABDAudioLab_Vst3Validation PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=$<IF:$<PLATFORM_ID:Darwin>,1,0>
)

target_link_libraries(ABDAudioLab_Vst3Validation PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_dsp
    nlohmann_json::nlohmann_json
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

