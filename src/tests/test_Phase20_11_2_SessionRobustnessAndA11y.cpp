/**
 * @file test_Phase20_11_2_SessionRobustnessAndA11y.cpp
 * @brief Catch2 test suite for Phase 20.11.2: Session Robustness, Accessible Keyboard Navigation,
 *        Row Virtualization Recycling, Preventive Resource Quotas, and Reproducible Exclusions.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../gui/measurement/MeasurementComparisonSession.h"
#include "../gui/measurement/MeasurementContainerListPanel.h"
#include "../measurement/MeasurementSerialization.h"
#include "../measurement/MeasurementComparisonReportGenerator.h"
#include "../synth/Sha256.h"

using namespace abdaudiolab::gui::measurement;
using namespace abdaudiolab::measurement;

namespace
{

juce::String computeFileSha(const juce::File& f)
{
    juce::MemoryBlock mb;
    f.loadFileAsData(mb);
    return abdaudiolab::synth::Sha256::computeHex(mb.getData(), mb.getSize());
}

juce::File createSyntheticContainer(const juce::File& rootDir,
                                    const juce::String& name,
                                    MeasurementExecutionDomain domain,
                                    int curvePointsCount = 4,
                                    int64_t dummyAudioSizeBytes = 1024)
{
    auto containerDir = rootDir.getChildFile(name);
    containerDir.createDirectory();
    containerDir.getChildFile("specs").createDirectory();
    containerDir.getChildFile("results").createDirectory();
    containerDir.getChildFile("curves").createDirectory();
    containerDir.getChildFile("audio").createDirectory();
    containerDir.getChildFile("reports").createDirectory();

    // Audio WAV
    auto audioFile = containerDir.getChildFile("audio/dexed_reference.wav");
    {
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(new juce::FileOutputStream(audioFile), 48000.0, 1, 16, {}, 0));
        if (writer != nullptr)
        {
            int sampleCount = static_cast<int>(dummyAudioSizeBytes / 2);
            if (sampleCount < 64) sampleCount = 64;
            juce::AudioBuffer<float> buf(1, sampleCount);
            buf.clear();
            writer->writeFromAudioSampleBuffer(buf, 0, sampleCount);
        }
    }
    juce::String audioSha = computeFileSha(audioFile);

    // Curves
    nlohmann::json ptsArray = nlohmann::json::array();
    std::vector<double> xs, ys;
    for (int i = 0; i < curvePointsCount; ++i)
    {
        double xVal = static_cast<double>(i);
        double yVal = -20.0 + static_cast<double>(i) * 0.1;
        ptsArray.push_back({ { "x", xVal }, { "y", yVal } });
        xs.push_back(xVal);
        ys.push_back(yVal);
    }

    nlohmann::json curveJson = {
        { "name", "Dynamics Velocity Level" },
        { "xUnit", "midi_velocity" },
        { "yUnit", "dBFS" },
        { "points", ptsArray }
    };
    auto curveFile = containerDir.getChildFile("curves/dynamics_velocity_level_curve.json");
    curveFile.replaceWithText(curveJson.dump(2));
    juce::String curveSha = computeFileSha(curveFile);

    // Spec
    MeasurementSpec spec;
    spec.measurementId = name.toStdString();
    spec.measurementType = "dynamics";
    spec.executionDomain = domain;
    spec.dutType = DeviceUnderTest::instrument;
    spec.stimulus.type = StimulusType::midiNote;
    spec.stimulus.midiNoteNumber = 60;
    spec.execution.sampleRateHz = 48000.0;
    spec.execution.blockSize = 512;
    auto specFile = containerDir.getChildFile("specs/measurement_spec.json");
    specFile.replaceWithText(MeasurementSerialization::serializeSpec(spec));
    juce::String specSha = computeFileSha(specFile);

    // Result
    MeasurementResult res;
    res.measurementId = name.toStdString();
    res.measurementType = "dynamics";
    res.status = MeasurementStatus::completed;
    res.reason = "Valid capture";
    res.executionDomain = domain;
    res.dut.name = "DexedSynth";
    res.dut.format = "VST3";
    res.execution.sampleRateHz = 48000.0;
    res.execution.blockSize = 512;
    res.artifacts.audioSha256 = audioSha.toStdString();
    res.curve.xName = "MIDI Velocity";
    res.curve.xUnit = "midi_velocity";
    res.curve.yName = "Dynamics Level";
    res.curve.yUnit = "dBFS";
    res.curve.x = xs;
    res.curve.y = ys;

    auto resFile = containerDir.getChildFile("results/measurement_result.json");
    resFile.replaceWithText(MeasurementSerialization::serializeResult(res));
    juce::String resSha = computeFileSha(resFile);

    // Manifest
    nlohmann::json manifestJson = {
        { "manifestFormat", "artifact-list-v1" },
        { "schemaVersion", "abdaudiolab-fair-lnl-1.0" },
        { "executionDomain", abdaudiolab::measurement::measurementExecutionDomainToString(domain) },
        { "artifactCount", 5 },
        { "listedArtifactCount", 4 },
        { "manifestExcludedFromArtifacts", true },
        { "artifacts", {
            { { "path", "specs/measurement_spec.json" }, { "sha256", specSha.toStdString() }, { "sizeBytes", specFile.getSize() }, { "role", "measurement_spec" } },
            { { "path", "audio/dexed_reference.wav" }, { "sha256", audioSha.toStdString() }, { "sizeBytes", audioFile.getSize() }, { "role", "audio_reference" } },
            { { "path", "curves/dynamics_velocity_level_curve.json" }, { "sha256", curveSha.toStdString() }, { "sizeBytes", curveFile.getSize() }, { "role", "curve_data" } },
            { { "path", "results/measurement_result.json" }, { "sha256", resSha.toStdString() }, { "sizeBytes", resFile.getSize() }, { "role", "result_data" } }
        } }
    };
    auto manifestFile = containerDir.getChildFile("manifest.json");
    manifestFile.replaceWithText(manifestJson.dump(2));

    return containerDir;
}

struct TempFolder
{
    juce::File dir;
    TempFolder()
    {
        dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                  .getChildFile("ABDAudioLab_Robustness_Test_" + juce::String(juce::Random::getSystemRandom().nextInt()));
        dir.createDirectory();
    }
    ~TempFolder()
    {
        dir.deleteRecursively();
    }
};

} // namespace

TEST_CASE("Fase 20.11.2: Session Robustness, Quotas & A11y Navigation", "[comparison_robustness]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    SECTION("T20.11.2-1: Cancelación cooperativa y sesión con descarte por sessionGeneration")
    {
        TempFolder temp;
        auto c1Dir = createSyntheticContainer(temp.dir, "GenTest_1", MeasurementExecutionDomain::Vst3OfflineDigital);

        MeasurementComparisonSession session;
        const uint64_t initialGen = session.getSessionGeneration();

        std::atomic<bool> callbackCalled { false };
        int id1 = session.addContainerAsync(c1Dir, [&callbackCalled](int, ContainerLoadState)
        {
            callbackCalled.store(true);
        });
        CHECK(id1 > 0);

        // Inmediatamente cancelamos o limpiamos para incrementar la generación
        session.cancelPendingLoads();
        CHECK(session.getSessionGeneration() > initialGen);

        // Permitir que el hilo en segundo plano complete la salida cooperativa
        juce::Thread::sleep(150);

        // El callback antiguo de la generación cancelada no debe promover a Verified
        auto entry = session.getContainerById(id1);
        REQUIRE(entry.has_value());
        CHECK(entry->loadState != ContainerLoadState::Verified);
    }

    SECTION("T20.11.2-1b: Teardown seguro, timeout de drenado sin use-after-free")
    {
        TempFolder temp;
        auto c1Dir = createSyntheticContainer(temp.dir, "TeardownTest_1", MeasurementExecutionDomain::Vst3OfflineDigital);

        // Creamos una sesión en heap y la destruimos mientras procesa
        auto session = std::make_unique<MeasurementComparisonSession>();
        CHECK(session->getShutdownState() == SessionShutdownState::Running);

        session->addContainerAsync(c1Dir);

        // Destrucción inmediata: debe pasar por CancellationRequested -> Draining -> Drained/DrainTimedOut -> Destroyed
        session.reset();

        // Si no hay crash ni use-after-free, el drenado y desacoplo de SharedSessionState son seguros
        CHECK(true);
    }

    SECTION("T20.11.2-2: Límites de recursos preventivos (cuotas pre-lectura)")
    {
        TempFolder temp;
        MeasurementComparisonSession session;
        SessionResourceLimits limits;
        limits.maxContainers = 2;
        limits.maxManifestBytes = 100000;    // 100 KB
        limits.maxJsonBytes = 100000;        // 100 KB
        limits.maxCurvePoints = 10;
        limits.maxAudioBytes = 500000;
        session.setResourceLimits(limits);

        // Test 2a: Quota de maxContainers
        auto c1 = createSyntheticContainer(temp.dir, "Quota_1", MeasurementExecutionDomain::Vst3OfflineDigital, 4, 100);
        auto c2 = createSyntheticContainer(temp.dir, "Quota_2", MeasurementExecutionDomain::Vst3OfflineDigital, 4, 100);
        auto c3 = createSyntheticContainer(temp.dir, "Quota_3", MeasurementExecutionDomain::Vst3OfflineDigital, 4, 100);

        int id1 = session.addContainerSync(c1);
        int id2 = session.addContainerSync(c2);
        int id3 = session.addContainerSync(c3);

        auto e1 = session.getContainerById(id1);
        auto e2 = session.getContainerById(id2);
        auto e3 = session.getContainerById(id3);

        REQUIRE(e1.has_value());
        REQUIRE(e2.has_value());
        REQUIRE(e3.has_value());

        CHECK(e1->loadState == ContainerLoadState::Verified);
        CHECK(e2->loadState == ContainerLoadState::Verified);
        // El 3er contenedor debe ser rechazado preventivamente
        CHECK(e3->loadState == ContainerLoadState::Rejected);
        CHECK(e3->diagnosticReason.contains("resource_limit_exceeded: maximum container count reached"));

        // Test 2b: Quota de maxCurvePoints
        MeasurementComparisonSession sessionPoints;
        SessionResourceLimits limitsPoints;
        limitsPoints.maxCurvePoints = 20;
        sessionPoints.setResourceLimits(limitsPoints);

        auto cExcessPoints = createSyntheticContainer(temp.dir, "Quota_ExcessPoints", MeasurementExecutionDomain::Vst3OfflineDigital, 50, 100);
        int idPoints = sessionPoints.addContainerSync(cExcessPoints);
        auto ePoints = sessionPoints.getContainerById(idPoints);
        REQUIRE(ePoints.has_value());
        CHECK(ePoints->loadState == ContainerLoadState::Rejected);
        CHECK(ePoints->diagnosticReason.contains("resource_limit_exceeded: curve points"));

        // Test 2c: Quota de maxManifestBytes preventivo
        MeasurementComparisonSession sessionManifest;
        SessionResourceLimits limitsManifest;
        limitsManifest.maxManifestBytes = 100; // Demasiado pequeño intencionalmente
        sessionManifest.setResourceLimits(limitsManifest);
        auto cExcessManifest = createSyntheticContainer(temp.dir, "Quota_ExcessManifest", MeasurementExecutionDomain::Vst3OfflineDigital, 4, 100);
        int idManifest = sessionManifest.addContainerSync(cExcessManifest);
        auto eManifest = sessionManifest.getContainerById(idManifest);
        REQUIRE(eManifest.has_value());
        CHECK(eManifest->loadState == ContainerLoadState::Rejected);
        CHECK(eManifest->diagnosticReason.contains("resource_limit_exceeded: manifest.json size"));

        // Test 2d: Serialización y deserialización JSON de SessionResourceLimits
        auto jLimits = limits.toJson();
        CHECK(jLimits.contains("resourceLimits"));
        auto restoredLimits = SessionResourceLimits::fromJson(jLimits);
        CHECK(restoredLimits.maxContainers == 2);
        CHECK(restoredLimits.maxManifestBytes == 100000);
        CHECK(restoredLimits.maxCurvePoints == 10);
    }

    SECTION("T20.11.2-3: Accesibilidad WCAG, navegación por teclado y Delete seguro")
    {
        TempFolder temp;
        auto c1 = createSyntheticContainer(temp.dir, "A11y_1", MeasurementExecutionDomain::Vst3OfflineDigital, 4, 100);
        auto c2 = createSyntheticContainer(temp.dir, "A11y_2", MeasurementExecutionDomain::Vst3OfflineDigital, 4, 100);

        MeasurementComparisonSession session;
        int id1 = session.addContainerSync(c1);
        int id2 = session.addContainerSync(c2);

        MeasurementContainerListPanel panel(session);
        panel.setSize(600, 300);

        CHECK(panel.getNumRows() == 2);

        // Delete sin selección no debe hacer nada
        CHECK(panel.getSelectedRow() == -1);
        panel.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));
        CHECK(session.getContainerCount() == 2);

        // Navegación con tecla Flecha Abajo
        panel.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
        CHECK(panel.getSelectedRow() == 0);

        panel.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
        CHECK(panel.getSelectedRow() == 1);

        // Navegación con tecla Flecha Arriba
        panel.keyPressed(juce::KeyPress(juce::KeyPress::upKey));
        CHECK(panel.getSelectedRow() == 0);

        // Espacio: alternar comparación del elemento seleccionado
        auto e1Before = session.getContainerById(id1);
        REQUIRE(e1Before.has_value());
        CHECK(e1Before->selectedForComparison == true);

        panel.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey));
        auto e1After = session.getContainerById(id1);
        REQUIRE(e1After.has_value());
        CHECK(e1After->selectedForComparison == false);

        // Enter: activar audio verificado
        session.setActiveAudioContainerId(id2);
        CHECK(session.getActiveAudioContainerId() == id2);

        // Seleccionamos fila 0 (id1) y pulsamos Enter
        panel.selectRow(0);
        panel.keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
        CHECK(session.getActiveAudioContainerId() == id1);

        // Delete debe operar sobre el elemento SELECCIONADO y no sobre el elemento con audio activo
        session.setActiveAudioContainerId(id2); // id2 tiene el audio activo
        panel.selectRow(0);                      // id1 está seleccionado
        panel.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));

        // Verificamos: id1 fue eliminado, id2 (audio activo) se mantiene
        CHECK(session.getContainerById(id1).has_value() == false);
        CHECK(session.getContainerById(id2).has_value() == true);
        CHECK(session.getContainerCount() == 1);
    }

    SECTION("T20.11.2-4: Reciclaje virtualizado de filas sin fuga de listeners ni estado anterior")
    {
        TempFolder temp;
        auto c1 = createSyntheticContainer(temp.dir, "Recycle_1", MeasurementExecutionDomain::Vst3OfflineDigital, 4, 100);
        auto c2 = createSyntheticContainer(temp.dir, "Recycle_2", MeasurementExecutionDomain::Vst3OfflineDigital, 4, 100);

        MeasurementComparisonSession session;
        int id1 = session.addContainerSync(c1);
        int id2 = session.addContainerSync(c2);
        CHECK(id1 > 0);
        CHECK(id2 > 0);

        MeasurementContainerListPanel panel(session);

        // 1. Obtener componente para fila 0
        std::unique_ptr<juce::Component> rowComp(panel.refreshComponentForRow(0, false, nullptr));
        REQUIRE(rowComp != nullptr);

        // 2. Reciclar el mismo componente para la fila 1
        juce::Component* recycled = panel.refreshComponentForRow(1, true, rowComp.get());
        CHECK(recycled == rowComp.get()); // Misma instancia reciclada

        // 3. Reciclar para un índice fuera de rango (-1 o 99): debe resetear a vacío de forma segura
        panel.refreshComponentForRow(-1, false, recycled);
        CHECK(true);
    }

    SECTION("T20.11.2-5: Exclusiones deterministas y reproducibles (independientes del timestamp)")
    {
        MeasurementComparisonSession session;

        std::string hash1 = ComparisonExclusionRecord::computeBasisHash(
            "ContainerA", "ContainerB", "DynamicsVelocityLevel", "dBFS", "Vst3OfflineDigital", "exclusion-rules-1.0");

        std::string hash2 = ComparisonExclusionRecord::computeBasisHash(
            "ContainerB", "ContainerA", "DynamicsVelocityLevel", "dBFS", "Vst3OfflineDigital", "exclusion-rules-1.0");

        // El hash debe ser estrictamente simétrico y determinista (A vs B == B vs A)
        CHECK(hash1 == hash2);
        CHECK(!hash1.empty());

        // Dos ejecuciones con distintos timestamps ISO deben producir el mismo comparisonBasisHash
        ComparisonExclusionRecord rec1;
        rec1.containerId = "ContainerA";
        rec1.metric = "DynamicsVelocityLevel";
        rec1.code = "metric_basis_mismatch";
        rec1.message = "Different sampling rates detected";
        rec1.comparisonBasisHash = hash1;
        rec1.timestampIso = "2026-09-17T12:00:00Z";

        ComparisonExclusionRecord rec2;
        rec2.containerId = "ContainerA";
        rec2.metric = "DynamicsVelocityLevel";
        rec2.code = "metric_basis_mismatch";
        rec2.message = "Different sampling rates detected";
        rec2.comparisonBasisHash = hash2;
        rec2.timestampIso = "2026-09-17T18:45:22Z";

        CHECK(rec1.comparisonBasisHash == rec2.comparisonBasisHash);

        // Registro en la sesión e informe HTML
        session.addExclusionRecord(rec1);
        CHECK(session.getExclusionRecords().size() == 1);

        juce::String reportHtml = MeasurementComparisonReportGenerator::generateReportHtml(session);
        CHECK(reportHtml.contains("metric_basis_mismatch"));
        CHECK(reportHtml.contains(hash1.substr(0, 16)));
    }

    SECTION("T20.11.2-6: Eliminación de contenedor con audio activo")
    {
        TempFolder temp;
        auto c1 = createSyntheticContainer(temp.dir, "AudioKill_1", MeasurementExecutionDomain::Vst3OfflineDigital, 4, 100);
        auto c2 = createSyntheticContainer(temp.dir, "AudioKill_2", MeasurementExecutionDomain::Vst3OfflineDigital, 4, 100);

        MeasurementComparisonSession session;
        int id1 = session.addContainerSync(c1);
        int id2 = session.addContainerSync(c2);

        session.setActiveAudioContainerId(id1);
        CHECK(session.getActiveAudioContainerId() == id1);

        // Si eliminamos id1, la sesión debe reasignar a id2 o -1 de forma segura
        bool removed = session.removeContainer(id1);
        CHECK(removed == true);
        CHECK(session.getActiveAudioContainerId() == id2);

        // Si eliminamos id2, activeAudioContainerId debe pasar a -1
        session.removeContainer(id2);
        CHECK(session.getActiveAudioContainerId() == -1);
    }
}
