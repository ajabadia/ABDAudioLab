/**
 * @file HoldoutSequence.h
 * @brief Formal out-of-sample holdout sequence with anti-leakage verification and SHA-256 fixity.
 * @author ABDSynths
 * @date 2026
 *
 * Implements a scientifically strict, out-of-sample validation stimulus and dynamic
 * control trajectory. Guarantees that validation points do NOT participate in
 * training, LUT interpolation grid calibration, or parameter fitting.
 */

#pragma once

#include <string>
#include <vector>
#include <utility>
#include <cstddef>
#include <nlohmann/json.hpp>
#include "GeneratedAcousticModel.h"

namespace abdaudiolab::core
{

/**
 * @struct HoldoutTrajectoryPoint
 * @brief Waypoint along the dynamic holdout parameter trajectory.
 */
struct HoldoutTrajectoryPoint
{
    double timeSeconds { 0.0 };
    float param1 { 0.5f };
    float param2 { 0.5f };
    float stimulusFreqHz { 440.0f };
    float stimulusGain { 0.5f };
};

/**
 * @struct HoldoutSequence
 * @brief Canonical holdout definition with provenance, hashes, and leakage audit.
 */
struct HoldoutSequence
{
    std::string sequenceId { "holdout-dynamic-v1" };
    double sampleRate { 48000.0 };
    double totalDurationSeconds { 1.5 };
    int numChannels { 1 };

    /** Coordinates strictly unseen during training/calibration [0.0, 1.0]. */
    std::vector<std::pair<float, float>> unseenCoordinates;

    /** Dynamic parameter and stimulus trajectory waypoints. */
    std::vector<HoldoutTrajectoryPoint> trajectory;

    /** Provenance and fixity hashes (RFC 8785 canonical SHA-256). */
    std::string trainingPlanHash;        /**< Hash of training coordinates to guarantee disjoint sets. */
    std::string holdoutPlanHash;         /**< Deterministic SHA-256 of the holdout specification. */
    std::string sequenceDefinitionHash;  /**< Deterministic SHA-256 of the trajectory and stimulus points. */

    /**
     * @brief Evaluates interpolated acoustic parameters at a specific time offset.
     */
    [[nodiscard]] AcousticModelParameters getParametersAtTime(double timeSeconds) const noexcept;

    /**
     * @brief Computes frequency and amplitude of the reference stimulus at a specific time.
     */
    void getStimulusAtTime(double timeSeconds, float& outFreqHz, float& outGain) const noexcept;

    /**
     * @brief Synthesizes the deterministic multi-harmonic stimulus into the provided buffer.
     * Realizes a band-limited multi-harmonic probe covering fundamental and upper harmonics.
     */
    void renderStimulus(float* outBuffer, int numSamples, double sampleRate) const noexcept;

    /**
     * @brief Checks if any unseen holdout coordinate intersects with training points.
     * @param trainingPoints Vector of (p1, p2) points used to fit the model.
     * @param tolerance Euclidean distance threshold (default 1e-3).
     * @return true if data leakage is detected (points intersect), false if strictly disjoint.
     */
    [[nodiscard]] bool hasDataLeakage(const std::vector<std::pair<float, float>>& trainingPoints,
                                      float tolerance = 1e-3f) const noexcept;

    /**
     * @brief Updates holdoutPlanHash and sequenceDefinitionHash deterministically via SHA-256.
     */
    void updateHashes();

    /**
     * @brief Serializes holdout specification to JSON.
     */
    [[nodiscard]] nlohmann::json toJson() const;

    /**
     * @brief Deserializes holdout specification from JSON.
     */
    static HoldoutSequence fromJson(const nlohmann::json& j);
};

/**
 * @brief Creates the canonical out-of-sample holdout sequence for ABDAudioLab.
 * Coordinates are placed at midpoints of an 8x8 uniform grid (e.g. 0.071, 0.214, 0.357, etc.)
 * ensuring that training points at k/7 are completely excluded.
 */
HoldoutSequence createCanonicalHoldoutSequence(double sampleRate = 48000.0,
                                               const std::string& trainingPlanHash = "");

} // namespace abdaudiolab::core
