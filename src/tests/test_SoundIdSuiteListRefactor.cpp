/**
 * @file test_SoundIdSuiteListRefactor.cpp
 * @brief Unit tests for the modular SuiteQueueModelManager and SuiteListEventHandler.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include "gui/suite/SuiteQueueModelManager.h"
#include "gui/suite/SuiteListEventHandler.h"
#include "gui/SoundIdSuiteList.h"

using namespace abdaudiolab::gui;
using namespace abdaudiolab::gui::suite;

TEST_CASE("SuiteQueueModelManager - Pinned Baseline and Queue Operations", "[gui][suite][refactor]")
{
    SuiteQueueModelManager mgr;

    SECTION("Pinning of test 0 baseline")
    {
        mgr.ensureNoiseBaselineTestPinned();
        REQUIRE(mgr.getQueueSize() == 1);
        CHECK(mgr.getQueue()[0].isPinned == true);
        CHECK(mgr.getQueue()[0].badgeText == "NOI");

        // Attempting to remove pinned test 0 must fail
        bool removed = mgr.removeTestDirectly(0);
        CHECK_FALSE(removed);
        CHECK(mgr.getQueueSize() == 1);
    }

    SECTION("Adding and duplicating items")
    {
        mgr.ensureNoiseBaselineTestPinned();

        QueueItem item;
        item.id = "hw:vcf:sweep";
        item.title = "VCF Sweep";
        item.totalPoints = 16;
        item.status = QueueItemStatus::Queued;

        bool added = mgr.addTest(item);
        CHECK(added);
        CHECK(mgr.getQueueSize() == 2);
        CHECK(mgr.getTotalPointCount() == 17); // 1 pinned + 16

        // Duplicate test
        mgr.duplicateTest(1);
        REQUIRE(mgr.getQueueSize() == 3);
        CHECK(mgr.getQueue()[2].title == "VCF Sweep (Copy)");
        CHECK(mgr.getQueue()[2].isPinned == false);

        // Reordering
        mgr.moveDown(1);
        CHECK(mgr.getQueue()[1].title == "VCF Sweep (Copy)");
        CHECK(mgr.getQueue()[2].title == "VCF Sweep");
    }

    SECTION("Multi-Point selection and range operations")
    {
        mgr.ensureNoiseBaselineTestPinned();

        QueueItem item;
        item.id = "hw:adsr:test";
        item.totalPoints = 8;
        item.status = QueueItemStatus::Queued;
        mgr.addTest(item);

        // Discontinuous selection
        mgr.setPointSelected(1, 2, true);
        mgr.setPointSelected(1, 5, true);
        CHECK(mgr.isPointSelected(1, 2));
        CHECK(mgr.isPointSelected(1, 5));
        CHECK_FALSE(mgr.isPointSelected(1, 3));
        CHECK(mgr.getSelectedPointCount() == 2);

        // Range selection
        mgr.selectPointRange(1, 3, 6, true);
        CHECK(mgr.getSelectedPointCount() == 5); // 2, 3, 4, 5, 6

        // Clear all
        mgr.clearAllSelections();
        CHECK(mgr.getSelectedPointCount() == 0);
    }

    SECTION("Point status mutation and invalidation")
    {
        mgr.ensureNoiseBaselineTestPinned();

        QueueItem item;
        item.id = "hw:sat:test";
        item.totalPoints = 4;
        mgr.addTest(item);

        mgr.setPointStatus(1, 1, PointStatus::Completed);
        mgr.setPointStatus(1, 2, PointStatus::Invalidated);

        CHECK(mgr.getPointStatus(1, 1) == PointStatus::Completed);
        CHECK(mgr.getPointStatus(1, 2) == PointStatus::Invalidated);
        CHECK(mgr.getPointStatus(1, 3) == PointStatus::Queued);

        // Select all invalidated
        mgr.selectAllInvalidatedPoints();
        CHECK(mgr.getSelectedPointCount() == 1);
        CHECK(mgr.isPointSelected(1, 2));
    }
}

TEST_CASE("SoundIdSuiteList - Orchestration with Modular Components", "[gui][suite][refactor]")
{
    SoundIdSuiteList suiteList;

    CHECK(suiteList.getQueueSize() >= 1); // Pinned test present
    CHECK(suiteList.isTestInQueue("system:noise_floor_baseline"));

    QueueItem custom;
    custom.id = "custom:filter:test";
    custom.title = "Custom Filter";
    custom.totalPoints = 10;

    suiteList.addTestToQueue(custom);
    CHECK(suiteList.isTestInQueue("custom:filter:test"));

    suiteList.setPointSelected(1, 3, true);
    CHECK(suiteList.getSelectedPointCount() >= 1);
    CHECK(suiteList.isPointSelected(1, 3));

    suiteList.clearAllSelections();
    CHECK(suiteList.getSelectedPointCount() == 0);
}
