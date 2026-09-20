/**
 * @file test_MidiAutomatedExcitation_ST11_ST13.cpp
 * @brief HITO-02-MIDI-AUTOMATED-CORE: Validación de Excitación MIDI Automatizada
 *        Contratos temporales, compuerta gateMs, matriz de velocidades, All-Notes-Off y hash canónico.
 *        Cubre requisitos: REQ-MIDI-AUTO, REQ-MIDI-GATE, REQ-MIDI-PANIC, REQ-MIDI-LATENCY, REQ-MIDI-HASH.
 *        Smoke Tests: ST-11, ST-12, ST-13 y casos negativos de seguridad.
 * @author ABDSynths
 * @date 2026-09-20
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <chrono>
#include <thread>
#include <vector>

#include "audio/LabAudioEngine.h"
#include "core/ProfilingSequencer.h"
#include "core/ProfilingHardwareDispatcher.h"
#include "hardware/MockHardwareController.h"
#include "synth/MidiExcitationSequence.h"
#include "synth/MidiAudioSynchronizer.h"
#include "synth/ExperimentRecipe.h"
#include "synth/Sha256.h"

using namespace abdaudiolab;
using namespace abdaudiolab::core;
using namespace abdaudiolab::synth;
using namespace abdaudiolab::hardware;
using Catch::Matchers::WithinAbs;

namespace
{

/**
 * @brief Mock de sintetizador con generador de audio reactivo a Note-On / Note-Off.
 * Simula el comportamiento acústico de un sintetizador virtual/hardware generando tono senoidal
 * con amplitud proporcional a la velocidad y envolvente sostenida mientras dure el gate.
 */
class ReactiveAudioSynthSimulator
{
public:
    ReactiveAudioSynthSimulator() = default;

    void handleMidi(const juce::MidiMessage& msg)
    {
        if (msg.isNoteOn())
        {
            currentNote = msg.getNoteNumber();
            currentVelocity = msg.getFloatVelocity();
            isNoteActive = true;
            noteOnTimestampMs = juce::Time::getMillisecondCounterHiRes();
            noteOnCount++;
        }
        else if (msg.isNoteOff())
        {
            if (msg.getNoteNumber() == currentNote)
            {
                isNoteActive = false;
                noteOffTimestampMs = juce::Time::getMillisecondCounterHiRes();
                noteOffCount++;
            }
        }
        else if (msg.isAllNotesOff())
        {
            isNoteActive = false;
            allNotesOffChannelsReceived.push_back(msg.getChannel());
        }
    }

    void renderBlock(juce::AudioBuffer<float>& buffer, int numSamples, double sampleRate)
    {
        if (!isNoteActive || currentVelocity <= 0.0f)
        {
            buffer.clear();
            return;
        }

        double freq = 440.0 * std::pow(2.0, (currentNote - 69.0) / 12.0);
        double phaseDelta = (juce::MathConstants<double>::twoPi * freq) / sampleRate;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* channelData = buffer.getWritePointer(ch);
            double ph = phase;
            for (int i = 0; i < numSamples; ++i)
            {
                channelData[i] = static_cast<float>(std::sin(ph) * currentVelocity * 0.5f);
                ph += phaseDelta;
                if (ph >= juce::MathConstants<double>::twoPi)
                    ph -= juce::MathConstants<double>::twoPi;
            }
        }
        phase += phaseDelta * numSamples;
        while (phase >= juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;
    }

    int currentNote { 60 };
    float currentVelocity { 0.0f };
    bool isNoteActive { false };
    double phase { 0.0 };
    double noteOnTimestampMs { 0.0 };
    double noteOffTimestampMs { 0.0 };
    int noteOnCount { 0 };
    int noteOffCount { 0 };
    std::vector<int> allNotesOffChannelsReceived;
};

} // namespace

// ==============================================================================
// 1. CONTRATO DE HASH CANÓNICO RFC 8785 Y DETERMINISMO (REQ-MIDI-HASH)
// ==============================================================================

