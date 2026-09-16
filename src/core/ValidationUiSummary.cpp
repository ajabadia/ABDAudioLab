/**
 * @file ValidationUiSummary.cpp
 * @brief Implementation of ValidationUiSummary parser and factory functions.
 * @author ABDSynths
 * @date 2026
 */

#include "ValidationUiSummary.h"
#include "ModelHoldoutValidator.h"
#include "ExperimentStorage.h"
#include <nlohmann/json.hpp>

namespace abdaudiolab::core
{

juce::String ValidationUiSummary::statusToString(Status s)
{
    switch (s)
    {
        case Status::completed:   return "COMPLETED";
        case Status::error:       return "ERROR";
        case Status::corrupt:     return "CORRUPT";
        case Status::notExecuted: return "NOT_EXECUTED";
    }
    return "NOT_EXECUTED";
}

juce::String ValidationUiSummary::verdictToString(Verdict v)
{
    switch (v)
    {
        case Verdict::pass:                return "PASS";
        case Verdict::passWithLimitations: return "PASS_WITH_LIMITATIONS";
        case Verdict::fail:                return "FAIL";
        case Verdict::notAvailable:        return "NOT_AVAILABLE";
    }
    return "NOT_AVAILABLE";
}

ValidationUiSummary ValidationUiSummary::fromValidationReport(const ValidationReport& report,
                                                              const juce::File& experimentDir,
                                                              bool integrityVerified)
{
    ValidationUiSummary summary;
    summary.integrityVerified = integrityVerified;

    if (!integrityVerified)
    {
        summary.status = Status::corrupt;
        summary.verdict = Verdict::notAvailable;
        summary.reason = "Cryptographic mismatch or tampering detected in experiment container";
        return summary;
    }

    // Map verdict
    if (report.verdict == "PASS")
    {
        summary.status = Status::completed;
        summary.verdict = Verdict::pass;
    }
    else if (report.verdict == "PASS_WITH_LIMITATIONS")
    {
        summary.status = Status::completed;
        summary.verdict = Verdict::passWithLimitations;
    }
    else if (report.verdict == "FAIL")
    {
        summary.status = Status::completed;
        summary.verdict = Verdict::fail;
    }
    else
    {
        summary.status = Status::error;
        summary.verdict = Verdict::notAvailable;
    }

    summary.policy = report.verdictPolicy.empty() ? "audio-ab-v1" : juce::String(report.verdictPolicy);
    summary.reason = juce::String(report.reasonCode);
    summary.esrDb = static_cast<double>(report.postAlignment.esrDb);
    summary.correlation = static_cast<double>(report.postAlignment.correlationPeak);
    summary.sampleOffset = report.sampleOffset;

    if (experimentDir.isDirectory())
    {
        juce::File valDir = experimentDir.getChildFile("validation");
        summary.targetFile = valDir.getChildFile("target.wav");
        summary.modelFile = valDir.getChildFile("model.wav");
        summary.residualFile = valDir.getChildFile("residual.wav");

        summary.targetAvailable = summary.targetFile.existsAsFile();
        summary.modelAvailable = summary.modelFile.existsAsFile();
        summary.residualAvailable = summary.residualFile.existsAsFile();

        juce::File repDir = experimentDir.getChildFile("reports");
        summary.htmlReportFile = repDir.getChildFile("certification_report.html");
        summary.htmlReportAvailable = summary.htmlReportFile.existsAsFile();
    }

    return summary;
}

ValidationUiSummary ValidationUiSummary::fromExperimentFolder(const juce::File& experimentDir)
{
    ValidationUiSummary summary;

    // 1. manifest.json existe
    juce::File manifestFile = experimentDir.getChildFile("manifest.json");
    if (!manifestFile.existsAsFile())
    {
        summary.status = Status::notExecuted;
        summary.verdict = Verdict::notAvailable;
        summary.reason = "manifest.json does not exist";
        return summary;
    }

    // 2. manifest es parseable
    nlohmann::json manifestJson;
    try
    {
        manifestJson = nlohmann::json::parse(manifestFile.loadFileAsString().toStdString());
    }
    catch (...)
    {
        summary.status = Status::corrupt;
        summary.verdict = Verdict::notAvailable;
        summary.reason = "manifest.json is unparseable";
        summary.integrityVerified = false;
        return summary;
    }

    // 3. Experimento no está corrupto (verificación bit a bit de todos los artefactos indexados)
    juce::String expError;
    auto expRecord = ExperimentStorage::loadExperiment(experimentDir, expError);
    if (!expRecord.has_value() || expRecord->isCorrupt())
    {
        summary.status = Status::corrupt;
        summary.verdict = Verdict::notAvailable;
        summary.reason = expError.isNotEmpty() ? expError : "Experiment failed cryptographic integrity verification";
        summary.integrityVerified = false;
        return summary;
    }

    summary.integrityVerified = true;

    // 4. validation_report.json está indexado en manifest
    std::string reportShaDeclared;
    bool reportIndexed = false;
    if (manifestJson.contains("artifacts") && manifestJson["artifacts"].is_array())
    {
        for (const auto& item : manifestJson["artifacts"])
        {
            if (item.value("path", "") == "validation/validation_report.json")
            {
                reportIndexed = true;
                reportShaDeclared = item.value("sha256", "");
                break;
            }
        }
    }

    juce::File reportFile = experimentDir.getChildFile("validation").getChildFile("validation_report.json");
    if (!reportIndexed || !reportFile.existsAsFile())
    {
        // Experimento válido pero sin validación holdout out-of-sample (ej. legado o sólo entrenamiento)
        summary.status = Status::notExecuted;
        summary.verdict = Verdict::notAvailable;
        summary.reason = "Validation report not present in experiment";

        // Comprobar si al menos existe el informe HTML
        juce::File htmlFile = experimentDir.getChildFile("reports").getChildFile("certification_report.html");
        if (htmlFile.existsAsFile())
        {
            summary.htmlReportFile = htmlFile;
            summary.htmlReportAvailable = true;
        }
        return summary;
    }

    // 5. SHA-256 del reporte coincide
    std::string actualReportSha = ExperimentStorage::computeFileSha256(reportFile);
    if (actualReportSha != reportShaDeclared)
    {
        summary.status = Status::corrupt;
        summary.verdict = Verdict::notAvailable;
        summary.reason = "Cryptographic mismatch for validation/validation_report.json";
        summary.integrityVerified = false;
        return summary;
    }

    // 6. Parsear validation_report.json
    nlohmann::json repJson;
    try
    {
        repJson = nlohmann::json::parse(reportFile.loadFileAsString().toStdString());
    }
    catch (...)
    {
        summary.status = Status::corrupt;
        summary.verdict = Verdict::notAvailable;
        summary.reason = "validation_report.json is malformed";
        return summary;
    }

    // 7. schemaVersion es compatible
    std::string schemaVer = repJson.value("schemaVersion", "");
    if (schemaVer.empty())
    {
        // Backward-compatibility: si no tiene schemaVersion pero tiene reportId, aceptar
        if (!repJson.contains("reportId"))
        {
            summary.status = Status::error;
            summary.verdict = Verdict::notAvailable;
            summary.errorCode = "MISSING_SCHEMA_VERSION";
            summary.reason = "Missing schemaVersion in validation_report.json";
            return summary;
        }
    }
    else if (schemaVer.rfind("audio-validation-report-1.", 0) != 0)
    {
        summary.status = Status::error;
        summary.verdict = Verdict::notAvailable;
        summary.errorCode = "UNSUPPORTED_SCHEMA_VERSION";
        summary.reason = "Unsupported schemaVersion: " + schemaVer;
        return summary;
    }

    // 8. Leer status y verdict
    std::string stStr = repJson.value("status", "completed");
    if (stStr == "completed")
        summary.status = Status::completed;
    else if (stStr == "corrupt")
        summary.status = Status::corrupt;
    else if (stStr == "notExecuted" || stStr == "not_executed")
        summary.status = Status::notExecuted;
    else
        summary.status = Status::error;

    std::string verdCode;
    if (repJson.contains("verdict") && repJson["verdict"].is_object())
    {
        verdCode = repJson["verdict"].value("code", "");
        summary.policy = juce::String(repJson["verdict"].value("policy", "audio-ab-v1"));
        summary.reason = juce::String(repJson["verdict"].value("reason", ""));
    }
    else
    {
        verdCode = repJson.value("verdict", "");
        summary.policy = juce::String(repJson.value("verdictPolicy", "audio-ab-v1"));
        summary.reason = juce::String(repJson.value("reasonCode", ""));
    }

    if (verdCode == "PASS")
        summary.verdict = Verdict::pass;
    else if (verdCode == "PASS_WITH_LIMITATIONS")
        summary.verdict = Verdict::passWithLimitations;
    else if (verdCode == "FAIL")
        summary.verdict = Verdict::fail;
    else
        summary.verdict = Verdict::notAvailable;

    if (repJson.contains("error") && repJson["error"].is_object())
    {
        summary.errorCode = juce::String(repJson["error"].value("code", ""));
        if (summary.reason.isEmpty())
            summary.reason = juce::String(repJson["error"].value("message", ""));
    }

    // Métricas
    if (repJson.contains("metrics") && repJson["metrics"].is_object())
    {
        const auto& m = repJson["metrics"];
        if (m.contains("postAlignment") && m["postAlignment"].is_object())
        {
            summary.esrDb = m["postAlignment"].value("esrDb", 0.0);
            if (m["postAlignment"].contains("correlationPeak"))
                summary.correlation = m["postAlignment"].value("correlationPeak", 0.0);
            else
                summary.correlation = m["postAlignment"].value("correlation", 0.0);
        }
        if (m.contains("alignment") && m["alignment"].is_object())
        {
            summary.sampleOffset = m["alignment"].value("sampleOffset", 0);
        }
    }
    else
    {
        // Flat legacy format fallback
        if (repJson.contains("postAlignment") && repJson["postAlignment"].is_object())
        {
            summary.esrDb = repJson["postAlignment"].value("esrDb", 0.0);
            summary.correlation = repJson["postAlignment"].value("correlationPeak", 0.0);
        }
        summary.sampleOffset = repJson.value("sampleOffset", 0);
    }

    // 9. Resolver y verificar límites de rutas dentro del contenedor
    auto checkSafeArtifact = [&](const std::string& relPath, juce::File& outFile, bool& outAvail) {
        if (relPath.empty() || relPath.find("..") != std::string::npos)
        {
            outAvail = false;
            return;
        }
        juce::File f = experimentDir.getChildFile(juce::String(relPath));
        if (f.isAChildOf(experimentDir) && f.existsAsFile())
        {
            outFile = f;
            outAvail = true;
        }
        else
        {
            outAvail = false;
        }
    };

    checkSafeArtifact("validation/target.wav", summary.targetFile, summary.targetAvailable);
    checkSafeArtifact("validation/model.wav", summary.modelFile, summary.modelAvailable);
    checkSafeArtifact("validation/residual.wav", summary.residualFile, summary.residualAvailable);
    checkSafeArtifact("reports/certification_report.html", summary.htmlReportFile, summary.htmlReportAvailable);

    return summary;
}

} // namespace abdaudiolab::core
