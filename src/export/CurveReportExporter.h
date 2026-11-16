// ==============================================================================
// src/export/CurveReportExporter.h
// Fase 4 — Exportador JSON de reporte offline de barrido de curva.
// ==============================================================================

#pragma once

#include "../math/LabEnvelopeCurveAnalyzer.h"
#include <string>
#include <vector>

namespace abdaudiolab::exporting
{

struct CurveReportMeta
{
    std::string hardwareId      { "behringer_deepmind12" };
    std::string hardwareName    { "Behringer DeepMind 12 / 12D" };
    std::string functionId      { "vcf_adsr_envelope" };
    std::string parameterName   { "Env2 Attack Curve" };
    std::string parameterNrpn   { "58" };
    std::string captureMode     { "ENVELOPE_CURVE_SWEEP" };
    double sampleRate           { 48000.0 };
    int sweepPointCount         { 0 };
    std::string sweepByteValue  { "128" };
    std::string sweepNormalizedValue { "0.5" };
    std::string sweepTargetByte { "128" };
};

/**
 * @class CurveReportExporter
 * @brief Exporta a JSON el resultado del análisis offline de una curva de
 *        barrido de envolvente, con metadatos, métrica R^2 y vector completo
 *        de residuos.
 *
 * Ruta de salida obligatoria del protocolo:
 *   build/ABDAudioLab_artefacts/CapturedSweeps/curve_sweep_128.json
 */
class CurveReportExporter
{
public:
    CurveReportExporter() = default;
    ~CurveReportExporter() = default;

    static bool writeOfflineReport(
        const std::string& outputFilePath,
        const CurveReportMeta& meta,
        const math::EnvelopeRegressionResult& regression,
        const math::EnvelopeBreakpointResult& breakpoint,
        const std::vector<math::EnvelopePoint>& rawPoints);

    /**
     * @brief Ruta canónica de la Fase 4.
     */
    [[nodiscard]] static std::string canonicalOutputPath() noexcept;
};

} // namespace abdaudiolab::exporting
