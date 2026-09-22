/**
 * @file test_ReportExportService.cpp
 * @brief Characterization and regression test suite for ReportExportService.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "export/ReportExportService.h"
#include "export/CertificationReportExporter.h"
#include "gui/SessionReportManager.h"
#include <filesystem>
#include <fstream>

using namespace abdaudiolab;
using namespace abdaudiolab::exporting;

namespace {

std::vector<MeasuredPoint> makeTestPoints(size_t count)
{
    std::vector<MeasuredPoint> pts;
    pts.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        MeasuredPoint p;
        p.pointId = "P_" + std::to_string(i + 1);
        p.testId = "TEST_SWEEP";
        p.blockType = "VCF";
        p.stimulusType = "SINE_SWEEP";
        p.param1Normalized = static_cast<float>(i) / std::max(1.0f, static_cast<float>(count - 1));
        p.param2Normalized = 0.5f;
        p.snrDb = 60.0f + static_cast<float>(i);
        p.thdPercent = 0.05f * static_cast<float>(i + 1);
        p.secondaryValue.mean = -6.0f;
        p.secondaryValue.stdDev = 0.1f;
        p.irSamples = { 0.1f, -0.05f, 0.02f };
        pts.push_back(p);
    }
    return pts;
}

std::filesystem::path getTempTestDir(const std::string& subfolder)
{
    auto temp = std::filesystem::temp_directory_path() / "abdaudiolab_tests" / subfolder;
    std::error_code ec;
    std::filesystem::remove_all(temp, ec);
    std::filesystem::create_directories(temp, ec);
    return temp;
}

} // namespace

// ==============================================================================
// 1. Pure Transformations: calculateMetrics & buildAuditionGrid
// ==============================================================================

TEST_CASE("ReportExportService: Pure Metrics and Audition Grid", "[ReportExportService]")
{
    SECTION("calculateMetrics with known input values returns deterministic results")
    {
        auto pts = makeTestPoints(4); // snr: 60, 61, 62, 63 -> avg: 61.5
        // thd: 0.05, 0.10, 0.15, 0.20 -> avg: 0.125
        auto m = ReportExportService::calculateMetrics(pts, 0.0f);

        REQUIRE(m.validPointCount == 4);
        REQUIRE(m.avgSnrDb == Catch::Approx(61.5f));
        REQUIRE(m.avgThdPercent == Catch::Approx(0.125f));
        REQUIRE(m.totalDurationSec == Catch::Approx(10.0f)); // 4 * 2.5s = 10.0s
        REQUIRE(m.noiseFloorDb == Catch::Approx(-90.0f));    // inputTrim <= 1e-4
    }

    SECTION("calculateMetrics on empty points returns canonical fallback metrics")
    {
        auto m = ReportExportService::calculateMetrics({});
        REQUIRE(m.validPointCount == 0);
        REQUIRE(m.avgSnrDb == Catch::Approx(38.5f));
        REQUIRE(m.avgThdPercent == Catch::Approx(0.015f));
        REQUIRE(m.noiseFloorDb == Catch::Approx(-90.0f));
        REQUIRE(m.totalDurationSec == Catch::Approx(10.0f));
    }

    SECTION("buildAuditionGrid returns 64 elements for 8x8 grid")
    {
        auto pts = makeTestPoints(5);
        auto grid = ReportExportService::buildAuditionGrid(pts, 8);
        REQUIRE(grid.size() == 64);
        REQUIRE(grid[0].p1 == 0.0f);
        REQUIRE(grid[0].p2 == 0.0f);
        REQUIRE(grid[63].p1 == 1.0f);
        REQUIRE(grid[63].p2 == 1.0f);
    }

    SECTION("Parity: ReportExportService vs SessionReportManager")
    {
        // 1. Empty dataset parity
        float legAvgSnr = 0.0f, legNoise = 0.0f, legThd = 0.0f, legDur = 0.0f;
        int legCount = 0;
        gui::SessionReportManager::calculateSessionMetrics({}, 0.0f, legAvgSnr, legNoise, legThd, legCount, legDur);

        auto newMetricsEmpty = ReportExportService::calculateMetrics({}, 0.0f);
        REQUIRE(newMetricsEmpty.validPointCount == legCount);
        REQUIRE(newMetricsEmpty.avgSnrDb == Catch::Approx(legAvgSnr));
        REQUIRE(newMetricsEmpty.noiseFloorDb == Catch::Approx(legNoise));
        REQUIRE(newMetricsEmpty.avgThdPercent == Catch::Approx(legThd));
        REQUIRE(newMetricsEmpty.totalDurationSec == Catch::Approx(legDur));

        // 2. Populated dataset parity with non-zero trim
        auto testPts = makeTestPoints(6);
        float trim = 0.5f;
        gui::SessionReportManager::calculateSessionMetrics(testPts, trim, legAvgSnr, legNoise, legThd, legCount, legDur);

        auto newMetricsPop = ReportExportService::calculateMetrics(testPts, trim);
        REQUIRE(newMetricsPop.validPointCount == legCount);
        REQUIRE(newMetricsPop.avgSnrDb == Catch::Approx(legAvgSnr));
        REQUIRE(newMetricsPop.noiseFloorDb == Catch::Approx(legNoise));
        REQUIRE(newMetricsPop.avgThdPercent == Catch::Approx(legThd));
        REQUIRE(newMetricsPop.totalDurationSec == Catch::Approx(legDur));

        // 3. Audition grid parity (element by element across all 64 cells)
        auto legacyGrid = gui::SessionReportManager::buildAuditionLutGrid(testPts, 8);
        auto newGrid = ReportExportService::buildAuditionGrid(testPts, 8);
        REQUIRE(legacyGrid.size() == newGrid.size());
        for (size_t i = 0; i < legacyGrid.size(); ++i)
        {
            REQUIRE(legacyGrid[i].p1 == Catch::Approx(newGrid[i].p1));
            REQUIRE(legacyGrid[i].p2 == Catch::Approx(newGrid[i].p2));
            REQUIRE(legacyGrid[i].mu == Catch::Approx(newGrid[i].mu));
            REQUIRE(legacyGrid[i].sigma == Catch::Approx(newGrid[i].sigma));
        }
    }
}

// ==============================================================================
// 2. Pre-condition Validation & Error Handling
// ==============================================================================

TEST_CASE("ReportExportService: Request Validation", "[ReportExportService]")
{
    SECTION("Empty destination directory rejected")
    {
        ReportExportRequest req;
        req.measuredPoints = makeTestPoints(1);
        auto res = ReportExportService::exportReport(req);

        REQUIRE_FALSE(res.succeeded());
        REQUIRE(res.status == ReportExportStatus::InvalidRequest);
        REQUIRE(res.errorCode == "ERR_NO_DESTINATION");
    }

    SECTION("No format selected rejected")
    {
        ReportExportRequest req;
        req.destinationDirectory = getTempTestDir("no_format");
        req.measuredPoints = makeTestPoints(1);
        req.options.includeHtmlCertification = false;
        auto res = ReportExportService::exportReport(req);

        REQUIRE_FALSE(res.succeeded());
        REQUIRE(res.status == ReportExportStatus::InvalidRequest);
        REQUIRE(res.errorCode == "ERR_NO_FORMAT_SELECTED");
    }

    SECTION("Missing measured points rejected")
    {
        ReportExportRequest req;
        req.destinationDirectory = getTempTestDir("no_points");
        req.measuredPoints = {};
        auto res = ReportExportService::exportReport(req);

        REQUIRE_FALSE(res.succeeded());
        REQUIRE(res.status == ReportExportStatus::MissingSessionData);
        REQUIRE(res.errorCode == "ERR_NO_MEASURED_POINTS");
    }
}

// ==============================================================================
// 3. Sanitization & Windows Reserved Device Names
// ==============================================================================

TEST_CASE("ReportExportService: Windows Reserved Names and Sanitization", "[ReportExportService]")
{
    auto tempDir = getTempTestDir("naming_tests");

    SECTION("Reserved names CON, PRN, AUX, NUL are safely sanitized")
    {
        for (const auto& reserved : { "CON", "con", "PRN", "AUX", "NUL" })
        {
            ReportExportRequest req;
            req.destinationDirectory = tempDir;
            req.baseFileName = reserved;
            req.measuredPoints = makeTestPoints(2);
            req.options.includeHtmlCertification = true;

            auto res = ReportExportService::exportReport(req);
            REQUIRE(res.succeeded());
            REQUIRE_FALSE(res.artifacts.empty());

            std::string filename = res.artifacts.front().publishedPath.filename().string();
            // Must not start with bare CON/con/etc.
            REQUIRE(filename.find(reserved) != std::string::npos);
            REQUIRE(filename.front() == '_');
        }
    }

    SECTION("Names with invalid characters and trailing dots/spaces are sanitized")
    {
        ReportExportRequest req;
        req.destinationDirectory = tempDir;
        req.baseFileName = "bad<>:\"/\\|?*name...   ";
        req.measuredPoints = makeTestPoints(2);
        req.options.includeHtmlCertification = true;

        auto res = ReportExportService::exportReport(req);
        REQUIRE(res.succeeded());
        REQUIRE(std::filesystem::exists(res.artifacts.front().publishedPath));
    }
}

// ==============================================================================
// 4. Atomic Staging, Publication, and Fixity Verification
// ==============================================================================

TEST_CASE("ReportExportService: Atomic Publication and Fixity", "[ReportExportService]")
{
    auto tempDir = getTempTestDir("atomic_publish");

    SECTION("HTML export creates single valid file and returns published path (never staging)")
    {
        ReportExportRequest req;
        req.destinationDirectory = tempDir;
        req.baseFileName = "korg_ms20";
        req.measuredPoints = makeTestPoints(3);
        req.options.includeHtmlCertification = true;

        auto res = ReportExportService::exportReport(req);
        REQUIRE(res.succeeded());
        REQUIRE(res.artifacts.size() == 1);

        const auto& art = res.artifacts.front();
        REQUIRE(art.kind == ReportArtifactKind::CertificationHtml);
        REQUIRE(std::filesystem::exists(art.publishedPath));
        REQUIRE(art.publishedPath.string().find(".staging") == std::string::npos);
        REQUIRE(art.byteSize > 0);
        REQUIRE_FALSE(art.sha256.empty());

        // Staging directory must be cleaned up
        for (const auto& entry : std::filesystem::directory_iterator(tempDir))
        {
            REQUIRE(entry.path().filename().string().find(".staging") == std::string::npos);
        }
    }

    SECTION("Production package generates all 4 artifacts with non-empty SHA-256 fixity")
    {
        ReportExportRequest req;
        req.destinationDirectory = tempDir;
        req.baseFileName = "juno_106";
        req.measuredPoints = makeTestPoints(3);
        req.options.includeProductionPackage = true;

        auto res = ReportExportService::exportReport(req);
        REQUIRE(res.succeeded());
        REQUIRE(res.artifacts.size() == 4);
        REQUIRE_FALSE(res.manifestSha256.empty());

        for (const auto& art : res.artifacts)
        {
            REQUIRE(std::filesystem::exists(art.publishedPath));
            REQUIRE(art.publishedPath.string().find(".staging") == std::string::npos);
            REQUIRE(art.byteSize > 0);
            REQUIRE(art.sha256.size() == 64);
        }
    }

    SECTION("Existing destination replaced cleanly without leaving backup files")
    {
        ReportExportRequest req;
        req.destinationDirectory = tempDir;
        req.baseFileName = "overwrite_test";
        req.measuredPoints = makeTestPoints(2);
        req.options.includeHtmlCertification = true;

        // First export
        auto res1 = ReportExportService::exportReport(req);
        REQUIRE(res1.succeeded());
        auto path1 = res1.artifacts.front().publishedPath;
        auto size1 = res1.artifacts.front().byteSize;
        REQUIRE(size1 > 0);

        // Second export overwrites cleanly
        req.measuredPoints = makeTestPoints(5);
        auto res2 = ReportExportService::exportReport(req);
        REQUIRE(res2.succeeded());
        auto path2 = res2.artifacts.front().publishedPath;

        REQUIRE(path1 == path2);
        REQUIRE(std::filesystem::exists(path2));

        // No backup files left in target directory
        for (const auto& entry : std::filesystem::directory_iterator(tempDir))
        {
            REQUIRE(entry.path().filename().string().find(".backup") == std::string::npos);
        }
    }

    SECTION("Input request is not mutated by export")
    {
        ReportExportRequest req;
        req.destinationDirectory = tempDir;
        req.baseFileName = "immutability_test";
        req.measuredPoints = makeTestPoints(2);
        req.options.includeHtmlCertification = true;

        const size_t origCount = req.measuredPoints.size();
        const std::string origBase = req.baseFileName;

        auto res = ReportExportService::exportReport(req);
        REQUIRE(res.succeeded());

        REQUIRE(req.measuredPoints.size() == origCount);
        REQUIRE(req.baseFileName == origBase);
    }
}

// ==============================================================================
// 5. Individual Artifact Canonical Parity Tests
// ==============================================================================

TEST_CASE("ReportExportService: Individual Artifact Parity", "[ReportExportService]")
{
    auto tempDir = getTempTestDir("individual_artifact_parity");
    auto pts = makeTestPoints(4);
    core::ProfilingMetadata meta;
    meta.hardwareName = "Dexed Mock";
    meta.targetModule = "Filter";
    meta.operatorMode = "MANUAL";
    meta.sampleRate = 44100.0;
    meta.operatorNotes = "Parity check";
    meta.timestamp = "2026-09-19T00:00:00Z";

    SECTION("1. Telemetry JSON parity between LutExporter and ReportExportService")
    {
        auto legacyPath = tempDir / "legacy_telemetry.json";
        auto newPath = tempDir / "new_telemetry.json";

        bool okLegacy = LutExporter::exportToJsonReport(legacyPath.string(), meta, pts);
        bool okNew = ReportExportService::writeTelemetryJson(newPath, meta, pts);

        REQUIRE(okLegacy);
        REQUIRE(okNew);
        REQUIRE(std::filesystem::exists(legacyPath));
        REQUIRE(std::filesystem::exists(newPath));

        std::string contentLegacy, contentNew;
        {
            std::ifstream fA(legacyPath);
            std::stringstream ssA; ssA << fA.rdbuf(); contentLegacy = ssA.str();
            std::ifstream fB(newPath);
            std::stringstream ssB; ssB << fB.rdbuf(); contentNew = ssB.str();
        }

        REQUIRE_FALSE(contentLegacy.empty());
        REQUIRE(contentLegacy.size() == contentNew.size());
        REQUIRE(contentLegacy == contentNew);
    }

    SECTION("2. Session Manifest JSON parity between LutExporter and ReportExportService")
    {
        SessionManifestData manifestData;
        manifestData.hardwareId = "dexed_01";
        manifestData.hardwareName = "Dexed Mock";
        manifestData.functionId = "cutoff_res";
        manifestData.functionName = "Cutoff Resonance";
        manifestData.deviceType = "MANUAL_EURORACK";
        manifestData.sampleRate = 44100.0;
        manifestData.operatorNotes = "Manifest parity check";
        manifestData.ambientTemperatureC = 21.5f;
        manifestData.warmupTimeMinutes = 15;

        auto legacyPath = tempDir / "legacy_manifest.json";
        auto newPath = tempDir / "new_manifest.json";

        bool okLegacy = LutExporter::exportSessionManifest(legacyPath.string(), manifestData, pts);
        bool okNew = ReportExportService::writeSessionManifestJson(newPath, manifestData, pts);

        REQUIRE(okLegacy);
        REQUIRE(okNew);
        REQUIRE(std::filesystem::exists(legacyPath));
        REQUIRE(std::filesystem::exists(newPath));

        std::string contentLegacy, contentNew;
        {
            std::ifstream fA(legacyPath);
            std::stringstream ssA; ssA << fA.rdbuf(); contentLegacy = ssA.str();
            std::ifstream fB(newPath);
            std::stringstream ssB; ssB << fB.rdbuf(); contentNew = ssB.str();
        }

        REQUIRE_FALSE(contentLegacy.empty());
        REQUIRE(contentLegacy.size() == contentNew.size());
        REQUIRE(contentLegacy == contentNew);
    }

    SECTION("3. C++ LUT Header parity between LutExporter and ReportExportService")
    {
        auto legacyPath = tempDir / "legacy_lut.h";
        auto newPath = tempDir / "new_lut.h";

        bool okLegacy = LutExporter::exportToCppHeader(legacyPath.string(), meta, "parity_table", pts);
        bool okNew = ReportExportService::writeCppLutHeader(newPath, meta, "parity_table", pts);

        REQUIRE(okLegacy);
        REQUIRE(okNew);
        REQUIRE(std::filesystem::exists(legacyPath));
        REQUIRE(std::filesystem::exists(newPath));

        std::string contentLegacy, contentNew;
        {
            std::ifstream fA(legacyPath);
            std::stringstream ssA; ssA << fA.rdbuf(); contentLegacy = ssA.str();
            std::ifstream fB(newPath);
            std::stringstream ssB; ssB << fB.rdbuf(); contentNew = ssB.str();
        }

        REQUIRE_FALSE(contentLegacy.empty());
        REQUIRE(contentLegacy.size() == contentNew.size());
        REQUIRE(contentLegacy == contentNew);
    }

    SECTION("4. Certification HTML parity between CertificationReportExporter and ReportExportService")
    {
        SessionManifestData manifestData;
        manifestData.hardwareId = "dexed_01";
        manifestData.hardwareName = "Dexed Mock";
        manifestData.functionId = "cutoff_res";
        manifestData.functionName = "Cutoff Resonance";
        manifestData.deviceType = "MANUAL_EURORACK";
        manifestData.sampleRate = 44100.0;
        manifestData.operatorNotes = "HTML parity check";
        manifestData.ambientTemperatureC = 21.5f;
        manifestData.warmupTimeMinutes = 15;

        auto legacyPath = tempDir / "legacy_report.html";
        auto newPath = tempDir / "new_report.html";

        bool okLegacy = CertificationReportExporter::exportReportToHtml(legacyPath.string(), manifestData, pts);
        bool okNew = ReportExportService::writeCertificationHtml(newPath, manifestData, pts);

        REQUIRE(okLegacy);
        REQUIRE(okNew);
        REQUIRE(std::filesystem::exists(legacyPath));
        REQUIRE(std::filesystem::exists(newPath));

        std::string contentLegacy, contentNew;
        {
            std::ifstream fA(legacyPath);
            std::stringstream ssA; ssA << fA.rdbuf(); contentLegacy = ssA.str();
            std::ifstream fB(newPath);
            std::stringstream ssB; ssB << fB.rdbuf(); contentNew = ssB.str();
        }

        REQUIRE_FALSE(contentLegacy.empty());
        REQUIRE(contentLegacy.size() == contentNew.size());
        REQUIRE(contentLegacy == contentNew);
    }
}

// ==============================================================================
// 6. ProductionPackage: Staging, Fixity, Backup & Rollback Protocol
// ==============================================================================

TEST_CASE("ReportExportService: ProductionPackage Staging, Fixity & Publication Protocol", "[ReportExportService]")
{
    auto baseTemp = getTempTestDir("prod_package_protocol");
    auto pts = makeTestPoints(8);

    ReportExportRequest baseReq;
    baseReq.manifest.hardwareId = "aira_torcido";
    baseReq.manifest.hardwareDisplayName = "Roland AIRA: Torcido";
    baseReq.manifest.activeFunctionId = "tube_warmth";
    baseReq.manifest.activeFunctionName = "Vacuum Tube Warmth & Clipper";
    baseReq.manifest.targetModule = "tube_warmth";
    baseReq.baseFileName = "Torcido_Prod";
    baseReq.context.sampleRate = 48000.0;
    baseReq.context.operatorNotes = "Production Package Protocol Test";
    baseReq.options.includeProductionPackage = true;
    baseReq.options.includeHtmlCertification = false;
    baseReq.measuredPoints = pts;

    SECTION("1. Paquete nuevo en destino inexistente: creacion limpia y publicacion")
    {
        auto dest = baseTemp / "non_existent_target_dir";
        std::error_code ec;
        std::filesystem::remove_all(dest, ec);

        auto req = baseReq;
        req.destinationDirectory = dest;

        auto res = ReportExportService::exportReport(req);

        REQUIRE(res.succeeded());
        REQUIRE(res.status == ReportExportStatus::Success);
        REQUIRE(std::filesystem::is_directory(dest));
        REQUIRE(res.artifacts.size() == 4);

        // Nombres canónicos esperados
        auto lutFile = dest / "Torcido_Prod_lut.h";
        auto jsonFile = dest / "Torcido_Prod_telemetry.json";
        auto htmlFile = dest / "Torcido_Prod_Certification_Report.html";
        auto manifestFile = dest / "Torcido_Prod_manifest.json";

        REQUIRE(std::filesystem::exists(lutFile));
        REQUIRE(std::filesystem::exists(jsonFile));
        REQUIRE(std::filesystem::exists(htmlFile));
        REQUIRE(std::filesystem::exists(manifestFile));

        // Hashes y tamaños coinciden
        for (const auto& art : res.artifacts)
        {
            REQUIRE(std::filesystem::exists(art.publishedPath));
            REQUIRE(std::filesystem::file_size(art.publishedPath) == art.byteSize);
            REQUIRE(art.byteSize > 0);
            REQUIRE(art.sha256.length() == 64);
        }

        REQUIRE(res.manifestSha256.length() == 64);

        // Sin carpetas de staging ni backups residuales
        for (const auto& entry : std::filesystem::directory_iterator(dest))
        {
            auto fn = entry.path().filename().string();
            REQUIRE(fn.find(".staging_") == std::string::npos);
            REQUIRE(fn.find(".backup_") == std::string::npos);
        }
    }

    SECTION("2. Reemplazo de paquete existente valido: atomicidad y limpieza de backups")
    {
        auto dest = baseTemp / "existing_valid_target";
        std::error_code ec;
        std::filesystem::create_directories(dest, ec);

        // Crear archivos preexistentes con contenido previo
        std::ofstream(dest / "Torcido_Prod_lut.h") << "// OLD CONTENT LUT";
        std::ofstream(dest / "Torcido_Prod_telemetry.json") << "{\"old\": true}";
        std::ofstream(dest / "Torcido_Prod_Certification_Report.html") << "<html>Old</html>";
        std::ofstream(dest / "Torcido_Prod_manifest.json") << "{\"manifest\": \"old\"}";

        auto req = baseReq;
        req.destinationDirectory = dest;

        auto res = ReportExportService::exportReport(req);

        REQUIRE(res.succeeded());
        REQUIRE(res.status == ReportExportStatus::Success);

        // Verificar que el contenido fue reemplazado por el nuevo paquete
        std::string newLutContent;
        {
            std::ifstream f(dest / "Torcido_Prod_lut.h");
            std::stringstream ss; ss << f.rdbuf(); newLutContent = ss.str();
        }
        REQUIRE(newLutContent.find("OLD CONTENT") == std::string::npos);

        // Verificar que no quedó NINGÚN archivo .backup_* ni .staging_*
        for (const auto& entry : std::filesystem::directory_iterator(dest))
        {
            auto fn = entry.path().filename().string();
            REQUIRE(fn.find(".staging_") == std::string::npos);
            REQUIRE(fn.find(".backup_") == std::string::npos);
        }
    }

    SECTION("3. Archivo destino bloqueado en Windows: deteccion de sharing violation y preservacion")
    {
        auto dest = baseTemp / "locked_target_dir";
        std::error_code ec;
        std::filesystem::create_directories(dest, ec);

        auto targetLut = dest / "Torcido_Prod_lut.h";
        {
            std::ofstream initial(targetLut);
            initial << "ORIGINAL_UNTOUCHED_CONTENT";
        }

        // Mantener el archivo abierto exclusivamente para simular un bloqueo de Windows (antivirus o visor)
        std::ofstream lockStream(targetLut, std::ios::in | std::ios::out | std::ios::binary);
        REQUIRE(lockStream.is_open());

        auto req = baseReq;
        req.destinationDirectory = dest;

        auto res = ReportExportService::exportReport(req);

        // La operación debe fallar de forma visible
        REQUIRE_FALSE(res.succeeded());
        REQUIRE(res.status == ReportExportStatus::CannotWriteArtifact);
        REQUIRE(res.errorCode == "ERR_BACKUP_FAILED");

        lockStream.close();

        // El destino anterior debe estar preservado
        std::string preserved;
        {
            std::ifstream f(targetLut);
            std::stringstream ss; ss << f.rdbuf(); preserved = ss.str();
        }
        REQUIRE(preserved == "ORIGINAL_UNTOUCHED_CONTENT");

        // Staging debe quedar limpio
        for (const auto& entry : std::filesystem::directory_iterator(dest))
        {
            auto fn = entry.path().filename().string();
            REQUIRE(fn.find(".staging_") == std::string::npos);
        }
    }

    SECTION("4. Fallo simulado de Backup: aborta inmediatamente y limpia staging")
    {
        auto dest = baseTemp / "fail_backup_dir";
        std::error_code ec;
        std::filesystem::create_directories(dest, ec);

        auto targetFile = dest / "Torcido_Prod_lut.h";
        std::ofstream(targetFile) << "INITIAL_DATA";

        auto req = baseReq;
        req.destinationDirectory = dest;
        req.faultInjection.failBackup = true;

        auto res = ReportExportService::exportReport(req);

        REQUIRE_FALSE(res.succeeded());
        REQUIRE(res.status == ReportExportStatus::CannotWriteArtifact);
        REQUIRE(res.errorCode == "ERR_BACKUP_FAILED");

        // Archivo destino preservado intacto
        std::string content;
        {
            std::ifstream f(targetFile);
            std::stringstream ss; ss << f.rdbuf(); content = ss.str();
        }
        REQUIRE(content == "INITIAL_DATA");

        // Sin backups ni staging residual
        for (const auto& entry : std::filesystem::directory_iterator(dest))
        {
            auto fn = entry.path().filename().string();
            REQUIRE(fn.find(".staging_") == std::string::npos);
            REQUIRE(fn.find(".backup_") == std::string::npos);
        }
    }

    SECTION("5. Fallo al publicar staging (Promote failed): Rollback y restauracion del backup")
    {
        auto dest = baseTemp / "fail_promote_dir";
        std::error_code ec;
        std::filesystem::create_directories(dest, ec);

        auto lutTarget = dest / "Torcido_Prod_lut.h";
        auto jsonTarget = dest / "Torcido_Prod_telemetry.json";
        std::ofstream(lutTarget) << "PREVIOUS_LUT_DATA";
        std::ofstream(jsonTarget) << "PREVIOUS_JSON_DATA";

        auto req = baseReq;
        req.destinationDirectory = dest;
        req.faultInjection.failPromote = true;

        auto res = ReportExportService::exportReport(req);

        REQUIRE_FALSE(res.succeeded());
        REQUIRE(res.status == ReportExportStatus::CannotWriteArtifact);
        REQUIRE(res.errorCode == "ERR_PUBLISH_RENAME_FAILED");

        // Backups DEBEN haberse restaurado a sus nombres originales
        std::string lutRestored, jsonRestored;
        {
            std::ifstream f1(lutTarget);
            std::stringstream ss1; ss1 << f1.rdbuf(); lutRestored = ss1.str();
            std::ifstream f2(jsonTarget);
            std::stringstream ss2; ss2 << f2.rdbuf(); jsonRestored = ss2.str();
        }
        REQUIRE(lutRestored == "PREVIOUS_LUT_DATA");
        REQUIRE(jsonRestored == "PREVIOUS_JSON_DATA");

        // Ningún backup residual y ningún staging residual
        for (const auto& entry : std::filesystem::directory_iterator(dest))
        {
            auto fn = entry.path().filename().string();
            REQUIRE(fn.find(".staging_") == std::string::npos);
            REQUIRE(fn.find(".backup_") == std::string::npos);
        }
    }

    SECTION("6. Fallo en Post-Publish Verification: Rollback y restauracion de estado previo")
    {
        auto dest = baseTemp / "fail_post_verify_dir";
        std::error_code ec;
        std::filesystem::create_directories(dest, ec);

        auto lutTarget = dest / "Torcido_Prod_lut.h";
        std::ofstream(lutTarget) << "PREVIOUS_LUT_BEFORE_POST_VERIFY_FAIL";

        auto req = baseReq;
        req.destinationDirectory = dest;
        req.faultInjection.failPostPublishVerification = true;

        auto res = ReportExportService::exportReport(req);

        REQUIRE_FALSE(res.succeeded());
        REQUIRE(res.status == ReportExportStatus::IntegrityCheckFailed);
        REQUIRE(res.errorCode == "ERR_POST_PUBLISH_VERIFICATION_FAILED");

        // Estado previo restaurado
        std::string restored;
        {
            std::ifstream f(lutTarget);
            std::stringstream ss; ss << f.rdbuf(); restored = ss.str();
        }
        REQUIRE(restored == "PREVIOUS_LUT_BEFORE_POST_VERIFY_FAIL");

        // Sin backups ni staging residual
        for (const auto& entry : std::filesystem::directory_iterator(dest))
        {
            auto fn = entry.path().filename().string();
            REQUIRE(fn.find(".staging_") == std::string::npos);
            REQUIRE(fn.find(".backup_") == std::string::npos);
        }
    }

    SECTION("7. Estabilidad de manifestSha256 e inmutabilidad de request")
    {
        auto dest1 = baseTemp / "stability_dest1";
        auto dest2 = baseTemp / "stability_dest2";

        auto req = baseReq;
        req.destinationDirectory = dest1;

        size_t originalPtsCount = req.measuredPoints.size();
        auto originalHwId = req.manifest.hardwareId;

        auto res1 = ReportExportService::exportReport(req);

        // Inputs no mutados
        REQUIRE(req.measuredPoints.size() == originalPtsCount);
        REQUIRE(req.manifest.hardwareId == originalHwId);

        req.destinationDirectory = dest2;
        auto res2 = ReportExportService::exportReport(req);

        REQUIRE(res1.succeeded());
        REQUIRE(res2.succeeded());
        REQUIRE(res1.manifestSha256 == res2.manifestSha256);
    }
}

// ==============================================================================
// 7. Paridad Semántica entre Legacy SessionReportManager y ReportExportService
// ==============================================================================

TEST_CASE("ReportExportService: Paridad Semantica con Legacy ProductionPackage", "[ReportExportService]")
{
    auto baseTemp = getTempTestDir("semantic_package_parity");
    auto legacyDir = baseTemp / "legacy_package";
    auto newDir = baseTemp / "new_package";

    std::error_code ec;
    std::filesystem::create_directories(legacyDir, ec);
    std::filesystem::create_directories(newDir, ec);

    auto pts = makeTestPoints(12);

    // Configuración para el motor legacy
    gui::ReportExportRequest legacyReq;
    legacyReq.format = gui::ReportFormat::ProductionPackage;
    legacyReq.destination = juce::File(legacyDir.string());
    legacyReq.baseName = "Seam1_Parity";
    legacyReq.hardwareId = "aira_torcido";
    legacyReq.hardwareName = "Roland AIRA: Torcido";
    legacyReq.functionId = "tube_warmth";
    legacyReq.functionName = "Vacuum Tube Warmth & Clipper";
    legacyReq.deviceType = "MANUAL_EURORACK";
    legacyReq.sampleRate = 48000.0;
    legacyReq.ambientTemperatureC = 21.0;
    legacyReq.warmupTimeMinutes = 15.0;
    legacyReq.operatorNotes = "Parity check session";

    gui::SessionReportManager reportManager;
    auto legacyResult = reportManager.exportReport(legacyReq, pts);

    REQUIRE(legacyResult.succeeded);
    REQUIRE(legacyResult.artifactPaths.size() == 4);

    // Configuración para ReportExportService nuevo
    ReportExportRequest newReq;
    newReq.manifest.hardwareId = legacyReq.hardwareId.toStdString();
    newReq.manifest.hardwareDisplayName = legacyReq.hardwareName.toStdString();
    newReq.manifest.activeFunctionId = legacyReq.functionId.toStdString();
    newReq.manifest.activeFunctionName = legacyReq.functionName.toStdString();
    newReq.manifest.targetModule = newReq.manifest.activeFunctionId;
    newReq.destinationDirectory = newDir;
    newReq.baseFileName = legacyReq.baseName.toStdString();
    newReq.context.sampleRate = legacyReq.sampleRate;
    newReq.context.operatorNotes = legacyReq.operatorNotes.toStdString();
    newReq.context.ambientTemperatureC = legacyReq.ambientTemperatureC;
    newReq.context.warmupTimeMinutes = legacyReq.warmupTimeMinutes;
    newReq.options.includeProductionPackage = true;
    newReq.options.includeHtmlCertification = false;
    newReq.measuredPoints = pts;

    auto newResult = ReportExportService::exportReport(newReq);

    INFO("newResult status=" << static_cast<int>(newResult.status) << " code=" << newResult.errorCode << " msg=" << newResult.userMessage);
    REQUIRE(newResult.succeeded());
    REQUIRE(newResult.artifacts.size() == 4);

    // Comparación Semántica
    // 1. Archivos esperados
    std::vector<std::string> expectedNames = {
        "Seam1_Parity_lut.h",
        "Seam1_Parity_telemetry.json",
        "Seam1_Parity_Certification_Report.html",
        "Seam1_Parity_manifest.json"
    };

    for (const auto& name : expectedNames)
    {
        auto legP = legacyDir / name;
        auto newP = newDir / name;

        REQUIRE(std::filesystem::exists(legP));
        REQUIRE(std::filesystem::exists(newP));

        // Para LUT y Telemetry JSON: paridad bit a bit exacta
        if (name.find("_lut.h") != std::string::npos || name.find("_telemetry.json") != std::string::npos)
        {
            std::string contentLeg, contentNew;
            {
                std::ifstream f1(legP); std::stringstream s1; s1 << f1.rdbuf(); contentLeg = s1.str();
                std::ifstream f2(newP); std::stringstream s2; s2 << f2.rdbuf(); contentNew = s2.str();
            }
            REQUIRE(contentLeg == contentNew);
            REQUIRE(std::filesystem::file_size(legP) == std::filesystem::file_size(newP));
        }

        // Para HTML: ambos deben tener contenido válido y estructura idéntica
        if (name.find("_Certification_Report.html") != std::string::npos)
        {
            REQUIRE(std::filesystem::file_size(legP) > 0);
            REQUIRE(std::filesystem::file_size(newP) > 0);
            std::string contentLeg, contentNew;
            {
                std::ifstream f1(legP); std::stringstream s1; s1 << f1.rdbuf(); contentLeg = s1.str();
                std::ifstream f2(newP); std::stringstream s2; s2 << f2.rdbuf(); contentNew = s2.str();
            }
            REQUIRE(contentLeg == contentNew);
        }

        // Para manifest JSON: parsear semánticamente con nlohmann::json
        if (name.find("_manifest.json") != std::string::npos)
        {
            nlohmann::json jsonLeg, jsonNew;
            {
                std::ifstream f1(legP); f1 >> jsonLeg;
                std::ifstream f2(newP); f2 >> jsonNew;
            }
            REQUIRE(jsonLeg["hardwareProfile"]["hardwareId"] == jsonNew["hardwareProfile"]["hardwareId"]);
            REQUIRE(jsonLeg["hardwareProfile"]["functionId"] == jsonNew["hardwareProfile"]["functionId"]);
            REQUIRE(jsonLeg["audioCalibration"]["averageSnrDb"] == jsonNew["audioCalibration"]["averageSnrDb"]);
            REQUIRE(jsonLeg["totalPointsMeasured"] == jsonNew["totalPointsMeasured"]);
            REQUIRE(jsonNew["outputArtifacts"]["packageArtifacts"].is_array());
            REQUIRE(jsonNew["outputArtifacts"]["packageArtifacts"].size() == 3);
        }
    }
}
