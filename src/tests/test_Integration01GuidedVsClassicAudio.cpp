/**
 * @file test_Integration01GuidedVsClassicAudio.cpp
 * @brief INTEGRATION-01: Auditoría de Paridad E2E de Audio y DSP entre Guiado y Libre Sistemático.
 *
 * Ejecuta el bucle cerrado conducido por MockAudioEngine sobre el fixture sintético puro
 * (SyntheticAudioFixture), sin dispositivos WASAPI/ASIO y sin esperar por reloj: el avance
 * temporal del audio es estrictamente por muestras (t = bloque * 256).
 *
 * POR QUE NO AFIRMA IGUALDAD EXACTA DE POSICIONES NI DE DURACIONES
 * ---------------------------------------------------------------
 * El eje del audio es determinista, pero el secuenciador NO lo es, y confundirlos
 * es lo que hacia que este test dependiera del planificador del sistema:
 *
 *   - ProfilingSequencer decide CUÁNDO emitir el NoteOff comparando
 *     juce::Time::getMillisecondCounterHiRes() contra el gate planificado, y duerme
 *     10 ms entre sondeos (ProfilingSequencer.cpp:566,589).
 *   - El hilo principal solo avanza el audio cuando bombea un bloque de 256
 *     muestras (5,33 ms), y duerme 5 ms cuando el receptor todavía no está armado.
 *
 * Son dos relojes desincronizados. Dónde cae el NoteOn y el NoteOff dentro del buffer
 * capturado, y cuántos bloques llegan a bombearse, dependen del entrelazado de los dos
 * hilos del SO. Afirmar pumpedBlocks == pumpedBlocks o processedSamples ==
 * processedSamples no certificaba paridad: medía cuánto tiempo de CPU concedía el
 * sistema a cada ruta.
 *
 * Lo que este test SÍ certifica, y que es invariante del plan:
 *   - Nivel 1 (Control): ambas rutas emiten los MISMOS eventos lógicos y cada gate dura
 *     lo planificado dentro de la granularidad del sondeo.
 *   - Nivel 2 (Audio canónico): sobre la ventana alineada por el primer NoteOn, los
 *     buffers float32 son bit a bit idénticos (max diff <= 1e-7, RMSE <= 1e-7,
 *     SHA-256 idéntico), y esa ventana cubre el estímulo planificado completo, con
 *     un suelo explícito que impide que "idéntico" se cumpla comparando relleno.
 *   - Nivel 3 (Salida científica): paridad de métricas (RMS, Peak, SNR, THD, f0) y
 *     clasificación formal.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <filesystem>

#include "support/MockAudioEngine.h"
#include "core/ProfilingSequencer.h"
#include "hardware/MockHardwareController.h"
#include "audio/LabAudioEngine.h"
#include "gui/session/ProfilingSessionContracts.h"
#include "gui/session/ProfilingSessionController.h"
#include "measurement/MeasurementSessionContracts.h"

using namespace abdaudiolab;
using namespace abdaudiolab::core;
using namespace abdaudiolab::gui::session;
using namespace abdaudiolab::test::support;

namespace
{

struct TemporaryTestDirectory
{
    std::filesystem::path path;

    explicit TemporaryTestDirectory(const std::string& prefix)
    {
        auto tempRoot = std::filesystem::temp_directory_path();
        path = tempRoot / (prefix + "_" + std::to_string(juce::Random::getSystemRandom().nextInt()));
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path, ec);
    }

    ~TemporaryTestDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    TemporaryTestDirectory(const TemporaryTestDirectory&) = delete;
    TemporaryTestDirectory& operator=(const TemporaryTestDirectory&) = delete;
};

TargetSelectionState createCanonicalTarget()
{
    TargetSelectionState t;
    t.targetId = "synthetic_fixture_integration01";
    t.targetName = "SyntheticAudioFixture (Pure Sine C4 Test Witness)";
    t.manufacturer = "ABDAudioLab";
    t.version = "1.0.0";
    t.kind = TargetKind::SyntheticFixture;
    t.isConnected = true;
    t.isDeterministic = true;
    t.availableDomainDescription = "Notas MIDI C1-C6, Vel 1-127";
    t.parameterCount = 8;
    return t;
}

ProfilingSession createCanonicalBenchmarkPlan()
{
    ProfilingSession session;
    ProfilingMetadata meta;
    meta.hardwareName = "SyntheticAudioFixture";
    meta.targetModule = "SineOscillator";
    meta.operatorMode = "AUTOMATED_MIDI_NOTES";
    session.setMetadata(meta);

    // Caso Testigo Controlado: C4 (Nota 60), Velocity 64 (0.50f), Gate 250ms, Settling 50ms, 3 repeticiones
    for (int rep = 0; rep < 3; ++rep)
    {
        TestCase tc;
        tc.testId = "INTEG01_C4_REP_" + std::to_string(rep + 1);
        tc.functionalBlockType = "SpectrumFilter";
        tc.stimulusType = audio::StimulusType::Silence;
        tc.stimulusDurationSec = 0.35;
        tc.isAutonomousSynth = true;
        tc.midiChannel = 1;
        tc.midiNoteNumber = 60; // C4
        tc.midiVelocity = 0.50f; // Velocity 64 aprox
        tc.noteGateDurationSec = 0.25f; // 250 ms
        tc.stabilizationWaitMs = 50.0;  // 50 ms settling
        tc.numPasses = 1;
        session.addTestCase(tc);
    }

    return session;
}

struct RouteExecutionResult
{
    std::string routeName;
    bool sessionCompleted { false };
    int pumpedBlocks { 0 };
    int64_t processedSamples { 0 };
    std::string stopReason;
    std::string diagnosticReport;

    std::vector<RecordedMidiEvent> midiTrace;
    std::vector<float> capturedAudioL;
    std::vector<float> capturedAudioR;
    std::string canonicalAudioSha256;

    std::vector<exporting::MeasuredPoint> measuredPoints;
    ExperimentKind deducedKind { ExperimentKind::Exploration };
};

// ===========================================================================
// Invariantes del plan, no del planificador
// ===========================================================================

/** @brief Tasa del fixture; coincide con la que prepara MockAudioEngine. */
constexpr double kIntegrationSampleRate = 48000.0;

