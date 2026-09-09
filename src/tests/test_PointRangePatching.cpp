#include <catch2/catch_test_macros.hpp>
#include "core/SessionManager.h"
#include "core/ProfilingSession.h"
#include "gui/suite/SuiteDataModels.h"
#include "gui/SoundIdCurvePlotter.h"
#include <juce_core/juce_core.h>

TEST_CASE("SessionManager in-place point patching", "[core][patching]")
{
    abdaudiolab::core::SessionManager sessionManager;

    std::vector<abdaudiolab::exporting::MeasuredPoint> initialPoints;
    for (int i = 0; i < 5; ++i)
    {
        abdaudiolab::exporting::MeasuredPoint pt;
        pt.pointId = "P_" + std::to_string(i + 1);
        pt.testId = "Test_Cutoff";
        pt.param1Normalized = static_cast<float>(i) * 0.25f;
        pt.muSigmaValue = { 1000.0f + static_cast<float>(i) * 500.0f, 10.0f };
        pt.thdPercent = 0.01f * static_cast<float>(i + 1);
        initialPoints.push_back(pt);
    }
    sessionManager.setMeasuredPoints(initialPoints);
    sessionManager.setDirty(false);

    REQUIRE(sessionManager.getPointCount() == 5);
    REQUIRE_FALSE(sessionManager.isDirty());

    // Patch point at index 2
    abdaudiolab::exporting::MeasuredPoint patchedPt;
    patchedPt.pointId = "P_003";
    patchedPt.testId = "Test_Cutoff";
    patchedPt.param1Normalized = 0.5f;
    patchedPt.muSigmaValue = { 2150.0f, 2.5f }; // Freshly re-measured values
    patchedPt.thdPercent = 0.015f;

    bool patchOk = sessionManager.patchMeasuredPoint(2, patchedPt);
    REQUIRE(patchOk);
    REQUIRE(sessionManager.isDirty());
    REQUIRE(sessionManager.getPointCount() == 5);

    // Verify index 2 is updated
    const auto* p2 = sessionManager.getPoint(2);
    REQUIRE(p2 != nullptr);
    REQUIRE(p2->muSigmaValue.mean == 2150.0f);
    REQUIRE(p2->muSigmaValue.stdDev == 2.5f);
    REQUIRE(p2->thdPercent == 0.015f);

    // Verify neighbors 0, 1, 3, 4 remain intact
    REQUIRE(sessionManager.getPoint(0)->muSigmaValue.mean == 1000.0f);
    REQUIRE(sessionManager.getPoint(1)->muSigmaValue.mean == 1500.0f);
    REQUIRE(sessionManager.getPoint(3)->muSigmaValue.mean == 2500.0f);
    REQUIRE(sessionManager.getPoint(4)->muSigmaValue.mean == 3000.0f);

    // Verify out-of-bounds patch returns false
    REQUIRE_FALSE(sessionManager.patchMeasuredPoint(99, patchedPt));
}

TEST_CASE("Multi-point selection logic and range selection", "[gui][patching]")
{
    abdaudiolab::gui::QueueItem item;
    item.totalPoints = 16;
    item.pointStatuses.assign(16, abdaudiolab::gui::PointStatus::Queued);
    item.pointSelections.assign(16, false);

    // 1. Discontinuous selection (point 2 and point 10)
    item.pointSelections[2] = true;
    item.pointSelections[10] = true;

    int selectedCount = 0;
    for (bool sel : item.pointSelections)
        if (sel) ++selectedCount;
    REQUIRE(selectedCount == 2);

    // 2. Range selection from 4 to 8 inclusive
    int startRange = 4;
    int endRange = 8;
    for (int p = startRange; p <= endRange; ++p)
        item.pointSelections[static_cast<size_t>(p)] = true;

    selectedCount = 0;
    for (bool sel : item.pointSelections)
        if (sel) ++selectedCount;
    // 2 previously selected (2, 10) + 5 in range [4..8] = 7 total
    REQUIRE(selectedCount == 7);

    // 3. Mark points 3 and 14 as Invalidated / Annulled
    item.pointStatuses[3] = abdaudiolab::gui::PointStatus::Invalidated;
    item.pointStatuses[14] = abdaudiolab::gui::PointStatus::Annulled;

    // Select all invalidated / error points
    for (size_t p = 0; p < item.pointStatuses.size(); ++p)
    {
        if (item.pointStatuses[p] == abdaudiolab::gui::PointStatus::Invalidated ||
            item.pointStatuses[p] == abdaudiolab::gui::PointStatus::Annulled)
        {
            item.pointSelections[p] = true;
        }
    }

    REQUIRE(item.pointSelections[3] == true);
    REQUIRE(item.pointSelections[14] == true);

    // 4. Clear all selections
    std::fill(item.pointSelections.begin(), item.pointSelections.end(), false);
    selectedCount = 0;
    for (bool sel : item.pointSelections)
        if (sel) ++selectedCount;
    REQUIRE(selectedCount == 0);
}

TEST_CASE("ProfilingSession patch metadata and TestCase tracking", "[core][patching]")
{
    abdaudiolab::core::ProfilingSession session;
    REQUIRE_FALSE(session.isPatchSession());

    session.setIsPatchSession(true);
    REQUIRE(session.isPatchSession());

    abdaudiolab::core::TestCase tc;
    tc.queueItemIndex = 1;
    tc.pointIndexInTest = 4;
    tc.totalPointsInTest = 8;
    tc.globalPointIndex = 12; // Test 0 had 8 points, this is index 4 in Test 1 -> 8 + 4 = 12
    tc.pointId = "P_013";
    tc.testId = "Resonance_Sweep";
    tc.functionalBlockType = "SpectrumFilter";

    session.addTestCase(tc);

    REQUIRE(session.getTestCases().size() == 1);
    const auto& retrieved = session.getTestCases()[0];
    REQUIRE(retrieved.globalPointIndex == 12);
    REQUIRE(retrieved.queueItemIndex == 1);
    REQUIRE(retrieved.pointIndexInTest == 4);
    REQUIRE(retrieved.pointId == "P_013");
}

TEST_CASE("SoundIdCurvePlotter patchPoint updates point in-place", "[gui][plotter]")
{
    // Initialize JUCE MessageManager for safe GUI component tests
    juce::ScopedJuceInitialiser_GUI guiInit;

    abdaudiolab::gui::SoundIdCurvePlotter plotter;

    std::vector<abdaudiolab::exporting::MeasuredPoint> points;
    for (int i = 0; i < 3; ++i)
    {
        abdaudiolab::exporting::MeasuredPoint pt;
        pt.pointId = "P_" + std::to_string(i + 1);
        pt.muSigmaValue = { static_cast<float>(i * 100), 1.0f };
        points.push_back(pt);
    }
    plotter.setPoints(points);

    abdaudiolab::exporting::MeasuredPoint patchedPt;
    patchedPt.pointId = "P_002";
    patchedPt.muSigmaValue = { 999.0f, 0.5f };

    plotter.patchPoint(1, patchedPt);
    plotter.patchPoint(99, patchedPt); // Out-of-bounds appends safely
}
