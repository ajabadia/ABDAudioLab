// ==============================================================================
// src/export/CurveReportExporter.cpp
// Fase 4 — Implementación del exportador JSON de reporte de curva.
// ==============================================================================

#include "CurveReportExporter.h"
#include "math/LabEnvelopeCurveAnalyzer.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iomanip>


namespace abdaudiolab::exporting
{

std::string CurveReportExporter::canonicalOutputPath() noexcept
{
    return "build/ABDAudioLab_artefacts/CapturedSweeps/curve_sweep_128.json";
}

bool CurveReportExporter::writeOfflineReport(
    const std::string& outputFilePath,
    const CurveReportMeta& meta,
    const math::EnvelopeRegressionResult& regression,
    const math::EnvelopeBreakpointResult& breakpoint,
    const std::vector<math::EnvelopePoint>& rawPoints)
{
    nlohmann::json root;
    root["schemaVersion"]   = "1.0";
    root["generator"]       = "ABDAudioLab Fase 4 CurveReportExporter";
    root["createdBy"]       = "Offline Envelope Curve Analyzer";

    // Metadatos del barrido.
    nlohmann::json metaJson;
    metaJson["hardwareId"]              = meta.hardwareId;
    metaJson["hardwareName"]            = meta.hardwareName;
    metaJson["functionId"]              = meta.functionId;
    metaJson["parameterName"]           = meta.parameterName;
    metaJson["parameterNrpn"]           = meta.parameterNrpn;
    metaJson["captureMode"]             = meta.captureMode;
    metaJson["sampleRateHz"]            = meta.sampleRate;
    metaJson["sweepPointCount"]         = meta.sweepPointCount;
    metaJson["sweepByteValue"]          = meta.sweepByteValue;
    metaJson["sweepNormalizedValue"]    = meta.sweepNormalizedValue;
    metaJson["sweepTargetByte"]         = meta.sweepTargetByte;
    metaJson["outputFilePath"]          = outputFilePath;
    root["sweepMeta"]                  = metaJson;

    // Regresión lineal.
    nlohmann::json regJson;
    regJson["valid"]            = regression.valid;
    regJson["slopeHzPerSec"]   = regression.slope;
    regJson["interceptHz"]     = regression.intercept;
    regJson["rSquared"]        = regression.rSquared;
    regJson["residualMean"]    = regression.residualMean;
    regJson["residualStdDev"]  = regression.residualStdDev;
    root["regression"]         = regJson;

    // Residuos muestra a muestra.
    nlohmann::json residualsJson = nlohmann::json::array();
    for (float r : regression.residuals)
        residualsJson.push_back(r);
    root["residuals"] = residualsJson;

    // Y ajustado muestra a muestra.
    nlohmann::json fittedJson = nlohmann::json::array();
    for (float f : regression.fittedHz)
        fittedJson.push_back(f);
    root["fittedHz"] = fittedJson;

    // Puntos brutos.
    nlohmann::json pointsJson = nlohmann::json::array();
    for (const auto& p : rawPoints)
    {
        nlohmann::json pt;
        pt["timeSec"]     = p.timeSec;
        pt["measuredHz"]  = p.measuredHz;
        pointsJson.push_back(pt);
    }
    root["rawPoints"] = pointsJson;

    // Breakpoint / saturación.
    nlohmann::json bpJson;
    bpJson["detected"]            = breakpoint.detected;
    bpJson["breakpointTimeSec"]   = breakpoint.breakpointTimeSec;
    bpJson["localSlopeHzPerSec"]  = breakpoint.localSlopeHzPerSec;
    bpJson["residualSignMean"]    = breakpoint.residualSignMean;
    bpJson["reason"]              = breakpoint.reason;
    root["breakpoint"]            = bpJson;

    // Escritura.
    std::ofstream out(outputFilePath);
    if (!out.is_open())
        return false;

    out.imbue(std::locale::classic());
    out << std::setw(2) << root << std::endl;
    return out.good();
}

} // namespace abdaudiolab::exporting