/** @brief Repeticiones del plan canonico de INTEGRATION-01. */
constexpr std::size_t kPlannedRepetitions = 3;

/** @brief Duracion de gate planificada por repeticion (250 ms). */
constexpr double kPlannedGateSeconds = 0.25;

/**
 * @brief Duracion de estimulo planificada por repeticion (350 ms = gate + settling).
 *
 * Es la constante que fija el suelo de la ventana de audio del Nivel 2: el
 * oscilador del fixture solo suena dentro del gate, asi que una ventana mas corta
 * que el estimulo planificado no puede contener las tres repeticiones por muy
 * identica que sea.
 */
constexpr double kPlannedStimulusSeconds = 0.35;

/** @brief Nota y canal del plan canonico: C4 (60) en el canal 1. */
constexpr int kPlannedMidiNote = 60;
constexpr int kPlannedMidiChannel = 1;
constexpr float kPlannedMidiVelocity = 0.50f;

/**
 * @brief Margen que se concede al planificador, en segundos.
 *
 * Se deriva de las constantes reales del harness y no de un numero redondo:
 * un ciclo del sondeo del worker (Thread::sleep(10) ms), mas un bloque de audio
 * (256/48000 = 5,33 ms) por la cuantizacion a bloque, mas el reposo del bombeo
 * principal (Thread::sleep(5) ms). Suman 20,33 ms y se redondea a 25 ms.
 *
 * Cualquier diferencia MAYOR ya no es planificador: es un fallo de plan, que es
 * justo lo que el margen no debe tapar.
 */
constexpr double kSchedulerToleranceSeconds = 0.025;

/**
 * @brief Un NoteOn y su NoteOff emparejados, en muestras del fixture.
 */
struct GateWindow
{
    int64_t noteOnOffset { 0 };
    int64_t noteOffOffset { 0 };

