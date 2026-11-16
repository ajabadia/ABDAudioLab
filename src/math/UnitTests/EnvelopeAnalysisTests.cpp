// ==============================================================================
// src/math/UnitTests/EnvelopeAnalysisTests.cpp
// Fase 4 — Suite de aceptación sintética para LabEnvelopeCurveAnalyzer y
//          CurveReportExporter, validada antes de cualquier ejecución sobre
//          hardware físico.
// ==============================================================================

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "math/LabEnvelopeCurveAnalyzer.h"
#include "export/CurveReportExporter.h"

#include <cmath>
#include <fstream>
#include <vector>
#include <string>

using namespace abdaudiolab::math;
using namespace abdaudiolab::exporting;

namespace
{
    static constexpr float kFLOAT_TOLERANCE_ABS = 1e-4f;
    static constexpr float kR2_NEAR_PERFECT     = 1e-3f;
}

// -----------------------------------------------------------------------------
// Utilidad local: generar puntos sintéticos
// -----------------------------------------------------------------------------

static std::vector<EnvelopePoint> makeLinearRamp(float startTime,
                                                 float endTime,
                                                 float startHz,
                                                 float endHz,
                                                 size_t pointCount)
{
    std::vector<EnvelopePoint> pts;
    if (pointCount < 2)
        return pts;

    const float dt = (endTime - startTime) / static_cast<float>(pointCount - 1);
    for (size_t i = 0; i < pointCount; ++i)
    {
        const float t = startTime + static_cast<float>(i) * dt;
        const float hz = startHz + (endHz - startHz) * static_cast<float>(i) / static_cast<float>(pointCount - 1);
        pts.emplace_back(t, hz);
    }
    return pts;
}

// -----------------------------------------------------------------------------
// Regresión lineal sobre rampa perfecta
// -----------------------------------------------------------------------------

TEST_CASE("EnvelopeAnalysis: perfect linear ramp yields R^2 ~ 1.0", "[math][analysis][envelope]")
{
    const auto pts = makeLinearRamp(0.0f, 0.004f, 200.0f, 800.0f, 17);

    const auto reg = LabEnvelopeCurveAnalyzer::analyzeTrajectory(pts);

    CHECK(reg.valid);
    CHECK_THAT(reg.rSquared, Catch::Matchers::WithinAbs(1.0f, kR2_NEAR_PERFECT));
    CHECK_THAT(reg.slope, Catch::Matchers::WithinAbs((800.0f - 200.0f) / 0.004f, kFLOAT_TOLERANCE_ABS));
    CHECK_THAT(reg.intercept, Catch::Matchers::WithinAbs(200.0f, kFLOAT_TOLERANCE_ABS));

    // Residuos de una recta perfecta deben ser numéricamente 0.
    for (float r : reg.residuals)
        CHECK_THAT(std::abs(r), Catch::Matchers::WithinAbs(0.0f, kFLOAT_TOLERANCE_ABS));
}

// -----------------------------------------------------------------------------
// Regresión lineal sobre línea plana
// -----------------------------------------------------------------------------

TEST_CASE("EnvelopeAnalysis: flat line yields slope 0 and R^2 1.0", "[math][analysis][envelope]")
{
    std::vector<EnvelopePoint> pts;
    pts.reserve(13);
    for (size_t i = 0; i < 13; ++i)
        pts.emplace_back(static_cast<float>(i) * 0.0005f, 900.0f);

    const auto reg = LabEnvelopeCurveAnalyzer::analyzeTrajectory(pts);

    CHECK(reg.valid);
    CHECK_THAT(reg.rSquared, Catch::Matchers::WithinAbs(1.0f, kR2_NEAR_PERFECT));
    CHECK_THAT(reg.slope, Catch::Matchers::WithinAbs(0.0f, kFLOAT_TOLERANCE_ABS));
    CHECK_THAT(reg.intercept, Catch::Matchers::WithinAbs(900.0f, kFLOAT_TOLERANCE_ABS));
}

// -----------------------------------------------------------------------------
// Curva con meseta de saturación artificial
// -----------------------------------------------------------------------------

