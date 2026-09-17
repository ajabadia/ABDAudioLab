/**
 * @file test_Phase20_11_VerticalDexedAndFair_T5.cpp
 * @brief Suite de pruebas unitarias y E2E para Fase 20.11 T5:
 *        Prueba Vertical de Integración y Exportación FAIR/LNL de Dexed.vst3.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../measurement/DexedVerticalCampaign.h"
#include "../measurement/MeasurementContracts.h"
#include "../gui/measurement/MeasurementViewModelLoader.h"
#include "../synth/ExternalPluginFixture.h"
#include "../synth/Sha256.h"

#include <nlohmann/json.hpp>
#include <vector>
#include <fstream>

using namespace abdaudiolab::measurement;
using namespace abdaudiolab::gui::measurement;
using namespace abdaudiolab::synth;

namespace
{

DexedVerticalFixture createStandardDexedFixture(ExternalPluginFixture& plug)
{
    DexedVerticalFixture fix;
    fix.presetName = "Dexed_Controlled_Init";
    
    std::vector<uint8_t> stateBytes;
    auto res = plug.getState(stateBytes);
    REQUIRE(res.succeeded);
    REQUIRE(!stateBytes.empty());

    fix.presetBytes = stateBytes;
    fix.stateSha256 = abdaudiolab::synth::Sha256::computeHex(stateBytes.data(), stateBytes.size());
    fix.componentUid = "VST3-Dexed-3f015740-d7709eec";
    fix.midiNote = 60;
    fix.velocityGrid = { 32, 64, 96, 127 };
    fix.executionDomain = MeasurementExecutionDomain::Vst3OfflineDigital;
    fix.sampleRateHz = 48000.0;
    fix.blockSize = 512;
    fix.noteDurationSec = 0.2;
    fix.releaseDurationSec = 0.1;

    return fix;
}

} // namespace

TEST_CASE("Fase 20.11 T5: Campaña Vertical Dexed Real y Exportacion FAIR/LNL", "[vertical_dexed][fair][t5]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File dexedFile = DexedVerticalCoordinator::resolveDexedBinary();
    if (!dexedFile.exists())
    {
        SKIP("Dexed.vst3 no encontrado en rutas estándar; comprobación en entorno sin plugin.");
        return;
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    // 1. Escenario 1: Carga del fixture Dexed real e introspección
    ExternalPluginFixture initPlug(formatManager);
    std::string err;
    bool loaded = initPlug.loadPluginFromDisk(dexedFile, 48000.0, 512, err);
    REQUIRE(loaded);
    REQUIRE(err.empty());
    REQUIRE(initPlug.supportsBinaryState());

    DexedVerticalFixture fixture = createStandardDexedFixture(initPlug);

    // 2. Escenario 2: Verificación de hash del plugin y preset
    CHECK(fixture.verifyFixity());
    CHECK(!fixture.stateSha256.empty());
    CHECK(fixture.componentUid == "VST3-Dexed-3f015740-d7709eec");
    CHECK(fixture.executionDomain == MeasurementExecutionDomain::Vst3OfflineDigital);

    // 3. Ejecución de la Campaña Vertical (Escenarios 3, 4, 5 y 6)
    DexedCampaignResults campaignResults;
    bool campOk = DexedVerticalCoordinator::executeCampaign(
        formatManager, dexedFile, fixture, campaignResults, err);

    REQUIRE(campOk);
    REQUIRE(err.empty());

    // 4. Escenario 3 y 4: Secuencia MIDI y Captura no vacía
    CHECK(!campaignResults.referenceAudioSamples.empty());
    CHECK(campaignResults.referenceAudioSamples.size() >= static_cast<size_t>(0.3 * 48000.0));
    CHECK(campaignResults.dynamicPoints.size() == 4);

    // 5. Escenario 5: Dinámica calculada (curvas de nivel y timbre)
    CHECK(campaignResults.dynamicResult.points.size() == 4);
    for (size_t i = 0; i < campaignResults.dynamicPoints.size(); ++i)
    {
        CHECK(campaignResults.dynamicPoints[i].velocity == fixture.velocityGrid[i]);
        CHECK(campaignResults.dynamicPoints[i].rmsDbfs < 0.0);
        CHECK(campaignResults.dynamicPoints[i].spectralCentroidHz > 0.0);
    }
    // Monotonía de amplitud con velocidad creciente
    CHECK(campaignResults.dynamicPoints[0].peakDbfs <= campaignResults.dynamicPoints[3].peakDbfs);

    // 6. Escenario 6: Modulación calculada o declarada not_observable
    CHECK(campaignResults.modulationResult.waveform.status == "not_observable");
    CHECK(campaignResults.measurementResult.reason == "observed_without_modulation");
    CHECK(campaignResults.measurementResult.status == MeasurementStatus::completed);

    // 7. Escenario 7: Exportación FAIR/LNL con estructura de 12 artefactos
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("ABDAudioLab_T5_Vertical_" + juce::String::toHexString(juce::Random::getSystemRandom().nextInt()));

    if (tempDir.exists())
        tempDir.deleteRecursively();

    bool exportOk = DexedVerticalContainerExporter::exportContainer(tempDir, campaignResults, err);
    REQUIRE(exportOk);
    REQUIRE(err.empty());

    // Comprobar la existencia física de los 12 artefactos en disco
    juce::File fSpec = tempDir.getChildFile("specs/measurement_spec.json");
    juce::File fStim = tempDir.getChildFile("specs/measurement_stimulus.json");
    juce::File fIdent = tempDir.getChildFile("specs/plugin_identity.json");
    juce::File fResult = tempDir.getChildFile("results/measurement_result.json");
    juce::File fTele = tempDir.getChildFile("results/capture_telemetry.json");
    juce::File fDynLevel = tempDir.getChildFile("curves/dynamics_velocity_level_curve.json");
    juce::File fDynTimbre = tempDir.getChildFile("curves/dynamics_velocity_timbre_curve.json");
    juce::File fModTime = tempDir.getChildFile("curves/modulation_time_curve.json");
    juce::File fModSpec = tempDir.getChildFile("curves/modulation_spectrum_curve.json");
    juce::File fAudio = tempDir.getChildFile("audio/dexed_reference.wav");
    juce::File fReport = tempDir.getChildFile("reports/measurement_report.html");
    juce::File fManifest = tempDir.getChildFile("manifest.json");

    CHECK(fSpec.existsAsFile());
    CHECK(fStim.existsAsFile());
    CHECK(fIdent.existsAsFile());
    CHECK(fResult.existsAsFile());
    CHECK(fTele.existsAsFile());
    CHECK(fDynLevel.existsAsFile());
    CHECK(fDynTimbre.existsAsFile());
    CHECK(fModTime.existsAsFile());
    CHECK(fModSpec.existsAsFile());
    CHECK(fAudio.existsAsFile());
    CHECK(fReport.existsAsFile());
    CHECK(fManifest.existsAsFile());

    // 8. Escenario 8: Hashes del manifiesto válidos (sin incluirse a sí mismo)
    std::string manifestContent = fManifest.loadFileAsString().toStdString();
    auto jManifest = nlohmann::ordered_json::parse(manifestContent);

    CHECK(jManifest["manifestFormat"] == "artifact-list-v1");
    CHECK(jManifest["schemaVersion"] == "abdaudiolab-fair-lnl-1.0");
    CHECK(jManifest["executionDomain"] == "Vst3OfflineDigital");
    CHECK(jManifest["artifactCount"] == 12);
    REQUIRE(jManifest.contains("artifacts"));
    CHECK(jManifest["artifacts"].size() == 11); // 11 artefactos contenidos

    for (const auto& a : jManifest["artifacts"])
    {
        std::string relPath = a["path"];
        std::string expectedSha = a["sha256"];
        uint64_t expectedSize = a["sizeBytes"];

        juce::File artFile = tempDir.getChildFile(relPath);
        REQUIRE(artFile.existsAsFile());
        CHECK(artFile.getSize() == static_cast<int64_t>(expectedSize));

        juce::MemoryBlock mb;
        REQUIRE(artFile.loadFileAsData(mb));
        std::string actualSha = abdaudiolab::synth::Sha256::computeHex(mb.getData(), mb.getSize());
        CHECK(actualSha == expectedSha);
    }

    // 9. Escenario 9: Round-trip en MeasurementViewModelLoader (Carga e integridad UI)
    MeasurementViewModel viewModel;
    juce::String loadErr;
    bool uiLoaded = MeasurementViewModelLoader::loadFromContainer(tempDir, viewModel, loadErr);
    REQUIRE(uiLoaded);
    CHECK(loadErr.isEmpty());
    CHECK(viewModel.integrityStatus == UiIntegrityStatus::Verified);
    CHECK(viewModel.statusText == "COMPLETED");
    CHECK(viewModel.audioFile.existsAsFile());
    CHECK(viewModel.curveFile.existsAsFile());
    CHECK(viewModel.secondaryCurveFile.existsAsFile());
    CHECK(viewModel.htmlReportFile.existsAsFile());

    // 10. Escenario 10: Detección de Tampering (falsificación criptográfica detectada)
    // 10.1 Alteración de 1 byte en el archivo de audio
    juce::FileOutputStream audioAppend(fAudio);
    audioAppend.writeByte(0x7F);
    audioAppend.flush();

    juce::String corruptDiagnostic;
    bool integrityWithCorruptAudio = MeasurementViewModelLoader::verifyContainerIntegrity(tempDir, corruptDiagnostic);
    CHECK_FALSE(integrityWithCorruptAudio);
    CHECK(corruptDiagnostic.contains("Cryptographic mismatch"));

    // Recargar con el archivo alterado: debe marcar UiIntegrityStatus::Corrupt
    MeasurementViewModel corruptModel;
    juce::String corruptLoadErr;
    bool corruptLoaded = MeasurementViewModelLoader::loadFromContainer(tempDir, corruptModel, corruptLoadErr);
    CHECK(corruptLoaded);
    CHECK(corruptModel.integrityStatus == UiIntegrityStatus::Corrupt);
    CHECK(corruptModel.statusText == "CORRUPT");

    // Limpieza
    tempDir.deleteRecursively();
}
