/**
 * @file test_ProfileValidationScopeReportExporter.cpp
 * @brief Catch2 unit tests for ProfileValidationScopeReportExporter (Phase 19.5).
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "export/ProfileValidationScopeReportExporter.h"
#include "support/LabTestScratch.h"
#include <juce_core/juce_core.h>

using namespace abdaudiolab;
using namespace abdaudiolab::exporting;
using namespace abdaudiolab::synth;

namespace
{

ProfileScopeReportData buildMockScopeReportData(SelectionStatus status = SelectionStatus::Accepted)
{
    ProfileScopeReportData data;
    data.targetName = "Boss OD-1 Classic Overdrive";
    data.targetManufacturer = "Roland / Boss";
    data.hardwareType = "Analog Asymmetric Clipper Overdrive";
    data.profileTimestampUtc = "2026-10-14T12:00:00Z";
    data.operatorNotes = "Medición nominal en banco controlado con temperatura estable.";

    // Dominio de validez
    data.validityDomain.sampleRate = 96000.0;
    data.validityDomain.minInputLevelDbfs = -36.0;
    data.validityDomain.maxInputLevelDbfs = -3.0;
    data.validityDomain.warmupMinutes = 15;
    data.validityDomain.documentedControlRanges = "Level [0..100%], Drive [0..100%]";

    // Calibración de interfaz
    data.loopbackSnrDb = 94.2;
    data.loopbackNoiseFloorDbfs = -97.5;

    // Evaluación multiobjetivo
    data.evaluation.decision.status = status;
    data.evaluation.decision.recommendedModelId = "WienerHammerstein";
    data.evaluation.decision.bestRawFidelityModelId = "NamNeural";
    data.evaluation.decision.selectionScore = 0.045;
    data.evaluation.metrics.rSquaredScore = 0.9935;
    data.evaluation.metrics.errorToSignalRatioDb = -28.4;
    data.evaluation.metrics.rootMeanSquareError = 0.0038;
    data.evaluation.canonicalEvaluationHash = "a1b2c3d4e5f67890123456789abcdef0123456789abcdef0123456789abcdef0";

    data.evaluation.uncertainty.instrumentalUncertaintyDb = 0.05;
    data.evaluation.uncertainty.structuralModelErrorDb = 0.12;
    data.evaluation.uncertainty.combinedUncertaintyDb = 0.13;

    data.evaluation.resourceCost.cpuUsagePercentPerVoice = 1.20;
    data.evaluation.resourceCost.memoryBytes = 16384;
    data.evaluation.resourceCost.introducedLatencySamples = 16;

    // Benchmarking report
    data.benchmarkingReport.preset = OptimizationPreset::Balanced;
    data.benchmarkingReport.recommendedTargetModelId = "WienerHammerstein";
    data.benchmarkingReport.bestRawFidelityModelId = "NamNeural";

    auto addCandidate = [&](const std::string& id, const std::string& arch, double r2, double esr,
                            double rho1, double cpu, size_t ram, int lat, double score, bool isLeader) {
        CandidateBenchmarkResult c;
        c.modelId = id;
        c.architecture = arch;
        c.metrics.rSquaredScore = r2;
        c.metrics.errorToSignalRatioDb = esr;
        c.diagnostics.peakAutocorrelation = rho1;
        c.footprint.cpuUsagePercentPerVoice = cpu;
        c.footprint.memoryBytes = ram;
        c.footprint.introducedLatencySamples = lat;
        c.totalCompositeScore = score;
        c.isRawFidelityLeader = isLeader;
        data.benchmarkingReport.candidates.push_back(c);
    };

    addCandidate("Lut1D", "LUT 1D Waveshaper", 0.8850, -14.2, 0.08, 0.15, 8192, 0, 0.185, false);
    addCandidate("LutSimd2D", "LUT 2D Bilinear Grid", 0.9240, -18.5, 0.09, 0.35, 65536, 0, 0.142, false);
    addCandidate("WienerHammerstein", "FIR-Poly-FIR (LNL)", 0.9935, -28.4, 0.05, 1.20, 16384, 16, 0.045, false);
    addCandidate("DynamicMemory", "RC Hysteresis Dynamic", 0.9942, -29.1, 0.04, 1.80, 8192, 0, 0.052, false);
    addCandidate("NamNeural", "WaveNet Lite RNN", 0.9950, -30.2, 0.02, 8.50, 2097152, 64, 0.165, true);

    // Holdout
    data.holdoutDatasetHash = "99887766554433221100aabbccddeeff99887766554433221100aabbccddeeff";
    data.leakageDetected = false;
    data.leakageMinDistance = 2.45;

    auto addBattery = [&](const std::string& name, const std::string& desc, double dur, double pk, double rms, double cf) {
        HoldoutBatterySummary bs;
        bs.batteryName = name;
        bs.description = desc;
        bs.acoustics.durationSeconds = dur;
        bs.acoustics.peakDbfs = pk;
        bs.acoustics.rmsDbfs = rms;
        bs.acoustics.crestFactorDb = cf;
        data.holdoutBatteries.push_back(bs);
    };

    addBattery("Sub-batería 1: Escalas Dinámicas", "Escalera de amplitudes [-30..-3 dBFS] con ventanas de Hann", 0.15, -3.0, -11.2, 8.2);
    addBattery("Sub-batería 2: Transitorios con Reposo", "Transitorios rápidos con reposo > 33%", 0.20, -3.0, -18.4, 15.4);
    addBattery("Sub-batería 3: Multitono Schroeder", "Multitono no armónico con fases Schroeder", 0.15, -3.0, -11.43, 8.43);
    addBattery("Sub-batería 4: Secuencia Pseudo-musical", "Secuencia cuasi-estacionaria", 0.25, -3.0, -12.1, 9.1);

    // Curvas vectoriales simuladas (50 puntos de 20 Hz a 20 kHz)
    for (int i = 0; i < 50; ++i)
    {
        float f = 20.0f * std::pow(1000.0f, static_cast<float>(i) / 49.0f);
        data.frequencyBinsHz.push_back(f);
        data.targetMagnitudeDb.push_back(-6.0f - (f > 1000.0f ? 12.0f * std::log10(f / 1000.0f) : 0.0f));
        data.modelMagnitudeDb.push_back(data.targetMagnitudeDb.back() + 0.15f * std::sin(static_cast<float>(i)));
        data.residualMagnitudeDb.push_back(-42.0f - 0.2f * static_cast<float>(i));
    }

    // Residuo temporal simulado (100 muestras)
    for (int n = 0; n < 100; ++n)
    {
        data.timeDomainResidual.push_back(0.005f * std::sin(2.0f * 3.14159f * static_cast<float>(n) / 10.0f));
    }

    return data;
}

} // namespace

TEST_CASE("ProfileValidationScopeReportExporter: Nominal HTML & SVG Generation", "[export][scope_report]")
{
    auto data = buildMockScopeReportData(SelectionStatus::Accepted);

    juce::File reportFile = abdaudiolab::test::scratchDir("ScopeReport")
        .getChildFile("test_validation_scope_report.html");
    if (reportFile.existsAsFile())
        reportFile.deleteFile();

    bool ok = ProfileValidationScopeReportExporter::exportReportToHtml(reportFile.getFullPathName().toStdString(), data);
    REQUIRE(ok);
    REQUIRE(reportFile.existsAsFile());
    REQUIRE(reportFile.getSize() > 3000);

    juce::String content = reportFile.loadFileAsString();

    SECTION("Encabezado, identidad y badges")
    {
        REQUIRE(content.contains("INFORME DE VALIDACIÓN Y ALCANCE DEL PERFIL"));
        REQUIRE(content.contains("Boss OD-1 Classic Overdrive"));
        REQUIRE(content.contains("badge-pass"));
        REQUIRE(content.contains("[OK] PERFIL VALIDADO"));
    }

    SECTION("Dominio de Validez y Advertencia de No-Extrapolación")
    {
        REQUIRE(content.contains("DOMINIO DE VALIDEZ DECLARADO"));
        REQUIRE(content.contains("96 kHz"));
        REQUIRE(content.contains("-36 a -3 dBFS"));
        REQUIRE(content.contains("Cláusula Estricta de No-Extrapolación"));
        REQUIRE(content.contains("extrapolación matemática no validada"));
    }

    SECTION("Presupuesto de Incertidumbre Metrológica GUM")
    {
        REQUIRE(content.contains("Presupuesto de Incertidumbre Metrológica (GUM / ISO 98-3)"));
        REQUIRE(content.contains("Incertidumbre Instrumental (u<sub>inst</sub>)"));
        REQUIRE(content.contains("Incertidumbre Estructural (u<sub>struct</sub>)"));
        REQUIRE(content.contains("Incertidumbre Combinada (u<sub>combined</sub>)"));
        REQUIRE(content.contains("RESOLUCIÓN EFECTIVA (ENOB)"));
    }

    SECTION("Tabla Comparativa Multiobjetivo")
    {
        REQUIRE(content.contains("Evaluación Multiobjetivo de Modelos Candidatos"));
        REQUIRE(content.contains("Lut1D"));
        REQUIRE(content.contains("LutSimd2D"));
        REQUIRE(content.contains("WienerHammerstein"));
        REQUIRE(content.contains("DynamicMemory"));
        REQUIRE(content.contains("NamNeural"));
        REQUIRE(content.contains("RECOMENDADO"));
        REQUIRE(content.contains("MAX. FIDELIDAD"));
    }

    SECTION("Batería Holdout y Auditoría Anti-Leakage")
    {
        REQUIRE(content.contains("Batería de Validación Ciega Fuera de Muestra"));
        REQUIRE(content.contains("Multitono Schroeder"));
        REQUIRE(content.contains("CF &lt; 9.0 dB (Pass)"));
        REQUIRE(content.contains("PASSED — ZERO CONTAMINATION"));
    }

    SECTION("Gráficos Vectoriales SVG Embebidos")
    {
        REQUIRE(content.contains("<svg"));
        REQUIRE(content.contains("ESPECTRO COMPARATIVO Y RESIDUO"));
        REQUIRE(content.contains("line-target"));
        REQUIRE(content.contains("line-model"));
        REQUIRE(content.contains("line-residual"));
        REQUIRE(content.contains("FORMA DE ONDA DEL ERROR RESIDUAL"));
    }

    SECTION("Desglose de Coste Computacional y Sellado Criptográfico")
    {
        REQUIRE(content.contains("Desglose de Coste Computacional en Producción"));
        REQUIRE(content.contains("CPU ESTIMADA / VOZ"));
        REQUIRE(content.contains("POLIFONÍA ADMISIBLE"));
        REQUIRE(content.contains("EVAL_HASH (RFC 8785)"));
        REQUIRE(content.contains("HOLDOUT_HASH"));
    }

    reportFile.deleteFile();
}

TEST_CASE("ProfileValidationScopeReportExporter: Status Variants & Edge Cases", "[export][scope_report]")
{
    SECTION("Status: Inconclusive (Empate Estadístico)")
    {
        auto data = buildMockScopeReportData(SelectionStatus::Inconclusive);
        juce::File reportFile = abdaudiolab::test::scratchDir("ScopeReport")
            .getChildFile("test_scope_inconclusive.html");
        if (reportFile.existsAsFile())
            reportFile.deleteFile();

        REQUIRE(ProfileValidationScopeReportExporter::exportReportToHtml(reportFile.getFullPathName().toStdString(), data));
        juce::String content = reportFile.loadFileAsString();
        REQUIRE(content.contains("badge-inconclusive"));
        REQUIRE(content.contains("INCONCLUSIVE: EMPATE ESTADÍSTICO"));
        reportFile.deleteFile();
    }

    SECTION("Status: AcceptedWithWarnings")
    {
        auto data = buildMockScopeReportData(SelectionStatus::AcceptedWithWarnings);
        juce::File reportFile = abdaudiolab::test::scratchDir("ScopeReport")
            .getChildFile("test_scope_warnings.html");
        if (reportFile.existsAsFile())
            reportFile.deleteFile();

        REQUIRE(ProfileValidationScopeReportExporter::exportReportToHtml(reportFile.getFullPathName().toStdString(), data));
        juce::String content = reportFile.loadFileAsString();
        REQUIRE(content.contains("badge-warn"));
        REQUIRE(content.contains("VALIDADO CON OBSERVACIONES"));
        reportFile.deleteFile();
    }

    SECTION("Status: Rejected")
    {
        auto data = buildMockScopeReportData(SelectionStatus::Rejected);
        juce::File reportFile = abdaudiolab::test::scratchDir("ScopeReport")
            .getChildFile("test_scope_rejected.html");
        if (reportFile.existsAsFile())
            reportFile.deleteFile();

        REQUIRE(ProfileValidationScopeReportExporter::exportReportToHtml(reportFile.getFullPathName().toStdString(), data));
        juce::String content = reportFile.loadFileAsString();
        REQUIRE(content.contains("badge-fail"));
        REQUIRE(content.contains("NO ACEPTADO"));
        reportFile.deleteFile();
    }

    SECTION("Empty Vectors Graceful Fallback")
    {
        auto data = buildMockScopeReportData(SelectionStatus::Accepted);
        data.frequencyBinsHz.clear();
        data.targetMagnitudeDb.clear();
        data.modelMagnitudeDb.clear();
        data.residualMagnitudeDb.clear();
        data.timeDomainResidual.clear();

        juce::File reportFile = abdaudiolab::test::scratchDir("ScopeReport")
            .getChildFile("test_scope_empty.html");
        if (reportFile.existsAsFile())
            reportFile.deleteFile();

        REQUIRE(ProfileValidationScopeReportExporter::exportReportToHtml(reportFile.getFullPathName().toStdString(), data));
        juce::String content = reportFile.loadFileAsString();
        REQUIRE(content.contains("<svg")); // Genera contenedor SVG con rejilla vacía sin crashear
        reportFile.deleteFile();
    }
}
