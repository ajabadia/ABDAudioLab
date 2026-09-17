/**
 * @file test_PhysicalLoopback_T4_2.cpp
 * @brief Suite de pruebas unitarias y de integración para Medición de Loopback Puro (Fase 20.11 T4.2).
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../measurement/PhysicalLoopbackAdapter.h"
#include "../measurement/MeasurementContracts.h"
#include "../synth/Sha256.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <vector>

using namespace abdaudiolab::measurement;

namespace
{

/**
 * @class VirtualLoopbackCableTransport
 * @brief Mock transport que emula un cable de referencia físico entre DAC y ADC con perturbaciones configurables.
 */
class VirtualLoopbackCableTransport : public ILoopbackAudioTransport
{
public:
    bool shouldFailOpen = false;
    std::string openFailureMsg = "device_not_found";
    bool shouldFailCapture = false;
    std::string captureFailureMsg = "underflow_detected";

    int simulatedDelaySamples = 48;   // Retardo físico del cable + conversores (1 ms a 48 kHz)
    float gainLinear = 0.98f;         // Pérdida mínima de cable (-0.17 dB)
    float noiseLevel = 0.00001f;      // Ruido de fondo analógico muy bajo (SNR ~ 95 dB)
    float dcBias = 0.00001f;          // DC bias insignificante (~ -100 dBFS)

    bool truncateResponse = false;
    bool returnEmptyResponse = false;
    bool returnPureSilence = false;
    bool injectExcessiveClipping = false;
    bool injectExcessiveDc = false;
    bool injectExcessiveNoise = false;
    bool injectAmbiguousPulsing = false;

    bool open(const std::string& /*deviceName*/,
              double /*sampleRate*/,
              int /*blockSize*/,
              int /*outputChannel*/,
              int /*inputChannel*/,
              std::string& error) override
    {
        if (shouldFailOpen)
        {
            error = openFailureMsg;
            return false;
        }
        return true;
    }

    void close() override
    {
    }

    bool transmitAndCapture(const std::vector<float>& stimulus,
                            std::vector<float>& response,
                            std::string& error) override
    {
        if (shouldFailCapture)
        {
            error = captureFailureMsg;
            return false;
        }

        if (returnEmptyResponse)
        {
            response.clear();
            return true;
        }

        if (truncateResponse)
        {
            // Truncado severo: devolver solo 10 muestras
            response.assign(10, 0.0f);
            return true;
        }

        const size_t totalSize = stimulus.size() + static_cast<size_t>(std::max(0, simulatedDelaySamples)) + 256;
        response.assign(totalSize, 0.0f);

        if (returnPureSilence)
        {
            return true;
        }

        // Inyectar DC bias constante
        for (size_t i = 0; i < response.size(); ++i)
        {
            response[i] = injectExcessiveDc ? 0.05f : dcBias; // 0.05 ~ -26 dBFS DC (supera -60 dBFS)
        }

        // Simular ruido de fondo
        if (injectExcessiveNoise)
        {
            for (size_t i = 0; i < response.size(); ++i)
            {
                // Ruido blanco intenso (SNR ~ 6 dB)
                float r = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 0.4f - 0.2f;
                response[i] += r;
            }
        }
        else
        {
            for (size_t i = 0; i < response.size(); ++i)
            {
                float r = ((static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f) * noiseLevel;
                response[i] += r;
            }
        }

        // Superponer estímulo con retardo
        const size_t delay = static_cast<size_t>(std::max(0, simulatedDelaySamples));
        for (size_t i = 0; i < stimulus.size(); ++i)
        {
            if (i + delay < response.size())
            {
                float val = stimulus[i] * (injectExcessiveClipping ? 3.0f : gainLinear);
                if (injectExcessiveClipping && val > 1.0f) val = 1.0f;
                if (injectExcessiveClipping && val < -1.0f) val = -1.0f;
                response[i + delay] += val;
            }
        }

        // Inyectar un pulso ambiguo idéntico si se solicita
        if (injectAmbiguousPulsing && delay + 100 < response.size())
        {
            for (size_t i = 0; i < 4; ++i)
            {
                response[delay + 100 + i] = (i % 2 == 0 ? 1.0f : -1.0f);
            }
        }

        return true;
    }
};

} // namespace

