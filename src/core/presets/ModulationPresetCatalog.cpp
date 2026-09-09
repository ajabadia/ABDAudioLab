#include "ModulationPresetCatalog.h"

namespace abdaudiolab {
namespace core {
namespace presets {

std::vector<ComponentPresetRecommendation> ModulationPresetCatalog::getPresets()
{
    std::vector<ComponentPresetRecommendation> list;

    // =========================================================================
    // Moduladores & Envolventes Clásicos (2 presets)
    // =========================================================================
    {
        ComponentPresetRecommendation p;
        p.typologyId = "env_adsr_transient";
        p.displayName = "Envelope Generator (ADSR / AR / Function Gen)";
        p.category = ComponentCategory::ModulatorsAndEnvelopes;
        p.badgeText = "ENV";
        p.algorithmName = "Savitzky-Golay 2nd Derivative + Non-Linear Exponential Fit";
        p.stimulusType = audio::StimulusType::SyncPulses3;
        p.captureMode = "ADAPTIVE_ENVELOPE";
        p.recommendedSamples = 96000;
        p.recommendedFftSize = 2048;
        p.burstDurationSec = 2.0f;
        p.preRollMs = 100.0f;
        p.settlingWaitMs = 100.0f;
        p.silenceThresholdDb = -60.0f;
        p.recommendedSteps = 8;
        p.stimulusDescription = "Dynamic Gate Pulses with variable hold";
        p.routingNotes = "Audio signal fed through VCA modulated by target envelope.";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "mod_lfo_sample_hold";
        p.displayName = "LFO & Sample / Hold Circuit";
        p.category = ComponentCategory::ModulatorsAndEnvelopes;
        p.badgeText = "LFO";
        p.algorithmName = "Slow-Step Autocorrelation (ACF) + Chi-Square Step Uniformity";
        p.stimulusType = audio::StimulusType::SineWave1kHz; // Carrier tone modulated by LFO
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 288000; // 6.0s @ 48kHz
        p.recommendedFftSize = 8192;
        p.burstDurationSec = 6.0f;
        p.preRollMs = 200.0f;
        p.settlingWaitMs = 0.0f;
        p.recommendedSteps = 8;
        p.stimulusDescription = "Carrier Tone 1 kHz modulated via VCA / VCF";
        p.routingNotes = "Record carrier envelope AM/FM variations over time.";
        list.push_back(p);
    }

    // =========================================================================
    // Roland AIRA Eurorack Modular Submodules: Modulación (12..15, 24, 25)
    // =========================================================================
    {
        ComponentPresetRecommendation p;
        p.typologyId = "aira_12_adsr";
        p.displayName = "Roland AIRA: 12. ADSR Envelope Generator";
        p.category = ComponentCategory::EurorackAiraSubmodules;
        p.badgeText = "AIRA";
        p.stimulusType = audio::StimulusType::SyncPulses3;
        p.captureMode = "ADAPTIVE_ENVELOPE";
        p.silenceThresholdDb = -60.0f;
        p.algorithmName = "Savitzky-Golay 2nd Derivative Transient Segmentation";
        p.burstDurationSec = 2.0f;
        p.recommendedSamples = 96000;
        p.recommendedFftSize = 4096;
        p.preRollMs = 150.0f;
        p.settlingWaitMs = 150.0f;
        p.recommendedSteps = 16;
        p.stimulusDescription = "Submodule calibrated test signal";
        p.routingNotes = "Audio In L/R -> AIRA Submodule -> Audio Out L/R";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "aira_13_ar_envelope";
        p.displayName = "Roland AIRA: 13. AR Attack-Release Envelope";
        p.category = ComponentCategory::EurorackAiraSubmodules;
        p.badgeText = "AIRA";
        p.stimulusType = audio::StimulusType::SyncPulses3;
        p.captureMode = "ADAPTIVE_ENVELOPE";
        p.silenceThresholdDb = -60.0f;
        p.algorithmName = "Savitzky-Golay 2nd Derivative Transient Segmentation";
        p.burstDurationSec = 2.0f;
        p.recommendedSamples = 96000;
        p.recommendedFftSize = 4096;
        p.preRollMs = 150.0f;
        p.settlingWaitMs = 150.0f;
        p.recommendedSteps = 16;
        p.stimulusDescription = "Submodule calibrated test signal";
        p.routingNotes = "Audio In L/R -> AIRA Submodule -> Audio Out L/R";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "aira_14_multi_lfo";
        p.displayName = "Roland AIRA: 14. Multi-Wave LFO Modulator";
        p.category = ComponentCategory::EurorackAiraSubmodules;
        p.badgeText = "AIRA";
        p.stimulusType = audio::StimulusType::SineWave1kHz;
        p.algorithmName = "Low-Frequency Autocorrelation (ACF) + Duty Cycle";
        p.burstDurationSec = 4.0f;
        p.recommendedSamples = 192000;
        p.recommendedFftSize = 4096;
        p.preRollMs = 150.0f;
        p.settlingWaitMs = 150.0f;
        p.recommendedSteps = 16;
        p.stimulusDescription = "Submodule calibrated test signal";
        p.routingNotes = "Audio In L/R -> AIRA Submodule -> Audio Out L/R";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "aira_15_dual_lfo";
        p.displayName = "Roland AIRA: 15. Dual Cross-Modulated LFO";
        p.category = ComponentCategory::EurorackAiraSubmodules;
        p.badgeText = "AIRA";
        p.stimulusType = audio::StimulusType::SineWave1kHz;
        p.algorithmName = "Low-Frequency Autocorrelation (ACF) + Duty Cycle";
        p.burstDurationSec = 4.0f;
        p.recommendedSamples = 192000;
        p.recommendedFftSize = 4096;
        p.preRollMs = 150.0f;
        p.settlingWaitMs = 150.0f;
        p.recommendedSteps = 16;
        p.stimulusDescription = "Submodule calibrated test signal";
        p.routingNotes = "Audio In L/R -> AIRA Submodule -> Audio Out L/R";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "aira_24_sample_hold";
        p.displayName = "Roland AIRA: 24. Sample & Hold (S&H) Circuit";
        p.category = ComponentCategory::EurorackAiraSubmodules;
        p.badgeText = "AIRA";
        p.stimulusType = audio::StimulusType::WhiteNoise;
        p.algorithmName = "Discrete S&H Amplitude Distribution Test";
        p.burstDurationSec = 3.0f;
        p.recommendedSamples = 144000;
        p.recommendedFftSize = 4096;
        p.preRollMs = 150.0f;
        p.settlingWaitMs = 150.0f;
        p.recommendedSteps = 16;
        p.stimulusDescription = "Submodule calibrated test signal";
        p.routingNotes = "Audio In L/R -> AIRA Submodule -> Audio Out L/R";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "aira_25_slew_limiter";
        p.displayName = "Roland AIRA: 25. Slew Limiter / Portamento Glide";
        p.category = ComponentCategory::EurorackAiraSubmodules;
        p.badgeText = "AIRA";
        p.stimulusType = audio::StimulusType::SyncPulses3;
        p.algorithmName = "Step Transient dV/dt Rise and Fall Regression";
        p.burstDurationSec = 1.0f;
        p.recommendedSamples = 48000;
        p.recommendedFftSize = 4096;
        p.preRollMs = 150.0f;
        p.settlingWaitMs = 150.0f;
        p.recommendedSteps = 16;
        p.stimulusDescription = "Submodule calibrated test signal";
        p.routingNotes = "Audio In L/R -> AIRA Submodule -> Audio Out L/R";
        list.push_back(p);
    }

    return list;
}

} // namespace presets
} // namespace core
} // namespace abdaudiolab
