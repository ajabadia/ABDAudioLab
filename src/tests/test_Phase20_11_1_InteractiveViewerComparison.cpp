/**
 * @file test_Phase20_11_1_InteractiveViewerComparison.cpp
 * @brief Test suite for Phase 20.11.1: Multi-Container FAIR/LNL Interactive Comparison & Session.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../gui/measurement/MeasurementComparisonSession.h"
#include "../gui/measurement/MeasurementContainerListPanel.h"
#include "../gui/measurement/MeasurementDynamicsComparisonComponent.h"
#include "../gui/measurement/MeasurementStateEquivalenceCard.h"
#include "../gui/measurement/MeasurementComparisonPanel.h"
#include "../measurement/MeasurementSerialization.h"
#include "../measurement/MeasurementComparisonReportGenerator.h"
#include "../measurement/DexedVerticalCampaign.h"
#include "../synth/Sha256.h"

using namespace abdaudiolab::gui::measurement;
using namespace abdaudiolab::measurement;

TEST_CASE("Fase 20.11.1: Multi-Container Comparison Session & Integrity", "[comparison_viewer]")
{
    // Initialize minimal JUCE GUI message thread if needed
    juce::ScopedJuceInitialiser_GUI guiInit;

    auto computeFileSha = [](const juce::File& f) -> juce::String
    {
        juce::MemoryBlock mb;
        f.loadFileAsData(mb);
        return abdaudiolab::synth::Sha256::computeHex(mb.getData(), mb.getSize());
    };

    // Helper lambda to generate a synthetic verified container
    auto createSyntheticContainer = [&computeFileSha](const juce::File& rootDir,
                                                      const juce::String& name,
                                                      MeasurementExecutionDomain domain,
                                                      double yOffset = 0.0) -> juce::File
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
                juce::AudioBuffer<float> buf(1, 480);
                buf.clear();
                writer->writeFromAudioSampleBuffer(buf, 0, 480);
            }
        }
        juce::String audioSha = computeFileSha(audioFile);

        // Curves
        nlohmann::json curveJson = {
            { "name", "Dynamics Velocity Level" },
            { "xUnit", "midi_velocity" },
            { "yUnit", "dBFS" },
            { "points", {
                { { "x", 32 },  { "y", -30.0 + yOffset } },
                { { "x", 64 },  { "y", -18.0 + yOffset } },
                { { "x", 96 },  { "y", -8.0 + yOffset } },
                { { "x", 127 }, { "y", -0.5 + yOffset } }
            } }
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
        res.dut.name = "Dexed";
        res.dut.format = "VST3";
        res.execution.sampleRateHz = 48000.0;
        res.execution.blockSize = 512;
        res.artifacts.audioSha256 = audioSha.toStdString();
        res.curve.xName = "MIDI Velocity";
        res.curve.xUnit = "midi_0_127";
        res.curve.yName = "Dynamics Level";
        res.curve.yUnit = "dBFS";
        res.curve.x = { 32.0, 64.0, 96.0, 127.0 };
        res.curve.y = { -30.0 + yOffset, -18.0 + yOffset, -8.0 + yOffset, -0.5 + yOffset };

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
    };

    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("ABDAudioLab_Comparison_Test_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempDir.createDirectory();

    SECTION("1. Sesión vacía opera sin errores y con cero excepciones")
    {
        MeasurementComparisonSession session;
        CHECK(session.getContainerCount() == 0);
        CHECK(session.getActiveAudioContainerId() == -1);
        CHECK(session.getFilteredContainers().empty());
        CHECK(session.getEligibleComparisonContainers().empty());
        CHECK(session.removeContainer(999) == false);
    }

    SECTION("2. Carga síncrona y verificación de contenedor válido")
    {
        auto c1Dir = createSyntheticContainer(tempDir, "Cont_A", MeasurementExecutionDomain::Vst3OfflineDigital, 0.0);

        MeasurementComparisonSession session;
        int id1 = session.addContainerSync(c1Dir);
        CHECK(id1 > 0);
        CHECK(session.getContainerCount() == 1);

        auto entry = session.getContainerById(id1);
        REQUIRE(entry.has_value());
        CHECK(entry->loadState == ContainerLoadState::Verified);
        CHECK(entry->isEligibleForComparison() == true);
        CHECK(entry->isPlayable() == true);
        CHECK(session.getActiveAudioContainerId() == id1);
    }

    SECTION("3. Detección de corrupción y bloqueo estricto de audio y curvas")
    {
        auto cCorruptDir = createSyntheticContainer(tempDir, "Cont_Corrupt", MeasurementExecutionDomain::Vst3OfflineDigital, 0.0);
        // Tamper audio file by appending 1 byte
        auto audioFile = cCorruptDir.getChildFile("audio/dexed_reference.wav");
        audioFile.appendText("X");

        MeasurementComparisonSession session;
        int idCorrupt = session.addContainerSync(cCorruptDir);
        CHECK(idCorrupt > 0);

        auto entry = session.getContainerById(idCorrupt);
        REQUIRE(entry.has_value());
        CHECK(entry->loadState == ContainerLoadState::Corrupt);
        CHECK(entry->isEligibleForComparison() == false);
        CHECK(entry->isPlayable() == false);
        CHECK(entry->diagnosticReason.contains("Cryptographic mismatch"));

        // Corrupt container CANNOT be active audio source
        CHECK(session.setActiveAudioContainerId(idCorrupt) == false);
        CHECK(session.getActiveAudioContainerId() == -1);

        // Corrupt container must not appear in eligible list
        auto eligible = session.getEligibleComparisonContainers();
        CHECK(eligible.empty());
    }

    SECTION("4. Filtrado por dominio metrológico")
    {
        auto cOffline = createSyntheticContainer(tempDir, "Cont_Offline", MeasurementExecutionDomain::Vst3OfflineDigital);
        auto cRealtime = createSyntheticContainer(tempDir, "Cont_Realtime", MeasurementExecutionDomain::Vst3Realtime);

        MeasurementComparisonSession session;
        int idOff = session.addContainerSync(cOffline);
        int idRt = session.addContainerSync(cRealtime);

        CHECK(session.getContainerCount() == 2);
        CHECK(session.getFilteredContainers().size() == 2);

        // Filter: only Offline
        session.setDomainFilter(MeasurementExecutionDomain::Vst3OfflineDigital);
        auto filteredOff = session.getFilteredContainers();
        REQUIRE(filteredOff.size() == 1);
        CHECK(filteredOff.front().id == idOff);

        // Filter: only Realtime
        session.setDomainFilter(MeasurementExecutionDomain::Vst3Realtime);
        auto filteredRt = session.getFilteredContainers();
        REQUIRE(filteredRt.size() == 1);
        CHECK(filteredRt.front().id == idRt);

        // Reset filter
        session.setDomainFilter(std::nullopt);
        CHECK(session.getFilteredContainers().size() == 2);
    }

    SECTION("5. Validación de compatibilidad antes de superponer curvas")
    {
        MeasurementViewModel vmLevel;
        vmLevel.curve.yUnit = "dBFS";
        vmLevel.curve.yName = "Dynamics Level";
        vmLevel.sampleRateHz = 48000.0;
        vmLevel.executionDomain = MeasurementExecutionDomain::Vst3OfflineDigital;

        MeasurementViewModel vmRms;
        vmRms.curve.yUnit = "dBFS";
        vmRms.curve.yName = "Dynamics RMS Power";
        vmRms.sampleRateHz = 48000.0;
        vmRms.executionDomain = MeasurementExecutionDomain::Vst3OfflineDigital;

        MeasurementViewModel vmCentroid;
        vmCentroid.curve.yUnit = "Hz";
        vmCentroid.curve.yName = "Spectral Centroid";
        vmCentroid.sampleRateHz = 48000.0;
        vmCentroid.executionDomain = MeasurementExecutionDomain::Vst3OfflineDigital;

        MeasurementViewModel vmAnalog;
        vmAnalog.curve.yUnit = "dBFS";
        vmAnalog.curve.yName = "Dynamics Level";
        vmAnalog.sampleRateHz = 48000.0;
        vmAnalog.executionDomain = MeasurementExecutionDomain::CombinedDutAndChain;

        juce::String reason;

        // Level vs RMS -> Incompatible
        CHECK(MeasurementComparisonSession::areMeasurementBasesCompatible(vmLevel, vmRms, reason) == false);
        CHECK(reason.contains("Cannot overlay Peak level with RMS power"));

        // Level vs Centroid -> Incompatible (dBFS vs Hz)
        CHECK(MeasurementComparisonSession::areMeasurementBasesCompatible(vmLevel, vmCentroid, reason) == false);
        CHECK(reason.contains("Incompatible Y-axis units"));

        // Offline vs Uncompensated Analog -> Incompatible
        CHECK(MeasurementComparisonSession::areMeasurementBasesCompatible(vmLevel, vmAnalog, reason) == false);
        CHECK(reason.contains("Cannot overlay pure Vst3OfflineDigital with uncompensated"));

        // Level vs Identical Level -> Compatible
        CHECK(MeasurementComparisonSession::areMeasurementBasesCompatible(vmLevel, vmLevel, reason) == true);
    }

    SECTION("6. Comparación de equivalencia de estado por pares (A <-> B)")
    {
        auto cA = createSyntheticContainer(tempDir, "Cont_PairA", MeasurementExecutionDomain::Vst3OfflineDigital, 0.0);
        auto cB = createSyntheticContainer(tempDir, "Cont_PairB", MeasurementExecutionDomain::Vst3OfflineDigital, 0.0); // Identical
        auto cC = createSyntheticContainer(tempDir, "Cont_PairC", MeasurementExecutionDomain::Vst3OfflineDigital, 5.0); // Divergent

        MeasurementComparisonSession session;
        int idA = session.addContainerSync(cA);
        int idB = session.addContainerSync(cB);
        int idC = session.addContainerSync(cC);

        // Pair A <-> B (BitExact curve match)
        auto resAB = session.compareContainers(idA, idB);
        CHECK(resAB.equivalence == PairwiseStateEquivalence::BitExact);
        CHECK(resAB.maxAudioDelta == 0.0);

        // Pair A <-> C (Divergent curve match)
        auto resAC = session.compareContainers(idA, idC);
        CHECK(resAC.equivalence == PairwiseStateEquivalence::NotEquivalent);
        CHECK(resAC.maxAudioDelta == 5.0);
    }

    SECTION("7. Eliminación de contenedor activo conmuta limpiamente a otro elegible")
    {
        auto c1 = createSyntheticContainer(tempDir, "Cont_Del1", MeasurementExecutionDomain::Vst3OfflineDigital);
        auto c2 = createSyntheticContainer(tempDir, "Cont_Del2", MeasurementExecutionDomain::Vst3OfflineDigital);

        MeasurementComparisonSession session;
        int id1 = session.addContainerSync(c1);
        int id2 = session.addContainerSync(c2);

        CHECK(session.getActiveAudioContainerId() == id1);

        // Remove active container id1
        CHECK(session.removeContainer(id1) == true);
        CHECK(session.getContainerCount() == 1);
        // Automatically switches to id2
        CHECK(session.getActiveAudioContainerId() == id2);

        // Remove remaining container
        CHECK(session.removeContainer(id2) == true);
        CHECK(session.getContainerCount() == 0);
        CHECK(session.getActiveAudioContainerId() == -1);
    }

    SECTION("8. Generación de informe HTML comparativo auto-contenido con SVG")
    {
        auto c1 = createSyntheticContainer(tempDir, "Report_C1", MeasurementExecutionDomain::Vst3OfflineDigital, 0.0);
        auto c2 = createSyntheticContainer(tempDir, "Report_C2", MeasurementExecutionDomain::Vst3OfflineDigital, 1.5);
        auto cCorrupt = createSyntheticContainer(tempDir, "Report_Corrupt", MeasurementExecutionDomain::Vst3OfflineDigital, 0.0);
        cCorrupt.getChildFile("audio/dexed_reference.wav").appendText("TAMPER");

        MeasurementComparisonSession session;
        session.addContainerSync(c1);
        session.addContainerSync(c2);
        session.addContainerSync(cCorrupt);

        auto reportFile = tempDir.getChildFile("test_comparison_report.html");
        juce::String err;
        const bool genOk = MeasurementComparisonReportGenerator::generateReport(session, reportFile, err);

        REQUIRE(genOk);
        REQUIRE(reportFile.existsAsFile());
        REQUIRE(reportFile.getSize() > 500);

        const juce::String htmlContent = reportFile.loadFileAsString();
        CHECK(htmlContent.contains("<svg"));
        CHECK(htmlContent.contains("Series Verificadas y Comparadas"));
        CHECK(htmlContent.contains("Contenedores Excluidos de Comparación"));
        CHECK(htmlContent.contains("Report_Corrupt"));
        CHECK(htmlContent.contains("Matriz de Equivalencia de Estado"));
    }

    SECTION("9. Componentes gráficos interactivos se instancian y redimensionan de forma segura")
    {
        MeasurementComparisonSession session;
        MeasurementContainerListPanel listPanel(session);
        MeasurementDynamicsComparisonComponent curveComp(session);
        MeasurementStateEquivalenceCard card(session);
        MeasurementComparisonPanel masterPanel;

        listPanel.setSize(300, 400);
        curveComp.setSize(600, 400);
        card.setSize(300, 200);
        masterPanel.setSize(1024, 768);

        // Trigger safe repaints
        juce::Image img(juce::Image::ARGB, 1024, 768, true);
        juce::Graphics g(img);
        masterPanel.paintEntireComponent(g, true);

        CHECK(masterPanel.getWidth() == 1024);
        CHECK(masterPanel.getHeight() == 768);
    }

    // Cleanup temp directory
    tempDir.deleteRecursively();
}
