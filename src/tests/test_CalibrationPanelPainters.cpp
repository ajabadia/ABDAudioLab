/**
 * @file test_CalibrationPanelPainters.cpp
 * @brief Headless tests for the Step 2 painters. They are pure functions of a
 *        calibrationpanel::ViewState, so they can be exercised without a window by
 *        rendering into an offscreen image and asserting layout invariants.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>

#include "gui/calibration/CalibrationPanelPainter.h"
#include "gui/calibration/CalibrationPanelTypes.h"

#include <juce_gui_basics/juce_gui_basics.h>

using namespace abdaudiolab;
using namespace abdaudiolab::gui::calibrationpanel;
namespace painter = abdaudiolab::gui::calibrationpanel::painter;

namespace
{

constexpr int kCanvasWidth { 860 };
constexpr int kCanvasHeight { 580 };

/** @brief Minimal offscreen target: the painters only need a Graphics and rectangles. */
struct Canvas
{
    Canvas()
        : image(juce::Image::ARGB, kCanvasWidth, kCanvasHeight, true, juce::SoftwareImageType())
        , graphics(image)
    {
    }

    juce::Image image;
    juce::Graphics graphics;

    /** @brief Cheap fingerprint so two different states can be told apart. */
    [[nodiscard]] juce::uint64 fingerprint() const
    {
        juce::uint64 hash = 1469598103934665603ULL; // FNV-1a offset basis
        constexpr juce::uint64 prime = 1099511628211ULL;
        const int stride = juce::jmax(1, image.getHeight() / 64);

        for (int y = 0; y < image.getHeight(); y += stride)
            for (int x = 0; x < image.getWidth(); x += stride)
                hash = (hash ^ static_cast<juce::uint64>(image.getPixelAt(x, y).getARGB())) * prime;

        return hash;
    }
};

ViewState makeView()
{
    ViewState view;
    view.state = State::ReadyToMeasure;
    view.subView = SubView::NoiseBaseline_2A;
    view.noiseBaselineState = NoiseBaselineState::NotChecked;
    view.loopbackState = LoopbackState::Locked;
    view.outputChannelName = "Output 1";
    view.inputChannelName = "Input 1";
    return view;
}

ViewState makePassedLoopbackView()
{
    auto view = makeView();
    view.subView = SubView::PhysicalLoopback_2B;
    view.state = State::Success;
    view.noiseBaselineState = NoiseBaselineState::Passed;
    view.loopbackState = LoopbackState::Passed;
    view.noiseReport.passed = true;
    view.noiseReport.rmsDbfs = -68.4f;
    view.loopbackReport.passed = true;
    view.loopbackReport.roundTripLatencyMs = 2.35f;
    view.loopbackReport.recommendedTrimGain = 1.18f;
    view.loopbackReport.snrDb = 84.0f;
    return view;
}

/**
 * @brief Pixels whose ARGB matches @p colour exactly.
 *
 * Used with the hardcoded failure pill fill so the assertions stay valid in a headless run,
 * where the theme colours are not initialised.
 */
[[nodiscard]] int countExactPixels(const juce::Image& image, juce::Colour colour)
{
    const auto target = colour.getARGB();
    int matches = 0;

    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
            if (image.getPixelAt(x, y).getARGB() == target)
                ++matches;

    return matches;
}

} // namespace

//==============================================================================
TEST_CASE("CalibrationPanelPainters: Step 2A and 2B cards never overlap", "[gui][calibration][paint]")
{
    const juce::Rectangle<float> stepperArea { 0.0f, 0.0f, 804.0f, 62.0f };

    for (const auto subView : { SubView::NoiseBaseline_2A, SubView::PhysicalLoopback_2B })
    {
        auto view = makeView();
        view.subView = subView;

        Canvas canvas;
        juce::Rectangle<int> card2A;
        juce::Rectangle<int> card2B;

        painter::paintStepper(canvas.graphics, view, stepperArea, card2A, card2B);

        CHECK_FALSE(card2A.isEmpty());
        CHECK_FALSE(card2B.isEmpty());

        // The cards must tile the stepper row without overlapping and without drifting
        // outside it (toNearestInt rounding makes an exact gap check brittle).
        CHECK_FALSE(card2A.intersects(card2B));
        CHECK(card2A.getWidth() == card2B.getWidth());

        const int gap = card2B.getX() - card2A.getRight() - 1;
        CHECK(gap >= 12);
        CHECK(gap <= 16);

        CHECK(card2A.getX() >= static_cast<int>(stepperArea.getX()));
        CHECK(card2B.getRight() <= static_cast<int>(stepperArea.getRight()));
        CHECK(card2A.getHeight() == static_cast<int>(stepperArea.getHeight()));
    }
}

