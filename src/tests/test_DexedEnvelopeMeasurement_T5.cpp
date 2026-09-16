/**
 * @file test_DexedEnvelopeMeasurement_T5.cpp
 * @brief Fase 20.10.0 - T5: Primera prueba vertical completa de medicion de envolvente sobre Dexed VST3.
 *
 * Flujo completo real:
 * Dexed VST3 -> NoteOn (0) -> NoteOff (57.600) -> Captura (72.000 muestras a 48 kHz) ->
 * SynthEnvelopeAnalyzer / EnvelopeMeasurementAdapter -> MeasurementResult ->
 * WAV / JSON / Curva temporal -> manifest SHA-256 -> Informe HTML -> Reapertura e integridad.
 *
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "synth/ExternalPluginFixture.h"
#include "measurement/MeasurementContracts.h"
#include "measurement/MeasurementStimulusCoordinator.h"
#include "measurement/MeasurementCaptureCoordinator.h"
#include "measurement/adapters/EnvelopeMeasurementAdapter.h"
#include "measurement/MeasurementContainerExporter.h"
#include "core/ExperimentStorage.h"

#include <cmath>

namespace
{

juce::File getDexedPluginFile()
{
    // 1. Variable de entorno
    auto envPath = juce::SystemStats::getEnvironmentVariable("DEXED_VST3_PATH", "");
    if (envPath.isNotEmpty())
    {
        juce::File f(envPath);
        if (f.exists())
            return f;
    }

    // 2. Ruta estandar VST3 en Windows
    juce::File defaultWin("C:\\Program Files\\Common Files\\VST3\\Dexed.vst3");
    if (defaultWin.exists())
        return defaultWin;

    // 3. Ruta en AppData local
    juce::File localVst3 = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("../Local/Programs/Common/VST3/Dexed.vst3");
    if (localVst3.exists())
        return localVst3;

    return {};
}

bool writeAudioBufferToWav(const juce::File& destinationFile, const std::vector<float>& monoSamples, double sampleRate)
{
    destinationFile.deleteFile();
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> outStream(destinationFile.createOutputStream());
    if (outStream == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(outStream.get(), sampleRate, 1, 16, {}, 0));
    if (writer != nullptr)
    {
        outStream.release(); // writer took ownership
        juce::AudioBuffer<float> buf(1, static_cast<int>(monoSamples.size()));
        for (size_t i = 0; i < monoSamples.size(); ++i)
            buf.setSample(0, static_cast<int>(i), monoSamples[i]);

        writer->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
        return true;
    }
    return false;
}

} // namespace

TEST_CASE("Fase 20.10.0 - T5: Medicion vertical completa de envolvente sobre Dexed VST3", "[measurement][dexed][t5]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // 1. Localizar binario de Dexed VST3
    juce::File dexedFile = getDexedPluginFile();
    if (!dexedFile.exists())
    {
        SKIP("Dexed.vst3 no encontrado en rutas estandar. Omitiendo prueba vertical T5.");
        return;
    }

    // 2. Instanciar e inicializar Dexed via ExternalPluginFixture
    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    abdaudiolab::synth::ExternalPluginFixture fixture(formatManager);
    std::string loadErr;
    const double sampleRate = 48000.0;
    const int blockSize = 512;
    const int numChannels = 2;

    bool loaded = fixture.loadPluginFromDisk(dexedFile, sampleRate, blockSize, loadErr);
    REQUIRE(loaded);
    REQUIRE(loadErr.empty());

    const auto& identity = fixture.getIdentity();
    REQUIRE(identity.pluginName == "Dexed");
    REQUIRE(identity.format == "VST3");
    REQUIRE(!identity.binaryHash.empty());

    // 3. Configurar especificacion metrologica exacta de T5
    abdaudiolab::measurement::MeasurementSpec spec;
    spec.measurementId = "meas-dexed-envelope-t5";
    spec.measurementType = "envelope";
    spec.dutType = abdaudiolab::measurement::DeviceUnderTest::instrument;
    spec.parameterName = "Dexed";

    spec.execution.sampleRateHz = sampleRate;
    spec.execution.blockSize = blockSize;
    spec.execution.numChannels = numChannels;
    spec.execution.latencySamples = static_cast<int>(fixture.timingInfo().declaredLatencySamples);

    spec.stimulus.type = abdaudiolab::measurement::StimulusType::midiNote;
    spec.stimulus.midiChannel = 1;
    spec.stimulus.midiNoteNumber = 60; // Nota C4
    spec.stimulus.midiVelocity = 0.8f;
    spec.stimulus.noteOnSample = 0;
    spec.stimulus.noteOffSample = 57600; // 1.2 segundos sostenida a 48 kHz
    spec.stimulus.durationSec = 1.5;     // 72.000 muestras en total (300 ms de cola)
    spec.stimulus.sha256 = abdaudiolab::measurement::MeasurementStimulusCoordinator::computeSpecHash(spec.stimulus);

    spec.analysis.analysisType = "SynthEnvelopeAnalyzer";

    // 4. Ejecutar captura sincrona determinista (MeasurementCaptureCoordinator)
    auto capResult = abdaudiolab::measurement::MeasurementCaptureCoordinator::captureSynchronous(
        &fixture, spec, nullptr);

    REQUIRE(capResult.status == abdaudiolab::measurement::MeasurementStatus::completed);
    REQUIRE(capResult.capturedAudio.size() == 72000); // 72.000 muestras exactas
    REQUIRE(!capResult.stimulusSha256.empty());
    REQUIRE(!capResult.presetStateSha256.empty());

    // Verificar que el audio capturado no sea plano ni silencioso
    float maxAbs = 0.0f;
    for (float sample : capResult.capturedAudio)
    {
        float a = std::abs(sample);
        if (a > maxAbs)
            maxAbs = a;
    }
    float peakDb = maxAbs > 1e-9f ? 20.0f * std::log10(maxAbs) : -120.0f;
    REQUIRE(peakDb > -60.0f);

    // 5. Analisis metrologico de envolvente via SynthEnvelopeAnalyzer (EnvelopeMeasurementAdapter)
    auto measResult = abdaudiolab::measurement::EnvelopeMeasurementAdapter::measure(
        spec,
        capResult.capturedAudio,
        sampleRate,
        spec.stimulus.noteOnSample,
        spec.stimulus.noteOffSample,
        0);

    REQUIRE(measResult.status == abdaudiolab::measurement::MeasurementStatus::completed);
    REQUIRE(measResult.dut.name == "Dexed");
    REQUIRE(measResult.dut.format == "VST3");
    REQUIRE(measResult.analyzer.name == "SynthEnvelopeAnalyzer");
    REQUIRE(measResult.analyzer.version == "1.0.0");
    REQUIRE(measResult.observability.status == "observed");

    // Verificar presencia de metricas ADSR con unidades tipadas
    auto getMetric = [&](const juce::String& name) -> const abdaudiolab::measurement::MeasurementMetric* {
        for (const auto& m : measResult.metrics)
            if (m.name == name) return &m;
        return nullptr;
    };

    const auto* attack = getMetric("attackTime");
    const auto* decay = getMetric("decayTime");
    const auto* sustain = getMetric("sustainLevel");
    const auto* release = getMetric("releaseTime");
    const auto* peak = getMetric("peakAmplitude");

    REQUIRE(attack != nullptr);
    REQUIRE(attack->unit == "ms");
    REQUIRE(attack->status == "observed");
    REQUIRE(attack->value >= 0.0);

    REQUIRE(decay != nullptr);
    REQUIRE(decay->unit == "ms");
    // decay puede ser observed o unreliable segun la compuerta y el sonido
    REQUIRE((decay->status == "observed" || decay->status == "unreliable"));

    REQUIRE(sustain != nullptr);
    REQUIRE(sustain->unit == "dBFS");
    REQUIRE(sustain->status == "observed");

    REQUIRE(release != nullptr);
    REQUIRE(release->unit == "ms");
    REQUIRE((release->status == "observed" || release->status == "unreliable"));

    REQUIRE(peak != nullptr);
    REQUIRE(peak->unit == "dBFS");
    REQUIRE(peak->status == "observed");
    REQUIRE(peak->value > -60.0);

    // Verificar curva temporal de amplitud
    REQUIRE(!measResult.curve.x.empty());
    REQUIRE(measResult.curve.x.size() == measResult.curve.y.size());
    REQUIRE(measResult.curve.xName == "time");
    REQUIRE(measResult.curve.xUnit == "ms");
    REQUIRE(measResult.curve.yName == "amplitude");
    REQUIRE(measResult.curve.yUnit == "dBFS");

    // 6. Escribir archivo temporal WAV para exportacion FAIR
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_Dexed_T5_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempDir.createDirectory();

    juce::File wavFile = tempDir.getChildFile("dexed_capture.wav");
    REQUIRE(writeAudioBufferToWav(wavFile, capResult.capturedAudio, sampleRate));

    // 7. Persistir en contenedor FAIR formal
    juce::File containerDir = tempDir.getChildFile("Dexed_Envelope_Container");
    juce::String exportErr;
    bool exported = abdaudiolab::measurement::MeasurementContainerExporter::exportMeasurement(
        containerDir, spec, measResult, wavFile, exportErr);

    REQUIRE(exported);
    REQUIRE(exportErr.isEmpty());

    // Verificar existencia de los artefactos requeridos
    REQUIRE(containerDir.getChildFile("experiment.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("specs/measurement_spec.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("specs/measurement_stimulus.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("results/measurement_result.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("curves/envelope_curve.json").existsAsFile());
    REQUIRE(containerDir.getChildFile("audio/envelope_reference.wav").existsAsFile());
    REQUIRE(containerDir.getChildFile("reports/measurement_report.html").existsAsFile());
    REQUIRE(containerDir.getChildFile("manifest.json").existsAsFile());

    // 8. Reabrir contenedor mediante ExperimentFolderReader y verificar roles FAIR
    abdaudiolab::core::ExperimentFolderReader reader;
    REQUIRE(reader.canRead(containerDir));

    juce::String readErr;
    auto recordOpt = reader.read(containerDir, readErr);
    REQUIRE(recordOpt.has_value());
    REQUIRE(recordOpt->status != abdaudiolab::core::ExperimentStatus::Corrupt);
    REQUIRE(recordOpt->artifacts.size() == 6);

    auto hasRole = [&](const std::string& role) {
        for (const auto& art : recordOpt->artifacts)
            if (art.role == role) return true;
        return false;
    };

    REQUIRE(hasRole("measurement_spec"));
    REQUIRE(hasRole("measurement_stimulus"));
    REQUIRE(hasRole("measurement_result"));
    REQUIRE(hasRole("envelope_curve"));
    REQUIRE(hasRole("measurement_baseline_audio"));
    REQUIRE(hasRole("measurement_report"));

    // 9. Comprobar reglas del informe HTML
    juce::File reportHtmlFile = containerDir.getChildFile("reports/measurement_report.html");
    std::string htmlContent = reportHtmlFile.loadFileAsString().toStdString();

    REQUIRE(htmlContent.find("Envelope measurement: COMPLETED") != std::string::npos);
    REQUIRE(htmlContent.find("PASS") == std::string::npos); // NUNCA emitir veredicto falso PASS
    REQUIRE(htmlContent.find("<svg") != std::string::npos);  // Curva SVG embebida
    REQUIRE(htmlContent.find("<audio controls") != std::string::npos); // Reproductor de audio
    REQUIRE(htmlContent.find("attackTime") != std::string::npos);
    REQUIRE(htmlContent.find("sustainLevel") != std::string::npos);

    // 10. Prueba de manipulacion sobre el contenedor real de Dexed
    SECTION("Tampering with results/measurement_result.json is detected as Corrupt")
    {
        juce::File resFile = containerDir.getChildFile("results/measurement_result.json");
        std::string original = resFile.loadFileAsString().toStdString();
        resFile.replaceWithText(juce::String(original + " ")); // un espacio adicional altera el SHA-256

        juce::String tamperErr;
        auto tamperedRecord = reader.read(containerDir, tamperErr);
        REQUIRE(tamperedRecord.has_value());
        REQUIRE(tamperedRecord->status == abdaudiolab::core::ExperimentStatus::Corrupt);
        REQUIRE(tamperErr.contains("Cryptographic mismatch"));
    }

    SECTION("Tampering with audio/envelope_reference.wav is detected as Corrupt")
    {
        juce::File wavInContainer = containerDir.getChildFile("audio/envelope_reference.wav");
        std::vector<float> alteredSamples(24000, 0.0f);
        writeAudioBufferToWav(wavInContainer, alteredSamples, sampleRate);

        juce::String tamperErr;
        auto tamperedRecord = reader.read(containerDir, tamperErr);
        REQUIRE(tamperedRecord.has_value());
        REQUIRE(tamperedRecord->status == abdaudiolab::core::ExperimentStatus::Corrupt);
        REQUIRE(tamperErr.contains("Cryptographic mismatch"));
    }

    // Limpieza
    tempDir.deleteRecursively();
}
