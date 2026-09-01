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

#include "../math/NoiseFloorTracker.h"

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
    using PointMeasuredCallback = std::function<void(const exporting::MeasuredPoint& pt)>;
    using TestIndexCallback = std::function<void(int queueIndex, int currentPointInTest, int totalPointsInTest)>;

    ProfilingSequencer(audio::LabAudioEngine& audioEngine,
                       hardware::IHardwareController& hardwareController);
    ~ProfilingSequencer() override;

    void setProgressCallback(ProgressCallback cb) { progressCallback = std::move(cb); }
    void setPointMeasuredCallback(PointMeasuredCallback cb) { pointMeasuredCallback = std::move(cb); }
    void setTestIndexCallback(TestIndexCallback cb) { testIndexCallback = std::move(cb); }

    bool startSession(const ProfilingSession& session,
                      const juce::File& outputDirectory,
                      const juce::String& baseExportName);

    void stopSession();
    void confirmOperatorStep(); // Called when manual operator confirms knob setting
    void repeatCurrentStep();   // Repeat the last measured step without losing progress
    void stepBack();            // Return to previous measurement step

    [[nodiscard]] SequencerState getCurrentState() const noexcept { return currentState.load(std::memory_order_relaxed); }
    [[nodiscard]] bool isRunningSession() const noexcept { return isThreadRunning(); }
    [[nodiscard]] const std::vector<exporting::MeasuredPoint>& getMeasuredPoints() const noexcept { return measuredPoints; }
    [[nodiscard]] const math::NoiseFloorTracker& getNoiseTracker() const noexcept { return noiseTracker; }

    void run() override;

private:
    audio::LabAudioEngine& audioEngine;
    hardware::IHardwareController& hardware;

    ProfilingSession activeSession;
    juce::File exportDir;
    juce::String exportBaseName;

    std::atomic<SequencerState> currentState { SequencerState::Idle };
    std::atomic<bool> operatorConfirmed { false };
    std::atomic<bool> repeatRequested { false };
    std::atomic<bool> stepBackRequested { false };

    ProgressCallback progressCallback;
    PointMeasuredCallback pointMeasuredCallback;
    TestIndexCallback testIndexCallback;
    std::vector<exporting::MeasuredPoint> measuredPoints;
    math::NoiseFloorTracker noiseTracker;

    void notifyProgress(float progress, const juce::String& task, SequencerState state);
    void saveSessionCheckpoint();
};

} // namespace abdaudiolab::core
