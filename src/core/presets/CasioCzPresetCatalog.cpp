#include "CasioCzPresetCatalog.h"

namespace abdaudiolab {
namespace core {
namespace presets {

std::vector<ComponentPresetRecommendation> CasioCzPresetCatalog::getPresets()
{
    std::vector<ComponentPresetRecommendation> czPresets;
    czPresets.reserve(3);

    // =========================================================================
    // 1. PRESET: CASIO CZ DISTORSIÓN DE FASE (DCW WAVESHAPER)
    // =========================================================================
    {
        ComponentPresetRecommendation dcwPreset;
        dcwPreset.typologyId = "casio_cz_dcw_waveshaper";
        dcwPreset.displayName = "Casio CZ DCW Phase Distortion";
        dcwPreset.category = ComponentCategory::FiltersAndToneShapers;
        dcwPreset.badgeText = "CZ-DCW";
        dcwPreset.algorithmName = "Phase Distortion Resonant Knee Fit + Chebyshev Harmonic Spectrum";
        dcwPreset.captureMode = "SYNCHRONOUS_SPECTRUM_SWEEP";

        // Configuración detallada de la recomendación de laboratorio
        dcwPreset.stimulusType = audio::StimulusType::SineWave1kHz;
        dcwPreset.burstDurationSec = 1.5f;
        dcwPreset.settlingWaitMs = 50.0f;
        dcwPreset.preRollMs = 50.0f;
        dcwPreset.recommendedFftSize = 16384;
        dcwPreset.recommendedSamples = 96000;
        dcwPreset.recommendedSteps = 100; // Resolución exacta 0..99 de Casio CZ
        dcwPreset.stimulusDescription = "1 kHz Pure Sine excitation via Phase Distortion engine";
        dcwPreset.routingNotes = "Virtual loopback: Inject SysEx DCW sweep [0..99] to VES via loopMIDI.";

        // Receta de medición y preparación SysEx específica en el contrato
        dcwPreset.recipe.recipeType = "DIRECT_AUDIO_IN";
        dcwPreset.recipe.description = "Casio CZ DCW Phase Distortion characterization sweep";
        dcwPreset.recipe.postSettlingDelayMs = 50;

        // Inyectar acciones de setup inicial SysEx específicas en el contrato de la receta
        // F0 44 00 [Ch0] [MSB] [LSB] [MSN] [LSN] F7
        // INITIALIZE_CZ_DCO1: Opcode 0x00 0x01, Val 0 -> F0 44 00 00 00 01 00 00 F7
        {
            HardwareSetupAction act;
            act.description = "INITIALIZE_CZ_DCO1";
            act.method = HardwareMethod::SYSEX_RAW;
            act.channel = 1;
            act.sysexHexPayload = "F0 44 00 00 00 01 00 00 F7";
            act.settlingDelayMs = 20;
            dcwPreset.recipe.setupActions.push_back(act);
        }
        // DISABLE_VIBRATO: Opcode 0x00 0x00, Val 0 -> F0 44 00 00 00 00 00 00 F7
        {
            HardwareSetupAction act;
            act.description = "DISABLE_VIBRATO";
            act.method = HardwareMethod::SYSEX_RAW;
            act.channel = 1;
            act.sysexHexPayload = "F0 44 00 00 00 00 00 00 F7";
            act.settlingDelayMs = 20;
            dcwPreset.recipe.setupActions.push_back(act);
        }
        // MAX_DCA1_SUSTAIN: Opcode 0x30 0x0F, Val 99 (0x63: MSN=6, LSN=3) -> F0 44 00 00 30 0F 06 03 F7
        {
            HardwareSetupAction act;
            act.description = "MAX_DCA1_SUSTAIN";
            act.method = HardwareMethod::SYSEX_RAW;
            act.channel = 1;
            act.sysexHexPayload = "F0 44 00 00 30 0F 06 03 F7";
            act.settlingDelayMs = 20;
            dcwPreset.recipe.setupActions.push_back(act);
        }

        czPresets.push_back(dcwPreset);
    }

    // =========================================================================
    // 2. PRESET: CASIO CZ ENVOLVENTE MULTI-PASO (8-STAGE ENVELOPE)
    // =========================================================================
    {
        ComponentPresetRecommendation envPreset;
        envPreset.typologyId = "casio_cz_multistep_envelope";
        envPreset.displayName = "Casio CZ Multi-Step 8-Stage Envelope";
        envPreset.category = ComponentCategory::ModulatorsAndEnvelopes;
        envPreset.badgeText = "CZ-ENV";
        envPreset.algorithmName = "Multi-Step 8-Stage Inverse Logarithmic Segment Calibration";
        envPreset.captureMode = "ADAPTIVE_ENVELOPE";

        envPreset.stimulusType = audio::StimulusType::SyncPulses3; // Ráfagas de Gate con tiempos medidos
        envPreset.burstDurationSec = 5.0f;
        envPreset.settlingWaitMs = 20.0f;
        envPreset.preRollMs = 50.0f;
        envPreset.silenceThresholdDb = -60.0f; // Early Stopping dinámico optimizado para envolventes rápidas
        envPreset.recommendedSamples = 96000;
        envPreset.recommendedFftSize = 4096;
        envPreset.recommendedSteps = 8; // Pasos equivalentes a los 8 steps de Casio
        envPreset.stimulusDescription = "Dynamic Gate Pulses with measured sustain hold";
        envPreset.routingNotes = "Audio output from VES captured across 8 envelope steps.";

        envPreset.recipe.recipeType = "DIRECT_AUDIO_IN";
        envPreset.recipe.description = "Casio CZ Multi-Step Envelope log calibration";
        envPreset.recipe.postSettlingDelayMs = 20;

        czPresets.push_back(envPreset);
    }

    // =========================================================================
    // 3. PRESET: CASIO CZ DCO FUNDAMENTAL TRACKING
    // =========================================================================
    {
        ComponentPresetRecommendation dcoPreset;
        dcoPreset.typologyId = "casio_cz_dco_phase_octave";
        dcoPreset.displayName = "Casio CZ DCO Tuning & Phase";
        dcoPreset.category = ComponentCategory::OscillatorsAndGenerators;
        dcoPreset.badgeText = "CZ-DCO";
        dcoPreset.algorithmName = "YIN Fundamental Tracking & Octave Duty-Cycle Phase Analysis";
        dcoPreset.captureMode = "AUTONOMOUS_PITCH_DETECTION";

        dcoPreset.stimulusType = audio::StimulusType::Silence; // El sinte genera el audio de forma autónoma
        dcoPreset.burstDurationSec = 2.0f;
        dcoPreset.settlingWaitMs = 10.0f;
        dcoPreset.preRollMs = 50.0f;
        dcoPreset.recommendedSamples = 96000;
        dcoPreset.recommendedFftSize = 16384;
        dcoPreset.recommendedSteps = 12;
        dcoPreset.stimulusDescription = "Autonomous Note-On excitation for pitch and phase duty cycle";
        dcoPreset.routingNotes = "Direct Audio In from VES with MIDI Note articulation.";

        dcoPreset.recipe.recipeType = "LEGATO_PITCH_SWEEP";
        dcoPreset.recipe.description = "Casio CZ DCO tuning and duty cycle tracking";
        dcoPreset.recipe.postSettlingDelayMs = 10;

        czPresets.push_back(dcoPreset);
    }

    return czPresets;
}

} // namespace presets
} // namespace core
} // namespace abdaudiolab
