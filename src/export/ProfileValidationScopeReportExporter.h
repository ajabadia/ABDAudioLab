/**
 * @file ProfileValidationScopeReportExporter.h
 * @brief Standalone publication-grade HTML report generator for Profile Validation and Scope Declaration (Phase 19.5).
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <string>
#include <vector>
#include "../synth/ModelEvaluationTypes.h"
#include "../synth/MultiObjectiveModelSelector.h"
#include "../synth/HoldoutStimulusGenerator.h"

namespace abdaudiolab::exporting
{

/**
 * @struct DomainOfValidity
 * @brief Formal metrological boundaries defining the tested operating regime.
 */
struct DomainOfValidity
{
    double sampleRate { 96000.0 };
    double minInputLevelDbfs { -36.0 };
    double maxInputLevelDbfs { -3.0 };
    int warmupMinutes { 15 };
    std::string channelLayout { "Mono / Stereo" };
    std::string operatingTemperature { "20°C - 24°C" };
    std::string documentedControlRanges { "All mapped parameter steps [0..100%]" };
    std::string extrapolationWarning {
        "Modelo validado exclusivamente dentro del dominio dinámico, espectral y de control documentado. "
        "Cualquier excitación fuera de estos límites se considera extrapolación matemática no validada."
    };
};

/**
 * @struct HoldoutBatterySummary
 * @brief Acoustic and metrological summary of an out-of-sample holdout test sub-battery.
 */
struct HoldoutBatterySummary
{
    std::string batteryName;
    std::string description;
    synth::StimulusAcousticProperties acoustics;
};

/**
 * @struct ProfileScopeReportData
 * @brief Complete payload required to synthesize a publication-grade HTML/SVG validation report.
 */
struct ProfileScopeReportData
{
    std::string reportTitle { "INFORME DE VALIDACIÓN Y ALCANCE DEL PERFIL" };
    std::string targetName;
    std::string targetManufacturer { "Empirical Hardware" };
    std::string hardwareType { "Analog Non-Linear Processor" };
    std::string profileTimestampUtc;
    std::string operatorNotes;

    DomainOfValidity validityDomain;
    synth::ModelEvaluation evaluation;
    synth::CandidateBenchmarkingReport benchmarkingReport;

    // Métricas de calibración de interfaz de audio / loopback
    double loopbackSnrDb { 92.5 };
    double loopbackNoiseFloorDbfs { -96.0 };

    // Sub-baterías y auditoría de holdout
    std::vector<HoldoutBatterySummary> holdoutBatteries;
    std::string holdoutDatasetHash;
    bool leakageDetected { false };
    double leakageMinDistance { 2.45 };

    // Vectores opcionales para renderizar gráficos vectoriales SVG embebidos
    std::vector<float> frequencyBinsHz;
    std::vector<float> targetMagnitudeDb;
    std::vector<float> modelMagnitudeDb;
    std::vector<float> residualMagnitudeDb;
    std::vector<float> timeDomainResidual;
};

/**
 * @class ProfileValidationScopeReportExporter
 * @brief Exports rich, self-contained HTML reports with inline SVG charts declaring validity scope and model benchmarking.
 */
class ProfileValidationScopeReportExporter
{
public:
    ProfileValidationScopeReportExporter() = default;
    ~ProfileValidationScopeReportExporter() = default;

    /**
     * @brief Exports the complete validation and scope report to a standalone HTML file.
     * @param targetPath Target filesystem path (.html).
     * @param data Full report data payload.
     * @return true on success, false on I/O error.
     */
    static bool exportReportToHtml(const std::string& targetPath,
                                   const ProfileScopeReportData& data);

    /**
     * @brief Generates an inline SVG showing Target, Model, and Residual spectrum.
     */
    static std::string generateResidualSpectrumSvg(const std::vector<float>& freqsHz,
                                                   const std::vector<float>& targetMagDb,
                                                   const std::vector<float>& modelMagDb,
                                                   const std::vector<float>& residualMagDb,
                                                   int width = 760,
                                                   int height = 280);

    /**
     * @brief Generates an inline SVG waveform of the time-domain residual error e[n].
     */
    static std::string generateTimeDomainResidualSvg(const std::vector<float>& residualWaveform,
                                                     double sampleRate = 96000.0,
                                                     int width = 760,
                                                     int height = 160);

    /**
     * @brief Generates the multi-objective candidate comparison HTML table.
     */
    static std::string generateCandidateComparisonTableHtml(const synth::CandidateBenchmarkingReport& benchmarking,
                                                            const synth::ModelEvaluation& evaluation);

    /**
     * @brief Generates the out-of-sample holdout battery HTML table.
     */
    static std::string generateHoldoutBatteryTableHtml(const std::vector<HoldoutBatterySummary>& batteries,
                                                       bool leakageDetected,
                                                       double minDistance);

    /**
     * @brief Generates the Domain of Validity disclaimer card.
     */
    static std::string generateDomainOfValidityBoxHtml(const DomainOfValidity& domain);

    /**
     * @brief Generates the runtime compute and polyphony footprint estimation card.
     */
    static std::string generatePluginFootprintCardHtml(const synth::ModelEvaluation& evaluation,
                                                       double sampleRate = 96000.0);
};

} // namespace abdaudiolab::exporting