TEST_CASE("EnvelopeAnalysis: synthetic saturation plateau is flagged by breakpoint", "[math][analysis][envelope]")
{
    // Fase de subida lineal fuerte, luego meseta plana.
    std::vector<EnvelopePoint> pts;

    // Subida: 0 -> 0.003 s, 100 Hz -> 1200 Hz
    const auto rising = makeLinearRamp(0.0f, 0.003f, 100.0f, 1200.0f, 21);
    pts.insert(pts.end(), rising.begin(), rising.end());

    // Meseta: 0.003 -> 0.007 s, se mantiene en ~ 1200 Hz con ruido pequeño.
    for (size_t i = 1; i <= 8; ++i)
    {
        const float t = 0.003f + static_cast<float>(i) * 0.0005f;
        pts.emplace_back(t, 1200.0f + (static_cast<float>(i) * 0.0f));
    }

    const auto reg = LabEnvelopeCurveAnalyzer::analyzeTrajectory(pts);
    CHECK(reg.valid);

    // R^2 debe ser menor que 1 porque la curva global ya no es lineal pura.
    CHECK(reg.rSquared < 1.0f);

    const auto bp = LabEnvelopeCurveAnalyzer::detectBreakpoint(pts, reg);

    CHECK(bp.detected);
    CHECK(bp.breakpointTimeSec >= 0.002f - kFLOAT_TOLERANCE_ABS);
    CHECK(bp.breakpointTimeSec <= 0.0075f + kFLOAT_TOLERANCE_ABS);
}

// -----------------------------------------------------------------------------
// Residuos: media y desviación
// -----------------------------------------------------------------------------

TEST_CASE("EnvelopeAnalysis: residuals statistics match direct computation",
          "[math][analysis][envelope]")
{
    const auto pts = makeLinearRamp(0.0f, 0.003f, 100.0f, 700.0f, 13);

    const auto reg = LabEnvelopeCurveAnalyzer::analyzeTrajectory(pts);

    const float computedMean = LabEnvelopeCurveAnalyzer::residualsMean(reg.residuals);
    const float computedStd  = LabEnvelopeCurveAnalyzer::residualsStdDev(reg.residuals);

    CHECK_THAT(computedMean, Catch::Matchers::WithinAbs(reg.residualMean, kFLOAT_TOLERANCE_ABS));
    CHECK_THAT(computedStd, Catch::Matchers::WithinAbs(reg.residualStdDev, kFLOAT_TOLERANCE_ABS));
}

// -----------------------------------------------------------------------------
// Exportador JSON: estructura y recepción del archivo
// -----------------------------------------------------------------------------

TEST_CASE("CurveReportExporter: writes offline report with R^2 and full residual vector",
          "[math][analysis][envelope]")
{
    const auto pts = makeLinearRamp(0.0f, 0.005f, 200.0f, 1000.0f, 21);

    const auto reg = LabEnvelopeCurveAnalyzer::analyzeTrajectory(pts);
    REQUIRE(reg.valid);

    const auto bp = LabEnvelopeCurveAnalyzer::detectBreakpoint(pts, reg);

    CurveReportMeta meta;
    meta.sweepPointCount        = static_cast<int>(pts.size());
    meta.sweepByteValue         = "128";
    meta.sweepNormalizedValue   = "0.5";
    meta.sweepTargetByte        = "128";

    const std::string path = "build/ABDAudioLab_artefacts/CapturedSweeps/curve_sweep_128.json";

    const bool written = CurveReportExporter::writeOfflineReport(
        path, meta, reg, bp, pts);

    CHECK(written);

    // Verificación básica de presencia de campos clave sin depender de un parser
    // externo de JSON de schema, ya que usamos el formato producido por
    // CurveReportExporter mismo para validar la escritura.
    std::ifstream in(path);
    CHECK(in.is_open());

    std::string contents;
    {
        std::string line;
        while (std::getline(in, line))
            contents += line;
    }

    CHECK(contents.find("\"rSquared\"") != std::string::npos);
    CHECK(contents.find("\"residuals\"") != std::string::npos);
    CHECK(contents.find("\"breakpoint\"") != std::string::npos);
    CHECK(contents.find("\"sweepMeta\"") != std::string::npos);
    CHECK(contents.find("\"schemaVersion\"") != std::string::npos);
}

// -----------------------------------------------------------------------------
// Ruta canónica del protocolo
// -----------------------------------------------------------------------------

TEST_CASE("CurveReportExporter: canonical path matches protocol document",
          "[math][analysis][envelope]")
{
    const std::string canonical = CurveReportExporter::canonicalOutputPath();

    CHECK(canonical.find("build/ABDAudioLab_artefacts/CapturedSweeps") != std::string::npos);
    CHECK(canonical.find("curve_sweep_128.json") != std::string::npos);
    CHECK(canonical.find("\\") == std::string::npos);
    CHECK(canonical.find("/") != std::string::npos);
}