    [[nodiscard]] int64_t durationSamples() const noexcept
    {
        return noteOffOffset - noteOnOffset;
    }

    [[nodiscard]] double durationSeconds() const noexcept
    {
        return static_cast<double> (durationSamples()) / kIntegrationSampleRate;
    }
};

/**
 * @brief Empareja cada NoteOn con el NoteOff que lo sigue, en orden de aparición.
 *
 * El apareamiento es POR SECUENCIA, no por proximidad temporal: comparar "el
 * NoteOff más cercano en el tiempo" sería reintroducir el reloj de pared en un test
 * que precisamente viene de salirse de él.
 */
[[nodiscard]] std::vector<GateWindow> extractGateWindows (const std::vector<RecordedMidiEvent>& trace) noexcept
{
    std::vector<GateWindow> windows;
    bool pendingOn = false;
    int64_t pendingOffset = 0;

    for (const auto& ev : trace)
    {
        if (ev.isNoteOn)
        {
            pendingOn = true;
            pendingOffset = ev.sampleOffset;
        }
        else if (ev.isNoteOff && pendingOn)
        {
            windows.push_back (GateWindow { pendingOffset, ev.sampleOffset });
            pendingOn = false;
        }
    }

    return windows;
}

/** @brief Porción [offset, offset + count) del buffer, acotada a lo que existe. */
[[nodiscard]] std::vector<float> sliceFrom (const std::vector<float>& buffer,
                                            std::size_t offset,
                                            std::size_t count)
{
    if (offset >= buffer.size())
        return {};

    const auto available = buffer.size() - offset;

    return std::vector<float> (buffer.begin() + static_cast<std::ptrdiff_t> (offset),
                               buffer.begin() + static_cast<std::ptrdiff_t> (offset + std::min (available, count)));
}

RouteExecutionResult executeRoute(const std::string& routeName, UiWorkflowMode workflowMode)
{
    RouteExecutionResult result;
    result.routeName = routeName;

    TemporaryTestDirectory runDir("integ01_" + routeName);
    juce::File exportDirectory(runDir.path.string());

    // 1. Instanciar fixture sintético determinista
    SyntheticAudioFixture syntheticFixture;
    syntheticFixture.prepareToPlay(48000.0, 256);

    // 2. Instanciar motor de audio de laboratorio y conectar fixture como plugin activo
    audio::LabAudioEngine audioEngine;
    audioEngine.setActivePluginInstance(&syntheticFixture, 48000.0, 256);

    // 3. Hardware automatizado de prueba y secuenciador
    hardware::MockHardwareController mockHw;
    ProfilingSequencer sequencer(audioEngine, mockHw);

    // 4. Preparar MockAudioEngine conductor de callbacks
    MockAudioEngine mockEngine(audioEngine);
    mockEngine.prepare(48000.0, 256, 2);

    // 5. Configurar sesión y plan canónico común
    TargetSelectionState target = createCanonicalTarget();
    ProfilingSession session = createCanonicalBenchmarkPlan();

    // 6. Lanzar sesión
    bool started = sequencer.startSession(session, exportDirectory, "integ01_" + juce::String(routeName));
    REQUIRE(started);

    // 7. Diagnóstico para pumpUntil
    auto diagCallback = [&]() -> std::string {
        std::string d;
        d += "    route:          " + routeName + "\n";
        d += "    sequencerState: " + std::to_string(static_cast<int>(sequencer.getCurrentState())) + "\n";
        d += "    receiverState:  " + std::to_string(static_cast<int>(audioEngine.getResponseReceiver().getState())) + "\n";
        d += "    capturedSamples:" + std::to_string(mockEngine.getProcessedSampleCount()) + "\n";
        d += "    measuredCount:  " + std::to_string(sequencer.getMeasuredPoints().size()) + "\n";
        return d;
    };

    // 8. Bombear callbacks hasta completar
    auto pumpRes = mockEngine.pumpUntil([&]() {
        return sequencer.getCurrentState() == SequencerState::Finished ||
               sequencer.getCurrentState() == SequencerState::ErrorState ||
               !sequencer.isRunningSession();
    }, 5000, diagCallback);

    // Esperar a que el hilo del secuenciador cierre de forma limpia
    sequencer.waitForThreadToExit(2000);
    // Purgar último bloque para procesar eventos pendientes en la cola MIDI (NoteOff / AllNotesOff)
    mockEngine.pumpOneBlock();

    result.sessionCompleted = pumpRes.success && (sequencer.getCurrentState() == SequencerState::Finished);
    result.pumpedBlocks = pumpRes.pumpedBlocks + 1;
    result.processedSamples = pumpRes.processedSamples + 256;
    result.stopReason = pumpRes.stopReason;
    result.diagnosticReport = pumpRes.diagnosticReport;

    // 9. Recolectar artefactos de la ejecución
    result.midiTrace = syntheticFixture.getMidiTrace();
    result.capturedAudioL = mockEngine.getCapturedOutputL();
    result.capturedAudioR = mockEngine.getCapturedOutputR();
    result.canonicalAudioSha256 = MockAudioEngine::computeCanonicalBufferSha256(result.capturedAudioL);
    result.measuredPoints = sequencer.getMeasuredPoints();

    // 10. Evaluar clasificación formal resultante
    ProfilingSessionSnapshot snap;
    snap.target = target;
    snap.sessionStatus = ProfilingSessionStatus::Completed;
    snap.workflowMode = workflowMode;
    ExcitationRecipeState recipe;
    recipe.status = RecipeStatus::Valid;
    recipe.isValid = true;
    snap.excitation = recipe;
    snap.calibration.digital.verified = true;
    result.deducedKind = deduceExperimentKindFromSnapshot(snap);

    // 11. Desconectar fixture
    audioEngine.setActivePluginInstance(nullptr);

    return result;
}

} // namespace

