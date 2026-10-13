/**
 * @file CalibrationCompatibilityEvaluator.h
 * @brief Pure evaluator determining whether a CalibrationSnapshot is valid for the current audio stream.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <string>
#include <vector>
#include "CalibrationSnapshot.h"
#include "CalibrationMatchEvaluator.h"

namespace abdaudiolab::calibration
{

/**
 * @enum CompatibilityVerdict
 * @brief Precise classification of compatibility between a CalibrationSnapshot and current hardware.
 */
enum class CompatibilityVerdict
{
    Compatible,             /**< Exact match on device, driver, sample rate, buffer size and routing. */
    BufferSizeMismatch,     /**< Same device/SR/routing, but buffer size differs (RTL will differ). */
    SampleRateMismatch,     /**< Sample rate differs (frequency response & sample RTL invalid). */
    RoutingMismatch,        /**< Input/output channels differ. */
    DeviceOrDriverMismatch, /**< Device or audio driver family differs. */
    InvalidSnapshot,        /**< Snapshot status is not Valid, clipped, or corrupted hash. */
    NoActiveDevice          /**< Audio stream is stopped or uninitialized. */
};

/**
 * @struct CompatibilityEvaluation
 * @brief Structured result of a compatibility check.
 */
struct CompatibilityEvaluation
{
    CompatibilityVerdict verdict { CompatibilityVerdict::NoActiveDevice };
    bool isActionable { false };
    std::string summary;
    std::vector<std::string> mismatchDetails;

    [[nodiscard]] bool isCompatible() const noexcept
    {
        return verdict == CompatibilityVerdict::Compatible && isActionable;
    }
};

/**
 * @class CalibrationCompatibilityEvaluator
 * @brief Evaluator for CalibrationSnapshot compatibility against live audio configuration.
 */
class CalibrationCompatibilityEvaluator
{
public:
    /**
     * @brief Evaluates compatibility between a snapshot and the current audio configuration.
     * @param snapshot The saved or embedded snapshot.
     * @param current The current active audio configuration.
     * @param strictBufferSize If true, different buffer sizes cause BufferSizeMismatch and are not actionable.
     */
    [[nodiscard]] static CompatibilityEvaluation evaluate(
        const CalibrationSnapshot& snapshot,
        const CurrentAudioConfigurationSnapshot& current,
        bool strictBufferSize = true);
};

} // namespace abdaudiolab::calibration
