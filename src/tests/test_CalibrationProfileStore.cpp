#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "calibration/CalibrationProfileStore.h"
#include "support/LabTestScratch.h"
#include <nlohmann/json.hpp>
#include <fstream>

using namespace abdaudiolab::calibration;

namespace
{

CalibrationRecord makeValidRecord(const std::string& profileId = "test-cal-01",
                                  const std::string& createdAt = "2026-10-04T10:30:00Z")
{
    CalibrationRecord rec;
    rec.schemaVersion = 1;
    rec.profileId = profileId;
    rec.createdAt = createdAt;

    rec.deviceSnapshot.deviceName = "Focusrite USB ASIO";
    rec.deviceSnapshot.driverType = "ASIO";
    rec.deviceSnapshot.sampleRate = 48000.0;
    rec.deviceSnapshot.bufferSizeSamples = 256;

    rec.routingSnapshot.inputChannelIndex = 0;
    rec.routingSnapshot.inputChannelLabel = "Input 1";
    rec.routingSnapshot.outputChannelIndex = 0;
    rec.routingSnapshot.outputChannelLabel = "Output 1";

    rec.calibrationResult.isCalibrated = true;
    rec.calibrationResult.sampleRate = 48000.0;
    rec.calibrationResult.peakInDbfs = -3.1f;
    rec.calibrationResult.recommendedTrimGain = 0.76f;
    rec.calibrationResult.targetHeadroomDbfs = -3.0f;
    rec.calibrationResult.roundTripLatencyMs = 12.4f;
    rec.calibrationResult.latencySamples = 595;
    rec.calibrationResult.snrDb = 82.0f;
    rec.calibrationResult.frequencyFlatnessDb = 1.7f;
    rec.calibrationResult.phaseInversionDetected = false;
    rec.calibrationResult.phaseInversionCorrelation = 1.0f;
    rec.calibrationResult.clippingDetected = false;
    rec.calibrationResult.clippedSamplesCount = 0;
    rec.calibrationResult.dcOffsetVolts = 0.001f;

    rec.provenance.applicationVersion = "2.1.0";
    rec.provenance.calibrationAlgorithmVersion = 1;

    return rec;
}

} // namespace

TEST_CASE("CalibrationProfileStore: Guardado atomico y carga de calibracion valida", "[calibration][store][hermetic]")
{
    auto scratch = abdaudiolab::test::ScratchDir("calib_store_save_load");
    CalibrationProfileStore store(scratch.path());

    auto record = makeValidRecord("focusrite-48k-256", "2026-10-04T12:00:00Z");

    auto saveRes = store.save(record);
    REQUIRE(saveRes.success == true);
    REQUIRE(saveRes.profileId == "focusrite-48k-256");

    auto targetFile = scratch.path().getChildFile("focusrite-48k-256.json");
    REQUIRE(targetFile.existsAsFile());

    // El archivo temporal .tmp no debe haber quedado
    auto tmpFile = scratch.path().getChildFile("focusrite-48k-256.json.tmp");
    REQUIRE_FALSE(tmpFile.existsAsFile());

    auto loadedOpt = store.load("focusrite-48k-256");
    REQUIRE(loadedOpt.has_value());
    const auto& loaded = *loadedOpt;

    CHECK(loaded.schemaVersion == 1);
    CHECK(loaded.profileId == "focusrite-48k-256");
    CHECK(loaded.createdAt == "2026-10-04T12:00:00Z");

    CHECK(loaded.deviceSnapshot.deviceName == "Focusrite USB ASIO");
    CHECK(loaded.deviceSnapshot.driverType == "ASIO");
    CHECK(loaded.deviceSnapshot.sampleRate == Catch::Approx(48000.0));
    CHECK(loaded.deviceSnapshot.bufferSizeSamples == 256);

    CHECK(loaded.routingSnapshot.inputChannelIndex == 0);
    CHECK(loaded.routingSnapshot.inputChannelLabel == "Input 1");
    CHECK(loaded.routingSnapshot.outputChannelIndex == 0);
    CHECK(loaded.routingSnapshot.outputChannelLabel == "Output 1");

    CHECK(loaded.calibrationResult.isCalibrated == true);
    CHECK(loaded.calibrationResult.latencySamples == 595);
    CHECK(loaded.calibrationResult.roundTripLatencyMs == Catch::Approx(12.4f));
    CHECK(loaded.calibrationResult.recommendedTrimGain == Catch::Approx(0.76f));
    CHECK(loaded.calibrationResult.peakInDbfs == Catch::Approx(-3.1f));
    CHECK(loaded.calibrationResult.snrDb == Catch::Approx(82.0f));
    CHECK(loaded.calibrationResult.frequencyFlatnessDb == Catch::Approx(1.7f));
    CHECK(loaded.calibrationResult.phaseInversionDetected == false);
    CHECK(loaded.calibrationResult.clippingDetected == false);
    CHECK(loaded.calibrationResult.clippedSamplesCount == 0);

    CHECK(loaded.provenance.applicationVersion == "2.1.0");
    CHECK(loaded.provenance.calibrationAlgorithmVersion == 1);
}

