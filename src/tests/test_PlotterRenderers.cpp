#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../gui/PlotterHeatmapRenderer.h"
#include "../gui/PlotterFrequencyCurveRenderer.h"

using namespace abdaudiolab;

TEST_CASE("PlotterRenderers: Autonomous Heatmap and Frequency Curve widgets", "[PlotterRenderers][gui]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    SECTION("PlotterHeatmapRenderer: empty state, population, and viridis color map")
    {
        gui::PlotterHeatmapRenderer heatmap;
        heatmap.setBounds(0, 0, 600, 400);

        // Empty state paint
        juce::Image testCanvas(juce::Image::ARGB, 600, 400, true);
        juce::Graphics g(testCanvas);
        heatmap.paint(g);

        // Viridis bounds and continuity checks
        auto c0 = gui::PlotterHeatmapRenderer::viridisColor(0.0f);
        auto cMid = gui::PlotterHeatmapRenderer::viridisColor(0.5f);
        auto c1 = gui::PlotterHeatmapRenderer::viridisColor(1.0f);
        REQUIRE(c0 != cMid);
        REQUIRE(cMid != c1);

        // Populate with synthetic grid data
        std::vector<exporting::MeasuredPoint> pts;
        for (int i = 0; i < 16; ++i)
        {
            exporting::MeasuredPoint p;
            p.param1Normalized = static_cast<float>(i) / 15.0f;
            p.muSigmaValue.mean = static_cast<float>(i) * 2.0f;
            pts.push_back(p);
        }

        heatmap.setPoints(pts);
        heatmap.paint(g);

        heatmap.clear();
        heatmap.paint(g);
    }

    SECTION("PlotterFrequencyCurveRenderer: curve, sigma band, roadmap, and tooltip interactions")
    {
        gui::PlotterFrequencyCurveRenderer curveRenderer;
        curveRenderer.setBounds(0, 0, 800, 500);

        juce::Image testCanvas(juce::Image::ARGB, 800, 500, true);
        juce::Graphics g(testCanvas);
        curveRenderer.paint(g);

        // PreScan data injection
        std::vector<math::PreScanPoint> trajectory;
        for (int i = 0; i < 10; ++i)
        {
            math::PreScanPoint pt;
            pt.controlValue = static_cast<float>(i * 12);
            pt.primaryMetric = 200.0f + static_cast<float>(i * 300);
            trajectory.push_back(pt);
        }
        std::vector<int> roadmap = { 10, 45, 90 };
        curveRenderer.setPreScanData(trajectory, roadmap, true);
        curveRenderer.paint(g);

        // Measurement points population
        std::vector<exporting::MeasuredPoint> pts;
        for (int i = 0; i < 8; ++i)
        {
            exporting::MeasuredPoint p;
            p.param1Normalized = static_cast<float>(i) / 7.0f;
            p.secondaryValue.mean = -6.0f + static_cast<float>(i) * 1.5f;
            p.muSigmaValue.stdDev = 0.45f;
            p.thdPercent = 0.08f;
            p.snrDb = 42.0f;
            pts.push_back(p);
        }
        curveRenderer.setPoints(pts);
        curveRenderer.setHighlightedPointIndex(3);
        curveRenderer.setMeasuringProgress(true, 0.65f);
        curveRenderer.paint(g);

        // Toggles
        curveRenderer.setShowMeanCurve(false);
        curveRenderer.setShowSigmaBand(false);
        curveRenderer.setShowThdPoints(false);
        curveRenderer.paint(g);

        curveRenderer.setShowMeanCurve(true);
        curveRenderer.setShowSigmaBand(true);
        curveRenderer.setShowThdPoints(true);
        curveRenderer.paint(g);

        // Mouse move crosshair trigger
        auto grid = curveRenderer.getGridBounds();
        curveRenderer.setHoverPosition({ grid.getCentreX(), grid.getCentreY() });
        curveRenderer.paint(g);

        curveRenderer.clearHover();
        curveRenderer.paint(g);

        curveRenderer.clear();
        curveRenderer.clearPreScanData();
        curveRenderer.paint(g);
    }
}
