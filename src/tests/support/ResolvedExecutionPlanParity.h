#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <sstream>
#include <iomanip>

#include "synth/ExperimentPlan.h"
#include "profiling/ExperimentPlanCompiler.h"
#include "core/ProfilingSession.h"

namespace abdaudiolab::test::support
{

struct ParityComparisonResult
{
    bool isEquivalent { true };
    std::string failureReason;
    std::string layer; // "experiment-plan", "target-event", "observation-window", "resolved-plan", "session", etc.
    size_t index { 0 };
    std::string field;
    std::string legacyValue;
    std::string declarativeValue;
    std::string diagnosticDetails;

    [[nodiscard]] std::string toString() const
    {
        if (isEquivalent)
            return "Parity check passed: Exact equivalence confirmed.";

        std::ostringstream oss;
        oss << "Parity failure:\n"
            << "  layer=" << layer << "\n";
        if (!field.empty())
            oss << "  field=" << field << "\n";
        if (layer == "target-event" || layer == "observation-window" || layer == "test-case" || layer == "parameter-step")
            oss << "  index=" << index << "\n";
        oss << "  legacy=" << legacyValue << "\n"
            << "  declarative=" << declarativeValue << "\n"
            << "  reason=" << failureReason << "\n";
        if (!diagnosticDetails.empty())
            oss << "  details=" << diagnosticDetails << "\n";
        return oss.str();
    }
};

ParityComparisonResult compareExperimentPlans(
    const synth::ExperimentPlan& legacy,
    const synth::ExperimentPlan& declarative);

ParityComparisonResult compareResolvedExecutionPlans(
    const profiling::ResolvedExecutionPlan& legacy,
    const profiling::ResolvedExecutionPlan& declarative);

ParityComparisonResult compareProfilingSessions(
    const core::ProfilingSession& legacy,
    const core::ProfilingSession& declarative);

void requireEquivalentExperimentPlans(
    const synth::ExperimentPlan& legacy,
    const synth::ExperimentPlan& declarative);

void requireEquivalentResolvedExecutionPlans(
    const profiling::ResolvedExecutionPlan& legacy,
    const profiling::ResolvedExecutionPlan& declarative);

void requireEquivalentProfilingSessions(
    const core::ProfilingSession& legacy,
    const core::ProfilingSession& declarative);

} // namespace abdaudiolab::test::support