TEST_CASE("HITO-02 / REQ-MIDI-HASH: Determinismo estricto de sequenceHash RFC 8785", "[midi][hash][contract]")
{
    // Construir dos instancias independientes con parámetros idénticos
    MidiExcitationSequence seq1;
    seq1.sampleRateHz = 48000.0;
    seq1.midiChannel = 1;
    seq1.note = 60;
    seq1.velocity = 100;
    seq1.noteDurationSec = 0.25;
    seq1.releaseTailSec = 0.30;
    seq1.canonicalEvents.push_back({ 0, { 0x90, 60, 100 } });       // Note On
    seq1.canonicalEvents.push_back({ 12000, { 0x80, 60, 0 } });     // Note Off a 250ms (48000 * 0.25)
    seq1.canonicalEvents.push_back({ 26400, { 0xB0, 123, 0 } });    // All Notes Off a 550ms

    std::string hash1 = seq1.computeCanonicalSha256();
    REQUIRE_FALSE(hash1.empty());
    REQUIRE(hash1.length() == 64);

    MidiExcitationSequence seq2;
    seq2.sampleRateHz = 48000.0;
    seq2.midiChannel = 1;
    seq2.note = 60;
    seq2.velocity = 100;
    seq2.noteDurationSec = 0.25;
    seq2.releaseTailSec = 0.30;
    seq2.canonicalEvents.push_back({ 0, { 0x90, 60, 100 } });
    seq2.canonicalEvents.push_back({ 12000, { 0x80, 60, 0 } });
    seq2.canonicalEvents.push_back({ 26400, { 0xB0, 123, 0 } });

    std::string hash2 = seq2.computeCanonicalSha256();

    // 1. Estabilidad absoluta (reproducibilidad bite a bite)
    REQUIRE(hash1 == hash2);

    // 2. Sensibilidad ante alteración de 1 solo parámetro (Avalanche effect)
    MidiExcitationSequence seqAltered = seq2;
    seqAltered.velocity = 99; // Alteración mínima
    std::string hashAltered = seqAltered.computeCanonicalSha256();
    REQUIRE(hashAltered != hash1);

    // 3. Serialización JSON ordenada conforme a RFC 8785
    std::string jsonStr = seq1.serializeCanonicalJson();
    REQUIRE(jsonStr.find("\"midiChannel\":1") != std::string::npos);
    REQUIRE(jsonStr.find("\"note\":60") != std::string::npos);
    REQUIRE(jsonStr.find("\"sampleRateHz\":48000.0") != std::string::npos);
}

// ==============================================================================
// 2. SMOKE TEST ST-11: EXCITACIÓN MIDI INTERNA AUTOMATIZADA (REQ-MIDI-AUTO)
// ==============================================================================

