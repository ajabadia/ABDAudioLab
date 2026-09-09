#include <catch2/catch_test_macros.hpp>
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