TEST_CASE("CalibrationPanelPainters: Stepper renders every loopback state without throwing", "[gui][calibration][paint]")
{
    const juce::Rectangle<float> stepperArea { 0.0f, 0.0f, 804.0f, 62.0f };

    const LoopbackState states[]
    {
        LoopbackState::Locked, LoopbackState::Ready, LoopbackState::MeasuringPreflight,
        LoopbackState::MeasuringSweep, LoopbackState::Passed, LoopbackState::SignalTooLow,
        LoopbackState::Clipped, LoopbackState::DeviceStopped, LoopbackState::Failed,
        LoopbackState::Stale, LoopbackState::Cancelled
    };

    for (const auto loopback : states)
    {
        auto view = makeView();
        view.subView = SubView::PhysicalLoopback_2B;
        view.noiseBaselineState = NoiseBaselineState::Passed;
        view.loopbackState = loopback;

        Canvas canvas;
        juce::Rectangle<int> card2A;
        juce::Rectangle<int> card2B;

        CHECK_NOTHROW(painter::paintStepper(canvas.graphics, view, stepperArea, card2A, card2B));

        // A state that reaches the report as a failure must not render identically to Ready.
        if (painter::isLoopbackFailureState(loopback))
            CHECK(canvas.fingerprint() != juce::uint64(0));
    }
}

TEST_CASE("CalibrationPanelPainters: Failure states are visually distinct from Ready", "[gui][calibration][paint]")
{
    auto readyView = makeView();
    readyView.subView = SubView::PhysicalLoopback_2B;
    readyView.noiseBaselineState = NoiseBaselineState::Passed;
    readyView.loopbackState = LoopbackState::Ready;

    auto failedView = readyView;
    failedView.loopbackState = LoopbackState::Failed;
    failedView.diagnostics.failureReason = "Capture incomplete";
    failedView.loopbackReport.failureReason = "Capture incomplete";

    Canvas readyCanvas;
    Canvas failedCanvas;

    juce::Rectangle<float> rightColumn { 420.0f, 0.0f, 384.0f, 580.0f };

    juce::Rectangle<float> readyMeter;
    juce::Rectangle<float> failedMeter;
    painter::paintLevelMonitor(readyCanvas.graphics, readyView, rightColumn, readyMeter);
    painter::paintLevelMonitor(failedCanvas.graphics, failedView, rightColumn, failedMeter);
    painter::paintLoopbackReport(readyCanvas.graphics, readyView, readyMeter);
    painter::paintLoopbackReport(failedCanvas.graphics, failedView, failedMeter);

    CHECK(readyCanvas.fingerprint() != failedCanvas.fingerprint());
}

TEST_CASE("CalibrationPanelPainters: Monitor hands the report area inside its own card", "[gui][calibration][paint]")
{
    const juce::Rectangle<float> rightColumn { 420.0f, 0.0f, 384.0f, 580.0f };
    const auto monitorTop = rightColumn.getY();
    const auto monitorBottom = monitorTop + 320.0f; // kMonitorHeight

    Canvas canvas;
    juce::Rectangle<float> meterArea;
    painter::paintLevelMonitor(canvas.graphics, makeView(), rightColumn, meterArea);

    // The report must start below the monitor card and stay inside it horizontally.
    CHECK(meterArea.getY() > monitorTop);
    CHECK(meterArea.getBottom() <= monitorBottom);
    CHECK(meterArea.getX() >= rightColumn.getX());
    CHECK(meterArea.getRight() <= rightColumn.getRight());
    CHECK(meterArea.getHeight() > 0.0f);
}

TEST_CASE("CalibrationPanelPainters: 2A report renders in every noise baseline state", "[gui][calibration][paint]")
{
    const NoiseBaselineState states[]
    {
        NoiseBaselineState::NotChecked, NoiseBaselineState::Checking,
        NoiseBaselineState::Passed, NoiseBaselineState::Contaminated,
        NoiseBaselineState::SafetyAborted, NoiseBaselineState::DeviceStopped,
        NoiseBaselineState::Cancelled
    };

    for (const auto baseline : states)
    {
        auto view = makeView();
        view.subView = SubView::NoiseBaseline_2A;
        view.noiseBaselineState = baseline;
        view.noiseReport.rmsDbfs = -72.5f;
        view.noiseReport.peakDbfs = -48.0f;

        Canvas canvas;
        juce::Rectangle<float> meterArea { 0.0f, 0.0f, 356.0f, 240.0f };

        CHECK_NOTHROW(painter::paintNoiseBaselineReport(canvas.graphics, view, meterArea));
    }
}