TEST_CASE("CalibrationProfileStore: Listado ordenado por createdAt descendente", "[calibration][store][hermetic]")
{
    auto scratch = abdaudiolab::test::ScratchDir("calib_store_list_order");
    CalibrationProfileStore store(scratch.path());

    // Crear 3 perfiles con fechas deterministas distintas
    auto recOld = makeValidRecord("cal-antigua", "2026-01-01T08:00:00Z");
    auto recRecent = makeValidRecord("cal-reciente", "2026-10-04T10:30:00Z");
    auto recMid = makeValidRecord("cal-intermedia", "2026-05-15T14:00:00Z");

    REQUIRE(store.save(recOld).success);
    REQUIRE(store.save(recRecent).success);
    REQUIRE(store.save(recMid).success);

    auto list = store.list();
    REQUIRE(list.size() == 3);

    // Más reciente primero
    CHECK(list[0].profileId == "cal-reciente");
    CHECK(list[1].profileId == "cal-intermedia");
    CHECK(list[2].profileId == "cal-antigua");
}

TEST_CASE("CalibrationProfileStore: Eliminacion de un perfil", "[calibration][store][hermetic]")
{
    auto scratch = abdaudiolab::test::ScratchDir("calib_store_remove");
    CalibrationProfileStore store(scratch.path());

    auto rec = makeValidRecord("cal-a-borrar", "2026-10-04T10:00:00Z");
    REQUIRE(store.save(rec).success);
    REQUIRE(store.list().size() == 1);

    CHECK(store.remove("cal-a-borrar") == true);
    CHECK(store.list().empty());
    CHECK(store.remove("cal-a-borrar") == false); // Ya no existe
}

TEST_CASE("CalibrationProfileStore: Rechazo seguro de calibracion invalida o con clipping", "[calibration][store][hermetic]")
{
    auto scratch = abdaudiolab::test::ScratchDir("calib_store_reject_invalid");
    CalibrationProfileStore store(scratch.path());

    SECTION("Rechaza isCalibrated == false")
    {
        auto rec = makeValidRecord("cal-invalida");
        rec.calibrationResult.isCalibrated = false;

        auto res = store.save(rec);
        CHECK(res.success == false);
        CHECK_FALSE(res.errorMessage.empty());
        CHECK_FALSE(scratch.path().getChildFile("cal-invalida.json").existsAsFile());
    }

    SECTION("Rechaza clippingDetected == true")
    {
        auto rec = makeValidRecord("cal-saturada");
        rec.calibrationResult.clippingDetected = true;
        rec.calibrationResult.clippedSamplesCount = 5;

        auto res = store.save(rec);
        CHECK(res.success == false);
        CHECK_FALSE(res.errorMessage.empty());
        CHECK_FALSE(scratch.path().getChildFile("cal-saturada.json").existsAsFile());
    }
}

TEST_CASE("CalibrationProfileStore: Proteccion contra sobrescritura silenciosa", "[calibration][store][hermetic]")
{
    auto scratch = abdaudiolab::test::ScratchDir("calib_store_no_overwrite");
    CalibrationProfileStore store(scratch.path());

    auto rec1 = makeValidRecord("cal-duplicada", "2026-01-01T00:00:00Z");
    rec1.calibrationResult.roundTripLatencyMs = 10.0f;
    REQUIRE(store.save(rec1).success);

    // Intentar guardar con el mismo ID sin overwrite = true
    auto rec2 = makeValidRecord("cal-duplicada", "2026-02-02T00:00:00Z");
    rec2.calibrationResult.roundTripLatencyMs = 20.0f;

    auto res = store.save(rec2, false);
    CHECK(res.success == false);

    // Verificar que el registro original no fue modificado
    auto original = store.load("cal-duplicada");
    REQUIRE(original.has_value());
    CHECK(original->calibrationResult.roundTripLatencyMs == Catch::Approx(10.0f));

    // Si se solicita explicitamente overwrite = true, si sobrescribe
    auto resOverwrite = store.save(rec2, true);
    CHECK(resOverwrite.success == true);
    auto updated = store.load("cal-duplicada");
    REQUIRE(updated.has_value());
    CHECK(updated->calibrationResult.roundTripLatencyMs == Catch::Approx(20.0f));
}

