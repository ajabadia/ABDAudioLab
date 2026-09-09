#include <catch2/catch_test_macros.hpp>
#include "core/AutoTestPresetEngine.h"
#include "core/presets/CasioCzPresetCatalog.h"

using namespace abdaudiolab;
using namespace abdaudiolab::core;

TEST_CASE("CasioCzProfilingRecipes - Taxonomy, Timing and SysEx Coverage", "[core][presets][cz]")
{
    auto czPresets = presets::CasioCzPresetCatalog::getPresets();
    REQUIRE(czPresets.size() == 3);

    SECTION("1. Casio CZ DCW Phase Distortion Preset Specifications")
    {
        auto dcw = AutoTestPresetEngine::getPresetById("casio_cz_dcw_waveshaper");
        REQUIRE(dcw.typologyId == "casio_cz_dcw_waveshaper");
        CHECK(dcw.displayName == "Casio CZ DCW Phase Distortion");
        CHECK(dcw.category == ComponentCategory::FiltersAndToneShapers);
        CHECK(dcw.badgeText == "CZ-DCW");
        CHECK(dcw.algorithmName == "Phase Distortion Resonant Knee Fit + Chebyshev Harmonic Spectrum");
        CHECK(dcw.captureMode == "SYNCHRONOUS_SPECTRUM_SWEEP");
        CHECK(dcw.stimulusType == audio::StimulusType::SineWave1kHz);
        CHECK(dcw.burstDurationSec == 1.5f);
        CHECK(dcw.settlingWaitMs == 50.0f);
        CHECK(dcw.preRollMs == 50.0f);
        CHECK(dcw.recommendedFftSize == 16384);
        CHECK(dcw.recommendedSamples == 96000);
        CHECK(dcw.recommendedSteps == 100);

        // Validar receta y acciones SysEx de inicialización del chip NZ-1
        const auto& recipe = dcw.recipe;
        CHECK(recipe.recipeType == "DIRECT_AUDIO_IN");
        CHECK(recipe.postSettlingDelayMs == 50);
        REQUIRE(recipe.setupActions.size() == 3);

        // Acción 1: INITIALIZE_CZ_DCO1
        CHECK(recipe.setupActions[0].description == "INITIALIZE_CZ_DCO1");
        CHECK(recipe.setupActions[0].method == HardwareMethod::SYSEX_RAW);
        CHECK(recipe.setupActions[0].sysexHexPayload == "F0 44 00 00 00 01 00 00 F7");

        // Acción 2: DISABLE_VIBRATO
        CHECK(recipe.setupActions[1].description == "DISABLE_VIBRATO");
        CHECK(recipe.setupActions[1].method == HardwareMethod::SYSEX_RAW);
        CHECK(recipe.setupActions[1].sysexHexPayload == "F0 44 00 00 00 00 00 00 F7");

        // Acción 3: MAX_DCA1_SUSTAIN (Val 99 = 0x63 -> MSN=6, LSN=3)
        CHECK(recipe.setupActions[2].description == "MAX_DCA1_SUSTAIN");
        CHECK(recipe.setupActions[2].method == HardwareMethod::SYSEX_RAW);
        CHECK(recipe.setupActions[2].sysexHexPayload == "F0 44 00 00 30 0F 06 03 F7");
    }

    SECTION("2. Casio CZ Multi-Step 8-Stage Envelope Preset Specifications")
    {
        auto env = AutoTestPresetEngine::getPresetById("casio_cz_multistep_envelope");
        REQUIRE(env.typologyId == "casio_cz_multistep_envelope");
        CHECK(env.displayName == "Casio CZ Multi-Step 8-Stage Envelope");
        CHECK(env.category == ComponentCategory::ModulatorsAndEnvelopes);
        CHECK(env.badgeText == "CZ-ENV");
        CHECK(env.algorithmName == "Multi-Step 8-Stage Inverse Logarithmic Segment Calibration");
        CHECK(env.captureMode == "ADAPTIVE_ENVELOPE");
        CHECK(env.stimulusType == audio::StimulusType::SyncPulses3);
        CHECK(env.burstDurationSec == 5.0f);
        CHECK(env.settlingWaitMs == 20.0f);
        CHECK(env.silenceThresholdDb == -60.0f);
        CHECK(env.recommendedSteps == 8);
        CHECK(env.recommendedSamples == 96000);
        CHECK(env.recommendedFftSize == 4096);
    }

    SECTION("3. Casio CZ DCO Tuning and Phase Preset Specifications")
    {
        auto dco = AutoTestPresetEngine::getPresetById("casio_cz_dco_phase_octave");
        REQUIRE(dco.typologyId == "casio_cz_dco_phase_octave");
        CHECK(dco.displayName == "Casio CZ DCO Tuning & Phase");
        CHECK(dco.category == ComponentCategory::OscillatorsAndGenerators);
        CHECK(dco.badgeText == "CZ-DCO");
        CHECK(dco.algorithmName == "YIN Fundamental Tracking & Octave Duty-Cycle Phase Analysis");
        CHECK(dco.captureMode == "AUTONOMOUS_PITCH_DETECTION");
        CHECK(dco.stimulusType == audio::StimulusType::Silence);
        CHECK(dco.burstDurationSec == 2.0f);
        CHECK(dco.settlingWaitMs == 10.0f);
        CHECK(dco.recommendedSteps == 12);
        CHECK(dco.recommendedSamples == 96000);
        CHECK(dco.recommendedFftSize == 16384);
    }
}
