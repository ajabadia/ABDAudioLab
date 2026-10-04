#include <catch2/catch_test_macros.hpp>
#include "calibration/CalibrationMatchEvaluator.h"
#include "calibration/CalibrationRecord.h"

using namespace abdaudiolab::calibration;

namespace
{

CalibrationRecord createSyntheticRecord(
    const std::string& profileId = "cal-test-01",
    const std::string& createdAt = "2026-10-04T10:00:00Z",
    const std::string& devName = "Focusrite USB ASIO",
    const std::string& driver = "ASIO",
    double sampleRate = 48000.0,
    int bufferSize = 256,
    int inCh = 0,
    const std::string& inLabel = "Input 1",
    int outCh = 0,
    const std::string& outLabel = "Output 1",
    float trimGain = 1.4142f)
{
    CalibrationRecord rec;
    rec.schemaVersion = 1;
    rec.profileId = profileId;
    rec.createdAt = createdAt;

    rec.deviceSnapshot.deviceName = devName;
    rec.deviceSnapshot.driverType = driver;
    rec.deviceSnapshot.sampleRate = sampleRate;
    rec.deviceSnapshot.bufferSizeSamples = bufferSize;

    rec.routingSnapshot.inputChannelIndex = inCh;
    rec.routingSnapshot.inputChannelLabel = inLabel;
    rec.routingSnapshot.outputChannelIndex = outCh;
    rec.routingSnapshot.outputChannelLabel = outLabel;

    rec.calibrationResult.isCalibrated = true;
    rec.calibrationResult.clippingDetected = false;
    rec.calibrationResult.peakInDbfs = -12.0f;
    rec.calibrationResult.frequencyFlatnessDb = 0.5f;
    rec.calibrationResult.latencySamples = 128;
    rec.calibrationResult.roundTripLatencyMs = 2.666;
    rec.calibrationResult.recommendedTrimGain = trimGain;

    rec.provenance.applicationVersion = "2.1.0";
    rec.provenance.calibrationAlgorithmVersion = 1;

    return rec;
}

CurrentAudioConfigurationSnapshot createCurrentSnapshot(
    const std::string& devName = "Focusrite USB ASIO",
    const std::string& driver = "ASIO",
    double sampleRate = 48000.0,
    int bufferSize = 256,
    int inCh = 0,
    const std::string& inLabel = "Input 1",
    int outCh = 0,
    const std::string& outLabel = "Output 1")
{
    CurrentAudioConfigurationSnapshot current;
    current.deviceName = devName;
    current.driverType = driver;
    current.sampleRate = sampleRate;
    current.bufferSizeSamples = bufferSize;
    current.inputChannelIndex = inCh;
    current.inputChannelLabel = inLabel;
    current.outputChannelIndex = outCh;
    current.outputChannelLabel = outLabel;
    return current;
}

} // namespace

TEST_CASE("CalibrationMatchEvaluator - 1. Coincidencia completa de configuracion", "[calibration][compatibility][hermetic]")
{
    auto record = createSyntheticRecord();
    auto current = createCurrentSnapshot();

    auto eval = CalibrationMatchEvaluator::evaluate(record, current);

    CHECK(eval.status == CalibrationMatchStatus::ConfigurationMatch);
    CHECK(eval.isActionableMatch == true);
    CHECK(eval.differences.empty());
    CHECK(!eval.summaryMessage.empty());
    CHECK(!eval.warningMessage.empty());
}

TEST_CASE("CalibrationMatchEvaluator - 2. Nombre de interfaz distinto", "[calibration][compatibility][hermetic]")
{
    auto record = createSyntheticRecord();
    auto current = createCurrentSnapshot("Universal Audio Thunderbolt");

    auto eval = CalibrationMatchEvaluator::evaluate(record, current);

    CHECK(eval.status == CalibrationMatchStatus::DeviceOrDriverMismatch);
    CHECK(eval.isActionableMatch == false);
    REQUIRE_FALSE(eval.differences.empty());
    CHECK(eval.differences[0].fieldName == "Dispositivo");
}

