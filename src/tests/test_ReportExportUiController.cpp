#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <vector>
#include <filesystem>
#include <atomic>
#include <chrono>
#include <thread>

#include "../gui/controllers/ReportExportUiController.h"
#include "../gui/controllers/IReportExportHost.h"

using namespace abdaudiolab;
using namespace abdaudiolab::gui;

namespace {

std::filesystem::path getTempControllerDir(const std::string& sub)
{
    auto p = std::filesystem::temp_directory_path() / "abd_controller_tests" / sub;
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    return p;
}

std::vector<exporting::MeasuredPoint> makeTestPoints(size_t count = 4)
{
    std::vector<exporting::MeasuredPoint> pts;
    for (size_t i = 0; i < count; ++i)
    {
        exporting::MeasuredPoint p;
        p.testId = "pt_" + std::to_string(i + 1);
        p.param1Normalized = static_cast<float>(i) / static_cast<float>(std::max(size_t(1), count - 1));
        p.param2Normalized = 0.5f;
        p.muSigmaValue.mean = 0.1f * static_cast<float>(i);
        p.muSigmaValue.stdDev = 0.01f;
        p.thdPercent = 0.5f;
        p.snrDb = 60.0f;
        pts.push_back(p);
    }
    return pts;
}

void waitForController(ReportExportUiController& controller, int timeoutMs = 2000)
{
    int elapsed = 0;
    while (controller.isExportInProgress() && elapsed < timeoutMs)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        elapsed += 5;
    }
    controller.flushAsyncUpdates();
}

class MockReportExportHost : public IReportExportHost
{
public:
    ReportExportSnapshot snapshotToReturn;

    mutable std::atomic<int> createSnapshotCalls{ 0 };
    std::atomic<int> statusBannerCalls{ 0 };
    std::atomic<int> messageBoxCalls{ 0 };
    std::atomic<int> updateMetricsCalls{ 0 };
    std::atomic<int> notifySuccessCalls{ 0 };
    std::atomic<int> panelStatusCalls{ 0 };
    std::atomic<int> launchProcessCalls{ 0 };
    std::atomic<int> revealInFolderCalls{ 0 };

    std::atomic<bool> allMutationsOnMessageThread{ true };

    juce::String lastBannerMessage;
    juce::String lastMessageBoxTitle;
    juce::String lastMessageBoxMessage;
    juce::File lastSuccessDir;
    juce::String lastSuccessBase;
    juce::File lastLaunchedFile;
    juce::File lastRevealedFolder;
    exporting::CalculatedSessionMetrics lastMetrics;

    ReportExportSnapshot createReportExportSnapshot() const override
    {
        createSnapshotCalls.fetch_add(1);
        checkMessageThread();
        return snapshotToReturn;
    }

    void showStatusBanner(const juce::String& message, bool /*isError*/) override
    {
        statusBannerCalls.fetch_add(1);
        checkMessageThread();
        lastBannerMessage = message;
    }

    void showMessageBox(const juce::String& title, const juce::String& message, bool /*isError*/) override
    {
        messageBoxCalls.fetch_add(1);
        checkMessageThread();
        lastMessageBoxTitle = title;
        lastMessageBoxMessage = message;
    }

    void updateExportReportMetrics(const exporting::CalculatedSessionMetrics& metrics) override
    {
        updateMetricsCalls.fetch_add(1);
        checkMessageThread();
        lastMetrics = metrics;
    }

    void notifyExportSuccess(const juce::File& destinationDir, const juce::String& baseName) override
    {
        notifySuccessCalls.fetch_add(1);
        checkMessageThread();
        lastSuccessDir = destinationDir;
        lastSuccessBase = baseName;
    }

    void showPanelStatus(const juce::String& /*statusMessage*/, bool /*isWarning*/) override
    {
        panelStatusCalls.fetch_add(1);
        checkMessageThread();
    }

    void launchProcess(const juce::File& file) override
    {
        launchProcessCalls.fetch_add(1);
        checkMessageThread();
        lastLaunchedFile = file;
    }

    void revealInFolder(const juce::File& folder) override
    {
        revealInFolderCalls.fetch_add(1);
        checkMessageThread();
        lastRevealedFolder = folder;
    }

private:
    void checkMessageThread() const
    {
        if (!juce::MessageManager::getInstance()->isThisTheMessageThread())
        {
            const_cast<MockReportExportHost*>(this)->allMutationsOnMessageThread.store(false);
        }
    }
};

} // namespace

