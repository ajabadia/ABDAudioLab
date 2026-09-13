#pragma once

namespace abdaudiolab::fixtures
{

/**
 * @brief Estructura de oráculo para Ground Truth interno del ReferenceSynth.
 *
 * CRÍTICO: Esta estructura y sus canales de lectura están terminantemente prohibidos
 * para el motor de ABDAudioLab (TargetContractDiscovery, TargetAuditor, ParameterExcitationEngine,
 * AcousticObserver y CLI). Solo se incluye y compila en las suites de test unitario (test_Vst3Validation)
 * para certificar que el motor infiere las propiedades en modo caja negra sin conocer la verdad interna.
 */
struct ReferenceSynthGroundTruth
{
    double realInternalCutoffHz { 1000.0 };
    double realResonanceQ { 0.707 };
    double realAttackSec { 0.010 };
    double realGainLinear { 1.0 };
    double realPitchSemitones { 0.0 };

    bool isBlockQuantizedActive { false };
    double realSmoothingTimeMs { 0.0 };
    int realPipelineLatencySamples { 0 };
    int activeVoiceCount { 0 };
};

} // namespace abdaudiolab::fixtures