// ===========================================================================
// INTEGRATION-01: Auditoría de Paridad E2E de Audio y DSP
// ===========================================================================

TEST_CASE("INTEGRATION-01: Guiado Sistemático vs. Libre Sistemático - Paridad E2E de Audio y DSP", "[integration][integration-01]")
{
    // Ejecutar Ruta GS (Guiado Sistemático)
    RouteExecutionResult gs = executeRoute("GS", UiWorkflowMode::Guided);

    // Ejecutar Ruta LS (Libre Sistemático)
    RouteExecutionResult ls = executeRoute("LS", UiWorkflowMode::Classic);

    // -----------------------------------------------------------------------
    // DIAGNÓSTICO PREVIO: Ambas rutas deben completar sin timeout
    // -----------------------------------------------------------------------
    INFO("GS Stop Reason: " << gs.stopReason);
    INFO("GS Diagnostic:  " << gs.diagnosticReport);
    REQUIRE(gs.sessionCompleted);

    INFO("LS Stop Reason: " << ls.stopReason);
    INFO("LS Diagnostic:  " << ls.diagnosticReport);
    REQUIRE(ls.sessionCompleted);

    // Las ventanas de nota se extraen una vez y las reutilizan los tres niveles.
    // El emparejamiento es por secuencia, no por reloj: es lo que hace que las
    // comparaciones siguientes sean invariantes del plan.
    const auto gsGates = extractGateWindows(gs.midiTrace);
    const auto lsGates = extractGateWindows(ls.midiTrace);

    // -----------------------------------------------------------------------
    // NIVEL 1: CONTROL (D5 - Estímulo y Eventos Efectivos)
    // -----------------------------------------------------------------------
    SECTION("Nivel 1: Control - Plan ejecutado y paridad lógica de la traza MIDI")
    {
        // pumpedBlocks y processedSamples NO se comparan entre rutas, y quitarlos
        // no es relajar el test: es dejar de medir lo único que nunca estuvo bajo
        // control del test. Son cuántos bloques llegó a bombear el hilo principal,
        // y eso depende de cuánto CPU concediera el SO y de si el bombeo cayó en la
        // rama de reposo de 5 ms. Afirmar que coinciden era pedirle al sistema
        // operativo que colocase dos ejecuciones en el mismo instante.
        //
        // Lo que sí se afirma del bombeo es que ambas rutas avanzaron audio:
        // un 0 aquí significaría que el comparador de audio de más abajo compararía
        // dos buffers vacíos y daría verde.
        REQUIRE(gs.pumpedBlocks > 0);
        REQUIRE(ls.pumpedBlocks > 0);

        // El plan tiene 3 repeticiones y ambas sesiones llegaron a Finished (lo
        // que REQUIRE(sessionCompleted) ya garantiza), así que cada ruta debe
        // haber medido exactamente 3 puntos. Esto SÍ es una invariante del plan:
        // depende de lo que se le pidió ejecutar, no de cuándo lo ejecutó.
        REQUIRE(gs.measuredPoints.size() == kPlannedRepetitions);
        REQUIRE(ls.measuredPoints.size() == kPlannedRepetitions);

        // Un NoteOn y su NoteOff por repetición, en las dos rutas.
        REQUIRE(gsGates.size() == kPlannedRepetitions);
        REQUIRE(lsGates.size() == kPlannedRepetitions);

        // La duración del gate se acerca a la planificada DENTRO de la
        // granularidad del sondeo. Afirmar las 12000 muestras exactas sería
        // afirmar que el SO despertó del Thread::sleep(10) justo en el milisegundo
        // previsto, que no es una propiedad del código bajo prueba.
        for (const auto& gate : gsGates)
            CHECK(gate.durationSeconds() == Catch::Approx(kPlannedGateSeconds)
                                           .margin(kSchedulerToleranceSeconds));

        for (const auto& gate : lsGates)
            CHECK(gate.durationSeconds() == Catch::Approx(kPlannedGateSeconds)
                                           .margin(kSchedulerToleranceSeconds));

        // Paridad lógica: los mismos eventos, en el mismo orden y con los mismos
        // valores. RecordedMidiEvent::matches() compara exactamente esos campos y
        // NO el sampleOffset, que es lo único que el planificador mueve.
        REQUIRE(gs.midiTrace.size() == ls.midiTrace.size());

        for (std::size_t i = 0; i < gs.midiTrace.size(); ++i)
        {
            INFO("Evento MIDI #" << i << " GS: canal=" << gs.midiTrace[i].channel
                                 << " nota=" << gs.midiTrace[i].noteNumber
                                 << " vel=" << gs.midiTrace[i].velocity
                                 << " on=" << gs.midiTrace[i].isNoteOn
                                 << " off=" << gs.midiTrace[i].isNoteOff
                                 << " offset=" << gs.midiTrace[i].sampleOffset);
            CHECK(gs.midiTrace[i].matches(ls.midiTrace[i]));
        }

        // Y cada NoteOn es el del plan canónico: C4 (60), canal 1, velocity 0.50.
        // Esta es la invariante más fuerte del nivel 1 porque no compara nada: se
        // apoya solo en las constantes del plan, así que no puede depender ni del
        // planificador ni de la otra ruta.
        for (const auto* trace : { &gs.midiTrace, &ls.midiTrace })
        {
            for (const auto& ev : *trace)
            {
                if (! ev.isNoteOn)
                    continue;

                CHECK(ev.channel == kPlannedMidiChannel);
                CHECK(ev.noteNumber == kPlannedMidiNote);
                CHECK(ev.velocity == Catch::Approx(kPlannedMidiVelocity).margin(1e-5f));
            }
        }
    }

    // -----------------------------------------------------------------------
    // NIVEL 2: AUDIO CANÓNICO (D6 - Audio Observado Idéntico)
    // -----------------------------------------------------------------------
    SECTION("Nivel 2: Audio canónico - Bit a bit idéntico sobre la ventana alineada")
    {
        REQUIRE_FALSE(gs.capturedAudioL.empty());
        REQUIRE_FALSE(ls.capturedAudioL.empty());

        // Los dos canales se capturan en el mismo callback, así que su longitud
        // es una propiedad del bloque, no del reloj.
        REQUIRE(gs.capturedAudioR.size() == gs.capturedAudioL.size());
        REQUIRE(ls.capturedAudioR.size() == ls.capturedAudioL.size());

        REQUIRE(gsGates.size() == kPlannedRepetitions);
        REQUIRE(lsGates.size() == kPlannedRepetitions);

        // Alineación. El audio empieza a ser comparable cuando suena la primera
        // nota, y ese instante lo fija el NoteOn. Las muestras anteriores son
        // silencio de relleno cuya longitud depende de cuándo el bombeo salió del
        // reposo, que es exactamente el grado de libertad que el planificador
        // controla. Comparar desde 0 sería comparar dos señales desalineadas y
        // obtener un maxDiff del orden de la amplitud en vez de 1e-7.
        const auto startGS = static_cast<std::size_t> (std::max<int64_t> (gsGates.front().noteOnOffset, 0));
        const auto startLS = static_cast<std::size_t> (std::max<int64_t> (lsGates.front().noteOnOffset, 0));

        REQUIRE(startGS < gs.capturedAudioL.size());
        REQUIRE(startLS < ls.capturedAudioL.size());

        // La ventana comparable es la que ambas rutas tienen por delante. Ahora sí
        // tiene sentido que sean distintas: el planificador puede cortar el
        // bombeo en bloques distintos y eso ya no es materia de este test.
        const auto windowSamples = std::min (gs.capturedAudioL.size() - startGS,
                                             ls.capturedAudioL.size() - startLS);

        // SUELO DE LA VENTANA. Sin él, relajar la igualdad de longitudes convertiría
        // la comprobación en un cliché: comparar solo el prefijo común da
        // bit-identidad aunque las dos rutas no hayan ejecutado nada, porque el
        // silencio también es bit-idéntico a sí mismo.
        //
        // El suelo es el ESTÍMULO PLANIFICADO completo (3 x 350 ms = 50400
        // muestras), no una fracción del gate: es lo mínimo para que "las tres
        // repeticiones suenan idénticas" signifique algo. El margen del planificador
        // no se resta aquí porque no aplica: lo que se mide es audio bombeado, que
        // avanza por muestras y no por reloj de pared.
        const auto requiredSamples = static_cast<std::size_t> (
            static_cast<double> (kPlannedRepetitions)
                * kPlannedStimulusSeconds
                * kIntegrationSampleRate);

        // Que el suelo sea alcanzable es cosa del harness, no suerte: cada
        // repetición arma una captura de (estimulo + 0,3 s) = 31200 muestras
        // (ProfilingSequencer.cpp:541), asi que tres repeticiones completadas
        // garantizan 93600 muestras bombeadas. El suelo queda por debajo de eso.
        INFO("Ventana comparada: " << windowSamples << " muestras; suelo requerido: "
             << requiredSamples);
        INFO("Offset del primer NoteOn: GS=" << startGS << " LS=" << startLS);
        REQUIRE(windowSamples >= requiredSamples);

        const auto gsWindowL = sliceFrom (gs.capturedAudioL, startGS, windowSamples);
        const auto lsWindowL = sliceFrom (ls.capturedAudioL, startLS, windowSamples);
        const auto gsWindowR = sliceFrom (gs.capturedAudioR, startGS, windowSamples);
        const auto lsWindowR = sliceFrom (ls.capturedAudioR, startLS, windowSamples);

        // Anti-vacuidad del contenido: la ventana tiene que contener señal. Un pico
        // por debajo del suelo significaría que se comparó relleno, y el maxDiff 0
        // no certificaría nada.
        const auto windowPeakDbfs = MockAudioEngine::computePeakDbfs (gsWindowL);

        INFO("Pico de la ventana izquierda (dBFS): " << windowPeakDbfs);
        CHECK(windowPeakDbfs > -60.0f);

        // Máxima diferencia absoluta muestra a muestra <= 1e-7
        const auto maxDiffL = MockAudioEngine::computeMaxAbsoluteDifference (gsWindowL, lsWindowL);
        const auto maxDiffR = MockAudioEngine::computeMaxAbsoluteDifference (gsWindowR, lsWindowR);

        INFO("Max Absolute Difference Canal L: " << maxDiffL);
        INFO("Max Absolute Difference Canal R: " << maxDiffR);
        CHECK(maxDiffL <= 1e-7f);
        CHECK(maxDiffR <= 1e-7f);

        // Error cuadrático medio (RMSE) <= 1e-7
        const auto rmseL = MockAudioEngine::computeRmse (gsWindowL, lsWindowL);
        const auto rmseR = MockAudioEngine::computeRmse (gsWindowR, lsWindowR);

        INFO("RMSE Canal L: " << rmseL);
        INFO("RMSE Canal R: " << rmseR);
        CHECK(rmseL <= 1e-7f);
        CHECK(rmseR <= 1e-7f);

        // SHA-256 canónico idéntico sobre bytes IEEE-754 float32 little-endian de
        // la ventana alineada. Es el hash del buffer COMPLETO el que ya no se
        // afirma: incluiría el relleno de silencio previo al NoteOn, cuya longitud
        // es un artefacto del planificador, y dos buffers idénticos en contenido
        // con distinto relleno no pueden dar el mismo hash.
        const auto gsWindowSha = MockAudioEngine::computeCanonicalBufferSha256 (gsWindowL);
        const auto lsWindowSha = MockAudioEngine::computeCanonicalBufferSha256 (lsWindowL);

        INFO("SHA-256 de la ventana GS: " << gsWindowSha);
        INFO("SHA-256 de la ventana LS: " << lsWindowSha);

        // Los hash del buffer completo se conservan como diagnostico: si difieren
        // es lo esperable, y ver por cuanto es justo lo que hace un fallo legible.
        INFO("SHA-256 del buffer completo GS (diagnostico, no se afirma): "
             << gs.canonicalAudioSha256);
        INFO("SHA-256 del buffer completo LS (diagnostico, no se afirma): "
             << ls.canonicalAudioSha256);

        CHECK(gsWindowSha == lsWindowSha);
    }

    // -----------------------------------------------------------------------
    // NIVEL 3: SALIDA CIENTÍFICA (D7 - Métricas DSP y Clasificación Metrológica)
    // -----------------------------------------------------------------------     SECTION("Nivel 3: Salida científica - Métricas DSP y clasificación formal idénticas")
     {
        // El bucle de abajo indexa ls.measuredPoints con el índice de gs, así que
        // el REQUIRE va AQUÍ y no en el Nivel 1: cada SECTION vuelve a ejecutar el
        // cuerpo del TEST_CASE, y si el corte se queda en otro sitio el Nivel 3
        // se ejecuta con la garantía sin comprobar y lee fuera de rango.
        REQUIRE(gs.measuredPoints.size() == ls.measuredPoints.size());

        for (size_t i = 0; i < gs.measuredPoints.size(); ++i)
        {
            INFO("Punto medido #" << i);
            const auto& ptGS = gs.measuredPoints[i];
            const auto& ptLS = ls.measuredPoints[i];

            CHECK(ptGS.testId == ptLS.testId);
            CHECK(ptGS.blockType == ptLS.blockType);
            CHECK(ptGS.stimulusType == ptLS.stimulusType);

            // THD y SNR calculados por el motor analítico
            CHECK(ptGS.thdPercent == Catch::Approx(ptLS.thdPercent).margin(1e-5f));
            CHECK(ptGS.snrDb == Catch::Approx(ptLS.snrDb).margin(1e-4f));

            // Mu / Sigma (Peak y desviación)
            CHECK(ptGS.muSigmaValue.mean == Catch::Approx(ptLS.muSigmaValue.mean).margin(1e-5f));
            CHECK(ptGS.muSigmaValue.stdDev == Catch::Approx(ptLS.muSigmaValue.stdDev).margin(1e-5f));
        }

        // Clasificación formal: Ambos deben converger en Measurement (HITO-CONVERGENCIA-01)
        CHECK(gs.deducedKind == ExperimentKind::Measurement);
        CHECK(ls.deducedKind == ExperimentKind::Measurement);
    }
}