TEST_CASE("CalibrationProfileStore: Resiliencia ante JSON corrupto y schemaVersion desconocido", "[calibration][store][hermetic]")
{
    auto scratch = abdaudiolab::test::ScratchDir("calib_store_corrupt_and_schema");
    CalibrationProfileStore store(scratch.path());

    // 1. Crear un archivo con JSON sintacticamente corrupto
    auto corruptFile = scratch.path().getChildFile("corrupto.json");
    {
        std::ofstream out(corruptFile.getFullPathName().toStdString());
        out << "{ \"schemaVersion\": 1, \"incompleto\": ";
    }

    // 2. Crear un archivo con schemaVersion desconocido (ej. 999)
    auto futureFile = scratch.path().getChildFile("futuro.json");
    {
        std::ofstream out(futureFile.getFullPathName().toStdString());
        out << "{ \"schemaVersion\": 999, \"profileId\": \"futuro\" }";
    }

    // 3. Crear uno valido
    auto validRec = makeValidRecord("valido", "2026-10-04T12:00:00Z");
    REQUIRE(store.save(validRec).success);

    // Cargar directamente los no validos no debe explotar
    CHECK_FALSE(store.load("corrupto").has_value());
    CHECK_FALSE(store.load("futuro").has_value());
    CHECK(store.load("valido").has_value());

    // list() debe excluir los invalidos sin explotar
    auto list = store.list();
    REQUIRE(list.size() == 1);
    CHECK(list[0].profileId == "valido");
}

TEST_CASE("CalibrationProfileStore: Archivos temporales e incompletos nunca aparecen en list()", "[calibration][store][hermetic]")
{
    auto scratch = abdaudiolab::test::ScratchDir("calib_store_tmp_files");
    CalibrationProfileStore store(scratch.path());

    // Guardar uno valido
    auto validRec = makeValidRecord("perfil-bueno", "2026-10-04T12:00:00Z");
    REQUIRE(store.save(validRec).success);

    // Simular archivo temporal abandonado por caida o corte de energia
    auto fakeTmp = scratch.path().getChildFile("perfil-incompleto.json.tmp");
    {
        std::ofstream out(fakeTmp.getFullPathName().toStdString());
        out << "{ \"schemaVersion\": 1 }";
    }

    auto list = store.list();
    REQUIRE(list.size() == 1);
    CHECK(list[0].profileId == "perfil-bueno");
}

TEST_CASE("CalibrationProfileStore: Rechazo de profileId inseguro y path traversal", "[calibration][store][hermetic]")
{
    auto scratch = abdaudiolab::test::ScratchDir("calib_store_unsafe_id");
    CalibrationProfileStore store(scratch.path());

    std::vector<std::string> unsafeIds = {
        "../escape",
        "..\\escape_win",
        "../otra-ruta",
        "..\\otra-ruta",
        "perfil/con-barra",
        "perfil\\con-barra",
        "perfil:con-dos-puntos",
        "/etc/passwd",
        "C:\\Windows\\System32\\bad",
        "con espacios",
        "dos:puntos",
        "",
        "signo$dolar",
        "asterisco*bad"
    };

    for (const auto& badId : unsafeIds)
    {
        auto rec = makeValidRecord(badId);
        auto res = store.save(rec);
        CHECK(res.success == false);
        CHECK(store.load(badId) == std::nullopt);
        CHECK(store.remove(badId) == false);
    }

    // Ningun archivo debe haberse creado en scratch
    CHECK(store.list().empty());
}

TEST_CASE("CalibrationProfileStore: Directorio no escribible o fallo de escritura no deja perfil invalido", "[calibration][store][hermetic]")
{
    auto scratch = abdaudiolab::test::ScratchDir("calib_store_write_failure");
    auto blockingFile = scratch.path().getChildFile("blocking_file.txt");
    blockingFile.create();

    // Directorio imposible porque 'blocking_file.txt' es un archivo regular
    auto impossibleDir = blockingFile.getChildFile("sub_calibrations");
    CalibrationProfileStore store(impossibleDir);

    auto rec = makeValidRecord("perfil-fallido", "2026-10-04T12:00:00Z");
    auto res = store.save(rec);

    CHECK(res.success == false);
    CHECK_FALSE(res.errorMessage.empty());
    CHECK(store.list().empty());
}