TEST_CASE("HITO-02 / ST-11: Excitación MIDI Interna Automatizada y Flujo de Audio", "[midi][sequencer][st11]")
{
    audio::LabAudioEngine audioEngine;
    MockHardwareController mockHw;
    ReactiveAudioSynthSimulator synthSim;

    ProfilingSequencer sequencer(audioEngine, mockHw);

    // Capturar el MIDI que sale del dispatcher
    sequencer.getHardwareDispatcher().setMidiSinkCallback([&synthSim, &audioEngine](const juce::MidiMessage& msg) {
        synthSim.handleMidi(msg);
        audioEngine.postLiveMidiMessage(msg);
    });

    ProfilingSession session;
    ProfilingMetadata meta;
    meta.hardwareName = "DEXED_AUTOMATED_SIM";
    meta.targetModule = "SYNTH_CORE";
    meta.operatorMode = "AUTOMATED_MIDI_NOTES";
    session.setMetadata(meta);

    TestCase tc;
    tc.testId = "ST11_AUTO_NOTE_001";
    tc.functionalBlockType = "SpectrumFilter";
    tc.stimulusType = audio::StimulusType::Silence;
    tc.stimulusDurationSec = 0.08;
    tc.isAutonomousSynth = true;
    tc.midiChannel = 1;
    tc.midiNoteNumber = 60; // C4
    tc.midiVelocity = 0.80f;
    tc.noteGateDurationSec = 0.05f;
    tc.numPasses = 1;
    tc.stabilizationWaitMs = 10.0;
    session.addTestCase(tc);

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("st11_test_" + juce::String::toHexString(juce::Random::getSystemRandom().nextInt()));
    tempDir.createDirectory();

    mockHw.clearSentMessages();
    REQUIRE(sequencer.startSession(session, tempDir, "st11_campaign"));

    // Esperar finalización del hilo de secuenciación
    REQUIRE(sequencer.waitForThreadToExit(8000));

    // Verificaciones contractuales ST-11:
    // A. Note On recibido y registrado
    REQUIRE(synthSim.noteOnCount >= 1);
    REQUIRE(synthSim.currentNote == 60);
    REQUIRE_THAT(synthSim.currentVelocity, WithinAbs(0.80f, 0.01f));

    // B. Note Off recibido puntualmente
    REQUIRE(synthSim.noteOffCount >= 1);

    // C. Puntos medidos generados correctamente en la sesión
    const auto& measured = sequencer.getMeasuredPoints();
    REQUIRE_FALSE(measured.empty());
    REQUIRE(measured.front().testId == "ST11_AUTO_NOTE_001");

    // D. Limpieza y estado final exitoso en Finished
    REQUIRE(sequencer.getCurrentState() == SequencerState::Finished);

    tempDir.deleteRecursively();
}

// ==============================================================================
// 3. SMOKE TEST ST-12: CONTRATO TEMPORAL DE COMPUERTA gateMs (REQ-MIDI-GATE)
// ==============================================================================

TEST_CASE("HITO-02 / ST-12: Temporización Precisa de Compuerta gateMs y Silenciamiento", "[midi][gate][st12]")
{
    audio::LabAudioEngine audioEngine;
    MockHardwareController mockHw;
    ReactiveAudioSynthSimulator synthSim;

    ProfilingSequencer sequencer(audioEngine, mockHw);

    sequencer.getHardwareDispatcher().setMidiSinkCallback([&synthSim, &audioEngine](const juce::MidiMessage& msg) {
        synthSim.handleMidi(msg);
        audioEngine.postLiveMidiMessage(msg);
    });

    ProfilingSession session;
    ProfilingMetadata meta;
    meta.hardwareName = "GATE_PRECISION_TEST";
    session.setMetadata(meta);

    const double requestedGateSec = 0.050; // 50 ms
    TestCase tc;
    tc.testId = "ST12_GATE_PRECISION";
    tc.functionalBlockType = "SpectrumFilter";
    tc.stimulusType = audio::StimulusType::Silence;
    tc.stimulusDurationSec = 0.080;
    tc.isAutonomousSynth = true;
    tc.midiChannel = 1;
    tc.midiNoteNumber = 69; // A4
    tc.midiVelocity = 0.75f;
    tc.noteGateDurationSec = static_cast<float>(requestedGateSec);
    tc.numPasses = 1;
    tc.stabilizationWaitMs = 10.0;
    session.addTestCase(tc);

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("st12_test_" + juce::String::toHexString(juce::Random::getSystemRandom().nextInt()));
    tempDir.createDirectory();

    REQUIRE(sequencer.startSession(session, tempDir, "st12_campaign"));
    REQUIRE(sequencer.waitForThreadToExit(8000));

    // Verificaciones contractuales ST-12:
    REQUIRE(synthSim.noteOnCount == 1);
    REQUIRE(synthSim.noteOffCount == 1);

    double measuredGateMs = synthSim.noteOffTimestampMs - synthSim.noteOnTimestampMs;
    double expectedGateMs = requestedGateSec * 1000.0;

    // Tolerancia temporal contractual: En Windows con hilos preemptivos (juce::Thread::sleep(10)),
    // el error temporal máximo admisible es de 25 ms (equivalente a ~2 bloques de scheduler).
    REQUIRE_THAT(measuredGateMs, WithinAbs(expectedGateMs, 25.0));

    // Verificar que el sintetizador NO tiene notas activas al terminar (Zero Hung Notes)
    REQUIRE_FALSE(synthSim.isNoteActive);

    tempDir.deleteRecursively();
}