TEST_CASE("T4.2: Especificacion de Loopback y Serializacion Round-Trip", "[loopback][t4][spec]")
{
    PhysicalLoopbackSpec spec;
    spec.deviceName = "RME Fireface UCX II";
    spec.sampleRateHz = 96000.0;
    spec.blockSize = 256;
    spec.outputChannel = 3;
    spec.inputChannel = 4;
    spec.sweepDurationSec = 1.5;
    spec.leadInSilenceSec = 0.1;
    spec.levelDbfs = -3.0f;
    spec.snrDbMin = 24.0;
    spec.peakDbfsMax = -1.0;
    spec.dcOffsetDbMax = -65.0;
    spec.maxClockDriftPpm = 25.0;

    std::string jsonStr = spec.toJsonString(2);
    REQUIRE(!jsonStr.empty());

    PhysicalLoopbackSpec restored = PhysicalLoopbackSpec::fromJsonString(jsonStr);
    CHECK(restored.deviceName == "RME Fireface UCX II");
    CHECK(restored.sampleRateHz == 96000.0);
    CHECK(restored.blockSize == 256);
    CHECK(restored.outputChannel == 3);
    CHECK(restored.inputChannel == 4);
    CHECK(restored.sweepDurationSec == Catch::Approx(1.5));
    CHECK(restored.leadInSilenceSec == Catch::Approx(0.1));
    CHECK(restored.levelDbfs == Catch::Approx(-3.0f));
    CHECK(restored.snrDbMin == Catch::Approx(24.0));
    CHECK(restored.peakDbfsMax == Catch::Approx(-1.0));
    CHECK(restored.dcOffsetDbMax == Catch::Approx(-65.0));
    CHECK(restored.maxClockDriftPpm == Catch::Approx(25.0));
}

TEST_CASE("T4.2: Validaciones de Entrada y Errores Metrologicos", "[loopback][t4][validation]")
{
    PhysicalLoopbackSpec spec;
    LoopbackCalibrationArtifacts artifacts;
    std::string error;

    SECTION("Transporte nulo")
    {
        bool ok = PhysicalLoopbackCoordinator::executeCalibration(nullptr, spec, artifacts, error);
        CHECK_FALSE(ok);
        CHECK(error == "transport_null");
    }

    VirtualLoopbackCableTransport transport;

    SECTION("Nombre de dispositivo vacio")
    {
        spec.deviceName = "";
        bool ok = PhysicalLoopbackCoordinator::executeCalibration(&transport, spec, artifacts, error);
        CHECK_FALSE(ok);
        CHECK(error == "missing_device_name");
    }

    SECTION("Sample rate invalido o cero")
    {
        spec.sampleRateHz = 0.0;
        bool ok = PhysicalLoopbackCoordinator::executeCalibration(&transport, spec, artifacts, error);
        CHECK_FALSE(ok);
        CHECK(error == "invalid_sample_rate");

        spec.sampleRateHz = -44100.0;
        ok = PhysicalLoopbackCoordinator::executeCalibration(&transport, spec, artifacts, error);
        CHECK_FALSE(ok);
        CHECK(error == "invalid_sample_rate");
    }

    SECTION("Block size invalido")
    {
        spec.blockSize = 0;
        bool ok = PhysicalLoopbackCoordinator::executeCalibration(&transport, spec, artifacts, error);
        CHECK_FALSE(ok);
        CHECK(error == "invalid_block_size");
    }

    SECTION("Canal de salida invalido (< 0)")
    {
        spec.outputChannel = -1;
        bool ok = PhysicalLoopbackCoordinator::executeCalibration(&transport, spec, artifacts, error);
        CHECK_FALSE(ok);
        CHECK(error == "invalid_output_channel");
    }

    SECTION("Canal de entrada invalido (< 0)")
    {
        spec.inputChannel = -2;
        bool ok = PhysicalLoopbackCoordinator::executeCalibration(&transport, spec, artifacts, error);
        CHECK_FALSE(ok);
        CHECK(error == "invalid_input_channel");
    }

    SECTION("Fallo al abrir transporte físico")
    {
        transport.shouldFailOpen = true;
        transport.openFailureMsg = "hardware_io_error";
        bool ok = PhysicalLoopbackCoordinator::executeCalibration(&transport, spec, artifacts, error);
        CHECK_FALSE(ok);
        CHECK(error.find("transport_open_failed") != std::string::npos);
    }

    SECTION("Fallo al capturar audio físico")
    {
        transport.shouldFailCapture = true;
        transport.captureFailureMsg = "buffer_overflow";
        bool ok = PhysicalLoopbackCoordinator::executeCalibration(&transport, spec, artifacts, error);
        CHECK_FALSE(ok);
        CHECK(error.find("transmit_and_capture_failed") != std::string::npos);
    }

    SECTION("Respuesta fisica vacia")
    {
        transport.returnEmptyResponse = true;
        bool ok = PhysicalLoopbackCoordinator::executeCalibration(&transport, spec, artifacts, error);
        CHECK_FALSE(ok);
        CHECK(error == "empty_response");
    }

    SECTION("Buffer truncado (< 64 muestras)")
    {
        transport.truncateResponse = true;
        bool ok = PhysicalLoopbackCoordinator::executeCalibration(&transport, spec, artifacts, error);
        CHECK(ok); // El coordinador ejecuta, pero el registro metrológico debe marcar fail
        CHECK(artifacts.record.status == "fail");
    }

    SECTION("Respuesta de puro silencio (sin marcador)")
    {
        transport.returnPureSilence = true;
        bool ok = PhysicalLoopbackCoordinator::executeCalibration(&transport, spec, artifacts, error);
        CHECK(ok);
        CHECK(artifacts.record.status == "fail");
        CHECK(artifacts.record.snrDb < spec.snrDbMin);
    }
}

