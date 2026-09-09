#include "FilterPresetCatalog.h"

namespace abdaudiolab {
namespace core {
namespace presets {

std::vector<ComponentPresetRecommendation> FilterPresetCatalog::getPresets()
{
    std::vector<ComponentPresetRecommendation> list;

    // =========================================================================
    // Filtros & Tone Shapers Clásicos (4 presets)
    // =========================================================================
    {
        ComponentPresetRecommendation p;
        p.typologyId = "vcf_ladder_ota_svf";
        p.displayName = "VCF Filter (Ladder, OTA, Sallen-Key, SVF, Diode)";
        p.category = ComponentCategory::FiltersAndToneShapers;
        p.badgeText = "VCF";
        p.algorithmName = "Farina Log-Sine Sweep + Volterra Distortion (H2..H5) + Q Resonance";
        p.stimulusType = audio::StimulusType::LogFarinaSweep;
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 96000;
        p.recommendedFftSize = 4096;
        p.burstDurationSec = 2.0f;
        p.preRollMs = 150.0f;
        p.settlingWaitMs = 300.0f;
        p.recommendedSteps = 16;
        p.stimulusDescription = "Logarithmic Sine Sweep 10 Hz - 24 kHz";
        p.routingNotes = "Interface DAC Out -> VCF Audio In -> Interface ADC In";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "filter_formant_vocal";
        p.displayName = "Formant / Vocal Resonator Filter";
        p.category = ComponentCategory::FiltersAndToneShapers;
        p.badgeText = "FRM";
        p.algorithmName = "Farina Sweep + Linear Predictive Coding (LPC order 20) for F1..F3";
        p.stimulusType = audio::StimulusType::LogFarinaSweep;
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 96000;
        p.recommendedFftSize = 4096;
        p.burstDurationSec = 2.0f;
        p.preRollMs = 100.0f;
        p.settlingWaitMs = 250.0f;
        p.recommendedSteps = 16;
        p.stimulusDescription = "Log Sine Sweep (20 Hz - 16 kHz)";
        p.routingNotes = "Direct Loopback via VCF Formant In/Out.";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "eq_parametric_graphic";
        p.displayName = "Parametric / Graphic Equalizer";
        p.category = ComponentCategory::FiltersAndToneShapers;
        p.badgeText = "EQ";
        p.algorithmName = "Farina Fast Sweep (1.0s) + Parametric Bell / Shelf Fit";
        p.stimulusType = audio::StimulusType::LogFarinaSweep;
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 48000;
        p.recommendedFftSize = 4096;
        p.burstDurationSec = 1.0f;
        p.preRollMs = 50.0f;
        p.settlingWaitMs = 50.0f;
        p.recommendedSteps = 11; // -12dB to +12dB
        p.stimulusDescription = "Fast Log-Sine Sweep (1.0s)";
        p.routingNotes = "Stereo/Mono Loopback across EQ band.";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "filter_comb";
        p.displayName = "Comb Filter (Positive / Negative Feedback)";
        p.category = ComponentCategory::FiltersAndToneShapers;
        p.badgeText = "CMB";
        p.algorithmName = "Full Cepstrum (IFFT(log|FFT|)) Quefrency Peak + Notch Depth";
        p.stimulusType = audio::StimulusType::LogFarinaSweep;
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 57600;
        p.recommendedFftSize = 16384;
        p.burstDurationSec = 1.2f;
        p.preRollMs = 50.0f;
        p.settlingWaitMs = 500.0f;
        p.recommendedSteps = 16;
        p.stimulusDescription = "Log Sweep or Dirac Impulse with extended tail";
        p.routingNotes = "In/Out Loopback across Comb processor.";
        list.push_back(p);
    }

    // =========================================================================
    // Roland AIRA Eurorack Modular Submodules: Filtros (01..06)
    // =========================================================================
    const std::vector<std::pair<std::string, std::string>> airaFilterModules = {
        { "aira_01_filter_24db", "01. Ladder Low-Pass Filter (-24dB/Oct)" },
        { "aira_02_filter_18db", "02. Resonant Filter (-18dB/Oct)" },
        { "aira_03_filter_12db", "03. High-Pass Filter (-12dB/Oct)" },
        { "aira_04_bpf", "04. Band-Pass Filter (BPF)" },
        { "aira_05_formant_filter", "05. Formant Vocal Filter" },
        { "aira_06_svf", "06. State Variable Filter (SVF Multi-Mode)" }
    };

    for (const auto& m : airaFilterModules)
    {
        ComponentPresetRecommendation p;
        p.typologyId = m.first;
        p.displayName = "Roland AIRA: " + m.second;
        p.category = ComponentCategory::EurorackAiraSubmodules;
        p.badgeText = "AIRA";
        p.stimulusType = audio::StimulusType::LogFarinaSweep;
        p.burstDurationSec = 2.0f;
        p.recommendedSamples = 96000;
        p.recommendedFftSize = 4096;
        p.preRollMs = 150.0f;
        p.settlingWaitMs = 150.0f;
        p.recommendedSteps = 16;
        p.stimulusDescription = "Submodule calibrated test signal";
        p.routingNotes = "Audio In L/R -> AIRA Submodule -> Audio Out L/R";
        p.algorithmName = "Farina Sweep + Q Resonance Deconvolution";
        list.push_back(p);
    }

    return list;
}

} // namespace presets
} // namespace core
} // namespace abdaudiolab
