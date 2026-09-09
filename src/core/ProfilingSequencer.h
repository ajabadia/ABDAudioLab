#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <atomic>
#include <functional>
#include <memory>

#include "ProfilingSession.h"
#include "HardwareContractRegistry.h"
#include "../audio/LabAudioEngine.h"
#include "../hardware/HardwareController.h"
#include "../export/LutExporter.h"
#include "../math/ModulationMatrixProfile.h"
#include "../math/NoiseFloorTracker.h"

#include "ProfilingHardwareDispatcher.h"
#include "ProfilingAudioCapture.h"

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
    using OperatorStepCallback = std::function<void(const TestCase& currentTestCase, int stepIndex, int totalSteps)>;
    using PreScanCallback = std::function<void(const math::PreScanResult& result)>;

    ProfilingSequencer(audio::LabAudioEngine& audioEngine,
                       hardware::IHardwareController& hardwareController);
    ~ProfilingSequencer() override;

    void setProgressCallback(ProgressCallback cb) { progressCallback = std::move(cb); }
    void setPointMeasuredCallback(PointMeasuredCallback cb) { pointMeasuredCallback = std::move(cb); }
    void setTestIndexCallback(TestIndexCallback cb) { testIndexCallback = std::move(cb); }
    void setOperatorStepCallback(OperatorStepCallback cb) { operatorStepCallback = std::move(cb); }
    void setPreScanCallback(PreScanCallback cb) { preScanCallback = std::move(cb); }
    using ManualPromptCallback = std::function<void(const juce::String& description)>;
    void setManualPromptCallback(ManualPromptCallback cb) { manualPromptCallback = std::move(cb); }
    void setLifecycleContract(const HardwareLifecycleContract* contract) noexcept { lifecycleContract = contract; }
    void executeLifecycleActions(const std::vector<HardwareSetupAction>& actions);
    void executeMeasurementRecipe(const MeasurementPresetRecipe& recipe);
    void setHardwareController(hardware::IHardwareController* newHardware) noexcept;

    using ModExcitationType = ProfilingHardwareDispatcher::ModExcitationType;

    struct ModulationProbeContract
    {
        ModExcitationType excitationType { ModExcitationType::Velocity };
        int targetSlotIndex              { 0 };
        int sourceID                     { 0 };
        int destID                       { 0 };
        int controlCCNumber              { 0 };
        int midiChannel                  { 1 };
        juce::String sysexTemplate;     // Ej: "F0 41 10 00 00 00 1B 12 00 00 00 XX F7"
        int settlingDelayMs              { 100 };
        juce::String destinationBlockType; // "SpectrumFilter", "AmplitudeGain", "CyclicModulator"
    };

    using ModulationNodeMeasuredCallback = std::function<void(const math::ModulationNode& node)>;
    void setModulationNodeMeasuredCallback(ModulationNodeMeasuredCallback cb) { onModulationNodeMeasured = std::move(cb); }

    /**
     * Access to modular sub-components
     */
    ProfilingHardwareDispatcher& getHardwareDispatcher() noexcept { return *hardwareDispatcher; }
    ProfilingAudioCapture& getAudioCapture() noexcept { return *audioCapture; }

    /**
     * Ejecuta una ráfaga agnóstica de 4 puntos de control, calculando deltas contra reposo
     * y poblando de forma segura la matriz dispersa de modulación.
     */
    bool runUniversalModulationProbe(const ModulationProbeContract& contract,
                                     const std::vector<float>& restingAudio,
                                     math::ModulationMatrixProfile& targetProfile);

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

    [[nodiscard]] bool isSafetyAborted() const noexcept { return safetyAborted.load(std::memory_order_acquire); }
    [[nodiscard]] bool isLinearBypassDetected() const noexcept { return linearBypassDetected.load(std::memory_order_acquire); }
    [[nodiscard]] bool isAdaptiveOptimizationApplied() const noexcept { return adaptiveOptimizationApplied.load(std::memory_order_acquire); }

    void run() override;

private:
    audio::LabAudioEngine& audioEngine;
    hardware::IHardwareController* hardware { nullptr };

    std::unique_ptr<ProfilingHardwareDispatcher> hardwareDispatcher;
    std::unique_ptr<ProfilingAudioCapture> audioCapture;

    ProfilingSession activeSession;
    juce::File exportDir;
    juce::String exportBaseName;

    std::atomic<SequencerState> currentState { SequencerState::Idle };
    std::atomic<bool> operatorConfirmed { false };
    std::atomic<bool> repeatRequested { false };
    std::atomic<bool> stepBackRequested { false };
    std::atomic<bool> safetyAborted { false };
    std::atomic<bool> linearBypassDetected { false };
    std::atomic<bool> adaptiveOptimizationApplied { false };

    ProgressCallback progressCallback;
    PointMeasuredCallback pointMeasuredCallback;
    TestIndexCallback testIndexCallback;
    OperatorStepCallback operatorStepCallback;
    PreScanCallback preScanCallback;
    ManualPromptCallback manualPromptCallback;
    const HardwareLifecycleContract* lifecycleContract { nullptr };
    std::vector<exporting::MeasuredPoint> measuredPoints;
    math::NoiseFloorTracker noiseTracker;

    void notifyProgress(float progress, const juce::String& task, SequencerState state);
    void saveSessionCheckpoint();

    ModulationNodeMeasuredCallback onModulationNodeMeasured;
    float computeTargetMetric(const std::vector<float>& buffer, const juce::String& blockType);
};

} // namespace abdaudiolab::core