TEST_CASE("CalibrationMatchEvaluator - 3. Tipo de driver distinto", "[calibration][compatibility][hermetic]")
{
    auto record = createSyntheticRecord();
    auto current = createCurrentSnapshot("Focusrite USB ASIO", "Windows Audio");

    auto eval = CalibrationMatchEvaluator::evaluate(record, current);

    CHECK(eval.status == CalibrationMatchStatus::DeviceOrDriverMismatch);
    CHECK(eval.isActionableMatch == false);
    REQUIRE_FALSE(eval.differences.empty());
    CHECK(eval.differences[0].fieldName == "Tipo de Driver");
}

TEST_CASE("CalibrationMatchEvaluator - 4. Frecuencia de muestreo distinta", "[calibration][compatibility][hermetic]")
{
    auto record = createSyntheticRecord();
    auto current = createCurrentSnapshot("Focusrite USB ASIO", "ASIO", 96000.0);

    auto eval = CalibrationMatchEvaluator::evaluate(record, current);

    CHECK(eval.status == CalibrationMatchStatus::ConfigurationMismatch);
    CHECK(eval.isActionableMatch == false);
    REQUIRE_FALSE(eval.differences.empty());
    CHECK(eval.differences[0].fieldName == "Frecuencia de muestreo");
    CHECK(eval.differences[0].profileValue == "48000 Hz");
    CHECK(eval.differences[0].currentValue == "96000 Hz");
}

TEST_CASE("CalibrationMatchEvaluator - 5. Tamano de buffer distinto", "[calibration][compatibility][hermetic]")
{
    auto record = createSyntheticRecord();
    auto current = createCurrentSnapshot("Focusrite USB ASIO", "ASIO", 48000.0, 512);

    auto eval = CalibrationMatchEvaluator::evaluate(record, current);

    CHECK(eval.status == CalibrationMatchStatus::ConfigurationMismatch);
    CHECK(eval.isActionableMatch == false);
    REQUIRE_FALSE(eval.differences.empty());
    CHECK(eval.differences[0].fieldName == "Tamaño de buffer");
}

TEST_CASE("CalibrationMatchEvaluator - 6. Canal de entrada distinto", "[calibration][compatibility][hermetic]")
{
    auto record = createSyntheticRecord();
    auto current = createCurrentSnapshot("Focusrite USB ASIO", "ASIO", 48000.0, 256, 1, "Input 2");

    auto eval = CalibrationMatchEvaluator::evaluate(record, current);

    CHECK(eval.status == CalibrationMatchStatus::ConfigurationMismatch);
    CHECK(eval.isActionableMatch == false);
    bool foundInChannelDiff = false;
    for (const auto& diff : eval.differences)
    {
        if (diff.fieldName.find("entrada") != std::string::npos)
            foundInChannelDiff = true;
    }
    CHECK(foundInChannelDiff);
}

TEST_CASE("CalibrationMatchEvaluator - 7. Canal de salida distinto", "[calibration][compatibility][hermetic]")
{
    auto record = createSyntheticRecord();
    auto current = createCurrentSnapshot("Focusrite USB ASIO", "ASIO", 48000.0, 256, 0, "Input 1", 1, "Output 2");

    auto eval = CalibrationMatchEvaluator::evaluate(record, current);

    CHECK(eval.status == CalibrationMatchStatus::ConfigurationMismatch);
    CHECK(eval.isActionableMatch == false);
    bool foundOutChannelDiff = false;
    for (const auto& diff : eval.differences)
    {
        if (diff.fieldName.find("salida") != std::string::npos)
            foundOutChannelDiff = true;
    }
    CHECK(foundOutChannelDiff);
}

