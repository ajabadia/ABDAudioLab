cmake_minimum_required(VERSION 3.22)

project(ABDAudioLab VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable MSVC optimizations & UTF-8
if(MSVC)
    add_compile_options(/W4 /utf-8 /permissive- /Zc:__cplusplus)
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
        src/core/ProfilingSequencer.h
        src/core/ProfilingSequencer.cpp
        src/core/HardwareContractRegistry.h
        src/core/HardwareContractRegistry.cpp
        src/core/SessionSerializer.h
        src/core/SessionSerializer.cpp
        src/core/SessionManager.h
        src/core/SessionManager.cpp
        src/core/HardwareManager.h
        src/core/HardwareManager.cpp

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
        src/math/FarinaDeconvolver.h
        src/math/FarinaDeconvolver.cpp
        src/math/WienerHammersteinFitter.h
        src/math/WienerHammersteinFitter.cpp
        src/math/NoiseFloorTracker.h
        src/math/NoiseFloorTracker.cpp
        src/math/SplineInterpolator2D.h
        src/math/LoopbackCalibrator.h
        src/math/LoopbackCalibrator.cpp
        
        # Hardware Control Contracts & Validation
        src/hardware/HardwareController.h
        src/hardware/MidiCcController.h
        src/hardware/AiraSysExController.h
        src/hardware/MidiIdentityDetector.h
        src/hardware/MidiIdentityDetector.cpp
        src/hardware/ManualAnalogueController.h
        src/hardware/MockHardwareController.h
        src/hardware/RoutingValidator.h
        
        # GUI & Interactive Visualizers (SoundID Architecture)
        src/gui/SoundIdTheme.h
        src/gui/SoundIdCurvePlotter.h
        src/gui/SoundIdCurvePlotter.cpp
        src/gui/SoundIdMeterStrip.h
        src/gui/SoundIdMeterStrip.cpp
        src/gui/SoundIdSuiteList.h
        src/gui/SoundIdSuiteList.cpp
        src/gui/MainContentComponent.h
        src/gui/LabApplication.h
        src/gui/SlideInDrawer.h
        src/gui/SlideInDrawer.cpp
        src/gui/InfoDrawer.h
        src/gui/InfoDrawer.cpp
        src/gui/AboutModalDialog.h
        src/gui/AboutModalDialog.cpp
        src/gui/LoopbackCalibrationModal.h
        src/gui/LoopbackCalibrationModal.cpp
        src/gui/TestConfigModal.h
        src/gui/TestConfigModal.cpp
        src/gui/TestEditorPanel.h
        src/gui/TestEditorPanel.cpp
        src/gui/HardwareControlRenderer.h
        src/gui/HardwareControlRenderer.cpp
        src/gui/SoundIdSplashScreen.h
        src/gui/HardwareSelectorPill.h
        src/gui/LiveSpectrumAnalyzer.h
        src/gui/MainContentComponent.h
        src/gui/LabApplication.h
        src/gui/ScopeWebFloatingWindow.h
        src/gui/MeasurementHealthPanel.h
        src/gui/OperatorStepModalDialog.h
        src/gui/ConfirmationModalDialog.h
        src/gui/ControlIcon.h
        
        # Exporting & Code Generation
        src/export/LutExporter.h
        src/export/LutExporter.cpp
        src/export/CertificationReportExporter.h
        src/export/CertificationReportExporter.cpp
        src/export/NamDatasetExporter.h
        src/export/NamDatasetExporter.cpp
        src/config/AutoUpdaterConfig.h
)

target_include_directories(ABDAudioLab
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDScope/Source
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
    
    src/audio/LabAudioEngine.cpp
    src/audio/LabAudioReceiver.cpp
    src/audio/LabStimulusGenerator.cpp
    src/export/NamDatasetExporter.cpp
    src/math/FarinaDeconvolver.cpp
    src/math/LabAnalyticEngine.cpp
    src/math/WienerHammersteinFitter.cpp
    src/hardware/MidiIdentityDetector.cpp
    src/core/SessionSerializer.cpp
    src/core/ProfilingSession.cpp
    src/core/SessionManager.cpp
    src/core/HardwareContractRegistry.cpp
    src/gui/HardwareControlRenderer.cpp
    src/export/CertificationReportExporter.cpp
)

target_include_directories(ABDAudioLab_Tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../ABDScope/Source
)

target_compile_definitions(ABDAudioLab_Tests
    PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
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
)

include(CTest)
include(Catch)
catch_discover_tests(ABDAudioLab_Tests)
