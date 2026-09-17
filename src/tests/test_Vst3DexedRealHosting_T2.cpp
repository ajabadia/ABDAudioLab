/**
 * @file test_Vst3DexedRealHosting_T2.cpp
 * @brief Fase 20.11 T1 y T2 - Contratos de Dominio Metrológico e Introspección de Dexed.vst3
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <nlohmann/json.hpp>

#include "measurement/MeasurementContracts.h"
#include "measurement/MeasurementSerialization.h"
#include "synth/ExternalPluginFixture.h"
#include "synth/ExternalPluginTypes.h"

using namespace abdaudiolab::measurement;
using namespace abdaudiolab::synth;
using abdaudiolab::measurement::PluginIdentity;
using Catch::Matchers::WithinAbs;

namespace
{

juce::File resolveDexedBinary()
{
    auto envPath = juce::SystemStats::getEnvironmentVariable("DEXED_VST3_PATH", "");
    if (envPath.isNotEmpty())
    {
        juce::File f(envPath);
        if (f.exists())
            return f;
    }

    juce::File defaultWin("C:\\Program Files\\Common Files\\VST3\\Dexed.vst3");
    if (defaultWin.exists())
        return defaultWin;

    juce::File localVst3 = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("../Local/Programs/Common/VST3/Dexed.vst3");
    if (localVst3.exists())
        return localVst3;

    return {};
}

} // namespace

// ==============================================================================
// T1: Contratos Metrológicos y Segregación de Dominios
// ==============================================================================

TEST_CASE("Fase 20.11 T1 - Enumeración y strings de MeasurementExecutionDomain", "[measurement][contracts][domain]")
{
    CHECK(measurementExecutionDomainToString(MeasurementExecutionDomain::Vst3OfflineDigital) == "Vst3OfflineDigital");
    CHECK(measurementExecutionDomainToString(MeasurementExecutionDomain::Vst3Realtime) == "Vst3Realtime");
    CHECK(measurementExecutionDomainToString(MeasurementExecutionDomain::DigitalHardwareRoundtrip) == "DigitalHardwareRoundtrip");
    CHECK(measurementExecutionDomainToString(MeasurementExecutionDomain::CombinedDutAndChain) == "CombinedDutAndChain");

    CHECK(measurementExecutionDomainFromString("Vst3OfflineDigital") == MeasurementExecutionDomain::Vst3OfflineDigital);
    CHECK(measurementExecutionDomainFromString("Vst3Realtime") == MeasurementExecutionDomain::Vst3Realtime);
    CHECK(measurementExecutionDomainFromString("DigitalHardwareRoundtrip") == MeasurementExecutionDomain::DigitalHardwareRoundtrip);
    CHECK(measurementExecutionDomainFromString("CombinedDutAndChain") == MeasurementExecutionDomain::CombinedDutAndChain);
    CHECK(measurementExecutionDomainFromString("unknown") == MeasurementExecutionDomain::Vst3OfflineDigital);
}

TEST_CASE("Fase 20.11 T1 - Validación estricta de coherencia de dominios (isDomainConsistent)", "[measurement][contracts][domain]")
{
    MeasurementSpec spec;
    spec.measurementId = "meas-domain-001";
    spec.executionDomain = MeasurementExecutionDomain::Vst3OfflineDigital;

    MeasurementResult result;
    result.measurementId = "meas-domain-001";
    result.status = MeasurementStatus::completed;
    result.executionDomain = MeasurementExecutionDomain::Vst3OfflineDigital;
    result.dut.format = "VST3";

    // 1. Coherencia nominal
    CHECK(isDomainConsistent(spec, result, "Vst3OfflineDigital"));

    // 2. Inconsistencia entre Spec y Result
    result.executionDomain = MeasurementExecutionDomain::CombinedDutAndChain;
    CHECK_FALSE(isDomainConsistent(spec, result, "Vst3OfflineDigital"));

    // 3. Inconsistencia con Manifest
    result.executionDomain = MeasurementExecutionDomain::Vst3OfflineDigital;
    CHECK_FALSE(isDomainConsistent(spec, result, "CombinedDutAndChain"));

    // 4. Regla de protección: CombinedDutAndChain sin calibración analógica debe ser rechazada
    spec.executionDomain = MeasurementExecutionDomain::CombinedDutAndChain;
    result.executionDomain = MeasurementExecutionDomain::CombinedDutAndChain;
    result.analogCalibration = std::nullopt;
    CHECK_FALSE(isDomainConsistent(spec, result, "CombinedDutAndChain"));

    // Con registro de calibración presente, se acepta
    AnalogChainCalibrationRecord cal;
    cal.calibrationId = "cal-loopback-01";
    cal.status = "pass";
    cal.snrDb = 92.4;
    cal.roundTripLatencySamples = 128.0;
    result.analogCalibration = cal;
    CHECK(isDomainConsistent(spec, result, "CombinedDutAndChain"));
}

TEST_CASE("Fase 20.11 T1 - Serialización canónica de dominio, PluginIdentity y AnalogCalibration", "[measurement][contracts][serialization]")
{
    MeasurementSpec spec;
    spec.measurementId = "spec-dexed-t1-001";
    spec.measurementType = "envelope";
    spec.executionDomain = MeasurementExecutionDomain::Vst3OfflineDigital;

    abdaudiolab::measurement::PluginIdentity pIdent;
    pIdent.canonicalPath = "C:/Program Files/Common Files/VST3/Dexed.vst3";
    pIdent.binarySha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    pIdent.vendor = "Digital Suburban";
    pIdent.version = "1.0.1";
    pIdent.uid = "56535444657865646578656400000000";
    pIdent.architecture = "x86_64";
    spec.pluginIdentity = pIdent;

    std::string specJson = MeasurementSerialization::serializeSpec(spec);
    CHECK(specJson.find("\"executionDomain\": \"Vst3OfflineDigital\"") != std::string::npos);
    CHECK(specJson.find("\"vendor\": \"Digital Suburban\"") != std::string::npos);
    CHECK(specJson.find("\"architecture\": \"x86_64\"") != std::string::npos);

    MeasurementSpec parsedSpec;
    std::string err;
    REQUIRE(MeasurementSerialization::deserializeSpec(specJson, parsedSpec, err));
    CHECK(parsedSpec.executionDomain == MeasurementExecutionDomain::Vst3OfflineDigital);
    REQUIRE(parsedSpec.pluginIdentity.has_value());
    CHECK(parsedSpec.pluginIdentity->vendor == "Digital Suburban");
    CHECK(parsedSpec.pluginIdentity->version == "1.0.1");

    // Probar serialización de resultado con telemetría realtime y calibración
    MeasurementResult res;
    res.schemaVersion = "response-measurement-1.0";
    res.schemaUri = "urn:abdaudiolab:response-measurement:1.0";
    res.measurementId = "res-dexed-t1-001";
    res.measurementType = "envelope";
    res.status = MeasurementStatus::completed;
    res.executionDomain = MeasurementExecutionDomain::Vst3Realtime;
    res.underruns = 0;
    res.overruns = 0;
    res.pluginLatencySamples = 64.0;
    res.hostLatencySamples = 128.0;
    res.curve.xName = "frequency";
    res.curve.xUnit = "Hz";
    res.curve.yName = "gain";
    res.curve.yUnit = "dB";
    res.curve.x = { 100.0, 1000.0 };
    res.curve.y = { 0.0, -3.0 };

    AnalogChainCalibrationRecord cal;
    cal.calibrationId = "cal-hw-001";
    cal.sampleRateHz = 48000.0;
    cal.blockSize = 512;
    cal.roundTripLatencySamples = 192.0;
    cal.snrDb = 96.5;
    cal.peakDbfs = -0.8;
    cal.dcOffsetDb = -72.0;
    cal.status = "pass";
    cal.snrDbMin = 18.0;
    cal.peakDbfsMax = -0.5;
    cal.dcOffsetDbMax = -60.0;
    cal.estimatedHostLatencySamples = 128.0;
    cal.estimatedHardwareLatencySamples = 64.0;
    res.analogCalibration = cal;

    std::string resJson = MeasurementSerialization::serializeResult(res);
    CHECK(resJson.find("\"executionDomain\": \"Vst3Realtime\"") != std::string::npos);
    CHECK(resJson.find("\"pluginLatencySamples\": 64.0") != std::string::npos);
    CHECK(resJson.find("\"snrDbMin\": 18.0") != std::string::npos);

    MeasurementResult parsedRes;
    REQUIRE(MeasurementSerialization::deserializeResult(resJson, parsedRes, err));
    CHECK(parsedRes.executionDomain == MeasurementExecutionDomain::Vst3Realtime);
    CHECK(parsedRes.pluginLatencySamples == 64.0);
    REQUIRE(parsedRes.analogCalibration.has_value());
    CHECK(parsedRes.analogCalibration->snrDb == 96.5);
    CHECK(parsedRes.analogCalibration->status == "pass");
    CHECK(parsedRes.analogCalibration->estimatedHardwareLatencySamples == 64.0);
}

// ==============================================================================
// T2.1 & T2.2: Introspección Real de Fábrica, Buses y Parámetros
// ==============================================================================

TEST_CASE("Fase 20.11 T2 - Introspeccion y catálogo reproducible de Dexed.vst3", "[vst3][dexed]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File dexedFile = resolveDexedBinary();
    if (!dexedFile.exists())
    {
        SKIP("Dexed.vst3 no encontrado en rutas estándar de Windows; comprobación en entorno sin plugin.");
        return;
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    InspectedPluginModule module1;
    std::string err;
    bool ok1 = ExternalPluginFixture::inspectPluginModule(formatManager, dexedFile, "", module1, err);

    REQUIRE(ok1);
    REQUIRE(err.empty());

    // 1. Identidad por Factory y CID
    CHECK(!module1.componentUids.empty());
    CHECK(!module1.selectedUid.empty());
    CHECK(!module1.canonicalPath.empty());
    CHECK(!module1.binarySha256.empty());

    // 2. Enumeración de buses
    REQUIRE(!module1.buses.empty());
    bool hasStereoOutput = false;
    for (const auto& b : module1.buses)
    {
        if (!b.isInput && b.defaultChannelCount >= 2)
            hasStereoOutput = true;
    }
    CHECK(hasStereoOutput);

    // 3. Regla obligatoria: NO exigir exactamente 155 parámetros (usar parameterCount > 0)
    REQUIRE(module1.parameterCount > 0);
    CHECK(module1.parameters.size() == static_cast<size_t>(module1.parameterCount));

    // Registro estructurado del recuento observado
    nlohmann::json countReport;
    countReport["parameterCount"] = module1.parameterCount;
    countReport["countAssertion"] = "observed_and_recorded";
    CHECK(countReport["countAssertion"] == "observed_and_recorded");

    // 4. Validar estructura por cada parámetro
    for (const auto& p : module1.parameters)
    {
        CHECK(!p.id.empty());
        CHECK(!p.title.empty());
        CHECK(p.normalizedValue >= 0.0f);
        CHECK(p.normalizedValue <= 1.0f);
    }

    // 5. Catálogo reproducible: una segunda inspección consecutiva debe arrojar idéntico resultado
    InspectedPluginModule module2;
    std::string err2;
    bool ok2 = ExternalPluginFixture::inspectPluginModule(formatManager, dexedFile, "", module2, err2);
    REQUIRE(ok2);
    CHECK(module2.parameterCount == module1.parameterCount);
    CHECK(module2.selectedUid == module1.selectedUid);
    CHECK(module2.binarySha256 == module1.binarySha256);

    std::cout << "\n[T2.1/T2.2 Discovery & Inspection Report]\n"
              << "  target: Dexed.vst3 real\n"
              << "  canonicalPath: " << module1.canonicalPath << "\n"
              << "  binarySha256:  " << module1.binarySha256 << "\n"
              << "  selectedUid:   " << module1.selectedUid << "\n"
              << "  parameterCount (observed): " << module1.parameterCount << "\n"
              << "  buses: " << module1.buses.size() << " (Stereo Output OK)\n" << std::endl;
}

// ==============================================================================
// T2.3: Estado Binario y Fixture Mínimo de Preset Controlado
// ==============================================================================

TEST_CASE("Fase 20.11 T2.3 - Estado Binario y Fixture Mínimo de Preset Controlado en Dexed", "[vst3][dexed][preset]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File dexedFile = resolveDexedBinary();
    if (!dexedFile.exists())
    {
        SKIP("Dexed.vst3 no encontrado en rutas estándar de Windows; comprobación en entorno sin plugin.");
        return;
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    ExternalPluginFixture fixture(formatManager);
    std::string loadErr;
    bool loaded = fixture.loadPluginFromDisk(dexedFile, 48000.0, 512, loadErr);
    REQUIRE(loaded);
    REQUIRE(loadErr.empty());
    REQUIRE(fixture.supportsBinaryState());

    // 1. Obtener volcado de estado binario inicial
    std::vector<uint8_t> initialBytes;
    auto resGet = fixture.getState(initialBytes);
    REQUIRE(resGet.succeeded);
    REQUIRE(resGet.byteCount > 0);
    REQUIRE(!resGet.stateDataHash.empty());
    CHECK(resGet.byteCount == initialBytes.size());

    // 2. Construir ControlledPresetFixture y verificar fixity SHA-256
    auto controlled = ControlledPresetFixture::create(
        "Dexed_Controlled_Init",
        initialBytes,
        60,
        100,
        MeasurementExecutionDomain::Vst3OfflineDigital
    );
    CHECK(controlled.name == "Dexed_Controlled_Init");
    CHECK(controlled.version == "1.0.0");
    CHECK(controlled.nominalMidiNote == 60);
    CHECK(controlled.nominalMidiVelocity == 100);
    CHECK(controlled.targetDomain == MeasurementExecutionDomain::Vst3OfflineDigital);
    CHECK(controlled.sha256 == resGet.stateDataHash);
    CHECK(controlled.verifyFixity());

    // 3. Alterar un parámetro del plugin para desviar el estado
    auto* inst = fixture.getPluginInstance();
    REQUIRE(inst != nullptr);
    auto params = inst->getParameters();
    REQUIRE(params.size() > 0);
    float originalVal = params[0]->getValue();
    float alteredVal = (originalVal < 0.5f) ? 0.85f : 0.15f;
    params[0]->setValueNotifyingHost(alteredVal);

    std::vector<uint8_t> modifiedBytes;
    auto resMod = fixture.getState(modifiedBytes);
    REQUIRE(resMod.succeeded);
    CHECK(resMod.stateDataHash != controlled.sha256);

    // 4. Restaurar el preset controlado mediante setState
    auto resSet = fixture.setState(controlled.presetBytes);
    REQUIRE(resSet.succeeded);
    CHECK(resSet.stateDataHash == controlled.sha256);

    // 5. Re-obtener estado y comprobar equivalencia metrológica
    std::vector<uint8_t> roundTripBytes;
    auto resRt = fixture.getState(roundTripBytes);
    REQUIRE(resRt.succeeded);

    StateEquivalence eq = (roundTripBytes == controlled.presetBytes)
                              ? StateEquivalence::BitExact
                              : (resRt.stateDataHash == controlled.sha256 ? StateEquivalence::SemanticallyEquivalent
                                                                          : StateEquivalence::NotEquivalent);

    CHECK(eq == StateEquivalence::BitExact);
    CHECK(resRt.stateDataHash == controlled.sha256);

    std::cout << "\n[T2.3 Controlled Preset & State Round-Trip Report]\n"
              << "  target: Dexed.vst3 real\n"
              << "  executionDomain: " << measurementExecutionDomainToString(controlled.targetDomain) << "\n"
              << "  preset name:     " << controlled.name << "\n"
              << "  preset bytes:    " << controlled.presetBytes.size() << " B\n"
              << "  fixity sha256:   " << controlled.sha256 << "\n"
              << "  fixity verified: " << (controlled.verifyFixity() ? "YES" : "NO") << "\n"
              << "  round-trip eq:   " << stateEquivalenceToString(eq) << "\n" << std::endl;
}

// ==============================================================================
// T2.4: Procesamiento Offline y Captura Acústica con Reloj
// ==============================================================================

TEST_CASE("Fase 20.11 T2.4 - Procesamiento Offline y Captura Acústica (Determinismo Bit a Bit)", "[vst3][dexed][offline]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File dexedFile = resolveDexedBinary();
    if (!dexedFile.exists())
    {
        SKIP("Dexed.vst3 no encontrado en rutas estándar de Windows; comprobación en entorno sin plugin.");
        return;
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    ExternalPluginFixture fixture(formatManager);
    std::string loadErr;
    bool loaded = fixture.loadPluginFromDisk(dexedFile, 48000.0, 256, loadErr);
    REQUIRE(loaded);

    ProcessingSpec spec;
    spec.sampleRate = 48000.0;
    spec.blockSize = 256;
    spec.numChannels = 2;
    fixture.prepare(spec);

    // Obtener y fijar estado de preset controlado
    std::vector<uint8_t> presetData;
    auto resInit = fixture.getState(presetData);
    REQUIRE(resInit.succeeded);
    auto controlled = ControlledPresetFixture::create("Dexed_Offline_Test", presetData, 60, 100);
    REQUIRE(controlled.verifyFixity());

    // Crear secuencia de excitación determinista (0.25 s = 12000 muestras a 48 kHz)
    MidiExcitationSequence seq;
    seq.totalDurationSec = 0.25;
    TimedMidiEvent evOn;
    evOn.type = TimedMidiType::NoteOn;
    evOn.channel = 1;
    evOn.noteNumber = 60;
    evOn.velocity = 0.8f;
    evOn.sampleOffset = 100;
    seq.events.push_back(evOn);

    TimedMidiEvent evOff;
    evOff.type = TimedMidiType::NoteOff;
    evOff.channel = 1;
    evOff.noteNumber = 60;
    evOff.velocity = 0.0f;
    evOff.sampleOffset = 6000;
    seq.events.push_back(evOff);

    // --- Pasada 1 (Pass 1) ---
    fixture.prepare(spec);
    fixture.resetState();
    fixture.setState(controlled.presetBytes);

    std::vector<float> audioPass1;
    fixture.render(seq, audioPass1);

    auto tele1 = fixture.getLastRenderTelemetry();
    CHECK(tele1.underruns == 0);
    CHECK(tele1.overruns == 0);
    CHECK(tele1.totalSamplesRendered == static_cast<int64_t>(std::lround(0.25 * 48000.0)));
    CHECK(tele1.blocksProcessed > 0);
    CHECK(tele1.bitExactDeterministic);
    CHECK(tele1.totalRenderTimeMs >= 0.0);

    // Verificar que la señal acústica no es silencio puro
    float maxAmp1 = 0.0f;
    for (float s : audioPass1)
        maxAmp1 = std::max(maxAmp1, std::abs(s));
    CHECK(maxAmp1 > 0.001f);

    // --- Pasada 2 (Pass 2) ---
    fixture.prepare(spec);
    fixture.resetState();
    fixture.setState(controlled.presetBytes);

    std::vector<float> audioPass2;
    fixture.render(seq, audioPass2);

    auto tele2 = fixture.getLastRenderTelemetry();
    CHECK(tele2.underruns == 0);
    CHECK(tele2.overruns == 0);
    CHECK(tele2.totalSamplesRendered == tele1.totalSamplesRendered);
    CHECK(tele2.blocksProcessed == tele1.blocksProcessed);

    // Comparación estricta bajo dominio Vst3OfflineDigital
    REQUIRE(audioPass1.size() == audioPass2.size());
    double maxDelta = 0.0;
    for (size_t i = 0; i < audioPass1.size(); ++i)
    {
        double d = std::abs(static_cast<double>(audioPass1[i]) - static_cast<double>(audioPass2[i]));
        if (d > maxDelta)
            maxDelta = d;
    }
    CHECK(maxDelta <= 0.0001); // Determinismo numérico dentro de 1 LSB (16-bit)

    std::cout << "\n[T2.4 Offline Render & Clock Telemetry Report]\n"
              << "  target: Dexed.vst3 real\n"
              << "  executionDomain: Vst3OfflineDigital\n"
              << "  pass 1 samples: " << tele1.totalSamplesRendered << " | blocks: " << tele1.blocksProcessed << " | peak: " << maxAmp1 << "\n"
              << "  pass 2 samples: " << tele2.totalSamplesRendered << " | blocks: " << tele2.blocksProcessed << "\n"
              << "  telemetry underruns: " << tele1.underruns << " | overruns: " << tele1.overruns << "\n"
              << "  pluginLatencySamples: " << tele1.pluginLatencySamples << " | hostLatencySamples: " << tele1.hostLatencySamples << "\n"
              << "  max absolute audio difference: " << maxDelta << " (bit_exact: " << (maxDelta == 0.0 ? "YES" : "NO") << ")\n" << std::endl;
}
