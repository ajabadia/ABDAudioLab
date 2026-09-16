/**
 * @file test_HoldoutSequence.cpp
 * @brief Unit tests for HoldoutSequence out-of-sample specification and anti-leakage audit (T2).
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/HoldoutSequence.h"
#include <vector>

TEST_CASE("HoldoutSequence - Canonical Creation and Cryptographic Hashes", "[holdout][provenance][fixity]")
{
    using namespace abdaudiolab::core;

    std::string mockTrainHash = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2";
    auto seq = createCanonicalHoldoutSequence(48000.0, mockTrainHash);

    CHECK(seq.sequenceId == "holdout-dynamic-v1");
    CHECK(seq.sampleRate == 48000.0);
    CHECK(seq.totalDurationSeconds == 1.5);
    CHECK_FALSE(seq.unseenCoordinates.empty());
    CHECK_FALSE(seq.trajectory.empty());

    // Verify 64-char SHA-256 hashes
    CHECK(seq.sequenceDefinitionHash.size() == 64);
    CHECK(seq.holdoutPlanHash.size() == 64);
    CHECK(seq.trainingPlanHash == mockTrainHash);

    // Hash determinism
    auto seq2 = createCanonicalHoldoutSequence(48000.0, mockTrainHash);
    CHECK(seq.sequenceDefinitionHash == seq2.sequenceDefinitionHash);
    CHECK(seq.holdoutPlanHash == seq2.holdoutPlanHash);
}

TEST_CASE("HoldoutSequence - Anti-Leakage Audit", "[holdout][anti_leakage]")
{
    using namespace abdaudiolab::core;

    auto seq = createCanonicalHoldoutSequence(48000.0, "mock_train_hash");

    // Construct 8x8 training grid where points are strictly at k/7:
    // { 0.0, 0.1428, 0.2857, 0.4285, 0.5714, 0.7142, 0.8571, 1.0 }
    std::vector<std::pair<float, float>> trainingGrid;
    for (int y = 0; y < 8; ++y)
    {
        for (int x = 0; x < 8; ++x)
        {
            float p1 = static_cast<float>(x) / 7.0f;
            float p2 = static_cast<float>(y) / 7.0f;
            trainingGrid.emplace_back(p1, p2);
        }
    }

    // Must NOT leak: holdout coordinates are at midpoints and strictly disjoint
    CHECK_FALSE(seq.hasDataLeakage(trainingGrid, 0.02f));

    // Deliberate leakage injection: add an exact holdout point into training set
    auto corruptedTraining = trainingGrid;
    corruptedTraining.emplace_back(0.0714f, 0.2143f); // First unseen point
    CHECK(seq.hasDataLeakage(corruptedTraining, 0.001f));

    // Deliberate leakage injection: add an exact trajectory waypoint
    auto corruptedWaypoint = trainingGrid;
    corruptedWaypoint.emplace_back(0.6429f, 0.2143f); // Trajectory waypoint at t=0.3
    CHECK(seq.hasDataLeakage(corruptedWaypoint, 0.001f));
}

TEST_CASE("HoldoutSequence - Trajectory Parameter Interpolation", "[holdout][trajectory]")
{
    using namespace abdaudiolab::core;

    auto seq = createCanonicalHoldoutSequence(48000.0, "mock_train_hash");

    // At t = 0.0: p1 = 0.2143, p2 = 0.3571
    auto p0 = seq.getParametersAtTime(0.0);
    REQUIRE_THAT(p0.param1, Catch::Matchers::WithinAbs(0.2143f, 1e-3f));
    REQUIRE_THAT(p0.param2, Catch::Matchers::WithinAbs(0.3571f, 1e-3f));

    // At midpoint between t=0.0 and t=0.3 (t = 0.15):
    // p1 = 0.2143 + 0.5*(0.6429 - 0.2143) = 0.4286
    // p2 = 0.3571 + 0.5*(0.2143 - 0.3571) = 0.2857
    auto pMid = seq.getParametersAtTime(0.15);
    REQUIRE_THAT(pMid.param1, Catch::Matchers::WithinAbs(0.4286f, 1e-3f));
    REQUIRE_THAT(pMid.param2, Catch::Matchers::WithinAbs(0.2857f, 1e-3f));

    // Beyond end (t = 3.0): clamps to final point
    auto pEnd = seq.getParametersAtTime(3.0);
    REQUIRE_THAT(pEnd.param1, Catch::Matchers::WithinAbs(0.5000f, 1e-3f));
    REQUIRE_THAT(pEnd.param2, Catch::Matchers::WithinAbs(0.0714f, 1e-3f));
}

TEST_CASE("HoldoutSequence - Deterministic Stimulus Rendering", "[holdout][stimulus]")
{
    using namespace abdaudiolab::core;

    auto seq = createCanonicalHoldoutSequence(48000.0, "mock_train_hash");
    const int numSamples = static_cast<int>(seq.totalDurationSeconds * seq.sampleRate);
    std::vector<float> buffer(numSamples, 0.0f);

    seq.renderStimulus(buffer.data(), numSamples, seq.sampleRate);

    // Verify non-silent and bounded
    float maxAmp = 0.0f;
    for (float s : buffer)
    {
        maxAmp = std::max(maxAmp, std::abs(s));
    }

    CHECK(maxAmp > 0.1f);
    CHECK(maxAmp <= 1.0f);

    // Reproducibility
    std::vector<float> buffer2(numSamples, 0.0f);
    seq.renderStimulus(buffer2.data(), numSamples, seq.sampleRate);
    CHECK(buffer == buffer2);
}

TEST_CASE("HoldoutSequence - JSON Serialization Round-Trip", "[holdout][json]")
{
    using namespace abdaudiolab::core;

    auto seq = createCanonicalHoldoutSequence(48000.0, "mock_train_hash");
    auto j = seq.toJson();

    auto deserialized = HoldoutSequence::fromJson(j);
    CHECK(deserialized.sequenceId == seq.sequenceId);
    CHECK(deserialized.sampleRate == seq.sampleRate);
    CHECK(deserialized.totalDurationSeconds == seq.totalDurationSeconds);
    CHECK(deserialized.unseenCoordinates.size() == seq.unseenCoordinates.size());
    CHECK(deserialized.trajectory.size() == seq.trajectory.size());
    CHECK(deserialized.sequenceDefinitionHash == seq.sequenceDefinitionHash);
    CHECK(deserialized.holdoutPlanHash == seq.holdoutPlanHash);
    CHECK(deserialized.trainingPlanHash == seq.trainingPlanHash);
}
