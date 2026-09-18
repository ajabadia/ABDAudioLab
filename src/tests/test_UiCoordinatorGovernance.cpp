#include <catch2/catch_test_macros.hpp>
#include "../measurement/CoordinatorStateMachine.h"
#include "../gui/SessionExecutionCoordinator.h"
#include "../gui/OperatorStepModalDialog.h"
#include "../core/ProfilingSequencer.h"
#include "../core/SessionManager.h"
#include "../gui/SoundIdCurvePlotter.h"
#include "../gui/SoundIdSuiteList.h"
#include "../audio/LabAudioEngine.h"
#include "../hardware/MockHardwareController.h"
#include "../core/HardwareManager.h"
#include "../core/plugins/PluginHardwareContractAdapter.h"
#include "../gui/ExportReportPanel.h"

using namespace abdaudiolab;
using namespace abdaudiolab::measurement;

TEST_CASE("UI Governance: Interaction Mode Switching & Execution Guards", "[ui_governance]")
{
    CoordinatorStateMachine sm;
    CoordinatorContext ctx;
    ctx.mode = WorkspaceInteractionMode::Guided;
    ctx.hasActiveSession = true;
    ctx.profileSha256Verified = true;
    ctx.stimulusReady = true;

    // 1. Initial passive states allow switching mode
    REQUIRE(sm.isModeChangeAllowed());
    sm.dispatch(CoordinatorEvent::SelectProfile, ctx, "sess_gov_1");
    REQUIRE(sm.isModeChangeAllowed());
    sm.dispatch(CoordinatorEvent::PrepareSession, ctx, "sess_gov_1");
    REQUIRE(sm.isModeChangeAllowed());

    // 2. Active execution states strictly block mode change
    ctx.isManualControlRequired = true;
    sm.dispatch(CoordinatorEvent::AwaitingManualPrompt, ctx, "sess_gov_1");
    sm.dispatch(CoordinatorEvent::ConfirmManualControl, ctx, "sess_gov_1");
    REQUIRE(sm.getState() == CoordinatorState::Capturing);

    // During Capturing, mode change is prohibited
    REQUIRE_FALSE(sm.isModeChangeAllowed());
    std::string reason = sm.getRejectionReasonForAction("change_mode", ctx);
    REQUIRE(reason == "Cambiar modo: no disponible durante la captura o validacion activa");

    // Advance to Validation & Persistence
    ctx.rawSha256Verified = true;
    sm.dispatch(CoordinatorEvent::CaptureFinished, ctx, "sess_gov_1");
    REQUIRE(sm.getState() == CoordinatorState::Validating);
    REQUIRE_FALSE(sm.isModeChangeAllowed());

    sm.dispatch(CoordinatorEvent::ValidationPassed, ctx, "sess_gov_1");
    REQUIRE(sm.getState() == CoordinatorState::Persisting);
    REQUIRE_FALSE(sm.isModeChangeAllowed());

    sm.dispatch(CoordinatorEvent::PersistenceSucceeded, ctx, "sess_gov_1");
    REQUIRE(sm.getState() == CoordinatorState::PointCompleted);
    // Point completed: passive again, mode change is allowed
    REQUIRE(sm.isModeChangeAllowed());
}

