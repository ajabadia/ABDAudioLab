/**
 * @file ExportReadinessEvaluator.cpp
 * @brief Implementation of the pre-export validation gate and metadata generator (Phase 19.6).
 * @author ABDSynths
 * @date 2026
 */

#include "ExportReadinessEvaluator.h"
#include <sstream>
#include <iomanip>

namespace abdaudiolab::exporting
{

synth::ExportReadiness ExportReadinessEvaluator::evaluate(const synth::ModelEvaluation& evaluation,
                                                         bool allowUnvalidatedOverride)
{
    synth::ExportReadiness r;
    r.status = evaluation.decision.status;
    r.primaryModelId = evaluation.decision.recommendedModelId;
    r.canonicalHash = evaluation.canonicalEvaluationHash;

    // Certified limits string descriptors
    std::ostringstream ssSr;
    ssSr << std::fixed << std::setprecision(1) << evaluation.sampleRate << " Hz";
    r.certifiedLimits.push_back("Sample Rate: " + ssSr.str());

    std::ostringstream ssRange;
    ssRange << "Input Dynamic Range: [-36.0 dBFS, -3.0 dBFS]";
    r.certifiedLimits.push_back(ssRange.str());

    if (!evaluation.validatedParameters.empty())
    {
        std::string params = "Validated Parameters: ";
        for (size_t i = 0; i < evaluation.validatedParameters.size(); ++i)
        {
            if (i > 0) params += ", ";
            params += evaluation.validatedParameters[i];
        }
        r.certifiedLimits.push_back(params);
    }

    switch (evaluation.decision.status)
    {
        case synth::SelectionStatus::Accepted:
        {
            r.verdict = synth::ExportGateVerdict::Permitted;
            r.isExportPermitted = true;
            r.requiresOperatorOverride = false;
            r.verdictReason = "Model satisfies all metrological criteria (ESR, R2, stability, CPU) without warnings.";
            break;
        }

        case synth::SelectionStatus::AcceptedWithWarnings:
        {
            r.verdict = synth::ExportGateVerdict::PermittedWithWarning;
            r.isExportPermitted = true;
            r.requiresOperatorOverride = false;
            r.verdictReason = "Model accepted with documented metrological warnings or extrapolation domain constraints.";
            for (const auto& w : evaluation.warnings)
            {
                r.blockingIssues.push_back("Warning: " + w);
            }
            break;
        }

        case synth::SelectionStatus::Inconclusive:
        {
            r.verdict = synth::ExportGateVerdict::Blocked;
            r.isExportPermitted = allowUnvalidatedOverride;
            r.requiresOperatorOverride = true;
            r.verdictReason = "Model is inconclusive (runner-up gap within uncertainty margin or missing holdout).";
            r.blockingIssues.push_back("Inconclusive candidate differentiation");
            break;
        }

        case synth::SelectionStatus::Rejected:
        {
            r.verdict = synth::ExportGateVerdict::Blocked;
            r.isExportPermitted = allowUnvalidatedOverride;
            r.requiresOperatorOverride = true;
            r.verdictReason = "Model fails acceptance criteria (excessive ESR, instability, or target rejection).";
            r.blockingIssues.push_back("Model rejected by acceptance policy");
            for (const auto& w : evaluation.warnings)
            {
                r.blockingIssues.push_back("Failure cause: " + w);
            }
            break;
        }

        case synth::SelectionStatus::InvalidMeasurement:
        default:
        {
            r.verdict = synth::ExportGateVerdict::Blocked;
            r.isExportPermitted = false; // Never allowed to export invalid measurements
            r.requiresOperatorOverride = true;
            r.verdictReason = "Measurement corrupted, clipped, or desynchronized.";
            r.blockingIssues.push_back("Fatal measurement corruption or clip");
            break;
        }
    }

    return r;
}

std::string ExportReadinessEvaluator::generatePreprocessorGuards(const synth::ExportReadiness& readiness,
                                                                double sampleRate,
                                                                double minDbfs,
                                                                double maxDbfs)
{
    std::ostringstream out;

    out << "// --- ABDAudioLab Phase 19.6 Certified Dynamic Limits & Compile-Time Gates ---\n";
    out << "#define ABD_PROFILE_CERTIFIED_SAMPLE_RATE " << std::fixed << std::setprecision(1) << sampleRate << "\n";
    out << "#define ABD_PROFILE_MIN_INPUT_DBFS " << std::fixed << std::setprecision(1) << minDbfs << "\n";
    out << "#define ABD_PROFILE_MAX_INPUT_DBFS " << std::fixed << std::setprecision(1) << maxDbfs << "\n";
    out << "#define ABD_PROFILE_STATUS \"" << synth::selectionStatusToString(readiness.status) << "\"\n";
    out << "#define ABD_PROFILE_CANONICAL_HASH \"" << readiness.canonicalHash << "\"\n";

    if (readiness.verdict == synth::ExportGateVerdict::PermittedWithWarning)
    {
        out << "\n// Documented Metrological Warnings Guard\n";
        out << "#if defined(__GNUC__) || defined(__clang__)\n";
        out << "#pragma message \"[ABDAudioLab WARNING] Hardware profile has documented warnings or extrapolation boundaries.\"\n";
        out << "#elif defined(_MSC_VER)\n";
        out << "#pragma message(\"[ABDAudioLab WARNING] Hardware profile has documented warnings or extrapolation boundaries.\")\n";
        out << "#endif\n";
    }
    else if (readiness.verdict == synth::ExportGateVerdict::Blocked)
    {
        out << "\n// Metrological Validation Failure Guard (Strict Compile Block)\n";
        out << "#ifndef ABD_ALLOW_UNVALIDATED_PROFILE\n";
        out << "#error \"[ABDAudioLab FATAL] This model profile failed metrological validation (Status: "
            << synth::selectionStatusToString(readiness.status)
            << "). Define ABD_ALLOW_UNVALIDATED_PROFILE to bypass.\"\n";
        out << "#endif\n";
    }

    return out.str();
}

std::string ExportReadinessEvaluator::generateEmbeddedJsonBlock(const synth::ExportReadiness& readiness,
                                                               const synth::ModelEvaluation& evaluation)
{
    nlohmann::json root = toMetrologicalCertificationJson(readiness, evaluation);
    std::string serialized = root.dump(2);

    std::ostringstream out;
    out << "// ==============================================================================\n";
    out << "// METADATA (RFC 8785 Traceable Verification & Certified Operating Scope)\n";
    out << "// ==============================================================================\n";

    std::istringstream stream(serialized);
    std::string line;
    while (std::getline(stream, line))
    {
        out << "// " << line << "\n";
    }

    out << "// ==============================================================================\n";
    return out.str();
}

nlohmann::json ExportReadinessEvaluator::toMetrologicalCertificationJson(const synth::ExportReadiness& readiness,
                                                                       const synth::ModelEvaluation& evaluation)
{
    nlohmann::json root;
    root["schemaVersion"] = "1.0.0";
    root["verdict"] = synth::exportGateVerdictToString(readiness.verdict);
    root["status"] = synth::selectionStatusToString(readiness.status);
    root["isExportPermitted"] = readiness.isExportPermitted;
    root["verdictReason"] = readiness.verdictReason;
    root["canonicalEvaluationHash"] = readiness.canonicalHash;
    root["selectedModelId"] = readiness.primaryModelId;

    nlohmann::json limits;
    limits["sampleRateHz"] = evaluation.sampleRate;
    limits["minInputDbfs"] = -36.0;
    limits["maxInputDbfs"] = -3.0;
    limits["warmupTimeMinutes"] = 15;
    limits["extrapolationWarning"] =
        "Validated strictly within documented boundaries. Excitations outside constitute unverified mathematical extrapolation.";
    root["domainOfValidity"] = limits;

    nlohmann::json metrics;
    metrics["rSquared"] = evaluation.metrics.rSquaredScore;
    metrics["errorToSignalRatioDb"] = evaluation.metrics.errorToSignalRatioDb;
    metrics["rootMeanSquareError"] = evaluation.metrics.rootMeanSquareError;
    metrics["peakAutocorrelation"] = evaluation.diagnostics.peakAutocorrelation;
    metrics["combinedUncertaintyDb"] = evaluation.uncertainty.combinedUncertaintyDb;
    root["metrics"] = metrics;

    if (!readiness.blockingIssues.empty())
    {
        root["issues"] = readiness.blockingIssues;
    }

    return root;
}

} // namespace abdaudiolab::exporting
