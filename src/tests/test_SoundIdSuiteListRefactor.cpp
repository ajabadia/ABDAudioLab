/**
 * @file test_SoundIdSuiteListRefactor.cpp
 * @brief Characterization tests for SuiteQueueModelManager, SuiteListEventHandler and SoundIdSuiteList.
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

    SECTION("Pinning of test 0 baseline by semantic property")
    {
        mgr.ensureNoiseBaselineTestPinned();
        REQUIRE(mgr.getQueueSize() == 1);
        CHECK(mgr.getQueue()[0].isPinned == true);
        CHECK(mgr.getQueue()[0].badgeText == "NOI");

        // Attempting to remove pinned baseline must fail
        bool removed = mgr.removeTestDirectly(0);
        CHECK_FALSE(removed);
        CHECK(mgr.getQueueSize() == 1);

        // Attempting to invalidate pinned baseline must fail
        bool invalidated = mgr.invalidateTest(0);
        CHECK_FALSE(invalidated);
        CHECK(mgr.getQueue()[0].status == QueueItemStatus::Queued);

        // Attempting to move pinned baseline must fail
        CHECK_FALSE(mgr.moveUp(0));
        CHECK_FALSE(mgr.moveDown(0));
    }

    SECTION("Add: Valid addition vs duplicate ID rejection")
    {
        mgr.ensureNoiseBaselineTestPinned();

        QueueItem item1;
        item1.id = "hw:vcf:sweep";
        item1.title = "VCF Sweep";
        item1.totalPoints = 16;
        item1.status = QueueItemStatus::Queued;

        CHECK(mgr.addTest(item1));
        CHECK(mgr.getQueueSize() == 2);
        CHECK(mgr.getTotalPointCount() == 17); // 1 pinned + 16

        // Duplicate ID rejection
        QueueItem itemDuplicate;
        itemDuplicate.id = "hw:vcf:sweep";
        itemDuplicate.title = "VCF Sweep Duplicate";
        CHECK_FALSE(mgr.addTest(itemDuplicate));
        CHECK(mgr.getQueueSize() == 2);
    }

    SECTION("Remove: Pending vs Completed unpinned items")
    {
        mgr.ensureNoiseBaselineTestPinned();

        QueueItem pendingItem;
        pendingItem.id = "test:pending";
        pendingItem.status = QueueItemStatus::Queued;
        mgr.addTest(pendingItem);

        QueueItem completedItem;
        completedItem.id = "test:completed";
        completedItem.status = QueueItemStatus::Completed;
        mgr.addTest(completedItem);

        REQUIRE(mgr.getQueueSize() == 3);

        // Remove pending item
        CHECK(mgr.removeTestDirectly(1));
        CHECK(mgr.getQueueSize() == 2);
        CHECK_FALSE(mgr.isTestInQueue("test:pending"));

        // Remove completed unpinned item
        CHECK(mgr.removeTestDirectly(1));
        CHECK(mgr.getQueueSize() == 1);
        CHECK_FALSE(mgr.isTestInQueue("test:completed"));
    }

    SECTION("Invalidate: Completed item preserves position and point integrity")
    {
        mgr.ensureNoiseBaselineTestPinned();

        QueueItem item;
        item.id = "hw:filter:res";
        item.title = "Filter Resonance";
        item.totalPoints = 4;
        item.status = QueueItemStatus::Completed;
        item.pointStatuses = { PointStatus::Completed, PointStatus::Completed, PointStatus::Annulled, PointStatus::Completed };
        mgr.addTest(item);

        REQUIRE(mgr.getQueueSize() == 2);
        CHECK(mgr.invalidateTest(1));

        const auto& qItem = mgr.getQueue()[1];
        CHECK(qItem.id == "hw:filter:res");
        CHECK(qItem.status == QueueItemStatus::Invalidated);
        // Completed points become Invalidated, while Annulled remains Annulled
        CHECK(qItem.pointStatuses[0] == PointStatus::Invalidated);
        CHECK(qItem.pointStatuses[1] == PointStatus::Invalidated);
        CHECK(qItem.pointStatuses[2] == PointStatus::Annulled);
        CHECK(qItem.pointStatuses[3] == PointStatus::Invalidated);
    }

    SECTION("Duplicate: Generates distinct identity and queued state")
    {
        mgr.ensureNoiseBaselineTestPinned();

        QueueItem item;
        item.id = "hw:lfo:speed";
        item.title = "LFO Speed";
        item.totalPoints = 8;
        item.status = QueueItemStatus::Completed;
        mgr.addTest(item);

        CHECK(mgr.duplicateTest(1));
        REQUIRE(mgr.getQueueSize() == 3);

        const auto& original = mgr.getQueue()[1];
        const auto& copy = mgr.getQueue()[2];

        CHECK(original.id == "hw:lfo:speed");
        CHECK(copy.id != original.id);
        CHECK(copy.id.startsWith("hw:lfo:speed_copy_"));
        CHECK(copy.title == "LFO Speed (Copy)");
        CHECK(copy.status == QueueItemStatus::Queued);
        CHECK(copy.isPinned == false);
        CHECK(copy.totalPoints == 8);
    }

    SECTION("Reorder: Move Up and Move Down with boundary limits")
    {
        mgr.ensureNoiseBaselineTestPinned();

        QueueItem a; a.id = "item:A"; a.title = "Item A"; mgr.addTest(a);
        QueueItem b; b.id = "item:B"; b.title = "Item B"; mgr.addTest(b);
        QueueItem c; c.id = "item:C"; c.title = "Item C"; mgr.addTest(c);

        REQUIRE(mgr.getQueueSize() == 4); // [0]=Noise, [1]=A, [2]=B, [3]=C

        // Move Up on index 1 must fail (cannot jump over pinned baseline at 0)
        CHECK_FALSE(mgr.moveUp(1));

        // Move Down on index 3 must fail (already last)
        CHECK_FALSE(mgr.moveDown(3));

        // Move Up index 2 (B swaps with A)
        CHECK(mgr.moveUp(2));
        CHECK(mgr.getQueue()[1].id == "item:B");
        CHECK(mgr.getQueue()[2].id == "item:A");

        // Move Down index 1 (B swaps with A back)
        CHECK(mgr.moveDown(1));
        CHECK(mgr.getQueue()[1].id == "item:A");
        CHECK(mgr.getQueue()[2].id == "item:B");
    }

    SECTION("Selection by Stable Identity: Preserved after mutations and cleared on delete")
    {
        mgr.ensureNoiseBaselineTestPinned();

        QueueItem a; a.id = "item:A"; mgr.addTest(a);
        QueueItem b; b.id = "item:B"; mgr.addTest(b);
        QueueItem c; c.id = "item:C"; mgr.addTest(c);

        // Select item B (currently at index 2)
        mgr.selectItem(2);
        CHECK(mgr.getSelectedItemId() == "item:B");
        CHECK(mgr.getSelectedItemIndex() == 2);
        REQUIRE(mgr.getSelectedItem() != nullptr);
        CHECK(mgr.getSelectedItem()->id == "item:B");

        // Move B up to index 1: selection follows stable ID
        CHECK(mgr.moveUp(2));
        CHECK(mgr.getSelectedItemId() == "item:B");
        CHECK(mgr.getSelectedItemIndex() == 1);

        // Remove item A (now at index 2): selection of B at index 1 is untouched
        CHECK(mgr.removeTestDirectly(2));
        CHECK(mgr.getSelectedItemId() == "item:B");
        CHECK(mgr.getSelectedItemIndex() == 1);

        // Remove item B itself: selection cleared
        CHECK(mgr.removeTestDirectly(1));
        CHECK(mgr.getSelectedItemId().isEmpty());
        CHECK(mgr.getSelectedItemIndex() == -1);
        CHECK(mgr.getSelectedItem() == nullptr);
    }

    SECTION("Reject Invalid Indices")
    {
        mgr.ensureNoiseBaselineTestPinned();

        CHECK_FALSE(mgr.removeTestDirectly(-1));
        CHECK_FALSE(mgr.removeTestDirectly(99));
        CHECK_FALSE(mgr.invalidateTest(-5));
        CHECK_FALSE(mgr.invalidateTest(100));
        CHECK_FALSE(mgr.moveUp(-1));
        CHECK_FALSE(mgr.moveUp(50));
        CHECK_FALSE(mgr.moveDown(-2));
        CHECK_FALSE(mgr.moveDown(50));
        CHECK_FALSE(mgr.duplicateTest(-1));
        CHECK_FALSE(mgr.duplicateTest(25));
    }

    SECTION("Atomic Notifications: Exactly one event per valid mutation, zero on rejected")
    {
        mgr.ensureNoiseBaselineTestPinned();

        int eventCount = 0;
        mgr.onQueueChanged = [&]() {
            ++eventCount;
            // Ensure model is fully coherent and queryable during notification
            CHECK(mgr.getQueueSize() >= 1);
        };

        // 1. Valid Add -> exactly 1 event
        QueueItem item; item.id = "valid:test";
        CHECK(mgr.addTest(item));
        CHECK(eventCount == 1);

        // 2. Rejected Duplicate Add -> 0 events
        CHECK_FALSE(mgr.addTest(item));
        CHECK(eventCount == 1);

        // 3. Selection change -> exactly 1 event
        mgr.selectItem(1);
        CHECK(eventCount == 2);

        // 4. Valid Move -> exactly 1 event (if we add another item)
        QueueItem item2; item2.id = "valid:test2";
        CHECK(mgr.addTest(item2));
        CHECK(eventCount == 3);

        CHECK(mgr.moveUp(2));
        CHECK(eventCount == 4);

        // 5. Rejected Move on pinned baseline -> 0 events
        CHECK_FALSE(mgr.moveUp(1));
        CHECK(eventCount == 4);

        // 6. Valid Remove -> exactly 1 event
        CHECK(mgr.removeTestDirectly(2));
        CHECK(eventCount == 5);

        // 7. Rejected Remove on pinned baseline -> 0 events
        CHECK_FALSE(mgr.removeTestDirectly(0));
        CHECK(eventCount == 5);

        // 8. Null callback works safely without crash
        mgr.onQueueChanged = nullptr;
        CHECK(mgr.removeTestDirectly(1));
        CHECK(mgr.getQueueSize() == 1);
    }

    SECTION("Multi-Point selection and range operations")
    {
        mgr.ensureNoiseBaselineTestPinned();

        QueueItem item;
        item.id = "hw:adsr:test";
        item.totalPoints = 8;
        item.status = QueueItemStatus::Queued;
        mgr.addTest(item);

        mgr.setPointSelected(1, 2, true);
        mgr.setPointSelected(1, 5, true);
        CHECK(mgr.isPointSelected(1, 2));
        CHECK(mgr.isPointSelected(1, 5));
        CHECK_FALSE(mgr.isPointSelected(1, 3));
        CHECK(mgr.getSelectedPointCount() == 2);

        mgr.selectPointRange(1, 3, 6, true);
        CHECK(mgr.getSelectedPointCount() == 5); // 2, 3, 4, 5, 6

        mgr.clearAllSelections();
        CHECK(mgr.getSelectedPointCount() == 0);
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

    suiteList.selectItemById("custom:filter:test");
    CHECK(suiteList.getSelectedItemId() == "custom:filter:test");
    int customIdx = suiteList.getSelectedItemIndex();
    CHECK(customIdx == 2);

    suiteList.setPointSelected(1, 3, true);
    CHECK(suiteList.getSelectedPointCount() >= 1);
    CHECK(suiteList.isPointSelected(1, 3));

    suiteList.clearAllSelections();
    CHECK(suiteList.getSelectedPointCount() == 0);
}
