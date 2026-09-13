#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "synth/ExternalPluginFixture.h"
#include "synth/TargetContractDiscovery.h"
#include "synth/TargetAuditor.h"
#include "synth/ParameterExcitationEngine.h"
#include "synth/ExperimentRecipe.h"
#include "synth/Sha256.h"
#include <iostream>
#include <fstream>
#include <sstream>

using namespace abdaudiolab::synth;

static void writeStringToFile(const juce::File& file, const std::string& content)
{
    file.replaceWithText(juce::String::fromUTF8(content.c_str()));
}

static void saveWavFile(const juce::File& file, const std::vector<float>& audio, double sampleRate)
{
    juce::WavAudioFormat wavFormat;
    file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> fileStream(file.createOutputStream());
    if (fileStream != nullptr)
    {
        juce::StringPairArray metadata;
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(fileStream.get(), sampleRate, 1, 24, metadata, 0));
        if (writer != nullptr)
        {
            fileStream.release(); // El writer toma propiedad del stream
            const float* channelData[1] = { audio.data() };
            writer->writeFromFloatArrays(channelData, 1, static_cast<int>(audio.size()));
        }
    }
}

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::string pluginPathStr;
    std::string recipeName = "ParameterStep";
    double sampleRate = 96000.0;
    int blockSize = 256;
    std::string outputDirStr = "./artifacts/vst3-validation";

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--plugin" && i + 1 < argc) pluginPathStr = argv[++i];
        else if (arg == "--recipe" && i + 1 < argc) recipeName = argv[++i];
        else if (arg == "--sample-rate" && i + 1 < argc) sampleRate = std::stod(argv[++i]);
        else if (arg == "--block-size" && i + 1 < argc) blockSize = std::stoi(argv[++i]);
        else if (arg == "--output" && i + 1 < argc) outputDirStr = argv[++i];
    }

    if (pluginPathStr.empty())
    {
        std::cerr << "Uso: ABDAudioLab_Vst3Validation --plugin <path.vst3> [--recipe <name>] [--sample-rate <sr>] [--block-size <bs>] [--output <dir>]\n";
        return 1;
    }

    juce::File pluginFile(juce::String::fromUTF8(pluginPathStr.c_str()));
    juce::File outDir(juce::String::fromUTF8(outputDirStr.c_str()));
    outDir.createDirectory();
    juce::File audioDir = outDir.getChildFile("audio");
    audioDir.createDirectory();

    std::cout << "=== ABDAudioLab VST3 Validation Harness ===\n";
    std::cout << "Plugin: " << pluginFile.getFullPathName().toStdString() << "\n";
    std::cout << "Recipe: " << recipeName << " | SR: " << sampleRate << " | Block: " << blockSize << "\n";

    // 1. Inicializar AudioPluginFormatManager con soporte VST3
    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    // 2. Carga física del binario desde disco
    ExternalPluginFixture fixture(formatManager);
    std::string loadErr;
    if (!fixture.loadPluginFromDisk(pluginFile, sampleRate, blockSize, loadErr))
    {
        std::cerr << "[ERROR] Fallo al cargar plugin VST3: " << loadErr << "\n";
        return 2;
    }

    std::cout << "[OK] Plugin instanciado correctamente: " << fixture.getIdentity().pluginName << "\n";

    // 3. Descubrimiento agnóstico del contrato
    TargetContract contract = fixture.discoverContract();
    TargetContractDiscovery discovery;
    std::string contractJson = discovery.exportToJson(contract);
    writeStringToFile(outDir.getChildFile("discovered-contract.json"), contractJson);

    // 4. Auditoría del Target (Fase 20.2)
    TargetAuditor auditor;
    ProcessingSpec spec{ sampleRate, blockSize, 2 };
    TargetAuditReport auditReport = auditor.auditTarget(fixture, spec);

    std::ostringstream auditJson;
    auditJson << "{\n"
              << "  \"approvalStatus\": " << static_cast<int>(auditReport.approvalStatus) << ",\n"
              << "  \"determinism\": " << static_cast<int>(auditReport.determinism) << ",\n"
              << "  \"resetCapability\": " << static_cast<int>(auditReport.resetCapability) << ",\n"
              << "  \"generation\": " << static_cast<int>(auditReport.generation) << ",\n"
              << "  \"summaryMessage\": \"" << auditReport.summaryMessage << "\"\n"
              << "}\n";
    writeStringToFile(outDir.getChildFile("audit-report.json"), auditJson.str());

    // 5. Ejecutar la receta seleccionada (Fase 20.3)
    ParameterExcitationEngine engine(fixture, contract, auditReport, spec);
    std::unique_ptr<IExperimentRecipe> recipe;

    std::string targetParam = "cutoff";
    for (const auto& p : contract.parameters)
    {
        if (p.role == ParameterRole::AudioControl && p.category == ParameterCategory::Filter)
        {
            targetParam = p.nativeId;
            break;
        }
    }

    if (recipeName == "ParameterRamp")
    {
        recipe = std::make_unique<ParameterRampRecipe>("ramp_test", targetParam, 0.1, 1.0, 0.5, 32);
    }
    else if (recipeName == "LocalPerturbation")
    {
        recipe = std::make_unique<LocalPerturbationRecipe>("pert_test", targetParam, 0.50, 0.05, 0.25);
    }
    else
    {
        recipe = std::make_unique<ParameterStepRecipe>("step_test", targetParam, std::vector<double>{ 0.2, 0.4, 0.6, 0.8, 1.0 }, 0.2);
    }

    SynthPresetState baseState;
    auto experimentReport = engine.executeRecipe(*recipe, baseState);

    // 6. Exportar trazas, planes y audio capturado
    ExperimentPlan plan = recipe->generatePlan(sampleRate, SettlingPolicy{}, RandomizationPolicy{});
    std::ostringstream planJson;
    planJson << "{\n"
             << "  \"planId\": \"" << plan.planId << "\",\n"
             << "  \"recipeType\": \"" << plan.recipeType << "\",\n"
             << "  \"totalDurationSec\": " << plan.totalDurationSec << ",\n"
             << "  \"eventsCount\": " << plan.events.size() << ",\n"
             << "  \"windowsCount\": " << plan.windows.size() << "\n"
             << "}\n";
    writeStringToFile(outDir.getChildFile("experiment-plan.json"), planJson.str());

    std::ostringstream traceJson;
    traceJson << "{\n"
              << "  \"planId\": \"" << experimentReport.trace.planId << "\",\n"
              << "  \"executedParameters\": " << experimentReport.trace.executedParameterEvents.size() << ",\n"
              << "  \"rmsLevelDb\": " << experimentReport.trace.rmsLevelDb << ",\n"
              << "  \"peakLevelDb\": " << experimentReport.trace.peakLevelDb << ",\n"
              << "  \"traceHash\": \"" << experimentReport.trace.traceHash << "\"\n"
              << "}\n";
    writeStringToFile(outDir.getChildFile("execution-trace.json"), traceJson.str());

    juce::File mainAudioWav = audioDir.getChildFile("rendered_session.wav");
    saveWavFile(mainAudioWav, experimentReport.trace.capturedAudio, sampleRate);

    // 7. Manifiesto del Bundle y Entorno
    const auto& ident = fixture.getIdentity();
    std::ostringstream bundleJson;
    bundleJson << "{\n"
               << "  \"bundleHash\": \"" << ident.bundleHash << "\",\n"
               << "  \"binaryHash\": \"" << ident.binaryHash << "\",\n"
               << "  \"filesCount\": " << ident.bundleFiles.size() << ",\n"
               << "  \"files\": [\n";
    for (size_t i = 0; i < ident.bundleFiles.size(); ++i)
    {
        const auto& f = ident.bundleFiles[i];
        bundleJson << "    { \"path\": \"" << f.relativePath << "\", \"size\": " << f.fileSize << ", \"sha256\": \"" << f.fileSha256 << "\" }"
                   << (i + 1 < ident.bundleFiles.size() ? "," : "") << "\n";
    }
    bundleJson << "  ]\n}\n";
    writeStringToFile(outDir.getChildFile("plugin-bundle-manifest.json"), bundleJson.str());

    std::ostringstream envJson;
    envJson << "{\n"
            << "  \"host\": \"ABDAudioLab_Vst3Validation\",\n"
            << "  \"juceVersion\": \"" << JUCE_STRINGIFY(JUCE_MAJOR_VERSION) "." JUCE_STRINGIFY(JUCE_MINOR_VERSION) "." JUCE_STRINGIFY(JUCE_BUILDNUMBER) "\",\n"
            << "  \"architecture\": \"" << ident.architecture << "\",\n"
            << "  \"sampleRate\": " << sampleRate << ",\n"
            << "  \"blockSize\": " << blockSize << "\n"
            << "}\n";
    writeStringToFile(outDir.getChildFile("environment.json"), envJson.str());

    // 8. Informe de Validación Consolidado
    std::ostringstream valJson;
    valJson << "{\n"
            << "  \"plugin\": \"" << ident.pluginName << "\",\n"
            << "  \"executionPermitted\": " << (experimentReport.executionPermitted ? "true" : "false") << ",\n"
            << "  \"observedWindows\": " << experimentReport.observations.size() << ",\n"
            << "  \"summary\": \"" << experimentReport.summaryNotes << "\",\n"
            << "  \"observedFeatureMonotonicity\": \"" << experimentReport.rampAnalysis.observedFeatureMonotonicity << "\",\n"
            << "  \"physicalParameterMonotonicity\": \"" << experimentReport.rampAnalysis.physicalParameterMonotonicity << "\"\n"
            << "}\n";
    writeStringToFile(outDir.getChildFile("validation-report.json"), valJson.str());

    // 9. Manifiesto maestro con hashes de todos los artefactos generados
    std::ostringstream manifestJson;
    manifestJson << "{\n"
                 << "  \"schemaVersion\": \"1.0.0\",\n"
                 << "  \"pluginBinaryHash\": \"" << ident.binaryHash << "\",\n"
                 << "  \"parameterContractHash\": \"" << contract.parameterContractHash << "\",\n"
                 << "  \"experimentPlanHash\": \"" << experimentReport.planHash << "\",\n"
                 << "  \"executionTraceHash\": \"" << experimentReport.trace.traceHash << "\"\n"
                 << "}\n";
    writeStringToFile(outDir.getChildFile("manifest.json"), manifestJson.str());

    std::cout << "[OK] Ensayos VST3 completados exitosamente. Artefactos exportados en: " << outDir.getFullPathName().toStdString() << "\n";
    return 0;
}