TEST_CASE("UI Governance: Blocked Action Explanations & Tooltip Text", "[ui_governance]")
{
    CoordinatorStateMachine sm;
    CoordinatorContext ctx;

    SECTION("Confirm manual action reasons")
    {
        // No session: confirm blocked
        REQUIRE_FALSE(sm.isManualConfirmationAllowed());
        std::string r1 = sm.getRejectionReasonForAction("confirm", ctx);
        REQUIRE(r1 == "Confirmar: no hay paso manual pendiente");

        // During capturing
        ctx.hasActiveSession = true;
        ctx.profileSha256Verified = true;
        sm.dispatch(CoordinatorEvent::SelectProfile, ctx, "sess_gov_2");
        sm.dispatch(CoordinatorEvent::PrepareSession, ctx, "sess_gov_2");
        ctx.isManualControlRequired = true;
        sm.dispatch(CoordinatorEvent::AwaitingManualPrompt, ctx, "sess_gov_2");
        sm.dispatch(CoordinatorEvent::ConfirmManualControl, ctx, "sess_gov_2");
        REQUIRE(sm.getState() == CoordinatorState::Capturing);

        std::string r2 = sm.getRejectionReasonForAction("confirm", ctx);
        REQUIRE(r2 == "Confirmar: no se puede confirmar mientras la grabacion esta activa");
    }

    SECTION("Capture action reasons")
    {
        // Stimulus not ready
        ctx.stimulusReady = false;
        std::string r1 = sm.getRejectionReasonForAction("capture", ctx);
        REQUIRE(r1 == "Capturar: prepara primero el estimulo");

        // During capturing
        ctx.stimulusReady = true;
        ctx.hasActiveSession = true;
        ctx.profileSha256Verified = true;
        sm.dispatch(CoordinatorEvent::SelectProfile, ctx, "sess_gov_3");
        sm.dispatch(CoordinatorEvent::PrepareSession, ctx, "sess_gov_3");
        ctx.mode = WorkspaceInteractionMode::Free;
        sm.dispatch(CoordinatorEvent::CaptureStarted, ctx, "sess_gov_3");
        REQUIRE(sm.getState() == CoordinatorState::Capturing);

        std::string r2 = sm.getRejectionReasonForAction("capture", ctx);
        REQUIRE(r2 == "Capturar: grabacion en curso");
    }

    SECTION("Profile change action reasons")
    {
        ctx.hasActiveSession = true;
        ctx.profileSha256Verified = true;
        sm.dispatch(CoordinatorEvent::SelectProfile, ctx, "sess_gov_4");
        sm.dispatch(CoordinatorEvent::PrepareSession, ctx, "sess_gov_4");
        ctx.isManualControlRequired = true;
        sm.dispatch(CoordinatorEvent::AwaitingManualPrompt, ctx, "sess_gov_4");
        sm.dispatch(CoordinatorEvent::ConfirmManualControl, ctx, "sess_gov_4");

        REQUIRE_FALSE(sm.isProfileChangeAllowed());
        std::string r = sm.getRejectionReasonForAction("change_profile", ctx);
        REQUIRE(r == "Cambiar perfil: no disponible durante la grabacion o medicion activa");
    }

    SECTION("Cancellation reasons")
    {
        // No session: cancellation blocked
        REQUIRE_FALSE(sm.isCancellationAllowed());
        std::string r1 = sm.getRejectionReasonForAction("cancel", ctx);
        REQUIRE(r1 == "Cancelar: no hay sesion activa que cancelar");

        // Session ready: cancellation allowed
        ctx.hasActiveSession = true;
        ctx.profileSha256Verified = true;
        sm.dispatch(CoordinatorEvent::SelectProfile, ctx, "sess_gov_5");
        sm.dispatch(CoordinatorEvent::PrepareSession, ctx, "sess_gov_5");
        REQUIRE(sm.isCancellationAllowed());
        std::string r2 = sm.getRejectionReasonForAction("cancel", ctx);
        REQUIRE(r2.empty());
    }
}

TEST_CASE("UI Governance: Free Mode Rigorous Unknown Control Semantics", "[ui_governance]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHardware;
    core::ProfilingSequencer sequencer(audioEngine, mockHardware);
    core::SessionManager sessionManager;
    gui::SoundIdCurvePlotter curvePlotter;

    gui::SessionExecutionCoordinator coordinator(sequencer, sessionManager, curvePlotter);

    core::HardwareContract contract;
    contract.id = "analogue_fuzz_pedal";
    contract.displayName = "Classic Fuzz Box";
    contract.deviceType = "ANALOGUE_PEDAL";

    coordinator.initializeMeasurementSession(contract, "fuzz_core", "a1b2c3d4e5f6");
    coordinator.setWorkspaceInteractionMode(WorkspaceInteractionMode::Free);

    SECTION("Ad-hoc capture without specifying controls produces unknown without inventing data")
    {
        // Operator clicks "Capturar toma libre" without declaring knob positions
        coordinator.triggerFreeCapture({});

        REQUIRE(coordinator.getCoordinatorState() == CoordinatorState::Capturing);
        const auto* sess = coordinator.getActiveMeasurementSession();
        REQUIRE(sess != nullptr);
        REQUIRE_FALSE(sess->controlStates.empty());

        const auto& snap = sess->controlStates.back();
        REQUIRE(snap.confirmationStatus == "unknown");
        REQUIRE(snap.displayValue == "Posición no declarada");
        REQUIRE_FALSE(snap.normalizedValue.has_value());
    }

    SECTION("Ad-hoc capture with operator declared controls preserves exact values")
    {
        ControlStateSnapshot knobSnap;
        knobSnap.controlId = "FUZZ_GAIN";
        knobSnap.confirmationStatus = "user_supplied";
        knobSnap.displayValue = "7.5 / 10.0";
        knobSnap.normalizedValue = 0.75f;

        coordinator.triggerFreeCapture({ knobSnap });

        const auto* sess = coordinator.getActiveMeasurementSession();
        REQUIRE(sess != nullptr);
        REQUIRE_FALSE(sess->controlStates.empty());

        const auto& snap = sess->controlStates.back();
        REQUIRE(snap.controlId == "FUZZ_GAIN");
        REQUIRE(snap.confirmationStatus == "user_supplied");
        REQUIRE(snap.displayValue == "7.5 / 10.0");
        REQUIRE(snap.normalizedValue.has_value());
        REQUIRE(*snap.normalizedValue == 0.75f);
    }
}

