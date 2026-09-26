#include "ResolvedExecutionPlanParity.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

namespace abdaudiolab::test::support
{

namespace
{

bool approxEqual(double a, double b, double epsilon = 1e-4) noexcept
{
    return std::abs(a - b) <= epsilon;
}

} // namespace

ParityComparisonResult compareExperimentPlans(
    const synth::ExperimentPlan& legacy,
    const synth::ExperimentPlan& declarative)
{
    ParityComparisonResult r;

    if (legacy.recipeId != declarative.recipeId)
    {
        r.isEquivalent = false;
        r.layer = "experiment-plan";
        r.field = "recipeId";
        r.legacyValue = legacy.recipeId;
        r.declarativeValue = declarative.recipeId;
        r.failureReason = "Recipe identifier mismatch";
        return r;
    }

    if (!approxEqual(legacy.sampleRate, declarative.sampleRate))
    {
        r.isEquivalent = false;
        r.layer = "experiment-plan";
        r.field = "sampleRate";
        r.legacyValue = std::to_string(legacy.sampleRate);
        r.declarativeValue = std::to_string(declarative.sampleRate);
        r.failureReason = "Sample rate mismatch";
        return r;
    }

    if (legacy.windows.size() != declarative.windows.size())
    {
        r.isEquivalent = false;
        r.layer = "experiment-plan";
        r.field = "windows.size";
        r.legacyValue = std::to_string(legacy.windows.size());
        r.declarativeValue = std::to_string(declarative.windows.size());
        r.failureReason = "Window count mismatch";
        return r;
    }

    for (size_t i = 0; i < legacy.windows.size(); ++i)
    {
        const auto& wL = legacy.windows[i];
        const auto& wD = declarative.windows[i];

        if (wL.windowId != wD.windowId)
        {
            r.isEquivalent = false;
            r.layer = "observation-window";
            r.index = i;
            r.field = "windowId";
            r.legacyValue = wL.windowId;
            r.declarativeValue = wD.windowId;
            r.failureReason = "Window ID mismatch";
            return r;
        }

        if (wL.startSample != wD.startSample)
        {
            r.isEquivalent = false;
            r.layer = "observation-window";
            r.index = i;
            r.field = "startSample";
            r.legacyValue = std::to_string(wL.startSample);
            r.declarativeValue = std::to_string(wD.startSample);
            r.failureReason = "Observation window startSample mismatch";
            r.diagnosticDetails = "delta=" + std::to_string(wD.startSample - wL.startSample) + " samples";
            return r;
        }

        if (wL.endSample != wD.endSample)
        {
            r.isEquivalent = false;
            r.layer = "observation-window";
            r.index = i;
            r.field = "endSample";
            r.legacyValue = std::to_string(wL.endSample);
            r.declarativeValue = std::to_string(wD.endSample);
            r.failureReason = "Observation window endSample mismatch";
            r.diagnosticDetails = "delta=" + std::to_string(wD.endSample - wL.endSample) + " samples";
            return r;
        }

        if (!approxEqual(wL.durationMs, wD.durationMs))
        {
            r.isEquivalent = false;
            r.layer = "observation-window";
            r.index = i;
            r.field = "durationMs";
            r.legacyValue = std::to_string(wL.durationMs);
            r.declarativeValue = std::to_string(wD.durationMs);
            r.failureReason = "Window durationMs mismatch";
            return r;
        }

        if (wL.targetParameterId != wD.targetParameterId)
        {
            r.isEquivalent = false;
            r.layer = "observation-window";
            r.index = i;
            r.field = "targetParameterId";
            r.legacyValue = wL.targetParameterId;
            r.declarativeValue = wD.targetParameterId;
            r.failureReason = "Target parameter identifier mismatch in window";
            return r;
        }

        if (wL.domain != wD.domain)
        {
            r.isEquivalent = false;
            r.layer = "observation-window";
            r.index = i;
            r.field = "domain";
            r.legacyValue = wL.domain;
            r.declarativeValue = wD.domain;
            r.failureReason = "Observation domain mismatch";
            return r;
        }
    }

    if (legacy.events.size() != declarative.events.size())
    {
        r.isEquivalent = false;
        r.layer = "experiment-plan";
        r.field = "events.size";
        r.legacyValue = std::to_string(legacy.events.size());
        r.declarativeValue = std::to_string(declarative.events.size());
        r.failureReason = "Target event count mismatch";
        return r;
    }

    for (size_t i = 0; i < legacy.events.size(); ++i)
    {
        const auto& eL = legacy.events[i];
        const auto& eD = declarative.events[i];

        if (eL.eventType != eD.eventType)
        {
            r.isEquivalent = false;
            r.layer = "target-event";
            r.index = i;
            r.field = "eventType";
            r.legacyValue = (eL.eventType == synth::TargetEventType::Midi ? "Midi" : "Parameter");
            r.declarativeValue = (eD.eventType == synth::TargetEventType::Midi ? "Midi" : "Parameter");
            r.failureReason = "Target event type mismatch";
            return r;
        }

        if (eL.absoluteSample != eD.absoluteSample)
        {
            r.isEquivalent = false;
            r.layer = "target-event";
            r.index = i;
            r.field = "absoluteSample";
            r.legacyValue = std::to_string(eL.absoluteSample);
            r.declarativeValue = std::to_string(eD.absoluteSample);
            r.failureReason = "Target event absoluteSample mismatch";
            r.diagnosticDetails = "delta=" + std::to_string(eD.absoluteSample - eL.absoluteSample) + " samples";
            return r;
        }

        if (eL.eventType == synth::TargetEventType::Midi)
        {
            if (eL.midi.type != eD.midi.type)
            {
                r.isEquivalent = false;
                r.layer = "target-event";
                r.index = i;
                r.field = "midi.type";
                r.legacyValue = std::to_string(static_cast<int>(eL.midi.type));
                r.declarativeValue = std::to_string(static_cast<int>(eD.midi.type));
                r.failureReason = "MIDI event type mismatch";
                return r;
            }
            if (eL.midi.noteNumber != eD.midi.noteNumber)
            {
                r.isEquivalent = false;
                r.layer = "target-event";
                r.index = i;
                r.field = "midi.noteNumber";
                r.legacyValue = std::to_string(eL.midi.noteNumber);
                r.declarativeValue = std::to_string(eD.midi.noteNumber);
                r.failureReason = "MIDI note number mismatch";
                return r;
            }
            if (!approxEqual(eL.midi.velocity, eD.midi.velocity))
            {
                r.isEquivalent = false;
                r.layer = "target-event";
                r.index = i;
                r.field = "midi.velocity";
                r.legacyValue = std::to_string(eL.midi.velocity);
                r.declarativeValue = std::to_string(eD.midi.velocity);
                r.failureReason = "MIDI velocity mismatch";
                return r;
            }
        }
        else
        {
            if (eL.parameter.normalizedParameterId != eD.parameter.normalizedParameterId)
            {
                r.isEquivalent = false;
                r.layer = "target-event";
                r.index = i;
                r.field = "parameter.normalizedParameterId";
                r.legacyValue = eL.parameter.normalizedParameterId;
                r.declarativeValue = eD.parameter.normalizedParameterId;
                r.failureReason = "Normalized parameter ID mismatch";
                return r;
            }
            if (!approxEqual(eL.parameter.normalizedValue, eD.parameter.normalizedValue))
            {
                r.isEquivalent = false;
                r.layer = "target-event";
                r.index = i;
                r.field = "parameter.normalizedValue";
                r.legacyValue = std::to_string(eL.parameter.normalizedValue);
                r.declarativeValue = std::to_string(eD.parameter.normalizedValue);
                r.failureReason = "Parameter normalized value mismatch";
                return r;
            }
        }
    }

    if (legacy.planHash != declarative.planHash)
    {
        r.isEquivalent = false;
        r.layer = "experiment-plan";
        r.field = "planHash";
        r.legacyValue = legacy.planHash;
        r.declarativeValue = declarative.planHash;
        r.failureReason = "Canonical ExperimentPlan hash mismatch";
        return r;
    }

    return r;
}

ParityComparisonResult compareResolvedExecutionPlans(
    const profiling::ResolvedExecutionPlan& legacy,
    const profiling::ResolvedExecutionPlan& declarative)
{
    // 1. Comparar plan base
    auto planRes = compareExperimentPlans(legacy.experimentPlan, declarative.experimentPlan);
    if (!planRes.isEquivalent)
        return planRes;

    ParityComparisonResult r;

    if (legacy.totalSamples != declarative.totalSamples)
    {
        r.isEquivalent = false;
        r.layer = "resolved-plan";
        r.field = "totalSamples";
        r.legacyValue = std::to_string(legacy.totalSamples);
        r.declarativeValue = std::to_string(declarative.totalSamples);
        r.failureReason = "Total sample count mismatch";
        return r;
    }

    if (!approxEqual(legacy.totalDurationSec, declarative.totalDurationSec))
    {
        r.isEquivalent = false;
        r.layer = "resolved-plan";
        r.field = "totalDurationSec";
        r.legacyValue = std::to_string(legacy.totalDurationSec);
        r.declarativeValue = std::to_string(declarative.totalDurationSec);
        r.failureReason = "Total duration mismatch";
        return r;
    }

    if (legacy.resolvedExecutionPlanHash != declarative.resolvedExecutionPlanHash)
    {
        r.isEquivalent = false;
        r.layer = "resolved-plan";
        r.field = "resolvedExecutionPlanHash";
        r.legacyValue = legacy.resolvedExecutionPlanHash;
        r.declarativeValue = declarative.resolvedExecutionPlanHash;
        r.failureReason = "Resolved execution plan hash mismatch";
        return r;
    }

    return r;
}

ParityComparisonResult compareProfilingSessions(
    const core::ProfilingSession& legacy,
    const core::ProfilingSession& declarative)
{
    ParityComparisonResult r;

    if (legacy.getTestCases().size() != declarative.getTestCases().size())
    {
        r.isEquivalent = false;
        r.layer = "session";
        r.field = "testCases.size";
        r.legacyValue = std::to_string(legacy.getTestCases().size());
        r.declarativeValue = std::to_string(declarative.getTestCases().size());
        r.failureReason = "TestCase count mismatch in ProfilingSession";
        return r;
    }

    for (size_t i = 0; i < legacy.getTestCases().size(); ++i)
    {
        const auto& tcL = legacy.getTestCases()[i];
        const auto& tcD = declarative.getTestCases()[i];

        if (tcL.testId != tcD.testId)
        {
            r.isEquivalent = false;
            r.layer = "test-case";
            r.index = i;
            r.field = "testId";
            r.legacyValue = tcL.testId;
            r.declarativeValue = tcD.testId;
            r.failureReason = "TestCase ID mismatch";
            return r;
        }

        if (tcL.midiNoteNumber != tcD.midiNoteNumber)
        {
            r.isEquivalent = false;
            r.layer = "test-case";
            r.index = i;
            r.field = "midiNoteNumber";
            r.legacyValue = std::to_string(tcL.midiNoteNumber);
            r.declarativeValue = std::to_string(tcD.midiNoteNumber);
            r.failureReason = "TestCase MIDI note number mismatch";
            return r;
        }

        if (!approxEqual(tcL.midiVelocity, tcD.midiVelocity))
        {
            r.isEquivalent = false;
            r.layer = "test-case";
            r.index = i;
            r.field = "midiVelocity";
            r.legacyValue = std::to_string(tcL.midiVelocity);
            r.declarativeValue = std::to_string(tcD.midiVelocity);
            r.failureReason = "TestCase MIDI velocity mismatch";
            return r;
        }

        if (!approxEqual(tcL.noteGateDurationSec, tcD.noteGateDurationSec))
        {
            r.isEquivalent = false;
            r.layer = "test-case";
            r.index = i;
            r.field = "noteGateDurationSec";
            r.legacyValue = std::to_string(tcL.noteGateDurationSec);
            r.declarativeValue = std::to_string(tcD.noteGateDurationSec);
            r.failureReason = "TestCase gate duration mismatch";
            return r;
        }

        if (tcL.parameterSteps.size() != tcD.parameterSteps.size())
        {
            r.isEquivalent = false;
            r.layer = "test-case";
            r.index = i;
            r.field = "parameterSteps.size";
            r.legacyValue = std::to_string(tcL.parameterSteps.size());
            r.declarativeValue = std::to_string(tcD.parameterSteps.size());
            r.failureReason = "ParameterStep count mismatch";
            return r;
        }

        for (size_t s = 0; s < tcL.parameterSteps.size(); ++s)
        {
            const auto& stepL = tcL.parameterSteps[s];
            const auto& stepD = tcD.parameterSteps[s];

            if (stepL.paramName != stepD.paramName)
            {
                r.isEquivalent = false;
                r.layer = "parameter-step";
                r.index = s;
                r.field = "paramName";
                r.legacyValue = stepL.paramName;
                r.declarativeValue = stepD.paramName;
                r.failureReason = "ParameterStep paramName mismatch";
                return r;
            }

            if (!approxEqual(stepL.normalizedValue, stepD.normalizedValue))
            {
                r.isEquivalent = false;
                r.layer = "parameter-step";
                r.index = s;
                r.field = "normalizedValue";
                r.legacyValue = std::to_string(stepL.normalizedValue);
                r.declarativeValue = std::to_string(stepD.normalizedValue);
                r.failureReason = "ParameterStep normalized value mismatch";
                return r;
            }
        }
    }

    return r;
}

void requireEquivalentExperimentPlans(
    const synth::ExperimentPlan& legacy,
    const synth::ExperimentPlan& declarative)
{
    auto res = compareExperimentPlans(legacy, declarative);
    INFO(res.toString());
    REQUIRE(res.isEquivalent);
}

void requireEquivalentResolvedExecutionPlans(
    const profiling::ResolvedExecutionPlan& legacy,
    const profiling::ResolvedExecutionPlan& declarative)
{
    auto res = compareResolvedExecutionPlans(legacy, declarative);
    INFO(res.toString());
    REQUIRE(res.isEquivalent);
}

void requireEquivalentProfilingSessions(
    const core::ProfilingSession& legacy,
    const core::ProfilingSession& declarative)
{
    auto res = compareProfilingSessions(legacy, declarative);
    INFO(res.toString());
    REQUIRE(res.isEquivalent);
}

} // namespace abdaudiolab::test::support
