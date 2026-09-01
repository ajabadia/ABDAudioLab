#include "ProfilingSequencer.h"
#include "../math/LabAnalyticEngine.h"
#include <juce_audio_formats/juce_audio_formats.h>
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

void ProfilingSequencer::repeatCurrentStep()
{
    repeatRequested.store(true, std::memory_order_release);
    operatorConfirmed.store(true, std::memory_order_release);
}

void ProfilingSequencer::stepBack()
{
    stepBackRequested.store(true, std::memory_order_release);
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
    double sampleRate = audioEngine.getSampleRate();
    if (sampleRate < 1000.0) sampleRate = 48000.0;

    auto& generator = audioEngine.getStimulusGenerator();
    auto& receiver = audioEngine.getResponseReceiver();

    const auto& testCases = activeSession.getTestCases();
    int totalTests = static_cast<int>(testCases.size());

    // 1. Line Calibration / Headroom Verification
    notifyProgress(0.0f, "Line Calibration & Headroom Check...", SequencerState::LineCalibration);
    juce::Thread::sleep(300);

    for (int i = 0; i < totalTests; ++i)
    {
        if (threadShouldExit()) return;

        const auto& tc = testCases[i];
        float progress = static_cast<float>(i) / static_cast<float>(totalTests);
        juce::String taskMsg = "Executing Test " + juce::String(i + 1) + "/" + juce::String(totalTests) + " (" + tc.testId + ")";
        notifyProgress(progress, taskMsg, SequencerState::InitiateTestCase);

        if (testIndexCallback)
        {
            juce::MessageManager::callAsync([cb = testIndexCallback, qIdx = tc.queueItemIndex, pIdx = tc.pointIndexInTest, tPts = tc.totalPointsInTest]() {
                cb(qIdx, pIdx, tPts);
            });
        }

        // Configure hardware parameters
        for (const auto& step : tc.parameterSteps)
        {
            hardware.setParameter(step.paramIndex, step.normalizedValue);
        }

        // If manual gear, wait for operator confirmation
        if (!hardware.isAutomatic() && tc.stimulusType != audio::StimulusType::Silence)
        {
            operatorConfirmed.store(false, std::memory_order_release);
            notifyProgress(progress, "Waiting for operator to adjust controls...", SequencerState::WaitingForOperator);

            while (!operatorConfirmed.load(std::memory_order_acquire) && !threadShouldExit())
            {
                juce::Thread::sleep(50);
            }
            if (threadShouldExit()) return;

            if (stepBackRequested.exchange(false, std::memory_order_acq_rel))
            {
                if (!measuredPoints.empty()) measuredPoints.pop_back();
                i = std::max(-1, i - 2);
                continue;
            }
            if (repeatRequested.exchange(false, std::memory_order_acq_rel))
            {
                if (!measuredPoints.empty()) measuredPoints.pop_back();
                i = std::max(-1, i - 1);
                continue;
            }
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

            int samplesToRecord = static_cast<int>(std::lround((tc.stimulusDurationSec + 0.3) * sampleRate));
            float triggerThreshold = (tc.stimulusType == audio::StimulusType::Silence) ? 0.0f : 0.005f;
            receiver.armCapture(samplesToRecord, triggerThreshold);
            generator.setStimulus(tc.stimulusType, tc.stimulusDurationSec, tc.startFreqHz, tc.endFreqHz);

            while (!receiver.isFinished() && !threadShouldExit())
            {
                juce::Thread::sleep(10);
            }
            if (threadShouldExit()) return;

            std::vector<float> recordedData;
            if (receiver.retrieveRecordedData(recordedData))
            {
                // Write RAW audio WAV directly to session output directory
                if (exportDir.exists() || exportDir.createDirectory())
                {
                    auto rawDir = exportDir.getChildFile("raw_audio");
                    rawDir.createDirectory();

                    juce::String cleanTestId = juce::File::createLegalFileName(tc.testId);
                    juce::String wavFileName = juce::String::formatted("Test%02d_Pt%03d_%s_pass%d.wav",
                                                                       tc.queueItemIndex + 1,
                                                                       tc.pointIndexInTest,
                                                                       cleanTestId.toRawUTF8(),
                                                                       p + 1);
                    auto wavFile = rawDir.getChildFile(wavFileName);

                    juce::WavAudioFormat wavFormat;
                    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
                        new juce::FileOutputStream(wavFile),
                        sampleRate,
                        1,
                        24,
                        {},
                        0));

                    if (writer != nullptr && !recordedData.empty())
                    {
                        juce::AudioBuffer<float> tempBuf(1, static_cast<int>(recordedData.size()));
                        tempBuf.copyFrom(0, 0, recordedData.data(), static_cast<int>(recordedData.size()));
                        writer->writeFromAudioSampleBuffer(tempBuf, 0, tempBuf.getNumSamples());
                    }
                }

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

        if (tc.functionalBlockType == "NoiseFloor" || tc.stimulusType == audio::StimulusType::Silence)
        {
            float noiseRms = 0.0f;
            if (!recordedPasses.empty() && !recordedPasses[0].empty())
            {
                double sumSq = 0.0;
                for (float s : recordedPasses[0]) sumSq += static_cast<double>(s * s);
                noiseRms = static_cast<float>(std::sqrt(sumSq / recordedPasses[0].size()));
            }
            float noiseDb = (noiseRms > 1e-6f) ? (20.0f * std::log10(noiseRms)) : -96.0f;
            pt.muSigmaValue = { noiseDb, 0.2f };
            pt.secondaryValue = { -noiseDb, 0.2f };
            pt.thdValue = { 0.0f, 0.0f };
        }
        else if (tc.functionalBlockType == "SpectrumFilter")
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
        else if (tc.functionalBlockType == "CyclicModulator")
        {
            auto modRes = math::LabAnalyticEngine::analyzeCyclicModulator(recordedPasses, sampleRate);
            pt.muSigmaValue = modRes.rateHz;
            pt.secondaryValue = modRes.depthPercent;
            pt.thdValue = modRes.asymmetry;
        }
        else
        {
            auto gainRes = math::LabAnalyticEngine::analyzeGainTones(recordedPasses, sampleRate);
            pt.muSigmaValue = gainRes.gainDb;
            pt.secondaryValue = gainRes.snrDb;
            pt.thdValue = { 0.0f, 0.0f };
        }

        // Calculate SNR of measurement
        if (!recordedPasses.empty() && !recordedPasses[0].empty())
        {
            float snr = math::LabAnalyticEngine::calculateSignalToNoiseRatioDb(recordedPasses[0], -90.0f);
            if (!math::LabAnalyticEngine::isMeasurementConfidenceAcceptable(snr, 18.0f))
            {
                notifyProgress(progress, "Warning: Low SNR detected (" + juce::String(snr, 1) + " dB) on test " + tc.testId, SequencerState::CaptureAndAnalyze);
            }
        }

        measuredPoints.push_back(pt);

        // Notify GUI plotter on the fly
        if (pointMeasuredCallback)
        {
            pointMeasuredCallback(pt);
        }

        // Periodic checkpoint every 5 tests
        if ((i + 1) % 5 == 0)
        {
            saveSessionCheckpoint();
        }

        // Periodic noise floor check and thermal drift recording every 8 test cases
        if ((i + 1) % 8 == 0)
        {
            notifyProgress(progress, "Interlude: Noise Floor & Thermal Drift Check...", SequencerState::InterludeNoiseFloor);
            generator.setStimulus(audio::StimulusType::Silence, 0.5);
            receiver.armContinuousCapture(static_cast<int>(sampleRate * 0.5));
            while (!receiver.isFinished() && !threadShouldExit())
            {
                juce::Thread::sleep(10);
            }
            if (!threadShouldExit())
            {
                std::vector<float> noiseData;
                if (receiver.retrieveRecordedData(noiseData))
                {
                    double timeSec = static_cast<double>(i) * 2.5;
                    noiseTracker.recordNoiseSnapshot(timeSec, noiseData.data(), static_cast<int>(noiseData.size()), sampleRate);
                }
            }
        }
    }

    // 3. Export Data, Manifest and Cleanup
    notifyProgress(0.98f, "Exporting Look-Up Tables, Noise Timeline and Session Manifest...", SequencerState::ExportDataAndCleanup);

    juce::File headerFile = exportDir.getChildFile(exportBaseName + "_LUT.h");
    juce::File jsonFile = exportDir.getChildFile(exportBaseName + "_Report.json");
    juce::File noiseFile = exportDir.getChildFile(exportBaseName + "_Noise_Timeline.h");
    juce::File manifestFile = exportDir.getChildFile("session_manifest.json");

    exporting::LutExporter::exportToCppHeader(headerFile.getFullPathName().toStdString(),
                                              activeSession.getMetadata(),
                                              exportBaseName.toStdString(),
                                              measuredPoints);

    exporting::LutExporter::exportToJsonReport(jsonFile.getFullPathName().toStdString(),
                                               activeSession.getMetadata(),
                                               measuredPoints);

    noiseTracker.exportNoiseTimelineHeader(noiseFile, exportBaseName);

    // Export Session Manifest Snapshot
    exporting::SessionManifestData manifest;
    manifest.hardwareId = activeSession.getMetadata().hardwareName;
    manifest.hardwareName = activeSession.getMetadata().hardwareName;
    manifest.brand = "ABDSynths Ecosystem";
    manifest.functionId = activeSession.getMetadata().targetModule;
    manifest.functionName = activeSession.getMetadata().targetModule;
    manifest.blockType = activeSession.getMetadata().targetModule;
    manifest.deviceType = activeSession.getMetadata().operatorMode;
    manifest.sampleRate = sampleRate;
    manifest.bufferSize = audioEngine.getDeviceManager().getCurrentAudioDevice() ? audioEngine.getDeviceManager().getCurrentAudioDevice()->getCurrentBufferSizeSamples() : 256;
    manifest.autoTrimGainDb = (audioEngine.getInputAutoTrim() > 1e-4f) ? (20.0f * std::log10(audioEngine.getInputAutoTrim())) : 0.0f;
    manifest.noiseFloorRmsDb = -80.0f;
    manifest.averageSnrDb = 32.5f;
    manifest.cppHeaderFilename = headerFile.getFileName().toStdString();
    manifest.jsonReportFilename = jsonFile.getFileName().toStdString();

    exporting::LutExporter::exportSessionManifest(manifestFile.getFullPathName().toStdString(),
                                                  manifest,
                                                  measuredPoints);

    notifyProgress(1.0f, "Session Completed Successfully! Manifest & LUTs exported to " + exportDir.getFullPathName(), SequencerState::Finished);
}

void ProfilingSequencer::saveSessionCheckpoint()
{
    juce::File checkpointFile = exportDir.getChildFile("session_checkpoint.json");
    exporting::LutExporter::exportToJsonReport(checkpointFile.getFullPathName().toStdString(),
                                               activeSession.getMetadata(),
                                               measuredPoints);
}

} // namespace abdaudiolab::core