TEST_CASE("ReportExportUiController: Characterization & Lifetime Contracts", "[ReportExportUiController]")
{
    // Ensure JUCE MessageManager is initialized
    juce::ScopedJuceInitialiser_GUI juceGui;

    auto testDir = getTempControllerDir("ctrl_contracts");
    MockReportExportHost mockHost;

    mockHost.snapshotToReturn.exportDirectory = testDir;
    mockHost.snapshotToReturn.baseFileName = "Test_Preset";
    mockHost.snapshotToReturn.manifest.hardwareId = "aira_torcido";
    mockHost.snapshotToReturn.manifest.activeFunctionId = "tube_warmth";
    mockHost.snapshotToReturn.measuredPoints = makeTestPoints(6);
    mockHost.snapshotToReturn.sampleRate = 48000.0;
    mockHost.snapshotToReturn.inputTrimDb = 0.0f;

    SECTION("1. Exportacion exitosa: metricas y exito exactamente una vez en message thread")
    {
        ReportExportUiController controller(mockHost);

        bool started = controller.requestExportProductionPackage();
        REQUIRE(started);
        REQUIRE(controller.isExportInProgress());

        // Wait for background execution and AsyncUpdater delivery
        waitForController(controller);

        REQUIRE_FALSE(controller.isExportInProgress());
        REQUIRE(mockHost.allMutationsOnMessageThread.load());
        REQUIRE(mockHost.updateMetricsCalls.load() == 1);
        REQUIRE(mockHost.notifySuccessCalls.load() == 1);
        REQUIRE(mockHost.statusBannerCalls.load() == 1);
        REQUIRE(mockHost.messageBoxCalls.load() == 0);
        REQUIRE(mockHost.lastSuccessBase == "Test_Preset");
    }

    SECTION("2. Error: muestra un unico mensaje de alerta y no notifica exito")
    {
        MockReportExportHost errorHost;
        // Provide empty measured points to trigger validation failure
        errorHost.snapshotToReturn.exportDirectory = testDir;
        errorHost.snapshotToReturn.measuredPoints.clear();

        ReportExportUiController controller(errorHost);

        bool started = controller.requestExportProductionPackage();
        // Should reject before launch
        REQUIRE_FALSE(started);
        REQUIRE_FALSE(controller.isExportInProgress());
        REQUIRE(errorHost.messageBoxCalls.load() == 1);
        REQUIRE(errorHost.notifySuccessCalls.load() == 0);
    }

    SECTION("3. Doble clic: rechaza reingreso y ejecuta una sola exportacion real")
    {
        ReportExportUiController controller(mockHost);

        bool first = controller.requestExportProductionPackage();
        bool second = controller.requestExportProductionPackage();

        REQUIRE(first);
        REQUIRE_FALSE(second); // Second must be rejected

        waitForController(controller);

        REQUIRE(mockHost.notifySuccessCalls.load() == 1);
    }

    SECTION("4. Destruccion segura: el destructor espera/cancela el worker sin crash ni leaks")
    {
        {
            ReportExportUiController controller(mockHost);
            bool started = controller.requestExportProductionPackage();
            REQUIRE(started);
            // Destruct controller immediately while worker is running
        }

        // Must not crash or call host after destruction
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        SUCCEED("Destructor completed cleanly without worker leak or crash.");
    }

    SECTION("5. Apertura directa de carpeta: revealInFolder apunta al directorio publicado")
    {
        ReportExportUiController controller(mockHost);
        controller.openExportFolderInExplorer();

        REQUIRE(mockHost.revealInFolderCalls.load() == 1);
        REQUIRE(mockHost.lastRevealedFolder == juce::File(testDir.string()));
        REQUIRE(mockHost.panelStatusCalls.load() == 1);
    }

    SECTION("6. Preview de metricas sincronas: actualiza panel de metricas")
    {
        ReportExportUiController controller(mockHost);
        controller.updateMetricsPreview();

        REQUIRE(mockHost.updateMetricsCalls.load() == 1);
        REQUIRE(mockHost.lastMetrics.validPointCount == 6);
    }

    SECTION("7. HTML Certification: launchProcess se invoca tras exportacion exitosa")
    {
        ReportExportUiController controller(mockHost);
        bool started = controller.requestExportCertificationReport();
        REQUIRE(started);

        waitForController(controller);

        REQUIRE(mockHost.notifySuccessCalls.load() == 1);
        REQUIRE(mockHost.launchProcessCalls.load() == 1);
        REQUIRE(mockHost.lastLaunchedFile.getFileName().contains("Certification_Report.html"));
    }

    SECTION("8. HTML: no se abre antes del exito si el archivo no existe")
    {
        MockReportExportHost emptyMock;
        auto emptyDir = getTempControllerDir("empty_html_dir");
        emptyMock.snapshotToReturn.exportDirectory = emptyDir;
        emptyMock.snapshotToReturn.baseFileName = "NonExistent";

        ReportExportUiController controller(emptyMock);
        controller.openCertificationReportHtml();

        REQUIRE(emptyMock.launchProcessCalls.load() == 0);
        REQUIRE(emptyMock.panelStatusCalls.load() == 1);
    }

    SECTION("9. Error al construir snapshot: directorio invalido rechaza antes de lanzar")
    {
        MockReportExportHost invalidHost;
        invalidHost.snapshotToReturn.exportDirectory = ""; // Directorio vacio
        invalidHost.snapshotToReturn.measuredPoints = makeTestPoints(2);

        ReportExportUiController controller(invalidHost);
        bool started = controller.requestExportProductionPackage();

        REQUIRE_FALSE(started);
        REQUIRE_FALSE(controller.isExportInProgress());
        REQUIRE(invalidHost.messageBoxCalls.load() == 1);
        REQUIRE(invalidHost.notifySuccessCalls.load() == 0);
    }

    SECTION("10. Host y Controller destruidos simultaneamente durante ejecucion de worker")
    {
        struct HostAndController {
            MockReportExportHost host;
            std::unique_ptr<ReportExportUiController> controller;

            HostAndController(const std::filesystem::path& dir) {
                host.snapshotToReturn.exportDirectory = dir;
                host.snapshotToReturn.baseFileName = "HostDestroyTest";
                host.snapshotToReturn.manifest.hardwareId = "aira_torcido";
                host.snapshotToReturn.manifest.activeFunctionId = "tube_warmth";
                host.snapshotToReturn.measuredPoints = makeTestPoints(8);
                controller = std::make_unique<ReportExportUiController>(host);
            }

            ~HostAndController() {
                // Controller MUST be destroyed before host
                controller.reset();
            }
        };

        auto hcDir = getTempControllerDir("host_destroy_test");
        {
            auto hc = std::make_unique<HostAndController>(hcDir);
            bool started = hc->controller->requestExportProductionPackage();
            REQUIRE(started);
            // Destroy both hc and controller while worker is running
            hc.reset();
        }

        SUCCEED("Host and controller cleanly destroyed without access violation.");
    }
}