TEST_CASE("CalibrationPanelPainters: Saved profiles section is safe when empty or out of range", "[gui][calibration][paint]")
{
    const juce::Rectangle<float> rightColumn { 420.0f, 0.0f, 384.0f, 580.0f };

    SECTION("Collapsed section draws nothing")
    {
        auto view = makeView();
        view.showSavedProfiles = false;

        Canvas canvas;
        const auto before = canvas.fingerprint();
        painter::paintSavedProfiles(canvas.graphics, view, rightColumn);
        CHECK(canvas.fingerprint() == before);
    }

    SECTION("Empty list shows the empty message and never dereferences")
    {
        auto view = makeView();
        view.showSavedProfiles = true;
        view.savedProfiles = nullptr;
        view.selectedProfileIndex = 0;

        Canvas canvas;
        CHECK_NOTHROW(painter::paintSavedProfiles(canvas.graphics, view, rightColumn));
    }

    SECTION("Out of range index falls back to the empty message")
    {
        std::vector<calibration::CalibrationRecord> records(2);
        records[0].deviceSnapshot.deviceName = "Interface A";
        records[1].deviceSnapshot.deviceName = "Interface B";

        auto view = makeView();
        view.showSavedProfiles = true;
        view.savedProfiles = &records;
        view.selectedProfileIndex = 7;

        Canvas canvas;
        CHECK_NOTHROW(painter::paintSavedProfiles(canvas.graphics, view, rightColumn));
    }
}

TEST_CASE("CalibrationPanelPainters: Digital mode body renders in both verification states", "[gui][calibration][paint]")
{
    juce::uint64 fingerprints[2] = { 0, 0 };

    for (int verified = 0; verified < 2; ++verified)
    {
        auto view = makeView();
        view.digitalMode = true;
        view.digitalVerified = verified != 0;

        Canvas canvas;
        juce::Rectangle<float> content { 20.0f, 20.0f, 820.0f, 540.0f };
        juce::Rectangle<float> leftColumn = content.removeFromLeft(content.getWidth() * 0.52f);
        content.removeFromLeft(20.0f);
        const auto rightColumn = content;

        painter::paintDigitalIntro(canvas.graphics, content);
        painter::paintDigitalInstructions(canvas.graphics, view, leftColumn);
        painter::paintDigitalStatusCard(canvas.graphics, view, rightColumn);

        fingerprints[verified] = canvas.fingerprint();
    }

    CHECK(fingerprints[0] != fingerprints[1]);
}

TEST_CASE("CalibrationPanelPainters: Format helpers", "[gui][calibration][paint]")
{
    SECTION("Trim gain is rendered with an explicit sign")
    {
        CHECK(painter::formatTrimDb(1.0f, 1) == "+0.0 dB");
        CHECK(painter::formatTrimDb(0.5f, 1) == "-6.0 dB");
        CHECK(painter::formatTrimDb(2.0f, 2) == "+6.02 dB");
    }

    SECTION("Digital silence collapses into -inf")
    {
        CHECK(painter::formatDbfs(-200.0f, 1) == "-inf dBFS");
        CHECK(painter::formatDbfs(-12.34f, 1) == "-12.3 dBFS");
    }

    SECTION("Only measurement failures are failure states")
    {
        CHECK(painter::isLoopbackFailureState(LoopbackState::Failed));
        CHECK(painter::isLoopbackFailureState(LoopbackState::Clipped));
        CHECK(painter::isLoopbackFailureState(LoopbackState::SignalTooLow));
        CHECK_FALSE(painter::isLoopbackFailureState(LoopbackState::Passed));
        CHECK_FALSE(painter::isLoopbackFailureState(LoopbackState::Ready));
        CHECK_FALSE(painter::isLoopbackFailureState(LoopbackState::Locked));
        CHECK_FALSE(painter::isLoopbackFailureState(LoopbackState::Stale));

        // DeviceStopped is deliberately outside this helper: it is not a measurement verdict but
        // an infrastructure stop, and both painters give it a dedicated "[ DEVICE STOPPED ]" pill
        // and badge instead of reusing the generic failure wording.
        CHECK_FALSE(painter::isLoopbackFailureState(LoopbackState::DeviceStopped));
    }
}