TEST_CASE("T4.2: Evaluacion de Criterios de Aceptacion Metrologica", "[loopback][t4][criteria]")
{
    PhysicalLoopbackSpec spec;
    spec.sampleRateHz = 48000.0;
    spec.sweepDurationSec = 0.5;
    spec.leadInSilenceSec = 0.05;
    spec.snrDbMin = 18.0;
    spec.peakDbfsMax = -0.5;
    spec.dcOffsetDbMax = -60.0;

    LoopbackCalibrationArtifacts artifacts;
    std::string error;

    SECTION("Fallo por Clipping")
    {
        VirtualLoopbackCableTransport transport;
        transport.injectExcessiveClipping = true;

        bool ok = PhysicalLoopbackCoordinator::executeCalibration(&transport, spec, artifacts, error);
        REQUIRE(ok);
        CHECK(artifacts.record.status == "fail");
        CHECK(artifacts.record.peakDbfs > spec.peakDbfsMax);
    }

    SECTION("Fallo por DC Offset excesivo")
    {
        VirtualLoopbackCableTransport transport;
        transport.injectExcessiveDc = true;

        bool ok = PhysicalLoopbackCoordinator::executeCalibration(&transport, spec, artifacts, error);
        REQUIRE(ok);
        CHECK(artifacts.record.status == "fail");
        CHECK(artifacts.record.dcOffsetDb > spec.dcOffsetDbMax);
    }

    SECTION("Fallo por SNR insuficiente")
    {
        VirtualLoopbackCableTransport transport;
        transport.injectExcessiveNoise = true;

        bool ok = PhysicalLoopbackCoordinator::executeCalibration(&transport, spec, artifacts, error);
        REQUIRE(ok);
        CHECK(artifacts.record.status == "fail");
        CHECK(artifacts.record.snrDb < spec.snrDbMin);
    }
}

TEST_CASE("T4.2: Calibracion Exitosa y Prueba de Repetibilidad Virtual", "[loopback][t4][repeatability]")
{
    PhysicalLoopbackSpec spec;
    spec.sampleRateHz = 48000.0;
    spec.sweepDurationSec = 0.5;
    spec.leadInSilenceSec = 0.05;
    spec.levelDbfs = -6.0f;

    VirtualLoopbackCableTransport transport1;
    transport1.simulatedDelaySamples = 64; // Retardo exacto de 64 muestras (1.333 ms)
    transport1.noiseLevel = 0.0f;          // Determinismo exacto para la prueba de repetibilidad

    LoopbackCalibrationArtifacts run1;
    std::string error1;
    bool ok1 = PhysicalLoopbackCoordinator::executeCalibration(&transport1, spec, run1, error1);
    REQUIRE(ok1);
    REQUIRE(run1.record.status == "pass");
    CHECK(run1.tier == AnalogChainArtifactTier::LoopbackReference);
    CHECK(run1.record.roundTripLatencySamples == Catch::Approx(64.0).epsilon(0.01));
    CHECK(run1.record.roundTripLatencyMs == Catch::Approx((64.0 / 48000.0) * 1000.0).epsilon(0.01));
    CHECK(run1.record.peakDbfs < spec.peakDbfsMax);
    CHECK(run1.record.snrDb >= spec.snrDbMin);
    CHECK(run1.record.dcOffsetDb <= spec.dcOffsetDbMax);

    // Segunda corrida con idénticos parámetros y transporte
    VirtualLoopbackCableTransport transport2;
    transport2.simulatedDelaySamples = 64;
    transport2.noiseLevel = 0.0f;

    LoopbackCalibrationArtifacts run2;
    std::string error2;
    bool ok2 = PhysicalLoopbackCoordinator::executeCalibration(&transport2, spec, run2, error2);
    REQUIRE(ok2);
    REQUIRE(run2.record.status == "pass");

    // Repetibilidad metrológica
    CHECK(run1.record.roundTripLatencySamples == run2.record.roundTripLatencySamples);
    CHECK(run1.record.roundTripLatencyMs == Catch::Approx(run2.record.roundTripLatencyMs));
    CHECK(run1.record.status == run2.record.status);

    // Hashes SHA-256
    CHECK_FALSE(run1.record.stimulusSha256.empty());
    CHECK_FALSE(run1.record.responseSha256.empty());
    CHECK(run1.record.stimulusSha256 == run2.record.stimulusSha256);
    CHECK(run1.record.responseSha256 == run2.record.responseSha256);

    // Si la respuesta cambia en 1 sola muestra, el hash debe diferir obligatoriamente
    transport2.simulatedDelaySamples = 65; // Desplazar 1 muestra
    LoopbackCalibrationArtifacts run3;
    bool ok3 = PhysicalLoopbackCoordinator::executeCalibration(&transport2, spec, run3, error2);
    REQUIRE(ok3);
    CHECK(run3.record.roundTripLatencySamples == Catch::Approx(65.0).epsilon(0.01));
    CHECK(run3.record.responseSha256 != run1.record.responseSha256);
}

