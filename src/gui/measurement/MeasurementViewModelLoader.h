/**
 * @file MeasurementViewModelLoader.h
 * @brief Loader and verifier for MeasurementViewModel from FAIR containers.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "MeasurementViewModel.h"
#include <juce_core/juce_core.h>

namespace abdaudiolab::gui::measurement
{

class MeasurementViewModelLoader
{
public:
    /**
     * @brief Safely loads a MeasurementViewModel from a persisted FAIR container directory.
     * 
     * Verifies path confinement (rejects path traversal outside container), parses JSON contracts,
     * checks initial integrity with ExperimentFolderReader, and populates the view model.
     * 
     * @param containerDir The root directory of the measurement container.
     * @param outModel The populated view model.
     * @param outError Diagnostic error message if loading fails.
     * @return true if loaded successfully.
     */
    static bool loadFromContainer(const juce::File& containerDir,
                                  MeasurementViewModel& outModel,
                                  juce::String& outError);

    /**
     * @brief Re-verifies all artifacts listed in manifest.json against disk.
     * 
     * Designed to be called safely on background worker threads without blocking GUI.
     * 
     * @param containerDir The root directory of the measurement container.
     * @param outDiagnostic Detailed diagnostic if verification fails.
     * @return true if all artifacts match their declared SHA-256 hashes.
     */
    static bool verifyContainerIntegrity(const juce::File& containerDir,
                                         juce::String& outDiagnostic);

    /**
     * @brief Verifies that a specific audio file matches its expected SHA-256 hash.
     * 
     * Performs an on-demand check prior to playback.
     * 
     * @param audioFile The WAV audio file to inspect.
     * @param expectedSha256 Expected hex SHA-256 hash.
     * @return true if file exists and hash matches.
     */
    static bool verifyAudioFileSha256(const juce::File& audioFile,
                                      const juce::String& expectedSha256);
};

} // namespace abdaudiolab::gui::measurement
