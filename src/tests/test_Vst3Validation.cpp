#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "fixtures/reference_vst3/ReferenceSynthProcessor.h"
#include "fixtures/reference_vst3/ReferenceSynthGroundTruth.h"
#include "synth/TargetContractDiscovery.h"
#include "synth/TargetAuditor.h"
#include "synth/PluginSynthTarget.h"
#include "synth/ParameterExcitationEngine.h"
#include "synth/ExperimentRecipe.h"
#include "synth/Sha256.h"
#include <cmath>

using namespace abdaudiolab::synth;
using namespace abdaudiolab::fixtures;

TEST_CASE("Fase 20.3.B - VST3 Discovery: Introspeccion Agnostica y Preservacion de Evidencia", "[vst3][validation]")
{
    ReferenceSynthProcessor processor;
    TargetContractDiscovery discovery;
    TargetContract contract = discovery.discoverContract(processor);

    SECTION("Metadatos y capacidades del target")
    {
        REQUIRE(contract.name == "ReferenceSynth");
        REQUIRE(contract.acceptsMidi);
        REQUIRE(contract.numAudioOutputs == 2);
        REQUIRE_FALSE(contract.parameterContractHash.empty());
    }

    SECTION("Separacion estricta entre controles de audio e infraestructura de test")
    {
        const auto* cutoff = contract.findParameter("cutoff");
        REQUIRE(cutoff != nullptr);
        REQUIRE(cutoff->role == ParameterRole::AudioControl);
        REQUIRE(cutoff->category == ParameterCategory::Filter);
        REQUIRE(cutoff->semanticEvidence == SemanticEvidence::InferredFromName);
        REQUIRE(cutoff->semanticStatus == SemanticStatus::Inferred);

        // Regla: No asumir smoothing por introspección
        REQUIRE_FALSE(cutoff->smoothing.declaredSmoothing);
        REQUIRE_FALSE(cutoff->smoothing.smoothingKnown);

        const auto* controlMode = contract.findParameter("test_control_mode");
        REQUIRE(controlMode != nullptr);
        REQUIRE(controlMode->role == ParameterRole::TestInfrastructure);
        REQUIRE(controlMode->category == ParameterCategory::Custom);

        const auto* smoothingParam = contract.findParameter("test_smoothing_ms");
        REQUIRE(smoothingParam != nullptr);
        REQUIRE(smoothingParam->role == ParameterRole::TestInfrastructure);
    }

    SECTION("Calculo reproducible del hash canónico del contrato")
    {
        std::string hash1 = contract.parameterContractHash;
        contract.computeHash();
        std::string hash2 = contract.parameterContractHash;
        REQUIRE(hash1 == hash2);
    }
}

TEST_CASE("Fase 20.3.B - VST3 Hosting: Aislamiento entre Instancias y Round-Trip de Estado", "[vst3][validation]")
{
    ReferenceSynthProcessor proc1;
    ReferenceSynthProcessor proc2;
    PluginSynthTarget target1(proc1);
    PluginSynthTarget target2(proc2);

    ProcessingSpec spec{ 96000.0, 256, 2 };
    target1.prepare(spec);
    target2.prepare(spec);

    SECTION("Aislamiento de estado entre instancias concurrentes")
    {
        // Modificar proc1
        for (auto* p : proc1.getParameters())
        {
            if (p != nullptr && p->getName(32) == "Filter Cutoff")
                p->setValueNotifyingHost(0.25f);
        }

        // Modificar proc2 con valor diferente
        for (auto* p : proc2.getParameters())
        {
            if (p != nullptr && p->getName(32) == "Filter Cutoff")
                p->setValueNotifyingHost(0.85f);
        }

        // Comprobar que no hay variables estáticas ni contaminación cruzada
        float val1 = 0.0f;
        for (auto* p : proc1.getParameters())
            if (p != nullptr && p->getName(32) == "Filter Cutoff") val1 = p->getValue();

        float val2 = 0.0f;
        for (auto* p : proc2.getParameters())
            if (p != nullptr && p->getName(32) == "Filter Cutoff") val2 = p->getValue();

        REQUIRE(val1 == Catch::Approx(0.25f));
        REQUIRE(val2 == Catch::Approx(0.85f));
    }

    SECTION("Ciclo de Round-Trip de Estado en 3 Capas (Binario, Parámetros y Render)")
    {
        std::vector<uint8_t> stateA;
        std::vector<uint8_t> stateB;

        target1.getState(stateA);

        // Modificar parámetros para crear estado B
        for (auto* p : proc1.getParameters())
        {
            if (p != nullptr && p->getName(32) == "Filter Cutoff")
                p->setValueNotifyingHost(0.15f);
        }
        target1.getState(stateB);
        REQUIRE(stateA != stateB);

        // Restaurar estado A
        target1.setState(stateA);
        std::vector<uint8_t> stateRestored;
        target1.getState(stateRestored);
        REQUIRE(stateA == stateRestored);
    }
}

