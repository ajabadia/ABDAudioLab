#include "TimeAcousticPresetCatalog.h"

namespace abdaudiolab {
namespace core {
namespace presets {

std::vector<ComponentPresetRecommendation> TimeAcousticPresetCatalog::getPresets()
{
    std::vector<ComponentPresetRecommendation> list;

    // =========================================================================
    // 1. Osciladores & Generadores Autónomos (3 presets)
    // =========================================================================
    {
        ComponentPresetRecommendation p;
        p.typologyId = "osc_analog_vco_dco";
        p.displayName = "Analog VCO / DCO (Pitch Tracking & Harmonics)";
        p.category = ComponentCategory::OscillatorsAndGenerators;
        p.badgeText = "VCO";
        p.algorithmName = "YIN / McLeod Pitch Tracking + Blackman-Harris FFT (H2..H10)";
        p.stimulusType = audio::StimulusType::Silence; // Autonomous synth Note-On
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 120000; // 2.5s @ 48kHz
        p.recommendedFftSize = 16384;
        p.burstDurationSec = 2.5f;
        p.preRollMs = 200.0f;
        p.settlingWaitMs = 100.0f;
        p.recommendedSteps = 12; // 1 octave chromatic sweep
        p.stimulusDescription = "Autonomous MIDI Note-On continuous excitation";
        p.routingNotes = "Direct Audio Out -> Interface In 1. Disconnect stimulus generator DAC.";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "osc_wavetable_digital";
        p.displayName = "Wavetable / Digital Oscillator (Aliasing & Jitter)";
        p.category = ComponentCategory::OscillatorsAndGenerators;
        p.badgeText = "WT";
        p.algorithmName = "Welch Periodogram + Nyquist/2 Aliasing Ratio + Zero-Cross Jitter";
        p.stimulusType = audio::StimulusType::Silence;
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 65536;
        p.recommendedFftSize = 16384;
        p.burstDurationSec = 1.5f;
        p.preRollMs = 100.0f;
        p.settlingWaitMs = 50.0f;
        p.recommendedSteps = 32; // 32 wavetable position slices
        p.stimulusDescription = "Note-On with Table Index sweep";
        p.routingNotes = "Audio Out -> Line In. Step through wave positions.";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "gen_noise_multimode";
        p.displayName = "Multi-Mode Noise Generator (PSD Slope)";
        p.category = ComponentCategory::OscillatorsAndGenerators;
        p.badgeText = "NOI";
        p.algorithmName = "Welch Power Spectral Density (PSD) + Linear Log Slope";
        p.stimulusType = audio::StimulusType::Silence;
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 192000; // 4.0s @ 48kHz
        p.recommendedFftSize = 8192;
        p.burstDurationSec = 4.0f;
        p.preRollMs = 500.0f;
        p.settlingWaitMs = 0.0f;
        p.recommendedSteps = 1;
        p.stimulusDescription = "Passive continuous audio capture";
        p.routingNotes = "Noise output directly to input.";
        list.push_back(p);
    }

    // =========================================================================
    // 2. Efectos de Tiempo & Espaciales Clásicos (3 presets)
    // =========================================================================
    {
        ComponentPresetRecommendation p;
        p.typologyId = "dly_bbd_tape_echo";
        p.displayName = "Delay Line (BBD, Tape Echo, Digital Multi-Tap)";
        p.category = ComponentCategory::TimeAndSpatialEffects;
        p.badgeText = "DLY";
        p.algorithmName = "Cross-Correlation Peak Taps + Farina Echo Damping + FM Wow/Flutter";
        p.stimulusType = audio::StimulusType::DiracDelta;
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 192000; // 4.0s @ 48kHz
        p.recommendedFftSize = 8192;
        p.burstDurationSec = 4.0f;
        p.preRollMs = 50.0f;
        p.settlingWaitMs = 200.0f;
        p.recommendedSteps = 16;
        p.stimulusDescription = "Dirac Delta pulse with extended echo listening window";
        p.routingNotes = "Wet output only or 50% mix to capture repeated taps.";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "mod_chorus_flanger_phaser";
        p.displayName = "Modulation FX (Chorus, Flanger, Phaser, Leslie)";
        p.category = ComponentCategory::TimeAndSpatialEffects;
        p.badgeText = "MOD";
        p.algorithmName = "Hilbert Analytic Demodulation + Interaural Stereo ACF + Notch Counting";
        p.stimulusType = audio::StimulusType::SineWave1kHz;
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 168000; // 3.5s
        p.recommendedFftSize = 8192;
        p.burstDurationSec = 3.5f;
        p.preRollMs = 100.0f;
        p.settlingWaitMs = 150.0f;
        p.recommendedSteps = 16;
        p.stimulusDescription = "Sustained 1 kHz Sine Tone or Pink Noise burst";
        p.routingNotes = "Stereo L/R return for interaural chorus analysis.";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "rev_spring_plate_hall";
        p.displayName = "Reverberation (Spring, Plate, Hall, Convo, Shimmer)";
        p.category = ComponentCategory::TimeAndSpatialEffects;
        p.badgeText = "REV";
        p.algorithmName = "Schroeder Backward Integration (EDC/RT60) + Octave Bands Decay";
        p.stimulusType = audio::StimulusType::LogFarinaSweep;
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 240000; // 5.0s @ 48kHz
        p.recommendedFftSize = 16384;
        p.burstDurationSec = 5.0f;
        p.preRollMs = 100.0f;
        p.settlingWaitMs = 500.0f;
        p.recommendedSteps = 8;
        p.stimulusDescription = "Log Sweep (2.5s) followed by 2.5s reverberant decay";
        p.routingNotes = "100% Wet reverb output preferred for RT60 deconvolution.";
        list.push_back(p);
    }

    // =========================================================================
    // 3. Roland AIRA Eurorack Modular Submodules: Tiempo & Espacio (7 submódulos)
    // 16 (Short Delay), 17 (Tape Delay), 18 (BBD Chorus), 19 (Flanger),
    // 20 (Phaser), 23 (Noise Gen), 30 (Pitch Transposer)
    // =========================================================================
    {
        ComponentPresetRecommendation p;
        p.typologyId = "aira_16_short_delay";
        p.displayName = "Roland AIRA: 16. Short Space Delay / Echo";
        p.category = ComponentCategory::EurorackAiraSubmodules;
        p.badgeText = "AIRA";
        p.stimulusType = audio::StimulusType::DiracDelta;
        p.algorithmName = "Cross-Correlation Echo Taps + Damping";
        p.burstDurationSec = 3.5f;
        p.recommendedSamples = 168000;
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
        p.typologyId = "aira_17_tape_delay";
        p.displayName = "Roland AIRA: 17. Analogue Tape Delay & Wow/Flutter";
        p.category = ComponentCategory::EurorackAiraSubmodules;
        p.badgeText = "AIRA";
        p.stimulusType = audio::StimulusType::DiracDelta;
        p.algorithmName = "Cross-Correlation Echo Taps + Damping";
        p.burstDurationSec = 3.5f;
        p.recommendedSamples = 168000;
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
        p.typologyId = "aira_18_bbd_chorus";
        p.displayName = "Roland AIRA: 18. Stereo BBD Chorus / Ensemble";
        p.category = ComponentCategory::EurorackAiraSubmodules;
        p.badgeText = "AIRA";
        p.stimulusType = audio::StimulusType::SineWave1kHz;
        p.algorithmName = "Hilbert Analytic Demodulation + Notch Counting";
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
        p.typologyId = "aira_19_flanger";
        p.displayName = "Roland AIRA: 19. Resonant BBD Flanger";
        p.category = ComponentCategory::EurorackAiraSubmodules;
        p.badgeText = "AIRA";
        p.stimulusType = audio::StimulusType::SineWave1kHz;
        p.algorithmName = "Hilbert Analytic Demodulation + Notch Counting";
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
        p.typologyId = "aira_20_phaser";
        p.displayName = "Roland AIRA: 20. 4-Stage Analog Phaser";
        p.category = ComponentCategory::EurorackAiraSubmodules;
        p.badgeText = "AIRA";
        p.stimulusType = audio::StimulusType::SineWave1kHz;
        p.algorithmName = "Hilbert Analytic Demodulation + Notch Counting";
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
        p.typologyId = "aira_23_noise_gen";
        p.displayName = "Roland AIRA: 23. Multi-Color Noise Generator";
        p.category = ComponentCategory::EurorackAiraSubmodules;
        p.badgeText = "AIRA";
        p.stimulusType = audio::StimulusType::Silence;
        p.algorithmName = "Welch Power Spectral Density Slope";
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
        p.typologyId = "aira_30_pitch_transposer";
        p.displayName = "Roland AIRA: 30. Octave Shifter & Pitch Transposer";
        p.category = ComponentCategory::EurorackAiraSubmodules;
        p.badgeText = "AIRA";
        p.stimulusType = audio::StimulusType::SineWave1kHz;
        p.algorithmName = "Cepstral Pitch Transposition & Harmonizer Shift";
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

    // =========================================================================
    // 4. Acústica, Transductores & Studio Stompboxes (2 presets)
    // =========================================================================
    {
        ComponentPresetRecommendation p;
        p.typologyId = "cab_speaker_cabinet";
        p.displayName = "Speaker Cabinet / Studio Monitor (Anechoic Gating)";
        p.category = ComponentCategory::AcousticsAndTransducers;
        p.badgeText = "CAB";
        p.algorithmName = "Ultra-HD Farina Sweep (262k pts) + 4ms Time-Gating Reflection Cutoff";
        p.stimulusType = audio::StimulusType::LogFarinaSweep;
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 192000; // 4.0s @ 48kHz
        p.recommendedFftSize = 16384;
        p.burstDurationSec = 4.0f;
        p.preRollMs = 200.0f;
        p.settlingWaitMs = 500.0f;
        p.recommendedSteps = 1;
        p.stimulusDescription = "High-definition Log Sweep (20 Hz - 24 kHz)";
        p.routingNotes = "Power Amp In -> Microphone Return (Anechoic windowed)";
        list.push_back(p);
    }
    {
        ComponentPresetRecommendation p;
        p.typologyId = "mic_preamp_ein";
        p.displayName = "Microphone Preamp & Stompbox (EIN & AES17 Range)";
        p.category = ComponentCategory::AcousticsAndTransducers;
        p.badgeText = "MIC";
        p.algorithmName = "Two-Channel Calibration vs Reference + EIN Noise Floor (dBu)";
        p.stimulusType = audio::StimulusType::LogFarinaSweep;
        p.captureMode = "FIXED_TIME";
        p.recommendedSamples = 144000;
        p.recommendedFftSize = 8192;
        p.burstDurationSec = 2.5f;
        p.preRollMs = 150.0f;
        p.settlingWaitMs = 150.0f;
        p.recommendedSteps = 12;
        p.stimulusDescription = "Log Sweep at low line level followed by 2.0s silence";
        p.routingNotes = "Attenuated DAC out -> Mic Pre In -> Line Out Return";
        list.push_back(p);
    }

    return list;
}

} // namespace presets
} // namespace core
} // namespace abdaudiolab
