/**
 * @file LoadedSessionApplier.cpp
 * @brief Implementation of LoadedSessionApplier presentation coordination.
 * @author ABDSynths
 * @date 2026
 */

#include "LoadedSessionApplier.h"
#include <algorithm>

namespace abdaudiolab::gui
{

namespace
{
void applyBadgeForStimulusType(gui::QueueItem& item, audio::StimulusType st)
{
    switch (st)
    {
        case audio::StimulusType::LogFarinaSweep:
            item.badgeText = "FLT";
            item.badgeColor = juce::Colour(0xFF388E3C);
            break;
        case audio::StimulusType::AmplitudeRamp:
            item.badgeText = "SAT";
            item.badgeColor = juce::Colour(0xFFE65100);
            break;
        case audio::StimulusType::SquareWave1kHz:
            item.badgeText = "ENV";
            item.badgeColor = juce::Colour(0xFF1976D2);
            break;
        case audio::StimulusType::PinkNoise:
        case audio::StimulusType::WhiteNoise:
            item.badgeText = "MOD";
            item.badgeColor = juce::Colour(0xFF7B1FA2);
            break;
        case audio::StimulusType::DiracDelta:
        case audio::StimulusType::SyncPulses3:
            item.badgeText = "WNH";
            item.badgeColor = juce::Colour(0xFF00796B);
            break;
        case audio::StimulusType::NamCalibration:
            item.badgeText = "NAM";
            item.badgeColor = juce::Colour(0xFFC2185B);
            break;
        default:
            item.badgeText = "STD";
            item.badgeColor = juce::Colour(0xFF455A64);
            break;
    }
}
} // namespace

std::vector<gui::QueueItem> LoadedSessionApplier::reconstructQueueItems(
    const std::vector<gui::TestConfiguration>& testConfigs,
    bool hasMeasuredPoints)
{
    std::vector<gui::QueueItem> items;
    items.reserve(testConfigs.size());

    int counter = 1;
    for (const auto& tc : testConfigs)
    {
        gui::QueueItem item;
        item.title = tc.testName;
        item.stimulusType = tc.stimulusType;
        item.burstDurationSec = tc.burstDurationSec;
        item.captureMode = tc.captureMode;
        item.controls = tc.controls;
        item.totalPoints = tc.getTotalMeasurementPoints();
        item.id = "restored_test_" + juce::String(counter++);
        item.status = hasMeasuredPoints ? gui::QueueItemStatus::Completed : gui::QueueItemStatus::Queued;

        applyBadgeForStimulusType(item, tc.stimulusType);
        items.push_back(item);
    }

    return items;
}

WorkflowStepState LoadedSessionApplier::computeWorkflowState(
    int totalMeasuredPointsInSession,
    size_t actualPointsCount)
{
    WorkflowStepState state;
    state.isSessionComplete = (!actualPointsCount == 0 &&
                               actualPointsCount >= static_cast<size_t>(totalMeasuredPointsInSession) &&
                               totalMeasuredPointsInSession > 0);

    if (state.isSessionComplete)
    {
        state.targetStepperStep = WorkflowStepperBar::Step::ExportReport;
        state.targetSidebarStep = SoundIdSidebarStepper::Step::ExportReport;
        state.runSessionStatus = WorkflowStepperBar::StepStatus::Completed;
    }
    else
    {
        state.targetStepperStep = WorkflowStepperBar::Step::RunSession;
        state.targetSidebarStep = SoundIdSidebarStepper::Step::RunSession;
        state.runSessionStatus = (actualPointsCount > 0)
            ? WorkflowStepperBar::StepStatus::Completed
            : WorkflowStepperBar::StepStatus::Current;
    }

    return state;
}

SessionApplicationResult LoadedSessionApplier::apply(
    const core::SessionManifest& manifest,
    const std::vector<exporting::MeasuredPoint>& points,
    bool hasHardwareContract,
    ILoadedSessionTarget& target)
{
    SessionApplicationResult result;

    // 1. Precondition validation before mutating any target
    if (manifest.formatVersion.empty() || manifest.hardwareDisplayName.empty())
    {
        result.status = SessionApplicationStatus::InvalidManifest;
        result.message = "Manifest formatVersion or hardwareDisplayName is invalid.";
        return result;
    }

    // 2. Prepare domain & presentation data
    SessionUiPresentationData presData;
    presData.hardwareId = juce::String(manifest.hardwareId);
    presData.hardwareDisplayName = juce::String(manifest.hardwareDisplayName);
    presData.activeFunctionId = juce::String(manifest.activeFunctionId);
    presData.activeFunctionName = juce::String(manifest.activeFunctionName);
    presData.targetModule = juce::String(manifest.targetModule);
    presData.operatorNotes = juce::String(manifest.operatorNotes);
    presData.ambientTemperatureC = manifest.ambientTemperatureC;
    presData.warmupTimeMinutes = manifest.warmupTimeMinutes;
    presData.hasValidHardwareContract = hasHardwareContract;

    // 3. Apply state in deterministic order
    // Step A: Session manager data
    target.setSessionData(manifest, points);

    // Step B: Plotter curves
    target.clearPlotterAndAddPoints(points);

    // Step C: Drawer & Environmental settings
    target.updateDrawerAndEnvironment(presData);

    // Step D: Hardware Routing & Panels
    target.updateHardwarePanels(presData);

    // Step E: Reconstruct test suite queue
    auto queueItems = reconstructQueueItems(manifest.tests, !points.empty());
    target.rebuildTestSuiteQueue({}, queueItems);

    // Step F: Workflow navigation & status
    auto wfState = computeWorkflowState(manifest.totalMeasuredPoints, points.size());
    target.updateWorkflowAndNavigation(wfState);

    result.status = SessionApplicationStatus::Success;
    result.pointsApplied = static_cast<int>(points.size());
    result.testsRestored = static_cast<int>(queueItems.size());
    result.message = "Session loaded: " + juce::String(manifest.hardwareDisplayName) +
                     " (" + juce::String(points.size()) + " points)";

    return result;
}

} // namespace abdaudiolab::gui
