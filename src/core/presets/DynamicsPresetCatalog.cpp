#include "DynamicsPresetCatalog.h"

namespace abdaudiolab {
namespace core {
namespace presets {

std::vector<ComponentPresetRecommendation> DynamicsPresetCatalog::getPresets()
{
    std::vector<ComponentPresetRecommendation> list;

    // =========================================================================
    // Dinámica, Saturación & No-Lineal Clásicos (4 presets)
    // =========================================================================
    {
        ComponentPresetRecommendation p;
        p.typologyId = "dyn_vca_attenuverter";
        p.displayName = "VCA & Attenuverter (Lin/Exp Response & Bleed)";
        p.category = ComponentCategory::DynamicsAndNonLinear;
        p.badgeText = "VCA";
        p.algorithmName = "Stepped RMS Gain vs CV + Zero-Gate Bleed-Through Floor";
        p.stimulusType = audio::StimulusType::SineWave1kHz;
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 131072;
        p.recommendedFftSize = 4096;
        p.burstDurationSec = 0.1f; // 100ms per step
        p.preRollMs = 25.0f;
        p.settlingWaitMs = 25.0f;
        p.recommendedSteps = 32;
        p.stimulusDescription = "Constant 1 kHz Sine Tone";
        p.routingNotes = "DAC 1kHz -> VCA Audio In -> ADC In. Step CV control.";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "sat_waveshaper_fuzz_tube";
        p.displayName = "Saturator / Fuzz / Tube Preamp / Wavefolder";
        p.category = ComponentCategory::DynamicsAndNonLinear;
        p.badgeText = "SAT";
        p.algorithmName = "Multi-level WaveShaper + Wiener-Hammerstein Adam LNL Fit";
        p.stimulusType = audio::StimulusType::AmplitudeRamp;
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 65536;
        p.recommendedFftSize = 4096;
        p.burstDurationSec = 1.5f;
        p.preRollMs = 30.0f;
        p.settlingWaitMs = 30.0f;
        p.recommendedSteps = 32;
        p.stimulusDescription = "Multi-level ascending amplitude ramp";
        p.routingNotes = "Line Out -> Drive In -> Line In.";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "dyn_compressor_limiter";
        p.displayName = "Compressor / Limiter / Gate Dynamics";
        p.category = ComponentCategory::DynamicsAndNonLinear;
        p.badgeText = "CMP";
        p.algorithmName = "5ms RMS Trapezoid + IEC Attack/Release + Threshold/Ratio Fit";
        p.stimulusType = audio::StimulusType::AmplitudeRamp;
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 120000;
        p.recommendedFftSize = 4096;
        p.burstDurationSec = 2.5f;
        p.preRollMs = 100.0f;
        p.settlingWaitMs = 100.0f;
        p.recommendedSteps = 16;
        p.stimulusDescription = "Trapezoidal ascending/descending level burst";
        p.routingNotes = "Direct loopback across compressor.";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "mod_ring_modulator";
        p.displayName = "Ring Modulator & 4-Quadrant Multiplier";
        p.category = ComponentCategory::DynamicsAndNonLinear;
        p.badgeText = "RNG";
        p.algorithmName = "Orthogonal Two-Tone (1kHz & 220Hz) + Carrier Rejection (dB)";
        p.stimulusType = audio::StimulusType::SineWave1kHz;
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 32768;
        p.recommendedFftSize = 16384;
        p.burstDurationSec = 1.0f;
        p.preRollMs = 50.0f;
        p.settlingWaitMs = 50.0f;
        p.recommendedSteps = 16;
        p.stimulusDescription = "Two-tone carrier and modulator injection";
        p.routingNotes = "DAC In -> Ring Mod -> ADC In.";
        list.push_back(p);
    }

    // =========================================================================
    // Roland AIRA Eurorack Modular Submodules: Saturación & Dinámica (12 submódulos)
    // 07 (Tube Clipper - Torcido), 08 (Overdrive), 09 (Diode Distortion),
    // 10 (Bit Crusher), 11 (Germanium Fuzz), 21 (Ring Mod), 22 (Compressor),
    // 26 (VCA), 27 (Mixer), 28 (Panner), 29 (Phase Inverter), 31 (Waveshaper)
    // =========================================================================
    const std::vector<std::pair<std::string, std::string>> airaDynamicsModules = {
        { "aira_07_tube_clipper", "07. Vacuum Tube Warmth & Clipper" },
        { "aira_08_overdrive", "08. Soft Overdrive & Asymmetric Saturator" },
        { "aira_09_diode_distortion", "09. Hard Diode Clipper & Distortion" },
        { "aira_10_bit_crusher", "10. Lo-Fi Bit Crusher & Downsampler" },
        { "aira_11_germanium_fuzz", "11. Germanium Transistor Fuzz" },
        { "aira_21_ring_mod", "21. Ring Modulator & Four-Quadrant Multiplier" },
        { "aira_22_compressor", "22. Dynamic Compressor / Limiter" },
        { "aira_26_vca", "26. Linear / Exponential VCA" },
        { "aira_27_mixer", "27. 4-Channel DC-Coupled Audio/CV Mixer" },
        { "aira_28_panner", "28. Stereo Constant-Power Panner" },
        { "aira_29_phase_inverter", "29. Phase Inverter & Polarizer" },
        { "aira_31_waveshaper", "31. Non-Linear Polynomial Waveshaper" }
    };

    for (const auto& m : airaDynamicsModules)
    {
        ComponentPresetRecommendation p;
        p.typologyId = m.first;
        p.displayName = "Roland AIRA: " + m.second;
        p.category = ComponentCategory::EurorackAiraSubmodules;
        p.badgeText = "AIRA";
        p.burstDurationSec = 2.0f;
        p.recommendedSamples = 96000;
        p.recommendedFftSize = 4096;
        p.preRollMs = 150.0f;
        p.settlingWaitMs = 150.0f;
        p.recommendedSteps = 16;
        p.stimulusDescription = "Submodule calibrated test signal";
        p.routingNotes = "Audio In L/R -> AIRA Submodule -> Audio Out L/R";

        if (m.first.find("clipper") != std::string::npos || m.first.find("overdrive") != std::string::npos ||
            m.first.find("distortion") != std::string::npos || m.first.find("fuzz") != std::string::npos ||
            m.first.find("waveshaper") != std::string::npos)
        {
            p.algorithmName = "Multi-level WaveShaper + Wiener-Hammerstein LNL";
            p.stimulusType = audio::StimulusType::AmplitudeRamp;
            p.burstDurationSec = 1.5f;
            p.recommendedSamples = 65536;
        }
        else if (m.first.find("crusher") != std::string::npos)
        {
            p.algorithmName = "Quantization Step & Sample Decimate Estimation";
            p.stimulusType = audio::StimulusType::SineWave1kHz;
            p.burstDurationSec = 1.0f;
            p.recommendedSamples = 48000;
        }
        else if (m.first.find("ring_mod") != std::string::npos)
        {
            p.algorithmName = "Orthogonal Two-Tone Sideband Analysis";
            p.stimulusType = audio::StimulusType::SineWave1kHz;
            p.burstDurationSec = 1.0f;
            p.recommendedSamples = 32768;
        }
        else if (m.first.find("compressor") != std::string::npos)
        {
            p.algorithmName = "RMS Level Detector + Dynamic Ratio Tracking";
            p.stimulusType = audio::StimulusType::AmplitudeRamp;
            p.burstDurationSec = 2.0f;
            p.recommendedSamples = 96000;
        }
        else if (m.first.find("vca") != std::string::npos)
        {
            p.algorithmName = "Stepped RMS Gain Transfer vs CV";
            p.stimulusType = audio::StimulusType::SineWave1kHz;
            p.burstDurationSec = 1.5f;
            p.recommendedSamples = 65536;
        }
        else if (m.first.find("mixer") != std::string::npos || m.first.find("panner") != std::string::npos)
        {
            p.algorithmName = "Multi-Channel Summation Linearity & Crosstalk";
            p.stimulusType = audio::StimulusType::SineWave1kHz;
            p.burstDurationSec = 1.0f;
            p.recommendedSamples = 48000;
        }
        else if (m.first.find("phase_inverter") != std::string::npos)
        {
            p.algorithmName = "Phase Correlation Polarity Inversion";
            p.stimulusType = audio::StimulusType::SineWave1kHz;
            p.burstDurationSec = 0.5f;
            p.recommendedSamples = 32768;
        }
        else
        {
            p.algorithmName = "Universal DSP Transfer Function Characterization";
            p.stimulusType = audio::StimulusType::LogFarinaSweep;
        }

        list.push_back(p);
    }

    return list;
}

} // namespace presets
} // namespace core
} // namespace abdaudiolab