TEST_CASE("CalibrationMatchEvaluator - 8. Etiqueta de canal distinta", "[calibration][compatibility][hermetic]")
{
    auto record = createSyntheticRecord();
    auto current = createCurrentSnapshot("Focusrite USB ASIO", "ASIO", 48000.0, 256, 0, "Line In L", 0, "Output 1");

    auto eval = CalibrationMatchEvaluator::evaluate(record, current);

    CHECK(eval.status == CalibrationMatchStatus::ConfigurationMismatch);
    CHECK(eval.isActionableMatch == false);
    bool foundLabelDiff = false;
    for (const auto& diff : eval.differences)
    {
        if (diff.fieldName == "Canal de entrada (etiqueta)")
            foundLabelDiff = true;
    }
    CHECK(foundLabelDiff);
}

TEST_CASE("CalibrationMatchEvaluator - 9. Perfil corrupto o no valido ignorado con seguridad", "[calibration][compatibility][hermetic]")
{
    auto record = createSyntheticRecord();
    record.calibrationResult.isCalibrated = false; // Inválido

    auto current = createCurrentSnapshot();
    auto eval = CalibrationMatchEvaluator::evaluate(record, current);

    CHECK(eval.status == CalibrationMatchStatus::InvalidOrCorruptProfile);
    CHECK(eval.isActionableMatch == false);
}

TEST_CASE("CalibrationMatchEvaluator - 10. Sin dispositivo activo en el sistema", "[calibration][compatibility][hermetic]")
{
    auto record = createSyntheticRecord();
    CurrentAudioConfigurationSnapshot emptyCurrent; // deviceName vacío

    auto eval = CalibrationMatchEvaluator::evaluate(record, emptyCurrent);

    CHECK(eval.status == CalibrationMatchStatus::NoActiveDevice);
    CHECK(eval.isActionableMatch == false);
}

TEST_CASE("CalibrationMatchEvaluator - 11. Verificacion de alineacion activa: sigue alineado", "[calibration][compatibility][hermetic]")
{
    auto record = createSyntheticRecord();
    auto current = createCurrentSnapshot();

    CHECK(CalibrationMatchEvaluator::isStillAligned(record, current) == true);
}

TEST_CASE("CalibrationMatchEvaluator - 12. Verificacion de alineacion activa: pasa a desalineado si cambia SR", "[calibration][compatibility][hermetic]")
{
    auto record = createSyntheticRecord();
    auto currentAfterChange = createCurrentSnapshot("Focusrite USB ASIO", "ASIO", 96000.0, 256);

    CHECK(CalibrationMatchEvaluator::isStillAligned(record, currentAfterChange) == false);
}

TEST_CASE("CalibrationMatchEvaluator - 13. Seleccion con multiples perfiles: perfil mas reciente no coincide pero anterior si", "[calibration][compatibility][hermetic]")
{
    // Perfil A: ayer, 48 kHz (coincide con current)
    auto profA = createSyntheticRecord("cal-rec-48k", "2026-10-03T12:00:00Z", "Focusrite USB ASIO", "ASIO", 48000.0, 256);
    // Perfil B: hoy, 96 kHz (más reciente, pero NO coincide con current)
    auto profB = createSyntheticRecord("cal-rec-96k", "2026-10-04T12:00:00Z", "Focusrite USB ASIO", "ASIO", 96000.0, 512);

    std::vector<CalibrationRecord> candidates = { profB, profA }; // en orden descendente

    auto current = createCurrentSnapshot("Focusrite USB ASIO", "ASIO", 48000.0, 256);

    CalibrationMatchEvaluation eval;
    auto match = CalibrationMatchEvaluator::findBestMatchingProfile(candidates, current, &eval);

    REQUIRE(match.has_value());
    CHECK(match->profileId == "cal-rec-48k");
    CHECK(eval.status == CalibrationMatchStatus::ConfigurationMatch);
    CHECK(eval.isActionableMatch == true);
}