TEST_CASE("UI Governance: Keyboard Handshake & Modal Focus Guards", "[ui_governance]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    gui::OperatorStepModalDialog modal;
    modal.showStepPrompt(nullptr, "Alignment Step", 1, 5, {});

    bool acceptedFired = false;
    bool cancelFired = false;
    modal.onAccept = [&] { acceptedFired = true; };
    modal.onCancel = [&] { cancelFired = true; };

    SECTION("Space key does not confirm while measurement is active")
    {
        modal.setMeasuringState(true);
        juce::KeyPress space(juce::KeyPress::spaceKey);
        bool handled = modal.keyPressed(space, &modal);

        REQUIRE_FALSE(handled);
        REQUIRE_FALSE(acceptedFired);
    }

    SECTION("Space key confirms when awaiting manual confirmation and modal is idle")
    {
        modal.setMeasuringState(false);
        modal.setAutomatedMode(false);

        juce::KeyPress space(juce::KeyPress::spaceKey);
        bool handled = modal.keyPressed(space, &modal);

        REQUIRE(handled);
        REQUIRE(acceptedFired);
    }

    SECTION("Escape key triggers safe cancellation")
    {
        modal.setMeasuringState(false);
        juce::KeyPress esc(juce::KeyPress::escapeKey);
        bool handled = modal.keyPressed(esc, &modal);

        REQUIRE(handled);
        REQUIRE(cancelFired);
    }
}

