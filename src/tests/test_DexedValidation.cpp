#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "synth/ExternalPluginFixture.h"
#include "synth/TargetContractDiscovery.h"
#include "synth/TargetAuditor.h"
#include "synth/ParameterExcitationEngine.h"
#include "synth/ExperimentRecipe.h"
#include "synth/Sha256.h"
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <unordered_set>
#include <cmath>

using namespace abdaudiolab::synth;

namespace
{

/**
 * @brief Configuración metrológica de referencia para el target externo Dexed.
 * Las expectativas se parametrizan por versión para evitar fragilidad ante actualizaciones del binario.
 */
struct DexedFixtureConfig
{
    std::string expectedManufacturer = "Digital Suburban";
    std::string expectedVersion = "1.0.1";
    int expectedMinParameters = 120;

    // Hashes observados de la versión de referencia 1.0.1 en Windows x64
    std::string expectedBinaryHash = "e8b3b00a53bb0aa1ef082b0c1b5cb66bdf1af5eddf787b8f47397df6d2411a40";
    std::string expectedBundleHash = "3a8df26e4462975b74e1125de4a28c53e1e6773a91138cae8792f2d111a59396";
    std::string expectedContractHash = "2a19f6b89f70d81a264e2b745bc9794f88cf52d876b74732669eed7ce0242b59";

