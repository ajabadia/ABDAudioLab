#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../gui/MatrixResolutionTableComponent.h"
#include "../gui/TestPlanEstimationCardComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>

using namespace abdaudiolab;
using namespace abdaudiolab::gui;

TEST_CASE("MatrixResolutionTableComponent - Row and Step Management", "[MatrixTable]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    MatrixResolutionTableComponent table;
    table.setSize(500, 200);

    CHECK(table.getControls().empty());
    CHECK(table.getNumRows() == 0);

    std::vector<ControlStepConfig> controls = {
        { "cutoff", "Cutoff Frequency", "Knob", 5, 10.0f, 90.0f, 0 },
        { "reso", "Resonance", "Slider", 3, 0.0f, 100.0f, 1 },
        { "drive", "Drive Saturation", "Switch", 1, 50.0f, 50.0f, 2 }
    };

    table.setControls(controls);
    CHECK(table.getControls().size() == 3);
    CHECK(table.getNumRows() == 3);
    CHECK(table.getPreferredHeight() >= 94);

    // Verify paint doesn't throw
    juce::Image canvas(juce::Image::ARGB, 500, 200, true);
    juce::Graphics g(canvas);
    CHECK_NOTHROW(table.paint(g));
}

TEST_CASE("TestPlanEstimationCardComponent - Estimation and Formatting", "[MatrixTable]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    TestPlanEstimationCardComponent card;
    card.setSize(500, 46);

    CHECK(card.getPoints() == 0);
    CHECK(card.getSeconds() == 0.0f);

    // 15 points with 0.5s = 7.5s
    card.setEstimation(15, 7.5f);
    CHECK(card.getPoints() == 15);
    CHECK(card.getSeconds() == 7.5f);

    juce::Image canvas(juce::Image::ARGB, 500, 46, true);
    juce::Graphics g(canvas);
    CHECK_NOTHROW(card.paint(g));

    // Long test (120 points, 2s each = 240s = 4m 0s)
    card.setEstimation(120, 240.0f);
    CHECK(card.getPoints() == 120);
    CHECK(card.getSeconds() == 240.0f);
    CHECK_NOTHROW(card.paint(g));
}

TEST_CASE("ProfilingTimeEstimate - Physical Transition & Multi-factor Calculations", "[ProfilingTimeEstimate]")
{
    // Caso 1 — Un control: Tone: 3
    // Estados: 3
    // Ajustes físicos consecutivos: 2
    {
        TestConfiguration conf;
        conf.burstDurationSec = 1.0f;
        conf.controls = {
            { "tone", "Tone", "Knob", 3, 0.0f, 100.0f, 0 }
        };

        auto est = conf.calculateEstimate(true);
        CHECK(est.measurementStates == 3);
        CHECK(est.manualControlAdjustmentEvents == 2);
        CHECK(est.dimensionalFormula == "3 Tone");
        CHECK(est.isManualProfiling == true);
        // Audio: 3s, manual adjustments: 2 * 2.0s = 4s, overhead: 3 * 0.5s = 1.5s -> Total: 8.5s
        CHECK(est.estimatedTotalSeconds == 8.5f);
    }

    // Caso 2 — Dos controles 2 × 2
    // Secuencia convencional: T0 L0 -> T0 L1 -> T1 L0 -> T1 L1
    // Cambios: 1 + 2 + 1 = 4 ajustes físicos
    {
        TestConfiguration conf;
        conf.burstDurationSec = 1.0f;
        conf.controls = {
            { "tone", "Tone", "Knob", 2, 0.0f, 100.0f, 0 },
            { "level", "Level", "Knob", 2, 0.0f, 100.0f, 1 }
        };

        auto est = conf.calculateEstimate(true);
        CHECK(est.measurementStates == 4);
        CHECK(est.manualControlAdjustmentEvents == 4);
        CHECK(est.dimensionalFormula == "2 Tone \u00d7 2 Level");
        // Audio: 4s + 4 * 2.0s + 4 * 0.5s = 14s
        CHECK(est.estimatedTotalSeconds == 14.0f);
    }

    // Caso 3 — Control fijo: 8 Tone × 4 Level × 1 Dist = 32
    // Dist permanece fija y nunca aporta cambios físicos
    {
        TestConfiguration conf;
        conf.burstDurationSec = 1.0f;
        conf.controls = {
            { "tone", "Tone", "Knob", 8, 0.0f, 100.0f, 0 },
            { "level", "Level", "Knob", 4, 0.0f, 100.0f, 1 },
            { "dist", "Dist", "Knob", 1, 100.0f, 100.0f, 2 }
        };

        auto est = conf.calculateEstimate(true);
        CHECK(est.measurementStates == 32);
        CHECK(est.dimensionalFormula == "8 Tone \u00d7 4 Level \u00d7 1 Dist");
        CHECK(est.manualControlAdjustmentEvents > 0);
        CHECK(est.isManualProfiling == true);
    }

    // Caso 4 — Automated (Mismo espacio 8 × 4 × 1 = 32)
    // isManualProfiling = false, manualControlAdjustmentEvents = 0, sin coste humano
    {
        TestConfiguration conf;
        conf.burstDurationSec = 1.0f;
        conf.controls = {
            { "tone", "Tone", "Knob", 8, 0.0f, 100.0f, 0 },
            { "level", "Level", "Knob", 4, 0.0f, 100.0f, 1 },
            { "dist", "Dist", "Knob", 1, 100.0f, 100.0f, 2 }
        };

        auto est = conf.calculateEstimate(false);
        CHECK(est.measurementStates == 32);
        CHECK(est.manualControlAdjustmentEvents == 0);
        CHECK(est.isManualProfiling == false);
        // Audio: 32s + 32 * 0.05s = 33.6s
        CHECK(est.estimatedTotalSeconds == Catch::Approx(33.6f));
    }
}