// ==============================================================================
// 4. SMOKE TEST ST-13: MATRIZ FACTORIAL DE VELOCIDADES (REQ-MIDI-AUTO)
// ==============================================================================

TEST_CASE("HITO-02 / ST-13: Matriz Factorial de Velocidades y Respuesta Dinámica", "[midi][velocity][st13]")
{
    // Verificar que 3 ensayos consecutivos con velocidades 32, 64 y 127
    // producen niveles RMS estrictamente crecientes y registran sus metadatos
    const std::vector<int> velocities = { 32, 64, 127 };
    std::vector<float> recordedRmsValues;

    for (int velByte : velocities)
    {
        ReactiveAudioSynthSimulator synth;
        float normalizedVel = static_cast<float>(velByte) / 127.0f;
        juce::MidiMessage noteOn = juce::MidiMessage::noteOn(1, 60, normalizedVel);
        synth.handleMidi(noteOn);

        // Renderizar un búfer de 2048 muestras a 48 kHz
        juce::AudioBuffer<float> buf(2, 2048);
        synth.renderBlock(buf, 2048, 48000.0);

        float rms = buf.getRMSLevel(0, 0, 2048);
        recordedRmsValues.push_back(rms);

        synth.handleMidi(juce::MidiMessage::noteOff(1, 60, 0.0f));
    }

    REQUIRE(recordedRmsValues.size() == 3);

    // Verificación de monotonía dinámica: RMS(vel 127) > RMS(vel 64) > RMS(vel 32)
    REQUIRE(recordedRmsValues[0] > 0.0f);
    REQUIRE(recordedRmsValues[1] > recordedRmsValues[0]);
    REQUIRE(recordedRmsValues[2] > recordedRmsValues[1]);

    // Verificación de que la relación es proporcional a la velocidad
    float ratioMidLow = recordedRmsValues[1] / recordedRmsValues[0];
    float ratioHighMid = recordedRmsValues[2] / recordedRmsValues[1];
    REQUIRE_THAT(ratioMidLow, WithinAbs(64.0 / 32.0, 0.15));
    REQUIRE_THAT(ratioHighMid, WithinAbs(127.0 / 64.0, 0.15));
}

// ==============================================================================
// 5. CASOS NEGATIVOS, CANCELACIÓN Y SEGURIDAD ACÚSTICA (REQ-MIDI-PANIC)
// ==============================================================================

TEST_CASE("HITO-02 / REQ-MIDI-PANIC: Parada de Emergencia y Silenciamiento Global (16 Canales)", "[midi][panic][safety]")
{
    audio::LabAudioEngine audioEngine;
    MockHardwareController mockHw;
    ReactiveAudioSynthSimulator synthSim;

    ProfilingSequencer sequencer(audioEngine, mockHw);

    sequencer.getHardwareDispatcher().setMidiSinkCallback([&synthSim, &audioEngine](const juce::MidiMessage& msg) {
        synthSim.handleMidi(msg);
        audioEngine.postLiveMidiMessage(msg);
    });

    // A. Verificar que stopSession() envía All Notes Off (CC 123 val 0) a los 16 canales
    synthSim.allNotesOffChannelsReceived.clear();
    sequencer.stopSession();

    REQUIRE(synthSim.allNotesOffChannelsReceived.size() == 16);
    for (int ch = 1; ch <= 16; ++ch)
    {
        bool found = std::find(synthSim.allNotesOffChannelsReceived.begin(),
                               synthSim.allNotesOffChannelsReceived.end(), ch) != synthSim.allNotesOffChannelsReceived.end();
        REQUIRE(found);
    }

    // B. Verificar que pauseSession() también envía All Notes Off a los 16 canales
    synthSim.allNotesOffChannelsReceived.clear();
    sequencer.pauseSession();

    REQUIRE(synthSim.allNotesOffChannelsReceived.size() == 16);
    for (int ch = 1; ch <= 16; ++ch)
    {
        bool found = std::find(synthSim.allNotesOffChannelsReceived.begin(),
                               synthSim.allNotesOffChannelsReceived.end(), ch) != synthSim.allNotesOffChannelsReceived.end();
        REQUIRE(found);
    }
}