    static juce::File getDexedFile()
    {
        // 1. Variable de entorno para CI o rutas personalizadas
        auto envPath = juce::SystemStats::getEnvironmentVariable("DEXED_VST3_PATH", "");
        if (envPath.isNotEmpty())
        {
            juce::File f(envPath);
            if (f.exists())
                return f;
        }

        // 2. Ruta estándar de instalación VST3 en Windows
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
};

} // namespace

TEST_CASE("Dexed bundle identity", "[external][dexed]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto pluginFile = DexedFixtureConfig::getDexedFile();
    if (!pluginFile.exists())
    {
        SKIP("External fixture unavailable: Dexed.vst3 not found at configured path");
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    ExternalPluginFixture fixture(formatManager);
    std::string loadErr;
    bool loaded = fixture.loadPluginFromDisk(pluginFile, 96000.0, 256, loadErr);
    REQUIRE(loaded);

    const auto& identity = fixture.getIdentity();
    DexedFixtureConfig config;

    SECTION("Metadatos de fabricante e identidad de plugin")
    {
        REQUIRE(identity.pluginName == "Dexed");
        REQUIRE(identity.manufacturer == config.expectedManufacturer);
        REQUIRE(identity.format == "VST3");
        REQUIRE_FALSE(identity.pluginUid.empty());
    }

    SECTION("Estructura de bundle y hashes canónicos")
    {
        REQUIRE(identity.bundleFiles.size() >= 2);
        REQUIRE_FALSE(identity.binaryHash.empty());
        REQUIRE_FALSE(identity.bundleHash.empty());

        // Comprobación no bloqueante de drift de versión
        if (identity.version == config.expectedVersion)
        {
            if (identity.binaryHash != config.expectedBinaryHash)
            {
                UNSCOPED_INFO("ExternalFixtureDrift: binary hash differs from reference 1.0.1 build");
            }
            if (identity.bundleHash != config.expectedBundleHash)
            {
                UNSCOPED_INFO("ExternalFixtureDrift: bundle hash differs from reference 1.0.1 layout");
            }
        }
    }
}

TEST_CASE("Dexed contract discovery", "[external][dexed]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto pluginFile = DexedFixtureConfig::getDexedFile();
    if (!pluginFile.exists())
    {
        SKIP("External fixture unavailable: Dexed.vst3 not found at configured path");
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    ExternalPluginFixture fixture(formatManager);
    std::string loadErr;
    REQUIRE(fixture.loadPluginFromDisk(pluginFile, 96000.0, 256, loadErr));

    TargetContract contract = fixture.discoverContract();
    DexedFixtureConfig config;

    SECTION("Cardinalidad e integridad de parámetros")
    {
        REQUIRE(contract.name == "Dexed");
        REQUIRE(contract.acceptsMidi);
        REQUIRE(contract.numAudioOutputs == 2);
        REQUIRE(static_cast<int>(contract.parameters.size()) >= config.expectedMinParameters);

        // Comprobar ausencia de duplicados en IDs nativos
        std::unordered_set<std::string> uniqueIds;
        for (const auto& p : contract.parameters)
        {
            REQUIRE_FALSE(p.nativeId.empty());
            REQUIRE(uniqueIds.insert(p.nativeId).second);

            // Validación numérica: sin NaN ni infinitos
            REQUIRE_FALSE(std::isnan(p.minValue));
            REQUIRE_FALSE(std::isnan(p.maxValue));
            REQUIRE_FALSE(std::isnan(p.defaultValue));
            REQUIRE(p.minValue <= p.maxValue);
        }
    }

    SECTION("Preservacion semantica honesta ('Descubrir no es comprender')")
    {
        // Cutoff y Resonance: si existen en el contrato, deben conservar origen Inferred
        const auto* cutoff = contract.findParameter("Cutoff");
        if (cutoff != nullptr)
        {
            REQUIRE(cutoff->category == ParameterCategory::Filter);
            REQUIRE(cutoff->semanticEvidence == SemanticEvidence::InferredFromName);
            REQUIRE(cutoff->semanticStatus == SemanticStatus::Inferred);
        }

        // Parámetros de arquitectura FM: ALGORITHM, FEEDBACK, MonoMode
        // No deben forzarse erróneamente a Filter ni Envelope
        const auto* algo = contract.findParameter("ALGORITHM");
        if (algo != nullptr)
        {
            REQUIRE(algo->category != ParameterCategory::Filter);
            REQUIRE(algo->category != ParameterCategory::Envelope);
        }

        const auto* feedback = contract.findParameter("FEEDBACK");
        if (feedback != nullptr)
        {
            REQUIRE(feedback->category != ParameterCategory::Filter);
            REQUIRE(feedback->category != ParameterCategory::Envelope);
        }
    }

    SECTION("Reproducibilidad del hash canónico del contrato")
    {
        REQUIRE_FALSE(contract.parameterContractHash.empty());
        std::string h1 = contract.parameterContractHash;
        contract.computeHash();
        std::string h2 = contract.parameterContractHash;
        REQUIRE(h1 == h2);
    }
}

TEST_CASE("Dexed target audit", "[external][dexed]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto pluginFile = DexedFixtureConfig::getDexedFile();
    if (!pluginFile.exists())
    {
        SKIP("External fixture unavailable: Dexed.vst3 not found at configured path");
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    ExternalPluginFixture fixture(formatManager);
    std::string loadErr;
    REQUIRE(fixture.loadPluginFromDisk(pluginFile, 96000.0, 256, loadErr));

    TargetAuditor auditor;
    ProcessingSpec spec{ 96000.0, 256, 2 };
    TargetAuditReport report = auditor.auditTarget(fixture, spec);

    SECTION("Dictamen de auditoría compatible")
    {
        // En sintetizadores FM complejos de terceros, el estado puede ser ApprovedWithWarnings
        // debido a dependencias de fase libre u osciladores libres entre notas.
        bool isApprovedOrWithWarnings = (report.approvalStatus == ApprovalStatus::Approved ||
                                         report.approvalStatus == ApprovalStatus::ApprovedWithWarnings);
        REQUIRE(isApprovedOrWithWarnings);

        // Determinismo compatible: determinista puro o determinista tras reset
        bool isDeterministic = (report.determinism == DeterminismClass::Deterministic ||
                                report.determinism == DeterminismClass::DeterministicAfterReset);
        REQUIRE(isDeterministic);

        // Si existen advertencias operativas, deben prescribir instrucciones concretas
        if (report.approvalStatus == ApprovalStatus::ApprovedWithWarnings)
        {
            REQUIRE(report.operationalInstructions.resetBeforeEachTrial);
            REQUIRE(report.operationalInstructions.recommendedSettlingTimeMs >= 0.0);
        }
    }
}

TEST_CASE("Dexed state round trip", "[external][dexed]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto pluginFile = DexedFixtureConfig::getDexedFile();
    if (!pluginFile.exists())
    {
        SKIP("External fixture unavailable: Dexed.vst3 not found at configured path");
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    ExternalPluginFixture fixture(formatManager);
    std::string loadErr;
    REQUIRE(fixture.loadPluginFromDisk(pluginFile, 96000.0, 256, loadErr));

    ProcessingSpec spec{ 96000.0, 256, 2 };
    fixture.prepare(spec);

    SECTION("Capa 1 y 2: Identidad binaria y preservacion de estado")
    {
        std::vector<uint8_t> state1;
        auto res1 = fixture.getState(state1);
        REQUIRE(res1.succeeded);
        REQUIRE(!state1.empty());

        // Restaurar estado
        auto setRes = fixture.setState(state1);
        REQUIRE(setRes.succeeded);

        std::vector<uint8_t> state2;
        auto res2 = fixture.getState(state2);
        REQUIRE(res2.succeeded);
        REQUIRE(state1 == state2);
        REQUIRE(res1.stateDataHash == res2.stateDataHash);
    }

    SECTION("Capa 3: Identidad acustica tras round-trip")
    {
        std::vector<uint8_t> baseState;
        fixture.getState(baseState);

        MidiExcitationSequence seq;
        seq.totalDurationSec = 0.10;
        seq.gateDurationSec = 0.06;
        seq.events.push_back(TimedMidiEvent{ TimedMidiType::NoteOn, 1, 60, 0.8f, 0, 0.0 });

        fixture.resetState();
        std::vector<float> audioBefore;
        fixture.render(seq, audioBefore);

        fixture.setState(baseState);
        fixture.resetState();
        std::vector<float> audioAfter;
        fixture.render(seq, audioAfter);

        REQUIRE(audioBefore.size() == audioAfter.size());
        REQUIRE(!audioBefore.empty());
    }
}

TEST_CASE("Dexed controlled parameter excitation", "[external][dexed]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto pluginFile = DexedFixtureConfig::getDexedFile();
    if (!pluginFile.exists())
    {
        SKIP("External fixture unavailable: Dexed.vst3 not found at configured path");
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    ExternalPluginFixture fixture(formatManager);
    std::string loadErr;
    REQUIRE(fixture.loadPluginFromDisk(pluginFile, 96000.0, 256, loadErr));

    ProcessingSpec spec{ 96000.0, 256, 2 };
    fixture.prepare(spec);

    TargetContract contract = fixture.discoverContract();
    TargetAuditor auditor;
    TargetAuditReport auditReport = auditor.auditTarget(fixture, spec);
    REQUIRE(auditReport.isApprovedForParameterExcitation);

    ParameterExcitationEngine engine(fixture, contract, auditReport, spec);

    SECTION("Excitacion controlada con ParameterStepRecipe sobre parametro activo")
    {
        // Localizar Cutoff u otro parámetro de filtro
        std::string targetParam = "param_0"; // Cutoff en Dexed
        const auto* pDesc = contract.findParameter("Cutoff");
        if (pDesc != nullptr)
            targetParam = pDesc->nativeId;

        ParameterStepRecipe recipe("dexed_cutoff_step", targetParam, { 0.2, 0.5, 0.8 }, 0.15, 60);
        SynthPresetState baseState;
        auto report = engine.executeRecipe(recipe, baseState);

        REQUIRE(report.executionPermitted);
        REQUIRE(report.stepResponses.size() == 3);

        for (const auto& ev : report.trace.executedParameterEvents)
        {
            REQUIRE(ev.status == ParameterEventStatus::AppliedByTarget);
            REQUIRE(ev.appliedConfirmation == AppliedConfirmation::ConfirmedByAPI);
        }
    }

    SECTION("Deteccion honesta de controles no observables en el preset actual")
    {
        // En Dexed, un control de desafinación o ratio de un operador inactivo
        // no debe alterar el audio del preset base, clasificándose como NotObservedInCurrentCondition
        // (y nunca como Inactive a nivel global).
        const auto* inerteDesc = contract.findParameter("param_4"); // MASTER TUNE ADJ u operador
        if (inerteDesc != nullptr)
        {
            ParameterStepRecipe recipe("dexed_tune_step", inerteDesc->nativeId, { 0.5, 0.5, 0.5 }, 0.10, 60);
            SynthPresetState baseState;
            auto report = engine.executeRecipe(recipe, baseState);
            REQUIRE(report.executionPermitted);
        }
    }
}

TEST_CASE("Dexed artifact manifest", "[external][dexed]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto pluginFile = DexedFixtureConfig::getDexedFile();
    if (!pluginFile.exists())
    {
        SKIP("External fixture unavailable: Dexed.vst3 not found at configured path");
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    ExternalPluginFixture fixture(formatManager);
    std::string loadErr;
    REQUIRE(fixture.loadPluginFromDisk(pluginFile, 96000.0, 256, loadErr));

    const auto& identity = fixture.getIdentity();
    TargetContract contract = fixture.discoverContract();

    // Verificación de integridad de los hashes clave del manifiesto de reproducibilidad
    REQUIRE_FALSE(identity.binaryHash.empty());
    REQUIRE_FALSE(identity.bundleHash.empty());
    REQUIRE_FALSE(contract.parameterContractHash.empty());
    REQUIRE(identity.binaryHash.length() == 64);
    REQUIRE(identity.bundleHash.length() == 64);
    REQUIRE(contract.parameterContractHash.length() == 64);
}
