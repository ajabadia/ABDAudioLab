#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../gui/Waterfall3DComponent.h"

using namespace abdaudiolab;

TEST_CASE("Waterfall3DComponent: Autonomous isometric mountain visualizer", "[Waterfall3D][gui]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    gui::Waterfall3DComponent waterfall;

    SECTION("Initial camera parameters and dimension safety")
    {
        waterfall.setBounds(0, 0, 800, 450);
        REQUIRE(waterfall.getWidth() == 800);
        REQUIRE(waterfall.getHeight() == 450);

        REQUIRE(waterfall.getXOffset() == Catch::Approx(1.5f));
        REQUIRE(waterfall.getYOffset() == Catch::Approx(2.0f));
        REQUIRE(waterfall.getZoomFactor() == Catch::Approx(1.0f));
        REQUIRE_FALSE(waterfall.getPaletteMode());
    }

    SECTION("Camera manipulation and clamping")
    {
        waterfall.setIsometricOffsets(3.5f, 4.2f);
        REQUIRE(waterfall.getXOffset() == Catch::Approx(3.5f));
        REQUIRE(waterfall.getYOffset() == Catch::Approx(4.2f));

        // Test boundary clamping
        waterfall.setIsometricOffsets(0.05f, 10.0f);
        REQUIRE(waterfall.getXOffset() == Catch::Approx(0.2f)); // Min 0.2
        REQUIRE(waterfall.getYOffset() == Catch::Approx(6.0f)); // Max 6.0

        waterfall.setZoomFactor(2.5f);
        REQUIRE(waterfall.getZoomFactor() == Catch::Approx(2.5f));

        waterfall.setZoomFactor(5.0f);
        REQUIRE(waterfall.getZoomFactor() == Catch::Approx(3.5f)); // Max 3.5

        waterfall.resetCamera();
        REQUIRE(waterfall.getXOffset() == Catch::Approx(1.5f));
        REQUIRE(waterfall.getYOffset() == Catch::Approx(2.0f));
        REQUIRE(waterfall.getZoomFactor() == Catch::Approx(1.0f));
    }

    SECTION("Palette mode toggle")
    {
        waterfall.setPaletteMode(true);
        REQUIRE(waterfall.getPaletteMode());

        waterfall.setPaletteMode(false);
        REQUIRE_FALSE(waterfall.getPaletteMode());
    }

    SECTION("Trajectory update and clear without leak")
    {
        std::vector<math::PreScanPoint> trajectory;
        for (int i = 0; i < 16; ++i)
        {
            math::PreScanPoint pt;
            pt.controlValue = static_cast<float>(i) / 15.0f;
            pt.timeSec = static_cast<float>(i) * 0.1f;
            pt.primaryMetric = 100.0f + (static_cast<float>(i) * 500.0f); // Cutoff Hz
            pt.thdPercent = 0.05f;
            trajectory.push_back(pt);
        }

        waterfall.updateTrajectoryData(trajectory);

        // Rendering headless check
        juce::Image testCanvas(juce::Image::ARGB, 800, 450, true);
        juce::Graphics g(testCanvas);
        waterfall.paint(g);

        waterfall.clearData();
        waterfall.paint(g);
    }
}