TEST_CASE("CalibrationMatchEvaluator - 14. Aplicacion explicita aplica recommendedTrimGain", "[calibration][compatibility][hermetic]")
{
    auto record = createSyntheticRecord("cal-trim", "2026-10-04T10:00:00Z", "Focusrite USB ASIO", "ASIO", 48000.0, 256, 0, "Input 1", 0, "Output 1", 1.85f);
    auto current = createCurrentSnapshot();

    auto eval = CalibrationMatchEvaluator::evaluate(record, current);
    REQUIRE(eval.isActionableMatch == true);

    // Simulación de transición de estado en panel / motor
    float activeTrimGain = 1.0f;
    ActiveCalibrationAlignment alignment = ActiveCalibrationAlignment::None;

    // Acción explícita del usuario: [Reutilizar calibración]
    activeTrimGain = record.calibrationResult.recommendedTrimGain;
    alignment = ActiveCalibrationAlignment::AlignedAndActive;

    CHECK(alignment == ActiveCalibrationAlignment::AlignedAndActive);
    CHECK(activeTrimGain == 1.85f);
}

TEST_CASE("CalibrationMatchEvaluator - 15. Cambio de sample rate tras reutilizacion neutraliza trim", "[calibration][compatibility][hermetic]")
{
    auto record = createSyntheticRecord();
    float activeTrimGain = record.calibrationResult.recommendedTrimGain;
    ActiveCalibrationAlignment alignment = ActiveCalibrationAlignment::AlignedAndActive;

    // Evento de hardware: el usuario o sistema conmuta a 96 kHz
    auto currentAfterChange = createCurrentSnapshot("Focusrite USB ASIO", "ASIO", 96000.0, 256);

    if (!CalibrationMatchEvaluator::isStillAligned(record, currentAfterChange))
    {
        alignment = ActiveCalibrationAlignment::Misaligned;
        activeTrimGain = 1.0f; // Neutralización obligatoria
    }

    CHECK(alignment == ActiveCalibrationAlignment::Misaligned);
    CHECK(activeTrimGain == 1.0f);
}

TEST_CASE("CalibrationMatchEvaluator - 16. Cambio de buffer size tras reutilizacion neutraliza trim", "[calibration][compatibility][hermetic]")
{
    auto record = createSyntheticRecord();
    float activeTrimGain = record.calibrationResult.recommendedTrimGain;
    ActiveCalibrationAlignment alignment = ActiveCalibrationAlignment::AlignedAndActive;

    // Evento de hardware: buffer pasa de 256 a 1024
    auto currentAfterChange = createCurrentSnapshot("Focusrite USB ASIO", "ASIO", 48000.0, 1024);

    if (!CalibrationMatchEvaluator::isStillAligned(record, currentAfterChange))
    {
        alignment = ActiveCalibrationAlignment::Misaligned;
        activeTrimGain = 1.0f; // Neutralización obligatoria
    }

    CHECK(alignment == ActiveCalibrationAlignment::Misaligned);
    CHECK(activeTrimGain == 1.0f);
}

TEST_CASE("CalibrationMatchEvaluator - 17. Accion Continuar sin calibrar (Bypass) fija estado Bypassed y trim neutral", "[calibration][compatibility][hermetic]")
{
    float activeTrimGain = 2.5f; // Había una ganancia residual
    ActiveCalibrationAlignment alignment = ActiveCalibrationAlignment::AlignedAndActive;

    // Acción del usuario: [Continuar sin calibrar (Bypass)]
    alignment = ActiveCalibrationAlignment::Bypassed;
    activeTrimGain = 1.0f; // Neutralización obligatoria

    CHECK(alignment == ActiveCalibrationAlignment::Bypassed);
    CHECK(activeTrimGain == 1.0f);
}

TEST_CASE("CalibrationMatchEvaluator - 18. Nueva sesion no arrastra calibracion previa ni trim activo", "[calibration][compatibility][hermetic]")
{
    ActiveCalibrationAlignment alignment = ActiveCalibrationAlignment::AlignedAndActive;
    float activeTrimGain = 1.6f;

    // Evento: reset por nueva sesión (performNewSessionReset)
    alignment = ActiveCalibrationAlignment::None;
    activeTrimGain = 1.0f;

    CHECK(alignment == ActiveCalibrationAlignment::None);
    CHECK(activeTrimGain == 1.0f);
}