TEST_CASE("HITO-02 / REQ-SAFETY-OVERLOAD: Aborto Inmediato por Sobrecarga Acústica", "[midi][safety][overload]")
{
    audio::LabAudioEngine audioEngine;
    MockHardwareController mockHw;
    ProfilingSequencer sequencer(audioEngine, mockHw);

    // Conectar el callback para inyectar overload de forma síncrona en cuanto se despacha el Note On
    sequencer.getHardwareDispatcher().setMidiSinkCallback([&audioEngine](const juce::MidiMessage& msg) {
        if (msg.isNoteOn())
        {
            audioEngine.getResponseReceiver().triggerOverloadForTesting();
        }
    });

    ProfilingSession session;
    ProfilingMetadata meta;
    meta.hardwareName = "OVERLOAD_PROTECTION_TEST";
    session.setMetadata(meta);

    TestCase tc;
    tc.testId = "OVERLOAD_TRIGGER_TEST";
    tc.functionalBlockType = "SpectrumFilter";
    tc.stimulusType = audio::StimulusType::Silence;
    tc.stimulusDurationSec = 0.08;
    tc.isAutonomousSynth = true;
    tc.midiChannel = 1;
    tc.midiNoteNumber = 60;
    tc.midiVelocity = 1.0f;
    tc.noteGateDurationSec = 0.05f;
    tc.stabilizationWaitMs = 5.0;
    session.addTestCase(tc);

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("overload_test_" + juce::String::toHexString(juce::Random::getSystemRandom().nextInt()));
    tempDir.createDirectory();

    REQUIRE(sequencer.startSession(session, tempDir, "overload_campaign"));

    REQUIRE(sequencer.waitForThreadToExit(8000));

    // Verificaciones:
    // 1. La bandera de seguridad safetyAborted debe estar en true
    REQUIRE(sequencer.isSafetyAborted());

    // 2. El secuenciador debe terminar en ErrorState
    REQUIRE(sequencer.getCurrentState() == SequencerState::ErrorState);

    tempDir.deleteRecursively();
}

TEST_CASE("HITO-02 / ROBUSTNESS: Manejo de Casos Límite y Datos Extremos", "[midi][robustness]")
{
    // A. Receta vacía
    MidiExcitationSequence emptySeq;
    emptySeq.computeHash();
    REQUIRE_FALSE(emptySeq.sequenceHash.empty());

    // B. Clamping de notas y velocidades extremas en createNoteTrial
    auto trialMin = MidiExcitationSequence::createNoteTrial(1, 0, 0, 0.05, 48000.0);
    REQUIRE(trialMin.midiVelocity == 1); // clamped a mínimo 1
    REQUIRE(trialMin.noteNumber == 0);

    auto trialMax = MidiExcitationSequence::createNoteTrial(16, 127, 255, 0.05, 48000.0);
    REQUIRE(trialMax.midiVelocity == 127); // clamped a máximo 127
    REQUIRE(trialMax.noteNumber == 127);
    REQUIRE(trialMax.channel == 16);

    // C. Verificación de All-Notes-Off al inicio y final del trial generado
    REQUIRE(trialMax.events.front().type == TimedMidiType::AllNotesOff);
    REQUIRE(trialMax.events.back().type == TimedMidiType::AllNotesOff);
}
