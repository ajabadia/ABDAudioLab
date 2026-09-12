#include "ProfilingSequencer.h"
#include "../math/LabAnalyticEngine.h"
#include "../math/PreScanSpectrumAnalyzer.h"
#include "../math/ModulationEstimator.h"
#include "../audio/LabStimulusGenerator.h"
#include "../export/NamDatasetExporter.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <thread>

namespace abdaudiolab::core
{

ProfilingSequencer::ProfilingSequencer(audio::LabAudioEngine& engine,
                                       hardware::IHardwareController& hw)
    : juce::Thread("ProfilingSequencerThread"),
      audioEngine(engine),
      hardware(&hw),
      hardwareDispatcher(std::make_unique<ProfilingHardwareDispatcher>(&hw)),
      audioCapture(std::make_unique<ProfilingAudioCapture>(engine.getResponseReceiver(), engine.getStimulusGenerator()))
{
}

ProfilingSequencer::~ProfilingSequencer()
{
    stopSession();
}

void ProfilingSequencer::setHardwareController(hardware::IHardwareController* newHardware) noexcept
{
    if (newHardware != nullptr)
    {
        hardware = newHardware;
        if (hardwareDispatcher != nullptr)
            hardwareDispatcher->setHardwareController(newHardware);
    }
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
    safetyAborted.store(false, std::memory_order_release);
    linearBypassDetected.store(false, std::memory_order_release);
    adaptiveOptimizationApplied.store(false, std::memory_order_release);
    sessionPaused.store(false, std::memory_order_release);
    rerunPointIndex.store(-1, std::memory_order_release);
    audioEngine.getResponseReceiver().resetOverloadGuard();


    exportDir.createDirectory();
    startThread();
    return true;
}

void ProfilingSequencer::stopSession()
{
    sessionPaused.store(false, std::memory_order_release);
    resumeEvent.signal();  // unblock the pause gate in the run loop
    signalThreadShouldExit();
    stopThread(4000);
    if (hardwareDispatcher != nullptr)
    {
        for (int ch = 1; ch <= 16; ++ch)
            hardwareDispatcher->sendAllNotesOff(ch);
    }
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

void ProfilingSequencer::pauseSession()
{
    sessionPaused.store(true, std::memory_order_release);
    notifyProgress(0.0f, "Session paused by operator.", SequencerState::Idle);
}

void ProfilingSequencer::resumeSession()
{
    sessionPaused.store(false, std::memory_order_release);
    resumeEvent.signal();
}

void ProfilingSequencer::scheduleRerunPoint(int globalPointIndex)
{
    rerunPointIndex.store(globalPointIndex, std::memory_order_release);
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

void ProfilingSequencer::executeLifecycleActions(const std::vector<HardwareSetupAction>& actions)
{
    for (const auto& action : actions)
    {
        if (threadShouldExit())
            break;

        if (action.method == HardwareMethod::MANUAL_PROMPT)
        {
            if (manualPromptCallback)
            {
                juce::MessageManager::callAsync([cb = manualPromptCallback, desc = action.description]() {
                    cb(juce::String(desc));
                });
            }
            continue;
        }

        if (hardwareDispatcher != nullptr)
        {
            std::vector<HardwareSetupAction> singleAction { action };
            hardwareDispatcher->executeLifecycleActions(singleAction);
        }
    }
}

void ProfilingSequencer::executeMeasurementRecipe(const MeasurementPresetRecipe& recipe)
{
    if (recipe.recipeType.empty() && recipe.setupActions.empty() && recipe.excitationNotes.empty())
        return;

    notifyProgress(0.01f, "Applying Measurement Recipe (" + juce::String(recipe.recipeType) + ": " + juce::String(recipe.description) + ")...", SequencerState::WaitForStabilization);

    executeLifecycleActions(recipe.setupActions);

    if (hardwareDispatcher != nullptr && !recipe.excitationNotes.empty())
    {
        for (const auto& ev : recipe.excitationNotes)
        {
            if (threadShouldExit()) break;
            if (ev.startDelayMs > 0)
                audioCapture->executeSettlingWait(ev.startDelayMs, *this);

            hardwareDispatcher->sendNoteOn(1, ev.noteNumber, static_cast<float>(ev.velocity) / 127.0f);

            if (!ev.isLegato && ev.durationMs > 0)
            {
                audioCapture->executeSettlingWait(ev.durationMs, *this);
                hardwareDispatcher->sendNoteOff(1, ev.noteNumber);
            }
        }
    }

    if (recipe.postSettlingDelayMs > 0 && audioCapture != nullptr)
    {
        audioCapture->executeSettlingWait(recipe.postSettlingDelayMs, *this);
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

    // 0. Apply Data-Driven Hardware Pre-Session Lifecycle Setup
    if (lifecycleContract != nullptr && !lifecycleContract->preSessionSetup.empty())
    {
        notifyProgress(0.01f, "Applying Hardware Pre-Session Setup (Disengaging FX / Zero Drift)...", SequencerState::WaitForStabilization);
        executeLifecycleActions(lifecycleContract->preSessionSetup);
    }

    // 1. Line Calibration / Headroom Verification & 1.5.4 Auto-Trim to -3.0 dBfs
    notifyProgress(0.0f, "Line Calibration & Headroom Verification...", SequencerState::LineCalibration);
    juce::Thread::sleep(150);

    // Run Pre-Roll Calibration Tone to measure line headroom and automatically apply Auto-Trim to -3 dBfs
    if (totalTests > 0)
    {
        notifyProgress(0.01f, "Calibrando ganancia de entrada (Pre-Roll Auto-Trim -3 dBfs)...", SequencerState::LineCalibration);
        constexpr float calToneDuration = 0.5f;

        std::vector<float> calAudio;
        bool ok = audioCapture->captureLineCalibrationSynchronous(calToneDuration, sampleRate, (calToneDuration + 1.5) * 1000.0, *this, calAudio, safetyAborted);

        if (safetyAborted.load(std::memory_order_acquire))
        {
            notifyProgress(0.0f, "Prueba detenida por seguridad: Se ha detectado una sobrecarga de volumen en la entrada. Por favor, baja el nivel de ganancia de tu tarjeta antes de reintentar.", SequencerState::ErrorState);
            return;
        }

        if (ok && !calAudio.empty())
        {
            float maxPeak = 0.0f;
            for (float s : calAudio)
            {
                maxPeak = std::max(maxPeak, std::abs(s));
            }

            if (maxPeak > 1e-4f)
            {
                // Target headroom: -3.0 dBfs = 10^(-3/20) ~ 0.70794578
                constexpr float targetHeadroomLinear = 0.70794578f;
                float calculatedGain = targetHeadroomLinear / maxPeak;
                calculatedGain = juce::jlimit(0.1f, 10.0f, calculatedGain);
                audioEngine.setInputAutoTrim(calculatedGain);

                float gainDb = 20.0f * std::log10(calculatedGain);
                notifyProgress(0.02f, "Auto-Trim aplicado: In 1 calibrado a -3.0 dBfs (" + juce::String(calculatedGain, 2) + "x / " + (gainDb >= 0.0f ? "+" : "") + juce::String(gainDb, 1) + " dB)", SequencerState::LineCalibration);
                audioCapture->executeSettlingWait(150, *this);
            }
        }
    }

    // Autonomous Hardware Pre-Scan (Zero-Friction Diagnostic)
    if (hardware != nullptr && hardware->isAutomatic() && totalTests > 4)
    {
        notifyProgress(0.03f, "Analizando la naturaleza del hardware (Pre-escaneo continuo)...", SequencerState::LineCalibration);

        constexpr float preScanDuration = 2.0f;
        int preScanSamples = static_cast<int>(std::lround((preScanDuration + 0.5f) * sampleRate));
        receiver.armContinuousCapture(preScanSamples);
        generator.setStimulus(audio::StimulusType::LogFarinaSweep, preScanDuration, 20.0f, 20000.0f);

        double preScanStartTime = juce::Time::getMillisecondCounterHiRes();
        while (!receiver.isFinished() && !threadShouldExit())
        {
            if (receiver.isOverloadTriggered())
            {
                generator.stop();
                safetyAborted.store(true, std::memory_order_release);
                notifyProgress(0.0f, "Prueba detenida por seguridad: Se ha detectado una sobrecarga de volumen en la entrada. Por favor, baja el nivel de ganancia de tu tarjeta antes de reintentar.", SequencerState::ErrorState);
                return;
            }
            if ((juce::Time::getMillisecondCounterHiRes() - preScanStartTime) > (preScanDuration + 2.5) * 1000.0)
            {
                receiver.forceFinish();
                break;
            }
            juce::Thread::sleep(10);
        }

        if (threadShouldExit() || safetyAborted.load(std::memory_order_acquire)) return;

        std::vector<float> preScanAudio;
        if (receiver.retrieveRecordedData(preScanAudio) && preScanAudio.size() > 1024)
        {
            auto refSweep = math::FarinaDeconvolver::generateLogFarinaSweep(sampleRate, preScanDuration, 20.0f, 20000.0f);
            auto preScanEval = math::LabAnalyticEngine::evaluateLinearBypass(preScanAudio, refSweep, -85.0f);

            if (preScanEval.isLinear)
            {
                // Branch A: Pure Linear Module (Pass-through, Clean Mixer, Buffer)
                linearBypassDetected.store(true, std::memory_order_release);

                // Synthesize identity/linear points
                for (int pIdx = 0; pIdx < totalTests; ++pIdx)
                {
                    const auto& currentTc = testCases[pIdx];
                    exporting::MeasuredPoint linearPt;
                    linearPt.pointId = "P_" + juce::String::formatted("%03d", pIdx + 1).toStdString();
                    linearPt.testId = currentTc.testId;
                    linearPt.blockType = currentTc.functionalBlockType;
                    linearPt.stimulusType = "FarinaLogSweep";
                    linearPt.param1Normalized = currentTc.parameterSteps.empty() ? 0.0f : currentTc.parameterSteps[0].normalizedValue;
                    linearPt.thdPercent = 0.001f;
                    linearPt.snrDb = 92.0f;
                    linearPt.muSigmaValue = { 20000.0f, 0.0f };
                    linearPt.secondaryValue = { 0.0f, 0.0f };
                    linearPt.thdValue = { 0.001f, 0.0f };
                    linearPt.controlSteps = currentTc.parameterSteps;
                    measuredPoints.push_back(linearPt);

                    if (pointMeasuredCallback)
                    {
                        juce::MessageManager::callAsync([cb = pointMeasuredCallback, linearPt]() {
                            cb(linearPt);
                        });
                    }
                }

                saveSessionCheckpoint();
                notifyProgress(1.0f, "¡Módulo lineal detectado! Calibración completada con éxito.", SequencerState::Finished);
                return;
            }
            else
            {
                // Branch B: Non-linear module - evaluate adaptive roadmap
                math::PreScanResult preScanResult;
                const auto& firstTc = testCases[0];
                if (firstTc.functionalBlockType == "SpectrumFilter")
                {
                    preScanResult = math::PreScanSpectrumAnalyzer::analyzeFilterNoiseSweep(
                        preScanAudio, sampleRate, 0.0f, 127.0f, 11, 60.0f
                    );
                }
                else
                {
                    preScanResult = math::PreScanSpectrumAnalyzer::analyzeSaturationToneSweep(
                        preScanAudio, sampleRate, 1000.0f, 0.0f, 127.0f, 11, 60.0f
                    );
                }

                math::LabAnalyticEngine::computeAdaptiveRoadmap(preScanResult, 0.05f);

                if (preScanCallback)
                {
                    juce::MessageManager::callAsync([cb = preScanCallback, preScanResult]() {
                        cb(preScanResult);
                    });
                }

                float estimatedTotalSec = static_cast<float>(totalTests) * 3.0f;
                if (estimatedTotalSec > 120.0f)
                {
                    adaptiveOptimizationApplied.store(true, std::memory_order_release);
                    notifyProgress(0.04f, "Optimizando rejilla analítica (Catmull-Rom 2D) para reducir espera...", SequencerState::InitiateTestCase);
                    juce::Thread::sleep(150);
                }
            }
        }
    }

    int lastAppliedQueueIndex = -1;
    std::string lastAppliedBlockType;

    for (int i = 0; i < totalTests; ++i)
    {
        if (threadShouldExit()) return;

        // ── Phase 14: Pause gate (waits between iterations, not mid-capture) ──
        while (sessionPaused.load(std::memory_order_acquire) && !threadShouldExit())
        {
            notifyProgress(0.0f, "Session paused — waiting for operator resume.", SequencerState::Idle);
            resumeEvent.wait(300); // wake every 300 ms to re-check exit flag
        }
        if (threadShouldExit()) return;

        // ── Phase 14: Granular single-point re-run ──
        int rerunIdx = rerunPointIndex.exchange(-1, std::memory_order_acq_rel);
        if (rerunIdx >= 0)
        {
            // Find the test-case index whose globalPointIndex matches
            for (int seek = 0; seek < totalTests; ++seek)
            {
                if (testCases[seek].globalPointIndex == rerunIdx)
                {
                    // Remove the previously stored point for that index if it exists
                    measuredPoints.erase(
                        std::remove_if(measuredPoints.begin(), measuredPoints.end(),
                            [rerunIdx](const exporting::MeasuredPoint& p) {
                                return p.globalIndex == rerunIdx;
                            }),
                        measuredPoints.end());
                    i = seek - 1; // loop will do ++i bringing us to seek
                    break;
                }
            }
            continue;
        }

        const auto& tc = testCases[i];

        // 1.7.16 Execute Measurement Recipe when entering a new test queue item or block type
        if ((tc.queueItemIndex != lastAppliedQueueIndex || tc.functionalBlockType != lastAppliedBlockType)
            && (!tc.presetRecipe.recipeType.empty() || !tc.presetRecipe.setupActions.empty()))
        {
            executeMeasurementRecipe(tc.presetRecipe);
            lastAppliedQueueIndex = tc.queueItemIndex;
            lastAppliedBlockType = tc.functionalBlockType;
        }

        float progress = static_cast<float>(i) / static_cast<float>(totalTests);
        juce::String taskMsg = "Executing Test " + juce::String(i + 1) + "/" + juce::String(totalTests) + " (" + tc.testId + ")";
        notifyProgress(progress, taskMsg, SequencerState::InitiateTestCase);

        if (testIndexCallback)
        {
            juce::MessageManager::callAsync([cb = testIndexCallback, qIdx = tc.queueItemIndex, pIdx = tc.pointIndexInTest, tPts = tc.totalPointsInTest]() {
                cb(qIdx, pIdx, tPts);
            });
        }

        // Configure hardware parameters via hardwareDispatcher
        if (hardwareDispatcher != nullptr)
        {
            for (const auto& step : tc.parameterSteps)
            {
                hardwareDispatcher->setParameter(step.paramIndex, step.normalizedValue);
            }
        }

        if (operatorStepCallback)
        {
            juce::MessageManager::callAsync([cb = operatorStepCallback, tcCopy = tc, stepIdx = i + 1, totalCount = totalTests]() {
                cb(tcCopy, stepIdx, totalCount);
            });
        }

        // If manual gear, wait for operator confirmation
        if (hardware != nullptr && !hardware->isAutomatic() && tc.stimulusType != audio::StimulusType::Silence)
        {
            juce::String promptText = "Set ";
            if (tc.parameterSteps.empty())
            {
                promptText += "controls to target position";
            }
            else
            {
                for (size_t sIdx = 0; sIdx < tc.parameterSteps.size(); ++sIdx)
                {
                    if (sIdx > 0) promptText += ", ";
                    int pct = static_cast<int>(std::round(tc.parameterSteps[sIdx].normalizedValue * 100.0f));
                    promptText += juce::String(tc.parameterSteps[sIdx].paramName) + " to " + juce::String(pct) + "%";
                }
            }
            promptText += " and click Accept [Space]";

            operatorConfirmed.store(false, std::memory_order_release);

            notifyProgress(progress, promptText, SequencerState::WaitingForOperator);

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
        if (audioCapture != nullptr && tc.stabilizationWaitMs > 0)
        {
            audioCapture->executeSettlingWait(static_cast<int>(tc.stabilizationWaitMs), *this);
        }

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

            bool isMidiTriggered = tc.isAutonomousSynth || (tc.stimulusType == audio::StimulusType::Silence);
            float gateDurationSec = (tc.noteGateDurationSec > 0.0f) ? tc.noteGateDurationSec : static_cast<float>(tc.stimulusDurationSec);

            double maxRecSec = (tc.captureMode == "ADAPTIVE_ENVELOPE") ? std::max(tc.stimulusDurationSec + 2.0, 4.0) : (tc.stimulusDurationSec + 0.3);
            int samplesToRecord = static_cast<int>(std::lround(maxRecSec * sampleRate));
            float triggerThreshold = isMidiTriggered ? 0.0f : 0.005f;
            receiver.armCapture(samplesToRecord, triggerThreshold);

            if (isMidiTriggered)
            {
                generator.stop();
                if (hardwareDispatcher != nullptr)
                    hardwareDispatcher->sendNoteOn(tc.midiChannel, tc.midiNoteNumber, tc.midiVelocity);
            }
            else
            {
                generator.setStimulus(tc.stimulusType, tc.stimulusDurationSec, tc.startFreqHz, tc.endFreqHz);
            }

            bool noteOffSent = !isMidiTriggered;
            double noteStartTime = juce::Time::getMillisecondCounterHiRes();
            double gateMs = static_cast<double>(gateDurationSec) * 1000.0;
            double captureStartTime = juce::Time::getMillisecondCounterHiRes();
            double timeoutMs = (maxRecSec + 3.5) * 1000.0;

            while (!receiver.isFinished() && !threadShouldExit())
            {
                double elapsedMs = juce::Time::getMillisecondCounterHiRes() - noteStartTime;
                if (!noteOffSent && elapsedMs >= gateMs)
                {
                    if (hardwareDispatcher != nullptr)
                        hardwareDispatcher->sendNoteOff(tc.midiChannel, tc.midiNoteNumber, 0.0f);
                    noteOffSent = true;
                }

                if (receiver.isOverloadTriggered())
                {
                    generator.stop();
                    if (hardwareDispatcher != nullptr)
                        hardwareDispatcher->sendAllNotesOff(tc.midiChannel);
                    safetyAborted.store(true, std::memory_order_release);
                    notifyProgress(progress, "Prueba detenida por seguridad: Se ha detectado una sobrecarga de volumen en la entrada. Por favor, baja el nivel de ganancia de tu tarjeta antes de reintentar.", SequencerState::ErrorState);
                    return;
                }

                if ((juce::Time::getMillisecondCounterHiRes() - captureStartTime) > timeoutMs)
                {
                    receiver.forceFinish();
                    break;
                }
                juce::Thread::sleep(10);
            }

            if (!noteOffSent && hardwareDispatcher != nullptr)
            {
                hardwareDispatcher->sendNoteOff(tc.midiChannel, tc.midiNoteNumber, 0.0f);
            }

            if (threadShouldExit() || safetyAborted.load(std::memory_order_acquire))
            {
                if (hardwareDispatcher != nullptr)
                    hardwareDispatcher->sendAllNotesOff(tc.midiChannel);
                return;
            }

            std::vector<float> recordedData;
            if (receiver.retrieveRecordedData(recordedData))
            {
                // Single-Take Multi-Analysis: transient saturation + sustained spectral response
                auto multiRes = math::LabAnalyticEngine::analyzeSingleTakeMultiplexed(recordedData, sampleRate, 0.2f);
                if (receiver.isEarlyStopTriggered())
                {
                    multiRes.earlyStopped = true;
                }
                // If ADAPTIVE_ENVELOPE mode, detect tail end and trim trailing silence
                if (tc.captureMode == "ADAPTIVE_ENVELOPE" && !recordedData.empty())
                {
                    int minSamples = static_cast<int>(std::lround(tc.stimulusDurationSec * sampleRate));
                    int cutIdx = static_cast<int>(recordedData.size());
                    int silenceCount = 0;
                    constexpr int kSilenceThresholdSamples = 2048; // ~20-45ms of continuous silence
                    for (int sampleIdx = minSamples; sampleIdx < static_cast<int>(recordedData.size()); ++sampleIdx)
                    {
                        if (std::abs(recordedData[static_cast<size_t>(sampleIdx)]) < 0.001f) // -60 dBfs threshold
                        {
                            silenceCount++;
                            if (silenceCount >= kSilenceThresholdSamples)
                            {
                                cutIdx = sampleIdx - kSilenceThresholdSamples + 1;
                                break;
                            }
                        }
                        else
                        {
                            silenceCount = 0;
                        }
                    }
                    if (cutIdx < static_cast<int>(recordedData.size()) && cutIdx > minSamples)
                    {
                        recordedData.resize(static_cast<size_t>(cutIdx));
                    }
                }
                // Write RAW audio WAV directly to session output directory
                if (exportDir.exists() || exportDir.createDirectory())
                {
                    auto rawDir = exportDir.getChildFile("raw_audio");
                    rawDir.createDirectory();

                    juce::String cleanTestId = juce::File::createLegalFileName(tc.testId);
                    juce::String wavFileName = "Test" + juce::String::formatted("%02d", tc.queueItemIndex + 1)
                                             + "_Pt" + juce::String::formatted("%03d", tc.pointIndexInTest)
                                             + "_" + cleanTestId
                                             + "_pass" + juce::String(p + 1) + ".wav";
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

        auto stimulusToString = [](audio::StimulusType st) -> std::string {
            switch (st) {
                case audio::StimulusType::Silence: return "Silence";
                case audio::StimulusType::DiracDelta: return "DiracDelta";
                case audio::StimulusType::SyncPulses3: return "SyncPulses3";
                case audio::StimulusType::WhiteNoise: return "WhiteNoise";
                case audio::StimulusType::PinkNoise: return "PinkNoise";
                case audio::StimulusType::SineWave1kHz: return "SineWave1kHz";
                case audio::StimulusType::SquareWave1kHz: return "SquareWave1kHz";
                case audio::StimulusType::LogFarinaSweep: return "LogFarinaSweep";
                case audio::StimulusType::AmplitudeRamp: return "AmplitudeRamp";
                case audio::StimulusType::NamCalibration: return "NamCalibration";
                default: return "Unknown";
            }
        };

        exporting::MeasuredPoint pt;
        pt.pointId = tc.pointId.empty() ? ("P_" + juce::String::formatted("%03d", i + 1).toStdString()) : tc.pointId;
        pt.testId = tc.testId;
        pt.blockType = tc.functionalBlockType;
        pt.stimulusType = stimulusToString(tc.stimulusType);
        pt.controlSteps = tc.parameterSteps;
        pt.globalIndex = tc.globalPointIndex;
        pt.queueIndex = tc.queueItemIndex;
        pt.pointIndexInTest = tc.pointIndexInTest;
        if (!recordedPasses.empty())
            pt.irSamples = recordedPasses[0];

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
            if (tc.stimulusType == audio::StimulusType::LogFarinaSweep)
            {
                auto filterRes = math::LabAnalyticEngine::analyzeFilterPasses(recordedPasses, invFilter, sampleRate, tc.stimulusDurationSec, tc.startFreqHz, tc.endFreqHz);
                pt.muSigmaValue = filterRes.cutoffHz;
                pt.secondaryValue = filterRes.resonanceDb;
                pt.thdValue = filterRes.thdPercent;
            }
            else
            {
                auto wsRes = math::LabAnalyticEngine::analyzeWaveShaperRamps(recordedPasses, sampleRate);
                pt.muSigmaValue = wsRes.thdPercent;
                pt.secondaryValue = { 1.0f, 0.0f };
                pt.thdValue = wsRes.thdPercent;
            }
        }
        else if (tc.functionalBlockType == "CyclicModulator")
        {
            auto modRes = math::LabAnalyticEngine::analyzeCyclicModulator(recordedPasses, sampleRate);
            pt.muSigmaValue = modRes.rateHz;
            pt.secondaryValue = modRes.depthPercent;
            pt.thdValue = modRes.asymmetry;
        }
        else if (tc.functionalBlockType == "WienerHammerstein")
        {
            std::vector<float> inputStimulus;
            if (tc.stimulusType == audio::StimulusType::LogFarinaSweep)
            {
                inputStimulus = math::FarinaDeconvolver::generateLogFarinaSweep(sampleRate, tc.stimulusDurationSec, tc.startFreqHz, tc.endFreqHz);
            }
            else
            {
                inputStimulus = math::FarinaDeconvolver::generateLogFarinaSweep(sampleRate, tc.stimulusDurationSec, 20.0f, 20000.0f);
            }

            auto whRes = math::LabAnalyticEngine::analyzeWienerHammerstein(recordedPasses, inputStimulus, sampleRate);
            pt.muSigmaValue = whRes.nonLinearCoeffA;
            pt.secondaryValue = whRes.postFilterCentroidHz;
            pt.thdValue = { 1.0f - whRes.goodnessOfFitR2.mean, whRes.goodnessOfFitR2.stdDev };
            pt.irSamples = whRes.representativeH1;
        }
        else if (tc.functionalBlockType == "NeuralCalibration" || tc.stimulusType == audio::StimulusType::NamCalibration)
        {
            auto stimBuffer = audio::LabStimulusGenerator::generateNamCalibrationBuffer(sampleRate, tc.stimulusDurationSec);
            int inSamples = stimBuffer.getNumSamples();
            const float* inPtr = stimBuffer.getReadPointer(0);

            const float* recPtr = (!recordedPasses.empty() && !recordedPasses[0].empty()) ? recordedPasses[0].data() : nullptr;
            int recSamples = (!recordedPasses.empty()) ? static_cast<int>(recordedPasses[0].size()) : 0;

            int searchWindow = static_cast<int>(std::min(0.3 * sampleRate, static_cast<double>(inSamples)));
            int maxLag = static_cast<int>(std::min(0.08 * sampleRate, static_cast<double>(recSamples - searchWindow)));
            if (maxLag <= 0) maxLag = 1;

            int lag = (recPtr != nullptr && inPtr != nullptr) ? exporting::NamDatasetExporter::findLatencyOffsetSamples(inPtr, recPtr, searchWindow, maxLag) : 0;
            float latencyMs = (static_cast<float>(lag) / static_cast<float>(sampleRate)) * 1000.0f;

            pt.muSigmaValue = { latencyMs, 0.01f };
            pt.secondaryValue = { static_cast<float>(lag), 0.0f };
            pt.thdValue = { 0.0f, 0.0f };

            if (exportDir.exists() && recPtr != nullptr && recSamples > lag)
            {
                exporting::NamDatasetManifest manifest;
                manifest.hardwareId = activeSession.getMetadata().targetModule;
                manifest.hardwareDisplayName = activeSession.getMetadata().hardwareName;
                manifest.functionId = tc.testId;
                manifest.sampleRate = sampleRate;
                manifest.bitDepth = 24;
                if (!tc.parameterSteps.empty())
                    manifest.controlPositions["Param1"] = tc.parameterSteps[0].normalizedValue;
                if (tc.parameterSteps.size() > 1)
                    manifest.controlPositions["Param2"] = tc.parameterSteps[1].normalizedValue;

                juce::AudioBuffer<float> recBuffer(1, recSamples);
                std::memcpy(recBuffer.getWritePointer(0), recPtr, sizeof(float) * static_cast<size_t>(recSamples));

                auto namFolder = exportDir.getChildFile("nam_dataset_" + tc.testId);
                exporting::NamDatasetExporter::exportDataset(namFolder, stimBuffer, recBuffer, sampleRate, manifest);
            }
        }
        else
        {
            auto gainRes = math::LabAnalyticEngine::analyzeGainTones(recordedPasses, sampleRate);
            pt.muSigmaValue = gainRes.gainDb;
            pt.secondaryValue = gainRes.snrDb;
            pt.thdValue = { 0.0f, 0.0f };
        }

        // Calculate SNR & signal presence validation (1.5.3 Confidence Check >= 18 dB)
        bool isActiveTest = (tc.functionalBlockType != "NoiseFloor" && tc.stimulusType != audio::StimulusType::Silence);
        float maxPeak = 0.0f;
        if (!recordedPasses.empty() && !recordedPasses[0].empty())
        {
            for (float s : recordedPasses[0])
                maxPeak = std::max(maxPeak, std::abs(s));
        }

        if (isActiveTest && maxPeak < 0.005f) // Signal below -46 dBfs -> No patch cable or zero volume
        {
            float dbPeak = (maxPeak > 1e-5f) ? (20.0f * std::log10(maxPeak)) : -96.0f;
            notifyProgress(progress, "Warning: Low audio signal detected on In 1 (Peak: " + juce::String(dbPeak, 1) + " dBfs). Check patch cable.", SequencerState::CaptureAndAnalyze);
        }

        if (!recordedPasses.empty() && !recordedPasses[0].empty())
        {
            float snr = math::LabAnalyticEngine::calculateSignalToNoiseRatioDb(recordedPasses[0], -90.0f);
            pt.snrDb = snr;

            // 1.5.3 Strict SNR >= 18 dB Confidence Check
            if (isActiveTest && !math::LabAnalyticEngine::isMeasurementConfidenceAcceptable(snr, 18.0f))
            {
                // If auto-retry has not been performed for this step yet, do a single auto-retry
                static int lastRetriedIndex = -1;
                if (lastRetriedIndex != i)
                {
                    lastRetriedIndex = i;
                    notifyProgress(progress, "SNR bajo (" + juce::String(snr, 1) + " dB < 18 dB) en " + tc.testId + ". Reintentando medición automáticamente...", SequencerState::CaptureAndAnalyze);
                    juce::Thread::sleep(200);
                    i = i - 1; // Repeat this exact test case
                    continue;
                }
                else
                {
                    // Already retried once; flag point and warn
                    lastRetriedIndex = -1;
                    notifyProgress(progress, "Alerta: SNR bajo persistente (" + juce::String(snr, 1) + " dB) en " + tc.testId + ". Punto registrado con flag de baja confianza.", SequencerState::CaptureAndAnalyze);
                }
            }
        }
        pt.thdPercent = pt.thdValue.mean;

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

    // 3. Apply Plateau Collapse Filter for Manual profiling sweeps if redundant pauses/plateaus exist
    if (!activeSession.isPatchSession() && activeSession.getMetadata().operatorMode == "MANUAL" && measuredPoints.size() > 2)
    {
        std::vector<math::PreScanPoint> rawTrajectory;
        rawTrajectory.reserve(measuredPoints.size());
        for (size_t p = 0; p < measuredPoints.size(); ++p)
        {
            const auto& pt = measuredPoints[p];
            math::PreScanPoint psp;
            psp.controlValue = pt.param1Normalized;
            psp.timeSec = static_cast<float>(p) * 0.5f;
            psp.primaryMetric = pt.muSigmaValue.mean;
            psp.thdPercent = pt.thdPercent;
            rawTrajectory.push_back(psp);
        }
        float avgNoiseDbfs = -90.0f;
        if (const auto* snap = noiseTracker.getLatestSnapshot())
            avgNoiseDbfs = snap->totalRmsDb;
        float noiseFloorLin = (avgNoiseDbfs < -10.0f) ? std::pow(10.0f, avgNoiseDbfs / 20.0f) : 0.0001f;
        auto collapseResult = math::LabAnalyticEngine::applyPlateauCollapseFilter(rawTrajectory, noiseFloorLin);
        if (collapseResult.trajectory.size() >= 2 && collapseResult.trajectory.size() < measuredPoints.size())
        {
            std::vector<exporting::MeasuredPoint> filteredPoints;
            filteredPoints.reserve(collapseResult.trajectory.size());
            for (size_t c = 0; c < collapseResult.trajectory.size(); ++c)
            {
                const auto& cp = collapseResult.trajectory[c];
                exporting::MeasuredPoint pt;
                pt.pointId = "P_" + juce::String::formatted("%03d", static_cast<int>(c + 1)).toStdString();
                pt.param1Normalized = cp.controlValue;
                pt.muSigmaValue = { cp.primaryMetric, 0.01f };
                pt.thdPercent = cp.thdPercent;
                pt.snrDb = (noiseFloorLin > 1e-6f) ? (20.0f * std::log10(std::max(1e-4f, std::abs(cp.primaryMetric)) / noiseFloorLin)) : 85.0f;
                if (!measuredPoints.empty())
                {
                    pt.testId = measuredPoints[0].testId;
                    pt.blockType = measuredPoints[0].blockType;
                    pt.stimulusType = measuredPoints[0].stimulusType;
                    pt.controlSteps = measuredPoints[0].controlSteps;
                    if (!pt.controlSteps.empty())
                        pt.controlSteps[0].normalizedValue = cp.controlValue;
                }
                filteredPoints.push_back(pt);
            }
            measuredPoints = std::move(filteredPoints);
        }
    }

    // 4. Export Data, Manifest and Cleanup (Skipped for partial patch runs to avoid overwriting full dataset)
    if (!activeSession.isPatchSession())
    {
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
        manifest.operatorNotes = activeSession.getMetadata().operatorNotes;
        manifest.ambientTemperatureC = activeSession.getMetadata().ambientTemperatureC;
        manifest.warmupTimeMinutes = activeSession.getMetadata().warmupTimeMinutes;
        manifest.cppHeaderFilename = headerFile.getFileName().toStdString();
        manifest.jsonReportFilename = jsonFile.getFileName().toStdString();

        exporting::LutExporter::exportSessionManifest(manifestFile.getFullPathName().toStdString(),
                                                      manifest,
                                                      measuredPoints);
    }

    // 6. Execute Hardware Post-Session Teardown
    if (lifecycleContract != nullptr && !lifecycleContract->postSessionTeardown.empty())
    {
        notifyProgress(0.99f, "Executing Hardware Post-Session Teardown...", SequencerState::WaitForStabilization);
        executeLifecycleActions(lifecycleContract->postSessionTeardown);
    }

    if (hardware != nullptr)
    {
        for (int ch = 1; ch <= 16; ++ch)
            hardware->sendAllNotesOff(ch);
    }

    if (activeSession.isPatchSession())
    {
        notifyProgress(1.0f, "Targeted Re-Measurement Completed! " + juce::String(totalTests) + " point(s) patched.", SequencerState::Finished);
    }
    else
    {
        notifyProgress(1.0f, "Session Completed Successfully! Manifest & LUTs exported to " + exportDir.getFullPathName(), SequencerState::Finished);
    }
}

void ProfilingSequencer::saveSessionCheckpoint()
{
    juce::File checkpointFile = exportDir.getChildFile("session_checkpoint.json");
    exporting::LutExporter::exportToJsonReport(checkpointFile.getFullPathName().toStdString(),
                                               activeSession.getMetadata(),
                                               measuredPoints);
}

bool ProfilingSequencer::runUniversalModulationProbe(const ModulationProbeContract& contract,
                                                     const std::vector<float>& restingAudio,
                                                     math::ModulationMatrixProfile& targetProfile)
{
    if (hardware == nullptr || threadShouldExit())
        return false;

    // 1. Obtener métrica base del estado estático en reposo
    float baseMetric = computeTargetMetric(restingAudio, contract.destinationBlockType);

    const std::vector<float> excitations { 32.0f, 64.0f, 96.0f, 127.0f };
    std::vector<math::ModulationProbePoint> capturedPoints;
    capturedPoints.reserve(excitations.size());

    double sRate = audioEngine.getSampleRate();
    if (sRate < 1000.0) sRate = 48000.0;

    // 2. Ejecutar secuencia de 4 ráfagas analíticas
    for (float xInjected : excitations)
    {
        if (threadShouldExit())
            return false;

        // Inyectar el valor de control según el método del contrato vía HardwareDispatcher
        hardwareDispatcher->injectHardwareModulationValue(contract.excitationType,
                                                          contract.midiChannel,
                                                          contract.controlCCNumber,
                                                          contract.sysexTemplate,
                                                          static_cast<int>(xInjected));

        // Sleep síncrono del hilo de fondo para asentamiento de conversores y circuitos analógicos
        if (contract.settlingDelayMs > 0)
            audioCapture->executeSettlingWait(contract.settlingDelayMs, *this);

        // Disparar captura en lazo cerrado síncrono vía ProfilingAudioCapture (ráfaga estándar de 0.5s de estímulo)
        constexpr float probeDurationSec = 0.5f;
        std::vector<float> recordedAudio;
        bool ok = audioCapture->captureModulationBurstSynchronous(probeDurationSec, sRate, *this, recordedAudio);

        if (!ok || threadShouldExit())
            return false;

        // 3. Extraer métrica excitada y calcular el delta limpio
        float currentMetric = computeTargetMetric(recordedAudio, contract.destinationBlockType);
        float yObserved = currentMetric - baseMetric;

        math::ModulationProbePoint pt;
        pt.xInjected = xInjected;
        pt.yObserved = yObserved;
        capturedPoints.push_back(pt);
    }

    // 4. Resolver mediante regresión de mínimos cuadrados lineales (cero allocations dinámicas)
    auto calculatedNode = math::ModulationEstimator::calculateNode(contract.sourceID, contract.destID, capturedPoints);
    targetProfile.setNode(contract.sourceID, contract.destID, calculatedNode);

    // 5. Notificación al observador / UI
    if (onModulationNodeMeasured)
    {
        if (juce::MessageManager::getInstanceWithoutCreating() != nullptr &&
            juce::MessageManager::getInstanceWithoutCreating()->isThisTheMessageThread())
        {
            onModulationNodeMeasured(calculatedNode);
        }
        else
        {
            juce::MessageManager::callAsync([cb = onModulationNodeMeasured, calculatedNode]() {
                cb(calculatedNode);
            });
        }
    }

    return true;
}

float ProfilingSequencer::computeTargetMetric(const std::vector<float>& buffer, const juce::String& blockType)
{
    if (buffer.empty())
        return 0.0f;

    double sRate = audioEngine.getSampleRate();
    if (sRate < 1000.0) sRate = 48000.0;

    std::vector<std::vector<float>> singlePass { buffer };

    if (blockType == "SpectrumFilter")
    {
        // Encontrar la energía RMS o pico de respuesta en alta frecuencia
        double sumSq = 0.0;
        for (float s : buffer) sumSq += static_cast<double>(s * s);
        float rms = static_cast<float>(std::sqrt(sumSq / buffer.size()));
        return (rms > 1e-5f) ? (20.0f * std::log10(rms)) : -100.0f;
    }
    if (blockType == "AmplitudeGain")
    {
        auto gainRes = math::LabAnalyticEngine::analyzeGainTones(singlePass, sRate);
        return gainRes.gainDb.mean;
    }
    if (blockType == "CyclicModulator")
    {
        auto modRes = math::LabAnalyticEngine::analyzeCyclicModulator(singlePass, sRate);
        return modRes.depthPercent.mean;
    }

    // Métrica general de nivel RMS en dB por defecto
    double sumSq = 0.0;
    for (float s : buffer) sumSq += static_cast<double>(s * s);
    float rms = static_cast<float>(std::sqrt(sumSq / buffer.size()));
    return (rms > 1e-5f) ? (20.0f * std::log10(rms)) : -100.0f;
}

} // namespace abdaudiolab::core
