/**
 * @file AutoTestPresetEngine.h
 * @brief Intelligent test recommendation engine mapping synth sections, modular units, and effects to optimal DSP parameters.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "../audio/LabStimulusGenerator.h"
#include "HardwareContractRegistry.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <string>

namespace abdaudiolab::core
{

/**
 * @enum ComponentCategory
 * @brief High-level categories encompassing synth blocks, Eurorack modular, studio FX, and acoustic gear.
 */
enum class ComponentCategory
{
    OscillatorsAndGenerators,
    FiltersAndToneShapers,
    ModulatorsAndEnvelopes,
    DynamicsAndNonLinear,
    TimeAndSpatialEffects,
    EurorackAiraSubmodules,
    AcousticsAndTransducers
};

/**
 * @struct ComponentPresetRecommendation
 * @brief Complete parameter package to auto-configure test routines without operator guesswork.
 */
struct ComponentPresetRecommendation
{
    std::string typologyId;
    std::string displayName;
    ComponentCategory category { ComponentCategory::FiltersAndToneShapers };
    std::string badgeText; // "VCO", "VCF", "ENV", "LFO", "VCA", "SAT", "MOD", "DLY", "REV", "AIRA", "CAB", "CZ-DCW", "CZ-ENV", "CZ-DCO"

    // DSP Analysis & Routing Parameters
    std::string algorithmName;             // e.g. "Farina Log-Sine Sweep + Volterra H2..H5"
    audio::StimulusType stimulusType { audio::StimulusType::LogFarinaSweep };
    std::string captureMode { "FIXED_TIME" }; // "FIXED_TIME", "ADAPTIVE_ENVELOPE", "SYNCHRONOUS_SPECTRUM_SWEEP", etc.

    // Buffer & Sample Windows
    size_t recommendedSamples { 96000 };   // Total samples to acquire at 48kHz
    size_t recommendedFftSize { 16384 };    // FFT/LPC resolution

    // Timing
    float burstDurationSec { 2.0f };       // Duration of excitation
    float preRollMs { 150.0f };            // Pre-roll settling time
    float settlingWaitMs { 200.0f };       // Post-burst ring / settling time
    float silenceThresholdDb { -60.0f };   // Adaptive tail cutoff threshold

    // Hardware Guidance
    std::string stimulusDescription;
    std::string routingNotes;
    int recommendedSteps { 16 };           // Standard knob step resolution

    // Explicit Hardware Measurement Recipe (Setup Actions, Note Sequences, etc.)
    MeasurementPresetRecipe recipe;
};

/**
 * @class AutoTestPresetEngine
 * @brief Queryable repository of standard measurement recommendations.
 */
class AutoTestPresetEngine
{
public:
    static std::vector<ComponentPresetRecommendation> getAllPresets();
    static std::vector<ComponentPresetRecommendation> getPresetsForCategory(ComponentCategory cat);
    static ComponentPresetRecommendation getPresetById(const std::string& typologyId);
    static ComponentPresetRecommendation getDefaultFilterPreset();
};

} // namespace abdaudiolab::core
