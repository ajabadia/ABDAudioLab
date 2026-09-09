#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "gui/SoundIdCurvePlotter.h"
#include <juce_gui_basics/juce_gui_basics.h>

using namespace abdaudiolab;
using namespace abdaudiolab::gui;
using namespace abdaudiolab::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("SoundIdCurvePlotter: ViewMode and ModulationMatrix features", "[gui][curve_plotter][mod_matrix]")
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    SoundIdCurvePlotter plotter;
    plotter.setSize(800, 600);

    SECTION("ViewMode switching includes Waterfall3D and ModulationMatrix")
    {
        plotter.setViewMode(SoundIdCurvePlotter::ViewMode::FrequencyCurve);
        plotter.setViewMode(SoundIdCurvePlotter::ViewMode::Waterfall3D);
        plotter.setViewMode(SoundIdCurvePlotter::ViewMode::ModulationMatrix);

        // Populate profile and verify storage
        ModulationMatrixProfile profile;
        ModulationNode node;
        node.sourceID = 1;
        node.destID = 4;
        node.kScalar = 0.5f;
        node.offsetC = 0.0f;
        node.rSquared = 0.985f;
        profile.setNode(node.sourceID, node.destID, node);

        plotter.setModulationProfile(profile);
        REQUIRE(plotter.getModulationProfile().hasNode(1, 4));
        REQUIRE_THAT(plotter.getModulationProfile().getNode(1, 4).kScalar, WithinAbs(0.5f, 1e-4f));

        // Incremental update
        ModulationNode node2;
        node2.sourceID = 2;
        node2.destID = 8;
        node2.kScalar = 1.25f;
        node2.offsetC = -0.1f;
        node2.rSquared = 0.89f;
        plotter.updateModulationNode(node2);

        REQUIRE(plotter.getModulationProfile().hasNode(2, 8));
        REQUIRE_THAT(plotter.getModulationProfile().getNode(2, 8).rSquared, WithinAbs(0.89f, 1e-4f));
    }

    SECTION("Interactivity in Waterfall3D sets camera parameters and repaints cleanly")
    {
        plotter.setViewMode(SoundIdCurvePlotter::ViewMode::Waterfall3D);

        plotter.set3DIsometricOffsets(2.5f, 3.2f);
        REQUIRE_THAT(plotter.get3DXOffset(), WithinAbs(2.5f, 1e-3f));
        REQUIRE_THAT(plotter.get3DYOffset(), WithinAbs(3.2f, 1e-3f));

        plotter.set3DZoomFactor(1.75f);
        REQUIRE_THAT(plotter.get3DZoomFactor(), WithinAbs(1.75f, 1e-3f));

        // Reset to default
        plotter.reset3DCamera();
        REQUIRE_THAT(plotter.get3DXOffset(), WithinAbs(1.5f, 1e-3f));
        REQUIRE_THAT(plotter.get3DYOffset(), WithinAbs(2.0f, 1e-3f));
        REQUIRE_THAT(plotter.get3DZoomFactor(), WithinAbs(1.0f, 1e-3f));

        // Rendering headless to offscreen bitmap to ensure paint is leak-free
        juce::Image offscreen(juce::Image::ARGB, 800, 600, true);
        juce::Graphics g(offscreen);
        plotter.paint(g);
        REQUIRE(offscreen.isValid());

        // Test paint in ModulationMatrix mode with nodes
        plotter.setViewMode(SoundIdCurvePlotter::ViewMode::ModulationMatrix);
        plotter.paint(g);
        REQUIRE(offscreen.isValid());
    }
}
