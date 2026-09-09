#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/ProfilingSequencer.h"
#include "audio/LabAudioEngine.h"
#include "hardware/MockHardwareController.h"

using namespace abdaudiolab;
using namespace abdaudiolab::core;
using namespace abdaudiolab::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("ProfilingSequencer: runUniversalModulationProbe executes 4 bursts and populates profile", "[sequencer_modulation][core]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    audio::LabAudioEngine audioEngine;
    hardware::MockHardwareController mockHardware;
    
    ProfilingSequencer sequencer(audioEngine, mockHardware);

    // 1. Configurar Contrato de Prueba de Modulación
    ProfilingSequencer::ModulationProbeContract contract;
    contract.excitationType = ProfilingSequencer::ModExcitationType::Velocity;
    contract.sourceID = 1; // Velocity
    contract.destID = 4;   // Filter Cutoff
    contract.midiChannel = 1;
    contract.settlingDelayMs = 5; // Retardo mínimo para test rápido
    contract.destinationBlockType = "SpectrumFilter";

    // 2. Crear buffer de reposo base simulado (nivel bajo en reposo)
    std::vector<float> restingAudio(1024, 0.01f);

    // 3. Callback de notificación UI
    bool callbackInvoked = false;
    math::ModulationNode capturedNodeFromCb;
    sequencer.setModulationNodeMeasuredCallback([&](const math::ModulationNode& node) {
        callbackInvoked = true;
        capturedNodeFromCb = node;
    });

    // 4. Ejecutar la prueba agnóstica de modulación
    math::ModulationMatrixProfile profile;
    bool result = sequencer.runUniversalModulationProbe(contract, restingAudio, profile);

    REQUIRE(result);
    REQUIRE(profile.hasNode(1, 4));

    auto node = profile.getNode(1, 4);
    REQUIRE(node.sourceID == 1);
    REQUIRE(node.destID == 4);
    REQUIRE(node.probePoints.size() == 4);

    // Validar valores de inyección {32, 64, 96, 127}
    REQUIRE(node.probePoints[0].xInjected == 32.0f);
    REQUIRE(node.probePoints[1].xInjected == 64.0f);
    REQUIRE(node.probePoints[2].xInjected == 96.0f);
    REQUIRE(node.probePoints[3].xInjected == 127.0f);

    // El MockHardwareController debe haber recibido los eventos NoteOn
    REQUIRE(mockHardware.getSentMessages().size() >= 4);

    REQUIRE(callbackInvoked);
    REQUIRE(capturedNodeFromCb.sourceID == 1);
    REQUIRE(capturedNodeFromCb.destID == 4);
}
