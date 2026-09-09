#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <vector>
#include "../core/ProfilingSequencer.h"
#include "../math/ModulationMatrixProfile.h"
#include "SoundIdCurvePlotter.h"

namespace abdaudiolab {
namespace core {
    class SessionManager;
    class HardwareManager;
    class ProfilingSession;
}
namespace gui {
    class SoundIdSuiteList;
    class MeasurementHealthPanel;
    class OperatorStepModalDialog;

class SessionExecutionCoordinator : public juce::Component
{
public:
    SessionExecutionCoordinator(core::ProfilingSequencer& seq,
                                core::SessionManager& sm,
                                SoundIdCurvePlotter& plotter);
    ~SessionExecutionCoordinator() override;

    void setCoordinatedViews(SoundIdSuiteList* suiteList,
                             MeasurementHealthPanel* healthPanel,
                             OperatorStepModalDialog* operatorModal,
                             juce::Label* manualPromptLabel,
                             juce::Button* btnStepBack,
                             juce::Button* btnRepeatStep,
                             juce::Button* confirmManualButton);

    void setHardwareContext(core::HardwareManager* hwMgr, const juce::String& selectedHwId);

    void wireSequencerCallbacks();
    void unbindSequencerCallbacks();

    void confirmOperatorStep();
    void repeatCurrentStep();
    void stepBack();

    void triggerStartSession(const core::ProfilingSession& session,
                             const juce::File& exportDir,
                             const juce::String& baseName,
                             bool isPatching = false);
    void triggerStopSession();

    [[nodiscard]] int getTotalPointsMeasured() const noexcept { return totalPointsMeasured; }
    void setTotalPointsMeasured(int count) noexcept { totalPointsMeasured = count; }
    [[nodiscard]] bool getIsPatchingSession() const noexcept { return isPatchingSession; }
    void setIsPatchingSession(bool patching) noexcept { isPatchingSession = patching; }

    // High-level granular callbacks to MainContentComponent
    std::function<void(bool isRunning)> onExecutionStateChanged;
    std::function<void(const juce::String& error)> onExecutionErrorTriggered;
    std::function<void(bool isPatching)> onSessionFinished;
    std::function<void()> onSessionAutoSaveRequested;

private:
    core::ProfilingSequencer& sequencer;
    core::SessionManager& sessionManager;
    SoundIdCurvePlotter& curvePlotter;

    // Coordinated views (Non-owning / Weak pointers)
    SoundIdSuiteList* viewSuiteList                 { nullptr };
    MeasurementHealthPanel* viewHealthPanel         { nullptr };
    OperatorStepModalDialog* viewOperatorModal      { nullptr };
    juce::Label* viewManualPromptLabel              { nullptr };
    juce::Button* viewBtnStepBack                   { nullptr };
    juce::Button* viewBtnRepeatStep                 { nullptr };
    juce::Button* viewConfirmManualButton           { nullptr };

    core::HardwareManager* hardwareManager          { nullptr };
    juce::String currentHardwareId;

    int totalPointsMeasured { 0 };
    bool isPatchingSession  { false };

    void handleOperatorStep(const core::TestCase& tc, int stepIndex, int totalSteps);
    void handleProgress(float progress, const juce::String& task, core::SequencerState state);
    void handlePreScan(const math::PreScanResult& preScan);
    void handleTestIndex(int queueIndex, int currentPoint, int totalPoints);
    void handlePointMeasured(const exporting::MeasuredPoint& pt);
    void handleModulationNodeMeasured(const math::ModulationNode& node);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionExecutionCoordinator)
};

} // namespace gui
} // namespace abdaudiolab