namespace
{
class GovernanceMockVst3Processor : public juce::AudioProcessor
{
public:
    GovernanceMockVst3Processor()
    {
        addParameter(new juce::AudioParameterFloat({"cutoff", 1}, "Cutoff Filter", 20.0f, 20000.0f, 1000.0f));
        addParameter(new juce::AudioParameterFloat({"resonance", 1}, "Resonance", 0.0f, 1.0f, 0.2f));
        addParameter(new juce::AudioParameterFloat({"drive", 1}, "Drive Saturation", 0.0f, 1.0f, 0.1f));
    }
    const juce::String getName() const override { return "Mock VST3 Dexed"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};
} // namespace

TEST_CASE("UI Governance: VST3 Dynamic Contract to MeasurementSession Initialization", "[ui_governance]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    GovernanceMockVst3Processor mockVst;
    juce::PluginDescription desc;
    desc.name = "Dexed Mock";
    desc.pluginFormatName = "VST3";
    desc.fileOrIdentifier = "C:/Program Files/Common Files/VST3/Dexed.vst3";
    desc.isInstrument = true;

    // 1. Convert VST3 to HardwareContract
    core::HardwareContract contract = core::PluginHardwareContractAdapter::createContractFromPlugin(mockVst, desc);
    REQUIRE(contract.deviceType == "SOFTWARE_PLUGIN");
    REQUIRE(contract.functions.size() == 1);
    REQUIRE(contract.functions[0].controls.size() == 3);

    // 2. Register in HardwareContractRegistry
    core::HardwareContractRegistry registry;
    registry.registerContract(contract);
    const auto* found = registry.findContractById(contract.id);
    REQUIRE(found != nullptr);
    REQUIRE(found->displayName == "Dexed Mock");

    // 3. Initialize MeasurementSession in Coordinator
    hardware::MockHardwareController mockHw;
    audio::LabAudioEngine engine;
    core::ProfilingSequencer seq(engine, mockHw);
    core::SessionManager sessMgr;
    gui::SoundIdCurvePlotter plotter;

    gui::SessionExecutionCoordinator coordinator(seq, sessMgr, plotter);
    core::HardwareManager hwMgr;
    hwMgr.getContractRegistry().registerContract(contract);
    coordinator.setHardwareContext(&hwMgr, juce::String(contract.id));

    std::string profSha = "vst3_dexed_sha256_mock_test";
    coordinator.initializeMeasurementSession(contract, juce::String(contract.functions[0].id), juce::String(profSha));

    // Verify session state is initialized and ready
    REQUIRE(coordinator.getActiveMeasurementSession() != nullptr);
    REQUIRE(coordinator.getCoordinatorState() == CoordinatorState::SessionReady);
    REQUIRE(coordinator.isModeChangeAllowed());
    REQUIRE(coordinator.isDirectCaptureAllowed());
    REQUIRE_FALSE(coordinator.isManualConfirmationAllowed());
}

TEST_CASE("UI Governance: Step-Based Action Invariants (Calibrate vs RunSession vs Export)", "[ui_governance]")
{
    CoordinatorStateMachine sm;
    CoordinatorContext ctx;
    ctx.mode = WorkspaceInteractionMode::Free;

    SECTION("Calibrate/Setup step: no active session strictly blocks direct capture")
    {
        ctx.hasActiveSession = false;
        ctx.stimulusReady = false;

        REQUIRE_FALSE(sm.isDirectCaptureAllowed(ctx.stimulusReady));
        std::string reason = sm.getRejectionReasonForAction("capture", ctx);
        REQUIRE(reason == "Capturar: prepara primero el estimulo");
    }

    SECTION("RunSession step: active session and stimulus ready allows direct capture in Free mode")
    {
        ctx.hasActiveSession = true;
        ctx.stimulusReady = true;
        ctx.profileSha256Verified = true;

        sm.dispatch(CoordinatorEvent::SelectProfile, ctx, "sess_inv_1");
        sm.dispatch(CoordinatorEvent::PrepareSession, ctx, "sess_inv_1");
        REQUIRE(sm.getState() == CoordinatorState::SessionReady);

        REQUIRE(sm.isDirectCaptureAllowed(ctx.stimulusReady));
    }

    SECTION("ExportReport step: completed session strictly blocks capture")
    {
        ctx.hasActiveSession = true;
        ctx.stimulusReady = true;
        ctx.profileSha256Verified = true;

        sm.dispatch(CoordinatorEvent::SelectProfile, ctx, "sess_inv_2");
        sm.dispatch(CoordinatorEvent::PrepareSession, ctx, "sess_inv_2");
        sm.dispatch(CoordinatorEvent::CaptureStarted, ctx, "sess_inv_2");
        ctx.rawSha256Verified = true;
        sm.dispatch(CoordinatorEvent::CaptureFinished, ctx, "sess_inv_2");
        sm.dispatch(CoordinatorEvent::ValidationPassed, ctx, "sess_inv_2");
        sm.dispatch(CoordinatorEvent::PersistenceSucceeded, ctx, "sess_inv_2");
        sm.dispatch(CoordinatorEvent::NextPointOrFinish, ctx, "sess_inv_2");

        REQUIRE(sm.getState() == CoordinatorState::SessionCompleted);
        REQUIRE_FALSE(sm.isDirectCaptureAllowed(ctx.stimulusReady));
        std::string reason = sm.getRejectionReasonForAction("capture", ctx);
        REQUIRE(reason == "Capturar: sesion ya finalizada");
    }
}

TEST_CASE("UI Governance: Plugin Instrument ExcitationMode & MIDI Delivery", "[ui_governance]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    class MockSynthProcessor : public juce::AudioPluginInstance
    {
    public:
        MockSynthProcessor() : juce::AudioPluginInstance(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
        const juce::String getName() const override { return "MockDexedSynth"; }
        void prepareToPlay(double, int) override {}
        void releaseResources() override {}
        void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
        {
            lastMidiEventsReceived = midi.getNumEvents();
            for (const auto meta : midi)
            {
                auto m = meta.getMessage();
                if (m.isNoteOn()) noteOnReceived = true;
                if (m.isNoteOff()) noteOffReceived = true;
            }
            // Generate non-silent audio on note on
            if (noteOnReceived && !noteOffReceived)
            {
                buffer.clear();
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    auto* w = buffer.getWritePointer(ch);
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        w[i] = 0.5f * std::sin(static_cast<float>(i) * 0.1f);
                }
            }
        }
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String&) override {}
        void getStateInformation(juce::MemoryBlock&) override {}
        void setStateInformation(const void*, int) override {}
        void fillInPluginDescription(juce::PluginDescription& desc) const override
        {
            desc.name = "MockDexedSynth";
            desc.pluginFormatName = "VST3";
            desc.isInstrument = true;
        }

        int lastMidiEventsReceived { 0 };
        bool noteOnReceived { false };
        bool noteOffReceived { false };
    };

