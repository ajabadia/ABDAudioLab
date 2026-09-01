#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <atomic>
#include <functional>
#include <memory>

#include "ProfilingSession.h"
#include "../audio/LabAudioEngine.h"
#include "../hardware/HardwareController.h"
#include "../export/LutExporter.h"

namespace abdaudiolab::core
{

enum class SequencerState
{
    Idle,
    LineCalibration,
    InitiateTestCase,
    WaitForStabilization,
    WaitingForOperator,
    InjectStimulus,
    CaptureAndAnalyze,
    InterludeNoiseFloor,
    ExportDataAndCleanup,
    Finished,
    ErrorState
};

class ProfilingSequencer : public juce::Thread
{
public:
    using ProgressCallback = std::function<void(float progress0to1, const juce::String& currentTask, SequencerState state)>;

    ProfilingSequencer(audio::LabAudioEngine& audioEngine,
                       hardware::IHardwareController& hardwareController);
    ~ProfilingSequencer() override;

    void setProgressCallback(ProgressCallback cb) { progressCallback = std::move(cb); }

    bool startSession(const ProfilingSession& session,
                      const juce::File& outputDirectory,
                      const juce::String& baseExportName);

    void stopSession();
    void confirmOperatorStep(); // Called when manual operator confirms knob setting

    [[nodiscard]] SequencerState getCurrentState() const noexcept { return currentState.load(std::memory_order_relaxed); }
    [[nodiscard]] bool isRunningSession() const noexcept { return isThreadRunning(); }
    [[nodiscard]] const std::vector<exporting::MeasuredPoint>& getMeasuredPoints() const noexcept { return measuredPoints; }

    void run() override;

private:
    audio::LabAudioEngine& audioEngine;
    hardware::IHardwareController& hardware;

    ProfilingSession activeSession;
    juce::File exportDir;
    juce::String exportBaseName;

    std::atomic<SequencerState> currentState { SequencerState::Idle };
    std::atomic<bool> operatorConfirmed { false };

    ProgressCallback progressCallback;
    std::vector<exporting::MeasuredPoint> measuredPoints;

    void notifyProgress(float progress, const juce::String& task, SequencerState state);
};

} // namespace abdaudiolab::core