TEST_CASE("T4.2: Exportacion de Contenedor FAIR y Verificacion de Manifiesto", "[loopback][t4][fair]")
{
    PhysicalLoopbackSpec spec;
    spec.sampleRateHz = 48000.0;
    spec.sweepDurationSec = 0.25;
    spec.leadInSilenceSec = 0.05;

    VirtualLoopbackCableTransport transport;
    transport.simulatedDelaySamples = 48;

    LoopbackCalibrationArtifacts artifacts;
    std::string err;
    bool ok = PhysicalLoopbackCoordinator::executeCalibration(&transport, spec, artifacts, err);
    REQUIRE(ok);
    REQUIRE(artifacts.record.status == "pass");

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("ABDAudioLab_T4_2_Test_" + juce::String::toHexString(juce::Random::getSystemRandom().nextInt()));

    if (tempDir.exists())
        tempDir.deleteRecursively();

    bool exportOk = LoopbackContainerExporter::exportLoopbackPackage(tempDir, artifacts, err);
    REQUIRE(exportOk);
    REQUIRE(err.empty());

    // Verificar estructura de los 6 artefactos obligatorios
    juce::File specFile = tempDir.getChildFile("specs/loopback_calibration_spec.json");
    juce::File stimWav = tempDir.getChildFile("audio/loopback_reference_stimulus.wav");
    juce::File respWav = tempDir.getChildFile("audio/loopback_reference_response.wav");
    juce::File resultJson = tempDir.getChildFile("results/loopback_calibration_result.json");
    juce::File reportHtml = tempDir.getChildFile("reports/loopback_calibration_report.html");
    juce::File manifestFile = tempDir.getChildFile("manifest.json");

    CHECK(specFile.existsAsFile());
    CHECK(stimWav.existsAsFile());
    CHECK(respWav.existsAsFile());
    CHECK(resultJson.existsAsFile());
    CHECK(reportHtml.existsAsFile());
    CHECK(manifestFile.existsAsFile());

    // Validar contenido del manifiesto
    std::string manifestContent = manifestFile.loadFileAsString().toStdString();
    REQUIRE_FALSE(manifestContent.empty());

    auto mf = nlohmann::ordered_json::parse(manifestContent);
    CHECK(mf["schemaVersion"] == "loopback-calibration-1.0");
    CHECK(mf["artifactTier"] == "loopback_reference");
    CHECK(mf["calibrationId"] == artifacts.record.calibrationId);
    CHECK(mf["roundTripLatencySamples"] == Catch::Approx(artifacts.record.roundTripLatencySamples));
    CHECK(mf["roundTripLatencyMs"] == Catch::Approx(artifacts.record.roundTripLatencyMs));
    CHECK(mf["stimulusSha256"] == artifacts.record.stimulusSha256);
    CHECK(mf["responseSha256"] == artifacts.record.responseSha256);
    CHECK(mf["status"] == "pass");

    // Verificar presencia de los 5 artefactos internos en el manifest array
    REQUIRE(mf.contains("artifacts"));
    CHECK(mf["artifacts"].size() == 5);

    for (const auto& art : mf["artifacts"])
    {
        std::string role = art["role"];
        std::string path = art["path"];
        std::string sha = art["sha256"];
        CHECK_FALSE(role.empty());
        CHECK_FALSE(path.empty());
        CHECK(sha.length() == 64);
        CHECK(tempDir.getChildFile(path).existsAsFile());
    }

    // Limpieza de directorio temporal
    tempDir.deleteRecursively();
}