    MockSynthProcessor synth;
    juce::PluginDescription desc;
    desc.name = "MockDexedSynth";
    desc.pluginFormatName = "VST3";
    desc.isInstrument = true;
    desc.fileOrIdentifier = "d:/plugins/MockDexed.vst3";

    // 1. Contract generation with explicit ExcitationMode::MidiNotes
    auto contract = core::PluginHardwareContractAdapter::createContractFromPlugin(synth, desc);
    REQUIRE(contract.functions.size() == 1);
    REQUIRE(contract.functions[0].excitationMode == core::ExcitationMode::MidiNotes);
    REQUIRE(contract.functions[0].measurementRecipe.excitationMode == core::ExcitationMode::MidiNotes);

    // 2. HardwareManager autonomous synth recognition
    core::HardwareManager hwManager;
    hwManager.getContractRegistry().registerContract(contract);
    REQUIRE(hwManager.isAutonomousSynth(juce::String(contract.id), juce::String(contract.functions[0].id)));

    // 3. AudioEngine & Dispatcher MIDI Sink Integration
    audio::LabAudioEngine engine;
    engine.setActivePluginInstance(&synth, 48000.0, 512);

    core::ProfilingHardwareDispatcher dispatcher;
    dispatcher.setMidiSinkCallback([&engine](const juce::MidiMessage& msg) {
        engine.postLiveMidiMessage(msg);
    });

    // Send Note On
    dispatcher.sendNoteOn(1, 60, 0.8f);

    float outL[512] = { 0.0f };
    float outR[512] = { 0.0f };
    float* outChannels[2] = { outL, outR };
    const float inL[512] = { 0.0f };
    const float inR[512] = { 0.0f };
    const float* inChannels[2] = { inL, inR };
    juce::AudioIODeviceCallbackContext ioCtx;

    engine.audioDeviceIOCallbackWithContext(inChannels, 2, outChannels, 2, 512, ioCtx);

    REQUIRE(synth.noteOnReceived);
    REQUIRE(engine.getPluginNoteOnCount() == 1);
    REQUIRE(engine.getLastNoteOnNumber() == 60);
    REQUIRE(engine.getLastPluginOutputRms() > 0.01f); // Audio is non-silent!

    // Send Note Off
    dispatcher.sendNoteOff(1, 60, 0.0f);
    engine.audioDeviceIOCallbackWithContext(inChannels, 2, outChannels, 2, 512, ioCtx);

    REQUIRE(synth.noteOffReceived);
    REQUIRE(engine.getPluginNoteOffCount() == 1);

    engine.setActivePluginInstance(nullptr);
}

TEST_CASE("UI Governance: Pause and Cancel Silence Active Notes", "[ui_governance]")
{
    int allNotesOffCount = 0;
    core::ProfilingHardwareDispatcher dispatcher;
    dispatcher.setMidiSinkCallback([&allNotesOffCount](const juce::MidiMessage& msg) {
        if (msg.isAllNotesOff())
            allNotesOffCount++;
    });

    dispatcher.sendAllNotesOff(1);
    REQUIRE(allNotesOffCount == 1);
}

