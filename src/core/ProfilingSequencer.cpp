#include "ProfilingSequencer.h"
#include "../math/LabAnalyticEngine.h"
#include <chrono>
#include <thread>

namespace abdaudiolab::core
{

ProfilingSequencer::ProfilingSequencer(audio::LabAudioEngine& engine,
                                       hardware::IHardwareController& hw)
    : juce::Thread("ProfilingSequencerThread"),
      audioEngine(engine),
      hardware(hw)
{
}

ProfilingSequencer::~ProfilingSequencer()
{
    stopSession();
}

bool ProfilingSequencer::startSession(const ProfilingSession& session,
                                      const juce::File& outputDirectory,
                                      const juce::String& baseExportName)
{
    if (isThreadRunning())
        return false;

    activeSession = session;
    exportDir = outputDirectory;
    exportBaseName = baseExportName;
    measuredPoints.clear();

    exportDir.createDirectory();
    startThread();
    return true;
}

void ProfilingSequencer::stopSession()
{
    signalThreadShouldExit();
    stopThread(4000);
    currentState.store(SequencerState::Idle, std::memory_order_release);
}

void ProfilingSequencer::confirmOperatorStep()
{
    operatorConfirmed.store(true, std::memory_order_release);
}

void ProfilingSequencer::notifyProgress(float progress, const juce::String& task, SequencerState state)
{
    currentState.store(state, std::memory_order_release);
    if (progressCallback)
    {
        juce::MessageManager::callAsync([cb = progressCallback, progress, task, state]() {
            cb(progress, task, state);
        });
    }
}

void ProfilingSequencer::run()
{
    const auto& testCases = activeSession.getTestCases();
    const size_t totalTests = testCases.size();
    if (totalTests == 0)
    {
        notifyProgress(1.0f, "No test cases found.", SequencerState::Finished);
        return;
    }

    double sampleRate = audioEngine.getCurrentSampleRate();
    auto& generator = audioEngine.getGenerator();
    auto& receiver = audioEngine.getReceiver();

    // 1. Line Calibration (Loopback baseline)
    notifyProgress(0.02f, "Performing Line Calibration (Noise & Loopback Check)...", SequencerState::LineCalibration);
    generator.setStimulus(audio::StimulusType::Silence, 0.5);
    receiver.armContinuousCapture(static_cast<int>(sampleRate * 0.5));

    while (!receiver.isFinished() && !threadShouldExit())
    {
        juce::Thread::sleep(10);
    }
    if (threadShouldExit()) return;

    // 2. Iterate through test cases
    for (size_t i = 0; i < totalTests; ++i)
    {
        if (threadShouldExit()) return;

        const auto& tc = testCases[i];
        float progress = static_cast<float>(i) / static_cast<float>(totalTests);
        juce::String taskMsg = "Executing Test " + juce::String(i + 1) + "/" + juce::String(totalTests) + " (" + tc.testId + ")";
        notifyProgress(progress, taskMsg, SequencerState::InitiateTestCase);

        // Configure hardware parameters
        for (const auto& step : tc.parameterSteps)
        {
            hardware.setParameter(step.paramIndex, step.normalizedValue);
        }

        // If manual gear, wait for operator confirmation
        if (!hardware.isAutomatic())
        {
            operatorConfirmed.store(false, std::memory_order_release);
            notifyProgress(progress, "Waiting for operator to adjust controls...", SequencerState::WaitingForOperator);

            while (!operatorConfirmed.load(std::memory_order_acquire) && !threadShouldExit())
            {
                juce::Thread::sleep(50);
            }
            if (threadShouldExit()) return;
        }

        // Wait for electronic/parameter stabilization
        notifyProgress(progress, "Stabilizing...", SequencerState::WaitForStabilization);
        juce::Thread::sleep(static_cast<int>(tc.stabilizationWaitMs));

        // Generate inverse filter if Farina sweep
        std::vector<float> invFilter;
        if (tc.stimulusType == audio::StimulusType::LogFarinaSweep)
        {
            invFilter = math::FarinaDeconvolver::generateInverseFilter(sampleRate, tc.stimulusDurationSec, tc.startFreqHz, tc.endFreqHz);
        }

        // Perform multiple measurement passes
        std::vector<std::vector<float>> recordedPasses;
        recordedPasses.reserve(static_cast<size_t>(tc.numPasses));

        for (int p = 0; p < tc.numPasses; ++p)
        {
            if (threadShouldExit()) return;

            notifyProgress(progress, taskMsg + " [Pass " + juce::String(p + 1) + "/" + juce::String(tc.numPasses) + "]", SequencerState::InjectStimulus);

            int samplesToRecord = static_cast<int>(std::lround((tc.stimulusDurationSec + 0.5) * sampleRate));
            receiver.armCapture(samplesToRecord, 0.005f); // -46 dBfs trigger
            generator.setStimulus(tc.stimulusType, tc.stimulusDurationSec, tc.startFreqHz, tc.endFreqHz);

            while (!receiver.isFinished() && !threadShouldExit())
            {
                juce::Thread::sleep(10);
            }
            if (threadShouldExit()) return;

            std::vector<float> recordedData;
            if (receiver.retrieveRecordedData(recordedData))
            {
                recordedPasses.push_back(std::move(recordedData));
            }
        }

        // Process analytical data
        notifyProgress(progress, "Analyzing Data...", SequencerState::CaptureAndAnalyze);

        exporting::MeasuredPoint pt;
        pt.testId = tc.testId;
        if (!tc.parameterSteps.empty())
            pt.param1Normalized = tc.parameterSteps[0].normalizedValue;
        if (tc.parameterSteps.size() > 1)
            pt.param2Normalized = tc.parameterSteps[1].normalizedValue;

        if (tc.functionalBlockType == "SpectrumFilter")
        {
            auto filterRes = math::LabAnalyticEngine::analyzeFilterPasses(recordedPasses, invFilter, sampleRate, tc.stimulusDurationSec, tc.startFreqHz, tc.endFreqHz);
            pt.muSigmaValue = filterRes.cutoffHz;
            pt.secondaryValue = filterRes.resonanceDb;
            pt.thdValue = filterRes.thdPercent;
        }
        else if (tc.functionalBlockType == "TimeDynamic")
        {
            auto timeRes = math::LabAnalyticEngine::analyzeAdsrEnvelopes(recordedPasses, sampleRate);
            pt.muSigmaValue = timeRes.attackTimeMs;
            pt.secondaryValue = timeRes.sustainLevel;
            pt.thdValue = { 0.0f, 0.0f };
        }
        else if (tc.functionalBlockType == "WaveShaper")
        {
            auto wsRes = math::LabAnalyticEngine::analyzeWaveShaperRamps(recordedPasses, sampleRate);
            pt.muSigmaValue = wsRes.thdPercent;
            pt.secondaryValue = { 1.0f, 0.0f };
            pt.thdValue = wsRes.thdPercent;
        }
        else
        {
            auto gainRes = math::LabAnalyticEngine::analyzeGainTones(recordedPasses, sampleRate);
            pt.muSigmaValue = gainRes.gainDb;
            pt.secondaryValue = gainRes.snrDb;
            pt.thdValue = { 0.0f, 0.0f };
        }

        measuredPoints.push_back(pt);

        // Periodic noise floor check every 10 test cases
        if ((i + 1) % 10 == 0)
        {
            notifyProgress(progress, "Interlude: Noise Floor Check...", SequencerState::InterludeNoiseFloor);
            generator.setStimulus(audio::StimulusType::Silence, 0.2);
            juce::Thread::sleep(250);
        }
    }

    // 3. Export Data and Cleanup
    notifyProgress(0.98f, "Exporting Look-Up Tables and JSON Report...", SequencerState::ExportDataAndCleanup);

    juce::File headerFile = exportDir.getChildFile(exportBaseName + "_LUT.h");
    juce::File jsonFile = exportDir.getChildFile(exportBaseName + "_Report.json");

    exporting::LutExporter::exportToCppHeader(headerFile.getFullPathName().toStdString(),
                                              activeSession.getMetadata(),
                                              exportBaseName.toStdString(),
                                              measuredPoints);

    exporting::LutExporter::exportToJsonReport(jsonFile.getFullPathName().toStdString(),
                                               activeSession.getMetadata(),
                                               measuredPoints);

    notifyProgress(1.0f, "Session Completed Successfully! Exported to " + exportDir.getFullPathName(), SequencerState::Finished);
}

} // namespace abdaudiolab::core
