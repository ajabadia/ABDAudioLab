#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "math/ThermalDriftAnalyzer.h"

using namespace abdaudiolab;

TEST_CASE("ThermalDriftAnalyzer Cutoff and Gain Drift Calculation", "[math][thermal][drift]")
{
    std::vector<exporting::MeasuredPoint> run1;
    std::vector<exporting::MeasuredPoint> run2;

    for (int i = 0; i < 5; ++i)
    {
        exporting::MeasuredPoint p1;
        p1.muSigmaValue.mean = 2000.0f; // 2000 Hz cutoff
        p1.secondaryValue.mean = 0.0f;   // 0 dB gain
        p1.thdPercent = 0.1f;
        run1.push_back(p1);

        exporting::MeasuredPoint p2;
        p2.muSigmaValue.mean = 2025.0f; // +25 Hz drift
        p2.secondaryValue.mean = 0.5f;   // +0.5 dB gain drift
        p2.thdPercent = 0.12f;
        run2.push_back(p2);
    }

    const float temp1 = 20.0f;
    const float temp2 = 25.0f; // +5 °C rise

    auto report = math::ThermalDriftAnalyzer::analyzeDrift(run1, run2, temp1, temp2);

    REQUIRE(report.validComparison == true);
    REQUIRE(report.pointsEvaluated == 5);
    REQUIRE_THAT(report.deltaTempCelsius, Catch::Matchers::WithinAbs(5.0f, 1e-3f));
    REQUIRE_THAT(report.cutoffDriftHz, Catch::Matchers::WithinAbs(25.0f, 1e-3f));
    REQUIRE_THAT(report.cutoffDriftHzPerCelsius, Catch::Matchers::WithinAbs(5.0f, 1e-3f)); // 25 / 5 = 5 Hz/°C
    REQUIRE_THAT(report.gainDriftDb, Catch::Matchers::WithinAbs(0.5f, 1e-3f));
    REQUIRE_THAT(report.gainDriftDbPerCelsius, Catch::Matchers::WithinAbs(0.1f, 1e-3f)); // 0.5 / 5 = 0.1 dB/°C
}
