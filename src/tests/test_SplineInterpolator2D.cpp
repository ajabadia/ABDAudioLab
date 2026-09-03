#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "math/SplineInterpolator2D.h"
#include <vector>
#include <cmath>

TEST_CASE("SplineInterpolator2D Grid Evaluation", "[math][spline]")
{
    abdaudiolab::math::SplineInterpolator2D interpolator;

    std::vector<float> p1Grid = { 0.0f, 0.5f, 1.0f };
    std::vector<float> p2Grid = { 0.0f, 0.5f, 1.0f };

    // Function f(p1, p2) = p1 * p2
    std::vector<std::vector<float>> values = {
        { 0.00f, 0.00f, 0.00f },
        { 0.00f, 0.25f, 0.50f },
        { 0.00f, 0.50f, 1.00f }
    };

    bool setOk = interpolator.setGridData(p1Grid, p2Grid, values);
    REQUIRE(setOk);
    REQUIRE(interpolator.isReady());

    // Evaluate exact grid points
    REQUIRE_THAT(interpolator.evaluate(0.0f, 0.0f), Catch::Matchers::WithinAbs(0.00f, 0.01f));
    REQUIRE_THAT(interpolator.evaluate(0.5f, 0.5f), Catch::Matchers::WithinAbs(0.25f, 0.01f));
    REQUIRE_THAT(interpolator.evaluate(1.0f, 1.0f), Catch::Matchers::WithinAbs(1.00f, 0.01f));

    // Interpolate midpoint (0.25, 0.5)
    float midVal = interpolator.evaluate(0.25f, 0.50f);
    REQUIRE(midVal >= 0.0f);
    REQUIRE(midVal <= 0.5f);
}

TEST_CASE("SplineInterpolator2D Boundary Clamping", "[math][spline]")
{
    abdaudiolab::math::SplineInterpolator2D interpolator;

    std::vector<float> p1Grid = { 0.0f, 1.0f };
    std::vector<float> p2Grid = { 0.0f, 1.0f };
    std::vector<std::vector<float>> values = {
        { 10.0f, 20.0f },
        { 30.0f, 40.0f }
    };

    interpolator.setGridData(p1Grid, p2Grid, values);

    // Out of bounds inputs should clamp to grid boundaries without throwing
    float clampMin = interpolator.evaluate(-0.5f, -0.5f);
    float clampMax = interpolator.evaluate(1.5f, 1.5f);

    REQUIRE_THAT(clampMin, Catch::Matchers::WithinAbs(10.0f, 0.1f));
    REQUIRE_THAT(clampMax, Catch::Matchers::WithinAbs(40.0f, 0.1f));
}