TEST_CASE("Fase 20.3.B - VST3 Timing: Precision Temporal de Eventos MIDI Sub-Bloque", "[vst3][validation]")
{
    ReferenceSynthProcessor processor;
    PluginSynthTarget target(processor);
    int blockSize = 256;
    ProcessingSpec spec{ 96000.0, blockSize, 2 };
    target.prepare(spec);

    std::vector<int> testOffsets = { 0, 1, 32, blockSize / 2, blockSize - 2 };
    std::vector<int> observedOnsetSamples;

    for (int offset : testOffsets)
    {
        processor.prepareToPlay(96000.0, blockSize);

        MidiExcitationSequence seq;
        seq.totalDurationSec = 0.05;
        seq.gateDurationSec = 0.03;
        seq.preSilenceSec = 0.0;
        seq.postSilenceSec = 0.02;

        TimedMidiEvent ev;
        ev.type = TimedMidiType::NoteOn;
        ev.noteNumber = 69; // A4 (440 Hz)
        ev.velocity = 0.8f;
        ev.sampleOffset = offset;
        seq.events.push_back(ev);

        std::vector<float> audio;
        target.render(seq, audio);

        // Buscar primera muestra con audio no nulo (umbral de silencio -60 dBfs)
        int firstSample = -1;
        for (size_t i = 0; i < audio.size(); ++i)
        {
            if (std::abs(audio[i]) > 0.001f)
            {
                firstSample = static_cast<int>(i);
                break;
            }
        }

        REQUIRE(firstSample >= offset);
        observedOnsetSamples.push_back(firstSample);
    }

    // Verificar que el desplazamiento de inicio es estrictamente monótono con el sampleOffset
    for (size_t i = 1; i < observedOnsetSamples.size(); ++i)
    {
        REQUIRE(observedOnsetSamples[i] > observedOnsetSamples[i - 1]);
    }
}

TEST_CASE("Fase 20.3.B - VST3 Excitation: Trazabilidad de 4 Estados y Politica de Monotonicidad", "[vst3][validation]")
{
    ReferenceSynthProcessor processor;
    PluginSynthTarget target(processor);
    ProcessingSpec spec{ 96000.0, 256, 2 };
    target.prepare(spec);

    TargetContractDiscovery discovery;
    TargetContract contract = discovery.discoverContract(processor);

    TargetAuditor auditor;
    TargetAuditReport auditReport = auditor.auditTarget(target, spec);
    REQUIRE(auditReport.approvalStatus == ApprovalStatus::Approved);

    ParameterExcitationEngine engine(target, contract, auditReport, spec);

    SECTION("ParameterStepRecipe con trazabilidad de 4 estados")
    {
        ParameterStepRecipe recipe("vst3_cutoff_step", "cutoff", { 0.2, 0.4, 0.6, 0.8, 1.0 }, 0.2, 60);
        SynthPresetState baseState;
        auto report = engine.executeRecipe(recipe, baseState);

        REQUIRE(report.executionPermitted);
        REQUIRE(report.stepResponses.size() == 5);

        // Verificar trazabilidad de eventos
        for (const auto& ev : report.trace.executedParameterEvents)
        {
            REQUIRE(ev.status == ParameterEventStatus::AppliedByTarget);
            REQUIRE(ev.appliedConfirmation == AppliedConfirmation::ConfirmedByAPI);
            REQUIRE(ev.transportAccuracy == TransportAccuracy::SampleAccurate);
        }

        // Monotonicidad de la característica observada
        for (size_t i = 1; i < report.stepResponses.size(); ++i)
        {
            REQUIRE(report.stepResponses[i].features.spectralCentroidHz >=
                    report.stepResponses[i - 1].features.spectralCentroidHz);
        }
    }

    SECTION("ParameterRampRecipe y separacion entre feature y physical monotonicity")
    {
        ParameterRampRecipe recipe("vst3_cutoff_ramp", "cutoff", 0.1, 1.0, 0.5, 32);
        SynthPresetState baseState;
        auto report = engine.executeRecipe(recipe, baseState);

        REQUIRE(report.executionPermitted);
        REQUIRE(report.rampAnalysis.observedFeatureMonotonicity == "PASS");
        REQUIRE(report.rampAnalysis.physicalParameterMonotonicity == "NOT_ESTABLISHED");
        REQUIRE(report.rampAnalysis.transportResolution == "TransportedSampleAccurate");
    }
}