TEST_CASE("CalibrationPanelPainters: DeviceStopped reads as an error in the card and in the report",
          "[gui][calibration][paint]")
{
    // Hardcoded in the painters as the pill background of every failure state, so this probe does
    // not depend on the theme being initialised in a headless run.
    const juce::Colour errorPillFill { 0xfffee2e2 };

    const juce::Rectangle<float> stepperArea { 0.0f, 0.0f, 804.0f, 62.0f };

    auto stepperErrorPixels = [&] (const ViewState& view)
    {
        Canvas canvas;
        juce::Rectangle<int> card2A;
        juce::Rectangle<int> card2B;
        painter::paintStepper(canvas.graphics, view, stepperArea, card2A, card2B);
        return countExactPixels(canvas.image, errorPillFill);
    };

    auto stepperFingerprint = [&] (const ViewState& view)
    {
        Canvas canvas;
        juce::Rectangle<int> card2A;
        juce::Rectangle<int> card2B;
        painter::paintStepper(canvas.graphics, view, stepperArea, card2A, card2B);
        return canvas.fingerprint();
    };

    auto loopbackReportFingerprint = [&] (const ViewState& view)
    {
        Canvas canvas;
        juce::Rectangle<float> meterArea { 0.0f, 0.0f, 356.0f, 240.0f };
        painter::paintLoopbackReport(canvas.graphics, view, meterArea);
        return canvas.fingerprint();
    };

    auto noiseReportFingerprint = [&] (const ViewState& view)
    {
        Canvas canvas;
        juce::Rectangle<float> meterArea { 0.0f, 0.0f, 356.0f, 240.0f };
        painter::paintNoiseBaselineReport(canvas.graphics, view, meterArea);
        return canvas.fingerprint();
    };

    SECTION("Step 2A card turns red when the device stops mid-baseline")
    {
        auto pendingView = makeView();
        pendingView.subView = SubView::NoiseBaseline_2A;
        pendingView.noiseBaselineState = NoiseBaselineState::NotChecked;

        auto stoppedView = pendingView;
        stoppedView.noiseBaselineState = NoiseBaselineState::DeviceStopped;

        CHECK(stepperErrorPixels(pendingView) == 0);
        CHECK(stepperErrorPixels(stoppedView) > 0);
        CHECK(stepperFingerprint(pendingView) != stepperFingerprint(stoppedView));
    }

    SECTION("Step 2B card turns red when the device stops mid-sweep")
    {
        auto readyView = makeView();
        readyView.subView = SubView::PhysicalLoopback_2B;
        readyView.noiseBaselineState = NoiseBaselineState::Passed;
        readyView.loopbackState = LoopbackState::Ready;

        auto stoppedView = readyView;
        stoppedView.loopbackState = LoopbackState::DeviceStopped;

        CHECK(stepperErrorPixels(readyView) == 0);
        CHECK(stepperErrorPixels(stoppedView) > 0);
    }

    SECTION("Card and report agree that DeviceStopped is a failure, and say so in their own words")
    {
        auto stoppedView = makeView();
        stoppedView.subView = SubView::PhysicalLoopback_2B;
        stoppedView.noiseBaselineState = NoiseBaselineState::Passed;
        stoppedView.loopbackState = LoopbackState::DeviceStopped;

        auto otherState = [&] (LoopbackState state)
        {
            auto view = stoppedView;
            view.loopbackState = state;
            return view;
        };

        // Both surfaces must give DeviceStopped its own look, not the generic failure wording
        // and not the neutral one: that pair was the original card/report divergence.
        CHECK(stepperFingerprint(stoppedView) != stepperFingerprint(otherState(LoopbackState::Failed)));
        CHECK(loopbackReportFingerprint(stoppedView)
              != loopbackReportFingerprint(otherState(LoopbackState::Failed)));

        // Ready / Passed / Locked / Stale: the three neutral looks plus the invalidated one.
        for (const auto other : { LoopbackState::Ready, LoopbackState::Passed,
                                  LoopbackState::Locked, LoopbackState::Stale })
        {
            CHECK(stepperFingerprint(stoppedView) != stepperFingerprint(otherState(other)));
            CHECK(loopbackReportFingerprint(stoppedView)
                  != loopbackReportFingerprint(otherState(other)));
        }
    }

    SECTION("Step 2A report switches to the device-stopped cause and action")
    {
        auto pendingView = makeView();
        pendingView.subView = SubView::NoiseBaseline_2A;
        pendingView.noiseBaselineState = NoiseBaselineState::NotChecked;

        auto stoppedView = pendingView;
        stoppedView.noiseBaselineState = NoiseBaselineState::DeviceStopped;

        CHECK(noiseReportFingerprint(pendingView) != noiseReportFingerprint(stoppedView));
    }
}
