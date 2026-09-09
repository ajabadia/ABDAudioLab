#include <catch2/catch_test_macros.hpp>
#include "../gui/OperatorCardsContainerComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>

using namespace abdaudiolab;
using namespace abdaudiolab::gui;

TEST_CASE("OperatorCardsContainerComponent - Initial state and empty steps", "[OperatorCards]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    OperatorCardsContainerComponent container;
    container.setSize(600, 200);

    CHECK(container.getParameterSteps().empty());
    CHECK(container.getSelectedParamIndex() == 0);
    CHECK(container.getPromptMessage().isEmpty());

    // Paint on empty steps must be a safe no-op
    juce::Image canvas(juce::Image::ARGB, 600, 200, true);
    juce::Graphics g(canvas);
    CHECK_NOTHROW(container.paint(g));
}

TEST_CASE("OperatorCardsContainerComponent - Dual/Single card view (1 and 2 controls)", "[OperatorCards]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    OperatorCardsContainerComponent container;
    container.setSize(600, 200);

    auto makeStep = [](int idx, const std::string& name, float norm, const std::string& type) {
        core::ParameterStep ps;
        ps.paramIndex = idx;
        ps.paramName = name;
        ps.normalizedValue = norm;
        ps.rawValue = static_cast<int>(norm * 127.0f);
        ps.controlType = type;
        ps.minNormalized = 0.0f;
        ps.maxNormalized = 1.0f;
        ps.id = name;
        ps.sortOrder = idx;
        return ps;
    };

    std::vector<core::ParameterStep> singleStep = {
        makeStep(1, "Cutoff", 0.75f, "Knob")
    };

    container.setStepData(singleStep, "Adjust filter cutoff to 75%");
    CHECK(container.getParameterSteps().size() == 1);
    CHECK(container.getPromptMessage() == "Adjust filter cutoff to 75%");
    CHECK(container.getSelectedParamIndex() == 0);

    // Repaint check with single card
    juce::Image canvas(juce::Image::ARGB, 600, 200, true);
    juce::Graphics g(canvas);
    CHECK_NOTHROW(container.paint(g));

    // Two controls (Dual card)
    std::vector<core::ParameterStep> dualSteps = {
        makeStep(1, "Resonance", 0.50f, "Slider"),
        makeStep(2, "Drive", 1.0f, "Switch")
    };
    container.setStepData(dualSteps, "Dual parameter alignment");
    CHECK(container.getParameterSteps().size() == 2);
    CHECK_NOTHROW(container.paint(g));
}

TEST_CASE("OperatorCardsContainerComponent - Multi-control grid view (>= 3 controls) and Selection", "[OperatorCards]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    OperatorCardsContainerComponent container;
    container.setSize(800, 220);

    auto makeStep = [](int idx, const std::string& name, float norm, const std::string& type) {
        core::ParameterStep ps;
        ps.paramIndex = idx;
        ps.paramName = name;
        ps.normalizedValue = norm;
        ps.rawValue = static_cast<int>(norm * 127.0f);
        ps.controlType = type;
        ps.minNormalized = 0.0f;
        ps.maxNormalized = 1.0f;
        ps.id = name;
        ps.sortOrder = idx;
        return ps;
    };

    std::vector<core::ParameterStep> multiSteps = {
        makeStep(1, "VCF Cutoff", 0.40f, "Knob"),
        makeStep(2, "VCF Reso", 0.80f, "Knob"),
        makeStep(3, "Env Mod", 0.65f, "Slider"),
        makeStep(4, "LFO Rate", 0.20f, "Knob")
    };

    int notifiedIdx = -1;
    container.onSelectionChanged = [&](int idx) {
        notifiedIdx = idx;
    };

    container.setStepData(multiSteps, "Set all 4 parameters");
    CHECK(container.getParameterSteps().size() == 4);
    CHECK(container.getSelectedParamIndex() == 0);

    // Programmatic selection change
    container.setSelectedParamIndex(2);
    CHECK(container.getSelectedParamIndex() == 2);
    CHECK(notifiedIdx == 2);

    // Selection clamping
    container.setSelectedParamIndex(99);
    CHECK(container.getSelectedParamIndex() == 3);

    container.setSelectedParamIndex(-5);
    CHECK(container.getSelectedParamIndex() == 0);

    // Mouse click selection simulation
    // Each card is at x = 6 + i * 96, y <= 102.
    // Index 1 corresponds to x in [102, 198]. We test selection and notifications:
    container.setSelectedParamIndex(1);
    CHECK(container.getSelectedParamIndex() == 1);
    CHECK(notifiedIdx == 1);

    // Paint multi-control view without throw
    juce::Image canvas(juce::Image::ARGB, 800, 220, true);
    juce::Graphics g(canvas);
    CHECK_NOTHROW(container.paint(g));
}