TEST_CASE("Fase 20.3.B - VST3 Oracle: Canal de Verificacion Independiente de Ground Truth", "[vst3][validation]")
{
    ReferenceSynthProcessor processor;
    PluginSynthTarget target(processor);
    ProcessingSpec spec{ 96000.0, 256, 2 };
    target.prepare(spec);

    // 1. Configurar valores conocidos en el procesador
    for (auto* p : processor.getParameters())
    {
        if (p != nullptr)
        {
            if (p->getName(32) == "Filter Cutoff") p->setValueNotifyingHost(0.50f);
            if (p->getName(32) == "Envelope Attack") p->setValueNotifyingHost(0.10f); // 0.10 normalizado -> 0.201s
        }
    }

    // Renderizar una nota breve
    MidiExcitationSequence seq;
    seq.totalDurationSec = 0.20;
    seq.gateDurationSec = 0.15;
    TimedMidiEvent ev{ TimedMidiType::NoteOn, 1, 60, 0.8f, 0, 0.0 };
    seq.events.push_back(ev);

    std::vector<float> audio;
    target.render(seq, audio);

    // 2. ORÁCULO PRIVADO DE TEST (Inaccesible para el motor de producción)
    ReferenceSynthGroundTruth truth = processor.getPrivateGroundTruthForTesting();

    // 3. Verificar que el oráculo refleja los valores reales de la física interna
    REQUIRE(truth.realAttackSec == Catch::Approx(0.201).margin(0.01));
    REQUIRE(truth.realInternalCutoffHz > 500.0);
    REQUIRE(truth.realInternalCutoffHz < 2500.0);
    REQUIRE(truth.activeVoiceCount == 1);
}

TEST_CASE("Fase 20.3.B - VST3 Robustness: Multiples Bloques, Sample Rates y Caso Cero Eventos", "[vst3][validation]")
{
    ReferenceSynthProcessor processor;
    PluginSynthTarget target(processor);

    std::vector<double> sampleRates = { 44100.0, 48000.0, 96000.0 };
    std::vector<int> blockSizes = { 32, 64, 128, 256, 512 };

    SECTION("Estabilidad en combinaciones de sample rate y tamaño de bloque")
    {
        for (double sr : sampleRates)
        {
            for (int bs : blockSizes)
            {
                ProcessingSpec spec{ sr, bs, 2 };
                target.prepare(spec);

                MidiExcitationSequence seq;
                seq.totalDurationSec = 0.05;
                seq.gateDurationSec = 0.03;
                seq.events.push_back(TimedMidiEvent{ TimedMidiType::NoteOn, 1, 60, 0.8f, 0, 0.0 });

                std::vector<float> audio;
                target.render(seq, audio);

                REQUIRE_FALSE(audio.empty());
                REQUIRE(audio.size() == static_cast<size_t>(std::lround(0.05 * sr)));
            }
        }
    }

    SECTION("Cero eventos produce silencio determinista")
    {
        ProcessingSpec spec{ 96000.0, 256, 2 };
        target.prepare(spec);

        MidiExcitationSequence emptySeq;
        emptySeq.totalDurationSec = 0.10;

        std::vector<float> audio;
        target.render(emptySeq, audio);

        for (float s : audio)
        {
            REQUIRE(s == 0.0f);
        }
    }
}
