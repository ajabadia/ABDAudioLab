#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "gui/controllers/SessionIoController.h"
#include "gui/SessionReportManager.h"
#include "core/SessionManager.h"
#include "gui/ConfirmationModalDialog.h"
#include "gui/ExportReportPanel.h"
#include "gui/suite/SuiteDataModels.h"
#include "synth/Sha256.h"

using namespace abdaudiolab;

TEST_CASE("SessionIoController: Session Persistence and Lifecycle Characterization", "[gui][session_io]")
{
    core::SessionManager sessionManager;
    gui::SessionReportManager reportManager;
    gui::ExportReportPanel exportPanel;
    gui::ConfirmationModalDialog confirmModal;

    gui::SessionIoController controller(sessionManager, reportManager, exportPanel, confirmModal);

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("ABDAudioLab_SessionIoTest_" + juce::String(juce::Random::getSystemRandom().nextInt(100000)));
    if (tempDir.exists())
        tempDir.deleteRecursively();
    tempDir.createDirectory();
    controller.setExportDirectory(tempDir);

    // Setup dummy session snapshot context
    core::SessionManifest dummyManifest;
    dummyManifest.hardwareId = "test_synth_hw";
    dummyManifest.hardwareDisplayName = "Hardware Synth MK1";
    dummyManifest.activeFunctionId = "osc_character";
    dummyManifest.activeFunctionName = "Oscillator Character";
    dummyManifest.sampleRate = 48000.0;
    dummyManifest.operatorNotes = "Test session notes";

    core::ProfilingMetadata dummyMeta;
    dummyMeta.hardwareName = "Hardware Synth MK1";
    dummyMeta.targetModule = "osc_character";
    dummyMeta.sampleRate = 48000.0;

    controller.setSessionContextProvider([&]() -> gui::SessionSaveContext {
        gui::SessionSaveContext ctx;
        ctx.manifest = dummyManifest;
        ctx.metadata = dummyMeta;
        ctx.suggestedFileName = "Hardware_Synth_MK1.abdlabtest";
        return ctx;
    });

    SECTION("saveSessionToFile writes valid package, updates dirty flag and activeFile only on success")
    {
        sessionManager.setDirty(true);
        exporting::MeasuredPoint pt;
        pt.pointId = "PT_01";
        pt.snrDb = 55.0f;
        sessionManager.addMeasuredPoint(pt);

        juce::File targetFile = tempDir.getChildFile("test_session.abdlabtest");
        bool savedCallbackFired = false;
        controller.onSessionSaved = [&](const juce::File& f) {
            savedCallbackFired = true;
            REQUIRE(f == targetFile);
        };

        bool ok = controller.saveSessionToFile(targetFile);
        REQUIRE(ok);
        REQUIRE(targetFile.existsAsFile());
        REQUIRE(targetFile.getSize() > 0);
        REQUIRE(savedCallbackFired);
        REQUIRE_FALSE(sessionManager.isDirty());
        REQUIRE(sessionManager.getActiveSessionFile() == targetFile);
    }

    SECTION("Failed write keeps dirty=true and does not change activeFile")
    {
        sessionManager.setDirty(true);
        juce::File originalActive = tempDir.getChildFile("original.abdlabtest");
        sessionManager.setActiveSessionFile(originalActive);

        // Path inside non-existent impossible root
        juce::File invalidFile("Z:/non_existent_drive_9999/session.abdlabtest");
        bool ok = controller.saveSessionToFile(invalidFile);
        REQUIRE_FALSE(ok);
        REQUIRE(sessionManager.isDirty());
        REQUIRE(sessionManager.getActiveSessionFile() == originalActive);
    }

    SECTION("Ctrl+S with active file saves directly without dialog and clears dirty")
    {
        juce::File existingFile = tempDir.getChildFile("active_session.abdlabtest");
        // Initial clean save to make file exist
        REQUIRE(controller.saveSessionToFile(existingFile));
        sessionManager.setActiveSessionFile(existingFile);

        // Mutate session to dirty
        sessionManager.setDirty(true);
        exporting::MeasuredPoint pt2;
        pt2.pointId = "PT_02";
        pt2.snrDb = 62.0f;
        sessionManager.addMeasuredPoint(pt2);

        bool savedCallbackFired = false;
        controller.onSessionSaved = [&](const juce::File& f) {
            savedCallbackFired = true;
            REQUIRE(f == existingFile);
        };

        controller.handleSaveSession();

        REQUIRE(savedCallbackFired);
        REQUIRE_FALSE(sessionManager.isDirty());
        REQUIRE(sessionManager.getActiveSessionFile() == existingFile);
    }

    SECTION("Round-trip save and load preserves manifest, points, and file fixity")
    {
        sessionManager.resetSession();
        exporting::MeasuredPoint pt1;
        pt1.pointId = "P1";
        pt1.param1Normalized = 0.25f;
        pt1.snrDb = 48.0f;
        pt1.thdPercent = 0.012f;
        sessionManager.addMeasuredPoint(pt1);

        juce::File file = tempDir.getChildFile("roundtrip.abdlabtest");
        REQUIRE(controller.saveSessionToFile(file));

        juce::MemoryBlock fileContent;
        file.loadFileAsData(fileContent);
        auto hash1 = synth::Sha256::computeHex(fileContent.getData(), fileContent.getSize());
        REQUIRE(hash1.length() == 64);

        // Reset memory state
        sessionManager.resetSession();
        REQUIRE(sessionManager.getPointCount() == 0);

        // Load via sessionManager with file saved by controller
        juce::String err;
        bool loaded = sessionManager.loadSessionFromPackage(file, err);
        REQUIRE(loaded);
        REQUIRE(err.isEmpty());
        REQUIRE(sessionManager.getPointCount() == 1);
        REQUIRE(sessionManager.getPoint(0)->pointId == "P1");
        REQUIRE(sessionManager.getPoint(0)->snrDb == Catch::Approx(48.0f));
        REQUIRE(sessionManager.getManifest().hardwareDisplayName == "Hardware Synth MK1");
    }

    SECTION("Invalid package load does not destroy current session state")
    {
        sessionManager.resetSession();
        exporting::MeasuredPoint currentPt;
        currentPt.pointId = "STAYS_VALID";
        sessionManager.addMeasuredPoint(currentPt);

        juce::File corruptFile = tempDir.getChildFile("corrupt.abdlabtest");
        corruptFile.replaceWithText("not a valid zip package content");

        juce::String err;
        bool loaded = sessionManager.loadSessionFromPackage(corruptFile, err);
        REQUIRE_FALSE(loaded);
        REQUIRE(err.isNotEmpty());
        REQUIRE(sessionManager.getPointCount() == 1);
        REQUIRE(sessionManager.getPoint(0)->pointId == "STAYS_VALID");
    }

    SECTION("promptNewSession directly triggers reset when session is clean")
    {
        sessionManager.setDirty(false);
        bool resetCalled = false;
        controller.promptNewSession(nullptr, [&]() {
            resetCalled = true;
        });
        REQUIRE(resetCalled);
    }

    SECTION("promptNewSession with dirty session handles Cancel, Discard, and Save")
    {
        // 1. Cancel does nothing
        {
            sessionManager.setDirty(true);
            bool resetCalled = false;
            controller.promptNewSession(nullptr, [&]() {
                resetCalled = true;
            });
            confirmModal.simulateResult(gui::ConfirmationModalDialog::Result::Cancel);
            REQUIRE_FALSE(resetCalled);
            REQUIRE(sessionManager.isDirty());
        }

        // 2. Discard (Secondary) triggers reset
        {
            sessionManager.setDirty(true);
            bool resetCalled = false;
            controller.promptNewSession(nullptr, [&]() {
                resetCalled = true;
            });
            confirmModal.simulateResult(gui::ConfirmationModalDialog::Result::Secondary);
            REQUIRE(resetCalled);
        }

        // 3. Save (Primary) with existing active file saves and triggers reset
        {
            juce::File activeFile = tempDir.getChildFile("active_for_new.abdlabtest");
            REQUIRE(controller.saveSessionToFile(activeFile));
            sessionManager.setActiveSessionFile(activeFile);
            sessionManager.setDirty(true);

            bool resetCalled = false;
            controller.promptNewSession(nullptr, [&]() {
                resetCalled = true;
            });
            confirmModal.simulateResult(gui::ConfirmationModalDialog::Result::Primary);

            REQUIRE(resetCalled);
            REQUIRE_FALSE(sessionManager.isDirty());
        }
    }

    SECTION("confirmAndExit directly triggers exit when session is clean")
    {
        sessionManager.setDirty(false);
        bool exitCalled = false;
        controller.confirmAndExit(nullptr, [&]() {
            exitCalled = true;
        });
        REQUIRE(exitCalled);
    }

    SECTION("confirmAndExit with dirty session handles Cancel, Discard, and Save failure")
    {
        // 1. Cancel does not exit
        {
            sessionManager.setDirty(true);
            bool exitCalled = false;
            controller.confirmAndExit(nullptr, [&]() {
                exitCalled = true;
            });
            confirmModal.simulateResult(gui::ConfirmationModalDialog::Result::Cancel);
            REQUIRE_FALSE(exitCalled);
        }

        // 2. Discard (Secondary) exits immediately
        {
            sessionManager.setDirty(true);
            bool exitCalled = false;
            controller.confirmAndExit(nullptr, [&]() {
                exitCalled = true;
            });
            confirmModal.simulateResult(gui::ConfirmationModalDialog::Result::Secondary);
            REQUIRE(exitCalled);
        }

        // 3. Save (Primary) with invalid active file does NOT exit
        {
            sessionManager.setDirty(true);
            juce::File invalidFile("Z:/non_existent_folder_xyz/never.abdlabtest");
            sessionManager.setActiveSessionFile(invalidFile);

            bool exitCalled = false;
            controller.confirmAndExit(nullptr, [&]() {
                exitCalled = true;
            });
            confirmModal.simulateResult(gui::ConfirmationModalDialog::Result::Primary);

            REQUIRE_FALSE(exitCalled);
            REQUIRE(sessionManager.isDirty());
        }
    }

    SECTION("promptDeleteTest with queued item directly invokes discard without modal")
    {
        gui::QueueItem queuedItem;
        queuedItem.status = gui::QueueItemStatus::Queued;

        bool discarded = false;
        bool invalidated = false;
        controller.promptDeleteTest(nullptr, 3, queuedItem,
            [&](int idx) { discarded = true; REQUIRE(idx == 3); },
            [&](int /*idx*/) { invalidated = true; }
        );

        REQUIRE(discarded);
        REQUIRE_FALSE(invalidated);
    }

    SECTION("promptDeleteTest with completed item handles Discard, Invalidate, and Cancel")
    {
        gui::QueueItem completedItem;
        completedItem.status = gui::QueueItemStatus::Completed;
        completedItem.title = "Harmonic Spectrum Test";

        // 1. Cancel modifies nothing
        {
            bool discarded = false;
            bool invalidated = false;
            controller.promptDeleteTest(nullptr, 5, completedItem,
                [&](int) { discarded = true; },
                [&](int) { invalidated = true; }
            );
            confirmModal.simulateResult(gui::ConfirmationModalDialog::Result::Cancel);
            REQUIRE_FALSE(discarded);
            REQUIRE_FALSE(invalidated);
        }

        // 2. Discard (Primary) invokes onDiscard
        {
            bool discarded = false;
            bool invalidated = false;
            controller.promptDeleteTest(nullptr, 5, completedItem,
                [&](int idx) { discarded = true; REQUIRE(idx == 5); },
                [&](int) { invalidated = true; }
            );
            confirmModal.simulateResult(gui::ConfirmationModalDialog::Result::Primary);
            REQUIRE(discarded);
            REQUIRE_FALSE(invalidated);
        }

        // 3. Invalidate (Secondary) invokes onInvalidate
        {
            bool discarded = false;
            bool invalidated = false;
            controller.promptDeleteTest(nullptr, 5, completedItem,
                [&](int) { discarded = true; },
                [&](int idx) { invalidated = true; REQUIRE(idx == 5); }
            );
            confirmModal.simulateResult(gui::ConfirmationModalDialog::Result::Secondary);
            REQUIRE_FALSE(discarded);
            REQUIRE(invalidated);
        }
    }

    // Cleanup
    tempDir.deleteRecursively();
}
