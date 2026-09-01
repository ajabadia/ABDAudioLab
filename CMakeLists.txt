cmake_minimum_required(VERSION 3.22)

project(ABDAudioLab VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable MSVC optimizations & UTF-8
if(MSVC)
    add_compile_options(/W4 /utf-8 /permissive- /Zc:__cplusplus)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

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
# Target: ABDAudioLab (Standalone Executable)
# ------------------------------------------------------------------------------
juce_add_gui_app(ABDAudioLab
    PRODUCT_NAME "ABDAudioLab"
    VERSION "1.0.0"
    COMPANY_NAME "ABDSynths"
    BUNDLE_ID "com.abdsynths.abdaudiolab"
)

# Source files
target_sources(ABDAudioLab
    PRIVATE
        src/main.cpp
        
        # Core Architecture
        src/core/ProfilingSession.h
        src/core/ProfilingSession.cpp
        src/core/ProfilingSequencer.h
        src/core/ProfilingSequencer.cpp
        
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
        
        # Hardware Control Contracts
        src/hardware/HardwareController.h
        src/hardware/MidiCcController.h
        src/hardware/AiraSysExController.h
        src/hardware/ManualAnalogueController.h
        src/hardware/MockHardwareController.h
        
        # Exporting & Code Generation
        src/export/LutExporter.h
        src/export/LutExporter.cpp
)

target_include_directories(ABDAudioLab
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(ABDAudioLab
    PRIVATE
        JUCE_WEB_BROWSER=0
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
)