TEST_CASE("UI Governance: Export Report UTF-8 String Integrity (No â□¢ mojibake)", "[ui_governance]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    gui::ExportReportPanel panel;
    panel.updateMetrics(45.2f, -86.5f, 0.008f, 5, 12.5f);

    juce::String bulletUtf8 = juce::String::fromUTF8(u8"•");
    std::string bulletStd = bulletUtf8.toStdString();

    REQUIRE(bulletStd.size() == 3);
    REQUIRE(static_cast<unsigned char>(bulletStd[0]) == 0xE2);
    REQUIRE(static_cast<unsigned char>(bulletStd[1]) == 0x80);
    REQUIRE(static_cast<unsigned char>(bulletStd[2]) == 0xA2);

    std::string mojibakeMarker = "\xC3\xA2";
    REQUIRE(bulletStd.find(mojibakeMarker) == std::string::npos);
}

TEST_CASE("UI Governance: SuiteList Resized Never Shows Run Button When Hidden", "[ui_governance]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    gui::SoundIdSuiteList suiteList;
    suiteList.setRunButtonVisible(false);
    REQUIRE_FALSE(suiteList.isRunButtonVisible());

    // Trigger multiple resized events
    suiteList.setSize(800, 600);
    REQUIRE_FALSE(suiteList.isRunButtonVisible());

    suiteList.setSize(1200, 400);
    REQUIRE_FALSE(suiteList.isRunButtonVisible());

    suiteList.setCollapsed(true);
    suiteList.setSize(600, 300);
    REQUIRE_FALSE(suiteList.isRunButtonVisible());

    suiteList.setCollapsed(false);
    suiteList.setSize(1000, 700);
    REQUIRE_FALSE(suiteList.isRunButtonVisible());
}

TEST_CASE("UI Governance: Session Cancelled Rearms Back to SessionReady", "[ui_governance]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine engine;
    hardware::MockHardwareController mockHardware;
    core::ProfilingSequencer sequencer(engine, mockHardware);
    core::SessionManager sessionManager;
    gui::SoundIdCurvePlotter plotter;

    gui::SessionExecutionCoordinator coordinator(sequencer, sessionManager, plotter);

    core::HardwareContract contract;
    contract.id = "dexed_synth";
    contract.displayName = "Dexed VST3";
    coordinator.initializeMeasurementSession(contract, "main_proc", "sha256_mock");

    REQUIRE(coordinator.getCoordinatorState() == measurement::CoordinatorState::SessionReady);

    // Cancel the session
    coordinator.triggerStopSession();
    REQUIRE(coordinator.getCoordinatorState() == measurement::CoordinatorState::Aborted);

    // Rearm session
    coordinator.rearmSession();
    REQUIRE(coordinator.getCoordinatorState() == measurement::CoordinatorState::SessionReady);
}

TEST_CASE("UI Governance: Mode Change Between Guided and Lab Blocked During Capture", "[ui_governance]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    audio::LabAudioEngine engine;
    hardware::MockHardwareController mockHardware;
    core::ProfilingSequencer sequencer(engine, mockHardware);
    core::SessionManager sessionManager;
    gui::SoundIdCurvePlotter plotter;

    gui::SessionExecutionCoordinator coordinator(sequencer, sessionManager, plotter);

    core::HardwareContract contract;
    contract.id = "dexed_synth";
    contract.displayName = "Dexed VST3";
    coordinator.initializeMeasurementSession(contract, "main_proc", "sha256_mock");

    REQUIRE(coordinator.isModeChangeAllowed());
    REQUIRE(coordinator.switchWorkspaceInteractionMode(measurement::WorkspaceInteractionMode::Free));
    REQUIRE(coordinator.getWorkspaceInteractionMode() == measurement::WorkspaceInteractionMode::Free);

    // Trigger capture in free mode
    coordinator.triggerFreeCapture();
    REQUIRE(coordinator.getCoordinatorState() == measurement::CoordinatorState::Capturing);

    // While capturing, mode change MUST be blocked
    REQUIRE_FALSE(coordinator.isModeChangeAllowed());
    REQUIRE_FALSE(coordinator.switchWorkspaceInteractionMode(measurement::WorkspaceInteractionMode::Guided));
    REQUIRE(coordinator.getWorkspaceInteractionMode() == measurement::WorkspaceInteractionMode::Free);
}


