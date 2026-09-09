#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../gui/PlotterModulationTableRenderer.h"

using namespace abdaudiolab;

TEST_CASE("PlotterModulationTableRenderer: Autonomous modulation table renderer", "[ModulationTable][gui]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    gui::PlotterModulationTableRenderer tableRenderer;

    SECTION("Initial empty state and dimensions")
    {
        tableRenderer.setBounds(0, 0, 700, 400);
        REQUIRE(tableRenderer.getWidth() == 700);
        REQUIRE(tableRenderer.getHeight() == 400);
        REQUIRE(tableRenderer.getProfile().getAllNodes().empty());

        // Paint empty state without crashing
        juce::Image testCanvas(juce::Image::ARGB, 700, 400, true);
        juce::Graphics g(testCanvas);
        tableRenderer.paint(g);
    }

    SECTION("Profile population and incremental node updates")
    {
        math::ModulationMatrixProfile profile;
        math::ModulationNode n1;
        n1.sourceID = 1; // CC1
        n1.destID = 4;   // Cutoff
        n1.kScalar = 0.75f;
        n1.offsetC = 0.05f;
        n1.rSquared = 0.982f;
        profile.setNode(1, 4, n1);

        tableRenderer.setProfile(profile);
        REQUIRE(tableRenderer.getProfile().hasNode(1, 4));
        REQUIRE(tableRenderer.getProfile().getNode(1, 4).kScalar == Catch::Approx(0.75f));

        // Incremental update
        math::ModulationNode n2;
        n2.sourceID = 2; // Velocity
        n2.destID = 7;   // Resonance
        n2.kScalar = -1.20f;
        n2.offsetC = 0.10f;
        n2.rSquared = 0.880f; // Amber warning
        tableRenderer.updateNode(n2);

        REQUIRE(tableRenderer.getProfile().hasNode(2, 7));
        REQUIRE(tableRenderer.getProfile().getAllNodes().size() == 2);

        // Third node with low R^2 (Red warning)
        math::ModulationNode n3;
        n3.sourceID = 3;
        n3.destID = 2;
        n3.kScalar = 0.15f;
        n3.offsetC = 0.0f;
        n3.rSquared = 0.65f;
        tableRenderer.updateNode(n3);

        REQUIRE(tableRenderer.getProfile().getAllNodes().size() == 3);

        // Headless paint check with 3 nodes
        juce::Image testCanvas(juce::Image::ARGB, 700, 400, true);
        juce::Graphics g(testCanvas);
        tableRenderer.paint(g);

        // Clear check
        tableRenderer.clear();
        REQUIRE(tableRenderer.getProfile().getAllNodes().empty());
        tableRenderer.paint(g);
    }
}
